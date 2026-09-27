#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Convert a Google CarrierSettings ZIP into the reviewed ImsStack XML subset.

The APK and vendor/APN payloads are never installed or imported. Only config
keys explicitly listed in policy_keys.json can enter the generated profiles.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET
import zipfile

from google.protobuf.message import DecodeError

from proto import carrier_list_pb2, carrier_settings_pb2


TOOLS = pathlib.Path(__file__).resolve().parent
ROOT = TOOLS.parent
SCHEMA_REVISION = "ff35212d322a3e892605b94fa777c67085d45efd"
POLICY = json.loads((TOOLS / "policy_keys.json").read_text())
TAGS = {
    "text_value": "string", "int_value": "int", "long_value": "long",
    "bool_value": "boolean", "text_array": "string-array", "int_array": "int-array",
    "bundle": "pbundle_as_map",
}
PAYLOAD_BUNDLES = {
    "imsvoice.amrnb_payload_description_bundle",
    "imsvoice.amrwb_payload_description_bundle",
}
PAYLOAD_ARRAY_KEYS = {
    "imsvoice.amrnb_payload_type_int_array", "imsvoice.amrwb_payload_type_int_array",
    "imsvoice.dtmfnb_payload_type_int_array", "imsvoice.dtmfwb_payload_type_int_array",
}
AMR_ATTRIBUTE_KEYS = {
    "imsvoice.amr_codec_attribute_modeset_int_array",
    "imsvoice.amr_codec_attribute_payload_format_int",
    "imsvoice.codec_attribute_mode_change_capability_int",
}
NESTED_KEYS = PAYLOAD_ARRAY_KEYS | AMR_ATTRIBUTE_KEYS
TOP_LEVEL_KEYS = set(POLICY) - NESTED_KEYS
MAX_MEMBER_BYTES = 16 * 1024 * 1024


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def profile_id(name: str) -> str:
    if re.fullmatch(r"[A-Za-z0-9_-]+", name):
        return name
    # Auto-generated names can contain SPNs with whitespace and non-ASCII text.
    return "carrier_" + digest(name.encode())[:20]


def require_known_fields(message, label: str) -> None:
    known = type(message)()
    known.CopyFrom(message)
    known.DiscardUnknownFields()
    if known.SerializeToString(deterministic=True) != message.SerializeToString(deterministic=True):
        raise ValueError(f"{label}: unknown protobuf fields; update the schema before importing")


def selector(carrier_id) -> dict[str, str]:
    require_known_fields(carrier_id, "carrier selector")
    network = carrier_id.mcc_mnc
    if not re.fullmatch(r"[0-9]{5,6}", network):
        raise ValueError(f"Invalid MCC/MNC: {network!r}")
    result = {"mcc": network[:3], "mnc": network[3:]}
    kind = carrier_id.WhichOneof("mvno_data")
    if kind is None:
        return result
    value = getattr(carrier_id, kind)
    if not value:
        raise ValueError(f"Empty {kind} selector for {network}")
    if kind == "gid1":
        if not re.fullmatch(r"[0-9a-fA-F]+", value):
            raise ValueError(f"Invalid GID1 prefix: {value!r}")
        result["gid1_prefix"] = value
    elif kind == "iccid":
        if not re.fullmatch(r"[0-9]+", value):
            raise ValueError(f"Invalid ICCID prefix: {value!r}")
        result["iccid_prefix"] = value
    elif kind == "imsi":
        if not re.fullmatch(r"[0-9xX]+", value):
            raise ValueError(f"Unsupported IMSI pattern: {value!r}")
        result["imsi"] = "^" + re.sub("[xX]", "[0-9]", value) + ".*$"
    elif kind == "spn":
        # CarrierSettings uses a literal, case-insensitive SPN prefix. Escape
        # the metacharacters shared by Java and Python regular expressions.
        quoted = "".join("\\" + c if c in r"\.^$|?*+()[]{}" else c for c in value)
        result["spn"] = "^" + quoted + ".*$"
    else:
        raise ValueError(f"Unsupported MVNO selector: {kind}")
    return result


def config_element(config, *, allowed: set[str], payloads: bool = False):
    name = config.key
    dynamic_payload = payloads and re.fullmatch(r"[0-9]{1,3}", name)
    if dynamic_payload and not 0 <= int(name) <= 127:
        raise ValueError(f"Invalid RTP payload number: {name}")
    if name not in allowed and not dynamic_payload:
        return None
    require_known_fields(config, f"config {name}")
    kind = config.WhichOneof("value")
    expected = "pbundle_as_map" if dynamic_payload else POLICY[name]
    if TAGS.get(kind) != expected:
        raise ValueError(f"{name}: expected {expected}, got {kind}")
    node = ET.Element(expected, {"name": name})
    value = getattr(config, kind)
    if kind == "bundle":
        if name == "imsvoice.audio_codec_capability_payload_types_bundle":
            child_keys = PAYLOAD_ARRAY_KEYS
        elif name in PAYLOAD_BUNDLES:
            child_keys = set()
        elif dynamic_payload:
            child_keys = AMR_ATTRIBUTE_KEYS
        else:
            raise ValueError(f"No nested schema for {name}")
        for child in config_elements(value, child_keys, name in PAYLOAD_BUNDLES):
            node.append(child)
        if not len(node):
            return None
    elif kind in ("text_array", "int_array"):
        node.set("num", str(len(value.item)))
        for item in value.item:
            ET.SubElement(node, "item", {"value": str(item)})
    elif kind == "text_value":
        node.text = value
    elif kind == "bool_value":
        node.set("value", str(value).lower())
    else:
        node.set("value", str(value))
    return node


def config_elements(bundle, allowed=TOP_LEVEL_KEYS, payloads=False):
    seen = set()
    result = []
    for config in sorted(bundle.config, key=lambda c: c.key):
        if config.key in seen:
            raise ValueError(f"Duplicate config key: {config.key}")
        seen.add(config.key)
        element = config_element(config, allowed=allowed, payloads=payloads)
        if element is not None:
            result.append(element)
    return result


def xml_bytes(root: ET.Element) -> bytes:
    ET.indent(root, space="    ")
    data = (b'<?xml version="1.0" encoding="utf-8"?>\n'
            b'<!-- Generated from Google CarrierSettings. See SOURCE.md for provenance. -->\n'
            + ET.tostring(root, encoding="utf-8") + b"\n")
    ET.fromstring(data)  # Reject source strings that cannot be represented in XML.
    return data


def convert(archive: pathlib.Path) -> tuple[dict[str, bytes], dict]:
    source_files = {}
    settings = {}
    setting_sources = {}
    with zipfile.ZipFile(archive) as z:
        # Read by archive member name, never extract paths or execute APK content.
        members = [i for i in z.infolist() if i.filename.endswith(".pb")]
        if len({i.filename for i in members}) != len(members):
            raise ValueError("Duplicate protobuf archive members")
        if sum(i.file_size for i in members) > 64 * 1024 * 1024:
            raise ValueError("Carrier protobuf input exceeds 64 MiB")
        for member in members:
            if member.file_size > MAX_MEMBER_BYTES:
                raise ValueError(f"Oversized protobuf: {member.filename}")
            data = z.read(member)
            source_files[member.filename] = digest(data)
            name = pathlib.PurePosixPath(member.filename).name
            if name == "carrier_list.pb":
                continue
            if name == "others.pb":
                group = carrier_settings_pb2.MultiCarrierSettings.FromString(data)
                entries = group.setting
                others_version = group.version
            else:
                entries = [carrier_settings_pb2.CarrierSettings.FromString(data)]
            for entry in entries:
                if not entry.canonical_name or entry.canonical_name in settings:
                    raise ValueError(f"Missing/duplicate profile name: {entry.canonical_name!r}")
                settings[entry.canonical_name] = entry
                setting_sources[entry.canonical_name] = member.filename
        index_names = [n for n in source_files if n.endswith("/carrier_list.pb")
                       or n == "carrier_list.pb"]
        if len(index_names) != 1:
            raise ValueError("Expected exactly one carrier_list.pb")
        index = carrier_list_pb2.CarrierList.FromString(z.read(index_names[0]))
    if "default" not in settings or not index.entry:
        raise ValueError("Missing default profile or empty carrier list")

    files = {}
    profiles = {}
    identities = []
    identifiers = {}
    for name, entry in sorted(settings.items()):
        ident = profile_id(name)
        if ident in identifiers:
            raise ValueError(f"Profile filename collision: {name}")
        identifiers[ident] = name
        root = ET.Element("carrier_config")
        root.extend(config_elements(entry.configs))
        path = "default.xml" if name == "default" else f"profiles/{ident}.xml"
        files[path] = xml_bytes(root)
        profiles[name] = {"file": path, "version": entry.version,
                          "source": setting_sources[name], "keys": len(root)}
    carrier_list = ET.Element("carrier_config_list")
    for entry in index.entry:
        if entry.canonical_name not in settings or not entry.carrier_id:
            raise ValueError(f"Unresolved carrier mapping: {entry.canonical_name}")
        ident = profile_id(entry.canonical_name)
        # Preserve source order, including empty profiles. An empty MVNO must
        # still win over its MNO instead of inheriting unrelated MNO overrides.
        for carrier_id in entry.carrier_id:
            attributes = selector(carrier_id)
            node = ET.SubElement(carrier_list, "carrier_config", attributes)
            ET.SubElement(node, "string", {"name": "profile"}).text = ident
            identities.append(attributes)
    # The default identity can select default as a named profile too.
    files["profiles/default.xml"] = files["default.xml"]
    files["carrier_list.xml"] = xml_bytes(carrier_list)
    manifest = {
        "archive_sha256": digest(archive.read_bytes()),
        "carrier_list_version": index.version,
        "others_version": locals().get("others_version"),
        "schema_revision": SCHEMA_REVISION,
        "schema_note": "CarrierId.mvno_data.iccid field 6 added for the supplied snapshot",
        "policy_sha256": digest((TOOLS / "policy_keys.json").read_bytes()),
        "source_files": dict(sorted(source_files.items())),
        "profiles": profiles,
        "identity_count": len(identities),
        "iccid_selector_count": sum("iccid_prefix" in x for x in identities),
        "output_sha256": {k: digest(v) for k, v in sorted(files.items())},
    }
    return files, manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=pathlib.Path)
    parser.add_argument("--check", action="store_true", help="Verify committed outputs without writing")
    args = parser.parse_args()
    try:
        files, manifest = convert(args.archive)
        output = ROOT / "assets" / "carrier_settings"
        record = ROOT / "carrier_settings_source.json"
        metadata = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
        existing = {p.relative_to(output).as_posix() for p in output.rglob("*.xml")}
        if args.check:
            errors = [name for name, data in files.items()
                      if not (output / name).is_file() or (output / name).read_bytes() != data]
            errors.extend(sorted(existing - files.keys()))
            if not record.is_file() or record.read_bytes() != metadata:
                errors.append(record.name)
            if errors:
                raise ValueError("Generated files differ: " + ", ".join(errors[:20]))
        else:
            for stale in existing - files.keys():
                (output / stale).unlink()
            for name, data in files.items():
                target = output / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            record.write_bytes(metadata)
        print(f"{'Verified' if args.check else 'Generated'} {len(manifest['profiles'])} profiles, "
              f"{manifest['identity_count']} SIM identities, "
              f"{manifest['iccid_selector_count']} ICCID selectors")
        return 0
    except (OSError, ValueError, DecodeError, zipfile.BadZipFile, ET.ParseError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

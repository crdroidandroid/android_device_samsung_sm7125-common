#!/usr/bin/env python3
#
# Copyright (C) 2026 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Validate carrier XML against ImsStack's ConfigXmlUtils parser contract."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


VALUE_TAGS = {"boolean", "int", "long", "string"}
ARRAY_TAGS = {"int-array", "long-array", "string-array"}
CONFIG_TAGS = VALUE_TAGS | ARRAY_TAGS | {"pbundle_as_map"}
FILTERS = {
    "mcc",
    "mnc",
    "gid1",
    "gid1_prefix",
    "iccid_prefix",
    "spn",
    "imsi",
    "cid",
}
INT_MIN = -(2**31)
INT_MAX = 2**31 - 1
LONG_MIN = -(2**63)
LONG_MAX = 2**63 - 1
CARRIER_ID_RE = re.compile(r"carrier_config_carrierid_(-?\d+)_")


class Auditor:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.carrier_ids: dict[str, list[pathlib.Path]] = collections.defaultdict(list)
        self.parent_carrier_ids: dict[str, list[pathlib.Path]] = collections.defaultdict(list)
        self.fragments = 0

    def error(self, path: pathlib.Path, message: str) -> None:
        self.errors.append(f"{path}: {message}")

    def warning(self, path: pathlib.Path, message: str) -> None:
        self.warnings.append(f"{path}: {message}")

    def check_number(
        self,
        path: pathlib.Path,
        description: str,
        raw_value: str | None,
        minimum: int,
        maximum: int,
    ) -> None:
        try:
            value = int(raw_value)  # type: ignore[arg-type]
        except (TypeError, ValueError):
            self.error(path, f"{description} is not an integer: {raw_value!r}")
            return
        if not minimum <= value <= maximum:
            self.error(path, f"{description} is out of range: {value}")

    def check_bundle(self, path: pathlib.Path, bundle: ET.Element, location: str) -> None:
        names: set[str] = set()
        for child in bundle:
            if child.tag not in CONFIG_TAGS:
                self.error(path, f"{location}: unsupported <{child.tag}> tag")
                continue

            name = child.get("name")
            if not name:
                self.error(path, f"{location}: <{child.tag}> has no non-empty name")
            elif name in names:
                self.error(path, f"{location}: duplicate key {name!r}")
            else:
                names.add(name)

            child_location = f"{location}/{name or child.tag}"
            if child.tag == "boolean":
                if list(child) or child.text:
                    self.error(path, f"{child_location}: boolean contains nested content")
                if child.get("value", "").lower() not in {"true", "false"}:
                    self.error(path, f"{child_location}: invalid boolean {child.get('value')!r}")
            elif child.tag == "int":
                if list(child) or child.text:
                    self.error(path, f"{child_location}: int contains nested content")
                self.check_number(path, child_location, child.get("value"), INT_MIN, INT_MAX)
            elif child.tag == "long":
                if list(child) or child.text:
                    self.error(path, f"{child_location}: long contains nested content")
                self.check_number(path, child_location, child.get("value"), LONG_MIN, LONG_MAX)
            elif child.tag == "string":
                if list(child):
                    self.error(path, f"{child_location}: string contains nested tags")
                if "value" in child.attrib:
                    self.warning(
                        path,
                        f"{child_location}: runtime reads string text, not the value attribute",
                    )
            elif child.tag in ARRAY_TAGS:
                items = list(child)
                self.check_number(path, f"{child_location} num", child.get("num"), 0, INT_MAX)
                try:
                    declared = int(child.get("num", ""))
                except (TypeError, ValueError):
                    declared = -1
                if declared != len(items):
                    self.error(
                        path,
                        f"{child_location}: num={declared}, but {len(items)} items exist",
                    )
                for index, item in enumerate(items):
                    if item.tag != "item":
                        self.error(
                            path,
                            f"{child_location}[{index}]: expected <item>, got <{item.tag}>",
                        )
                        continue
                    if list(item):
                        self.error(path, f"{child_location}[{index}]: item contains nested tags")
                    if child.tag == "int-array":
                        value = item.get("value")
                        self.check_number(
                            path,
                            f"{child_location}[{index}]",
                            value,
                            INT_MIN,
                            INT_MAX,
                        )
                        if (
                            name == "ims.parent_carrier_ids_int_array"
                            and value is not None
                        ):
                            self.parent_carrier_ids[value].append(path)
                    elif child.tag == "long-array":
                        self.check_number(
                            path,
                            f"{child_location}[{index}]",
                            item.get("value"),
                            LONG_MIN,
                            LONG_MAX,
                        )
                    elif item.get("value") is None:
                        self.error(path, f"{child_location}[{index}]: missing value")
            elif child.tag == "pbundle_as_map":
                self.check_bundle(path, child, child_location)

    def check_file(self, path: pathlib.Path) -> None:
        match = CARRIER_ID_RE.match(path.name)
        if match:
            self.carrier_ids[match.group(1)].append(path)

        try:
            root = ET.parse(path).getroot()
        except (ET.ParseError, OSError) as exc:
            self.error(path, f"XML parse failed: {exc}")
            return

        if root.tag == "carrier_config":
            configs = [root]
        elif root.tag == "carrier_config_list":
            configs = list(root)
            for child in configs:
                if child.tag != "carrier_config":
                    self.error(path, f"root contains unsupported <{child.tag}> child")
        else:
            self.error(path, f"unsupported root <{root.tag}>")
            return

        seen_filters: set[tuple[tuple[str, str], ...]] = set()
        for index, config in enumerate(configs):
            if config.tag != "carrier_config":
                continue
            self.fragments += 1
            unknown = set(config.attrib) - FILTERS
            if unknown:
                self.error(path, f"fragment {index}: unsupported filters {sorted(unknown)}")
            for regex_filter in ("imsi", "spn"):
                value = config.get(regex_filter)
                if value is not None:
                    try:
                        re.compile(value, re.IGNORECASE)
                    except re.error as exc:
                        self.error(path, f"fragment {index}: invalid {regex_filter} regex: {exc}")
            signature = tuple(sorted(config.attrib.items()))
            if signature in seen_filters:
                self.warning(path, f"fragment {index}: repeats filter set {dict(signature)!r}")
            seen_filters.add(signature)
            self.check_bundle(path, config, f"fragment {index}")
            if path.name == "carrier_list.xml" and path.parent.name == "carrier_settings":
                values = list(config)
                if (len(values) != 1 or values[0].tag != "string"
                        or values[0].get("name") != "profile"):
                    self.error(path, f"fragment {index}: expected one profile reference")
                    continue
                profile = values[0].text or ""
                if not re.fullmatch(r"[A-Za-z0-9_-]+", profile):
                    self.error(path, f"fragment {index}: unsafe profile name {profile!r}")
                elif not (path.parent / "profiles" / f"{profile}.xml").is_file():
                    self.error(path, f"fragment {index}: missing profile {profile}")
                if not re.fullmatch(r"[0-9]{3}", config.get("mcc", "")):
                    self.error(path, f"fragment {index}: invalid MCC")
                if not re.fullmatch(r"[0-9]{2,3}", config.get("mnc", "")):
                    self.error(path, f"fragment {index}: invalid MNC")

    def finish(self) -> None:
        for carrier_id, paths in sorted(self.carrier_ids.items(), key=lambda item: int(item[0])):
            if len(paths) > 1:
                joined = ", ".join(path.name for path in paths)
                self.error(paths[0], f"carrier ID {carrier_id} has ambiguous files: {joined}")
        for carrier_id, paths in sorted(
            self.parent_carrier_ids.items(), key=lambda item: int(item[0])
        ):
            if carrier_id not in self.carrier_ids:
                self.error(paths[0], f"parent carrier ID {carrier_id} has no profile")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "directory",
        nargs="?",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parent / "assets",
    )
    args = parser.parse_args()

    if not args.directory.is_dir():
        print(f"ERROR: carrier-config directory does not exist: {args.directory}")
        return 1

    paths = sorted(args.directory.rglob("*.xml"))
    if not paths:
        print(f"ERROR: no XML files found in: {args.directory}")
        return 1

    auditor = Auditor()
    for path in paths:
        auditor.check_file(path)
    auditor.finish()

    # Normal validation needs only Python's standard library; protobuf is used
    # only when refreshing or independently reproducing the imported snapshot.
    record = pathlib.Path(__file__).resolve().parent / "carrier_settings_source.json"
    output = args.directory / "carrier_settings"
    if output.is_dir():
        if not record.is_file():
            auditor.error(output, "missing carrier_settings_source.json")
        else:
            try:
                expected = json.loads(record.read_text())["output_sha256"]
                actual = {p.relative_to(output).as_posix():
                          hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in output.rglob("*.xml")}
                for name in sorted(expected.keys() | actual.keys()):
                    if expected.get(name) != actual.get(name):
                        auditor.error(output / name, "does not match generated source manifest")
            except (KeyError, ValueError, OSError) as exc:
                auditor.error(record, f"invalid source manifest: {exc}")

    for message in auditor.errors:
        print(f"ERROR: {message}")
    for message in auditor.warnings:
        print(f"WARNING: {message}")
    print(
        f"Checked {len(paths)} XML files and {auditor.fragments} carrier fragments: "
        f"{len(auditor.errors)} errors, {len(auditor.warnings)} warnings"
    )
    return 1 if auditor.errors else 0


if __name__ == "__main__":
    sys.exit(main())

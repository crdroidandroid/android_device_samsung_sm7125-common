# SPDX-License-Identifier: Apache-2.0
"""Offline regression tests for carrier identity and typed policy conversion."""

import pathlib
import re
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

import import_carrier_settings as importer
from proto import carrier_list_pb2 as cl
from proto import carrier_settings_pb2 as cs


class CarrierSettingsImportTest(unittest.TestCase):
    def test_mnc_length_is_preserved(self):
        self.assertEqual("07", importer.selector(cl.CarrierId(mcc_mnc="26207"))["mnc"])
        self.assertEqual("007", importer.selector(cl.CarrierId(mcc_mnc="262007"))["mnc"])

    def test_imsi_wildcards_are_digits_and_prefixes(self):
        pattern = importer.selector(cl.CarrierId(mcc_mnc="21670", imsi="21670xx1"))["imsi"]
        self.assertIsNotNone(re.fullmatch(pattern, "21670001234567"))
        self.assertIsNone(re.fullmatch(pattern, "21670ab1234567"))

    def test_spn_is_a_literal_prefix(self):
        pattern = importer.selector(cl.CarrierId(mcc_mnc="26207", spn="A+B (mobile)"))["spn"]
        self.assertIsNotNone(re.fullmatch(pattern, "a+b (mobile) extra", re.IGNORECASE))
        self.assertIsNone(re.fullmatch(pattern, "AAAB mobile", re.IGNORECASE))

    def test_iccid_selectors_are_not_mno_matches(self):
        self.assertEqual({"mcc": "208", "mnc": "01", "iccid_prefix": "894936"},
                         importer.selector(cl.CarrierId(mcc_mnc="20801", iccid="894936")))

    def test_unknown_selector_fields_fail_closed(self):
        value = cl.CarrierId.FromString(cl.CarrierId(mcc_mnc="26207").SerializeToString()
                                      + b"\x3a\x03bad")
        with self.assertRaisesRegex(ValueError, "unknown protobuf"):
            importer.selector(value)

    def test_empty_and_malformed_selectors_are_rejected(self):
        for value in (cl.CarrierId(mcc_mnc="26207", gid1=""),
                      cl.CarrierId(mcc_mnc="2620"),
                      cl.CarrierId(mcc_mnc="26207", imsi="26.*")):
            with self.subTest(value=value), self.assertRaises(ValueError):
                importer.selector(value)

    def test_unsupported_policy_is_not_transferred(self):
        values = cs.CarrierConfig()
        for key in ("carrier_volte_available_bool", "carrier_wfc_ims_available_bool",
                    "imsvoice.voice_qos_precondition_supported_bool",
                    "imsemergency.emergency_call_setup_timer_on_current_rat_sec_int",
                    "imsvoice.audio_evs_support_bool", "qns.ims_transport_type_int"):
            values.config.add(key=key, bool_value=True)
        self.assertEqual([], importer.config_elements(values))

    def test_allowed_keys_must_have_the_declared_type(self):
        value = cs.CarrierConfig.Config(key="ims.sip_timer_f_millis_int", text_value="32000")
        with self.assertRaisesRegex(ValueError, "expected int"):
            importer.config_element(value, allowed=importer.TOP_LEVEL_KEYS)

    def test_nested_evs_is_removed_and_amr_preserved(self):
        value = cs.CarrierConfig.Config(key="imsvoice.audio_codec_capability_payload_types_bundle")
        value.bundle.config.add(key="imsvoice.amrwb_payload_type_int_array").int_array.item.append(104)
        value.bundle.config.add(key="imsvoice.evs_payload_type_int_array").int_array.item.append(126)
        element = importer.config_element(value, allowed=importer.TOP_LEVEL_KEYS)
        self.assertEqual(1, len(element))
        self.assertEqual("104", element[0][0].get("value"))

    def test_amr_attributes_are_typed_inside_payload_bundles(self):
        value = cs.CarrierConfig.Config(key="imsvoice.amrwb_payload_description_bundle")
        payload = value.bundle.config.add(key="104")
        payload.bundle.config.add(key="imsvoice.amr_codec_attribute_modeset_int_array").int_array.item.extend([0, 2])
        element = importer.config_element(value, allowed=importer.TOP_LEVEL_KEYS)
        self.assertEqual("104", element[0].get("name"))
        self.assertEqual("2", element[0][0].get("num"))
        payload.key = "128"
        with self.assertRaisesRegex(ValueError, "Invalid RTP"):
            importer.config_element(value, allowed=importer.TOP_LEVEL_KEYS)

    def test_duplicate_config_keys_are_rejected(self):
        value = cs.CarrierConfig()
        value.config.add(key="ims.request_uri_type_int", int_value=0)
        value.config.add(key="ims.request_uri_type_int", int_value=1)
        with self.assertRaisesRegex(ValueError, "Duplicate config"):
            importer.config_elements(value)

    def test_unsafe_names_become_safe_stable_filenames(self):
        name = "../26207SPN=My Carrier"
        self.assertRegex(importer.profile_id(name), r"^carrier_[0-9a-f]{20}$")
        self.assertEqual(importer.profile_id(name), importer.profile_id(name))

    def test_archive_conversion_keeps_order_and_empty_mvnos(self):
        index = cl.CarrierList(version=99)
        index.entry.add(canonical_name="mvno").carrier_id.add(mcc_mnc="26207", gid1="AB")
        index.entry.add(canonical_name="mno").carrier_id.add(mcc_mnc="26207")
        default = cs.CarrierSettings(canonical_name="default", version=1)
        mvno = cs.CarrierSettings(canonical_name="mvno", version=2)
        mno = cs.CarrierSettings(canonical_name="mno", version=3)
        mno.configs.config.add(key="ims.request_uri_type_int", int_value=1)
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "input.zip"
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("CarrierSettings/carrier_list.pb", index.SerializeToString())
                for entry in (default, mvno, mno):
                    z.writestr(f"CarrierSettings/{entry.canonical_name}.pb", entry.SerializeToString())
                z.writestr("../../do-not-extract.apk", b"not executable input")
            files, manifest = importer.convert(archive)
            other_files, other_manifest = importer.convert(archive)
        self.assertEqual(files, other_files)
        self.assertEqual(manifest, other_manifest)
        selections = ET.fromstring(files["carrier_list.xml"])
        self.assertEqual(["mvno", "mno"], [node[0].text for node in selections])
        self.assertEqual(0, len(ET.fromstring(files["profiles/mvno.xml"])))
        self.assertEqual(2, manifest["identity_count"])
        self.assertEqual([], [name for name in files if "apk" in name])


if __name__ == "__main__":
    unittest.main()

# CarrierSettings source provenance

## AOSP profiles

The AOSP carrier profiles originate from ImsStack revision
`5f934dd60abdf29c2581dbdd2dd9dedb76277a18`, before the public profiles were removed.
Their path-filtered upstream history and Apache-2.0 notices are retained. The
profiles also include corrections for duplicate carrier ID 1849 and the TracFone
parent array count.

## Google-derived profiles

The XML under `assets/carrier_settings` is generated from Google CarrierSettings
protobuf data. The importer retains ordered SIM matching and converts only the
configuration keys permitted by `tools/policy_keys.json` for the paired ImsStack
implementation. Empty profiles remain valid selection results.

[carrier_settings_source.json](carrier_settings_source.json) records the source
archive checksum, protobuf member checksums, carrier-list and profile versions,
schema revision, policy checksum and generated output checksums. Use this record
to identify an import and verify its outputs. Source APKs, executable components
and APN/vendor payloads are not included in the generated assets.

The protobuf schemas are based on
[AOSP tools/carrier_settings](https://android.googlesource.com/platform/tools/carrier_settings/+/ff35212d322a3e892605b94fa777c67085d45efd/proto/)
revision `ff35212d322a3e892605b94fa777c67085d45efd`. The importer additionally
supports `CarrierId.mvno_data.iccid = 6` for ICCID-prefix selection. Unknown
carrier-selector fields are rejected. The AOSP selection model uses the first
matching carrier-list entry and SPN-prefix matching; the importer preserves
those semantics.

## Carrier corrections and licensing

Handwritten corrections are maintained separately from generated Google profiles:

- O2 Germany: incoming INVITE QoS waiting in the AOSP O2 profile, and the VoWiFi
  outgoing-call setup timeout in the MCC/MNC override.
- Orange Poland: incoming PRACK and initial final-response handling in the MCC/MNC
  override.

The O2 timeout override retains its original GPL-2.0-only notice. AOSP profiles
and the schema/conversion tools retain their own license notices. Google-derived
data has separate provenance and license metadata, described in
[LICENSES/Google-CarrierSettings-NOTICE.txt](LICENSES/Google-CarrierSettings-NOTICE.txt).
The Apache-2.0 license of the tools and schemas does not relicense that data.

The standalone `org.lineageos.carriersettings` package provides data to ImsStack.
It does not implement Google's `com.google.android.carrier` application or replace
Android's system-wide CarrierConfig service.

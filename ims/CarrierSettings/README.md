# CarrierSettings for ImsStack

CarrierSettings is a separately built, data-only Android APK for the paired
ImsStack fork. It packages carrier profiles and SIM matching rules; it has no
Java/native code, service, network access or launcher activity. Updating its data
does not require rebuilding ImsStack.

| Component | Build target | Responsibility |
| --- | --- | --- |
| CarrierSettings | `CarrierSettings` | AOSP profiles, reviewed Google settings and explicit carrier corrections |
| ImsStack | `ImsStack` | SIM selection, policy precedence, IMS signalling and Android ImsService |
| ImsMedia | `ImsMediaService` / `libimsmedia` | RTP/RTCP, codecs and device audio/video |

The package is `org.lineageos.carriersettings`, installed under `system_ext` and
signed with the product's platform certificate. Its required sysconfig file
installs it for the system user, where ImsStack runs. This app does not replace
Android's system-wide CarrierConfig service or Google's proprietary app.

## Build and product integration

Check this repository out at `packages/apps/CarrierSettings` in an Android source
tree, alongside the matching `packages/modules/ImsStack` and
`packages/modules/ImsMedia` forks. Add to the product makefile:

```make
$(call inherit-product, packages/apps/CarrierSettings/carrier_settings.mk)
```

Then build the APK independently:

```sh
m CarrierSettings
```

The normal image build signs and installs it. The old
`use_carrier_config_ext` Soong option and `imsstack-carrier-config-ext` static
library are no longer used by the matching ImsStack revision. Replace the old
carrier repository in the product manifest; do not retain its makefile option.
Existing ImsStack defaults remain built into ImsStack itself.

ImsStack checks the expected package name, system-app status, matching signing
certificate and `org.lineageos.carriersettings.SCHEMA_VERSION = 1` metadata before
opening assets. It never loads code from this APK. An absent, disabled, untrusted
or incompatible package contributes no carrier assets; built-in/framework policy
and the existing product/test/cache layers still apply.

The loader refreshes each configured SIM after package installation, update,
removal or enable/disable changes. It ignores the temporary removal during APK
replacement and reads a fresh AssetManager for the replacement. Initial shipping
must preinstall the package. A user-installed package alone is intentionally not
a substitute for product integration.

## Data and selection

The snapshot contains:

- 383 canonical profiles from the last public AOSP ImsStack carrier snapshot.
- 1,316 named Google source profiles and 3,452 ordered SIM identities.
- 465 Google profiles with transferable settings; empty profiles remain valid
  matches and prevent the importer from substituting an MNO or sibling MVNO.
- Two late MCC/MNC corrections for O2 Germany and Orange Poland. O2's incoming
  QoS correction also remains in its AOSP profile.

The first matching Google index entry selects one profile. Matching preserves
source order, exact two-/three-digit MNC lengths, GID1 and ICCID prefixes, IMSI
wildcards and literal SPN prefixes. The five observed ICCID selectors use an
explicit schema extension; unknown selector fields fail conversion.

ImsStack applies configuration in this order, with later layers taking priority:

1. Its built-in defaults and this APK's AOSP carrier/parent profiles.
2. Android framework carrier configuration and existing optional public profiles.
3. The reviewed Google default and the selected named Google profile.
4. This APK's explicit `carrier_config_override_mccmnc_<MCC><MNC>.xml` corrections.
5. Product resource overrides, then the explicit framework AP IMS hidden bundle.
6. Existing test overrides and no-SIM cache handling.
7. Final product/media capability constraints, including unsupported EVS.

`tools/policy_keys.json` permits 54 top-level and 7 nested fields for supported
SIP timing/transport, registration, identity formatting, AMR/DTMF, voice-session,
SMS fallback and UT settings. It excludes broad service enablement, emergency
policy, QoS/preconditions, video/RTT/RCS, QNS/IWLAN, APNs, vendor payloads and EVS.
New keys require a reviewed ImsStack consumer and matching type. Google's APK,
ODEX/VDEX and proprietary services are not included.

## Updating and validating the snapshot

Building the committed XML requires neither Python nor the source data. To
refresh Google-derived profiles, use Python 3.11+, the pinned protobuf tooling
and a source archive with the supported layout:

- A single `carrier_list.pb` containing the ordered carrier index.
- Settings protobufs for the default profile and every profile referenced by the
  index. Individual files contain `CarrierSettings`; `others.pb`, when present,
  contains `MultiCarrierSettings`.
- Protobuf files stored directly as ZIP members, optionally inside directories.
  The importer does not open nested APKs or archives.

Run from the repository root, substituting paths to your source data and temporary
Python environment:

```sh
CARRIER_SETTINGS_INPUT=/path/to/carrier-settings-input.zip
CARRIER_SETTINGS_VENV=/path/to/temp/carriersettings-venv
python3 -m venv "$CARRIER_SETTINGS_VENV"
"$CARRIER_SETTINGS_VENV/bin/pip" install -r tools/requirements.txt
"$CARRIER_SETTINGS_VENV/bin/python" tools/import_carrier_settings.py "$CARRIER_SETTINGS_INPUT"
"$CARRIER_SETTINGS_VENV/bin/python" tools/import_carrier_settings.py "$CARRIER_SETTINGS_INPUT" --check
"$CARRIER_SETTINGS_VENV/bin/python" -m unittest discover -s tools -p 'test_*.py'
python3 check_carrier_configs.py
```

Conversion reads protobuf members without extracting files or executing code.
It rejects unknown selectors, wrong types, duplicate keys, unresolved profiles
and oversized inputs. `--check` regenerates and compares every XML file and
source record without writing. Reproducing the committed source record requires
the archive identified by its recorded checksum; repackaging the same protobufs
changes the archive checksum.

The standard-library validator checks XML structure, selectors, array sizes,
inheritance, profile references and output hashes. Handwritten corrections remain
outside generated files. Review changes to the generated profiles and source
record together before publishing an update.

When changing the protobuf schema, regenerate bindings before refreshing data:

```sh
"$CARRIER_SETTINGS_VENV/bin/python" -m grpc_tools.protoc \
    -I tools/proto --python_out=tools/proto \
    tools/proto/carrier_list.proto tools/proto/carrier_settings.proto
```

For every published APK update, increment `android:versionCode` and update
`android:versionName` in `AndroidManifest.xml`. These are app release versions;
the much larger Google data versions belong in `carrier_settings_source.json`.
Keep schema metadata at 1 for compatible data-only changes. A format change needs
a coordinated loader revision. Sign updates with the same product certificate.

## Standalone packaging check

Without a full Android checkout, Android SDK aapt2 can build an unsigned data APK:

```sh
python3 tools/build_data_apk.py \
    --aapt2 /path/to/android-sdk/build-tools/35.0.0/aapt2 \
    --android-jar /path/to/android-sdk/platforms/android-35/android.jar \
    --output /path/to/temp/CarrierSettings-unsigned.apk
```

The tool verifies every packaged asset byte and rejects executable payloads. It
uses API 35 for the SDK packaging check; production Soong builds use the current
platform SDK. It does not sign, install or validate runtime package handling.

Use the importer tests, XML validator and packaging check when refreshing data.
Validate runtime signature checks, live package updates and IMS behavior with
the paired ImsStack Android suites and product/device regression tests. The
packaging check covers asset integrity; runtime behavior depends on the product
integration described above.

[Source provenance](SOURCE.md) describes the data sources, public schema and
carrier corrections. [carrier_settings_source.json](carrier_settings_source.json)
records individual versions, input checksums and generated output checksums.

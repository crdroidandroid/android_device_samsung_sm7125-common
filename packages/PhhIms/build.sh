#!/usr/bin/env bash

set -euo pipefail

if [ -d /usr/lib/jvm/java-8-openjdk-amd64/bin/ ];then
export PATH=/usr/lib/jvm/java-8-openjdk-amd64/bin/:$PATH
fi

if [ -z "${ANDROID_HOME:-}" ];then
    export ANDROID_HOME=$PWD/sdk
fi

gradleTarget=assembleDebug
target=debug
file=app-debug
if [ "${1:-}" = "release" ];then
    gradleTarget=assembleRelease
    target=release
    file=app-release-unsigned
fi
./gradlew $gradleTarget

input_apk="./app/build/outputs/apk/$target/${file}.apk"
output_apk="${PHHIMS_OUTPUT_APK:-app.apk}"
signing_cert="${PHHIMS_SIGNING_CERT:-}"
signing_key="${PHHIMS_SIGNING_KEY:-}"

if [ -n "$signing_cert" ] || [ -n "$signing_key" ]; then
    if [ -z "$signing_cert" ] || [ -z "$signing_key" ]; then
        echo "Both PHHIMS_SIGNING_CERT and PHHIMS_SIGNING_KEY are required" >&2
        exit 2
    fi
    if [ ! -r "$signing_cert" ] || [ ! -r "$signing_key" ]; then
        echo "Configured signing certificate or key is not readable" >&2
        exit 2
    fi

    signapk_jar="${PHHIMS_SIGNAPK_JAR:-signapk/signapk.jar}"
    LD_LIBRARY_PATH="${PHHIMS_SIGNAPK_LIBRARY_PATH:-./signapk/}" \
        java -jar "$signapk_jar" "$signing_cert" "$signing_key" \
        "$input_apk" "$output_apk"
    echo "Signed APK written to $output_apk"
else
    cp "$input_apk" "$output_apk"
    echo "No platform signing key supplied; APK copied to $output_apk without platform signing"
fi

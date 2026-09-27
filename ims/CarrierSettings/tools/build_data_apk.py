#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build an unsigned, data-only CarrierSettings APK using Android SDK aapt2.

Production builds use the Soong CarrierSettings target and the product's platform
certificate. This standalone check does not sign or install the package.
"""
import argparse
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aapt2", type=Path, required=True)
    parser.add_argument("--android-jar", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.aapt2), "link", "-o", str(args.output),
                    "--manifest", str(ROOT / "AndroidManifest.xml"),
                    "-I", str(args.android_jar), "-A", str(ROOT / "assets"),
                    "--min-sdk-version", "35", "--target-sdk-version", "35"], check=True)
    expected = {"assets/" + p.relative_to(ROOT / "assets").as_posix(): p.read_bytes()
                for p in (ROOT / "assets").rglob("*") if p.is_file()}
    # Android ZIP entry names are UTF-8; aapt2 may omit the ZIP UTF-8 flag.
    with zipfile.ZipFile(args.output, metadata_encoding="utf-8") as apk:
        actual = {n for n in apk.namelist() if n.startswith("assets/") and not n.endswith("/")}
        if actual != expected.keys() or any(apk.read(n) != data for n, data in expected.items()):
            raise SystemExit("Packaged assets differ from the source tree")
        if any(n.endswith(".dex") or n.startswith("lib/") for n in apk.namelist()):
            raise SystemExit("Unexpected executable content in data APK")
    print(f"Built {args.output}: {len(expected)} verified assets; unsigned, no executable code")


if __name__ == "__main__":
    main()

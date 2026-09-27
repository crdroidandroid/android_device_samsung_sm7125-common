#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the real audio player against a controlled AAudio/timer backend, with ASan/UBSan.
Requires clang++ and an existing GoogleTest source checkout. No Android HAL is used.
All generated headers and binaries go in the explicit output directory.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--googletest", type=Path, required=True, help="GoogleTest directory containing src/gtest-all.cc")
p.add_argument("--output-dir", type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
host = root / "tests/host"
core = root / "service/src/com/android/telephony/imsmedia/lib/libimsmedia/core"
a.output_dir.mkdir(parents=True, exist_ok=True)
inc = a.output_dir / "include"
inc.mkdir(exist_ok=True)
wrappers = ["aaudio/AAudio.h", "media/NdkMediaCodec.h", "media/NdkMediaFormat.h",
            "cutils/properties.h", "utils/Errors.h", "ImsMediaTimer.h", "ImsMediaAudioUtil.h"]
for name in wrappers:
    target = inc / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text('#pragma once\n#include "fake_audio_platform.h"\n')
# Use the actual enum definitions; omit the unrelated Binder configuration types.
defines = (core / "include/ImsMediaDefine.h").read_text()
enums = [re.search(r"enum " + name + r"\s*\{.*?\};", defines, re.S).group(0)
         for name in ["kAudioCodecType", "kEvsBandwidth", "kRtpPayloadHeaderMode"]]
(inc / "ImsMediaDefine.h").write_text('#pragma once\n#include "fake_audio_platform.h"\n' + "\n".join(enums))
(inc / "ImsMediaTrace.h").write_text('#pragma once\n' + ''.join(
    f'#define IMLOG{level}{suffix}{n}(...) ((void)0)\n'
    for level in 'DEIWT' for suffix in ['', '_PACKET'] for n in range(10)))
cmd = ["clang++", "-std=c++20", "-O1", "-g", "-pthread", "-fsanitize=address,undefined",
       "-fno-omit-frame-pointer", "-include", str(inc / "ImsMediaDefine.h")]
for directory in [inc, host, core / "audio/android/include", core / "interface/utils",
                  a.googletest / "include", a.googletest]:
    cmd.append("-I" + str(directory))
cmd += [str(core / "audio/android/ImsMediaAudioPlayer.cpp"),
        str(host / "audio_player_recovery_test.cpp"),
        str(a.googletest / "src/gtest-all.cc"), str(a.googletest / "src/gtest_main.cc"),
        "-o", str(a.output_dir / "audio-recovery-tests")]
env = {**os.environ, "TMPDIR": str(a.output_dir), "ASAN_OPTIONS": "halt_on_error=1",
       "UBSAN_OPTIONS": "halt_on_error=1"}
subprocess.run(cmd, check=True, env=env)
subprocess.run([str(a.output_dir / "audio-recovery-tests")], check=True, env=env)

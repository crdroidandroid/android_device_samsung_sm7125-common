#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run production utility/worker tests with Android logging/priority shims and ASan/UBSan."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--googletest", type=Path, required=True,
                    help="GoogleTest directory containing src/gtest-all.cc")
parser.add_argument("--output-dir", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
lib = root / "service/src/com/android/telephony/imsmedia/lib/libimsmedia"
tests = root / "tests/native/service/src/com/android/telephony/imsmedia/lib/libimsmedia"
inc = args.output_dir / "include"
(inc / "mediautils").mkdir(parents=True, exist_ok=True)
(inc / "mediautils/SchedulingPolicyService.h").write_text(
    "#pragma once\n#include <sys/types.h>\n#include <cerrno>\n"
    "namespace android { inline int requestPriority(pid_t, pid_t, int, bool, bool) "
    "{ return 0; } }\n")
(inc / "ImsMediaTrace.h").write_text("#pragma once\n" + "".join(
    f"#define IMLOG{level}{n}(...) ((void)0)\n"
    for level in "DEIWT" for n in range(10)))
names = ["ImsMediaBinaryFormat", "ImsMediaBitReader", "ImsMediaBitWriter",
         "ImsMediaImageRotate", "IImsMediaThread"]
command = ["clang++", "-std=c++20", "-g", "-O1", "-fsanitize=address,undefined",
           "-fno-omit-frame-pointer", "-pthread"]
for directory in [inc, lib / "core/include/utils", lib / "core/interface/utils",
                  args.googletest / "include", args.googletest]:
    command.append("-I" + str(directory))
command += [str(args.googletest / "src/gtest-all.cc"),
            str(args.googletest / "src/gtest_main.cc")]
command += [str(lib / "core/utils" / f"{name}.cpp") for name in names]
command += [str(tests / "core/utils" / f"{name}Test.cpp") for name in names]
command += ["-o", str(args.output_dir / "media-utility-tests")]
env = {**os.environ, "TMPDIR": str(args.output_dir), "UBSAN_OPTIONS": "halt_on_error=1",
       "ASAN_OPTIONS": "halt_on_error=1"}
subprocess.run(command, check=True, env=env)
subprocess.run([str(args.output_dir / "media-utility-tests")], check=True, env=env)

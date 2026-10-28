#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Wouter van Nifterick
"""A lane kernel's object defines no function outside its instruction set's namespace (src/kernel/README.md).

    python tools/check_isa.py avx2 build/vop3host/isa2/*.obj
    python tools/check_isa.py avx2 build/cmake/CMakeFiles/fsvr_kernel_2.dir/src/kernel/isa/*.o

An inline function a kernel file instantiates from a header becomes a COMDAT the linker may pick for the whole
program; compiled under /arch:AVX2 it would then run on a machine without it. This reads the object's symbols
with dumpbin (MSVC) or nm and fails on any defined function whose name is not in the namespace.
"""
import glob
import shutil
import subprocess
import sys


def dumpbin():
    """dumpbin on the path, else the newest one a Visual Studio 2022 install has."""
    if shutil.which("dumpbin"):
        return "dumpbin"
    found = sorted(glob.glob(r"C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe"))
    return found[-1] if found else None


def functions(obj):
    """Defined function symbols, by name."""
    if obj.endswith(".obj") and dumpbin():
        out = subprocess.run([dumpbin(), "/symbols", obj], capture_output=True, text=True).stdout
        names = []
        for line in out.splitlines():
            # 00A 00000000 SECT4  notype ()    External     | ?filter_bank@avx2@kernel@fsvr@@YAX...
            if "SECT" in line and "()" in line and "|" in line:
                names.append(line.split("|", 1)[1].strip().split()[0])
        return names
    out = subprocess.run(["nm", "--defined-only", obj], capture_output=True, text=True).stdout
    return [line.split()[-1] for line in out.splitlines() if line.split() and line.split()[-2] in ("T", "t", "W")]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ns, patterns = sys.argv[1], sys.argv[2:]
    objs = [o for p in patterns for o in glob.glob(p)]
    if not objs:
        print(f"check_isa: no objects match {patterns}")
        return 2
    bad = 0
    for obj in objs:
        for name in functions(obj):
            if ns not in name:
                print(f"{obj}: {name} is outside the {ns} namespace")
                bad += 1
    print(f"check_isa: {len(objs)} objects, {'ok' if not bad else f'{bad} functions outside {ns}'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

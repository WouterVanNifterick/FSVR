#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Wouter van Nifterick
"""Write rom_tables.cpp: the parts of the FS1R v1.20 EPROM the effect host reads.

    python tools/extract_vop3_rom.py <eprom.bin>      the 2 MB image, CPU byte order
"""
import hashlib
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "src/fs1r/firmware/vop3host/rom_tables.cpp"
BASE = 0x200000                               # where the EPROM sits in the CPU's map
RANGES = [(0x35D732, 0x388F18),               # effect programs, constants, tables and parameter descriptors
          (0x3895F2, 0x3895F2 + 41 * 14)]     # controller destination to insertion parameter, per type

rom = Path(sys.argv[1]).read_bytes()
if not hashlib.sha256(rom).hexdigest().startswith("3be337ed29bc44c0"):
    sys.exit("not the FS1R v1.20 EPROM in CPU byte order")
out = ["// FS1R v1.20 EPROM: the ranges the effect host reads. Yamaha's data.",
       "// Written by tools/extract_vop3_rom.py. Do not edit.", "",
       '#include "rom_tables.h"', "", "namespace fs1r::vop3host::rom {", ""]
for i, (a, b) in enumerate(RANGES):
    data = rom[a - BASE:b - BASE]
    out.append(f"static const uint8_t RANGE{i}[{b - a}] = {{  // 0x{a:06X}..0x{b:06X}")
    out += ["    " + ",".join(f"0x{x:02X}" for x in data[k:k + 32]) + "," for k in range(0, len(data), 32)]
    out += ["};", ""]
out.append("const Range RANGES[RANGE_COUNT] = {")
out += [f"    {{0x{a:06X}, 0x{b:06X}, RANGE{i}}}," for i, (a, b) in enumerate(RANGES)]
out += ["};", "", "}  // namespace fs1r::vop3host::rom", ""]
OUT.write_bytes("\n".join(out).encode())
print(f"wrote {OUT.name}: {sum(b - a for a, b in RANGES)} bytes in {len(RANGES)} ranges")

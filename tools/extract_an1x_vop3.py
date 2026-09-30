#!/usr/bin/env python3
"""Extract the AN1x's VOP3 AN-voice program from an1x_v104.bin (byte-swapped dump).

FUN_00056028 uploads images of the form: u16 count, count x 6 words (coef, r10 | byte<<4, r9, r8, r7, r6),
then (count+3)/4 x 2 words of delay-memory slot data, then 15 words of bus setup.
Writes docs/vop3_an1x/<name>.bin in the 8-word row format of docs/vop3_an (r0C, slot, r0B, r10, r9, r8, r7, r6)
so tools/vop3_disasm.py reads it unchanged.
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT.parent / "FS1R_DISASM" / "roms" / "an1x_v104.bin"
OUT = ROOT / "docs" / "vop3_an1x"
IMAGES = {"voice": 0xD0638, "boot": 0xD2058}   # LDI in FUN_00056b5c / FUN_00056162


def main():
    raw = ROM.read_bytes()
    rom = bytearray(len(raw)); rom[0::2] = raw[1::2]; rom[1::2] = raw[0::2]
    OUT.mkdir(exist_ok=True)
    for name, addr in IMAGES.items():
        n = struct.unpack_from(">H", rom, addr)[0]
        rows = [struct.unpack_from(">6H", rom, addr + 2 + 12 * i) for i in range(n)]
        o = addr + 2 + 12 * n
        slots = [struct.unpack_from(">2H", rom, o + 4 * i) for i in range((n + 3) // 4)]
        out = bytearray()
        for i, (coef, r10b, r9, r8, r7, r6) in enumerate(rows):
            slot = slots[i >> 2][i & 1] if (r10b & 7) else 0   # ponytail: slot word pairing (lo/hi) unverified
            out += struct.pack(">8H", (r10b >> 4) & 0xF, slot, coef, r10b & 7, r9, r8, r7, r6)
        (OUT / f"{name}.bin").write_bytes(out)
        print(f"{name}: {n} steps from {addr:#x} -> {OUT / (name + '.bin')}")


if __name__ == "__main__":
    main()

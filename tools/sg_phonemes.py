#!/usr/bin/env python3
"""Decode the PLG100-SG's phoneme formant table (docs/findings.md 2026-09-29).

    python tools/sg_phonemes.py [PLG100-SG.BIN] [--singer N]

FUN_0000E992(n) returns 0x1A068 + n * 0xFB8 for singers 0-7: a 24-byte header, then 125 records of 32
bytes. FUN_0000C0F6 reads a record's formant words as `hi * 256 + lo * 2`, the FS1R Fseq frame encoding,
four at bytes 3-10 and a fifth at bytes 1-2, and FUN_0000F1DC turns a word into the YMF293's frequency
number at 1024 units per octave. The Hz column assumes the FS1R's own pitch word origin (word 0x7FFF is
23982 Hz, measured off 12_fseqlevel); the ROM does not fix the SG's absolute scale, only its ratios.
"""
import sys
from pathlib import Path

ROM = Path(__file__).resolve().parents[1].parent / "FS1R_DISASM" / "roms" / "PLG100-SG.BIN"
BASE, STRIDE, HEADER, RECORDS, RECLEN = 0x1A068, 0xFB8, 24, 125, 32


def word(r, i):
    return r[i] * 256 + r[i + 1] * 2


def hz(w):
    return 23982 * 2 ** ((w - 0x7FFF) / 1024)


def singer(rom, n):
    b = BASE + n * STRIDE
    return rom[b:b + HEADER], [rom[b + HEADER + k * RECLEN:b + HEADER + (k + 1) * RECLEN] for k in range(RECORDS)]


def main():
    args = sys.argv[1:]
    only = None
    if "--singer" in args:
        i = args.index("--singer")
        only = int(args[i + 1])
        del args[i:i + 2]
    rom = Path(args[0] if args else ROM).read_bytes()
    # the tables FUN_0000F1DC converts through: 2^(i/1024) mantissa and 2^oct, both exact
    t1 = [int.from_bytes(rom[0x22A28 + 2 * i:0x22A2A + 2 * i], "big") for i in range(1024)]
    assert all(t1[i] == round(65536 * (2 ** (i / 1024) - 1)) for i in range(1024)), "not the SG image"
    for n in range(8):
        if only is not None and n != only:
            continue
        head, recs = singer(rom, n)
        print(f"# singer {n}  header {head.hex(' ')}")
        for k, r in enumerate(recs):
            ws = [word(r, 3), word(r, 5), word(r, 7), word(r, 9), word(r, 1)]
            print(f"{n} {k:3d} id={r[0]:02x}  " + "  ".join(f"{w:04x}={hz(w):5.0f}" for w in ws)
                  + f"   rest {r[11:].hex(' ')}")


if __name__ == "__main__":
    main()

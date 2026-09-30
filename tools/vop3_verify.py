#!/usr/bin/env python3
"""Dataflow check of the VOP3 field decode against the EX5 MEG listing and the firmware's patch tables.

Three tests:
  1. The constant (reg 0xB) is live on class-2 steps: the firmware patches it there (FS1R filter tables at
     0x374D64..0x374E84, the AN1x scene-parameter map) and it is non-zero only where rd-en is clear.
  2. r[] is one 7-bit space with a per-voice stride: FS1R filter channel c reads channel 0's register + 6c
     (through 0x40 and beyond), the AN1x reads r[1 + 5k + v] with one step shape per k in all 5 voices for
     k = 0..12. So w[] (6-bit wdst/rsrc) is a separate file, not r[40..7f]: a "97% of upper reads have a
     w[] writer" count is a coincidence of both files being dense, and the stride test is what settles it.
  3. The MEG listing passes the same read-has-writer test 260/260, the reference the VOP3 tests are shaped on.

    python tools/vop3_verify.py            # prints the numbers and asserts the ISA doc's claims
"""
import collections
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vop3_disasm import fields, load  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT.parent / "FS1R_DISASM" / "roms" / "fs1r_v120_eprom_cpuview.bin"
FS1R_TABLES = [0x374D64, 0x374D84, 0x374DA4, 0x374DC4, 0x374DE4, 0x374E04, 0x374E44, 0x374E64, 0x374E84]


def images():
    d = ROOT / "docs"
    fs, _, _ = load(d / "vop3/program_0.bin")
    fc = list(struct.unpack(">512H", (d / "vop3/coefficients_0.bin").read_bytes()))
    an, ac, _ = load(d / "vop3_an1x/voice.bin", an=True)
    pl, pc, _ = load(d / "vop3_an/mode0.bin", an=True)             # uploaded at 0x004 over the base image
    pb, pbc, _ = load(d / "vop3_an/base.bin", an=True)
    pl, pc = list(pb[:4]) + list(pl) + list(pb[4 + len(pl):]), list(pbc[:4]) + list(pc) + list(pbc[4 + len(pl):])
    return {"fs1r": (fs, fc), "an1x": (an, ac), "plg150an": (pl, pc)}


def patched_steps():
    """Steps the firmware rewrites the constant of: FS1R filter tables (u16 step per channel), AN1x param map."""
    out = {"fs1r": set(), "an1x": set()}
    if ROM.exists():
        rom = ROM.read_bytes()
        for t in FS1R_TABLES:
            out["fs1r"] |= set(struct.unpack_from(">16H", rom, t - 0x200000))
    for line in open(ROOT / "docs/an1x_param_map.md", encoding="utf-8"):
        m = re.search(r"steps=\[([0-9a-f ]+)\]|step=([0-9a-f]+)/([0-9a-f]+)", line)
        if m:
            sts = m.group(1).split() if m.group(1) else [m.group(2), m.group(3)]
            out["an1x"] |= {int(s, 16) for s in sts if 0 < int(s, 16) < 512}
    return out


def channel_stride(steps):
    """FS1R filter: r[] reads of channel c (group g = c // 4) against the same-position channel of group 0 at
    the same slot of the 31-step channel pattern. Every read is that register + 24g (the per-channel stride
    of 6 times four channels a group) or equal (a group-shared register such as r[61..63]/r[68]): 444/444,
    running through 0x40 (group 2 reads r[3b..4d] where group 0 reads r[0b..1d])."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from vop3_interp import channel_steps
    F = [fields(*s) for s in steps]
    groups = [0x10, 0x8C, 0x108, 0x184]

    def reads(c):
        out = {}
        for n, st in enumerate(channel_steps(groups[c // 4], c % 4)):
            if any(steps[st]) and F[st]["f6a"] in (2, 3):
                for side in ("ra", "rb"):
                    if F[st][side]:
                        out[(n, side)] = F[st][side]
        return out

    hit = tot = 0
    for c in range(4, 16):                    # group g's channel against group 0's channel in the same position
        base = reads(c % 4)
        for key, r in reads(c).items():
            if key in base:
                tot += 1
                hit += r - base[key] in (0, 6 * (c - c % 4))
    return hit, tot


def input_window(steps, voices=5, stride=5, first=1, last=0x41):
    """AN1x: r[first + stride*k + v] is read by the same step shape (side, op) in all `voices` voices for
    every k; returns (consistent groups, total groups). Holds for k = 0..12, i.e. r[01..41], so the
    per-voice register space runs straight through 0x40; above it the registers are scheduler temporaries
    (the VCO/mixer handlers' targets read r[4d..64] in no per-voice order)."""
    F = [fields(*s) for s in steps]
    shape = collections.defaultdict(set)
    for f, s in zip(F, steps):
        if f["f6a"] in (2, 3) and any(s):
            for side in ("ra", "rb"):
                r = f[side]
                if first <= r <= last:
                    k, v = divmod(r - first, stride)
                    shape[(k, v)].add((side, f["op7"]))
    ok = tot = 0
    for k in range((last - first + 1) // stride):
        tot += 1
        ok += len({frozenset(shape[(k, v)]) for v in range(voices)}) == 1
    return ok, tot


def const_liveness(steps, coef, patched):
    """Fraction of class-2 steps with a live constant, split by rd-en."""
    F = [fields(*s) for s in steps]
    n = collections.Counter(); k = collections.Counter()
    for i, (f, s) in enumerate(zip(F, steps)):
        if f["f6a"] == 2 and any(s):
            n[f["rd_en"]] += 1
            k[f["rd_en"]] += bool(coef[i]) or i in patched
    return k[0] / n[0], k[1] / max(n[1], 1)


def meg_coverage(path):
    wr = set(); rd = collections.Counter()
    for line in open(path):
        body = line.split(None, 1)[1] if " " in line.strip() else ""
        for part in body.split(";"):
            m = re.match(r"\s*([rt])([0-9a-f]+) = (.*)", part)
            rhs = m.group(3) if m else part
            if m:
                wr.add((m.group(1), int(m.group(2), 16)))
            for kind, num in re.findall(r"\b([rt])([0-9a-f]{1,2})\b", rhs):
                rd[(kind, int(num, 16))] += 1
    return sum(v for key, v in rd.items() if key in wr), sum(rd.values())


def main():
    pat = patched_steps()
    hit, tot = meg_coverage(ROOT / "docs/ex5_meg_an.txt")
    print(f"MEG      r/t reads with a writer in the program: {hit}/{tot}")
    assert hit == tot
    imgs = images()
    for name, (steps, coef) in imgs.items():
        lo, hi = const_liveness(steps, coef, pat.get(name, set()))
        print(f"{name:9s} constant live on class 2: rd-en clear {lo:.0%}, rd-en set {hi:.0%}")
        assert lo > 0.4 and hi < lo / 2, name                   # the constant is a class-2 operand, mostly exclusive with rd-en
        if name in pat and pat[name]:
            F = [fields(*s) for s in steps]
            tagged = [F[s] for s in pat[name] if F[s]["f6a"] == 2]
            assert tagged and all(f["rd_en"] == 0 for f in tagged), name   # the firmware never patches k into an rd-en step
    hit, tot = channel_stride(imgs["fs1r"][0])
    print(f"fs1r      per-group r[] stride 24: {hit}/{tot} reads are group 0's register + 24g or shared")
    assert hit == tot
    ok, tot = input_window(imgs["an1x"][0])
    print(f"an1x      per-voice r[1+5k+v], k=0..12: {ok}/{tot} groups read by one step shape in all 5 voices")
    assert ok == tot
    print("ok")


if __name__ == "__main__":
    main()

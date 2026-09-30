#!/usr/bin/env python3
"""Dataflow check of the VOP3 field decode against the EX5 MEG listing and the firmware's patch tables.

Two tests, the same ones the decoded MEG program passes by construction:
  1. Every operand a step reads should have a writer somewhere in the program (or be CPU state). This is
     what places the write area: with wdst = r[0x40 | n], 97% of upper-half reads have a writer on every
     VOP3 image; read as a separate w[] file only rd-en reads do.
  2. The constant (reg 0xB) is live on class-2 steps: the firmware patches it there (FS1R filter tables at
     0x374D64..0x374E84, the AN1x scene-parameter map) and it is non-zero only where rd-en is clear.

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


def read_coverage(steps):
    """(hits, total) for upper-half rA/rB reads: writer as r[0x40|wdst], or as a class-1 load (the FS1R loads its
    cutoff constants into r[61..63] and multiplies them by MAC results in r[7e], so both are high there)."""
    F = [fields(*s) for s in steps]
    live = [(f, s) for f, s in zip(F, steps) if any(s)]
    w = {f["f6c"] for f, s in live if f["f6a"] in (2, 3) and f["f6c"]}
    c1 = {f["ra"] for f, s in live if f["f6a"] == 1}
    reads = [r for f, s in live if f["f6a"] in (2, 3) for r in (f["ra"], f["rb"]) if r >= 0x40]
    return sum(r - 0x40 in w for r in reads), sum(r in c1 for r in reads), len(reads)


def const_liveness(steps, coef, patched):
    """Fraction of class-2 steps with a live constant, split by rd-en."""
    F = [fields(*s) for s in steps]
    n = collections.Counter(); k = collections.Counter()
    for i, (f, s) in enumerate(zip(F, steps)):
        if f["f6a"] == 2 and any(s):
            n[f["rd_en"]] += 1
            k[f["rd_en"]] += bool(coef[i]) or i in patched
    return k[0] / n[0], k[1] / max(n[1], 1)


def input_window(steps, voices=5, stride=5, first=1, last=0x3c):
    """AN1x: every lower-half read r[first + stride*k + v] should be read by the same step shape in all
    `voices` voices (op and operand side); returns (consistent input groups, total groups)."""
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
    for name, (steps, coef) in images().items():
        w, c1, tot = read_coverage(steps)
        lo, hi = const_liveness(steps, coef, pat.get(name, set()))
        print(f"{name:9s} upper reads: writer as r[40|wdst] {w}/{tot} ({w/tot:.0%}), as class-1 dest {c1}/{tot} ({c1/tot:.0%});"
              f"  constant live: rd-en clear {lo:.0%}, rd-en set {hi:.0%}")
        assert w / tot > 0.75, name                             # the write area is the upper half of the register file
        assert lo > 0.4 and hi < lo / 2, name                   # the constant is a class-2 operand, mostly exclusive with rd-en
        if name in pat and pat[name]:
            F = [fields(*s) for s in steps]
            tagged = [F[s] for s in pat[name] if F[s]["f6a"] == 2]
            assert tagged and all(f["rd_en"] == 0 for f in tagged), name   # the firmware never patches k into an rd-en step
    ok, tot = input_window(images()["an1x"][0])
    print(f"an1x      input window r[1+5k+v]: {ok}/{tot} inputs read by one step shape in all 5 voices")
    assert ok == tot
    print("ok")


if __name__ == "__main__":
    main()

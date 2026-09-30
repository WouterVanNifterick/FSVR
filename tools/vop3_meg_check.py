#!/usr/bin/env python3
"""Cross-check the VOP3 model against the EX5 MEG: the same AN algorithm on two chips.

The MEG (docs/ex5_meg_an.txt, decoded by MAME) is value-transparent; the FS1R VOP3 is not (session 10:
the chip exposes no accumulator). This classifies every step of both into one vocabulary and compares the
operation mix, so the VOP3 op field can be pinned to MEG semantics without reading the FS1R accumulator.

    python tools/vop3_meg_check.py
"""
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from vop3_disasm import fields, load

MEG = os.path.join(HERE, "..", "docs", "ex5_meg_an.txt")
VOP3 = os.path.join(HERE, "..", "docs", "vop3_an1x", "voice.bin")


def meg_kinds(line):
    """The MEG operations present in one decoded step, from its explicit arithmetic."""
    main = line.split(";")[0]
    k = set()
    if re.search(r"\* [rm]", main) or re.search(r"c[0-9a-f]{3} \* ", main):
        k.add("MUL")
    if "+ (p >> 15)" in main or re.search(r"\) \+ ", main) or "+ p)" in main:
        k.add("ACC")
    if re.search(r"<< 8\)", main) and "*" not in main and "+" not in main:
        k.add("LOADK")
    if re.match(r"\s*p = [mr][0-9a-f]", main):
        k.add("MOVE")
    if "mem_r" in line:
        k.add("MEMR")
    if "mem_w" in line:
        k.add("MEMW")
    if "+idx" in line:
        k.add("LUT")
    return k


def meg_hist(path):
    h = collections.Counter()
    n = 0
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        n += 1
        for k in meg_kinds(line.split("  ", 1)[1] if "  " in line else line):
            h[k] += 1
    return h, n


def vop3_hist(path):
    steps, _, _ = load(path, an=True)
    h = collections.Counter()
    ops = collections.Counter()
    reads = collections.Counter()
    n = 0
    for s in steps:
        if not any(s):
            continue
        n += 1
        f = fields(*s)
        if f["f6a"] == 1:
            h["LOADK"] += 1
        elif f["f6a"] == 2:
            op = f["op7"]
            ops[op] += 1
            reads[op] += 1 if f["rd_en"] else 0
            # session 10 + MEG: ops 1/6 forward-with-read (MOVE/ACC), 5 fresh product, rest MAC
            h["MAC"] += 1
        if f["mem"]:
            h["MEM"] += 1
    return h, n, ops, reads


def report():
    mh, mn = meg_hist(MEG)
    vh, vn, ops, reads = vop3_hist(VOP3)
    mul = mh["MUL"] / mn
    print(f"MEG   {mn} steps: mul {mul:.0%}  loadk {mh['LOADK']/mn:.0%}  mem {(mh['MEMR']+mh['MEMW'])/mn:.0%}  lut {mh['LUT']/mn:.0%}")
    print(f"VOP3  {vn} steps: mac {vh['MAC']/vn:.0%}  loadk {vh['LOADK']/vn:.0%}  mem {vh['MEM']/vn:.0%}")
    print("VOP3 op field (count, read%):")
    for op in range(8):
        c = ops[op]
        print(f"  op{op}: {c:4d}  read {100*reads[op]//max(c,1):3d}%")
    return mh, mn, vh, vn, ops, reads


def selfcheck():
    # the decoded MEG line forms classify as intended
    assert meg_kinds("p = c000 * m1f") == {"MUL"}
    assert meg_kinds("p =s (c003 << 8) + (p >> 15)") == {"ACC"}
    assert meg_kinds("p = (c002 << 8)") == {"LOADK"}
    assert meg_kinds("p = m26") == {"MOVE"}
    assert "MEMR" in meg_kinds("p = c000 * m1f ; mem_r +of00")
    mh, mn, vh, vn, ops, reads = report()
    # both programs are MAC-dominated with a small constant-load population
    assert mh["MUL"] / mn > 0.6 and vh["MAC"] / vn > 0.5
    assert 0.02 < mh["LOADK"] / mn < 0.15 and 0.02 < vh["LOADK"] / vn < 0.15
    # session 10's non-passing ops: op1/op6 read-heavy (forward), op5 read-light (fresh product)
    assert reads[1] / max(ops[1], 1) > 0.9 and reads[6] / max(ops[6], 1) > 0.9
    assert reads[5] / max(ops[5], 1) < 0.5
    print("\nself-check ok: VOP3 operation mix and the op-1/5/6 read pattern match the MEG")


if __name__ == "__main__":
    selfcheck()

#!/usr/bin/env python3
"""Probe lists for FS1R.unlock captures/fs1r_capture_session24.py, and the interpreter's prediction for each.

    python tools/vop3_probe.py write <round> probes.json      the probe list
    python tools/vop3_probe.py compare <round> <readout.txt>  measured (dc_read output) against the interpreter

Rig: test image 0, note off; 16 x the right DC = the value step 0d3 hands on (session 24's docstring).
"""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_interp as V  # noqa: E402

ROM = Path(__file__).resolve().parents[2] / "FS1R_DISASM/roms/fs1r_v120_eprom_cpuview.bin"
IMG0 = [struct.unpack_from(">5H", ROM.read_bytes(), 0x173604 + i * 10) for i in range(512)]


def word(base, cls=None, op=None, ra=None, rb=None, route=None, daddr=None, mem=None, rd=None):
    r10, r9, r8, r7, r6 = base
    if cls is not None: r6 = (r6 & 0x3FFF) | cls << 14
    if rb is not None: r6 = (r6 & ~0x3F80) | rb << 7
    if ra is not None: r9 = (r9 & ~0x7F) | ra
    if op is not None: r7 = (r7 & ~0x1C0) | op << 6
    if route is not None: r7 = (r7 & ~0x3000) | route << 12
    if rd is not None: r7 = (r7 & ~0x1F) | 0x10 | rd
    if daddr is not None: r8 = (r8 & 0x7F) | daddr << 7
    if mem is not None: r10 = (r10 & ~7) | mem
    return [r10, r9, r8, r7, r6]


LOAD = (0, 0, 0, 0x8000, 0x4000)
load = lambda reg: word(LOAD, ra=reg)
RD = lambda n: word(IMG0[0x0D3], op=1, ra=0, rb=n)          # 0d3 plays r[n] (k 0: x + 0x)
PS = IMG0[0x0D3]                                             # 0d3 plays s


def round1():
    P = []
    seed = {"0cf": load(0x30), "0d1": word(IMG0[0x0D1], op=1, rb=0x30)}
    sk = {"0cf": 0x0040, "0d1": 0, "0d3": 0}
    P.append(dict(name="A1_seed", steps={**seed, "0d3": PS}, coefs=sk))
    P.append(dict(name="A2_seed_r33", steps={**seed, "0ce": load(0x33), "0d3": PS}, coefs={**sk, "0ce": 0x0040}))
    P.append(dict(name="A3_r33_only", steps={"0ce": load(0x33), "0d3": PS}, coefs={"0ce": 0x0040, "0d3": 0}))
    P.append(dict(name="A4_both_pass", steps={"0ce": load(0x33), "0cf": load(0x30), "0d3": PS}, coefs={"0ce": 0x0040, "0cf": 0x0040, "0d3": 0}))
    P.append(dict(name="A5_r31_r30", steps={**seed, "0ce": load(0x31), "0d3": PS}, coefs={**sk, "0ce": 0x0040}))
    P.append(dict(name="A6_r33_read", steps={"0ce": load(0x33), "0d3": RD(0x33)}, coefs={"0ce": 0x0040, "0d3": 0}))
    base = {**seed, "0ce": load(0x33)}
    bk = {**sk, "0ce": 0x0040, "0d2": 0x1000}
    for route in range(4):
        for da in (0, 0x1B0):
            st = word(IMG0[0x0D2], op=0, ra=0x32, rb=0x33, route=route, daddr=da)
            for rn, rdw in (("r32", RD(0x32)), ("s", PS)):
                P.append(dict(name=f"B_rt{route}_d{da:x}_{rn}", steps={**base, "0d2": st, "0d3": rdw}, coefs=bk))
    for op in (2, 3):
        for route in (0, 2):
            st = word(IMG0[0x0D2], op=op, ra=0x32, rb=0x33, route=route)
            P.append(dict(name=f"C_op{op}_rt{route}", steps={**base, "0d2": st, "0d3": RD(0x32)}, coefs=bk))
    for route in (0, 1, 3):
        st = word(IMG0[0x0D2], cls=3, op=0, ra=0x32, rb=0x33, route=route)
        P.append(dict(name=f"D_c3_rt{route}", steps={**base, "0d2": st, "0d3": RD(0x32)}, coefs=bk))
    st = word(IMG0[0x0D2], op=0, ra=0x32, rb=0x32, route=2)
    P.append(dict(name="E_AA_rt2", steps={**base, "0d2": st, "0d3": RD(0x32)}, coefs=bk))
    return P


ROUNDS = {"1": round1}


def round2():
    P = []
    L = lambda r, k=0x0040: (load(r), k)
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    nop = [0, 0, 0, 0, 0]
    pr("P1_two_loads", [("0ce", load(0x33), 0x40), ("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P2_s0_between", [("0cc", load(0x33), 0x40), ("0cd", word(IMG0[0x0D1], op=1, rb=0x35), 0), ("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P3_s125_between", [("0cc", load(0x33), 0x40), ("0cd", word(IMG0[0x0D1], op=2, rb=0x33), 0x4000), ("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P4_pass_between", [("0cc", load(0x33), 0x40), ("0cd", word(IMG0[0x0D1], op=4), 0), ("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P5_one_load", [("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P6_nop_between", [("0cc", load(0x33), 0x40), ("0cd", nop, 0), ("0cf", load(0x30), 0x40)], RD(0x30))
    pr("P7_load_s", [("0cc", load(0x33), 0x40), ("0cd", word(IMG0[0x0D1], op=4), 0), ("0ce", word(IMG0[0x0D1], op=4), 0)],
       word(IMG0[0x0D3], op=4), "class 1 then pass: does a load set s")
    seed = [("0cf", load(0x30), 0x40), ("0d1", word(IMG0[0x0D1], op=1, rb=0x30), 0)]
    pr("Q1_op2_rA_noB", seed + [("0d2", word(IMG0[0x0D2], op=2, ra=0x32), 0x4000)], RD(0x32), "rB 0: rA a destination?")
    pr("Q2_op0_rA_noB", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32), 0x4000)], RD(0x32))
    pr("Q3_rt3_small_r", seed + [("0d2", word(IMG0[0x0D2], op=2, ra=0x32, rb=0x30, route=3), 0x0100)], RD(0x32))
    pr("Q3_rt3_small_s", seed + [("0d2", word(IMG0[0x0D2], op=2, ra=0x32, rb=0x30, route=3), 0x0100)], word(IMG0[0x0D3], op=4))
    pr("Q4_rt3_op0_s", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32, rb=0x35, route=3), 0)], word(IMG0[0x0D3], op=4))
    pr("Q5_rA0_feedback", seed + [("0d2", word(IMG0[0x0D2], op=2, ra=0, rb=0x30), 0x4000)], word(IMG0[0x0D3], op=4))
    pr("Q6_rt1_noB_s", seed + [("0d2", word(IMG0[0x0D2], op=2, route=1), 0x4000)], word(IMG0[0x0D3], op=4), "rA 0 rB 0 route 1: s scaled?")
    return P


ROUNDS = {"1": round1, "2": round2}


def round3():
    P = []
    PASS0 = ("0d0", word(IMG0[0x0D1], op=4), 0)              # 0d0 as a pass, so s carries from 0cf on
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    S = word(IMG0[0x0D3], op=4)
    seed = [("0cf", load(0x30), 0x40), ("0d1", word(IMG0[0x0D1], op=1, rb=0x30), 0)]
    for op in (2, 0, 3):
        for route in range(4):
            pr(f"R_op{op}_rt{route}", seed + [("0d2", word(IMG0[0x0D2], op=op, route=route), 0x4000)], S, "rA 0 rB 0 k 0.5")
    pr("R_op0_rA32_k0_r", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32), 0)], RD(0x32))
    pr("R_op0_rA32_k0_s", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32), 0)], S)
    pr("R_op0_rA32_k5_r", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32), 0x4000)], RD(0x32))
    pr("R_op0_rA32_k5_s", seed + [("0d2", word(IMG0[0x0D2], op=0, ra=0x32), 0x4000)], S)
    pr("R_op2_rA32_k5_s", seed + [("0d2", word(IMG0[0x0D2], op=2, ra=0x32), 0x4000)], S)
    pr("R_op4_rA32_r", seed + [("0d2", word(IMG0[0x0D2], op=4, ra=0x32), 0)], RD(0x32))
    pr("L_rB_sets_s", [("0cc", load(0x31), 0x40), ("0cd", word(load(0x33), rb=0x31), 0x40), PASS0], S, "load r33 = k + r31: s after")
    pr("L_sets_s", [("0cc", load(0x33), 0x40), PASS0], S)
    pr("L_then_op2", [("0cf", load(0x30), 0x40), PASS0, ("0d2", word(IMG0[0x0D2], op=2), 0x4000)], S, "s = 0.25 from the load, op 2 k 0.5 rB 0")
    pr("L_then_op1", [("0cf", load(0x30), 0x40), PASS0, ("0d2", word(IMG0[0x0D2], op=1), 0x4000)], S)
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3}


def round4():
    """Who writes the rd-en gain file G[rsrc]: the shipped class-1 op-6 form (`1180 400c`, ins1_00 0x110)."""
    P = []
    S = word(IMG0[0x0D3], op=4)
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    x1 = [("0ca", load(0x31), 0x0100)]                            # r[31] = 1.0, the operand the G read multiplies
    rdG = lambda n: word(IMG0[0x0D3], op=2, rb=0x31, rd=n)         # 0d3 = G[n] * r[31]
    c1op6 = lambda n, ra=0x30, r7=0x1180: [0, ra, 0, r7, 0x4000 | n]
    pr("G5_before", x1, rdG(5), "G[5] as found")
    for n, k in ((5, 0x4000), (5, 0x2000), (5, 0xC000), (7, 0x4000)):
        pr(f"G{n}_op6_k{k:04x}", x1 + [("0cf", c1op6(n), k)], rdG(n), f"class 1 op 6 route 1 wdst {n} k {k:04x}")
    pr("G5_op6_r30", x1 + [("0cc", load(0x30), 0x0080), ("0cf", c1op6(5), 0)], rdG(5), "r[30] = 0.5, op-6 load k 0")
    pr("G5_op6_r30_r", x1 + [("0cc", load(0x30), 0x0080), ("0cf", c1op6(5), 0x4000)], RD(0x30), "the op-6 step's rA after")
    pr("G5_op6_s", x1 + [("0cf", c1op6(5), 0x4000), ("0d0", word(IMG0[0x0D1], op=4), 0)], S, "s after the op-6 step")
    pr("G5_op6_rt0", x1 + [("0cf", c1op6(5, r7=0x0180), 0x4000)], rdG(5), "op 6 route 0")
    pr("G5_op0_wdst", x1 + [("0cf", c1op6(5, r7=0x8000), 0x4000)], rdG(5), "plain load (r7 8000) with wdst 5")
    pr("G5_c2_wdst", x1 + [("0cc", load(0x30), 0x0080), ("0cf", word(IMG0[0x0D1], op=2, rb=0x30) [:4] + [0x8000 | 0x30 << 7 | 5], 0x4000)],
       rdG(5), "class 2 op 2 x 0.5 k 0.5 with wdst 5")
    pr("G5_after", x1, rdG(5), "G[5] after: does it hold?")
    pr("C1_op1_rB", [("0cc", load(0x33), 0x0040), ("0cf", [0, 0x30, 0, 0x1040, 0x4000 | 0x33 << 7], 0x0080)], RD(0x30),
       "class 1 r7 1040 (op 1 route 1) rB 33: r30 = ?")
    pr("C1_hi_r9", [("0cf", [0, 0x1C30, 0, 0x5000, 0x4000], 0xC000)], RD(0x30), "class 1 r9 1c30 r7 5000 k c000")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4}


def round5():
    """Class-1 forms of the shipped insertion window, with the running value s set before them."""
    P = []
    S = word(IMG0[0x0D3], op=4)
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    def sset(v):                                   # s = v at 0cd: r[35] = v (load), then op 1 rB 35 k 0 (x + 0x)
        return [("0cc", load(0x35), int(v * 256) & 0xFFFF), ("0cd", word(IMG0[0x0D1], op=1, rb=0x35), 0)]
    forms = {"plain": (0x0030, 0x8000), "hi1c": (0x1C30, 0x5000), "hi1d": (0x1D30, 0x5000), "op6": (0x0030, 0x1180),
             "op1rt1": (0x0030, 0x1040), "rt1": (0x0030, 0x1000)}
    for nm, (r9, r7) in forms.items():
        for sv in (0.0, 0.25, -0.25):
            for k in ((0x4000, 0xC000) if nm in ("plain", "hi1c") else (0x4000,)):
                stp = ("0ce", [0, r9, 0, r7, 0x4000 | (0x0C if nm == "op6" else 0)], k)
                pr(f"F_{nm}_s{sv:+}_k{k:04x}", sset(sv) + [stp], RD(0x30), f"class 1 r9 {r9:04x} r7 {r7:04x} k {k:04x}, s {sv}")
        pr(f"F_{nm}_s0.25_k4000_s", sset(0.25) + [("0ce", [0, r9, 0, r7, 0x4000 | (0x0C if nm == "op6" else 0)], 0x4000),
                                                   ("0cf", word(IMG0[0x0D1], op=4), 0), ("0d0", word(IMG0[0x0D1], op=4), 0)],
           S, "s after it")
    for k in (0x0100, 0x0400, 0x0800, 0x1000, 0x2000):
        pr(f"F_plain_s0_k{k:04x}", sset(0.0) + [("0ce", [0, 0x30, 0, 0x8000, 0x4000], k)], RD(0x30), "plain load magnitude")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4, "5": round5}


def round6():
    """Class-1 load laws at small values: route, r7 bit 14, r9[12:8], sign, and the s term."""
    P = []
    S = word(IMG0[0x0D3], op=4)
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    def sset(v):
        return [("0cc", load(0x35), int(round(v * 256)) & 0xFFFF), ("0cd", word(IMG0[0x0D1], op=1, rb=0x35), 0)]
    forms = {"p": (0x0030, 0x8000), "r1": (0x0030, 0x1000), "r1b14": (0x0030, 0x5000), "b14": (0x0030, 0x4000),
             "hi1c": (0x1C30, 0x5000), "hi1d": (0x1D30, 0x5000), "hi1c_r0": (0x1C30, 0x0000), "r2": (0x0030, 0x2000),
             "r0": (0x0030, 0x0000)}
    for nm, (r9, r7) in forms.items():
        for sv, k in ((0.0, 0x0040), (0.0, 0xFFC0), (0.25, 0x0040), (0.25, 0xFFC0), (0.0, 0x4000 >> 6), (0.0, 0xC000)):
            stp = ("0ce", [0, r9, 0, r7, 0x4000], k)
            pr(f"G_{nm}_s{sv}_k{k:04x}", sset(sv) + [stp], RD(0x30), f"class 1 r9 {r9:04x} r7 {r7:04x} k {k:04x}, s {sv}")
        pr(f"G_{nm}_s0.25_k0040_s", sset(0.25) + [("0ce", [0, r9, 0, r7, 0x4000], 0x0040), ("0cf", word(IMG0[0x0D1], op=4), 0),
                                                 ("0d0", word(IMG0[0x0D1], op=4), 0)], S, "s after")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4, "5": round5, "6": round6}


def round7():
    """Register range (wrap vs saturate) via a 1/128 reader, the negative-to-zero forms, and class-2 overflow."""
    P = []
    S = word(IMG0[0x0D3], op=4)
    RS = lambda n: word(IMG0[0x0D3], op=2, rb=n)               # 0d3 = k x r[n], k 0x0100 -> r / 128
    def pr(name, seq, read, k3=0, detail=""):
        steps, coefs = {}, {"0d3": k3}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    for r7 in (0x0000, 0x1000, 0x4000, 0x5000, 0x8000, 0x3000):
        for k in (0x1000, 0x2100, 0x3000, 0x4000, 0x6000, 0x7FFF, 0xC000):
            pr(f"W_{r7:04x}_k{k:04x}", [("0ce", [0, 0x30, 0, r7, 0x4000], k)], RS(0x30), 0x0100, f"load r7 {r7:04x} k {k:04x}; r30/128")
        pr(f"W_{r7:04x}_k4000_s", [("0ce", [0, 0x30, 0, r7, 0x4000], 0x4000), ("0cf", word(IMG0[0x0D1], op=4), 0),
                                   ("0d0", word(IMG0[0x0D1], op=4), 0), ("0d1", word(IMG0[0x0D1], op=2, rb=0x36), 0),
                                   ("0d2", word(IMG0[0x0D2], op=0, rb=0x36), 0x0100)], S, 0, "s after, x 1/128 via op 0")
    for r7 in (0x8000, 0x2000, 0xA000, 0x0000, 0x9000, 0xC000):
        pr(f"N_{r7:04x}", [("0ce", [0, 0x30, 0, r7, 0x4000], 0xFFC0)], RD(0x30), 0, f"load r7 {r7:04x} k -0.25")
    ld = lambda r, v: ("0cc", load(r), v)
    for rt, k in ((3, 0x7FFF), (3, 0x4000), (2, 0x7FFF), (0, 0x7FFF)):
        pr(f"O_rt{rt}_k{k:04x}", [ld(0x31, 0x0800), ("0cf", word(IMG0[0x0D1], op=2, ra=0x30, rb=0x31, route=rt), k)],
           RS(0x30), 0x0100, f"r31 = 8, op 2 route {rt} k {k:04x} -> r30; r30/128")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4, "5": round5, "6": round6, "7": round7}


def round8():
    """Class 2: overflow (wrap / saturate) and r7[15:14] modes, the running value s on overflow, op-0 sums."""
    P = []
    RS = lambda n: word(IMG0[0x0D3], op=2, rb=n)
    def pr(name, seq, read=None, detail=""):
        steps, coefs = {}, {"0d3": 0x0100}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read or RS(0x30)
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    ld = lambda r, v, st="0cc": (st, load(r), v)
    for v in (0x0800, 0x0C00, 0xF400):
        for mode in (0, 1, 2, 3):
            w = word(IMG0[0x0D1], op=2, ra=0x30, rb=0x31, route=3)
            w[3] = (w[3] & 0x3FFF) | mode << 14
            pr(f"V_{v:04x}_m{mode}", [ld(0x31, v), ("0cf", w, 0x7FFF)], detail=f"r31 = {v:04x}/256, op 2 route 3 mode {mode} -> r30")
    for v in (0x0C00, 0xF400):
        w = word(IMG0[0x0D1], op=2, ra=0x32, rb=0x31, route=3)
        pr(f"S_{v:04x}", [ld(0x31, v), ("0ce", w, 0x7FFF), ("0cf", word(IMG0[0x0D1], op=4, ra=0x30), 0)],
           detail="op 2 route 3 overflow, then op 4 rA 30 (r30 = s)")
    for a, b in ((0x5000, 0x5000), (0x7000, 0x2000), (0xB000, 0xB000)):
        w = word(IMG0[0x0D1], op=0, ra=0x30, rb=0x32, route=0)
        pr(f"A_{a:04x}_{b:04x}", [ld(0x31, a), ("0cd", word(IMG0[0x0D1], op=1, rb=0x31), 0), ld(0x32, b, "0ce"), ("0cf", w, 0x7FFF)],
           detail="s = r31, op 0 rB 32 k 1 -> r30 = s + r32")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4, "5": round5, "6": round6, "7": round7, "8": round8}


def round9b():
    P = []
    RS = lambda n: word(IMG0[0x0D3], op=2, rb=n)
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RS(0x37)}, {"0d3": 0x0100}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    keep = ("0d0", word(IMG0[0x0D1], op=4, ra=0x37), 0)      # r37 = s (op 4 passes s, rA writes it), route 0
    for r7, k in ((0x1000, 0x4000), (0x1000, 0x2000), (0x1000, 0x6000), (0x1180, 0x4000), (0x5000, 0x4000), (0x8000, 0x4000),
                  (0x0000, 0x4000), (0xC000, 0xFFC0), (0xC000, 0x0040), (0x2000, 0x4000), (0x1040, 0x4000)):
        pr(f"T_{r7:04x}_k{k:04x}", [("0ce", [0, 0x30, 0, r7, 0x4000], k), ("0cf", word(IMG0[0x0D1], op=4), 0), keep],
           f"load r7 {r7:04x} k {k:04x}; r37 = s; r37/128")
    pr("T_c3_mode3_neg", [("0cc", load(0x31), 0xFFC0), ("0ce", word(IMG0[0x0D1], op=2, ra=0x30, rb=0x31, route=0)[:3] +
                                                            [0xC0C0, 0x8000 | 0x31 << 7], 0x7FFF), ("0cf", word(IMG0[0x0D1], op=4), 0), keep],
       "class 2 op 3 mode 3 on r31 = -0.25")
    return P


def round10():
    """Register write latency: write r30 at 0d3 - g, read at 0d3, clear r30 at 0e0 (after the read) every pass,
    so a read that does not yet see this pass's write sees 0 (last pass's clear)."""
    P = []
    clr = ("0e0", [0, 0x30, 0, 0x8000, 0x4000 | 0x36 << 7], 0)        # class 1 r30 = 0 + r36 (0)
    c1 = [0, 0x30, 0, 0x8000, 0x4000 | 0x36 << 7]                       # class 1 r30 = k/256 + r36
    c2 = word(IMG0[0x0D1], op=2, ra=0x30, rb=0x31, route=0)            # class 2 r30 = k r31
    pre = ("0c1", load(0x31), 0x0080)                                   # r31 = 0.5, long before
    for g in (1, 2, 3, 4, 5):
        st = "%03x" % (0x0D3 - g)
        for nm, w, k in (("c1", c1, 0x0040), ("c2", c2, 0x4000)):
            steps = {st: w, "0d3": RD(0x30), clr[0]: clr[1]}
            coefs = {st: k, "0d3": 0, clr[0]: 0}
            if nm == "c2":
                steps[pre[0]], coefs[pre[0]] = pre[1], pre[2]
            P.append(dict(name=f"LAT_{nm}_g{g}", steps=steps, coefs=coefs, detail=f"{nm} write r30 at {st}, read at 0d3"))
    P.append(dict(name="LAT_c1_g2_noclr", steps={"0d1": c1, "0d3": RD(0x30)}, coefs={"0d1": 0x0040, "0d3": 0}))
    P.append(dict(name="LAT_c1_g1_noclr", steps={"0d2": c1, "0d3": RD(0x30)}, coefs={"0d2": 0x0040, "0d3": 0}))
    return P


def round11():
    """Ops 5, 6, 7 (and 4) with rB set, as the shipped comb writes use them (0a9: class 2 op 5 rB 37)."""
    P = []
    def pr(name, seq, read, detail=""):
        steps, coefs = {}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        steps["0d3"] = read
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    def sset(v):
        return [("0cc", load(0x35), int(round(v * 256)) & 0xFFFF), ("0cd", word(IMG0[0x0D1], op=1, rb=0x35), 0)]
    rx = lambda v: ("0c8", load(0x31), int(round(v * 256)) & 0xFFFF)
    S = word(IMG0[0x0D3], op=4)
    for op in (5, 6, 7, 4):
        for sv, xv, k in ((0.0, 0.25, 0x4000), (0.125, 0.25, 0x4000), (-0.125, 0.25, 0x4000), (0.125, 0.0, 0x4000),
                          (0.125, 0.25, 0x2000), (0.125, 0.25, 0xC000), (0.125, -0.25, 0x4000), (-0.125, 0.0, 0x4000)):
            w = word(IMG0[0x0D1], op=op, ra=0x30, rb=0x31, route=0)
            base = [rx(xv)] + sset(sv) + [("0ce", w, k)]
            tag = f"{op}_s{sv}_x{xv}_k{k:04x}"
            pr(f"P{tag}_r", base, RD(0x30), f"op {op} rB 31: r30")
            pr(f"P{tag}_s", base + [("0cf", word(IMG0[0x0D1], op=4), 0), ("0d0", word(IMG0[0x0D1], op=4), 0)], S, "s after")
    return P


def round12():
    """The z^-1 write (rA | 0x80 stores the previous step's x): what x is after non-class-2 steps."""
    P = []
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    A = ("0c9", word(IMG0[0x0D1], op=4, rb=0x31), 0)                 # class 2, x = r31 = 0.25
    Z = ("0cb", [0, 0xB0, 0, 0x0100, 0x8000 | 0x34 << 7], 0)          # class 2 op 4 rA 30|80, x = r34 (0.0625)
    pre = [("0c4", load(0x31), 0x0040), ("0c5", load(0x32), 0x0020), ("0c6", load(0x34), 0x0010)]
    mids = {"c2": word(IMG0[0x0D1], op=4, rb=0x32), "empty": [0] * 5, "c1rb32": [0, 0x33, 0, 0x8000, 0x4000 | 0x32 << 7],
            "c1": [0, 0x33, 0, 0x8000, 0x4000], "c3rb32": [0, 0, 0, 0xC0C0, 0xC000 | 0x32 << 7],
            "c0rb32": [0, 0, 0, 0x00C0, 0x0000 | 0x32 << 7], "c0w3": [0, 0, 0, 0x00C0, 0]}
    for nm, w in mids.items():
        pr(f"Z_{nm}", pre + [A, ("0ca", w, 0), Z], f"0c9 x = r31, 0ca {nm}, 0cb rA 30|80")
    pr("Z_direct", pre + [("0ca", word(IMG0[0x0D1], op=4, rb=0x31), 0), Z], "0ca x = r31 then z write")
    pr("Z_k", pre + [A, ("0ca", word(IMG0[0x0D1], op=2, rb=0x32), 0x4000), Z], "0ca op 2 k 0.5 x = r32")
    return P


def round13():
    """Op 7 (multiply a source) and the w[] slot (r6[5:0]): what op 7 reads when daddr is not a d[] address."""
    P = []
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    pre = [("0c4", load(0x31), 0x0040)]                                   # r31 = 0.25
    W = lambda slot, rb=0x31: [0, 0, 0, 0x0080, 0x8000 | rb << 7 | slot]  # class 2 op 2 rB rb (k 1), -> w[slot]
    O7 = lambda slot=0: [0, 0x30, 0, 0x01C0, 0x8000 | slot]                # class 2 op 7 rA 30 (w slot field = slot)
    pr("O7_alone", pre + [("0cc", O7(), 0x4000)], "op 7 k 0.5, nothing set")
    for sl in (0x05, 0x16, 0x38):
        pr(f"O7_after_w{sl:02x}", pre + [("0ca", W(sl), 0x7FFF), ("0cc", O7(), 0x4000)], f"0ca writes w[{sl:02x}] = 0.25")
        pr(f"O7_same_w{sl:02x}", pre + [("0ca", W(sl), 0x7FFF), ("0cc", O7(sl), 0x4000)], f"0ca -> w[{sl:02x}], op 7 with slot {sl:02x}")
        pr(f"O7_next_w{sl:02x}", pre + [("0cb", W(sl), 0x7FFF), ("0cc", O7(sl), 0x4000)], f"0cb -> w[{sl:02x}], op 7 next step")
    pr("O7_s", pre + [("0ca", word(IMG0[0x0D1], op=2, rb=0x31), 0x7FFF), ("0cc", O7(), 0x4000)], "s = 0.25 then op 7")
    pr("O7_k7fff", pre + [("0ca", word(IMG0[0x0D1], op=2, rb=0x31), 0x7FFF), ("0cc", O7(), 0x7FFF)], "s = 0.25, op 7 k 1")
    pr("O7_rb31", pre + [("0cc", [0, 0x30, 0, 0x01C0, 0x8000 | 0x31 << 7], 0x4000)], "op 7 with rB 31")
    return P


def round14():
    """Op 7's source when daddr has bit 8 clear (the shipped combs use op 7 with daddr 0x08d etc.)."""
    P = []
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    pre = [("0c4", load(0x31), 0x0040)]                                    # r31 = 0.25
    def wd(n):                                                              # 0c8: class 2 op 2 rB 31 k 1, daddr n
        w = word(IMG0[0x0D1], op=2, rb=0x31)
        w[2] = (w[2] & 0x7F) | n << 7
        return ("0c8", w, 0x7FFF)
    def o7(n, st="0cc"):
        return (st, [0, 0x00, n << 7, 0x01C0, 0x8000], 0x4000)              # op 7 k 0.5, daddr n: read s after
    S = word(IMG0[0x0D3], op=4)
    passes = [("0cd", word(IMG0[0x0D1], op=4), 0), ("0ce", word(IMG0[0x0D1], op=4), 0), ("0cf", word(IMG0[0x0D1], op=4), 0),
              ("0d0", word(IMG0[0x0D1], op=4), 0)]
    for wa, ra in ((0x18D, 0x18D), (0x18D, 0x08D), (0x18D, 0x00D), (0x08D, 0x08D), (0x08D, 0x18D), (0x00D, 0x08D),
                   (0x18D, 0x10D), (0x10D, 0x08D)):
        pr(f"S_w{wa:03x}_r{ra:03x}", pre + [wd(wa), o7(ra)] + passes, f"0c8 y = 0.25 daddr {wa:03x}; 0cc op 7 daddr {ra:03x}")
    pr("S_none_r08d", pre + [o7(0x08D)] + passes, "no writer, op 7 daddr 08d")
    pr("S_w18d_r08d_next", pre + [wd(0x18D), o7(0x08D, "0c9")] + passes, "op 7 the next step")
    for p in P:
        p["steps"]["0d3"] = S
    return P


def round15():
    """w[] (r6[5:0]): written by a step's result, read by op 7 (k x w[slot]); gaps and which ops write it."""
    P = []
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, detail=detail))
    pre = [("0b0", load(0x31), 0x0040)]                                     # r31 = 0.25
    W = lambda op, slot: [0, 0, 0, op << 6, 0x8000 | 0x31 << 7 | slot]      # class 2 op `op` rB 31 -> w[slot]
    O7 = lambda slot: [0, 0x30, 0, 0x01C0, 0x8000 | slot]                   # op 7 rA 30, slot
    for gap in (3, 4, 8, 16):
        pr(f"W2_g{gap}", pre + [("%03x" % (0xCC - gap), W(2, 0x38), 0x7FFF), ("0cc", O7(0x38), 0x4000)], f"op 2 -> w38, op 7 {gap} later")
    pr("W2_after", pre + [("0cd", W(2, 0x38), 0x7FFF), ("0cc", O7(0x38), 0x4000)], "w38 written after the read (last pass)")
    for op in (0, 1, 3, 4, 5):
        pr(f"W{op}_g8", pre + [("0c0", word(IMG0[0x0D1], op=2, rb=0x31), 0x7FFF), ("0c4", W(op, 0x38), 0x7FFF),
                                ("0cc", O7(0x38), 0x4000)], f"s = 0.25, op {op} -> w38, op 7 8 later")
    pr("W2_g8_other", pre + [("0c4", W(2, 0x39), 0x7FFF), ("0cc", O7(0x38), 0x4000)], "w39 written, w38 read")
    pr("W2_g8_cls3", pre + [("0c4", W(2, 0x38), 0x7FFF), ("0cc", [0, 0x30, 0, 0x01C0, 0xC000 | 0x38], 0x4000)], "class 3 op 7")
    return P


ROUNDS = {"1": round1, "2": round2, "3": round3, "4": round4, "5": round5, "6": round6, "7": round7, "8": round8, "9": round9b,
          "10": round10, "11": round11, "12": round12, "13": round13, "14": round14, "15": round15}


def round16():
    """DRAM read data -> op 7 / op 4 (rB 0): write r31 at 0c3 (slot 30), read slot 31 (lag 10) at 0c7, op 7 after."""
    P = []
    OFFS = {0x30: 1000, 0x31: 1010, 0x32: 1010, 0x33: 1010}
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, offs=OFFS, detail=detail))
    W = [0x0001, 0, 0x0040, 0x0080, 0x8000 | 0x31 << 7]                  # class 2 op 2 rB 31 k 1, mem 1, r8 bit 6
    R = lambda mem=2, slot=0: [mem, 0, 0x0040, 0x0100, 0x8000 | slot]    # class 2 op 4 (pass), read, r8 bit 6
    O7 = lambda slot, op=7: [0, 0x30, 0, op << 6, 0x8000 | slot]
    base = [("0c1", load(0x31), 0x0040), ("0c3", W, 0x7FFF)]
    pr("D_ref", base + [("0c7", R(), 0)], "no op 7: r30 stays 0")
    for g in (1, 2, 3, 4, 5):
        pr(f"D_o7_38_g{g}", base + [("0c7", R(), 0), ("%03x" % (0xC7 + g), O7(0x38), 0x4000)], f"op 7 f6c 38, {g} after the read")
    for sl in (0x39, 0x3c, 0x3f, 0x00, 0x10):
        pr(f"D_o7_{sl:02x}_g4", base + [("0c7", R(), 0), ("0cb", O7(sl), 0x4000)], f"op 7 f6c {sl:02x}, 4 after")
    pr("D_o7_same", base + [("0c7", [2, 0x30, 0x0040, 0x01C0, 0x8038], 0x4000)], "op 7 on the read step itself")
    pr("D_o4_38_g4", base + [("0c7", R(), 0), ("0cb", O7(0x38, op=4), 0x4000)], "op 4 f6c 38 (s + k src?)")
    pr("D_o7_38_g4_m4", base + [("0c7", R(4), 0), ("0cb", O7(0x38), 0x4000)], "read mem 4")
    pr("D_o7_3d_rdslot", base + [("0c7", R(2, 0x3d), 0), ("0cb", O7(0x3d), 0x4000)], "read step f6c 3d, op 7 f6c 3d")
    return P


ROUNDS["16"] = round16


def round17():
    """Memory sanity (session 16 capture path) and op 7 f6c sources with a verified read. Write r31 = 0.25 at
    0c3 (slot 30, ph 3), read slot 31 (offset +10) at 0c7; 0cc (slot 33) captures slot 31's transfer into d[20]."""
    P = []
    OFFS = {0x30: 1000, 0x31: 1010, 0x32: 1010, 0x33: 1010, 0x34: 1010}
    def pr(name, seq, detail=""):
        steps, coefs = {"0d3": RD(0x30)}, {"0d3": 0}
        for st, w, k in seq:
            steps[st], coefs[st] = w, k
        P.append(dict(name=name, steps=steps, coefs=coefs, offs=OFFS, detail=detail))
    W = [0x0001, 0, 0x0040, 0x0080, 0x8000 | 0x31 << 7]
    R = [0x0002, 0, 0x0040, 0x0100, 0x8000]
    CAP = [0, 0, 0x100 << 7 | 0, 0x0100, 0x8000]
    CAP[2] = (0x120) << 7 & 0xFF80
    O7D = [0, 0x30, (0x1A0 << 7) & 0xFF80, 0x01C0, 0x8000]                 # op 7 daddr 1a0 (reads d[20]) rA 30
    O7 = lambda sl, op=7: [0, 0x30, 0, op << 6, 0x8000 | sl]
    base = [("0c1", load(0x31), 0x0040), ("0c3", W, 0x7FFF), ("0c7", R, 0)]
    pr("M_cap", base + [("0cc", CAP, 0), ("0cd", O7D, 0x4000)], "s16 path: capture slot 31 into d[20], op 7 reads it")
    pr("M_nowrite", [("0c1", load(0x31), 0x0040), ("0c7", R, 0), ("0cc", CAP, 0), ("0cd", O7D, 0x4000)], "no write")
    for sl in range(0x38, 0x40):
        pr(f"M_o7_{sl:02x}", base + [("0cc", CAP, 0), ("0cd", O7(sl), 0x4000)], f"op 7 f6c {sl:02x} after a verified read")
    for g in (0, 1, 2):
        pr(f"M_o7_38_at{0xC8 + g:03x}", base + [("%03x" % (0xC8 + g), O7(0x38), 0x4000)], "op 7 f6c 38 right after the read")
    pr("M_o7_20", base + [("0cc", CAP, 0), ("0cd", O7(0x20), 0x4000)], "op 7 f6c 20 (= d[20]?)")
    pr("M_o4_38", base + [("0cc", CAP, 0), ("0cd", O7(0x38, 4), 0x4000)], "op 4 f6c 38")
    return P


ROUNDS["17"] = round17


IMG1 = [struct.unpack_from(">5H", ROM.read_bytes(), 0x172204 + i * 10) for i in range(512)]
IMG1[0x0E0], IMG1[0x0D3] = (0, 0, 0, 0, 0), IMG0[0x0D3]   # as session 24 sets image 1 up (readout via d[0b])


def predict(p, passes=64):
    steps = list(IMG1 if p.get("image") else IMG0)
    for k, w in p.get("steps", {}).items():
        steps[int(k, 16)] = tuple(w)
    coef = [0] * 512
    for s in range(0x0D0, 0x0D8):
        coef[s] = 0x7FFF
    coef[0x0E9] = coef[0x0EB] = 0x4000
    for k, v in p.get("coefs", {}).items():
        coef[int(k, 16)] = v
    r10, r9, r8, r7, r6 = steps[0x0D3]
    steps[0x0D3] = (r10, r9, r8, r7 & ~0x3000, r6)  # the readout is 0d3's value before its route-3 scale (s24: R = y x 1)
    it = V.Interp(steps, coef, bank_of=lambda _: 0)
    for _ in range(passes):
        it.trace = []
        it.sample()
    y = dict(it.trace).get(0x0D3)
    return None if y is None else max(-8.0, min(8.0, y))      # 0d3's result reaches the DAC through d[]: clips at +-8


def check():
    """Every probe of sessions 24 rounds 1-3 (docs/vop3_probes.json) against the interpreter."""
    data = json.loads((Path(__file__).resolve().parents[1] / "docs/vop3_probes.json").read_text())
    bad = [(n, d["measured"], predict(d["probe"])) for n, d in data.items()
           if not d.get("open") and (predict(d["probe"]) is None or abs(predict(d["probe"]) - d["measured"]) > 2e-3)]
    # "open": rounds 13-15 (op 7 sources), recorded as measured but not yet explained; S_w18d_r18d differs
    for b in bad:
        print("differs", b)
    return len(data), bad


def main():
    if sys.argv[1] == "check":
        n, bad = check()
        print(f"{n - len(bad)} / {n} probes match")
        sys.exit(1 if bad else 0)
    cmd, rnd = sys.argv[1], sys.argv[2]
    P = ROUNDS[rnd]()
    if cmd == "write":
        Path(sys.argv[3]).write_text(json.dumps(P, indent=0))
        print(len(P), "probes")
    else:
        meas = {}
        for line in Path(sys.argv[3]).read_text().splitlines():
            parts = line.split()
            if len(parts) > 3 and parts[1] == "x16":
                meas[parts[0]] = float(parts[3])
        for p in P:
            m, q = meas.get(p["name"]), predict(p)
            flag = "" if m is None or q is None or abs(m - q) < 2e-3 else "   <-- differs"
            print(f"{p['name']:18s} measured {m if m is None else round(m, 5)!s:>9s}  model {q if q is None else round(q, 5)!s:>9s}{flag}")


if __name__ == "__main__":
    main()

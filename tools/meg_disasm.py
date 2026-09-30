#!/usr/bin/env python3
"""SWP30 MEG (effects DSP) microcode disassembler, ported from MAME's swp30.cpp
(swp30_disassembler::disassemble, Olivier Galibert, BSD-3-Clause).

Usage: meg_disasm.py ROM OFFSET [COUNT] [--le] [--const OFF] [--offset OFF]
Instructions are 64-bit, stored big-endian in a Fujitsu FR (EX5) ROM.
"""
import struct, sys


def bit(v, pos, n=1):
    return (v >> pos) & ((1 << n) - 1)


def disasm(pc, op, const=None, offs=None):
    r = []
    sm = bit(op, 0x04, 6)
    sr = bit(op, 0x0b, 7)
    dm = bit(op, 0x27, 6)
    dr = bit(op, 0x30, 7)
    t = bit(op, 0x38, 3)
    gconst = lambda: ("%g" % (const / 32768.0)) if const is not None else "c%03x" % pc
    goff = lambda: ("%x" % offs) if offs is not None else "of%02x" % (pc // 3)

    mmode = bit(op, 0x16, 2)
    if mmode and not bit(op, 0x3f):
        m1t = bit(op, 0x14, 2)
        mul1 = "t%x" % t if m1t in (1, 2) else gconst()
        if bit(op, 0x13):
            mul1 = "exp(%s)" % mul1
        mul2 = ("m%02x" % sm if sm else "0") if bit(op, 0x12) else ("r%02x" % sr if sr else "0")
        at = bit(op, 0x18, 2)
        aop = {0: "p", 1: ("r%02x" % sr if sr else "(p >> 15)"), 2: ("m%02x" % sm if sm else "(p >> 15)"), 3: ""}[at]
        o = {1: "(%s << 8)" % mul1, 2: "%s * %s" % (mul1, mul2), 3: mul2}[mmode]
        if at != 3:
            o += [" + %s", " - %s", " + abs(%s)", " & %s"][bit(op, 0x1a, 2)] % aop
        sh = bit(op, 0x1c, 2)
        if sh:
            o = "(%s) << %d" % (o, 4 if sh == 3 else sh)
        r.append("p %s %s" % (["=", "=s", "=_", "=a"][bit(op, 0x1e, 2)], o))
    if dm:
        h = bit(op, 0x2d, 3)
        dst = "m%02x" % dm
        if h < 4:
            r.append("%s = lfo.%02x" % (dst, pc >> 4))
        else:
            r.append({4: "%s = mr", 5: "%s = rand", 6: "%s = p"}.get(h, "%s = " + ("m%02x" % sm if sm else "0")) % dst)
    if dr:
        r.append("r%02x = r%02x" % (dr, sr) if bit(op, 0x37) else "r%02x = p" % dr)
    if bit(op, 0x3d):
        r.append("mw = p")
    if bit(op, 0x3e):
        r.append("idx = p")
    if bit(op, 0x3b):
        r.append("t%x = p" % t if bit(op, 0x3c) else "t%x = %s" % (t, gconst()))
    if bit(op, 0x0a):
        r.append("nodither")
    mm = bit(op, 0x24, 2)
    if mm:
        r.append("mem_%s %s%s%s" % ([None, "w", "r", "1r"][mm], "@" if (mm != 1 and bit(op, 0x23)) else "+", goff(), "+idx" if bit(op, 0x21) else ""))
    if op == 0:
        r.append("nop")
    return " ; ".join(r)


def main():
    a = sys.argv[1:]
    le = "--le" in a
    a = [x for x in a if x != "--le"]
    rom = open(a[0], "rb").read()
    off = int(a[1], 0)
    n = int(a[2], 0) if len(a) > 2 else 384
    fmt = "<Q" if le else ">Q"
    for pc in range(n):
        op = struct.unpack_from(fmt, rom, off + pc * 8)[0]
        print("%03x %016x  %s" % (pc, op, disasm(pc, op)))


if __name__ == "__main__":
    main()

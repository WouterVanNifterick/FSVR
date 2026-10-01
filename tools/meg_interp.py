#!/usr/bin/env python3
"""SWP30 MEG interpreter: MAME's swp30_device::meg_state::step() (Olivier Galibert, BSD-3-Clause), ported.

    python tools/meg_interp.py                    self-check
    python tools/meg_interp.py --an --trace       run the EX5 AN voice program one pass, print every p

The MEG is the EX5's effects/AN DSP; MAME executes it with explicit arithmetic, so it is value-transparent
where the VOP3 is not (docs/vop3_isa.md section 7, docs/ex5_meg.md). This is MAME's step() line for line:
3-cycle delayed r/m/index writes, 2-cycle memory ports, the m1/m2/a selectors, adder, shifter,
saturation, p (42-bit 27.15), pack24 to the 24-bit registers. Dither is off (MAME adds rand() & 0x7e0
unless bit 0x0a is set; a deterministic run needs it off). LFOs read 0 unless `lfo` is set.
"""
import struct
import sys
from pathlib import Path

ROM = Path("/work/FS1R_DISASM/roms/EX5_TG1.bin")
AN_PROG = 0x3C97D4 - 0x200000           # u16 count, then count x u64 (docs/ex5_meg.md)
STEPS = 0x180


def bit(v, pos, n=1):
    return (v >> pos) & ((1 << n) - 1)


def sext(v, n):
    v &= (1 << n) - 1
    return v - (1 << n) if v >> (n - 1) else v


def pack24(p):
    q = int(p / 32768)                 # C++ s64 division truncates toward zero
    q = 0x7FFFFF if q == 0x800000 else -0x800000 if q == -0x800001 else q
    return sext(q, 24)


def m1_expand(v):
    if v < 0:
        return 0
    s = v >> 12
    v = 0x1000 | (v & 0xFFF)
    return v if s == 5 else v >> (5 - s) if s < 5 else v << (s - 5)


def revram_encode(v):
    v &= 0x7FFFFFF
    s = 0
    if v & 0x4000000:
        v ^= 0x7FFFFFF
        s = 1
    e = 15
    while e and not (v & (0x400 << e)):
        e -= 1
    m = (v >> (e - 1)) & 0x7FF if e else v
    return (e << 12) | (s << 11) | m


def revram_decode(v):
    e, s, m = (v >> 12) & 15, (v >> 11) & 1, v & 0x7FF
    vb = (m | 0x800) << (e - 1) if e else m
    if s:
        vb ^= (0xFFFFFFFF << (e - 1)) & 0xFFFFFFFF if e else 0xFFFFFFFF
    return sext(vb, 32)


class Meg:
    def __init__(self, prog, const=None, offset=None):
        self.prog = list(prog) + [0] * (STEPS - len(prog))
        self.const = [sext(c, 16) for c in (const or [0] * STEPS)] + [0] * STEPS
        self.offset = list(offset or [0] * (STEPS // 3))
        self.r = [0] * 0x80
        self.m = [0] * 0x40
        self.t = [0] * 8
        self.p = 0
        self.ram = [0] * 0x40000
        self.map = [0] * 8                     # MAME m_map: one 2^18 bank by default
        self.counter = 0
        self.index = 0
        self.ram_read = self.ram_write = 0
        self.lfo = [0] * 24
        # pipeline slots
        self.mw = [(0, 0)] * 3
        self.rw = [(0, 0)] * 3
        self.ix = [None] * 3
        self.memw = [None] * 2
        self.memr = [None] * 2
        self.tv = [0] * 2
        self.d3 = self.d2 = 0
        self.trace = None

    def resolve(self, pc, off):
        key = (pc // 12) << 11
        mp = self.map
        for i in range(8):
            if i == 7 or mp[i + 1] <= mp[i] or (mp[i + 1] & 0xF800) > key:
                mask = (1 << (10 + bit(mp[i], 8, 3))) - 1
                return (off & mask) + (bit(mp[i], 0, 8) << 10)

    def step(self, pc):
        d3, d2 = self.d3, self.d2
        reg, val = self.mw[d3]
        if reg:
            self.m[reg] = val
        reg, val = self.rw[d3]
        if reg:
            self.r[reg] = val
        if self.ix[d3] is not None:
            self.index = self.ix[d3]
        if self.memw[d2] is not None:
            self.ram_write, self.memw[d2] = self.memw[d2], None
        if self.memr[d2] is not None:
            self.ram_read, self.memr[d2] = self.memr[d2], None

        op = self.prog[pc]
        sm, sr, dm, dr, t = bit(op, 0x04, 6), bit(op, 0x0B, 7), bit(op, 0x27, 6), bit(op, 0x30, 7), bit(op, 0x38, 3)
        mmode = bit(op, 0x16, 2)
        if mmode or bit(op, 0x1A, 6):
            m1t = bit(op, 0x14, 2)
            m1 = self.t[t] if m1t in (1, 2) else self.const[pc]
            if bit(op, 0x13):
                m1 = m1_expand(m1)
            m2 = self.m[sm] if bit(op, 0x12) else self.r[sr]
            m = (0, m1 << 23, m1 * m2, m2 << 15)[mmode]
            asel = bit(op, 0x18, 2)
            a = (self.p, (self.r[sr] << 15) if sr else (self.p >> 15), (self.m[sm] << 15) if sm else (self.p >> 15), 0)[asel]
            add = bit(op, 0x1A, 2)
            res = m + a if add == 0 else m - a if add == 1 else m + abs(a) if add == 2 else m & a
            sh = bit(op, 0x1C, 2)
            if sh:
                res <<= 4 if sh == 3 else sh
            sat = bit(op, 0x1E, 2)
            if sat == 0:
                res = sext(res, 42)
            elif sat == 1:
                res = max(-0x4000000000, min(0x3FFFFFFFFF, res))
            elif sat == 2:
                res = max(0, min(0x3FFFFFFFFF, res))
            else:
                res = min(abs(res), 0x3FFFFFFFFF)
            self.p = res

        if dm:
            src = bit(op, 0x2D, 3)
            v = self.lfo[pc >> 4] if src < 4 else self.ram_read if src == 4 else 0 if src == 5 else pack24(self.p) if src == 6 else self.m[sm]
            self.mw[d3] = (dm, v)
        else:
            self.mw[d3] = (0, 0)
        self.rw[d3] = (dr, self.r[sr] if bit(op, 0x37) else pack24(self.p)) if dr else (0, 0)
        self.memw[d2] = self.p >> 15 if bit(op, 0x3D) else None
        self.ix[d3] = self.p >> 23 if bit(op, 0x3E) else None
        if bit(op, 0x3B):
            self.t[t] = self.tv[d2] if bit(op, 0x3C) else self.const[pc]
        self.tv[d2] = sext((self.p >> 8) & 0x7FFF, 16) if bit(op, 0x3E) else max(-0x8000, min(0x7FFF, self.p >> 23))

        mem = bit(op, 0x24, 2)
        if mem:
            base = self.offset[pc // 3] + (self.index if bit(op, 0x21) else 0)
            if mem == 1:
                self.ram[self.resolve(pc, base - self.counter)] = revram_encode(self.ram_write)
            else:
                one = 1 if mem == 3 else 0
                addr = (base + one) & 0x3FFFF if bit(op, 0x23) else self.resolve(pc, base - self.counter + one)
                self.memr[d2] = revram_decode(self.ram[addr])
        self.d3 = (d3 + 1) % 3
        self.d2 = (d2 + 1) % 2
        if self.trace is not None:
            self.trace.append((pc, self.p))

    def sample(self, inputs=None):
        """One pass (384 steps). `inputs` maps m register -> 24-bit value, loaded first (MAME: the mixer
        writes m20..m2f before the pass)."""
        for k, v in (inputs or {}).items():
            self.m[k] = v
        for pc in range(STEPS):
            self.step(pc)
        self.counter += 1


def load_an(rom=ROM, addr=AN_PROG):
    b = rom.read_bytes()
    n = struct.unpack_from(">H", b, addr)[0]
    return list(struct.unpack_from(">%dQ" % n, b, addr + 2))


def demo():
    one = 1 << 15                                     # 1.0 in const
    # p = c * r: opcode with mmode 2 (bits 0x16), m2 = r[sr], sr at 0x0b, a = 0 (asel 3, bits 0x18)
    mul = lambda sr, dr=0: (2 << 0x16) | (3 << 0x18) | (sr << 0x0B) | (dr << 0x30)
    g = Meg([mul(1, dr=2), 0, 0, 0, mul(2, dr=3)], const=[0x4000, 0, 0, 0, 0x4000])
    g.r[1] = 0x100000
    g.trace = []
    g.sample()
    assert dict(g.trace)[0] == 0x4000 * 0x100000              # p = c * r, 27.15
    assert g.r[2] == 0x80000 and g.r[3] == 0x40000            # written 3 steps later, read at step 4
    g = Meg([mul(1, dr=2), mul(2)], const=[0x4000, 0x4000])   # read at the next step: not landed yet
    g.r[1] = 0x100000
    g.trace = []
    g.sample()
    assert dict(g.trace)[1] == 0
    assert revram_decode(revram_encode(0x123450)) == 0x123400        # 12-bit float, low bits lost
    assert pack24(0x800000 * 32768) == 0x7FFFFF and m1_expand(0x5123) == 0x1123
    # the AN program loads and runs; with zero constants it is silent
    an = load_an()
    assert len(an) == 254 and an[0] == 0x00230020038401F7     # matches tools/meg_disasm.py's listing
    g = Meg(an)
    for _ in range(4):
        g.sample()
    assert all(v == 0 for v in g.r) and all(v == 0 for v in g.m)
    print("ok")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--an", action="store_true")
    ap.add_argument("--trace", action="store_true")
    a = ap.parse_args()
    g = Meg(load_an(), const=[0x2000] * STEPS)
    g.trace = [] if a.trace else None
    g.sample({0x20: 0x100000})
    for pc, p in g.trace or []:
        print("%03x %+.6f" % (pc, p / 2 ** 38))


if __name__ == "__main__":
    demo() if len(sys.argv) == 1 else main()

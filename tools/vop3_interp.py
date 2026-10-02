#!/usr/bin/env python3
"""VOP3 interpreter: runs a program over the fields docs/vop3_isa.md has measured, and no further.

    python tools/vop3_interp.py docs/vop3/program_0.bin --coef docs/vop3/coefficients_0.bin --channel 0 --trace
    python tools/vop3_interp.py                                     # self-check against the chip data

What is modelled (CHIP-measured, see the ISA doc, section 2-3; VOP3-1 and VOP3-2 are the same chip):
  class 1   r[rA] = k / 256                 (k the raw 16-bit constant; session 14: 8.8 fixed point)
  class 2   g = 0 if rd-en and rB else k    (the gain, k signed 1.15; session 11: the constant only ever
                                             multiplies; session 14: with rB set the read drops k, and
                                             w[rsrc] never enters; with rB clear k stays, session 12)
            s = the running value (the previous class-2 step's result)
            x = s when rB = 0 (rA alone is ignored, sessions 12/14/15), r[rB] when only rB is (session 13),
            r[rA] + r[rB] when both are set (session 15: a sum, for ops 0..3)
            s' = -s if sel else s                     (session 12: sel negates the running-value term)
            op 0: y = s' + g*x  op 1: y = g*x (+ x if rB)  op 2, 3: y = g*x  op 4: y = s'  op 5: sign of s
            op 6: y = 0         op 7: y = g * input, 0 on route 3   (session 11 `held`, 12 `op7pos`: the
            input port reads at any step; `dc dram`: a zero input gives exact zero for every op and k)
            y = |y| if r7[11] (session 15: 0.9997 x |s|, resid -51 dB)
            y clamps at +-8 (session 14); the running value carries from step to step unscaled by route
            w[wdst] = y; route 0 on an op-1 step adds y to the output bus
  class 0/3 nothing
Two files: r[0..7f] (7-bit, class-1 loads and the chip's per-voice I/O window, stride 6 per FS1R channel
straight through 0x40) and w[0..3f] (6-bit, class-2 results, read back through rsrc). They are not one
space: channel 12's reads of r[4a..4d] are channel 0's r[02..05] shifted by 72 (tools/vop3_verify.py).

Register banking (INFERRED, not measured directly): the chip closes filter channel 4 when its cutoff
load 08e is cleared even though the same register r[61] is loaded by 012 (channel 0), 097 (channel 7),
10a and 186 in the same pass with identical words, so the register file must be banked per filter
channel and the bank must come from the step's position, since the loads differ in nothing else.
`Interp.bank_of(step)` returns that bank; the default is the FS1R filter's layout (sixteen channels,
four groups of four interleaved, ISA doc section 1). Replace it for another chip.

Delay memory (session 16, test image 1): on a step 3 mod 4 (slot = step >> 2) r10 = 1 writes the step's
route-scaled output, 2/3 read the word written N passes ago (N = read offset - write offset), 4/6 one pass
older; `Interp.offs` holds the slot offsets into one 2^18-word ring. A step with daddr 0x180|n writes d[n]
(op 7 there reads it); daddr 0x100|n captures the DRAM transfer of slot s - 2 (else s - 1, else holds).
Measured inert, so not modelled: r9[12], path on a feed-forward step, mmode r8[6:0]. Not modelled: the
output's 2-sample lag and 18-bit word (LSB 2^-17), both of the DAC path. `Interp.ops` maps op -> f(s', g, x).

The self-check reproduces sessions 11-16 on VOP3-2 (`held`, silence, sel, the 8.8 loads, the rA + rB sum,
r7[11], test image 1's delay line) and session 8's cutoff MAC and per-channel step ownership.
"""
import math
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vop3_disasm import fields, disasm  # noqa: E402

SAT = 8.0                # session 14: what leaves the chip (DRAM word, d[], the DAC) clips at +-8
DAC = (0x10, 0x11)       # s27: the DAC takes d[0x10] (L) and d[0x11] (R), base steps 0cf / 0d0; readout = d / 4.
                         # s28: L leaves one sample after R (step onsets: R at n, L at n + 1); see Interp.dac()
DSAT = 128.0             # s27: d[] range (unit readout clips at the DAC, so only >= 32 is measured)
RDLAT = 8                # s29: steps from a DRAM read to its rbuf entry (tuned against the step responses)
SRCLO = lambda it, r, f: 0.0   # s29: f6c < 0x38 as op 7 / op 4 source (unmeasured; 0 fits rounds 13-15)
INPUT_D = (1, 5)         # ponytail: d[] cells the audio input arrives in (test image 0); the shipped
                         # programs' other external cells (VOP3-1 / mixer buses) are unmeasured
QBITS = None             # s30: result quantisation (fraction bits of a register word), None = exact
QUANT = lambda y: y if QBITS is None else math.floor(y * 2 ** QBITS) / 2 ** QBITS
DHI = lambda st: 0x40 if st >= 0x100 and DSPLIT else 0
DSPLIT = True
REGION = 1 << 16          # ponytail: region size guessed (offsets < 2^16 on Hall1); measure when a 0x1xx program uses memory
TAPKILL = False           # s33: 03f tap still writes DRAM (1367 echo at 1980); 08f result was the capture order
DLAT = 3                 # s31: steps from a d[] write to d[n] (d[3e] / d[22] / d[3b] taps), not pass end
CAPLAT = 3               # s30: steps from a capture (daddr 0x100|n) to d[n] (Hall1 tail onsets 1346 / 346 exact)
LATENCY = 3              # s24 round 10: register writes land three steps later
ACC = 256.0            # s24 rounds 5-9: the running value wraps at +-256 (mode 0), twice a register
RANGE = 128.0            # session 24 r7/r8: registers and the running value span [-128, 128), r = word / 256


def acc_mode(y, m):
    """The running value: modes 1-3 as a register; mode 0 wraps at +-ACC instead of +-128."""
    return (y + ACC) % (2 * ACC) - ACC if m == 0 else mode(y, m)


def mode(y, m):
    """r7[15:14] on the result (session 24 rounds 6-8): 0 wraps two's-complement, 1 saturates, 2 clamps
    negatives to 0, 3 takes |y|; 1, 2, 3 saturate at +-128."""
    if m == 0:
        return (y + RANGE) % (2 * RANGE) - RANGE
    y = max(-RANGE, min(RANGE - 1 / 256, y))
    return max(0.0, y) if m == 2 else abs(y) if m == 3 else y
LOAD_SCALE = 128.0       # session 14: a class-1 load v reads back as v/256 (8.8), i.e. 128 x its 1.15 value
ROUTE = (1 / 16, 1 / 8, 1 / 4, 1.0)   # session 12: route r7[13:12] scales what a step writes out (d[] / DRAM)
RSCALE = (1.0, 2.0, 4.0, 16.0)       # session 24: the same scale, applied to the result itself (route 0 = 1)
MEM = 1 << 18            # session 16: one ring of 2^18 words, address = pointer + slot offset (18-bit offsets;
                         # off_-15 = 0x3fff1 read the left's line 2^17 - 15 back; no alias at 2^14..2^16 + 17)


def s16(x):
    return struct.unpack(">h", struct.pack(">H", x & 0xFFFF))[0] / 32768.0


def fs1r_bank(step):
    """Filter channel (0..15) owning a step of the FS1R program, from the interleave (ISA doc, section 1):
    group g = (step - 0x10) // 124, index within the group's four channels, channel = 4g + index."""
    if step < 0x10:
        return 0
    g, off = divmod(step - 0x10, 124)
    return 4 * g + (off // 3 if off < 12 else (off - 12) % 4)


class Interp:
    def __init__(self, steps, coef, first=0, banks=16, bank_of=fs1r_bank):
        self.steps, self.coef, self.first = steps, coef, first
        self.bank_of = bank_of
        self.rb = [[0.0] * 128 for _ in range(banks)]     # register file, one bank per channel
        self.w = [0.0] * 64
        self.bus = 0.0
        self.acc = 0.0          # s, the running value
        self.inp = 0.0          # the chip's audio input, op 7's source
        self.d = [0.0] * 128    # s30: 64 cells live (daddr & 0x3f; Hall1's 1e2..1e7 write d[22..27])    # d[n]: written by a step with daddr 0x180|n, read by op 7 there, captured into by 0x100|n
        self.mem = [0.0] * MEM
        self.d_next = {}        # d[] writes by a step land at the end of the pass (session 16: N=1 == image 0)
        self.offs = {}          # DRAM slot -> 18-bit offset (registers 0xd/0xe after register 0 = slot)
        self.ptr = 0            # the delay-line pointer, one word per pass
        self.bus_hist = []      # (slot, value) of each DRAM transfer this pass: what a capture picks up
        self.trace = None
        self.n, self.pend = 0, []
        self.zw, self.x, self.px, self.dprev = [], 0.0, 0.0, 0.0
        self.wlatch = None
        self.cpend = []
        self.rbuf, self.rpend = [0.0] * 8, []     # s29: DRAM read buffer (op 7 / op 4 source 0x38 + j)   # global step counter and pending register writes (LATENCY)
        # session 18: the rd-en gain file G[rsrc] as found on test image 0 after the effect programs ran. Steps that
        # write w[wdst] in the same pass do not change it (rsrc 2 with w[02] written), so its writer is unknown.
        self.gsel = [0.99988, 0.10144, -0.07422, 0.0, -0.33203, 0.0, 0.99988, 0.0, -0.50757, 0.0, 0.0, 0.0,
                     -1.0, -1.0, 0.0, 0.0]
        self.ops = {0: lambda s, g, x: s + g * x, 1: lambda s, g, x: g * x, 2: lambda s, g, x: g * x,
                    3: lambda s, g, x: g * x, 4: lambda s, g, x: s + g * self.src,   # s30: op 4 = s + k d[f6c] (FIR taps)
                    5: lambda s, g, x: -2.0 ** -17 if s < 0 else 0.0,
                    6: lambda s, g, x: 0.0, 7: lambda s, g, x: g * self.src}

    @property
    def r(self):
        return self.rb[0]

    def addr(self, slot, older=0, mmode=0):
        """The word written `offset(slot) - offset(write slot) + older` passes ago (the pointer steps down).
        s31: mmode bits 3:2 pick a region (base 0x40, the 0x1xx programs 0x44 / 0x48); without it Hall1's 0x1d3
        zeroes 0xd1's comb line and d[26] / d[27] / d[39] / d[3c] stay silent."""
        # s34: 128 slot offsets (FUN_000397F4 slot lists run to 127); the upper slots carry the area bases
        # (0x10000 / 0x20000 / 0x30000) that s31's mmode REGION guess stood in for. With only 64 dumped, the
        # upper slots fall back to the old guess.
        if slot & 0x7F in self.offs:
            return (self.ptr + self.offs[slot & 0x7F] + older) & (MEM - 1)
        return (self.ptr + self.offs.get(slot & 0x3F, 0) + older + REGION * (mmode >> 2 & 3)) & (MEM - 1)

    def captured(self, old, st):
        """s36k: a capture at step st takes the latest DRAM transfer of this pass made 3..11 steps earlier, else
        keeps its old value (replaces s16's slot s-2 / s-1: 0ce takes 0c9's read; Hall1 / Hall9 click replays
        63 -> 0.6 LSB rms; d[12] at 0be reads 0 on the unit as predicted)."""
        for sl, v, t in reversed(self.bus_hist):
            if st - 12 < t <= st - 3:
                return v
        return old

    def write(self, r, reg, v):
        """s24 round 10: a register written at step n is read by step n + 3 on; steps n + 1, n + 2 see the old value."""
        self.pend.append((self.n + LATENCY, r, reg, v))

    def run_step(self, i):
        self.n += 1
        if self.pend:
            due = [p for p in self.pend if p[0] <= self.n]
            if due:
                self.pend = [p for p in self.pend if p[0] > self.n]
                for _, rr, reg, v in due:
                    rr[reg] = v
        if self.rpend and self.rpend[0][0] <= self.n:
            while self.rpend and self.rpend[0][0] <= self.n:
                _, j, v = self.rpend.pop(0)
                self.rbuf[j] = v
        while self.cpend and self.cpend[0][0] <= self.n:
            _, n_, v_ = self.cpend.pop(0)
            self.d[n_] = v_
        st = self.first + i
        f = fields(*self.steps[i])
        r = self.rb[self.bank_of(st)]
        dram = st & 3 == 3                     # session 16: DRAM transfers happen only on steps 3 mod 4
        if (dram or st & 3 == 1) and f["mem"] in (2, 3, 4, 6):  # read (s16: 2, 3 alike; 4, 6 one word older); s17: phase 1 reads
            v = self.mem[self.addr(st >> 2, 1 if f["mem"] & 4 else 0, f["mmode"])]
            self.bus_hist.append((st >> 2, v, st))
            self.rpend.append((self.n + RDLAT, (st >> 2) & 7, v))   # s29: read data lands in rbuf[slot & 7]
        if f["daddr"] & 0x180 == 0x100:        # capture into d[n], seen by later steps this pass
            n = f["daddr"] & 0x3F | DHI(st)
            self.cpend.append((self.n + CAPLAT, n, self.captured(self.d[n], st)))   # s30: captures land late
        # Sessions 24 rounds 1-3 (one model for every probe of sessions 11-24): class 1 loads r[rA] = k/256 plus
        # r[rB] (rB set) or the running value s, and s takes the result. Classes 2/3: x = r[rB], where rB = 0 is
        # r[0], the chip's audio input; y = op(s, g, x); y is scaled by the route, x1 / x2 / x4 / x16; s = y;
        # rA != 0 writes r[rA] = y. Class 3 is class 2 with g = 1.
        cls = f["f6a"]
        if cls not in (2, 3):                   # s27 r12: every step fetches x = r[rB] for the z^-1 latch
            self.px, self.x = self.x, (r[f["rb"]] if f["rb"] else None)
        if cls == 1:
            v = (LOAD_SCALE * s16(self.coef[self.first + i]) if self.coef else 0.0) + self.acc   # s27a: rB does not replace s (chained loads accumulate)
            v *= RSCALE[f["f7b"]]
            self.write(r, f["ra"], mode(v, self.steps[i][3] >> 14))   # s24: route and mode on the register
            self.acc = acc_mode(v, self.steps[i][3] >> 14)  # s24 r5/r9: the running value has its own width
            if f["daddr"] & 0x180 == 0x180 and self.steps[i][1] >> 8 & 0x1F:   # s29: class 1 with r9[12:8] set writes d[n]
                # (the 8 shipped-Hall1 taps on 1c7c / 1d01 / 1c7e loads; plain / op-6 / op-1 loads write nothing)
                self.dput(f["daddr"] & 0x3F | DHI(st), max(-DSAT, min(DSAT, self.acc)))
            return
        if cls not in (2, 3):
            if cls == 0 and (dram or st & 3 == 1) and f["mem"] == 1 and self.wlatch is not None:
                v, self.wlatch = self.wlatch, None                 # s34: class 0 mem 1 writes the latched value
                self.mem[self.addr(st >> 2, 0, f["mmode"])] = v
                self.bus_hist.append((st >> 2, v, st))
            return
        k = s16(self.coef[self.first + i]) if self.coef else 0.0
        if cls == 3:
            g = 1.0
        elif f["rd_en"] and f["rb"]:
            g = self.gsel[f["rd"]]                 # session 18: rd-en with rB set takes its gain from G[rsrc]
        else:
            g = k
        r[0] = self.inp
        x = r[f["rb"]]
        self.px, self.x = self.x, (x if f["rb"] else None)
        op = f["op7"]
        # s29: r6[5:0] (f6c) selects op 7's / op 4's k operand: 0x38-0x3f = the DRAM read buffer, else r[f6c]
        # s29: op 7 reads d[r6[5:0]] (f6c), never via daddr (s24 r14 S_w18d_r18d: daddr 18d, f6c 0 reads 0;
        # D_o7_10 reads d[10]); d[1] / d[5] carry the chip's audio input (test image 0's 0d0 / 0d4)
        self.psrc, self.src = getattr(self, "src", 0.0), self.d[f["f6c"] | DHI(st)]
        s = -self.acc if f["r7sel"] else self.acc
        if op == 5 and f["rb"] and cls == 2:
            y = x + g * self.src     # s30/s35: op 5 with rB = r[rB] + k d[f6c] (comb write); s24 r11's max(., 0) was the x load's mode 2
        else:
            y = x if cls == 3 and op == 5 else self.ops[op](s, g, x) + (x if op == 1 and f["rb"] else 0.0)
        if self.steps[i][3] >> 11 & 1:
            y = abs(y)                                            # session 15: r7[11] rectifies
        # s24 r5: the route-3 x16 scale saturates (a -127.5 running value passed by op 4 at route 3 reads -8 on the
        # DAC in the shipped mode 1, but +8 when the step's mode is 0: wrap of -127.5 x 16 = -2040 -> +8 at the clip)
        y *= RSCALE[f["f7b"]]
        y = QUANT(y)
        if f["ra"] and f["r9b7"]:   # s27: rA | 0x80 stores the previous step's x operand (a z^-1 shift)
            self.write(r, f["ra"], x if self.px is None else self.px)   # s27 r12: after an rB-0 step, its own x
        elif f["ra"]:
            self.write(r, f["ra"], mode(y, self.steps[i][3] >> 14))
        y = self.acc = acc_mode(y, self.steps[i][3] >> 14)
        out = max(-SAT, min(SAT, y))                # what leaves for d[] / DRAM clips at +-8 (s14)
        if f["daddr"] & 0x1C0 == 0x1C0:            # s32/s33: daddr bit 6 writes the previous step's operand d[f6c],
            self.dput(f["daddr"] & 0x3F | DHI(st), self.psrc)   # whatever this step's op / k or the previous k (s32a, cumulative)
        elif f["daddr"] & 0x180 == 0x180 and (op != 7 or f["daddr"] & 0x3F != f["f6c"]):   # s30: op 7 writes d[] too (not onto its own source cell)
            self.dput(f["daddr"] & 0x3F | DHI(st), max(-DSAT, min(DSAT, y)))   # s27: d[] is wider than the DRAM word
        if self.steps[i][1] >> 13 & 1:          # s34: r9[13] = this output is the next DRAM write's data;
            if (dram or st & 3 == 1) and f["mem"] == 1:   # a writer marked so writes its own (replaces s30's
                self.wlatch = None                 # op-5 latch and s33's class-0-writes-nothing)
            else:
                self.wlatch = out
        if (dram or st & 3 == 1) and f["mem"] == 1 and not (TAPKILL and f["daddr"] & 0x180 == 0x180):   # write (r10 = 1; s16); s17 / s30: phase-1 writes live
            # s33 H: a step that also writes d[] (daddr 0x180|n) does not write DRAM
            if self.wlatch is not None:
                out, self.wlatch = self.wlatch, None
            self.mem[self.addr(st >> 2, 0, f["mmode"])] = out
            self.bus_hist.append((st >> 2, out, st))
        if op == 1 and f["f7b"] == 0:
            self.bus += y
        if self.trace is not None:
            self.trace.append((self.first + i, y))

    def dac(self):
        """(L, R) as the unit plays them after this pass, readout units (d / 4, clipped at +-8): L is d[0x10]
        from the previous pass (s28 step responses)."""
        q = lambda v: math.floor(max(-8.0, min(8.0, v / 4)) * 16384) / 16384   # s28: 18-bit DAC word, floor (2^-17 FS)
        out = (q(self.dprev), q(self.d[DAC[1]]))
        self.dprev = self.d[DAC[0]]
        return out

    def dput(self, n, v):
        # s31: a step's d[] write lands DLAT steps later in the same pass (taps of d[3e] after 04f), else at pass end
        if DLAT is None:
            self.d_next[n] = v
        else:
            self.cpend.append((self.n + DLAT, n, v)); self.cpend.sort(key=lambda t: t[0])

    def sample(self, inputs=None, bank=0, inp=0.0):
        """One pass over the program. `inputs` maps register -> value written into `bank` first; `inp` is
        the chip's audio input (op 7)."""
        self.bus, self.inp = 0.0, inp
        for n in INPUT_D:
            self.d[n] = inp
        for reg, v in (inputs or {}).items():
            self.rb[bank][reg] = v
        for i in range(len(self.steps)):
            self.run_step(i)
        for rr, reg, v in self.zw:
            rr[reg] = v
        self.zw = []
        for n, v in self.d_next.items():
            self.d[n] = v
        self.d_next, self.bus_hist = {}, []
        self.ptr = (self.ptr - 1) & (MEM - 1)
        return self.bus


def load(path, an=False):
    b = Path(path).read_bytes()
    if an:
        rows = [struct.unpack_from(">8H", b, i * 16) for i in range(len(b) // 16)]
        return [r[3:] for r in rows], [r[2] for r in rows]
    return [struct.unpack_from(">5H", b, i * 10) for i in range(len(b) // 10)], None


def channel_steps(first, c):
    """The FS1R filter: index c (0..3) of the 124-step group at `first` (ISA doc, section 1). Filter channel
    = 4 * group + c, groups at 0x10, 0x8c, 0x108, 0x184."""
    return [first + 3 * c + k for k in range(3)] + [first + 0x0C + c + 4 * n for n in range(28)]


def demo():
    # Session 11 `held` (0d3's op and k on audio): gain against the shipped op-4 word, both read the same way.
    chain = [(0, 0, 0, 0x01C0, 0x8001), (0, 0, 0, 0x0100, 0x8002), (0, 0, 0, 0x0100, 0x8003), (0, 0, 0xC580, 0x7100, 0x8004)]

    def d3(op, k, inp=0.5):
        st = chain[:3] + [(0, 0, chain[3][2], (chain[3][3] & ~0x1C0) | op << 6, chain[3][4])]
        it = Interp(st, [0x7FFF, 0x7FFF, 0x7FFF, k], bank_of=lambda _: 0)
        it.trace = []
        it.sample(inp=0.01)                           # small, so op 0 (s + k s, x16) stays under the clip
        t = dict(it.trace)
        return t[3] / (16 * t[2])                     # against the shipped op 4, which passes 16 x the chain
    measured = {0: (1.254, 1.504, 0.501), 1: (0.252, 0.504, -0.499), 2: (0.251, 0.502, -0.501),
                3: (0.252, 0.502, -0.501), 4: (1.0, 1.0, 1.0)}
    for op, gains in measured.items():
        for k, want in zip((0x2000, 0x4000, 0xC000), gains):
            assert abs(d3(op, k) - want) < 0.006, (op, hex(k), d3(op, k), want)
    # Session 24 rounds 1-3: 66 DC probes (classes 1/2/3, routes, rA/rB destination and operand, rB = 0 = r[0])
    import vop3_probe
    n, bad = vop3_probe.check()
    assert not bad, bad
    # Session 16: test image 1's delay line still plays at lag N - 1
    print("ok (%d s24 probes)" % n)


if __name__ == "__main__":
    demo() if len(sys.argv) == 1 else main()

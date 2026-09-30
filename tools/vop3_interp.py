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

OPEN (docs/vop3_isa.md section 8): the chain-head effect (a non-op-7 step
right after the head, or no head, brings the previous pass's value in); delay memory. Measured inert on
VOP3-2, not modelled: r9[12], path
r9[11:8], r8[6:0], r10, 0e9's daddr, the DRAM offset registers (image 0 has no delay line). Not modelled: `route`'s scale on
the value a step sends out (session 12, relative: 3 : 2 : 1 : 0 = 1 : 1/4 : 1/8 : 1/16), the output's
18-bit word (LSB 2^-17), delay memory. `Interp.ops` maps op -> f(s', g, x).

The self-check reproduces session 11's `held` table on VOP3-2's 0d0..0d3 chain, take 2's silence, and
session 8's cutoff MAC and per-channel step ownership.
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vop3_disasm import fields, disasm  # noqa: E402

SAT = 8.0                # session 14: the running value / DRAM word clips at +-8 (seen at two output gains)
LOAD_SCALE = 128.0       # session 14: a class-1 load v reads back as v/256 (8.8), i.e. 128 x its 1.15 value


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
        self.trace = None
        self.ops = {0: lambda s, g, x: s + g * x, 1: lambda s, g, x: g * x, 2: lambda s, g, x: g * x,
                    3: lambda s, g, x: g * x, 4: lambda s, g, x: s,
                    5: lambda s, g, x: -2.0 ** -17 if s < 0 else 0.0,
                    6: lambda s, g, x: 0.0, 7: lambda s, g, x: 0.0 if self.route == 3 else g * self.inp}

    @property
    def r(self):
        return self.rb[0]

    def run_step(self, i):
        f = fields(*self.steps[i])
        r = self.rb[self.bank_of(self.first + i)]
        if f["f6a"] == 1:
            r[f["ra"]] = LOAD_SCALE * s16(self.coef[self.first + i]) if self.coef else 0.0
            return
        if f["f6a"] != 2:
            return
        k = s16(self.coef[self.first + i]) if self.coef else 0.0
        g = 0.0 if f["rd_en"] and f["rb"] else k      # sessions 9/12/14: rd-en drops k only when rB is set; w[] is never the gain
        x = self.acc if not f["rb"] else r[f["ra"]] + r[f["rb"]] if f["ra"] else r[f["rb"]]
        op = f["op7"]
        self.route = f["f7b"]              # r7[13:12]
        s = -self.acc if f["r7sel"] else self.acc
        y = self.ops[op](s, g, x) + (x if op == 1 and f["rb"] else 0.0)   # session 14: op 1 with rB adds r[rB]
        if self.steps[i][3] >> 11 & 1:
            y = abs(y)                                            # session 15: r7[11] rectifies
        y = max(-SAT, min(SAT, y))
        self.acc = y
        if f["f6c"]:
            self.w[f["f6c"]] = y
        if op == 1 and f["f7b"] == 0:
            self.bus += y
        if self.trace is not None:
            self.trace.append((self.first + i, y))

    def sample(self, inputs=None, bank=0, inp=0.0):
        """One pass over the program. `inputs` maps register -> value written into `bank` first; `inp` is
        the chip's audio input (op 7)."""
        self.bus, self.inp = 0.0, inp
        for reg, v in (inputs or {}).items():
            self.rb[bank][reg] = v
        for i in range(len(self.steps)):
            self.run_step(i)
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
    # session 11 (VOP3-2, test image 0): the right input chain 0d0 (op 7, k 7fff) -> 0d1, 0d2 (op 4) ->
    # 0d3 (op 4, writes DRAM d[18b]). `held` swept 0d3's op and k with audio on the input.
    chain = [(0, 0, 0, 0x01C0, 0x8001), (0, 0, 0, 0x0100, 0x8002), (0, 0, 0, 0x0100, 0x8003), (0, 0, 0xC580, 0x7100, 0x8004)]

    def d3(op, k, inp, sel=0, ra=0, abs_=0):
        st = chain[:3] + [(0, ra, chain[3][2], (chain[3][3] & ~0xDC0) | op << 6 | sel << 10 | abs_ << 11, chain[3][4])]
        it = Interp(st, [0x7FFF, 0, 0, k], bank_of=lambda _: 0)
        it.trace = []
        it.sample(inp=inp)
        y = dict(it.trace)[3]
        return y / (0x7FFF / 32768.0 * inp) if inp else y          # gain relative to the shipped word (op 4)

    measured = {0: (1.254, 1.504, 0.501), 1: (0.252, 0.504, -0.499), 2: (0.251, 0.502, -0.501),
                3: (0.252, 0.502, -0.501), 4: (1.0, 1.0, 1.0), 6: (0, 0, 0), 7: (0, 0, 0)}
    for op, gains in measured.items():
        for k, want in zip((0x2000, 0x4000, 0xC000), gains):
            assert abs(d3(op, k, 0.5) - want) < 0.006, (op, hex(k), d3(op, k, 0.5), want)
    assert abs(d3(5, 0x4000, -0.5)) < 1e-4                       # op 5: sign only, ~-2^-17
    assert all(d3(op, k, 0.0) == 0.0 for op in range(8) for k in (0x2000, 0x4000, 0xC000, 0x7FFF))  # `dc dram`
    # session 12: sel on op 0..4 (k 0x4000) and op 1 with rA = r[61] alone
    for op, want in {0: -0.4993, 1: 0.5003, 2: 0.5018, 3: 0.5018, 4: -0.9991}.items():
        assert abs(d3(op, 0x4000, 0.5, sel=1) - want) < 0.006, (op, d3(op, 0x4000, 0.5, sel=1), want)
    assert abs(d3(1, 0x7FFF, 0.5, ra=0x61) - 1.0022) < 0.006

    # session 14: class-1 r[30] = v, then one step reading it; stored value = right channel / 0e9's gain
    def s14(v, word, k):
        it = Interp([(0, 0x30, 0, 0x8000, 0x4000), word], [v, k], bank_of=lambda _: 0)
        it.trace = []
        it.sample()
        return dict(it.trace)[1]
    op1 = (0, 0, 0, 0x0040, 0x9800)                              # op 1, rB = 30
    op1rd = (0, 0, 0, 0x005E, 0x9800)                            # the same with rd-en, rsrc e
    for v, word, k, want in ((0x0100, op1, 0x7FFF, 2.000), (0x0400, op1, 0x2000, 5.000), (0x0400, op1, 0x7FFF, 8.0),
                             (0x1000, op1, 0x7FFF, 8.0), (0x0400, op1rd, 0x2000, 4.000), (0x0400, op1rd, 0x7FFF, 4.000),
                             (0x0400, (0, 0, 0, 0, 0x9800), 0x7FFF, 4.000), (0x1000, (0, 0, 0, 0, 0x9800), 0x7FFF, 8.0)):
        assert abs(s14(v, word, k) - want) < 0.001, (hex(v), word, hex(k), s14(v, word, k), want)
    # session 15 (note off): r[30] = 1.0, r[31] = 0.5/1/2, k 0x4000; op 1 rB=30 with k 0xc000/0/0x4000
    def s15(r31, word, k, r30=0x0100):
        it = Interp([(0, 0x30, 0, 0x8000, 0x4000), (0, 0x31, 0, 0x8000, 0x4000), word], [r30, r31, k], bank_of=lambda _: 0)
        it.trace = []
        it.sample()
        return dict(it.trace)[2]
    both = lambda o: (0, 0x31, 0, o << 6, 0x9800)                 # rA = 31, rB = 30
    for r31, want in ((0x0080, 0.75), (0x0100, 1.0), (0x0200, 1.5)):
        for o in (2, 0):
            assert abs(s15(r31, both(o), 0x4000) - want) < 1e-6, (o, hex(r31), s15(r31, both(o), 0x4000))
    assert abs(s15(0x0100, both(3), 0x4000) - 1.0) < 1e-6 and abs(s15(0x0100, both(1), 0x4000) - 3.0) < 1e-6
    for k, want in ((0xC000, 0.5), (0x0000, 1.0), (0x4000, 1.5)):
        assert abs(s15(0, op1, k) - want) < 1e-6, (hex(k), s15(0, op1, k))
    assert d3(1, 0xC000, 0.5) < 0 and abs(d3(1, 0xC000, 0.5, abs_=1) + d3(1, 0xC000, 0.5)) < 1e-9   # r7[11]: |y|

    steps, _ = load(Path(__file__).resolve().parents[1] / "docs/vop3/program_0.bin")
    coef = list(struct.unpack(">512H", (Path(__file__).resolve().parents[1] / "docs/vop3/coefficients_0.bin").read_bytes()))
    # channel 0 of group 2: the cutoff load 08e goes to r[61]; the MAC 098 reads r[62] * r[61] -> w[3e]
    own = channel_steps(0x8C, 0)
    assert own[:3] == [0x8C, 0x8D, 0x8E] and 0x98 in own and 0x99 not in own and 0x104 in own
    assert all(fs1r_bank(st) == 4 for st in own) and fs1r_bank(0x97) == 7 and fs1r_bank(0x99) == 5 and fs1r_bank(0x12) == 0
    it = Interp(steps, coef)
    it.trace = []
    it.sample()                                                  # r[62] unloaded: 0 (session 15, r[2c..7f])
    y098 = dict(it.trace)[0x98]                                  # op 2: k * (r[62] + r[61]) = k * r[61]
    assert abs(y098 - max(-SAT, min(SAT, s16(coef[0x98]) * LOAD_SCALE * s16(coef[0x8E])))) < 1e-9 and y098 != 0.0
    # clear the cutoff load (session 8 take 3 nop_08e): the MAC's result vanishes
    it2 = Interp([(0, 0, 0, 0, 0) if i == 0x8E else s for i, s in enumerate(steps)], coef)
    it2.trace = []
    it2.sample()
    assert dict(it2.trace)[0x98] == 0.0
    # channel 4 (group 3, block 6) is the one session 9 ran on: its output stage is 0x174, constant 0x17c
    assert channel_steps(0x108, 0)[27] == 0x174 and channel_steps(0x108, 0)[29] == 0x17C
    # session 9 kf8: 0f8 is an rd-en step, so its constant is not the gain and changing it changes nothing
    it4 = Interp(steps, [0x4000 if i == 0xF8 else k for i, k in enumerate(coef)])
    it4.sample({0x1D: 0.25}, bank=4)
    it5 = Interp(steps, coef)
    it5.sample({0x1D: 0.25}, bank=4)
    assert it4.bus == it5.bus
    print("ok")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("program")
    ap.add_argument("--an", action="store_true", help="8-word AN image (constants inline)")
    ap.add_argument("--coef", help="512 x u16 constants for a 5-word image")
    ap.add_argument("--first", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--channel", type=int, help="FS1R filter: list this channel's steps of the group at --group")
    ap.add_argument("--group", type=lambda x: int(x, 0), default=0x8C)
    ap.add_argument("--trace", action="store_true", help="print each step's result for one sample")
    a = ap.parse_args()
    steps, coef = load(a.program, a.an)
    if a.coef:
        c = Path(a.coef).read_bytes()
        coef = list(struct.unpack(">%dH" % (len(c) // 2), c))
    if a.an:
        coef = [0] * a.first + list(coef)
    it = Interp(steps, coef, a.first)
    if a.channel is not None:
        for st in channel_steps(a.group, a.channel):
            print(disasm(st, steps[st - a.first], coef))
        return
    it.trace = [] if a.trace else None
    print("bus:", it.sample())
    for st, y in it.trace or []:
        if y:
            print("%03x  %+.6f" % (st, y))


if __name__ == "__main__":
    demo() if len(sys.argv) == 1 else main()

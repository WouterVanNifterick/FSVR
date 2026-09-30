#!/usr/bin/env python3
"""VOP3 interpreter: runs a program over the fields docs/vop3_isa.md has measured, and no further.

    python tools/vop3_interp.py docs/vop3/program_0.bin --coef docs/vop3/coefficients_0.bin --channel 0 --trace
    python tools/vop3_interp.py                                     # self-check against the chip data

What is modelled (CHIP-measured, see the ISA doc, section 2-3; VOP3-1 and VOP3-2 are the same chip):
  class 1   r[rA] = k                       (k signed 1.15)
  class 2   g = w[rsrc] if rd-en else k     (the gain; FS1R.unlock session 11: the constant only ever multiplies)
            s = the running value (the previous class-2 step's result)
            x = r[rA] * r[rB] over the non-zero operand fields, s when both are 0
            op 0: y = s + g*x   op 1..3: y = g*x   op 4: y = s   op 5: y = sign of s (-2^-17 or 0)
            op 6: y = 0         op 7: y = g * in[step]        (session 11 `held`, VOP3-2 0d3; `dc dram`:
            a zero input gives exact zero for every op and k. in[step] is the chip's input port at that step:
            op 7 heads VOP3-2's chains (0d0/0d4) and gives exact zero mid-chain at 0d3)
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

INFERRED, not yet split on the chip: `x` with non-zero operands (session 11 measured only rA = rB = 0,
where x is s; session 8's cutoff MAC follows rA x rB), and which steps have an input port (`inp` is
per step; only 0d0/0d4 are known to). Not modelled: accumulator width and
saturation beyond 1.15, `path`, delay memory (r10/r8), `sel`. `Interp.ops` maps op -> f(s, g, x).

The self-check reproduces session 11's `held` table on VOP3-2's 0d0..0d3 chain, take 2's silence, and
session 8's cutoff MAC and per-channel step ownership.
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vop3_disasm import fields, disasm  # noqa: E402

SAT = 32767 / 32768.0


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
        self.inp = {}           # step -> the chip's audio input at that step, op 7's source
        self.trace = None
        self.ops = {0: lambda s, g, x: s + g * x, 1: lambda s, g, x: g * x, 2: lambda s, g, x: g * x,
                    3: lambda s, g, x: g * x, 4: lambda s, g, x: s,
                    5: lambda s, g, x: -2.0 ** -17 if s < 0 else 0.0,
                    6: lambda s, g, x: 0.0, 7: lambda s, g, x: g * self.inp.get(self.step, 0.0)}

    @property
    def r(self):
        return self.rb[0]

    def run_step(self, i):
        f = fields(*self.steps[i])
        r = self.rb[self.bank_of(self.first + i)]
        if f["f6a"] == 1:
            r[f["ra"]] = s16(self.coef[self.first + i]) if self.coef else 0.0
            return
        if f["f6a"] != 2:
            return
        k = s16(self.coef[self.first + i]) if self.coef else 0.0
        g = self.w[f["rd"]] if f["rd_en"] else k       # exclusive on the FS1R (0/240 rd-en steps carry k); the AN's few k+rd steps are not modelled
        regs = [r[n] for n in (f["ra"], f["rb"]) if n]
        x = regs[0] * regs[1] if len(regs) == 2 else regs[0] if regs else self.acc
        op = f["op7"]
        self.step = self.first + i
        y = max(-1.0, min(SAT, self.ops[op](self.acc, g, x)))
        self.acc = y
        if f["f6c"]:
            self.w[f["f6c"]] = y
        if op == 1 and f["f7b"] == 0:
            self.bus += y
        if self.trace is not None:
            self.trace.append((self.first + i, y))

    def sample(self, inputs=None, bank=0, inp=None):
        """One pass over the program. `inputs` maps register -> value written into `bank` first; `inp` maps
        step -> the chip's audio input there (op 7)."""
        self.bus, self.inp = 0.0, inp or {}
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

    def d3(op, k, inp):
        st = chain[:3] + [chain[3][:3] + ((chain[3][3] & ~0x1C0) | op << 6, chain[3][4])]
        it = Interp(st, [0x7FFF, 0, 0, k], bank_of=lambda _: 0)
        it.trace = []
        it.sample(inp={0: inp})
        y = dict(it.trace)[3]
        return y / (0x7FFF / 32768.0 * inp) if inp else y          # gain relative to the shipped word (op 4)

    measured = {0: (1.254, 1.504, 0.501), 1: (0.252, 0.504, -0.499), 2: (0.251, 0.502, -0.501),
                3: (0.252, 0.502, -0.501), 4: (1.0, 1.0, 1.0), 6: (0, 0, 0), 7: (0, 0, 0)}
    for op, gains in measured.items():
        for k, want in zip((0x2000, 0x4000, 0xC000), gains):
            assert abs(d3(op, k, 0.5) - want) < 0.006, (op, hex(k), d3(op, k, 0.5), want)
    assert abs(d3(5, 0x4000, -0.5)) < 1e-4                       # op 5: sign only, ~-2^-17
    assert all(d3(op, k, 0.0) == 0.0 for op in range(8) for k in (0x2000, 0x4000, 0xC000, 0x7FFF))  # `dc dram`

    steps, _ = load(Path(__file__).resolve().parents[1] / "docs/vop3/program_0.bin")
    coef = list(struct.unpack(">512H", (Path(__file__).resolve().parents[1] / "docs/vop3/coefficients_0.bin").read_bytes()))
    # channel 0 of group 2: the cutoff load 08e goes to r[61]; the MAC 098 reads r[62] * r[61] -> w[3e]
    own = channel_steps(0x8C, 0)
    assert own[:3] == [0x8C, 0x8D, 0x8E] and 0x98 in own and 0x99 not in own and 0x104 in own
    assert all(fs1r_bank(st) == 4 for st in own) and fs1r_bank(0x97) == 7 and fs1r_bank(0x99) == 5 and fs1r_bank(0x12) == 0
    it = Interp(steps, coef)
    it.trace = []
    it.rb[4][0x62] = 0.5
    it.sample()
    y098 = dict(it.trace)[0x98]                                  # op 2: k * r[62] * r[61]
    assert abs(y098 - s16(coef[0x98]) * 0.5 * s16(coef[0x8E])) < 1e-9 and y098 != 0.0
    # clear the cutoff load (session 8 take 3 nop_08e): the MAC's result vanishes
    it2 = Interp([(0, 0, 0, 0, 0) if i == 0x8E else s for i, s in enumerate(steps)], coef)
    it2.trace = []
    it2.rb[4][0x62] = 0.5
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

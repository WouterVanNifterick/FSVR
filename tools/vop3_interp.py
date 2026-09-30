#!/usr/bin/env python3
"""VOP3 interpreter: runs a program over the fields docs/vop3_isa.md has measured, and no further.

    python tools/vop3_interp.py docs/vop3/program_0.bin --coef docs/vop3/coefficients_0.bin --channel 0 --trace
    python tools/vop3_interp.py                                     # self-check against the chip data

What is modelled (CHIP-measured, see the ISA doc, section 2-3):
  class 1   r[rA] = k                       (k signed 1.15)
  class 2   a = r[rA]; b = r[rB]; c = r[0x40 | rsrc] if rd-en else k
            (FW: the firmware patches k into class-2 steps on all three machines, never into an rd-en step;
             rd-en swaps the read in for the constant, the MEG's m1 = const | t)
            op 2: y = a * b                 (multiply-class: the cutoff MAC)
            op 1: y = a if sel else CONST   (move-class: the output stage puts a on its route)
            op 5: y = a                     (pass-through: on the output stage, the unfiltered input)
            other: y = OP[op](a, b, c)      (MEG mapping, section 7 of the ISA doc)
            if rd-en: y = y - c             (the read flattens the response on the output stage)
            r[0x40 | wdst] = y; route 0 on an op-1 step adds y to the output bus
  class 0/3 nothing
One register file of 128: class-1 loads name any of it, class-2 results land in the upper half (FW: 97% of
reads >= 0x40 name a written slot, r[40] is the never-written zero register).

Register banking (INFERRED, not measured directly): the chip closes filter channel 4 when its cutoff
load 08e is cleared even though the same register r[61] is loaded by 012 (channel 0), 097 (channel 7),
10a and 186 in the same pass with identical words, so the register file must be banked per filter
channel and the bank must come from the step's position, since the loads differ in nothing else.
`Interp.bank_of(step)` returns that bank; the default is the FS1R filter's layout (sixteen channels,
four groups of four interleaved, ISA doc section 1). Replace it for another chip.

What is NOT modelled and is left as a knob rather than guessed: the arithmetic of ops 0, 3, 4, 6, 7,
accumulator width and saturation, what `path` selects, delay memory (r10/r8), `sel` on non-move ops.
`Interp.ops` is a dict you can replace per op to test a hypothesis against the chip takes.

The self-check reproduces the FS1R.unlock session 8/9 signatures qualitatively: clearing the cutoff
load kills the MAC's result, a negative control constant mutes, op 1 with sel=0 emits DC, and the
per-channel step ownership of a group.
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
        self.rb = [[0.0] * 128 for _ in range(banks)]     # register file, one bank per channel; w[n] = r[0x40 | n]
        self.bus = 0.0
        self.trace = None
        # op semantics from the MEG cross-check (docs/vop3_isa.md section 7); the FS1R exposes no
        # accumulator, so these are the MEG's operations, not gate-level FS1R truth.
        # 2/5 = fresh product a*b (MEG p=c*r); 1/6 = forward-move a (MEG p=m, read-carrying);
        # 0/3/4/7 = multiply-accumulate a*b + c (MEG p = c*r + p).
        self.ops = {0: lambda a, b, c: a * b + c, 3: lambda a, b, c: a * b + c,
                    4: lambda a, b, c: a * b + c, 7: lambda a, b, c: a * b + c,
                    6: lambda a, b, c: a}

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
        a, b = r[f["ra"]], r[f["rb"]]
        k = s16(self.coef[self.first + i]) if self.coef else 0.0
        c = r[0x40 | f["rd"]] if f["rd_en"] else k     # exclusive on the FS1R (0/240 rd-en steps carry k); the AN's few k+rd steps are not modelled
        op = f["op7"]
        if op == 2 or op == 5:
            y = a * b                                # fresh product (MEG p = c*r); op 5 discards the accumulator
        elif op == 1:
            y = a if f["r7sel"] else 1.0            # forward-move a; sel=0 puts a constant on the route
        else:
            y = self.ops[op](a, b, c)
        if f["rd_en"]:
            y = y - c
        y = max(-1.0, min(SAT, y))
        if f["f6c"]:
            r[0x40 | f["f6c"]] = y
        if op == 1 and f["f7b"] == 0:
            self.bus += y
        if self.trace is not None:
            self.trace.append((self.first + i, y))

    def sample(self, inputs=None, bank=0):
        """One pass over the program. `inputs` maps register -> value written into `bank` first."""
        self.bus = 0.0
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
    steps, _ = load(Path(__file__).resolve().parents[1] / "docs/vop3/program_0.bin")
    coef = list(struct.unpack(">512H", (Path(__file__).resolve().parents[1] / "docs/vop3/coefficients_0.bin").read_bytes()))
    it = Interp(steps, coef)
    # channel 0 of group 2: the cutoff load 08e goes to r[61]; the MAC 098 reads r[62] * r[61] -> w[3e]
    own = channel_steps(0x8C, 0)
    assert own[:3] == [0x8C, 0x8D, 0x8E] and 0x98 in own and 0x99 not in own and 0x104 in own
    assert all(fs1r_bank(st) == 4 for st in own) and fs1r_bank(0x97) == 7 and fs1r_bank(0x99) == 5 and fs1r_bank(0x12) == 0
    # w[3e] and r[61] are shared by the four channels of the group, so look at 098's own result in the trace
    it.trace = []
    it.rb[4][0x62] = 0.5
    it.sample()
    y098 = dict(it.trace)[0x98]
    assert abs(y098 - 0.5 * s16(coef[0x8E])) < 1e-9 and y098 != 0.0
    # clear the cutoff load (session 8 take 3 nop_08e / take 4 r6 class bits): the MAC's result vanishes
    it2 = Interp([(0, 0, 0, 0, 0) if i == 0x8E else s for i, s in enumerate(steps)], coef)
    it2.trace = []
    it2.rb[4][0x62] = 0.5
    it2.sample()
    assert dict(it2.trace)[0x98] == 0.0
    # session 9: op 1 with sel cleared puts a constant on the bus; the output stage 0f8 is `0451 8e80`
    f = fields(*steps[0xF8])
    assert f["op7"] == 1 and f["r7sel"] == 1 and f["rd_en"] == 1 and f["f7b"] == 0
    it3 = Interp(steps, coef)
    it3.sample()
    dc = Interp([(s[0], s[1], s[2], s[3] & ~0x400, s[4]) if i == 0xF8 else s for i, s in enumerate(steps)], coef)
    dc.sample()
    assert dc.bus - it3.bus > 0.5          # DC at full scale on top of whatever the idle program puts out
    # channel 4 (group 3, block 6) is the one session 9 ran on: its output stage is 0x174, constant 0x17c
    assert channel_steps(0x108, 0)[27] == 0x174 and channel_steps(0x108, 0)[29] == 0x17C
    # the class-2 constant is ignored (session 9 kf8): changing k on 0f8 changes nothing
    it4 = Interp(steps, [0x4000 if i == 0xF8 else k for i, k in enumerate(coef)])
    it4.sample({0x1D: 0.25}, bank=4)
    it5 = Interp(steps, coef)
    it5.sample({0x1D: 0.25}, bank=4)
    assert it4.bus == it5.bus and it5.bus != 0.0
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

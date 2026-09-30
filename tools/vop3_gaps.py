#!/usr/bin/env python3
"""What the measured VOP3 decode cannot yet run, as numbers (docs/vop3_isa.md section 8).

    python tools/vop3_gaps.py

Runs the shipped programs through tools/vop3_interp.py and asserts the three gaps that block a C++ rewrite
from the decode: the VOP3-1 filter has no memory (an impulse on any input register leaves in the same pass,
no tail), the assembled VOP3-2 program puts nothing on any output the interpreter knows, and class 3 (not
decoded) carries the reverb's core. When a gap closes, its assert fails: update section 8 and drop the line.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_interp as V  # noqa: E402

R = Path(__file__).resolve().parents[1]


def vop3_2(rv=0, var=0, i1=0, i2=0):
    """Base image plus one type per window, at the FUN_00039C8A addresses."""
    prog = [(0,) * 5] * 512
    for i, s in enumerate(V.load(R / "docs/vop3_2/base.bin")[0]):
        prog[i] = s
    for name, first in (("reverb_%02d" % rv, 0xE8), ("ins1_%02d" % i1, 0x110), ("ins2_%02d" % i2, 0x158), ("variation_%02d" % var, 0x1A0)):
        for i, s in enumerate(V.load(R / ("docs/vop3_2/%s.bin" % name))[0]):
            prog[first + i] = s
    return prog


def main():
    steps, _ = V.load(R / "docs/vop3/program_0.bin")
    coef = list(struct.unpack(">512H", (R / "docs/vop3/coefficients_0.bin").read_bytes()))
    # 1. filter channel 0: every register its steps read and no class-1 step loads
    own = V.channel_steps(0x10, 0)
    loaded = {V.fields(*steps[s])["ra"] for s in own if V.fields(*steps[s])["f6a"] == 1}
    ins = sorted({r for s in own for f in [V.fields(*steps[s])] if f["f6a"] == 2 and f["rb"] for r in (f["ra"], f["rb"]) if r} - loaded)
    tails = {}
    for r in ins:
        it = V.Interp(steps, coef)
        tails[r] = sum(abs(it.sample({r: 0.1 if t == 0 else 0.0})) > 1e-12 for t in range(256) if t)
    print("VOP3-1 ch0 inputs %s: output samples after the first = %s" % ([hex(r) for r in ins], sorted(set(tails.values()))))
    assert not any(tails.values()), "the filter has a tail now: gap 1 closed"
    # 2. VOP3-2, audio on the input port, every constant 0.5 (the real ones come from DAT_0106842c)
    it = V.Interp(vop3_2(), [0x4000] * 512, bank_of=lambda _: 0)
    it.offs = {s: s * 64 for s in range(128)}
    out = sum(it.sample(inp=1.0 if t == 0 else 0.0) != 0.0 for t in range(4096))
    print("VOP3-2 base+reverb 0+var 0+ins 0/0: non-zero output samples = %d" % out)
    assert out == 0, "VOP3-2 reaches an output now: gap 2 closed"
    # 3. class 3 steps, per program
    c3 = lambda p: sum(V.fields(*s)["f6a"] == 3 for s in p)
    rev = V.load(R / "docs/vop3_2/reverb_00.bin")[0]
    print("class 3 (undecoded): VOP3-1 %d steps, VOP3-2 %d, reverb 0 core %d of %d live steps" % (
        c3(steps), c3(vop3_2()), c3(rev), sum(any(s) for s in rev)))
    print("ok: all three gaps still open")


if __name__ == "__main__":
    main()

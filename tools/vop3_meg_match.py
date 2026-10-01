#!/usr/bin/env python3
"""Match the FS1R reverb combs (VOP3-2 base program) to the EX5 reverb combs (MEG), step by step.

    python tools/vop3_meg_match.py

The MEG is value-transparent (MAME decodes it); the VOP3 is not. What pairs them is the firmware: the FS1R's
FUN_0039C796 writes each comb's four constants to named steps (list 0x35DB6C, docs/vop3_2_params.md, "Reverb
combs"), so every VOP3 comb step gets a role from the number Yamaha puts in it, and the EX5 reverb (EX5_TG1.bin
0x2AEDEC, 152 steps) shows the same comb with explicit arithmetic. Asserts:

  1. VOP3 base has six comb units of the shape  op7 k*in / op4 / op0 s+k*(r+r) ... op5 -> DRAM write,
     and FUN_0039C796's list puts (g, x, x, a) on exactly (op5, op7, op4, op0) of each unit.
  2. The EX5 reverb has six damped combs of the shape  c*m / c*m(old) + p / (c*r + p) << 1, r = p / c*in + r ; mw.
  3. The FS1R gain table 0x36FA12 is a power-normalized comb pair: h = sqrt(1 - k^2) / sqrt(24) on every row,
     and log2(-log10 k) falls 0.0584 per row, so k is an RT60-law feedback gain on a geometric time grid.
  4. With k as the feedback and each comb's length from the firmware's delay offsets, every Hall1 comb decays
     60 dB in the documented Reverb Time (Data List) to within 4% at 48 kHz, from 0.8 to 20 s.

The conclusions this supports are in docs/vop3_meg_match.md.
"""
import math
import re
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from vop3_disasm import fields, load   # noqa: E402

FS1R = HERE.parents[1] / "FS1R_DISASM/roms/fs1r_v120_eprom_cpuview.bin"
EX5 = HERE.parents[1] / "FS1R_DISASM/roms/EX5_TG1.bin"
BASE = 0x200000


def fs1r_combs():
    steps = [fields(*s) for s in load(str(HERE.parent / "docs/vop3_2/base.bin"))[0]]
    rom = FS1R.read_bytes()
    lst = struct.unpack(">24H", rom[0x35DB6C - BASE:0x35DB6C - BASE + 48])     # FUN_0039C796's step list
    units = []
    for i in range(6):
        g, x1, x2, a = lst[4 * i:4 * i + 4]
        f = {r: steps[s] for r, s in (("g", g), ("x1", x1), ("x2", x2), ("a", a))}
        units.append(dict(steps=(x1, x2, a, g), ops=tuple(f[r]["op7"] for r in ("x1", "x2", "a", "g")),
                          state=f["a"]["rb"], state_ra=f["a"]["ra"], g_rb=f["g"]["rb"], g_mem=f["g"]["mem"]))
    return units


def ex5_combs():
    out = subprocess.run([sys.executable, str(HERE / "meg_disasm.py"), str(EX5), "0x0AEDEC", "152"],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    units = []
    for i, line in enumerate(out):
        m = re.search(r"\(c\w+ \* (r\w+) \+ p\) << 1 ;.* \1 = p ; nodither", line)
        if not m:
            continue
        r = m.group(1)
        fir = [re.search(r"c\w+ \* (m\w+)", out[k]) for k in (i - 2, i - 1)]
        wr = next((k for k in range(i, min(i + 16, len(out))) if re.search(r"\* (m\w+) \+ %s ; mw = p" % r, out[k])), None)
        if all(fir) and wr is not None:
            units.append(dict(pc=int(line[:3], 16), state=r, fir=[f.group(1) for f in fir],
                              inp=re.search(r"\* (m\w+) \+", out[wr]).group(1), write_pc=int(out[wr][:3], 16)))
    return units


def gain_table():
    rom = FS1R.read_bytes()
    rows = [struct.unpack(">HH", rom[0x36FA12 - BASE + 4 * j:0x36FA16 - BASE + 4 * j]) for j in range(256)]
    norm = [h / 32768 / math.sqrt(1 - (k / 32768) ** 2) for k, h in rows if 0 < k < 32768]
    lg = [math.log2(-math.log10(k / 32768)) for k, _ in rows if 0 < k < 32768]
    slope = (lg[208] - lg[16]) / 192
    return min(norm), max(norm), slope


def comb_rt60(rtype=1, times=(5, 17, 30, 47, 57, 67)):
    """Each comb as y[t] = x[t] + k * y[t - L] at 48 kHz, k from 0x36FA12 (FUN_0039C796's index) and L from the
    delay offsets the firmware writes for the type (docs/vop3_2/params.json: read slot - write slot). Returns
    (documented time, measured RT60 per comb) for each time index (float table 0x36F8B0)."""
    import json
    rom = FS1R.read_bytes()
    f = lambda a, fmt: struct.unpack(fmt, rom[a - BASE:a - BASE + struct.calcsize(fmt)])[0]
    e = json.loads((HERE.parent / "docs/vop3_2/params.json").read_text())["reverb"][str(rtype)]
    of = {int(k, 16): v for k, v in e["offs"].items()}
    lens = [of[r] - of[w] for w, r in zip((0x2A, 0x2B, 0x2C, 0x2D, 0x34, 0x35), (0x22, 0x24, 0x25, 0x27, 0x28, 0x32))]
    sel = f(0x36F826 + rtype * 6, ">h")
    out = []
    for ti in times:
        T = f(0x36F8B0 + 4 * ti, ">f")
        rts = []
        for i, n in enumerate(lens):
            j = max(0, min(255, rom[0x36F9CC - BASE + ti] + rom[0x36FE1D - BASE + sel * 6 + i] - 0x38))
            k = f(0x36FA12 + 4 * j, ">H") / 32768
            rts.append(-3 * n / math.log10(k) / 48000)          # time for k^(t/n) to fall 60 dB
        out.append((T, rts))
    return out


def main():
    fs = fs1r_combs()
    for u in fs:
        print("VOP3 comb  x %03x %03x  a %03x  g %03x   ops %s   state r[%02x]  write rB r[%02x]" % (*u["steps"], u["ops"], u["state"], u["g_rb"]))
        assert u["ops"] == (7, 4, 0, 5), u                       # k*in, (op4), s + k*(r+r), op5
        assert u["state"] == u["state_ra"] == u["g_rb"]          # the pole's r+r and the write step read one register
        assert u["g_mem"] == 1                                   # the gain step is the DRAM write
    assert len({u["state"] for u in fs}) == 6
    ex = ex5_combs()
    for u in ex:
        print("MEG  comb  %03x  fir %s  state %s  write %03x = c*%s + %s" % (u["pc"], u["fir"], u["state"], u["write_pc"], u["inp"], u["state"]))
    assert len(ex) == 6, len(ex)
    assert len({u["inp"] for u in ex}) == 1                      # every comb writes c*input + its own state
    lo, hi, slope = gain_table()
    print("0x36FA12: h / sqrt(1-k^2) in [%.4f, %.4f] (1/sqrt(24) = %.4f); log2(-log10 k) slope %.4f per row" % (lo, hi, 24 ** -0.5, slope))
    assert 0.199 < lo and hi < 0.21 and abs(slope + 0.0584) < 0.001
    for T, rts in comb_rt60():
        print("Hall1 comb RT60 at 48 kHz for time %.1f s: %s" % (T, " ".join("%.2f" % r for r in rts)))
        assert all(abs(r / T - 1) < 0.04 for r in rts), (T, rts)    # the documented time, every comb, 0.8..20 s
    print("ok: six combs on both chips, roles pinned by the FS1R's own constants, decay at the documented time")


if __name__ == "__main__":
    main()

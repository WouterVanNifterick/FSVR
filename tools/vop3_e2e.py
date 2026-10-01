#!/usr/bin/env python3
"""End-to-end check against the unit: session 23's recorded effect input replayed through the shipped VOP3-2
program with the dumped constants and offsets; the model's r[0a] against the recorded right output.

    python tools/vop3_e2e.py [config ...]      (default: every config of the take)

Take: FS1R.unlock captures/2026-10-01-1817-s23 (L = the effect input register, R = the program's right output,
DAC = r / 8). Prints, per config, the model's and the unit's RMS and their correlation at the best lag.
"""
import json
import struct
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_interp as V  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ROM = (ROOT / "FS1R_DISASM/roms/fs1r_v120_eprom_cpuview.bin").read_bytes()
TAKE = ROOT / "FS1R.unlock/captures/2026-10-01-1817-s23"
WIN = [(0x3658D4, 0, 0xE8, 0, None), (0x3663B4, 400, 0x28, 0xE8, 0), (0x366A94, 0x3C0, 0x60, 0x1A0, 1),
       (0x36A874, 0x2D0, 0x48, 0x110, 2), (0x36CA34, 0x2D0, 0x48, 0x158, 3)]
INREG = {"rev": (0x03, 0x04), "var": (0x05, 0x06)}


def program(sel):
    p = [(0,) * 5] * 512
    for a, stride, n, first, i in WIN:
        t = 0 if i is None else sel[i]
        for k in range(n):
            p[first + k] = struct.unpack_from(">5H", ROM, a - 0x200000 + stride * t + k * 10)
    return p


def run(name, n=3000):
    x, sr = sf.read(str(TAKE / "session23.flac"), dtype="float64")
    rows = json.loads((TAKE / "probe_segments.json").read_text())
    st = float((TAKE / "rec_start.txt").read_text())
    cfg = json.loads((TAKE / "session.json").read_text())["results"]["probe"]["loads"][name]
    r = next(r for r in rows if r["name"] == name + "_noise")
    a = int((r["t"] - st + 0.4) * sr)
    seg = x[a:a + n]
    prog = program(cfg["selectors"])
    regs = INREG[name[:3]]
    # The take read the input register at step 10f, so the recorded value is what r[reg] holds from its last
    # writer before 10f until 10f. Inject it right after that writer (pass start when there is none).
    F = [V.fields(*w) for w in prog]
    at = max([i + 1 for i in range(0x10F) if F[i]["f6a"] in (1, 2, 3) and F[i]["ra"] in regs], default=0)

    class Probe(V.Interp):
        def run_step(self, i):
            if i == at:
                for reg in regs:
                    self.rb[0][reg] = self.v
            super().run_step(i)
    it = Probe(prog, cfg["coefs"], bank_of=lambda _: 0)
    it.offs = dict(enumerate(cfg["offsets"]))
    out = []
    for v in seg[:, 0] * 8:
        it.v = v
        it.sample()
        out.append(it.rb[0][0x0A] / 8)
    out, hw = np.array(out), seg[:, 1]
    db = lambda v: 10 * np.log10((v ** 2).mean() + 1e-30)
    best = max(((np.corrcoef(np.roll(out, k)[200:], hw[200:])[0, 1] if out[200:].std() else 0.0), k) for k in range(-4, 5))
    return db(out[200:]), db(hw[200:]), best


if __name__ == "__main__":
    names = sys.argv[1:] or ["rev0", "rev1", "rev9", "rev13", "rev16", "var1", "var9"]
    for nm in names:
        m, h, (c, k) = run(nm)
        print(f"{nm:6s} model {m:7.1f} dB  unit {h:7.1f} dB  corr {c:+.3f} at lag {k:+d}")

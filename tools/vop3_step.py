#!/usr/bin/env python3
"""Step-response check against the unit (session 28 `p28a`): the shipped program with the base input steps
000-007 reading r53, r53 stepped 0 -> 1/16 by a kept coefficient write; the model's DAC (d[10], d[11] / 4)
against the unit's first N samples after the onset.

    python tools/vop3_step.py <dir with session.json, step_<cfg>_B.npy> [N]
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_e2e as E  # noqa: E402
import vop3_interp as V  # noqa: E402


def patched(load):
    prog, coef = E.program(load["selectors"]), list(load["coefs"])
    for i in range(8):
        w = list(prog[i])
        w[4] = (w[4] & ~(0x7F << 7)) | 0x53 << 7
        prog[i], coef[i] = tuple(w), 0x7FFF
    prog[0x1F0], prog[0x1F1] = (0, 0, 0, 0x00C0, 0x8000), (0, 0x53, 0, 0x8000, 0x4000 | 0x76 << 7)
    coef[0x1F0] = coef[0x1F1] = 0
    return prog, coef


def model(load, n, settle=200):
    prog, coef = patched(load)
    it = V.Interp(prog, coef, bank_of=lambda _: 0)
    it.offs = dict(enumerate(load["offsets"]))
    for _ in range(settle):
        it.sample()
        it.dac()
    it.coef[0x1F1] = 0x10
    out = []
    for _ in range(n):
        it.sample()
        out.append(it.dac())
    return np.clip(np.array(out), -8, 8)


def onset(a):
    return int(np.argmax(np.abs(a).max(1) > 1e-5))


if __name__ == "__main__":
    d = Path(sys.argv[1])
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    loads = json.loads((d / "session.json").read_text())["results"]["probe"]["loads"]
    for cfg in ("none", "rev1", "var1", "rev9"):
        f = d / f"step_{cfg}_B.npy"
        if not f.exists():
            continue
        hw = np.load(f).astype(float)
        hw = hw[onset(hw):][:n]                          # readout units (flac x 8)
        m = model(loads[cfg], n)
        m = m[onset(m):][:len(hw)]                      # both aligned on the first channel to move (R)
        k = min(len(hw), len(m))
        err = np.abs(hw[:k] - m[:k]).max(0)
        print(f"{cfg:5s} unit end {hw[k-1]}  model end {m[k-1]}  max |err| L {err[0]:.2e} R {err[1]:.2e}")

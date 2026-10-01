#!/usr/bin/env python3
"""Dynamic probes on the shipped program (s25 runner, `keep` patches): the model replays the runner's
sequence and its DAC output after each change is compared, sample by sample, with the unit's.

    python tools/vop3_seq.py <probes.json> <dir with session.json and step_<name>.npy> [N]

step_<name>.npy (from the rig's step_read.py): the unit's L/R in readout units from 32 samples before
the first change in the segment. Both are aligned on their first change.
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_e2e as E  # noqa: E402
import vop3_interp as V  # noqa: E402


def onset(a, ref=None):
    ref = a[0] if ref is None else ref
    d = np.abs(a - ref).max(1) > 1e-5
    return int(np.argmax(d)) if d.any() else -1


def simulate(load, probes, n, settle=300):
    prog, coef = E.program(load["selectors"]), list(load["coefs"])
    it = V.Interp(list(prog), list(coef), bank_of=lambda _: 0)
    it.offs = dict(enumerate(load["offsets"]))
    for _ in range(settle):
        it.sample()
        it.dac()
    out = {}
    for p in probes:
        for k, v in p["coefs"].items():
            it.coef[int(k, 16)] = v
        for k, w in p["steps"].items():
            it.steps[int(k, 16)] = tuple(w)
        y = []
        for _ in range(max(n, settle)):
            it.sample()
            y.append(it.dac())
        out[p["name"]] = np.array(y)
        if not p.get("keep"):
            it.steps, it.coef = list(prog), list(coef)
            for _ in range(settle):
                it.sample()
                it.dac()
    return out


def main():
    spec = json.loads(Path(sys.argv[1]).read_text())
    d = Path(sys.argv[2])
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 3000
    loads = json.loads((d / "session.json").read_text())["results"]["probe"]["loads"]
    for cname in spec["configs"]:
        ps = [p for p in spec["probes"] if p["config"] == cname]
        sim = simulate(loads[cname], ps, n + 64)
        for p in ps:
            f = d / f"step_{p['name']}.npy"
            if not f.exists():
                continue
            hw = np.load(f).astype(float)
            m = sim[p["name"]]
            # the model segment starts at the change; the unit's at 32 samples before it
            a, b = onset(hw), onset(m, m[0] if onset(m) else None)
            hw, m = hw[max(a, 0):][:n], m[max(b, 0):][:n]
            k = min(len(hw), len(m))
            e = np.abs(hw[:k] - m[:k]).max(1)
            bad = np.nonzero(e > 2e-5)[0]
            print(f"{p['name']:14s} n {k}  max err {e.max():.2e}  first bad {bad[0] if len(bad) else '-'}"
                  f"  unit rms {np.sqrt((hw[:k] ** 2).mean()):.2e} model rms {np.sqrt((m[:k] ** 2).mean()):.2e}")


if __name__ == "__main__":
    main()

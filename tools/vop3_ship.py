#!/usr/bin/env python3
"""Shipped-program probes (FS1R.unlock session 25 runner) against the interpreter: L and R both.

    python tools/vop3_ship.py <probes.json> <readout.txt> <session.json> [passes]

The readout gives L/R means as register values / 8 (the DAC); the model's r[09] / r[0a] after `passes`
passes of the patched program, clipped at +-8.
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_e2e as E  # noqa: E402
import vop3_interp as V  # noqa: E402


def predicts(load, probes, passes=300, gap=100):
    """The runner's sequence: shipped program, then per probe patch -> hold -> restore -> gap, state carried
    (s27: a register a patch stops writing keeps what the previous probe left there)."""
    prog, coef = E.program(load["selectors"]), list(load["coefs"])
    it = V.Interp(list(prog), list(coef), bank_of=lambda _: 0)
    it.offs = dict(enumerate(load["offsets"]))
    c = lambda v: max(-8.0, min(8.0, v))
    for _ in range(passes):
        it.sample()
    out = []
    for p in probes:
        for k, v in p["coefs"].items():
            it.coef[int(k, 16)] = v
        for k, w in p["steps"].items():
            it.steps[int(k, 16)] = tuple(w)
        for _ in range(passes):
            it.sample()
        out.append(tuple(c(it.d[n] / 4) for n in V.DAC))
        it.steps, it.coef = list(prog), list(coef)
        for _ in range(gap):
            it.sample()
    return out


def compare(spec, readout, sess, passes=24):
    pat = r"(\S+)\s+L mean ([+-][\d.]+) sd \S+\s+R mean ([+-][\d.]+)"
    meas = {m.group(1): (float(m.group(2)), float(m.group(3))) for m in re.finditer(pat, readout)}
    loads = sess["results"]["probe"]["loads"]
    rows, pred = [], {}
    for cname in spec.get("configs", {}):
        ps = [p for p in spec["probes"] if p["config"] == cname]
        pred.update(zip((p["name"] for p in ps), predicts(loads[cname], ps)))
    for p in spec["probes"]:
        m, q = meas.get(p["name"]), pred[p["name"]]
        ok = m is not None and all(abs(a - b) < 2e-3 for a, b in zip(m, q))
        rows.append((p["name"], m, q, ok))
    return rows


if __name__ == "__main__":
    spec = json.loads(Path(sys.argv[1]).read_text())
    rows = compare(spec, Path(sys.argv[2]).read_text(), json.loads(Path(sys.argv[3]).read_text()),
                   int(sys.argv[4]) if len(sys.argv) > 4 else 24)
    for n, m, q, ok in rows:
        print(f"{n:22s} unit {m}  model ({q[0]:+.5f}, {q[1]:+.5f}){'' if ok else '   <--'}")
    print(sum(r[3] for r in rows), "/", len(rows), "match")

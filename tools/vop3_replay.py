#!/usr/bin/env python3
"""End-to-end acceptance replay: the shipped program fed the unit's recorded effect input, against the unit's
recorded output, sample by sample, through the C++ core.

    python tools/vop3_replay.py [take dir] [N]

Take (s35j, FS1R.unlock `2026-10-02-070151-s25`, s25 runner, voice noise): per config, `<cfg>_ia` re-points the
left DAC step 0cf at input register a (L = r[a] x 16 / 4 readout), R stays the program's right output. Inputs
(s35h): the reverb send lands in r[0d] / r[0e], the variation send in r[0f] / r[10]; the program reads them in
place at steps 002-005 (no step writes them). The two reverb inputs carry the same mono signal, r[0e] two DAC
samples ahead of r[0d] in the `_ab` take, so a reverb replay drives both from the one recorded register; the
variation pair is stereo (uncorrelated), so the variation replay is only a level check until both are recorded
in one segment.
"""
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_board as B  # noqa: E402
import vop3_cpp as C  # noqa: E402
import vop3_e2e as E  # noqa: E402

INREG = {"rev": (0x0D, 0x0E), "var": (0x0F, 0x10)}   # s35h


def segment(d, name, pad=0.3):
    x, sr = sf.read(str(next(d.glob("*.flac"))), dtype="float64")
    rows = {r["name"]: r for r in json.loads((d / "probe_segments.json").read_text())}
    st = float((d / "rec_start.txt").read_text())
    r = rows[name]
    a = int((r["t"] - st + pad) * sr)
    return x[a:a + int((r["hold"] - 2 * pad) * sr)] * 8            # readout units


def replay(d, cfg, n, sa=1, sb=2):
    """r[a] at pass p = L[p + sa] (L leaves the DAC a pass late, s28); r[b] = r[a] one pass later (_ab: L[n + 2] = R[n])."""
    spec = json.loads((d / "probes.json").read_text())
    load = json.loads((d / "session.json").read_text())["results"]["probe"]["loads"][cfg]
    io = segment(d, cfg + "_ia")[:n + 8]
    a, b = INREG[spec["configs"][cfg]["slot"]]
    L = io[:, 0]                                                      # 0cf k = 0x1fff, route 3 x16, readout d / 4: readout = r (level matches the unit to 0.6 dB)
    cmds = []
    for p in range(n):
        cmds += [f"G {a} {float(L[p + sa])!r}", f"G {b} {float(L[p + sb])!r}", "R 1"]   # float(): numpy 2 repr is np.float64(..)
    return io[:n], C.run(E.program(load["selectors"]), list(load["coefs"]), load["offsets"], cmds)


def score(hw, m, skip):
    e = np.abs(hw[skip:] - m[skip:])
    bad = np.nonzero(e > 1.5 / 16384)[0]
    c = np.corrcoef(hw[skip:], m[skip:])[0, 1] if m[skip:].std() else 0.0
    return (bad[0] + skip if len(bad) else len(hw)), e.max() * 16384, c


if __name__ == "__main__":
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else B.CAP / "2026-10-02-070151-s25"
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 24000
    spec = json.loads((d / "probes.json").read_text())
    for cfg in spec["configs"]:
        io, m = replay(d, cfg, n)
        fb, mx, c = score(io[:, 1], m[:, 1], 2000)
        db = lambda v: 10 * np.log10((v[2000:] ** 2).mean() + 1e-30)
        print(f"{cfg:6s} unit R {db(io[:, 1]):6.1f} dB  model R {db(m[:, 1]):6.1f} dB  corr {c:+.5f}  "
              f"first > 1 LSB after warm-up: {fb} of {n}, max {mx:.0f} LSB")

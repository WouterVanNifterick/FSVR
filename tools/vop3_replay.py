#!/usr/bin/env python3
"""End-to-end acceptance replay: the shipped program fed the unit's recorded effect input, against the unit's
recorded output, sample by sample, through the C++ core.

    python tools/vop3_replay.py                 # self-check on the s36d click take
    python tools/vop3_replay.py <take dir> [N]  # noise take: correlation and level per config

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


K = 0x5A82 / 32768        # s36: steps 002/003 scale r[0d]/r[0e] in place (op 3, rA = rB); the tap at 0cf reads after


def replay(d, cfg, n, sa=0, sb=2, tap="_ia"):
    """r[a] at pass p = tapped L[p + sa] / K; r[b] = the same signal two passes later (mono send, s36 click takes:
    first-difference 4324 against 1863 for the other shifts tried)."""
    spec = json.loads((d / "probes.json").read_text())
    load = json.loads((d / "session.json").read_text())["results"]["probe"]["loads"][cfg]
    io = segment(d, cfg + tap)[:n + 8]
    a, b = INREG[spec["configs"][cfg]["slot"]]
    L = np.concatenate([np.zeros(4), io[:, 0] / K])
    cmds = []
    for p in range(n):
        cmds += [f"G {a} {float(L[p + 4 + sa])!r}", f"G {b} {float(L[p + 4 + sb])!r}", "R 1"]
    return io[:n], C.run(E.program(load["selectors"]), list(load["coefs"]), load["offsets"], cmds)


def score(hw, m, skip):
    e = np.abs(hw[skip:] - m[skip:])
    bad = np.nonzero(e > 1.5 / 16384)[0]
    c = np.corrcoef(hw[skip:], m[skip:])[0, 1] if m[skip:].std() else 0.0
    return (bad[0] + skip if len(bad) else len(hw)), e.max() * 16384, c


CLICK = B.CAP / "2026-10-02-082621-s25"   # s36d: one click per segment, re-triggered after each patch


def click(cfg, n=20000):
    """The s36d click take: the segment cut at the click (the logged time is after the note-on)."""
    x, sr = sf.read(str(next(CLICK.glob("*.flac"))), dtype="float64")
    rows = {r["name"]: r for r in json.loads((CLICK / "probe_segments.json").read_text())}
    st = float((CLICK / "rec_start.txt").read_text())
    a = int((rows[cfg + "_ia1"]["t"] - st - 0.8) * sr)
    s = x[a:a + int(1.4 * sr)] * 8
    on = int(np.argmax(np.abs(s[:, 0]) > 0.05))
    return s[on - 400:on - 400 + n + 8]


def check():
    """Hall1 / Hall9 fed the recorded click: exact to the reverb's first echoes, then correlated. Fails when a rule
    change breaks the input path or the early reverb (the numbers the current model gives; raise them as it improves)."""
    load = json.loads((CLICK / "session.json").read_text())["results"]["probe"]["loads"]
    for cfg, exact, corr in (("rev1c", 1880, 0.975), ("rev9c", 2200, 0.965)):
        s = click(cfg)
        L = np.concatenate([np.zeros(4), s[:, 0] / K])
        cmds = [c for p in range(20000) for c in (f"G 13 {float(L[p + 4])!r}", f"G 14 {float(L[p + 6])!r}", "R 1")]
        m = C.run(E.program(load[cfg]["selectors"]), list(load[cfg]["coefs"]), load[cfg]["offsets"], cmds)[:, 1]
        hw = s[:20000, 1]
        fb = int(np.argmax(np.abs(hw - m) > 1.5 / 16384))
        c = np.corrcoef(hw[2000:], m[2000:])[0, 1]
        print(f"{cfg}: exact to sample {fb}, corr {c:.4f}, level {20 * np.log10(m[2000:].std() / hw[2000:].std()):+.2f} dB")
        assert fb >= exact and c >= corr, (cfg, fb, c)
    print("ok")


if __name__ == "__main__" and len(sys.argv) == 1:
    check()
elif __name__ == "__main__":
    d = Path(sys.argv[1])
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 24000
    spec = json.loads((d / "probes.json").read_text())
    for cfg in spec["configs"]:
        io, m = replay(d, cfg, n)
        fb, mx, c = score(io[:, 1], m[:, 1], 2000)
        db = lambda v: 10 * np.log10((v[2000:] ** 2).mean() + 1e-30)
        print(f"{cfg:6s} unit R {db(io[:, 1]):6.1f} dB  model R {db(m[:, 1]):6.1f} dB  corr {c:+.5f}  "
              f"first > 1 LSB after warm-up: {fb} of {n}, max {mx:.0f} LSB")

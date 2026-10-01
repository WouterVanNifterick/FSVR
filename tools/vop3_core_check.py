#!/usr/bin/env python3
"""C++ VOP3 core (src/fs1r/chips/vop3_core.h) against the Python reference (tools/vop3_interp.py).

    python tools/vop3_core_check.py [N]

Builds a 20-line driver, runs the session-28 step-response setup (shipped Hall1 / Hall9 / variation 1, inputs
re-pointed at r53, stepped 0 -> 1/16) through both, and requires the DAC output to be bit-identical for N passes
(default 3000). Then compares the C++ against the unit's take when it is on disk (FS1R.unlock s25 folder).
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_step as S  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
TAKE = ROOT.parent / "FS1R.unlock/captures/2026-10-01-215125-s25"
DRIVER = r"""
#include "fs1r/chips/vop3_core.h"
#include <cstdio>
int main(int, char** a) {
    static Vop3 v; int settle, n, k, kv;
    std::scanf("%d %d %d %d", &settle, &n, &k, &kv);
    for (auto& s : v.prog) for (auto& w : s.w) std::scanf("%hu", &w);
    for (auto& c : v.coef) std::scanf("%hu", &c);
    for (int& o : v.offs) std::scanf("%d", &o);
    double L, R;
    for (int i = 0; i < settle; i++) { v.pass(); v.dac(L, R); }
    v.coef[k] = kv;
    for (int i = 0; i < n; i++) { v.pass(); v.dac(L, R); std::printf("%.17g %.17g\n", L, R); }
}
"""


def build():
    d = Path(tempfile.mkdtemp())
    (d / "drv.cpp").write_text(DRIVER)
    exe = d / "drv"
    subprocess.run(["g++", "-O2", "-std=c++17", "-I", str(ROOT / "src"), str(d / "drv.cpp"), "-o", str(exe)], check=True)
    return exe


def cpp(exe, load, n, settle=200):
    prog, coef = S.patched(load)
    offs = (list(load["offsets"]) + [0] * 64)[:64]
    words = [settle, n, 0x1F1, 0x10] + [w for s in prog for w in s] + list(coef) + offs
    out = subprocess.run([str(exe)], input=" ".join(map(str, words)), capture_output=True, text=True, check=True).stdout
    return np.array([list(map(float, l.split())) for l in out.splitlines()])


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    loads = json.loads((TAKE / "session.json").read_text())["results"]["probe"]["loads"]
    exe = build()
    for cfg in ("rev1", "rev9", "var1"):
        c, p = cpp(exe, loads[cfg], n), S.model(loads[cfg], n)
        assert np.array_equal(c, p), (cfg, int(np.argmax(np.any(c != p, 1))))
        msg = f"{cfg}: C++ == Python over {n} passes"
        f = TAKE / f"step_{cfg}_B.npy"
        if f.exists():
            hw = np.load(f).astype(float)
            hw, m = hw[S.onset(hw):], c[S.onset(c):]
            k = min(len(hw), len(m))
            bad = np.nonzero(np.abs(hw[:k] - m[:k]).max(1) > 2e-5)[0]
            msg += f"; unit exact to sample {bad[0] if len(bad) else k} of {k}"
        print(msg)
    print("ok")

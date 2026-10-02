#!/usr/bin/env python3
"""Run the C++ VOP3 core (src/fs1r/chips/vop3_core.h) from Python: the fast path for probe replays.

    import vop3_cpp as C
    out = C.seq(load, probes, n)     # same semantics as vop3_seq.simulate, ~50x faster

`load` = a session.json probe load (selectors, coefs, offsets); probes as in the s25 runner (steps, coefs, offs,
keep). Returns {probe name: (n, 2) array of DAC (L, R) in readout units}. `defs` passes -D flags, so a rule
variant behind an #ifdef in vop3_core.h can be swept without editing the file.
"""
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_e2e as E  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DRIVER = r"""
#include "fs1r/chips/vop3_core.h"
#include <cstdio>
#include <cstring>
// stdin: 512x5 words, 512 coefs, 128 offsets, then commands: S st w0..w4 | K st v | O slot off | R n (print n DAC
// samples) | Q n (run n, no output) | B (restore the base program, coefs and offsets) | G reg value | A reg value | T (one pass; print all 512 step results) | H at ra va rb vb
int main() {
    static Vop3 v; static Vop3::Step p0[512]; static uint16_t c0[512]; static int o0[128];
    for (auto& s : p0) for (auto& w : s.w) std::scanf("%hu", &w);
    for (auto& c : c0) std::scanf("%hu", &c);
    for (int& o : o0) std::scanf("%d", &o);
    auto base = [&] { for (int i = 0; i < 512; i++) { v.prog[i] = p0[i]; v.coef[i] = c0[i]; }
                      std::memcpy(v.offs, o0, sizeof o0); };
    base();
    char cmd[4]; double L, R;
    while (std::scanf("%3s", cmd) == 1) {
        int a, b;
        if (*cmd == 'S') { std::scanf("%d", &a); for (auto& w : v.prog[a].w) std::scanf("%hu", &w); }
        else if (*cmd == 'K') { std::scanf("%d %d", &a, &b); v.coef[a] = b; }
        else if (*cmd == 'O') { std::scanf("%d %d", &a, &b); v.offs[a] = b; }
        else if (*cmd == 'B') base();
        else if (*cmd == 'G') { double x; std::scanf("%d %lf", &a, &x); v.r[a] = x; }   // set a register (an input)
        else if (*cmd == 'A') { double x; std::scanf("%d %lf", &a, &x); v.r[a] += x; }   // add into a register (a bus input that sums)
        else if (*cmd == 'H') {                  // H at ra va rb vb: one pass, inputs written just before step `at`
            int at, ra, rb; double va, vb; std::scanf("%d %d %lf %d %lf", &at, &ra, &va, &rb, &vb);
            v.inp = 0; v.d[1] = v.d[5] = 0;
            for (int st = 0; st < 512; st++) { if (st == at) { v.r[ra] = va; v.r[rb] = vb; } v.step(st); }
            v.hist.clear(); v.ptr = (v.ptr - 1) & (Vop3::MEMSZ - 1);
            v.dac(L, R); std::printf("%.17g %.17g\n", L, R);
        }
        else if (*cmd == 'T') {                  // T: one pass, print every step's result
            v.pass(); for (double y : v.trace) std::printf("%.17g ", y); std::printf("\n");
        }
        else if (*cmd == 'R' || *cmd == 'Q') {
            std::scanf("%d", &a);
            for (int i = 0; i < a; i++) { v.pass(); v.dac(L, R); if (*cmd == 'R') std::printf("%.17g %.17g\n", L, R); }
        }
    }
}
"""


def build(defs=()):
    src = DRIVER + (ROOT / "src/fs1r/chips/vop3_core.h").read_text() + " ".join(defs)
    exe = Path(tempfile.gettempdir()) / ("vop3drv_" + hashlib.sha1(src.encode()).hexdigest()[:12])
    if not exe.exists():
        cpp = exe.with_suffix(".cpp")
        cpp.write_text(DRIVER)
        subprocess.run(["g++", "-O2", "-std=c++17", *("-D" + d for d in defs), "-I", str(ROOT / "src"), str(cpp),
                        "-o", str(exe)], check=True)
    return exe


def header(prog, coef, offs):
    offs = (list(offs) + [0] * 128)[:128]
    return [w for s in prog for w in s] + list(coef) + offs


def run(prog, coef, offs, cmds, defs=()):
    txt = " ".join(map(str, header(prog, coef, offs))) + "\n" + "\n".join(cmds)
    out = subprocess.run([str(build(defs))], input=txt, capture_output=True, text=True, check=True).stdout
    rows = [list(map(float, l.split())) for l in out.splitlines()]
    return np.array(rows) if rows and len(rows[0]) != 2 else np.array(rows).reshape(-1, 2)


def seq(load, probes, n, settle=300, defs=(), prog=None, coef=None, hold=57600):
    """The s25 runner's sequence: settle, then per probe patch -> n passes recorded -> (unless keep) restore and
    settle. A non-kept probe restores to the base program (the runner restores the shipped words). `hold` passes
    per probe: the runner's --hold 1.0 + --gap 0.2 at 48 kHz, so a kept probe's tail decays as on the unit."""
    prog = prog or E.program(load["selectors"])
    coef = coef or list(load["coefs"])
    cmds, names = [f"Q {settle}"], []
    for p in probes:
        cmds += [f"K {int(k, 16)} {v}" for k, v in p.get("coefs", {}).items()]
        cmds += [f"S {int(k, 16)} " + " ".join(map(str, w)) for k, w in p.get("steps", {}).items()]
        cmds += [f"O {int(k)} {v}" for k, v in p.get("offs", {}).items()]
        cmds.append(f"R {n}")
        if hold > n:
            cmds.append(f"Q {hold - n}")
        names.append(p["name"])
        if not p.get("keep"):
            cmds += ["B", f"Q {settle}"]
    a = run(prog, coef, load["offsets"], cmds, defs)
    return {nm: a[i * n:(i + 1) * n] for i, nm in enumerate(names)}

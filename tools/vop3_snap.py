#!/usr/bin/env python3
"""Snapshot probes on the shipped VOP3-2 program (FS1R.unlock session 25), and the interpreter's prediction.

    python tools/vop3_snap.py write <config> <first> <last> probes.json   step N+1 := r[09] = r[rA of N]
    python tools/vop3_snap.py compare <readout.txt> <session.json> probes.json

Nothing after the base program's 0e7 writes r[09], so the left DAC (x8) is r[rA of N] as step N left it, in the
same pass. Registers read 0.99997 x (op 2, k 0x7fff).
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vop3_e2e as E  # noqa: E402
import vop3_interp as V  # noqa: E402

CONFIGS = {"rev1q": dict(slot="rev", type=1, voice="off"), "var1q": dict(slot="var", type=1, voice="off"),
           "rev9q": dict(slot="rev", type=9, voice="off"), "ins0q": dict(slot="ins", type=0, voice="off")}
READ = lambda reg: [0, 0x09, 0, 0x0080, 0x8000 | reg << 7]


def probes(prog, first, last, cname):
    out = []
    for n in range(first, last):
        f = V.fields(*prog[n])
        if f["f6a"] in (1, 2, 3) and f["ra"] and n + 1 < 512:
            out.append(dict(config=cname, name=f"s{n:03x}_r{f['ra']:02x}", steps={"%03x" % (n + 1): READ(f["ra"])},
                            coefs={"%03x" % (n + 1): 0x7FFF}, detail=f"after {n:03x}: r[{f['ra']:02x}]"))
    return out


def predict(load, p, passes=8):
    prog = E.program(load["selectors"])
    coef = list(load["coefs"])
    for k, w in p["steps"].items():
        prog[int(k, 16)] = tuple(w)
    for k, v in p["coefs"].items():
        coef[int(k, 16)] = v
    it = V.Interp(prog, coef, bank_of=lambda _: 0)
    it.offs = dict(enumerate(load["offsets"]))
    for _ in range(passes):
        it.sample()
    return max(-8.0, min(8.0, it.rb[0][0x09]))     # the DAC clips at +-8


def main():
    if sys.argv[1] == "write":
        cname, first, last, path = sys.argv[2], int(sys.argv[3], 16), int(sys.argv[4], 16), sys.argv[5]
        # the program for the config is assembled on the rig; here the selectors are known from s23's dumps
        sel = {"rev1q": [0, 0, 0, 0], "var1q": [0, 0, 0, 0], "rev9q": [0, 0, 0, 0], "ins0q": [0, 0, 0, 0]}[cname]
        P = probes(E.program(sel), first, last, cname)
        Path(path).write_text(json.dumps(dict(configs={cname: CONFIGS[cname]}, probes=P)))
        print(len(P), "probes")
        return
    readout, sess, path = sys.argv[2:5]
    meas = {m.group(1): float(m.group(2)) for m in re.finditer(r"(\S+)\s+L mean ([+-][\d.]+)", Path(readout).read_text())}
    loads = json.loads(Path(sess).read_text())["results"]["probe"]["loads"]
    spec = json.loads(Path(path).read_text())
    for p in spec["probes"]:
        m, q = meas.get(p["name"]), predict(loads[p["config"]], p)
        flag = "" if m is not None and abs(m - q) < 2e-3 else "   <--"
        print(f"{p['name']:12s} unit {m!s:>10s}  model {q:+.5f}{flag}")


if __name__ == "__main__":
    main()

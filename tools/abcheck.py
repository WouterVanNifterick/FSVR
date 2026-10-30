#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Wouter van Nifterick
"""A/B check: the regression renders and the Fat Line demo as 16-bit audio, hashed, and compared sample
for sample with a baseline, so an optimisation commit can say which tier it is.

    python tools/abcheck.py --save build/ab/mine                 render, print the hashes, keep the WAVs
    python tools/abcheck.py --ref build/ab/base [--tier A|B]      compare with a kept set; the tier is the gate
    python tools/abcheck.py --build-base HEAD                     build that revision apart and render its set
    python tools/abcheck.py --exe PATH ...                        render with this console or render_capture
    python tools/abcheck.py --write-hashes tools/abcheck_ref.json / --ref-hashes FILE   hashes only, for CI

Tier A: every sample of every case the same. Tier B: no sample moved more than one 16-bit step. Anything
else fails. The cases are tools/regress.py's nineteen plus the demo; the ROM cases come from the EPROM
when one is found (--rom, $FSVR_ROM, regress.py's default) and from the factory banks in plugin/generated
otherwise (--factory forces that). --build-base makes a detached worktree under build/ab and builds there,
so the main tree's build/ and bin/ are never touched (AGENTS.md).
"""
import argparse
import hashlib
import json
import os
import platform
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fs1r_render import renderer
from regress import CASES, DEFAULT_ROM

ROOT = Path(__file__).resolve().parents[1]
AB = ROOT / "build" / "ab"
DEMO = ("demo-fatline", "captures/demo/04_Fat Line.mid", 2.0)   # the song and a tail


def read_s16(path):
    """The data chunk as int16 frames, (n, 2)."""
    d = path.read_bytes()
    assert d[:4] == b"RIFF", path
    i = 12
    while i + 8 <= len(d):
        cid, n = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        if cid == b"data":
            return np.frombuffer(d[i + 8:i + 8 + n], dtype="<i2").reshape(-1, 2)
        i += 8 + n + (n & 1)
    raise SystemExit(f"no data chunk in {path}")


def digest(path):
    return hashlib.sha256(read_s16(path).tobytes()).hexdigest()[:16]


def render_all(exe, out, rom, factory):
    """Every case into out/, returning {name: wav path}. A case that fails to render is reported and skipped."""
    out.mkdir(parents=True, exist_ok=True)
    console = exe.name.startswith("fsvr_console")
    wavs = {}
    for name, extra, note, secs in CASES:
        if "{rom}" in " ".join(extra):
            if factory:
                extra = ["-factory"] + [x for x in extra if x not in ("-r", "{rom}")]
            else:
                extra = [x.replace("{rom}", str(rom)) for x in extra]
        if not console:
            extra = ["-fseq" if x == "-f" else x for x in extra]
        wav = out / f"{name}.wav"
        args = [str(exe)] + extra + ["-w", str(wav), "-n", str(note), "-d", str(secs)]
        r = subprocess.run(args, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"FAIL {name}: exit {r.returncode}\n{r.stdout}{r.stderr}")
            continue
        wavs[name] = wav
    name, mid, tail = DEMO
    wav = out / f"{name}.wav"
    args = [str(exe), "-smf", str(ROOT / mid), "-w", str(wav), "-d", str(tail)] if console \
        else [str(exe), "-d", str(tail), str(wav), str(ROOT / mid)]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"FAIL {name}: exit {r.returncode}\n{r.stdout}{r.stderr}")
    else:
        wavs[name] = wav
    return wavs


def compare(a, b):
    """max step, samples moved, samples moved by more than one, first moved frame; None when lengths differ."""
    x, y = read_s16(a).astype(np.int32), read_s16(b).astype(np.int32)
    if x.shape != y.shape:
        return None
    d = np.abs(x - y)
    moved = np.flatnonzero(d.ravel())
    return int(d.max()), int(moved.size), int((d > 1).sum()), int(moved[0] // 2) if moved.size else -1


def build_base(rev):
    """A detached worktree of rev under build/ab, built with its own build.bat or CMake; the renderer path."""
    sha = subprocess.check_output(["git", "rev-parse", "--short", rev], cwd=ROOT, text=True).strip()
    src = AB / f"src-{sha}"
    if not src.exists():
        subprocess.check_call(["git", "worktree", "add", "--detach", str(src), rev], cwd=ROOT)
    if platform.system() == "Windows":
        if shutil.which("cl"):
            script = ".\\build.bat\n"
        else:
            vc = next((p for p in (
                r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
                r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
                r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
            ) if Path(p).exists()), None)
            if vc is None:
                raise SystemExit("no cl on the path and no Visual Studio 2022 vcvars64.bat found")
            script = f'call "{vc}"\n.\\build.bat\n'
        bat = src / "build" / "abcheck_build.bat"
        bat.parent.mkdir(exist_ok=True)
        bat.write_text("@echo off\n" + script)
        subprocess.check_call(["cmd", "/c", str(bat)], cwd=src)
        exe = src / "bin" / "fsvr_console.exe"
    else:
        bd = src / "build" / "cmake"
        subprocess.check_call(["cmake", "-B", str(bd), "-S", str(src), "-DCMAKE_BUILD_TYPE=Release"])
        subprocess.check_call(["cmake", "--build", str(bd), "--config", "Release", "--target", "render_capture"])
        exe = src / "bin" / "render_capture"
    if not exe.exists():
        raise SystemExit(f"the build left no renderer at {exe}")
    print(f"built {rev} ({sha}) in {src}; remove it later with: git worktree remove {src}")
    return sha, exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--save", help="keep the renders in this folder (default: build/ab/head)")
    ap.add_argument("--ref", help="a folder of renders to compare with")
    ap.add_argument("--tier", choices=["A", "B"], help="fail unless every case meets this tier")
    ap.add_argument("--build-base", metavar="REV", help="build REV apart and render its set into build/ab/<sha>")
    ap.add_argument("--exe", help="the renderer to use")
    ap.add_argument("--rom", default=os.environ.get("FSVR_ROM", str(DEFAULT_ROM)))
    ap.add_argument("--factory", action="store_true", help="the ROM cases from the factory banks")
    ap.add_argument("--write-hashes", metavar="FILE", help="write the hashes of this render set")
    ap.add_argument("--ref-hashes", metavar="FILE", help="compare the hashes with this file, report only")
    a = ap.parse_args()

    rom = Path(a.rom)
    factory = a.factory or not rom.exists()
    if a.build_base:
        sha, exe = build_base(a.build_base)
        out = AB / sha
    else:
        exe = Path(a.exe) if a.exe else renderer()
        out = Path(a.save) if a.save else AB / "head"
    print(f"renderer: {exe}  rom: {'factory banks' if factory else rom}  into: {out}")
    wavs = render_all(exe, out, rom, factory)
    hashes = {name: digest(w) for name, w in wavs.items()}
    fails = len(CASES) + 1 - len(wavs)

    if a.ref:
        ref = Path(a.ref)
        worst = 0
        for name, w in wavs.items():
            other = ref / f"{name}.wav"
            if not other.exists():
                print(f"{name:18} no reference")
                continue
            c = compare(w, other)
            if c is None:
                print(f"{name:18} FAIL  length differs")
                worst = max(worst, 2)
                continue
            step, moved, big, first = c
            tier = "A" if step == 0 else "B" if step == 1 else "FAIL"
            worst = max(worst, 0 if step == 0 else 1 if step == 1 else 2)
            detail = "" if step == 0 else f"  max {step} step{'s' if step > 1 else ''}, {moved} moved ({big} by more than one), first at frame {first}"
            print(f"{name:18} {tier:4}{detail}")
        overall = "A" if worst == 0 else "B" if worst == 1 else "FAIL"
        print(f"abcheck: tier {overall}")
        if a.tier and (overall == "FAIL" or (a.tier == "A" and overall != "A")):
            fails += 1
    else:
        for name, h in hashes.items():
            print(f"{name:18} {h}")

    if a.write_hashes:
        Path(a.write_hashes).write_text(json.dumps({"renderer": exe.name, "cases": hashes}, indent=1, sort_keys=True) + "\n")
        print(f"wrote {a.write_hashes}")
    if a.ref_hashes:
        ref = json.loads(Path(a.ref_hashes).read_text())["cases"]
        same = [n for n in hashes if ref.get(n) == hashes[n]]
        other = [n for n in hashes if ref.get(n) not in (None, hashes[n])]
        print(f"hashes: {len(same)} same, {len(other)} differ" + (": " + " ".join(other) if other else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

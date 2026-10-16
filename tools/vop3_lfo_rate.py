"""s41: modulation rates on the unit. A 1 kHz sine through one modulation effect at a time, each the first ROM
preset's block for its type (Full B3's two rotaries, Vokodrone's flanger from the demo), nothing patched.
Writes docs/vop3_ship/41a.probes.json (41b is its eight-config subset, the take that was scored).
--score <capture dir> [probe] prints the three strongest lines of the tone's envelope and FM, unit then engine.
The probe is a small binary on src/effects/effects.h that plays a 0.25 1 kHz sine into one bus of a performance
for N seconds and writes the stereo out as f32 (built ad hoc; see findings 2026-10-15 session 41)."""
import json, sys, subprocess
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools")); import gen_parameters as G
ROM = ROOT.parent / "FS1R_DISASM/roms/fs1r_v120_eprom_cpuview.bin"
OUT = ROOT / "docs/vop3_ship/41a.probes.json"
TYPES = {"var": ["Chorus", "Celeste", "Flanger", "Symphonic", "Phaser1", "Phaser2", "Rotary SP", "Tremolo", "Auto Pan"],
         "ins": ["Chorus", "Flanger", "Phaser1", "Phaser2", "Rotary SP", "2WayRotary", "Tremolo", "Auto Pan"]}
NAMES = {"var": G.VARIATION_TYPES, "ins": G.INSERTION_TYPES}
OFF = {"rev": 0x58, "var": 0x5B, "ins": 0x5F}


def demo_perf(song="01_Vokodrone"):
    d = (ROOT / "captures/demo" / f"{song}.mid").read_bytes()
    i = d.find(bytes.fromhex("43005e03101000")) + 8       # the song's performance bulk (to 10 00 00)
    return d[i:i + 400]


def configs():
    r = ROM.read_bytes()
    first = {}
    for i in range(384):
        p = r[0xA000 + i * 400:0xA000 + i * 400 + 400]
        for k in ("var", "ins"):
            first.setdefault((k, p[80 + OFF[k]]), (f"P{i}", p))
    out = {}
    for k, names in TYPES.items():
        for nm in names:
            t = NAMES[k].index(nm)
            src, p = first[(k, t)]
            out[f"{k}_{nm.replace(' ', '').lower()}"] = (k, t, src, p)
    vk = demo_perf()
    assert vk[:9] == b"Vokodrone", vk[:12]
    out["ins_flanger_voko"] = ("ins", vk[80 + 0x5F], "Vokodrone", vk)
    return out


def perf_only(k, p):
    p = bytearray(p)
    for kk, o in OFF.items():
        if kk != k:
            p[80 + o] = 0
    return bytes(p)


def write():
    cfg, probes = {}, []
    for name, (k, t, src, p) in configs().items():
        cfg[name] = {"slot": k, "type": t, "voice": "sine", "hz": 1000.0, "level": 90, "block": p[80:192].hex(),
                     "src": src}
        probes.append({"config": name, "name": f"{name}_b", "detail": f"{name} from {src}, held again"})
    OUT.write_text(json.dumps({"configs": cfg, "probes": probes}, indent=1))
    print("wrote", OUT, len(cfg), "configs")


def engine(name, secs, mod="/tmp/ab/mod"):
    """the engine's left output for config `name`: `mod` is tools' 1 kHz probe (see the header), secs + 2 s,
    the first 2 dropped"""
    k, t, src, p = configs()[name]
    fn = f"/tmp/s41_{name}.bin"; Path(fn).write_bytes(perf_only(k, p))
    raw = subprocess.run([mod, fn, "1" if k == "ins" else "2", str(secs + 2)], capture_output=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).astype(float)[96000:, 0]


def lines(x, n=3):
    X = np.fft.fft(x); h = np.zeros(len(x)); h[0] = 1; h[1:(len(x)+1)//2] = 2; a = np.fft.ifft(X*h)
    out = {}
    for lab, s in (('env', np.abs(a)[:-1]), ('fm', np.diff(np.unwrap(np.angle(a))))):
        s = s[:len(s)//48*48].reshape(-1, 48).mean(1); s = s - s.mean()
        S = np.abs(np.fft.rfft(s*np.hanning(len(s)))); f = np.fft.rfftfreq(len(s), 1/1000); S[(f < 0.05) | (f > 30)] = 0
        pk = []
        for k in np.argsort(S)[::-1]:
            if all(abs(f[k]-q) > 0.03 for q, _ in pk): pk.append((f[k], S[k]))
            if len(pk) == n: break
        out[lab] = ' '.join('%.3f(%d)' % (q, 100*v/pk[0][1]) for q, v in pk) + '  depth %.4f' % s.std()
    return out

def score(d, mod):
    """per config, its three holds back to back (_ref, _b, _ref_end): the unit's lines, then the engine's"""
    import soundfile as sf
    d = Path(d); y, sr = sf.read(d / "session25.flac", dtype="float64")
    t0 = float((d / "rec_start.txt").read_text()); segs = json.loads((d / "probe_segments.json").read_text())
    by = {s["name"]: s for s in segs}
    for s in segs:
        if not s["name"].endswith("_ref"):
            continue
        name = s["name"][:-4]; e = by[name + "_ref_end"]
        a, b = s["t"] - t0 + 0.5, e["t"] + e["hold"] - t0 - 0.05
        u = lines(y[int(a * sr):int(b * sr), 0]); m = lines(engine(name, b - a, mod))
        print("==", name, "%.1f s" % (b - a))
        print("  unit   env", u["env"], "| fm", u["fm"])
        print("  engine env", m["env"], "| fm", m["fm"])


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--score":
        score(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "/tmp/ab/mod")
    else:
        write()

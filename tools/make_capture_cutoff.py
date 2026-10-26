#!/usr/bin/env python3
"""FSVR#56: does the unit smooth a controller's cutoff write, or step it as the firmware's write does?

    python tools/make_capture_cutoff.py out.mid       -> the take, played into the unit as it stands
    python tools/make_capture_cutoff.py --read take.wav ref.wav

`FUN_0000C36C` writes `0xC0D + 0xA9 * byte` straight into the filter's coefficient memory; whether the chip
then glides to it is the open question (docs/findings.md, 2026-10-26). One sine through LPF24, the part's
cutoff moved by the modulation wheel (controller set 1: MW -> Cutoff, depth +63), so every jump is a real
ctrlDest_21 write. Each segment strikes from silence:

* `hold`  no controller moves: the null. Its level must not step.
* `jump`  MW 0 -> 127 -> 0 under a held note, twice, at two notes (two datasets): the 10-90 % time of the
          level's move is the chip's smoothing (the unit's step settles in the filter's own ~1 ms).
* `walk`  MW 0..127 a value every 16 ms, then back: a breath-controller-rate staircase, the report's case.

`--read` takes the unit's recording and a render of the same file and prints, per jump, the 10-90 % time
of the sine's level in each, so an engine render with the glide off (the unit's step, by the firmware)
and one with it on (`-fsvr`) say which the unit is.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fs1r_patch as fp

SR = 48000
NOTES = (69, 81)                 # 440 and 880 Hz
LEAD = 1000                      # ms from the file's start to the first strike


def voice(cut):
    v = fp.init_voice("CutGlide  ")
    fp.set_op(v, 1, keysync=1, level=99)
    v[0x57] = cut                # the voice's cutoff; the controller adds to the part's
    return v


def perf():
    p = fp.init_performance("Cutoff Glide")
    fp.set_part(p, 0, filter=1, panscale=50, cutoff=40)  # part cutoff -24: the corner under the sine at MW 0
    p[0x28] = 1                  # set 1 acts on part 1
    bits = 1 << 12               # set 1's source: MW, SRC_MW in the engine's order (src/engine/synth.h)
    p[0x30], p[0x31] = bits >> 7, bits & 0x7F
    p[0x40] = 21                 # destination: Cutoff
    p[0x48] = 64 + 63            # depth +63
    return p


def segments():
    """(name, note, start ms, [(ms after start, mw)], hold ms). Jumps sit 600 ms apart, past any glide."""
    out, t = [], LEAD
    out.append(("hold", 81, t, [], 2400)); t += 3400
    for n in NOTES:
        out.append((f"jump-n{n}", n, t, [(600, 127), (1200, 0), (1800, 127), (2400, 0)], 3000)); t += 4000
    walk = [(400 + 16 * i, i) for i in range(128)] + [(400 + 16 * 128 + 16 * i, 127 - i) for i in range(128)]
    out.append(("walk", 81, t, walk, 400 + 16 * 256 + 600)); t += 400 + 16 * 256 + 1600
    return out


def make(path):
    s = fp.Smf()
    s.at(0, fp.perf_bulk(perf()))
    s.at(500, fp.voice_bulk(voice(64)))
    s.cc(600, 1, 0)
    for name, note, t0, ccs, hold in segments():
        s.cc(t0 - 50, 1, 0)
        for dt, v in ccs:
            s.cc(t0 + dt, 1, v)
        s.note(t0, note, 100, hold)
    s.write(path)
    Path(path).with_suffix(".json").write_text(json.dumps(segments(), indent=1))
    print(path, "written,", len(segments()), "segments")


def wav(path):
    """Left channel, scaled to its peak: a render's wav or the rig's take.raw (S32_LE stereo, as arecord writes)."""
    import numpy as np
    if str(path).endswith(".raw"):
        x = np.fromfile(path, "<i4").reshape(-1, 2)[:, 0].astype(float)
    else:
        import soundfile
        x = soundfile.read(str(path), always_2d=True)[0][:, 0]
    return x / np.abs(x).max()


def level_db(y, note):
    """The sine's level, a cycle-length RMS a sample at a time, in dB."""
    import numpy as np
    n = int(round(SR / (440 * 2 ** ((note - 69) / 12))))
    c = np.cumsum(np.r_[0, y * y])
    return 10 * np.log10((c[n:] - c[:-n]) / n + 1e-20)


def onset(y):
    import numpy as np
    return int(np.argmax(np.abs(y) > 10 ** (-80 / 20)))   # the hold strike sits under a closed filter, ~-52 dB


def rise(lv, a, b):
    """10-90 % time (ms) of the level's move between the windows before a and after b, on the amplitude (in dB a
    closing filter's tail stretches the 90 % point); the size of the move in dB."""
    import numpy as np
    lo, hi = np.median(lv[a - 4800:a - 480]), np.median(lv[b - 4800:b - 480])
    amp = lambda d: 10 ** (d / 20)
    f = (amp(lv[a - 480:a + 9600]) - amp(lo)) / (amp(hi) - amp(lo))
    i10, i90 = np.argmax(f > 0.1), np.argmax(f > 0.9)
    return (i90 - i10) / SR * 1000, hi - lo


def read(*paths):
    """Per jump in each file, the 10-90 % time and the size of the move. The first strike aligns each file."""
    import numpy as np
    for path in paths:
        y = wav(path)
        z = onset(y) - LEAD * SR // 1000
        print(path)
        for name, note, t0, ccs, hold in segments():
            if not name.startswith("jump"):
                lv = level_db(y, note)
                s = slice(z + (t0 + 300) * SR // 1000, z + (t0 + hold - 100) * SR // 1000)
                if name == "hold":
                    print(f"  {name:9s} level spread {np.ptp(lv[s]):.2f} dB (the null: must not step)")
                else:   # the commit's metric: the level's ripple at the controller's rate, dB per ms
                    s = slice(z + (t0 + 500) * SR // 1000, z + (t0 + 400 + 16 * 256) * SR // 1000)
                    ms = lv[s][::SR // 1000]
                    print(f"  {name:9s} ripple {np.abs(np.diff(ms) - np.convolve(np.diff(ms), np.ones(16) / 16, 'same')).mean():.3f} dB/ms")
                continue
            lv = level_db(y, note)
            row = []
            for k, (dt, v) in enumerate(ccs):
                a = z + (t0 + dt) * SR // 1000
                nxt = ccs[k + 1][0] if k + 1 < len(ccs) else hold
                b = z + (t0 + nxt) * SR // 1000
                ms, d = rise(lv, a, b)
                row.append(f"{'up' if v else 'dn'} {ms:5.1f} ms {d:+5.1f} dB")
            print(f"  {name:9s} " + " | ".join(row))


def demo():
    p = perf()
    assert p[0x40] == 21 and p[0x48] == 127 and (p[0x30] << 7 | p[0x31]) == 1 << 12
    v = voice(64)
    assert v[112 + 62 + 22] == 99 and v[112 + 22] == 0      # op 2 sounds, op 1 does not
    assert len(segments()) == 4


if __name__ == "__main__":
    demo()
    if sys.argv[1] == "--read":
        read(*sys.argv[2:])
    else:
        make(sys.argv[1])

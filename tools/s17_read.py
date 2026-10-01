"""Read session 17's MIDI-driven segments (freq, rt, comb) from a take.

    python3 tools/s17_read.py <session dir> <take.flac>

freq: the Mid-band peak (Hz) of R against eq_flat (spectral ratio, 1/24-octave smoothed).
rt:   RT60 of R from the Schroeder backward integral (-5..-35 dB, x2).
comb: the echo train on R after the click: period (autocorrelation peak, samples) and the ratio of successive
      echo energies (sqrt = amplitude ratio per period), plus R's level.
"""
import json
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, __file__.rsplit("/", 2)[0] + "/../FS1R.unlock/captures")


def segments(d, flac):
    x, sr = sf.read(flac, dtype="float64")
    rows = json.load(open(d + "/probe_segments.json"))
    start = float(open(d + "/rec_start.txt").read())
    out = []
    for i, r in enumerate(rows):
        a = r["t"] - start
        b = rows[i + 1]["t"] - start if i + 1 < len(rows) else a + r["hold"] + 0.4
        out.append((r, x[max(0, int(a * sr) - sr // 4):int(b * sr)]))
    return out, sr


def onset(v, sr):
    e = np.abs(v)
    th = e.max() * 0.1
    return int(np.flatnonzero(e > th)[0]) if e.max() > 0 else 0


def rt60(v, sr):
    v = v[onset(v, sr):]
    e = np.cumsum((v ** 2)[::-1])[::-1]
    db = 10 * np.log10(e / e[0] + 1e-30)
    i5, i35 = np.argmax(db < -5), np.argmax(db < -35)
    if i35 <= i5:
        return None
    t = np.arange(i5, i35) / sr
    k = np.polyfit(t, db[i5:i35], 1)[0]
    return -60 / k


def comb(v, sr, maxlag=12000):
    v = v[onset(v, sr):]
    v = v[:min(len(v), 6 * maxlag)]
    n = 1 << int(np.ceil(np.log2(2 * len(v))))
    ac = np.fft.irfft(np.abs(np.fft.rfft(v, n)) ** 2, n)[:maxlag]
    lo = 200
    p = lo + int(np.argmax(ac[lo:]))
    ratio = ac[p] / ac[0] if ac[0] else 0.0
    # echo amplitudes: RMS over a window around each multiple of p
    w = max(32, p // 8)
    amps = [np.sqrt((v[max(0, m * p - w):m * p + w] ** 2).mean()) for m in range(1, 6) if m * p + w < len(v)]
    steps = [amps[i + 1] / amps[i] for i in range(len(amps) - 1) if amps[i] > 0]
    return p, ratio, steps


def peak(v, ref, sr):
    f = np.fft.rfftfreq(len(v), 1 / sr)
    n = min(len(v), len(ref))
    S = lambda s: np.abs(np.fft.rfft(s[:n] * np.hanning(n))) ** 2
    f = np.fft.rfftfreq(n, 1 / sr)
    r = S(v) / (S(ref) + 1e-30)
    lf = np.log2(np.maximum(f, 1))
    sm = np.array([r[(lf > l - 1 / 48) & (lf < l + 1 / 48)].mean() if 100 < fi < 8000 else 0 for l, fi in zip(lf, f)])
    i = int(np.argmax(sm))
    return f[i], 10 * np.log10(sm[i] + 1e-30)


def main():
    segs, sr = segments(sys.argv[1], sys.argv[2])
    ref = None
    for r, s in segs:
        n = r["name"]
        L, R = s[:, 0], s[:, 1]
        lv = lambda v: 20 * np.log10(np.sqrt((v ** 2).mean()) + 1e-15)
        if n == "eq_flat":
            ref = s[sr // 2:-sr // 4]
            print(f"{n:14s} L {lv(L):6.1f} R {lv(R):6.1f}")
        elif n.startswith("eq_") and ref is not None:
            mid = s[sr // 2:-sr // 4]
            fL, gL = peak(mid[:, 0], ref[:, 0], sr)
            fR, gR = peak(mid[:, 1], ref[:, 1], sr)
            print(f"{n:14s} peak L {fL:7.1f} Hz {gL:+5.1f} dB   R {fR:7.1f} Hz {gR:+5.1f} dB")
        elif n.startswith("rt_"):
            print(f"{n:14s} RT60 R {rt60(R, sr)}  L {rt60(L, sr)}   R {lv(R):6.1f} dB")
        elif n.startswith(("comb", "k0", "a3", "a5", "a9", "off_", "c3_off", "revwin")):
            p, ratio, steps = comb(R, sr)
            print(f"{n:14s} R {lv(R):6.1f} dB  period {p:5d}  ac {ratio:+.3f}  echo steps " + " ".join(f"{x:.3f}" for x in steps))


if __name__ == "__main__":
    main()

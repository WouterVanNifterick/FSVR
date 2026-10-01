"""Comb 1 loop model against session 17's comb segments (FSVR docs/vop3_meg_match.md).

The comb's loop filter is first order with the firmware's three words: x3 (0a3), x4 (0a4), p (0a5), all 1.15:
    L(z) = 2 (x3 + x4 z^-1) / (1 - 2 p z^-1)
and the long-run echo ratio is the loop gain where |L| peaks, which for these words is DC or Nyquist.
"""
import numpy as np

X, P = 0x16F8 / 32768, (0xD06A - 0x10000) / 32768          # Hall1, time 0.8 s, damp 1.0 (dumped coefs)


def loop_peak(x3, x4, p):
    w = np.linspace(0, np.pi, 2049)
    z = np.exp(-1j * w)
    return np.abs(2 * (x3 + x4 * z) / (1 - 2 * p * z)).max()


MEASURED = {"comb1": (X, X, P, 0.412), "k0a3_half": (X / 2, X, P, 0.697), "k0a5_0": (X, X, 0, 0.714),
            "k0a3_0": (0, X, P, None), "k0a4_0": (X, 0, P, None)}    # None: ran away (ratio >= 1)

if __name__ == "__main__":
    for name, (a, b, p, m) in MEASURED.items():
        g = loop_peak(a, b, p)
        print(f"{name:10s} model {g:.3f}  measured {m}")
        assert (g >= 1) if m is None else abs(g - m) < 0.01, (name, g, m)
    print("ok: comb 1 loop = 2 (x3 + x4 z^-1) / (1 - 2 p z^-1) on all five segments")

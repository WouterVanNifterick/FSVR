#!/usr/bin/env python3
"""Write the Templates bank: starting points to build a sound from, not sounds.

    python tools/make_templates.py   -> presets/templates.syx

Each template is a performance with part 1 alone on the performance channel, dry, no filter, no Fseq,
followed by its voice as a part 1 bulk (0x40), which is the shape Save to Bank writes, so the plug-in
loads the voice with the performance. Built on tools/fs1r_patch.py's silent init voice and performance.
"""
from pathlib import Path

import fs1r_patch as fp

OUT = Path(__file__).resolve().parent.parent / "presets" / "templates.syx"


OFF = (0, 0, 0, 0)


def voice(name, alg=0, ops=(), **common):
    """Every operator, voiced and noise, off unless named. Off is EG levels 0, not output level 0: level 0 is
    -95.6 dB (LEVTAB[0] = 127 steps of 0.376 dB, doubled), above the engine's -100 dB gate, so a sine at a
    random note phase and a clock-seeded noise each leaked 64 LSB of 24-bit. EG level 0 is attenuation
    step 63, which the EG takes as off (EG::lvl_db), whatever level or bias a controller adds."""
    v = fp.init_voice(name.ljust(10))
    v[0x2C] = alg
    for k, val in common.items():
        v[int(k[1:], 16)] = val
    used = dict(ops)
    for o in range(8):
        fp.set_op(v, o, **{"eg_l": OFF, "u_eg_l": OFF, **used.get(o, {})})
    return v


def release(t):   # the amplitude EG: on at once, held, and t to fade after key off
    return {"eg_l": (99, 99, 99, 0), "eg_t": (0, 0, 0, t)}


TEMPLATES = [
    ("Init Patch", voice("Init", ops=[(0, dict(level=99, **release(30)))])),
    ("Silence", voice("Silence")),
    # algorithm 8: operator 1 modulates operator 2, the only carrier
    ("FM Pair", voice("FM Pair", alg=7, ops=[(0, dict(level=70, **release(30))), (1, dict(level=99, **release(30)))])),
    # algorithm 1: every operator a carrier, three sines at the first three harmonics
    ("Additive", voice("Additive", ops=[(0, dict(level=99, coarse=1, **release(20))),
                                        (1, dict(level=90, coarse=2, **release(20))),
                                        (2, dict(level=85, coarse=3, **release(20)))])),
    # a formant operator's coarse and fine are its centre as a fixed frequency (fp.fixed_hz), whatever the key
    ("Formant", voice("Formant", ops=[(0, dict(level=99, form=fp.FRMT, bw=60, coarse=fp.fixed_bytes(1000.0)[0],
                                               fine=fp.fixed_bytes(1000.0)[1], **release(30)))])),
    ("Noise", voice("Noise", ops=[(0, dict(level=0, u_level=99, u_coarse=fp.fixed_bytes(1000.0)[0],
                                           u_fine=fp.fixed_bytes(1000.0)[1], u_bw=99, u_eg_l=(99, 99, 99, 0),
                                           u_eg_t=(0, 0, 0, 30), eg_l=OFF))])),
]


def main():
    out = b""
    for name, v in TEMPLATES:
        p = fp.init_performance(name)
        fp.set_part(p, 0, channel=16, panscale=50)   # the performance channel, centred whatever the key
        out += fp.perf_bulk(p) + fp.voice_bulk(v, 0)
    OUT.write_bytes(out)
    print(f"{len(TEMPLATES)} templates -> {OUT.relative_to(OUT.parents[1])} ({len(out)} bytes)")


if __name__ == "__main__":
    main()

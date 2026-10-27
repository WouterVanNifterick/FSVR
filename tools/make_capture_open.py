#!/usr/bin/env python3
"""The takes v0.8.4's open items needed, in one self-playing file (2026-10-26).

    python tools/make_capture_open.py out.mid                      -> the take and out.json, its segments
    python tools/make_capture_open.py --take2 out2.mid             -> the second take (segments2)
    python tools/make_capture_open.py --take3 out3.mid             -> the third (segments3)
    python tools/make_capture_open.py --take4 out4.mid             -> the fourth (segments4)
    python tools/make_capture_open.py --take5 out5.mid             -> the fifth (segments5)
    python tools/make_capture_open.py --take6 out6.mid             -> the sixth (segments6)
    python tools/make_capture_open.py --take7 out7.mid             -> the seventh (segments7)
    python tools/make_capture_open.py --take8 out8.mid             -> the eighth (segments8)
    python tools/make_capture_open.py --take9 out9.mid             -> the ninth (segments9)
    python tools/make_capture_open.py --take10 out10.mid           -> the tenth (segments10)
    python tools/make_capture_open.py --take11 out11.mid           -> the eleventh (segments11)
    python tools/make_capture_open.py --take12 out12.mid           -> the twelfth (segments12)
    python tools/make_capture_open.py --take13 out13.mid           -> the thirteenth (segments13)
    python tools/make_capture_open.py --take14 out14.mid           -> the fourteenth (segments14)
    python tools/make_capture_open.py --take15 out15.mid           -> the fifteenth (segments15)
    python tools/make_capture_open.py --take16 out16.mid           -> the sixteenth (segments16)
    python tools/make_capture_open.py --take17 out17.mid           -> the seventeenth (segments17)
    python tools/make_capture_open.py --take18 out18.mid           -> the eighteenth (segments18)
    python tools/make_capture_open.py --take19 out19.mid           -> the nineteenth (segments19)
    python tools/make_capture_open.py --take20 out20.mid           -> the twentieth (segments20)
    python tools/make_capture_open.py --take21 out21.mid           -> the twenty-first (segments21)
    python tools/make_capture_open.py --take22 out22.mid           -> the twenty-second (segments22)
    python tools/make_capture_open.py --take23 out23.mid           -> the twenty-third (segments23)
    python tools/make_capture_open.py --take24 out24.mid           -> the twenty-fourth (segments24)
    python tools/make_capture_open.py --take25 out25.mid           -> the twenty-fifth (segments25)
    python tools/make_capture_open.py --read out.json take.raw|render.wav ...  -> one row per segment

Plain MIDI: performance and voice bulks, parameter changes, notes. Every segment strikes from silence, and the
first is a reference sine every level below is read against, so a render and the unit compare without a gain.

* `corr-*`  OP Att (voice 0x2D..0x34, carrier level correction). Algorithm 8, op 1 into op 2. A bulk with a
            correction on the modulator, which FUN_0003251C drops; the same value as a parameter change, which
            FUN_00032624 writes into the staged word without the carrier test; and on the carrier, whose dB a
            step is cal::CARRIER_DB, INFERRED. Read: the carrier's level and the first sideband's against it.
* `rate-*`  The noise centre and the channel pitch through the phase generator (2026-10-24 entry): a fixed sine
            on coarse 16 fine 0, and noise at resonance 7 (a tone at its centre) on the same word, and linked
            to the channel pitch at note 69. Read: each tone's frequency to a hundredth of a hertz.
* `teck-*`  Teck Hook (performance 180) whole, dry, and each part alone and dry, at notes 60 and 72. Read: level.
* `tail`    a sine into a reverb at full send, released: when the output returns to digital zero.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fs1r_patch as fp

SR = 48000
LEAD = 1000
PERF = ROOT / "presets/performances/180_Teck_Hook.syx"


def sine(op=1, **kw):
    v = fp.init_voice("Open      ")
    fp.set_op(v, op, **{"keysync": 1, "level": 99, **kw})
    return v


def fm(corr_mod=0, corr_car=0):
    v = fp.init_voice("OpenFM    ")
    v[0x2C] = 7                                   # algorithm 8: op 1 into op 2, op 2 alone in the mix
    fp.set_op(v, 0, keysync=1, level=85)
    fp.set_op(v, 1, keysync=1, level=99)
    v[0x2D], v[0x2E] = corr_mod, corr_car
    return v


def noise(link=False):
    v = fp.init_voice("OpenNoise ")
    kw = dict(mode=fp.UV_LINK_FO) if link else dict(coarse=16, fine=0)
    fp.set_op(v, 1, keysync=1, level=0, fixed=1, coarse=16, u_level=99, u_bw=20, u_res=7,
              **{"u_" + k: x for k, x in kw.items()})
    return v


def dry():
    p = fp.init_performance("Open Take")
    fp.set_part(p, 0, panscale=50)
    return p


def teck(solo=None, fx=True):
    p = bytearray(PERF.read_bytes()[9:409])
    if not fx:
        p[80 + 0x58] = p[80 + 0x5B] = p[80 + 0x5F] = 0
    for i in range(4):     # the parts receive on the performance channel (0x10); channel 1 here, as every segment
        p[192 + i * 52 + 0x04] = 0x7F if solo is not None and i != solo else 0
    return p


def hall():
    """Teck Hook's reverb (its whole effect block, type 14) with the variation and insertion off, at full send."""
    p = dry()
    p[80:192] = teck()[80:192]
    p[80 + 0x5B] = p[80 + 0x5F] = 0
    p[192 + 0x13] = 127                            # part 1 reverb send
    return p


def segments():
    """(name, perf or None, voice or None, [param changes], note, channel, hold ms, kind). Bytes as hex."""
    h = lambda b: None if b is None else bytes(b).hex()
    P = lambda p: fp.perf_bulk(p).hex()
    s = [("ref", P(dry()), h(sine()), [], 69, 0, 2000, "level")]
    s += [("corr-none", None, h(fm()), [], 69, 0, 2000, "fm"),
          ("corr-mod-bulk15", None, h(fm(corr_mod=15)), [], 69, 0, 2000, "fm"),
          ("corr-mod-edit15", None, h(fm()), [(0x2D, 15)], 69, 0, 2000, "fm"),
          ("corr-mod-edit8", None, h(fm()), [(0x2D, 8)], 69, 0, 2000, "fm"),
          ("corr-car-4", None, h(fm(corr_car=4)), [], 69, 0, 2000, "fm"),
          ("corr-car-8", None, h(fm(corr_car=8)), [], 69, 0, 2000, "fm"),
          ("corr-car-edit8", None, h(fm()), [(0x2E, 8)], 69, 0, 2000, "fm")]
    s += [("rate-sine", None, h(sine(fixed=1, coarse=16, fine=0)), [], 69, 0, 6000, "rate"),
          ("rate-noise", None, h(noise()), [], 69, 0, 6000, "rate"),
          ("rate-noise-link", None, h(noise(link=True)), [], 69, 0, 6000, "rate"),
          ("rate-sine-note", None, h(sine()), [], 69, 0, 6000, "rate")]
    for n in (60, 72):
        s.append((f"teck-n{n}", P(teck()), None, [], n, 0, 3000, "level"))
        s.append((f"teck-dry-n{n}", P(teck(fx=False)), None, [], n, 0, 3000, "level"))
        for i in range(4):
            s.append((f"teck-p{i + 1}-n{n}", P(teck(solo=i, fx=False)), None, [], n, 0, 3000, "level"))
    s.append(("tail", P(hall()), h(sine()), [], 69, 0, 1000, "tail"))
    return s


def segments2():
    """The second take. An algorithm change after an OP Att edit (FUN_0003251C rebuilds the words with the carrier
    test), and Teck Hook part by part, eight strikes each: op 8 is a 0 Hz formant with key sync off, so its phase
    and the part's level are drawn anew each note, and one strike measures nothing."""
    h = lambda b: None if b is None else bytes(b).hex()
    P = lambda p: fp.perf_bulk(p).hex()
    s = [("ref", P(dry()), h(sine()), [], 69, 0, 2000, "level"),
         ("corr-mod-edit15", None, h(fm()), [(0x2D, 15)], 69, 0, 2000, "fm"),
         ("corr-edit-then-alg", None, None, [(0x2C, 7)], 69, 0, 2000, "fm"),
         ("corr-edit-then-op", None, h(fm()), [(0x2D, 15), (0x2E, 0)], 69, 0, 2000, "fm")]
    for n in (60, 72):
        for i in (None, 0, 1, 2, 3):
            for k in range(8):
                nm = f"teck-{'all' if i is None else f'p{i + 1}'}-n{n}-{k}"
                s.append((nm, P(teck(solo=i, fx=i is None)) if k == 0 else None, None, [], n, 0, 1500, "level"))
    return s


def segments3():
    """The third take: what a key-sync-off operator starts at. One fixed sine at 1 Hz with key sync off, struck
    eight times (held 1.25 s, 1.5 s apart) on a mono part, then on a poly part. The phase each strike starts at
    tells the rules apart: +0.75 of a turn a strike if the phase runs on through the gaps, +0.25 if it stops at
    the note-off, 0 if it resets, scattered if it is drawn, as FSVR draws it (notes.cpp setup_ops)."""
    h = lambda b: None if b is None else bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for poly in (0, 1):
        p = dry()
        fp.set_part(p, 0, panscale=50, poly=poly)
        for k in range(8):
            s.append((f"phase-{'poly' if poly else 'mono'}-{k}", fp.perf_bulk(p).hex() if k == 0 else None,
                      h(sine(fixed=1, coarse=7, fine=28, keysync=0)) if k == 0 else None, [], 69, 0, 1250, "phase"))
    return s


def pair():
    """Op 2 at 1 Hz with key sync on, its phase 0 at the note-on, so it marks the strike; op 3 at 3 Hz with key
    sync off. Op 3's phase against op 2's is where its generator stood at the strike, free of the rig's timing."""
    v = fp.init_voice("OpenPair  ")
    fp.set_op(v, 1, keysync=1, level=99, fixed=1, coarse=7, fine=28)
    fp.set_op(v, 2, keysync=0, level=99, fixed=1, coarse=8, fine=103)
    return v


def segments4():
    """The fourth take: when a key-sync-off generator is reset. A mono part struck four times, then the same
    performance sent again and struck twice, then the voice alone sent again and struck twice; then a poly part."""
    h = lambda b: bytes(b).hex()
    mono = dry(); fp.set_part(mono, 0, panscale=50, poly=0)
    poly = dry(); fp.set_part(poly, 0, panscale=50, poly=1)
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    plan = [("mono-a", mono, True, 4), ("mono-perf", mono, True, 2), ("mono-voice", None, True, 2), ("poly", poly, True, 4)]
    for name, perf, voice, n in plan:
        for k in range(n):
            s.append((f"pair-{name}-{k}", fp.perf_bulk(perf).hex() if perf and k == 0 else None,
                      h(pair()) if voice and k == 0 else None, [], 69, 0, 1250, "pair"))
    return s


def segments5():
    """The fifth take: does an algorithm change clear an OP Att edit on a modulator? Take 2 wrote the same
    algorithm and the edit stayed; here the algorithm goes to 1 and back to 8."""
    h = lambda b: bytes(b).hex()
    return [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level"),
            ("corr-none", None, h(fm()), [], 69, 0, 2000, "fm"),
            ("corr-mod-edit15", None, h(fm()), [(0x2D, 15)], 69, 0, 2000, "fm"),
            ("corr-edit-alg-1-8", None, h(fm()), [(0x2D, 15), (0x2C, 0), (0x2C, 7)], 69, 0, 2000, "fm"),
            ("corr-edit-alg-same", None, h(fm()), [(0x2D, 15), (0x2C, 7)], 69, 0, 2000, "fm")]


def hook(ks8=None, fb=None):
    """Teck Hook's voice, PrA Hook: op 7 (sine, feedback 7) into op 8 (a formant fixed at 0 Hz, key sync off)."""
    v = bytearray((ROOT / "presets/native/063_Hook.syx").read_bytes()[9:9 + 608])
    if ks8 is not None:
        v[112 + 7 * 62] = (v[112 + 7 * 62] & 0x3F) | ks8 << 6
    if fb is not None:
        v[0x3D] = fb
    return v


def segments6():
    """The sixth take: what sets Teck Hook's level. Its voice on one dry mono part, six strikes a setting, each
    setting after its own voice bulk: as stored, op 8 with key sync on, and the feedback at 0."""
    h = lambda b: bytes(b).hex()
    mono = dry(); fp.set_part(mono, 0, panscale=50, poly=0)
    s = [("ref", fp.perf_bulk(mono).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for name, v in (("stored", hook()), ("ks8", hook(ks8=1)), ("fb0", hook(fb=0)), ("stored2", hook())):
        for n in (60, 72):
            for k in range(6):
                s.append((f"hook-{name}-n{n}-{k}", None, h(v) if k == 0 else None, [], n, 0, 1500, "level"))
    return s


def frmt(hz, bw, mod=None):
    """A formant operator, fixed at hz, bandwidth bw, alone; or at algorithm 8 under a sine at level mod."""
    v = fp.init_voice("OpenFrmt  ")
    c, f, _ = fp.fixed_bytes(hz) if hz else (0, 0, 0)
    if mod is not None:
        v[0x2C] = 7
        fp.set_op(v, 0, keysync=1, level=mod)
    fp.set_op(v, 1, keysync=1, level=99, form=fp.FRMT, fixed=1, coarse=c, fine=f, bw=bw)
    return v


def segments7():
    """The seventh take: a formant's level against the note, which sets its window in periods (FRMT_WL_MAX). One
    formant at 1 kHz at bandwidths 24, 48 and 72, and one at 0 Hz under a sine (Teck Hook's case), notes 36 to 84."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for name, v in (("1k-bw24", frmt(1000, 24)), ("1k-bw48", frmt(1000, 48)), ("1k-bw72", frmt(1000, 72)),
                    ("0hz-bw48-mod85", frmt(0, 48, mod=85)), ("0hz-bw24-mod85", frmt(0, 24, mod=85))):
        for k, n in enumerate((36, 48, 54, 60, 66, 72, 84)):
            s.append((f"frmt-{name}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1500, "level"))
    return s


def segments8():
    """The eighth take: whether a formant's window depends on its centre. Take 7 had a 1 kHz formant shorten its
    window with the note as the bandwidth law says, and a 0 Hz one under a sine hardly at all. Note 36, the
    formant alone at bandwidth 48 on centres 125 Hz to 4 kHz; then at 0 Hz under the sine, bandwidths 32 to 72."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for hz in (125, 250, 500, 1000, 2000, 4000):
        s.append((f"frmt-{hz}-bw48-n36", None, h(frmt(hz, 48)), [], 36, 0, 1500, "level"))
    for bw in (32, 40, 48, 56, 64, 72):
        s.append((f"frmt-0hz-bw{bw}-mod85-n36", None, h(frmt(0, bw, mod=85)), [], 36, 0, 1500, "level"))
    for hz in (125, 250, 500):
        s.append((f"frmt-{hz}-bw48-mod85-n36", None, h(frmt(hz, 48, mod=85)), [], 36, 0, 1500, "level"))
    return s


def segments9():
    """The ninth take: why Teck Hook's two portamento parts play their first strike after a performance load at
    one level and every later strike at another (take 2). Part 1 alone and dry, six strikes a setting, each
    setting a fresh performance bulk: as stored (portamento on, full time), portamento off, portamento on at time
    0, and as stored struck on a note a semitone up and back."""
    h = lambda b: bytes(b).hex()
    def p1(porta=None, time=None):
        p = teck(solo=0, fx=False)
        if porta is not None: p[192 + 0x24] = porta
        if time is not None: p[192 + 0x25] = time
        return p
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for name, p, notes in (("stored", p1(), [60] * 6), ("porta-off", p1(porta=0), [60] * 6),
                           ("porta-t0", p1(time=0), [60] * 6), ("stored-alt", p1(), [60, 61, 60, 61, 60, 60])):
        for k, n in enumerate(notes):
            s.append((f"p1-{name}-{k}-n{n}", fp.perf_bulk(p).hex() if k == 0 else None, None, [], n, 0, 1500, "level"))
    return s


def hook_steps():
    """PrA Hook taken one difference at a time toward take 7's 0 Hz formant under a sine, which matches."""
    v = hook(); out = [("stored", bytes(v))]
    for k in (0x1F, 0x20, 0x21, 0x22, 0x3E): v[k] = 50
    out.append(("peg-flat", bytes(v)))
    p7, p8 = 112 + 6 * 62, 112 + 7 * 62
    v[0x3D] = 0; out.append(("fb0", bytes(v)))
    v[p7 + 1], v[p7 + 2] = 1, 0; out.append(("op7-coarse1", bytes(v)))
    for i, x in enumerate((99, 99, 99, 0, 0, 0, 0, 0)): v[p7 + 12 + i] = x; v[p8 + 12 + i] = x
    out.append(("eg-square", bytes(v)))
    v[p8 + 2] = 0; v[p8] |= 0x40; out.append(("op8-fine0-ks", bytes(v)))
    for q in (p7, p8): v[q + 23], v[q + 24], v[q + 25], v[q + 26], v[q + 27] = 39, 0, 0, 0, 0
    out.append(("no-keyscale", bytes(v)))
    v[0x54], v[0x64] = 0, 64; v[0x13] = 1; v[0x11] = 0; out.append(("lfo-filter-off", bytes(v)))
    return out


def segments10():
    """The tenth take: which part of PrA Hook puts its level 4 dB over FSVR's. Each step of hook_steps at note 60,
    three strikes, on a dry mono part."""
    mono = dry(); fp.set_part(mono, 0, panscale=50, poly=0)
    s = [("ref", fp.perf_bulk(mono).hex(), bytes(sine()).hex(), [], 69, 0, 2000, "level")]
    for name, v in hook_steps():
        for k in range(3):
            s.append((f"step-{name}-{k}", None, v.hex() if k == 0 else None, [], 60, 0, 1500, "level"))
    return s


def segments11():
    """The eleventh take: Teck Hook's parts 1 and 2 differ by 3.5 dB on the unit and the other way round in FSVR.
    PrA Hook on a dry mono part at part detune -23 and +24, with op 7's frequency EG as stored and flat: if the
    EG is what sets the level, a flat EG makes the two detunes agree. Then portamento after a performance load:
    part 1 of Teck Hook as stored, struck once on a fresh load, its pitch read over the first 100 ms."""
    h = lambda b: bytes(b).hex()
    flat = hook(); flat[112 + 6 * 62 + 8] = 50; flat[112 + 6 * 62 + 9] = 50
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for name, v in (("feg", hook()), ("flat", flat)):
        for det in (-23, 24):
            p = dry(); fp.set_part(p, 0, panscale=50, poly=0, detune=det)
            for k in range(3):
                s.append((f"det-{name}-{det:+d}-{k}", fp.perf_bulk(p).hex() if k == 0 else None,
                          h(v) if k == 0 else None, [], 60, 0, 1500, "level"))
    for k in range(3):
        s.append((f"porta-load-{k}", fp.perf_bulk(teck(solo=0, fx=False)).hex(), None, [], 60, 0, 1500, "level"))
    return s


def segments12():
    """The twelfth take: PrA Hook's op 7 alone as a carrier, frequency EG as stored (init +48, decay time 20), so
    its pitch track can be read; and as stored but with op 8 a sine at 0.0067 Hz (the 0 Hz word) in place of the
    formant, to tell the EG from the formant."""
    h = lambda b: bytes(b).hex()
    v = hook(); p7 = 112 + 6 * 62
    solo = fp.init_voice("OpenFEG   ")
    solo[p7:p7 + 62] = v[p7:p7 + 62]; solo[p7 + 22] = 99
    for i, x in enumerate((99, 99, 99, 0, 0, 0, 0, 0)): solo[p7 + 12 + i] = x
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for n in (60, 84):                   # note 84 puts 1 kHz under the 5 ms windows, where note 60 had too few cycles
        for dec in (20, 40, 60):
            v2 = bytearray(solo); v2[p7 + 11] = dec
            for k in range(2):
                s.append((f"feg7-dec{dec}-n{n}-{k}", None, h(v2) if k == 0 else None, [], n, 0, 1500, "feg"))
    return s


def segments13():
    """The thirteenth take: the phase a frequency EG leaves behind. Op 2 a key-synced sine at the note with a flat
    EG, op 3 the same with frequency EG init i (decay as PrA Hook's op 7), each alone in algorithm 1 at level 99
    and 90 so the fit can tell them apart. Op 3's phase against op 2's once the sweep is over is the sweep's
    integral mod 1, which sets where a key-synced modulator meets a formant's grains (Teck Hook's level)."""
    h = lambda b: bytes(b).hex()
    src = hook(); p7 = 112 + 6 * 62
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for init in (48, 36, 24, 12, -24):
        v = fp.init_voice("OpenFEGph ")
        fp.set_op(v, 1, keysync=1, level=99)
        fp.set_op(v, 2, keysync=1, level=90)
        q = 112 + 2 * 62
        v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, src[p7 + 10], src[p7 + 11]
        for k, n in enumerate((60, 72, 84)):
            s.append((f"fegph-i{init:+d}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1500, "fegph"))
    return s


def segments14():
    """The fourteenth take: the frequency EG's phase integral, to a hundredth of a cycle. Op 2 a key-synced sine
    at the note, flat; op 3 a key-synced sine at twice the note with frequency EG init i and attack time t (attack
    level 0, so the ramp runs straight back to the note). Once the ramp is over op 3's phase less twice op 2's is
    the cycles the ramp added, mod 1, with no sign ambiguity; at notes 60 and 72 the second doubles the first."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for init in (48, 36, 24, -24, -48):
        for t in (0, 10, 20):
            v = fp.init_voice("OpenFEGint")
            fp.set_op(v, 1, keysync=1, level=99)
            fp.set_op(v, 2, keysync=1, level=99, coarse=2)
            q = 112 + 2 * 62
            v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, t, 20
            for k, n in enumerate((60, 72)):
                s.append((f"fegint-i{init:+d}-t{t}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1200, "fegint"))
    return s


def segments15():
    """The fifteenth take: take 14's integral over a grid, to fit the start level and the speed apart. Eight
    start levels by six attack times, at notes 60, 72 and 84 (the added cycles double each octave)."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for init in (48, 36, 24, 12, -12, -24, -36, -48):
        for t in (0, 5, 10, 15, 20, 30):
            v = fp.init_voice("OpenFEGint")
            fp.set_op(v, 1, keysync=1, level=99)
            fp.set_op(v, 2, keysync=1, level=99, coarse=2)
            q = 112 + 2 * 62
            v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, t, 20
            for k, n in enumerate((60, 72, 84)):
                s.append((f"fegint-i{init:+d}-t{t}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1200, "fegint"))
    return s


def segments16():
    """The sixteenth take: the top of the start-level curve, where take 15's init +48 fell off the table's line.
    Inits 38 to 50 and -38 to -50, attack times 0 and 20, notes 60 and 72."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for init in (38, 40, 42, 44, 46, 48, 50, -40, -44, -48, -50):
        for t in (0, 20):
            v = fp.init_voice("OpenFEGint")
            fp.set_op(v, 1, keysync=1, level=99)
            fp.set_op(v, 2, keysync=1, level=99, coarse=2)
            q = 112 + 2 * 62
            v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, t, 20
            for k, n in enumerate((60, 72)):
                s.append((f"fegint-i{init:+d}-t{t}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1200, "fegint"))
    return s


def segments17():
    """The seventeenth take: the start level read directly. One key-synced sine at note 60 with a slow frequency
    EG (attack time 40, about 60 semitones a second), inits 20 to 50 up and down, so the pitch over the first
    100 ms is a line whose intercept is the start level and whose slope is the speed."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for init in (20, 30, 36, 40, 44, 46, 48, 49, 50, -20, -30):
        v = fp.init_voice("OpenFEGst ")
        fp.set_op(v, 1, keysync=1, level=99)
        q = 112 + 62
        v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, 40, 40
        s.append((f"fegst-i{init:+d}", None, h(v), [], 60, 0, 1500, "fegst"))
    return s


def segments18():
    """The eighteenth take: the frequency EG's speed per attack time, around time 10 (take 15 read rate 58 where
    FUN_0000EBE8 writes 57) and at the other times whose rate sits just over a step (24, 35, 49). Init +24, whose
    start level takes 14 and 15 put on the table to 0.01 semitone, notes 60, 72 and 84."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for t in (8, 9, 10, 11, 12, 13, 23, 24, 25, 34, 35, 36):
        v = fp.init_voice("OpenFEGint")
        fp.set_op(v, 1, keysync=1, level=99)
        fp.set_op(v, 2, keysync=1, level=99, coarse=2)
        q = 112 + 2 * 62
        v[q + 8], v[q + 9], v[q + 10], v[q + 11] = 24 + 50, 50, t, 20
        for k, n in enumerate((60, 72, 84)):
            s.append((f"fegint-i+24-t{t}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1200, "fegint"))
    return s


def segments19():
    """The nineteenth take: the frequency EG's speed at every attack time 0 to 50 (take 18 found times 8 to 10 off
    the rate FUN_0000EBE8 writes, and no rule in the ladder). Inits +12 and +24, whose integrals do not scale
    together, so a speed that is off by a whole cycle at one is not at the other; notes 60 and 72."""
    h = lambda b: bytes(b).hex()
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for t in range(51):
        for init in (12, 24):
            v = fp.init_voice("OpenFEGint")
            fp.set_op(v, 1, keysync=1, level=99)
            fp.set_op(v, 2, keysync=1, level=99, coarse=2)
            q = 112 + 2 * 62
            v[q + 8], v[q + 9], v[q + 10], v[q + 11] = init + 50, 50, t, 20
            for k, n in enumerate((60, 72)):
                s.append((f"fegint-i{init:+d}-t{t}-n{n}", None, h(v) if k == 0 else None, [], n, 0, 1200, "fegint"))
    return s


def segments20():
    """The twentieth take: when the pitch EG ticks against the note-on. A sine at note 84 with the pitch EG from
    level 0 to 50 over time 60, so it climbs in visible steps; sixteen strikes whose start drifts by 1 ms each
    against the 5.2 ms tick. If the steps sit at fixed times after each strike the tick runs from the note-on;
    if they drift with the strike they are the machine's free-running tick."""
    h = lambda b: bytes(b).hex()
    v = sine()
    v[0x1F], v[0x20], v[0x21], v[0x22], v[0x3E] = 0, 50, 50, 50, 50     # L0 0 (down), L1..L4 centre
    v[0x23], v[0x24], v[0x25], v[0x26] = 60, 0, 0, 0
    v[0x3B] = 1                                                        # range 2 octaves
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for k in range(16):
        s.append((f"peg-{k}", None, h(v) if k == 0 else None, [], 84, 0, 600, "peg"))
    return s


def teck_steps():
    """Teck Hook's part 1, alone and dry, taken one performance difference at a time toward take 11's plain part."""
    p = teck(solo=0, fx=False); b = 192; out = [("stored", bytes(p))]
    p[b + 0x07] = 0; out.append(("filter-off", bytes(p)))
    p[0x40:0x48] = bytes(8); out.append(("ctrl-sets-off", bytes(p)))
    p[b + 0x1C] = 64; out.append(("offset-1c", bytes(p)))
    p[b + 0x0E] = 64; out.append(("pan-centre", bytes(p)))
    p[0x10] = 127; out.append(("perf-vol-127", bytes(p)))
    p[b + 0x0B] = 127; out.append(("part-vol-127", bytes(p)))
    p[b + 0x24], p[b + 0x25] = 0, 0; out.append(("porta-off", bytes(p)))
    return out


def segments21():
    """The twenty-first take: which byte of Teck Hook's performance puts part 1 2.5 dB under the unit in FSVR when
    the same voice on a plain part matches. Each step of teck_steps at note 60, three strikes."""
    s = [("ref", fp.perf_bulk(dry()).hex(), bytes(sine()).hex(), [], 69, 0, 2000, "level")]
    for name, p in teck_steps():
        for k in range(3):
            s.append((f"tstep-{name}-{k}", fp.perf_bulk(p).hex() if k == 0 else None, None, [], 60, 0, 1500, "level"))
    return s


def segments22():
    """The twenty-second take: which of PrA Hook's filter settings plays 2 dB apart between the unit and FSVR.
    Teck Hook's part 1 alone, dry, its controller sets and portamento off and pan centred, filter on; the voice
    taken one filter byte at a time to neutral, three strikes each, with the part filter off at each end."""
    p = teck(solo=0, fx=False); b = 192
    p[0x40:0x48] = bytes(8); p[b + 0x0E] = 64; p[b + 0x24] = p[b + 0x25] = 0
    off = bytearray(p); off[b + 0x07] = 0
    v = hook(); steps = [("stored", bytes(v))]
    v[0x5B] = 64; steps.append(("keyscale-0", bytes(v)))
    v[0x64] = 64; steps.append(("egdepth-0", bytes(v)))
    v[0x55] = 16; steps.append(("reso-0", bytes(v)))
    v[0x5D] = 12; steps.append(("gain-0db", bytes(v)))
    v[0x54] = 0; steps.append(("lpf24", bytes(v)))
    s = [("ref", fp.perf_bulk(dry()).hex(), bytes(sine()).hex(), [], 69, 0, 2000, "level")]
    for k in range(3):
        s.append((f"fstep-off-{k}", fp.perf_bulk(off).hex() if k == 0 else None, hook().hex() if k == 0 else None, [], 60, 0, 1500, "level"))
    for i, (name, vv) in enumerate(steps):
        for k in range(3):
            s.append((f"fstep-{name}-{k}", fp.perf_bulk(p).hex() if i == 0 and k == 0 else None, vv.hex() if k == 0 else None, [], 60, 0, 1500, "level"))
    for k in range(3):
        s.append((f"fstep-off2-{k}", fp.perf_bulk(off).hex() if k == 0 else None, hook().hex() if k == 0 else None, [], 60, 0, 1500, "level"))
    return s


def segments23():
    """The twenty-third take: the filter loop's loss at a quiet input. One sine through LPF24 wide open, input gain
    0 dB, at part volumes 127, 100, 80, 64 and 40, the part filter on and off at each; then PrA Hook the same way.
    The loss was measured at full level (cal::FLT_LOSS); take 22 put Hook 1.1 dB louder filtered than not, with
    part volume 100, where FSVR loses 0.3."""
    h = lambda b: bytes(b).hex()
    lp = sine(); lp[0x54], lp[0x55], lp[0x57], lp[0x5D], lp[0x64] = 0, 16, 127, 12, 64
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for vname, v in (("sine", lp), ("hook", hook())):
        for vol in (127, 100, 80, 64, 40):
            for flt in (0, 1):
                p = dry(); fp.set_part(p, 0, panscale=50, poly=0, volume=vol, filter=flt)
                for k in range(2):
                    s.append((f"loss-{vname}-v{vol}-f{flt}-{k}", fp.perf_bulk(p).hex() if k == 0 else None,
                              h(v) if k == 0 else None, [], 60, 0, 1500, "level"))
    return s


def segments24():
    """The twenty-fourth take: cal::FLT_LOSS (1.23 dB, fitted off 06_filter_1/2 on 2026-09-19) against take 23,
    where a sine through LPF24 wide open lost nothing. The part filter on and off on the same voice, LPF24 at
    resonance 0, over cutoff (127, 100, 80) at note 48, input gain (bytes 0, 6, 12, 18, 24) at cutoff 127, and
    the 06 files' own source, an all1 operator at note 24, at cutoffs 127 and 96."""
    h = lambda b: bytes(b).hex()
    def lp(cut=127, gain=12, form=fp.SINE):
        v = fp.init_voice("OpenLoss  ")
        fp.set_op(v, 1, keysync=1, level=99, form=form)
        v[0x54], v[0x55], v[0x57], v[0x5D], v[0x64] = 0, 16, cut, gain, 64
        return v
    cases = [(f"cut{c}", lp(cut=c), 48) for c in (127, 100, 80)]
    cases += [(f"gain{g}", lp(gain=g), 48) for g in (0, 6, 18, 24)]
    cases += [(f"all1-cut{c}", lp(cut=c, form=fp.ALL1), 24) for c in (127, 96)]
    s = [("ref", fp.perf_bulk(dry()).hex(), h(sine()), [], 69, 0, 2000, "level")]
    for name, v, n in cases:
        for flt in (0, 1):
            p = dry(); fp.set_part(p, 0, panscale=50, poly=0, filter=flt)
            s.append((f"loss-{name}-f{flt}", fp.perf_bulk(p).hex(), h(v), [], n, 0, 1500, "level"))
    return s


def segments25():
    """The twenty-fifth take: Teck Hook's slow level movement on a held note (rgwan's 2026-10-23 recording had a
    0.4 Hz movement 5 dB weaker on the unit than in FSVR). The whole performance at note 60, held 12 s, twice, and
    once with its effects off; the level's modulation spectrum is read from 0.2 to 5 Hz."""
    s = [("ref", fp.perf_bulk(dry()).hex(), bytes(sine()).hex(), [], 69, 0, 2000, "level")]
    for name, p in (("fx", teck()), ("fx2", teck()), ("dry", teck(fx=False))):
        s.append((f"slow-{name}", fp.perf_bulk(p).hex(), None, [], 60, 0, 12000, "slow"))
    return s


def make(path, segs=None):
    m, t = fp.Smf(), 0
    rows = []
    for name, perf, voice, params, note, ch, hold, kind in segs or segments():
        if perf:
            m.at(t, bytes.fromhex(perf)); t += 1200
        if voice:
            m.at(t, fp.voice_bulk(bytes.fromhex(voice))); t += 600
        for al, val in params:
            m.at(t, fp.param(0x40, 0x00, al, val)); t += 100
        t = max(t, LEAD)
        rows.append({"name": name, "ms": t, "note": note, "hold": hold, "kind": kind})
        m.note(t, note, 100, hold, channel=ch)
        t += hold + (12000 if kind == "tail" else 250 if kind in ("phase", "pair") else 1500)
        if kind == "peg": t += 1                     # drift the strike 1 ms a time against the 5.2 ms tick
    m.write(path, tail_ms=14000)                  # the tail segment's 12 s of reverb, past the last note-off
    Path(path).with_suffix(".json").write_text(json.dumps(rows, indent=1))
    print(path, "written,", len(rows), "segments,", t // 1000, "s")


def load(path):
    import numpy as np
    if str(path).endswith(".raw"):
        x = np.fromfile(path, "<i4").reshape(-1, 2).astype(float) / 2 ** 31
    else:
        import soundfile
        x = soundfile.read(str(path), always_2d=True)[0]
    return x


def spec(y):
    import numpy as np
    w = np.hanning(len(y))
    F = np.abs(np.fft.rfft(y * w, 8 * len(y))) / w.sum() * 2
    return np.fft.rfftfreq(8 * len(y), 1 / SR), F


def peak(hz, F, f, tol):
    """The strongest line within tol of f, its frequency by parabolic interpolation and its level in dB."""
    import numpy as np
    i = np.flatnonzero((hz > f - tol) & (hz < f + tol)); k = i[np.argmax(F[i])]
    a, b, c = np.log(F[k - 1:k + 2] + 1e-30)
    d = 0.5 * (a - c) / (a - 2 * b + c)
    return hz[k] + d * (hz[1] - hz[0]), 20 * np.log10(F[k] + 1e-30)


def read(*paths):
    import numpy as np
    rows = json.loads(Path(paths[0]).read_text())
    db = lambda v: 20 * np.log10(v + 1e-30)
    for path in paths[1:]:
        x = load(path)
        z = int(np.argmax(np.abs(x).max(1) > 1e-4)) - rows[0]["ms"] * SR // 1000
        print(path)
        ref = None
        for r in rows:
            a = z + (r["ms"] + 300) * SR // 1000
            b = z + (r["ms"] + r["hold"] - 100) * SR // 1000
            y = x[a:b]
            f0 = 440 * 2 ** ((r["note"] - 69) / 12)
            lv = db(np.sqrt((y ** 2).mean()))
            if r["name"] == "ref":
                ref = lv
            out = f"  {r['name']:16s} level {lv - ref:+6.2f} dB"
            if r["kind"] == "fm":
                hz, F = spec(y[:, 0])
                _, c = peak(hz, F, f0, 5); _, s1 = peak(hz, F, 2 * f0, 5); _, s2 = peak(hz, F, 3 * f0, 5)
                out += f" | carrier {c:7.2f} dB, sidebands 2f {s1 - c:+6.2f} 3f {s2 - c:+6.2f}"
            elif r["kind"] == "rate":
                hz, F = spec(y[:, 0])
                f, l = peak(hz, F, 440, 20)
                out += f" | line {f:9.3f} Hz at {l:6.1f} dB"
            elif r["kind"] == "tail":
                end = z + (r["ms"] + r["hold"]) * SR // 1000
                t = x[end:end + 12 * SR]
                nz = np.flatnonzero(np.abs(t).max(1) > 0)
                out += f" | last non-zero sample {(nz[-1] + 1) / SR if len(nz) else 0:6.2f} s after the release"
            elif r["kind"] == "phase":   # the 1 Hz sine's phase at the strike: y = a sin + b cos over the hold
                n = np.arange(len(y)) / SR + 0.3
                w = 2 * np.pi * 1.0000760505119806 * n
                a_, b_ = np.linalg.lstsq(np.c_[np.sin(w), np.cos(w)], y[:, 0], rcond=None)[0]
                out += f" | start phase {np.arctan2(b_, a_) / (2 * np.pi) % 1:.3f} turn"
            elif r["kind"] == "feg":     # the pitch over the onset, cents off the settled pitch, 10 ms windows
                st = z + r["ms"] * SR // 1000
                on = st - 4800 + int(np.argmax(np.abs(x[st - 4800:st + 9600, 0]) > 1e-3))   # the strike, off the audio
                def f_at(t, w=240):
                    yy = x[on + int(t * SR) - w // 2:on + int(t * SR) + w // 2, 0]
                    zc = np.flatnonzero((yy[:-1] < 0) & (yy[1:] >= 0))
                    if len(zc) < 2: return float("nan")
                    fr = zc + yy[zc] / (yy[zc] - yy[zc + 1])
                    return (len(fr) - 1) * SR / (fr[-1] - fr[0])
                fe = f_at(1.2, 9600)
                out += f" | {fe:8.3f} Hz settled; cents at ms " + " ".join(
                    f"{t * 1000:.0f}:{1200 * np.log2(f_at(t) / fe):+.0f}" for t in (0.003, 0.006, 0.009, 0.012, 0.015, 0.02, 0.03, 0.04, 0.06, 0.08, 0.1, 0.15, 0.2))
            elif r["kind"] == "fegph":   # two sines on one frequency: fit a and b, op 3 the smaller (level 90)
                n = np.arange(len(y)) / SR
                hz, F = spec(y[:, 0]); f, _ = peak(hz, F, f0, 5)
                X = np.c_[np.sin(2 * np.pi * f * n), np.cos(2 * np.pi * f * n)]
                c = np.linalg.lstsq(X, y[:, 0], rcond=None)[0]
                # the sum of two equal-frequency sines is one sine: amplitude A, from A^2 = a^2 + b^2 + 2ab cos(d)
                A = np.hypot(*c); a2 = 10 ** (-0 / 20); b2 = 10 ** ((90 - 99) * -0.0 / 20)
                out += f" | sum {db(A):7.2f} dB"
            elif r["kind"] == "fegint":  # op 3 (2 f) less twice op 2 (f), fitted together over the settled hold
                n = np.arange(len(y)) / SR
                hz, F = spec(y[:, 0]); f, _ = peak(hz, F, f0, 5)
                X = np.c_[[g(2 * np.pi * m * f * n) for m in (1, 2) for g in (np.sin, np.cos)]].T
                c = np.linalg.lstsq(X, y[:, 0], rcond=None)[0]
                p1, p2 = np.arctan2(c[1], c[0]) / (2 * np.pi), np.arctan2(c[3], c[2]) / (2 * np.pi)
                out += f" | added {(p2 - 2 * p1) % 1:.3f} cycle"
            elif r["kind"] == "fegst":   # pitch in semitones off the note, 4 ms windows from the strike; line fit 5..60 ms
                on = z + r["ms"] * SR // 1000 - 4800
                on += int(np.argmax(np.abs(x[on:on + 14400, 0]) > 1e-3))
                ts, st = [], []
                for k in range(1, 40):
                    a0 = on + int(k * 0.002 * SR); yy = x[a0:a0 + 480, 0]
                    zc = np.flatnonzero((yy[:-1] < 0) & (yy[1:] >= 0))
                    if len(zc) < 3: continue
                    fr = zc + yy[zc] / (yy[zc] - yy[zc + 1])
                    ts.append((a0 + 240 - on) / SR); st.append(12 * np.log2((len(fr) - 1) * SR / (fr[-1] - fr[0]) / f0))
                ts, st = np.array(ts), np.array(st); m = ts < 0.06
                if m.sum() < 3: out += " | too few cycles"; print(out); continue
                b1, b0 = np.polyfit(ts[m], st[m], 1)
                out += f" | start {b0:+7.2f} st, slope {b1:+7.1f} st/s, first {st[0]:+6.2f} at {ts[0] * 1000:.0f} ms"
            elif r["kind"] == "peg":     # times (ms after the strike) where the pitch steps by more than 3 cents
                on = int(z + r["ms"] * SR / 1000) - 2400
                on += int(np.argmax(np.abs(x[on:on + 9600, 0]) > 1e-3))
                yy = x[on:on + int(0.12 * SR), 0]
                zc = np.flatnonzero((yy[:-1] < 0) & (yy[1:] >= 0)); fr = zc + yy[zc] / (yy[zc] - yy[zc + 1])
                per = np.diff(fr); c = 1200 * np.log2(per[:-1] / per[1:])
                steps = [round(float(fr[i + 1]) / SR * 1000, 2) for i in np.flatnonzero(np.abs(c) > 3)]
                out += f" | steps at ms {steps[:12]}"
            elif r["kind"] == "slow":    # the level in 20 ms frames, its modulation spectrum's three strongest lines
                fr = int(0.02 * SR); m = len(y) // fr
                lvl = 10 * np.log10((y[:m * fr] ** 2).sum(1).reshape(m, fr).mean(1) + 1e-30)
                lvl = lvl - lvl.mean(); w = np.hanning(m); N = 1 << 14
                M = np.abs(np.fft.rfft(lvl * w, N)) / w.sum() * 2; hz = np.fft.rfftfreq(N, 0.02)
                band = (hz > 0.2) & (hz < 5); k = np.flatnonzero(band); top = []
                for i in k[np.argsort(M[k])[::-1]]:
                    if all(abs(hz[i] - h0) > 0.15 for h0, _ in top): top.append((hz[i], M[i]))
                    if len(top) == 3: break
                out += " | " + ", ".join(f"{f:.2f} Hz {a:.2f} dB" for f, a in top) + f"; spread {np.ptp(lvl):.1f} dB"
            elif r["kind"] == "pair":    # op 3's phase when op 2's is 0: y = sum of the two, fitted together
                n = np.arange(len(y)) / SR
                f2, f3 = 440 * 2 ** (7 - 16 + 28 / 128), 440 * 2 ** (8 - 16 + 103 / 128)
                X = np.c_[[f(2 * np.pi * fr * n) for fr in (f2, f3) for f in (np.sin, np.cos)]].T
                c = np.linalg.lstsq(X, y[:, 0], rcond=None)[0]
                p2, p3 = np.arctan2(c[1], c[0]) / (2 * np.pi), np.arctan2(c[3], c[2]) / (2 * np.pi)
                t0 = -p2 / f2                       # op 2's phase 0 (the strike) against the window's start
                out += f" | op 3 at the strike {(p3 + f3 * t0) % 1:.3f} turn"
            elif r["name"].startswith("teck"):
                out += f" | L {db(np.sqrt((y[:, 0] ** 2).mean())) - ref:+6.2f} R {db(np.sqrt((y[:, 1] ** 2).mean())) - ref:+6.2f}"
            print(out)


def demo():
    v = fm(corr_mod=15)
    assert v[0x2C] == 7 and v[0x2D] == 15 and v[112 + 22] == 85 and v[112 + 62 + 22] == 99
    p = teck(solo=2, fx=False)
    assert p[:9] == b"Teck Hook" and p[192 + 2 * 52 + 4] == 0 and p[192 + 4] == 0x7F and p[80 + 0x5F] == 0
    n = noise()
    q = 112 + 62 + 35
    assert n[q + 11] == 99 and n[q + 6] >> 3 == 7 and n[q + 1] & 0x1F == 16
    assert fp.param(0x40, 0, 0x2D, 15)[4:9] == bytes([0x40, 0, 0x2D, 0, 15])


if __name__ == "__main__":
    demo()
    if sys.argv[1] == "--read":
        read(*sys.argv[2:])
    elif sys.argv[1] == "--take2":
        make(sys.argv[2], segments2())
    elif sys.argv[1] == "--take3":
        make(sys.argv[2], segments3())
    elif sys.argv[1] == "--take4":
        make(sys.argv[2], segments4())
    elif sys.argv[1] == "--take5":
        make(sys.argv[2], segments5())
    elif sys.argv[1] == "--take6":
        make(sys.argv[2], segments6())
    elif sys.argv[1] == "--take7":
        make(sys.argv[2], segments7())
    elif sys.argv[1] == "--take8":
        make(sys.argv[2], segments8())
    elif sys.argv[1] == "--take9":
        make(sys.argv[2], segments9())
    elif sys.argv[1] == "--take10":
        make(sys.argv[2], segments10())
    elif sys.argv[1] == "--take11":
        make(sys.argv[2], segments11())
    elif sys.argv[1] == "--take12":
        make(sys.argv[2], segments12())
    elif sys.argv[1] == "--take13":
        make(sys.argv[2], segments13())
    elif sys.argv[1] == "--take14":
        make(sys.argv[2], segments14())
    elif sys.argv[1] == "--take15":
        make(sys.argv[2], segments15())
    elif sys.argv[1] == "--take16":
        make(sys.argv[2], segments16())
    elif sys.argv[1] == "--take17":
        make(sys.argv[2], segments17())
    elif sys.argv[1] == "--take18":
        make(sys.argv[2], segments18())
    elif sys.argv[1] == "--take19":
        make(sys.argv[2], segments19())
    elif sys.argv[1] == "--take20":
        make(sys.argv[2], segments20())
    elif sys.argv[1] == "--take21":
        make(sys.argv[2], segments21())
    elif sys.argv[1] == "--take22":
        make(sys.argv[2], segments22())
    elif sys.argv[1] == "--take23":
        make(sys.argv[2], segments23())
    elif sys.argv[1] == "--take24":
        make(sys.argv[2], segments24())
    elif sys.argv[1] == "--take25":
        make(sys.argv[2], segments25())
    else:
        make(sys.argv[1])

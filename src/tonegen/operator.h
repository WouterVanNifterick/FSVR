// tonegen/operator.h - one operator: its waveforms, grains and level. INFERRED: the constants are in fs1r/cal.h.
#pragma once
#include <algorithm>
#include <cmath>
#include "fs1r/hardware.h"
#include "fs1r/cal.h"
#include "math/fastmath.h"
#include "presets/voice.h"
#include "tonegen/envelope.h"

// The chip's LFO amplitude modulation, measured off 14_sens (three LFO depths by eight sensitivities):
// the channel AM word is 7 bits, so it floors at 127 steps (48 dB) where the CPU can send 255, and the
// per-operator sensitivity weights it {0,1,2,4,5,6,7,8}/8, not k/7. docs/findings.md 2026-09-23.
static const int AMS_W[8] = {0, 1, 2, 4, 5, 6, 7, 8};
static inline double am_att(int regAM, int ams) { return std::min(regAM, 127) * LEVEL_DB * AMS_W[ams & 7] / 8.0; }
// One pole per sample coefficient for the voiced level register, cal::LEVEL_SLEW_MS as a rate.
const double LEVEL_SLEW_K = 1.0 - exp(-1.0 / (cal::LEVEL_SLEW_MS * 0.001 * SR));
// 1024 units per octave, and the word is a 16 bit register that saturates rather than wrapping.
// MEASURED 2026-09-21 off 12_fseqlevel: a ratio operator driven by an Fseq lands past the ceiling at
// every note and the unit answers with one line at 23982 Hz, which is word 32767 through the phase
// generator below (op_increment), 23982.42 Hz. This smooth law reads 23969.9 there and plays the
// channel's own rates, the formant window and the noise centre, which nothing has measured that way.
static inline double word_hz(double w) { return 440.0 * fm::exp2((std::clamp(w, 0.0, 32767.0) - 26861) / 1024.0); }   // fractional for the bend LSB

// ------------------------------------------------------------------------------------------ tables (chip side)
extern float g_win[2][8][1025];
extern float g_winDC[2][8];
extern uint16_t g_opMant[1024];   // the phase generator's mantissas, floor(2048 * 2^(i / 1024)) (cal.h)
void init_tables();

// An operator's phase increment for a frequency word, in units of 2^-cal::OP_PHASE_BITS of a cycle a sample
// (MEASURED, cal.h). A word with a fraction, which the unit's never has (FSVR's 14-bit bend, the frequency
// EG's ramp), takes the same law at its fractional place. INFERRED: between two words there is nothing to
// measure on the unit.
static inline uint32_t op_increment(double w) {
    w = std::clamp(w, 0.0, 32767.0);
    const int wi = (int)w, i = wi & 1023; const double fr = w - wi;
    const uint32_t M = fr == 0.0 ? g_opMant[i] : (uint32_t)(cal::OP_MANT_ONE * fm::exp2((i + fr) / 1024.0));
    const int s = (wi >> 10) - cal::OP_OCTAVE0;
    return s >= 0 ? M << s : M >> -s;
}
// Ours: the phase is held at 2^32 to a cycle so that it wraps by itself; an increment in those units, and in hertz.
static inline uint32_t op_inc32(uint32_t inc) { return inc << (32 - cal::OP_PHASE_BITS); }
static inline double op_hz(uint32_t inc) { return inc * (double)SR / (double)(1u << cal::OP_PHASE_BITS); }
// The sine and the level path go through math/fastmath.h, which is where the backend is chosen.
static inline double fsin(double ph) { return fm::sin_turns(ph); }
static inline float fwin(int fam, int s, double x) { double y = x * 1024.0; int i = (int)y; if (i >= 1024) return 0.f; float f = (float)(y - i); const float* w = g_win[fam][s]; return w[i] + (w[i + 1] - w[i]) * f; }
// The per-sample level path: anything at or under -128 dB is silence, the rest is the backend's.
static inline double db2lin_fast(double db) { return db <= -128 ? 0.0 : fm::db2lin(db); }

// A grain runs at the carrier frequency it was fired with, latched beside the carrier phase the grain
// already resets. The level is not latched with it, it follows cal::LEVEL_SLEW_MS. MEASURED 2026-09-20
// and 2026-09-21, see op_sample.
struct WinGen { double w = 1.0, c = 0.0, fc = 0.0; bool on = false; };
struct OpState {
    uint32_t phase = 0;           // 2^32 to a cycle, stepped by op_inc32(inc): the phase generator (cal.h)
    WinGen g[2]; int nextGen = 0; double fphase = 0; int halfCount = 0;
    double word = 0; uint32_t inc = 0;   // the operator's frequency word and its increment (refresh_ctl)
    EG eg; FreqEG feg;
    EG ueg; FreqEG ufeg; double nphase = 0; double lp[8] = {}; uint32_t rng = 0x12345678;
    double att = 0, attS = 0, fop = 0, fw = 0, wl7 = 1, uatt = 0, nf = 0, na = 1, na2 = 1, nscale = 0, nres = 0, uprev = 0; int bw = 0, ratio = 0;   // refresh_ctl
    double gain = 0, gto = 0, dg = 0, ugain = 0, ugto = 0, dug = 0;   // the Fast steps' gain ramps, voiced and unvoiced (render_chan)
    double out = 0;                                                   // this sample's voiced output, for tools/wavetrace
};

// A grain runs at the carrier frequency it was fired with. The chip resets the carrier phase at every
// grain, and the frequency it then runs at is the one latched with that reset, so a write lands on the
// grain after it rather than bending the one in flight.
//
// `gain` is the operator's level and it is not latched here: it arrives already slewed, one pole at
// cal::LEVEL_SLEW_MS. MEASURED 2026-09-20 and 2026-09-21 against two recordings that disagree with
// each other unless the smoothing is a fixed time. Vokodrone's intro is part 4 alone under a 35.6 Hz
// Fseq, and a level applied per sample cut the grain in flight there, stepping the operator output
// from 0.149 to 0.033 between two samples: 12.3 dB of click above its own floor at the frame
// boundary where rgwan's recording has 1.2. Latching the level to the grain fixed that and went too
// far. 12_fseqlevel drives one formant operator with an Fseq over five octaves and three frame
// rates, and a latched level is 10.6 dB of octave band error against the unit, worse than the 9.7
// of no smoothing at all: at a low fundamental the grain is tens of milliseconds and the level
// cannot wait that long. A 1.6 ms glide is 4.74 dB, and still holds Vokodrone's boundary to 2.7 dB.
//
// The unvoiced operator's level is left stepping. Its own segments in the same file sit 15 to 26 dB
// dark above 640 Hz before any smoothing is applied, which is the noise formant's bandwidth law and
// not this, so they cannot judge a glide; slewing it there only makes them darker.
//
// The sine and the stored waves read the phase generator's own integer phase, stepped by `inc` (cal.h); the
// grain forms run their carriers at `fop`, the same increment in hertz.
inline double op_sample(OpState& s, const OpV& v, double f0, double fop, uint32_t inc, int ratio, double pm, double gain) {
    if (v.form <= 2) {
        s.phase += op_inc32(inc);
        const double ph = s.phase * (1.0 / 4294967296.0);
        if (v.form == 0) return gain * fsin(ph + pm);
        // all1 and all2 are a stored waveform read by phase, the same way the sine is: one period of
        // the skirt's window with its mean taken out, so a modulator reaches it as phase like any other
        // form. MEASURED 2026-09-24 off the skirt sweep's sixty partials: all2's lines are the symmetric
        // window's own Fourier row C(p, p/2 - k) to 0.4 to 0.7 dB at every skirt, and all1's the
        // asymmetric window's to 0.4 to 1.3, where the two-period grain under a carrier at fop that this
        // replaces was 36 to 66 dB out by skirt 7. At skirt 0 the DC-free sin^2 is -cos / 2, the single
        // partial the unit gives, and the mean is gone because the unit's is: every all1/all2 take reads
        // mean / rms under 0.03 where a raw sin^2 pulse train sits at 0.7. docs/skirt.md.
        double x = ph + pm; x -= floor(x);
        return gain * cal::FORM_LEVEL * 2.0 * (fwin(v.form == 1, v.skirt, x) - g_winDC[v.form == 1][v.skirt]);
    }
    double fw, fc, wl;
    if (v.form == 7) { fw = f0; fc = fop; wl = s.wl7; }                                                          // formant: window at the fundamental (INFERRED bw curve)
    // odd1/odd2 are a group that starts at the operator's own frequency, so they are the same windowed
    // carrier the formant is, with the carrier at fop. MEASURED: the unit gives partials 1 and 3 on odd1
    // and odd2, where the fixed quarter-period window at DC this replaces gave a full harmonic series.
    // Retriggering at twice fop is what leaves the odd partials: a grain train at that spacing under a
    // carrier at fop puts its lines at fop, 3 fop, 5 fop and nowhere else.
    //
    // The window is FIXED, and the bandwidth has nothing to do with it. MEASURED on 2026-09-19 by
    // sweeping register 0x218 through all hundred values on each form in turn: the formant's window
    // opens and every one of the other six is flat to the byte, width and peak both. Voice byte 6 does
    // reach those forms, but as register 0x230, which the Data List calls the "freq. ratio of band
    // spectrum" and which the voice image confirms carries the raw byte for them and the formant
    // transpose word for the formant. What it does there is unmeasured: the sweep meant to settle it
    // wrote 0x218. INFERRED, FS1R.unlock/docs/unknowns.md experiment 9.
    // res1 and res2 put a three-partial group on harmonic ratio + 1 and are otherwise identical.
    // MEASURED on 2026-09-19 by sweeping register 0x230 through all hundred values on each: the peak
    // lands on partial ratio + 1 at every one of the hundred, on both forms, with the two partials
    // either side 6 dB down and everything else at the noise floor, and the peak level flat across
    // the whole range. The 1 + ratio * 31 / 99 this replaces topped out at 32 times the fundamental
    // where the unit reaches 100. docs/formant.md.
    // The window is one grain period for the odd and resonant forms; the "1" and "2" of each pair
    // differ in the window's shape (g_win), not its length. MEASURED off the sideband ratios, which a
    // sin^2 window fixes exactly: a grain exactly one period long makes the two partials either side
    // of the peak 6.02 dB down and kills everything beyond them, which is what the unit gives res1
    // and res2 at every one of a hundred settings.
    else if (v.form == 5 || v.form == 6) { fw = fop; fc = fop * (ratio + 1); wl = 1.0; }
    else { fw = 2 * fop; fc = fop; wl = 1.0; }
    wl = std::min(wl, 2.0);
    s.fphase += fw / SR;
    if (s.fphase >= 1) {
        s.fphase -= 1; WinGen& g = s.g[s.nextGen]; g.on = true; g.w = 0; g.fc = fc;
        // The formant restarts its carrier every grain, which is what holds its peak still while the
        // fundamental moves. The odd forms retrigger twice a period, so restarting there would put
        // half a cycle of alternation into the grain train and land the group on the even partials;
        // half a cycle back is the same carrier running on.
        g.c = (v.form == 3 || v.form == 4) && (s.halfCount & 1) ? 0.5 : 0.0;
        s.nextGen ^= 1; s.halfCount++;
    }
    double winc = fw / (wl * SR), y = 0;
    // The "1" forms and the formant take the asymmetric window, the "2" forms the symmetric one.
    int fam = v.form == 3 || v.form == 5 || v.form == 7;
    for (int k = 0; k < 2; k++) {
        WinGen& g = s.g[k]; if (!g.on) continue;
        g.w += winc; if (g.w >= 1) { g.on = false; continue; }
        y += gain * fwin(fam, v.skirt, g.w) * fsin(g.c + pm); g.c += g.fc / SR;
    }
    if (v.form == 7) { if (cal::FRMT_NORM != 0.0) y *= pow(1.0 / wl, cal::FRMT_NORM); }
    else y *= cal::FORM_LEVEL * 2.0 / wl;    // every form but sine and frmt; a shorter grain carries less
    return y;
}

// Ours, for a display: voiced operator v alone through op_sample, n points over two periods (operator.cpp).
void op_wave(const OpV& v, float* out, int n);

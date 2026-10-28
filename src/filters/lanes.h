// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// filters/lanes.h - the sixteen voices of a filter bank side by side, a number a row, for the lane kernel. Ours.
//
// The same words and state VoiceFilter (voice_filter.h) keeps for one voice, laid out so that a register loads a
// number of W slots at once (kernel/simd.h); the kernel computes what VoiceFilter::run computes, to the bit. This
// header holds plain data only: a kernel file is compiled once per instruction set and must inline nothing of the
// engine's (kernel/README.md).
#pragma once
#include <cmath>
#include <cstdint>

namespace fsvr::filters {

// The program's global constants, the cells it loads in its first sixteen steps.
struct Shared { double eg_zero, c61, c62, c63; };

struct Lanes {
    static constexpr int N = 16;
    enum Tap { LPF24, LPF18, LPF12, HPF, BPF, BEF, TAPS };

    // ---- the coefficients, as the host writes them ----
    double eg_depth[N] = {}, lfo_depth[N] = {}, cutoff[N] = {};   // the cutoff word and what moves it
    double gain[N] = {};                                          // the input gain
    double type_k[N] = {};                                        // 0 for the six types, the type byte past them
    double reso_a[N] = {}, reso_b[N] = {};                        // the two resonance words
    double ramp[N] = {};                                          // the output ramp's step a sample
    double glide[N] = {};                                         // ours: a one-pole on the cutoff word, 0 is the unit's step
    double exp_slope[N] = {}, reso_bias[N] = {}, fb_scale[N] = {};   // the program's own constants
    double stage_k[N] = {}, damp_k[N] = {}, pole_k[N] = {};
    double route[N];                                              // the output's scale, x1 for the bandpass and x2 otherwise
    uint64_t tap[TAPS][N];                                        // every bit set where the slot's type reads that tap

    // ---- the state ----
    double x1[N] = {};                                            // the input a sample ago
    double s2[N] = {}, s3[N] = {};                                // the resonant two-pole section
    double s4[N] = {}, s5[N] = {};                                // the two one-poles after it
    double level[N] = {};                                         // the output ramp
    double cutoff_s[N];                                           // the cutoff word as glided; NaN snaps to the word
    double kf[N] = {}, kb[N] = {}, kp[N] = {}, memo_fc[N];        // the cutoff word's coefficients and the word they were made for

    Lanes() {
        for (int s = 0; s < N; s++) {
            route[s] = 1; cutoff_s[s] = NAN; memo_fc[s] = NAN;
            for (int t = 0; t < TAPS; t++) tap[t][s] = t == BPF ? ~0ull : 0;   // as the unit boots, before the host sets a type
        }
    }
};

}  // namespace fsvr::filters

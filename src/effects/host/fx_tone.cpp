// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The tone effects: the 3-band EQ of the variation and the insertion, and the insertion's Lo-Fi.
#include "effects/host/fx_common.h"

namespace fs1r::vop3host {

namespace {

// What the 3-band EQ of a block writes to: each band, and the other channel's copy
struct ThreeBand { const uint16_t *low, *lowCopy, *mid, *midCopy, *high, *highCopy; };
const ThreeBand THREE_BAND[2] = {
    {var::EQ3_LOW_SHELF2, var::EQ3_LOW_SHELF2_COPY, var::EQ3_PEAK, var::EQ3_PEAK_COPY, var::EQ3_HIGH_SHELF2, var::EQ3_HIGH_SHELF2_COPY},
    {ins::EQ7_A,          ins::EQ7_A_C,             ins::EQ7_B,    ins::EQ7_B_C,       ins::EQ7_C,           ins::EQ7_C_C},
};

}  // namespace

void EffectHost::threeBandEq(int w, int p)
{
    enum { LOW_GAIN, MID_FREQ, MID_GAIN, MID_Q, HIGH_GAIN, LOW_FREQ, HIGH_FREQ, MODE = 14 };
    const ThreeBand& b = THREE_BAND[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LOW_GAIN: case LOW_FREQ:
            designFilter(2, 1, LOW_SHELF2, P[LOW_FREQ], int8_t(P[LOW_GAIN]), 0, b.low);
            copyCoefs(5, b.low, b.lowCopy);
            break;
        case MID_FREQ: case MID_GAIN: case MID_Q:
            designFilter(2, 1, PEAK, P[MID_FREQ], P[MID_GAIN], P[MID_Q], b.mid);
            copyCoefs(5, b.mid, b.midCopy);
            break;
        case HIGH_GAIN: case HIGH_FREQ:
            designFilter(2, 4, HIGH_SHELF2, P[HIGH_FREQ], P[HIGH_GAIN], 0, b.high);
            copyCoefs(5, b.high, b.highCopy);
            break;
        case MODE: {
            // the input mono or stereo: the two blocks' programs take it each their own way
            const bool stereo = P[MODE] != 0;
            if (w == VARIATION) {
                const uint16_t* a = var::EQ3_A;
                const uint16_t* c = var::EQ3_B;
                const uint16_t x = typeGain(w, QUARTER);
                coef[a[0]] = stereo ? HALF : QUARTER;
                coef[a[1]] = stereo ? 0 : QUARTER;
                coef[a[2]] = HALF;
                coef[c[0]] = x;
                coef[c[1]] = stereo ? 0 : x;
                coef[c[2]] = stereo ? x : 0;
                writeCoefs(2, a);
                writeCoefs(2, c + 1);
            } else {
                const uint16_t* m = ins::PC_MIX;
                coef[m[0]] = stereo ? HALF : 0;
                coef[m[1]] = stereo ? 0 : QUARTER;
                coef[m[2]] = stereo ? HALF : QUARTER;
                coef[m[3]] = typeGain(w, stereo ? QUARTER : 0);
                coef[ins::EQ3_LEVEL[0]] = typeGain(w, stereo ? 0 : QUARTER);
                writeCoefs(3, m);
                writeCoefs(1, m + 3);
                writeCoefs(1, ins::EQ3_LEVEL);
            }
            break;
        }
        case ALL:
            if (w == INSERTION)
                coef[ins::PC_MIX[4]] = typeGain(w, QUARTER);
            applyAll(w, &EffectHost::threeBandEq, {LOW_GAIN, MID_FREQ, HIGH_GAIN, MODE});
            break;
    }
}

void EffectHost::loFi(int w, int p)
{
    enum { SAMPLE_FREQ, WORD_LENGTH, OUTPUT_GAIN, LPF_CUTOFF, FILTER, LPF_RESONANCE, BIT_ASSIGN, EMPHASIS, DRY_WET = 9 };
    const uint16_t* P = param[w];
    switch (p) {
        case SAMPLE_FREQ:
            coef[ins::LOFI_RATE[0]] = P[SAMPLE_FREQ];
            writeCoefs(1, ins::LOFI_RATE);
            break;
        case WORD_LENGTH: {
            // a step a value, the step doubling every sixteen values from 48
            const uint32_t v = P[WORD_LENGTH];
            const int32_t s = int16_t(v);
            uint32_t bits;
            if (v < 48)       bits = uint32_t(s + 15);
            else if (v < 64)  bits = uint32_t(((s - 47) << 1) + 62);
            else if (v < 80)  bits = uint32_t(((s - 63) << 2) + 94);
            else if (v < 96)  bits = uint32_t(((s - 79) << 3) + 158);
            else if (v < 112) bits = uint32_t(((s - 95) << 4) + 286);
            else if (v < 127) bits = uint32_t(((s - 111) << 5) + 542);
            else              bits = UNITY;
            coef[ins::LOFI_BITS[0]] = bits;
            writeCoefs(1, ins::LOFI_BITS);
            break;
        }
        case LPF_CUTOFF: case LPF_RESONANCE:
            designFilter(2, 1, LPF2, P[LPF_CUTOFF], 64, P[LPF_RESONANCE], ins::LOFI_LPF2);
            break;
        case FILTER: {
            // three filters by the filter type, each row its shape, frequency, gain and Q
            const int8_t* row = rom::LOFI_FILTERS + uint32_t(int8_t(10 * int8_t(P[FILTER])));
            designFilter(1, 1, row[0], row[1], row[2], 0, ins::LOFI_FILTER);
            designFilter(2, 2, row[3], row[4], row[5], row[6], ins::LOFI_FILTER_2);
            designFilter(1, 2, row[7], row[8], row[9], 0, ins::LOFI_FILTER_3);
            break;
        }
        case EMPHASIS:
            if (P[EMPHASIS] == 1) {
                designFilter(1, 2, HIGH_SHELF1, 44, 76, 0, ins::LOFI_HIGH_SHELF);
                designFilter(1, 2, HIGH_SHELF1, 44, 52, 0, ins::LOFI_HIGH_SHELF_2);
            } else {
                designFilter(1, 2, GAIN_ONLY, 0, 64, 0, ins::LOFI_HIGH_SHELF);
                designFilter(1, 2, GAIN_ONLY, 0, 64, 0, ins::LOFI_HIGH_SHELF_2);
            }
            break;
        case OUTPUT_GAIN: case BIT_ASSIGN: case DRY_WET: {
            const int32_t dry = rom::PAN_LAW[128 - P[DRY_WET]] >> 1, wet = rom::PAN_LAW[P[DRY_WET]] >> 1;
            coef[ins::LOFI_WET[0]] = dry;
            coef[ins::LOFI_WET[1]] = dry;
            coef[ins::LOFI_WET2[0]] = typeGain(w, wet);
            coef[ins::LOFI_WET2[1]] = typeGain(w, wet);
            // the bit assign picks the output's level: up to full, or at full with another level row entry
            const uint32_t assign = P[BIT_ASSIGN];
            const int16_t* law = rom::LOFI_LAW + P[OUTPUT_GAIN];
            int32_t level = law[0], out;
            switch (assign) {
                case 0:  out = 2048;           break;
                case 1:  out = 4096;           break;
                case 2:  out = 8192;           break;
                case 3:  out = 16384;          break;
                case 4:  out = UNITY;          break;
                case 5:  out = UNITY; level = law[6];  break;
                case 6:  out = UNITY; level = law[12]; break;
                default: out = wet;            break;
            }
            coef[ins::LOFI_LEVEL[0]] = level;
            coef[ins::LOFI_OUT[0]] = out;
            coef[ins::LOFI_GAIN[0]] = coef[ins::LOFI_GAIN2[0]] = (32767 >> assign) >> 1;
            writeCoefs(1, ins::LOFI_GAIN);
            writeCoefs(1, ins::LOFI_GAIN2);
            writeCoefs(2, ins::LOFI_WET);
            writeCoefs(2, ins::LOFI_WET2);
            writeCoefs(1, ins::LOFI_LEVEL);
            writeCoefs(1, ins::LOFI_OUT);
            break;
        }
        case ALL:
            applyAll(w, &EffectHost::loFi, {SAMPLE_FREQ, WORD_LENGTH, OUTPUT_GAIN, LPF_CUTOFF, FILTER, EMPHASIS});
            break;
    }
}

}  // namespace fs1r::vop3host

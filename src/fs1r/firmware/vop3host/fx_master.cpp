// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Window reset, master EQ, system parameters and the master gain.
#include "effect_host.h"
#include <algorithm>
#include <iterator>

namespace fs1r::vop3host {

using namespace rom::mix;

// Silence a window: zero its constants, spread its delay slots evenly over its area, and load
// the program that silences it.
void EffectHost::clearWindow(int kind)
{
    auto zero = [&](int w) {
        for (int i = 0; i < WINDOWS[w].nsteps; i++)
            coef[WINDOWS[w].steps[i]] = 0;
        writeCoefs(WINDOWS[w].nsteps, WINDOWS[w].steps);
    };
    // slot i of n at (span / n) * (n - 1 - i)
    auto spread = [&](const auto& slots, uint32_t span) {
        const uint32_t n = uint32_t(std::size(slots));
        for (uint32_t i = 0; i < n; i++)
            delay[slots[i]] = span / n * (n - 1 - i);
        writeDelays(n, slots);
    };
    switch (kind & 0xFF) {
        case REVERB:
            zero(REVERB);
            spread(rom::RESET_SLOTS_REVERB, AREA[1] - AREA[0]);
            break;
        case INSERTION:
            zero(INSERTION);
            spread(rom::RESET_SLOTS_INSERTION, AREA[3] - AREA[2]);
            uploadProgram(INSERTION, 1);
            break;
        case REVERB_DELAY:
            zero(REVERB_DELAY);
            spread(rom::RESET_SLOTS_REVERB_DELAY, AREA[4] - AREA[3]);
            uploadProgram(REVERB_DELAY, 1);
            break;
        case VARIATION:
            zero(VARIATION);
            spread(rom::RESET_SLOTS_VARIATION, TOP - 1 - AREA[4]);
            uploadProgram(VARIATION, 1);
            break;
    }
}

// The performance EQ, one biquad per band through designFilter, then the band's five words
// copied to the other channel's steps. p picks the band by its parameters: 0-3 low, 4-6
// low-mid, 8-10 mid, 12-14 high-mid, 16-19 high.
void EffectHost::masterEq(int p)
{
    auto eq = [&](int i) -> int32_t { return eqParam[i]; };
    if (p <= 3) {
        designFilter(2, 2, eq(3) == 0 ? LOW_SHELF1 : PEAK, eq(1), int8_t(eqParam[0]), eq(2), EQ_LOW);
        copyCoefs(5, EQ_LOW, EQ_LOW_COPY);
    } else if (p >= 4 && p <= 6) {
        designFilter(2, 2, PEAK, eq(5), eq(4), eq(6), EQ_LOW_MID);
        copyCoefs(5, EQ_LOW_MID, EQ_LOW_MID_COPY);
    } else if (p >= 8 && p <= 10) {
        designFilter(2, 2, PEAK, eq(9), eq(8), eq(10), EQ_MID);
        copyCoefs(5, EQ_MID, EQ_MID_COPY);
    } else if (p >= 12 && p <= 14) {
        designFilter(2, 2, PEAK, eq(13), eq(12), eq(14), EQ_HIGH_MID);
        copyCoefs(5, EQ_HIGH_MID, EQ_HIGH_MID_COPY);
    } else if (p >= 16 && p <= 19) {
        designFilter(2, 4, eq(19) == 0 ? HIGH_SHELF2 : PEAK, eq(17), eq(16), eq(18), EQ_HIGH);
        copyCoefs(5, EQ_HIGH, EQ_HIGH_COPY);
    } else if (p == ALL) {
        hold = true;
        for (int band : {0, 4, 8, 12, 16})
            masterEq(band);
        hold = false;
    }
}

// The mixer's parameters into the mixer steps of the base program. The reverb's pan and
// return share one pair (quarter-sine pan law scaled by the return, >> 3); the other pans are
// pairs off the pan law; the returns and sends are v * 8192 / 127, the variation to reverb
// send v * 5792 / 127, the volume a square law over masterLevel. A muted window gets zeros.
// 4, 7, 12 and 13 have no performance byte: 4 is set to 90 at every load, 12 and 13 keep their
// defaults, and 9 re-applies 6, 7, 8 and the insertion's Dry/Wet. ALL also designs the DC blocker.
void EffectHost::mixerParam(int p)
{
    auto muted = [&](int w) -> bool { return mute[w] != 0; };
    auto law = [&](int32_t index) -> uint32_t { return uint16_t(rom::PAN_LAW[index]); };
    auto panPair = [&](const uint16_t* list, int i, uint32_t v) {
        coef[list[i]]     = law(128 - int32_t(v));
        coef[list[i + 1]] = law(int32_t(v));
    };
    auto zeroPair = [&](const uint16_t* list, int i) {
        coef[list[i]]     = 0;
        coef[list[i + 1]] = 0;
    };
    // floatToInt(uintToFloat(v) * scale / 127)
    auto level = [&](uint32_t v, float scale) -> int32_t {
        const float f = float(v) * scale;
        return int(f / 127.0f);
    };
    const uint16_t* m = mixParam;

    switch (p) {
        case MIX_REVERB_PAN:
        case MIX_REVERB_RETURN:
            if (muted(REVERB)) {
                zeroPair(MIXER_B, 0);
            } else {
                const uint32_t pan = m[MIX_REVERB_PAN];
                const float gl = float(rom::PAN_LAW[128 - pan]) / 127.0f;
                const float gr = float(rom::PAN_LAW[pan]) / 127.0f;
                coef[MIXER_B[0]] = int16_t(int(float(m[MIX_REVERB_RETURN]) * gl)) >> 3;
                coef[MIXER_B[1]] = int16_t(int(float(m[MIX_REVERB_RETURN]) * gr)) >> 3;
            }
            writeCoefs(2, MIXER_B);
            break;
        case MIX_AUX_PAN:
            if (muted(AUX)) zeroPair(MIXER_A, 0); else panPair(MIXER_A, 0, m[MIX_AUX_PAN]);
            writeCoefs(2, MIXER_A);
            break;
        case MIX_DELAY_RETURN:
            coef[MIXER_B[2]] = int16_t(level(m[MIX_DELAY_RETURN], 8192.0f)) >> 1;
            coef[MIXER_B[3]] = int16_t(level(m[MIX_DELAY_RETURN], 8192.0f)) >> 1;
            writeCoefs(2, MIXER_B + 2);
            break;
        case 4:
            coef[MIXER_C[6]]  = int16_t(level(m[4], 8192.0f)) << 1;
            coef[MIXER_C[13]] = int16_t(level(m[4], 8192.0f)) << 1;
            writeCoefs(1, MIXER_C + 6);
            writeCoefs(1, MIXER_C + 13);
            break;
        case MIX_INSERTION_PAN:
            if (muted(INSERTION)) zeroPair(MIXER_A, 2); else panPair(MIXER_A, 2, m[MIX_INSERTION_PAN]);
            writeCoefs(2, MIXER_A + 2);
            break;
        case MIX_VARIATION_RETURN:
            coef[MIXER_B[4]] = int16_t(level(m[MIX_VARIATION_RETURN], 8192.0f)) >> 1;
            coef[MIXER_B[5]] = int16_t(level(m[MIX_VARIATION_RETURN], 8192.0f)) >> 1;
            writeCoefs(2, MIXER_B + 4);
            break;
        case 7:
            coef[MIXER_C[1]] = int16_t(level(m[7], 8192.0f)) << 1;
            coef[MIXER_C[8]] = int16_t(level(m[7], 8192.0f)) << 1;
            writeCoefs(1, MIXER_C + 1);
            writeCoefs(1, MIXER_C + 8);
            break;
        case MIX_VARIATION_TO_REVERB:
            coef[MIXER_C[15]] = level(m[MIX_VARIATION_TO_REVERB], 5792.0f);
            coef[MIXER_C[21]] = level(m[MIX_VARIATION_TO_REVERB], 5792.0f);
            writeCoefs(1, MIXER_C + 15);
            writeCoefs(1, MIXER_C + 21);
            break;
        case 9:
            mixerParam(MIX_VARIATION_RETURN);
            mixerParam(7);
            mixerParam(MIX_VARIATION_TO_REVERB);
            applyParam(INSERTION, DRY_WET);
            break;
        case MIX_DELAY_PAN:
            if (muted(REVERB_DELAY)) zeroPair(MIXER_A, 4); else panPair(MIXER_A, 4, m[MIX_DELAY_PAN]);
            writeCoefs(2, MIXER_A + 4);
            break;
        case MIX_VARIATION_PAN:
            if (muted(VARIATION)) zeroPair(MIXER_A, 6); else panPair(MIXER_A, 6, m[MIX_VARIATION_PAN]);
            writeCoefs(2, MIXER_A + 6);
            break;
        case 12:
            panPair(MIXER_A, 8, m[12]);
            writeCoefs(2, MIXER_A + 8);
            break;
        case 13:
            panPair(MIXER_A, 10, m[13]);
            writeCoefs(2, MIXER_A + 10);
            break;
        case MIX_VOLUME:
            if (muted(OUTPUT)) {
                zeroPair(MIXER_B, 14);
            } else {
                // v * v * level / 127^2, the 32-bit product divided as signed
                const uint32_t v = m[MIX_VOLUME];
                const int32_t q = int32_t(v * v * masterLevel) / 16129;
                coef[MIXER_B[15]] = q;
                coef[MIXER_B[14]] = q;
            }
            writeCoefs(2, MIXER_B + 14);
            break;
        case ALL:
            hold = true;
            for (int q = 0; q <= MIX_VOLUME; q++)
                if (q != MIX_REVERB_RETURN)
                    mixerParam(q);
            designFilter(1, 0, HPF1, 0, 64, 0, MIXER_HPF);
            copyCoefs(3, MIXER_HPF, MIXER_HPF_COPY);
            hold = false;
            break;
    }
}

// 0 dB, word 21, into the eight output gain steps
void EffectHost::setOutputGain()
{
    for (int i = 0; i < 8; i++)
        coef[rom::STEPS_OUTPUT_GAIN[i]] = 21;
    writeCoefs(8, rom::STEPS_OUTPUT_GAIN);
}

}  // namespace fs1r::vop3host

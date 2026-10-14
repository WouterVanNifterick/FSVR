// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Window reset, master EQ, system parameters and the master gain.
#include "host.h"
#include <algorithm>

namespace fs1r::vop3host {

using namespace rom::mix;

namespace {
}  // namespace

// 0x3A3308 fxResetWindow: silence a window (zero its constants), spread its delay slots
// evenly over the window's area, and for kinds 2-4 load the "clear" program.
void Host::fxResetWindow(int kind)
{
    kind &= 0xFF;
    auto zero_steps = [&](uint32_t n, const uint16_t* list) {
        for (uint32_t i = 0; i < n; i++)
            coef[list[i]] = 0;
        writeSweWordListSub(n, list);
    };
    // offs[slot_i] = ((span / n) * (n - 1 - i)), unsigned divide, 32-bit product
    auto spread = [&](uint32_t n, const uint8_t* slots, uint32_t span) {
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t q = span / n;
            delay[slots[i]] = int32_t(q * (n - 1 - i));
        }
        writeSweDeSub(n, slots);
    };
    const uint32_t b0 = AREA[0], b1 = AREA[1], b2 = AREA[2];
    const uint32_t b3 = AREA[3], b4 = AREA[4];
    switch (kind) {
    case 0:
        // the second tag image behind the base tags: the base program's "clear" tags
        std::copy_n(rom::BASE_TAGS[1], 232, tag);
        writeSwe0CSub(0x5E, rom::STEPS_REVERB);
        zero_steps(0x5E, rom::STEPS_REVERB);
        spread(14, rom::RESET_SLOTS_REVERB, b1 - b0);
        break;
    case 1:
        break;
    case 2:
        zero_steps(0x60, rom::STEPS_INSERTION);
        spread(24, rom::RESET_SLOTS_INSERTION, b3 - b2);
        loadSubCoeffBlock(2, 1);
        break;
    case 3:
        zero_steps(0x48, rom::STEPS_REVERB_DELAY);
        spread(12, rom::RESET_SLOTS_REVERB_DELAY, b4 - b3);
        loadSubCoeffBlock(3, 1);
        break;
    case 4:
        zero_steps(0x48, rom::STEPS_VARIATION);
        spread(12, rom::RESET_SLOTS_VARIATION, 0x3FFFF - b4);
        loadSubCoeffBlock(4, 1);
        break;
    }
}

// 0x3A356C fxMasterEq: the performance EQ, one biquad per band through fxDesignFilter, then the
// band's five words are copied to the other channel's steps. p selects the band by its
// parameter group (0-3 low, 4-6 low-mid, 8-10 mid, 12-14 high-mid, 16-19 high); 255 does all.
void Host::fxMasterEq(int p)
{
    p &= 0xFF;
    auto eq = [&](int i) -> int32_t { return eqParam[i]; };
    if (p <= 3) {
        const int kind = eq(3) == 0 ? 2 : 10;
        fxDesignFilter(2, 2, kind, eq(1), int8_t(eqParam[0]), eq(2), EQ_LOW);
        fxCopyWords5(EQ_LOW, EQ_LOW_COPY);
    } else if (p >= 4 && p <= 6) {
        fxDesignFilter(2, 2, 10, eq(5), eq(4), eq(6), EQ_LOW_MID);
        fxCopyWords5(EQ_LOW_MID, EQ_LOW_MID_COPY);
    } else if (p >= 8 && p <= 10) {
        fxDesignFilter(2, 2, 10, eq(9), eq(8), eq(10), EQ_MID);
        fxCopyWords5(EQ_MID, EQ_MID_COPY);
    } else if (p >= 12 && p <= 14) {
        fxDesignFilter(2, 2, 10, eq(13), eq(12), eq(14), EQ_HIGH_MID);
        fxCopyWords5(EQ_HIGH_MID, EQ_HIGH_MID_COPY);
    } else if (p >= 16 && p <= 19) {
        const int kind = eq(19) == 0 ? 7 : 10;
        fxDesignFilter(2, 4, kind, eq(17), eq(16), eq(18), EQ_HIGH);
        fxCopyWords5(EQ_HIGH, EQ_HIGH_COPY);
    } else if (p == 255) {
        hold = true;
        fxMasterEq(0);
        fxMasterEq(4);
        fxMasterEq(8);
        fxMasterEq(12);
        fxMasterEq(16);
        hold = false;
    }
}

// flash 0x3A2EC fxSystemParam: the system parameters into the mixer steps of the base program.
//   p 0/1 reverb pan + level (quarter-sine pan law scaled by level, >> 3), 2 reverb return pan,
//   3 reverb send level (>> 1), 4 insertion send (<< 1), 5 insertion return pan, 6 variation
//   send (>> 1), 7 variation return (<< 1), 8 variation to reverb (x 5792/127), 9 = 6, 7, 8 and
//   insertion parameter 9, 10/11 pans, 12/13 pans, 14 master volume (quadratic law over
//   masterLevel), 255 everything plus the mixer's DC blocker.
// A muted block gets zeros instead.
void Host::fxSystemParam(int p)
{
    p &= 0xFF;
    auto sys = [&](int i) -> uint32_t { return mixParam[i]; };
    auto step = [&](const uint16_t* list, int i) -> uint32_t { return list[i]; };
    auto enabled = [&](int e) -> bool { return mute[e] != 0; };
    auto law = [&](int32_t index) -> uint32_t { return uint16_t(rom::PAN_LAW[index]); };
    auto pan_pair = [&](const uint16_t* list, int i, uint32_t v) {
        coef[step(list, i)] = law(128 - int32_t(v));
        coef[step(list, i + 1)] = law(int32_t(v));
    };
    auto zero_pair = [&](const uint16_t* list, int i) {
        coef[step(list, i)] = 0;
        coef[step(list, i + 1)] = 0;
    };
    // floatToInt(uintToFloat(v) * scale / 127)
    auto level = [&](uint32_t v, float scale) -> int32_t {
        const float f = float(v) * scale;
        return int(f / 127.0f);
    };

    switch (p) {
    case 0: case 1: {
        if (enabled(0)) {
            zero_pair(MIXER_B, 0);
        } else {
            const uint32_t pan = sys(0);
            const float gl = float(rom::PAN_LAW[uint32_t(128 - int32_t(pan))]) / 127.0f;
            const float gr = float(rom::PAN_LAW[pan]) / 127.0f;
            const int32_t l = int(float(sys(1)) * gl);
            coef[step(MIXER_B, 0)] = uint32_t(int16_t(l) >> 3);
            const int32_t r = int(float(sys(1)) * gr);
            coef[step(MIXER_B, 1)] = uint32_t(int16_t(r) >> 3);
        }
        writeSweWordListSub(2, MIXER_B);
        break;
    }
    case 2:
        if (enabled(1)) zero_pair(MIXER_A, 0); else pan_pair(MIXER_A, 0, sys(2));
        writeSweWordListSub(2, MIXER_A);
        break;
    case 3:
        coef[step(MIXER_B, 2)] = uint32_t(int16_t(level(sys(3), 8192.0f)) >> 1);
        coef[step(MIXER_B, 3)] = uint32_t(int16_t(level(sys(3), 8192.0f)) >> 1);
        writeSweWordListSub(2, MIXER_B + 2);
        break;
    case 4:
        coef[step(MIXER_C, 6)] = uint32_t(int16_t(level(sys(4), 8192.0f)) << 1);
        coef[MIXER_C[13]] = uint32_t(int16_t(level(sys(4), 8192.0f)) << 1);
        writeSweWordListSub(1, MIXER_C + 6);
        writeSweWordListSub(1, MIXER_C + 13);
        break;
    case 5:
        if (enabled(2)) zero_pair(MIXER_A, 2); else pan_pair(MIXER_A, 2, sys(5));
        writeSweWordListSub(2, MIXER_A + 2);
        break;
    case 6:
        coef[step(MIXER_B, 4)] = uint32_t(int16_t(level(sys(6), 8192.0f)) >> 1);
        coef[step(MIXER_B, 5)] = uint32_t(int16_t(level(sys(6), 8192.0f)) >> 1);
        writeSweWordListSub(2, MIXER_B + 4);
        break;
    case 7:
        coef[step(MIXER_C, 1)] = uint32_t(int16_t(level(sys(7), 8192.0f)) << 1);
        coef[MIXER_C[8]] = uint32_t(int16_t(level(sys(7), 8192.0f)) << 1);
        writeSweWordListSub(1, MIXER_C + 1);
        writeSweWordListSub(1, MIXER_C + 8);
        break;
    case 8:
        coef[step(MIXER_C, 15)] = uint32_t(level(sys(8), 5792.0f));
        coef[MIXER_C[21]] = uint32_t(level(sys(8), 5792.0f));
        writeSweWordListSub(1, MIXER_C + 15);
        writeSweWordListSub(1, MIXER_C + 21);
        break;
    case 9:
        fxSystemParam(6);
        fxSystemParam(7);
        fxSystemParam(8);
        fxParamEntry(2, 9);
        break;
    case 10:
        if (enabled(3)) zero_pair(MIXER_A, 4); else pan_pair(MIXER_A, 4, sys(10));
        writeSweWordListSub(2, MIXER_A + 4);
        break;
    case 11:
        if (enabled(4)) zero_pair(MIXER_A, 6); else pan_pair(MIXER_A, 6, sys(11));
        writeSweWordListSub(2, MIXER_A + 6);
        break;
    case 12:
        pan_pair(MIXER_A, 8, sys(12));
        writeSweWordListSub(2, MIXER_A + 8);
        break;
    case 13:
        pan_pair(MIXER_A, 10, sys(13));
        writeSweWordListSub(2, MIXER_A + 10);
        break;
    case 14: {
        if (enabled(5)) {
            zero_pair(MIXER_B, 14);
        } else {
            // v * v * level / 127^2, the 32-bit product divided as signed
            const uint32_t v = sys(14);
            const uint32_t prod = v * v * masterLevel;
            const int32_t q = int32_t(prod) / 16129;
            coef[step(MIXER_B, 15)] = uint32_t(q);
            coef[step(MIXER_B, 14)] = uint32_t(q);
        }
        writeSweWordListSub(2, MIXER_B + 14);
        break;
    }
    case 255:
        hold = true;
        for (int q = 0; q <= 14; q++)
            if (q != 1)
                fxSystemParam(q);
        fxDesignFilter(1, 0, 1, 0, 64, 0, MIXER_HPF);
        fxCopyWords3(MIXER_HPF, MIXER_HPF_COPY);
        hold = false;
        break;
    }
}

// flash 0x3BCD0 setSubGain8: 0 dB, word 21, into the eight output gain steps
void Host::setSubGain8()
{
    for (int i = 0; i < 8; i++)
        coef[rom::STEPS_OUTPUT_GAIN[i]] = 21;
    writeSweWordListSub(8, rom::STEPS_OUTPUT_GAIN);
}

}  // namespace fs1r::vop3host

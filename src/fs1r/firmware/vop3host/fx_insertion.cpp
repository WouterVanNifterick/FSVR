// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 1: the modulation group, the delays, the early reflections and
// the cross delay.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ins;

// ---- the delays (msb 0 lsb 14-17) ----
namespace {
// offs[S[dst]] = offs[S[ref]] + samples(p), then the slot
void ins_delay_slot(EffectHost& h, int dst, int ref, int p)
{
    h.delay[MOD_SLOTS[dst]] = h.delay[MOD_SLOTS[ref]] + h.toSamples(P(h, p));
    h.writeDelays(1, MOD_SLOTS + dst);
}
void ins_delay_filters(EffectHost& h, int p) { filters(h, p, 12, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C); }
void ins_delay_wet(EffectHost& h)
{
    wet_words(h, MOD_WORDS, rom::ER_REV, PU(h, 9), GGXX);
    h.writeCoefs(4, MOD_WORDS);
}
}  // namespace

// type 33; msb 0 lsb 14: the three-tap layout
void EffectHost::fxInsDelayLR(int p)
{
    switch (p) {
    case 0: ins_delay_slot(*this, 3, 0, 0); break;
    case 1: ins_delay_slot(*this, 6, 0, 1); break;
    case 2: ins_delay_slot(*this, 8, 0, 2); break;
    case 3: ins_delay_slot(*this, 1, 0, 3); break;
    case 4:
        coef[MOD_WORDS[8]] = bipolar500(P(*this, 4));
        writeCoefs(1, MOD_WORDS + 8);
        break;
    case 5: {
        const int32_t x = int(double(rom::LEVEL_CURVE[PU(*this, 5)]) * 32767.0);
        coef[MOD_WORDS[7]] = int16_t(x);
        coef[MOD_WORDS[5]] = int16_t(x);
        writeCoefs(1, MOD_WORDS + 5);
        writeCoefs(1, MOD_WORDS + 7);
        break;
    }
    case 6:
        storeHighDamp(1, P(*this, 6), 64, DELAY_CURVE);
        break;
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 5, 6, 9, 12, 14})
            fxInsDelayLR(q);
        hold = false;
        break;
    }
}

// type 34; msb 0 lsb 15
void EffectHost::fxInsDelayLCR(int p)
{
    switch (p) {
    case 0: ins_delay_slot(*this, 3, 0, 0); break;
    case 1: ins_delay_slot(*this, 8, 0, 1); break;
    case 2: ins_delay_slot(*this, 1, 0, 2); break;
    case 3: ins_delay_slot(*this, 6, 0, 3); break;
    case 4: {
        const int32_t x = bipolar500(P(*this, 4));
        coef[MOD_WORDS[10]] = int16_t(x) >> 1;
        coef[MOD_WORDS[8]] = int16_t(x) >> 1;
        writeCoefs(1, MOD_WORDS + 8);
        writeCoefs(1, MOD_WORDS + 10);
        break;
    }
    case 5:
        storeHighDamp(1, P(*this, 5), 64, DELAY_CURVE);
        copyCoefs(2, DELAY_CURVE, DELAY_CURVE_COPY);
        break;
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 5, 9, 12, 14})
            fxInsDelayLCR(q);
        hold = false;
        break;
    }
}

// type 35; msb 0 lsb 16
void EffectHost::fxInsEcho(int p)
{
    switch (p) {
    case 0: ins_delay_slot(*this, 1, 0, 0); break;
    case 5: ins_delay_slot(*this, 3, 0, 5); break;
    case 2: ins_delay_slot(*this, 6, 5, 2); break;
    case 6: ins_delay_slot(*this, 8, 5, 6); break;
    case 1:
        coef[MOD_WORDS[8]] = bipolar500(P(*this, 1));
        writeCoefs(1, MOD_WORDS + 8);
        break;
    case 3:
        coef[MOD_WORDS[9]] = bipolar500(P(*this, 3));
        writeCoefs(1, MOD_WORDS + 9);
        break;
    case 4:
        storeHighDamp(1, P(*this, 4), 64, DELAY_CURVE);
        copyCoefs(2, DELAY_CURVE, DELAY_CURVE_COPY);
        break;
    case 7: {
        const int32_t i = int(32767.0f * rom::LEVEL_CURVE[PU(*this, 7)]);
        const uint32_t x = typeGain(INSERTION, int16_t(i));
        coef[MOD_WORDS[5]] = x;
        coef[MOD_WORDS[4]] = x;
        writeCoefs(2, MOD_WORDS + 4);
        break;
    }
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case ALL:
        delay[MOD_SLOTS[5]] = half_area();
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 14})
            fxInsEcho(q);
        hold = false;
        break;
    }
}

// type 36 "ER 1"; msb 0 lsb 17: the cross-fed pair with routing mode
// parameter 3; 3 re-runs 2
void EffectHost::fxInsEr1(int p)
{
    const int32_t mode = P(*this, 3);
    switch (p) {
    case 0: {
        const int ref = mode == 1 ? 5 : 0;
        delay[MOD_SLOTS[6]] = delay[MOD_SLOTS[ref]] + toSamples(P(*this, 0));
        writeDelays(1, MOD_SLOTS + 6);
        break;
    }
    case 1: {
        const int ref = mode == 1 ? 0 : 5;
        delay[MOD_SLOTS[1]] = delay[MOD_SLOTS[ref]] + toSamples(P(*this, 1));
        writeDelays(1, MOD_SLOTS + 1);
        break;
    }
    case 2:
        if (mode == 1) {
            coef[MOD_WORDS[9]] = 0;
            coef[MOD_WORDS[8]] = 0;
            const uint32_t x = bipolar500(P(*this, 2));
            coef[MOD_WORDS[11]] = x;
            coef[MOD_WORDS[10]] = x;
            writeCoefs(4, MOD_WORDS + 8);
        } else {
            const uint32_t x = bipolar500(P(*this, 2));
            coef[MOD_WORDS[9]] = x;
            coef[MOD_WORDS[8]] = x;
            coef[MOD_WORDS[11]] = 0;
            coef[MOD_WORDS[10]] = 0;
            writeCoefs(2, MOD_WORDS + 10);
            writeCoefs(2, MOD_WORDS + 8);
        }
        break;
    case 3: {
        const int32_t half = int32_t(half_area());
        if (mode == 0) {
            coef[MOD_WORDS[12]] = 0x4000; coef[MOD_WORDS[13]] = 0x4000; coef[MOD_WORDS[14]] = 0;
            delay[MOD_SLOTS[0]] = 1;
            delay[MOD_SLOTS[5]] = half;
        } else if (mode == 1) {
            coef[MOD_WORDS[12]] = 0x4000; coef[MOD_WORDS[13]] = 0x4000; coef[MOD_WORDS[14]] = 0;
            delay[MOD_SLOTS[0]] = half;
            delay[MOD_SLOTS[5]] = 1;
        } else {
            coef[MOD_WORDS[12]] = 0x7FFF; coef[MOD_WORDS[13]] = 0; coef[MOD_WORDS[14]] = 0x7FFF;
            delay[MOD_SLOTS[0]] = 1;
            delay[MOD_SLOTS[5]] = half;
        }
        writeCoefs(3, MOD_WORDS + 12);
        writeDelays(1, MOD_SLOTS);
        writeDelays(1, MOD_SLOTS + 5);
        fxInsEr1(2);
        break;
    }
    case 4:
        storeHighDamp(1, P(*this, 4), 64, DELAY_CURVE);
        copyCoefs(2, DELAY_CURVE, DELAY_CURVE_COPY);
        break;
    case 9: {
        const uint32_t wet = PU(*this, 9);
        wet_words(*this, MOD_WORDS, rom::ER_REV, wet, GGXX);
        writeCoefs(4, MOD_WORDS);
        break;
    }
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case ALL:
        hold = true;
        for (int q : {3, 0, 1, 4, 9, 12, 14})
            fxInsEr1(q);
        hold = false;
        break;
    }
}

// ---- the early reflection types (msb 3) ----
namespace {
inline int32_t er_row(const EffectHost& h) { return rom::ER_ROWS[6 * lsb(h) + PU(h, 0)]; }
void er_wet(EffectHost& h)
{
    wet_words(h, ER_WET, rom::ER_REV, PU(h, 9), GXGX);
    h.writeCoefs(4, ER_WET);
}
}  // namespace

// the comb-gain table's loss word at x clamped to 0..255
int32_t EffectHost::erGain(int x)
{
    int32_t v = int16_t(x);
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return int16_t(rom::COMB_GAIN[2 * v]);
}

// the 20 early reflections of the pattern row
// (parameter 0, +6 for type lsb 3, +8 for lsb 4) scaled by the density (parameter 1) after the
// pre-delay (parameter 3), gains from the level (parameter 10), clamped to the delay area
void EffectHost::erPattern()
{
    const uint32_t t = lsb(*this);
    const uint32_t rowi = (PU(*this, 0) + (t == 3 ? 6u : t == 4 ? 8u : 0u)) & 0xFFFF;
    const int16_t* row = rom::ER_PATTERNS + rowi * 40;
    const uint32_t density = rom::ER_DENSITY[PU(*this, 1)];
    const int32_t level = int32_t(rom::ER_LEVEL[PU(*this, 10)]) - 128;
    const int32_t base = toSamples(rom::DELAY_WORD[PU(*this, 3)]) + delay[ER_SLOTS[21]];
    const int32_t limit = int32_t(AREA[3] - AREA[2]) - 1;
    for (uint32_t i = 0; i < 20; i++, row += 2) {
        const uint32_t w = uint32_t(scaleDelay(row[0])) & 0xFFFF;
        int32_t pos = base + int32_t(density * w) / 10;
        int32_t gain;
        if (pos > limit) {
            pos = limit;
            gain = 0;
        } else {
            const int32_t g = row[1];
            if (g > 0)
                gain = int16_t(uint32_t(erGain(g + level)));
            else
                gain = -int16_t(uint32_t(erGain(level - g)));
        }
        delay[ER_SLOTS[i]] = pos;
        coef[ER_STEPS[i]] = (gain + (gain < 0 ? 1 : 0)) >> 1;
        if (i == 19)
            delay[ER_SLOTS[20]] = pos > 0x8000 ? 0x8000 : pos;
    }
    coef[ER_STEPS[19]] = 0;
    if (t == 3)
        for (uint32_t k = 15; k <= 18; k++)
            coef[ER_STEPS[k]] = 0;
}

// Cross Delay, ER 2, Gate Rev, Reverse Gate (types 37-40; msb 3 lsb 1-4)
void EffectHost::fxInsCrossDelayEr(int p)
{
    switch (p) {
    case 0:
        erPattern();
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeCoefs(20, ER_STEPS);
        writeDelays(21, ER_SLOTS);
        writeCoefs(16, ER_ROW_STEPS);
        writeDelays(8, ER_ROW_SLOTS);
        break;
    case 1: case 3:
        erPattern();
        writeCoefs(20, ER_STEPS);
        writeDelays(21, ER_SLOTS);
        break;
    case 10:
        erPattern();
        writeCoefs(20, ER_STEPS);
        break;
    case 2: {
        const int32_t diff = int32_t(rom::DIFFUSION[PU(*this, 2)]);
        delay[ER_DIFF_SLOTS[2]] = delay[ER_DIFF_SLOTS[0]] + diff + 1;
        delay[ER_DIFF_SLOTS[3]] = delay[ER_DIFF_SLOTS[1]] + diff + 1;
        writeDelays(2, ER_DIFF_SLOTS + 2);
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeDelays(1, ER_ROW_SLOTS + 7);
        break;
    }
    case 4:
        coef[ER_FEEDBACK[0]] = bipolar500(P(*this, 4));
        writeCoefs(1, ER_FEEDBACK);
        break;
    case 5:
        designFilter(2, 1, HPF1, P(*this, 5), 64, 0, ER_HPF);
        break;
    case 6:
        designFilter(2, 2, LPF1, P(*this, 6), 64, 0, ER_LPF_2);
        break;
    case 9:
        er_wet(*this);
        break;
    case 11:
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeCoefs(16, ER_ROW_STEPS);
        break;
    case 12:
        designFilter(1, 0, LPF1, rom::DAMP_FREQ[PU(*this, 12)], 64, 0, ER_LPF);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 2, 4, 5, 6, 9, 12})
            fxInsCrossDelayEr(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

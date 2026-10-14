// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The delays: Delay L,C,R, Delay L,R, Echo and Cross Delay, which the reverb (types 13-16), the
// variation and the insertion all run, each over its own fourteen steps and six delay slots; and
// the variation's Karaoke.
#include "fx_common.h"
#include <algorithm>

namespace fs1r::vop3host {

namespace {

// A block's delay steps and slots, as places in its lists
struct Block {
    const uint16_t* words;  uint8_t word[14];
    const uint8_t* slots;   uint8_t slot[6];
    Shelves shelves;                                    // parameters 12-15
    const uint16_t *damp, *dampCopy;
    int dampScale;
};
const Block BLOCKS[3] = {
    {rev::DELAY_WORDS, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, rev::DELAY_SLOTS, {0, 1, 2, 3, 4, 5},
     {rev::DELAY_LPF, rev::DELAY_LPF_COPY, rev::DELAY_HPF, rev::DELAY_HPF_COPY}, rev::DELAY_CURVE, rev::DELAY_CURVE_COPY, 0},
    {var::DELAY_WORDS, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, var::DELAY_SLOTS, {0, 1, 2, 3, 4, 5},
     {var::DELAY_LPF, var::DELAY_LPF_COPY, var::DELAY_HPF, var::DELAY_HPF_COPY}, var::DELAY_CURVE, var::DELAY_CURVE_COPY, 0},
    {ins::MOD_WORDS,   {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14}, ins::MOD_SLOTS,   {0, 1, 3, 5, 6, 8},
     {ins::LPF_MOD,   ins::LPF_MOD_C,      ins::HPF_MOD,   ins::HPF_MOD_C},      ins::DELAY_CURVE, ins::DELAY_CURVE_COPY, 1},
};
const Block& blockOf(int w) { return BLOCKS[w == REVERB_DELAY ? 0 : w == VARIATION ? 1 : 2]; }

// The block's helpers. Word i and slot i are the block's i-th step and slot.
struct Delay {
    EffectHost& h;
    const Block& b;
    int w;
    const uint16_t* word(int i) const { return b.words + b.word[i]; }
    const uint8_t* slot(int i) const { return b.slots + b.slot[i]; }
    uint16_t& coef(int i) const { return h.coef[*word(i)]; }
    int32_t& delay(int i) const { return h.delay[*slot(i)]; }

    // a tap v after another
    void tap(int i, int after, int v) const
    {
        delay(i) = delay(after) + h.toSamples(v);
        h.writeDelays(1, slot(i));
    }
    void bipolar(int i, int v) const
    {
        coef(i) = h.bipolar500(v);
        h.writeCoefs(1, word(i));
    }
    void damp(int v, bool copy) const
    {
        h.storeHighDamp(b.dampScale, v, 64, b.damp);
        if (copy)
            h.copyCoefs(2, b.damp, b.dampCopy);
    }
};

}  // namespace

void EffectHost::delayLCR(int w, int p)
{
    enum { LCH_DELAY, RCH_DELAY, CCH_DELAY, FB_DELAY, FB_LEVEL, CCH_LEVEL, HIGH_DAMP, DRY_WET = 9, EQ_LOW_FREQ = 12, EQ_HIGH_FREQ = 14 };
    const Delay d{*this, blockOf(w), w};
    const uint16_t* P = param[w];
    switch (p) {
        case LCH_DELAY: d.tap(2, 0, P[LCH_DELAY]); break;
        case RCH_DELAY: d.tap(4, 0, P[RCH_DELAY]); break;
        case CCH_DELAY: d.tap(5, 0, P[CCH_DELAY]); break;
        case FB_DELAY:  d.tap(1, 0, P[FB_DELAY]);  break;
        case FB_LEVEL:  d.bipolar(7, P[FB_LEVEL]); break;
        case CCH_LEVEL:
            d.coef(6) = d.coef(5) = int16_t(int(double(rom::LEVEL_CURVE[P[CCH_LEVEL]]) * 32767.0));
            writeCoefs(1, d.word(5));
            writeCoefs(1, d.word(6));
            break;
        case HIGH_DAMP:
            d.damp(P[HIGH_DAMP], false);
            break;
        case DRY_WET:
            dryWet(w, d.word(0), false, rom::ER_REV);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, d.b.shelves);
            break;
        case ALL:
            applyAll(w, &EffectHost::delayLCR, {LCH_DELAY, RCH_DELAY, CCH_DELAY, FB_DELAY, FB_LEVEL, CCH_LEVEL, HIGH_DAMP, DRY_WET, EQ_LOW_FREQ, EQ_HIGH_FREQ});
            break;
    }
}

void EffectHost::delayLR(int w, int p)
{
    enum { LCH_DELAY, RCH_DELAY, FB_DELAY1, FB_DELAY2, FB_LEVEL, HIGH_DAMP, DRY_WET = 9, EQ_LOW_FREQ = 12, EQ_HIGH_FREQ = 14 };
    const Delay d{*this, blockOf(w), w};
    const uint16_t* P = param[w];
    switch (p) {
        case LCH_DELAY: d.tap(2, 0, P[LCH_DELAY]); break;
        case RCH_DELAY: d.tap(5, 0, P[RCH_DELAY]); break;
        case FB_DELAY1: d.tap(1, 0, P[FB_DELAY1]); break;
        case FB_DELAY2: d.tap(4, 0, P[FB_DELAY2]); break;
        case FB_LEVEL:
            d.coef(9) = d.coef(7) = int16_t(bipolar500(P[FB_LEVEL])) >> 1;
            writeCoefs(1, d.word(7));
            writeCoefs(1, d.word(9));
            break;
        case HIGH_DAMP:
            d.damp(P[HIGH_DAMP], true);
            break;
        case DRY_WET:
            dryWet(w, d.word(0), false, rom::ER_REV);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, d.b.shelves);
            break;
        case ALL:
            applyAll(w, &EffectHost::delayLR, {LCH_DELAY, RCH_DELAY, FB_DELAY1, FB_DELAY2, FB_LEVEL, HIGH_DAMP, DRY_WET, EQ_LOW_FREQ, EQ_HIGH_FREQ});
            break;
    }
}

void EffectHost::echo(int w, int p)
{
    enum { LCH_DELAY1, LCH_FB_LEVEL, RCH_DELAY1, RCH_FB_LEVEL, HIGH_DAMP, LCH_DELAY2, RCH_DELAY2, DELAY2_LEVEL,
           DRY_WET = 9, EQ_LOW_FREQ = 12, EQ_HIGH_FREQ = 14 };
    const Delay d{*this, blockOf(w), w};
    const uint16_t* P = param[w];
    switch (p) {
        case LCH_DELAY1:   d.tap(1, 0, P[LCH_DELAY1]); break;
        case LCH_FB_LEVEL: d.bipolar(7, P[LCH_FB_LEVEL]); break;
        case RCH_DELAY1:   d.tap(4, 3, P[RCH_DELAY1]); break;     // the right channel counts from slot 3
        case RCH_FB_LEVEL: d.bipolar(8, P[RCH_FB_LEVEL]); break;
        case HIGH_DAMP:    d.damp(P[HIGH_DAMP], true); break;
        case LCH_DELAY2:   d.tap(2, 0, P[LCH_DELAY2]); break;
        case RCH_DELAY2:   d.tap(5, 3, P[RCH_DELAY2]); break;
        case DELAY2_LEVEL: {
            int32_t x = int(32767.0f * rom::LEVEL_CURVE[P[DELAY2_LEVEL]]);
            if (w == INSERTION)                     // only the insertion's at its type's level
                x = typeGain(w, int16_t(x));
            d.coef(5) = d.coef(4) = x;
            writeCoefs(2, d.word(4));
            break;
        }
        case DRY_WET:
            dryWet(w, d.word(0), false, rom::ER_REV);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, d.b.shelves);
            break;
        case ALL:
            d.delay(3) = HALF_AREA;
            applyAll(w, &EffectHost::echo, {LCH_DELAY1, LCH_FB_LEVEL, RCH_DELAY1, RCH_FB_LEVEL, HIGH_DAMP, LCH_DELAY2, RCH_DELAY2, DELAY2_LEVEL,
                                            DRY_WET, EQ_LOW_FREQ, EQ_HIGH_FREQ});
            break;
    }
}

void EffectHost::crossDelay(int w, int p)
{
    enum { L_TO_R_DELAY, R_TO_L_DELAY, FB_LEVEL, INPUT_SELECT, HIGH_DAMP, DRY_WET = 9, EQ_LOW_FREQ = 12, EQ_HIGH_FREQ = 14 };
    enum { INPUT_L, INPUT_R, INPUT_LR };
    const Delay d{*this, blockOf(w), w};
    const uint16_t* P = param[w];
    const bool right = P[INPUT_SELECT] == INPUT_R;   // the right input goes round first
    switch (p) {
        case L_TO_R_DELAY: d.tap(4, right ? 3 : 0, P[L_TO_R_DELAY]); break;
        case R_TO_L_DELAY: d.tap(1, right ? 0 : 3, P[R_TO_L_DELAY]); break;
        case FB_LEVEL: {
            const uint16_t x = bipolar500(P[FB_LEVEL]);
            d.coef(8) = d.coef(7) = right ? 0 : x;
            d.coef(10) = d.coef(9) = right ? x : 0;
            if (right) {
                writeCoefs(4, d.word(7));
            } else {
                writeCoefs(2, d.word(9));
                writeCoefs(2, d.word(7));
            }
            break;
        }
        case INPUT_SELECT: {
            const bool both = P[INPUT_SELECT] != INPUT_L && !right;
            d.coef(11) = both ? UNITY : HALF;
            d.coef(12) = both ? 0 : HALF;
            d.coef(13) = both ? UNITY : 0;
            d.delay(0) = right ? HALF_AREA : 1;
            d.delay(3) = right ? 1 : HALF_AREA;
            writeCoefs(3, d.word(11));
            writeDelays(1, d.slot(0));
            writeDelays(1, d.slot(3));
            crossDelay(w, FB_LEVEL);
            break;
        }
        case HIGH_DAMP:
            d.damp(P[HIGH_DAMP], true);
            break;
        case DRY_WET:
            dryWet(w, d.word(0), false, rom::ER_REV);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, d.b.shelves);
            break;
        case ALL:
            applyAll(w, &EffectHost::crossDelay, {INPUT_SELECT, L_TO_R_DELAY, R_TO_L_DELAY, HIGH_DAMP, DRY_WET, EQ_LOW_FREQ, EQ_HIGH_FREQ});
            break;
    }
}

void EffectHost::karaoke(int w, int p)
{
    enum { DELAY_TIME, FB_LEVEL, HPF_CUTOFF, LPF_CUTOFF, DRY_WET = 9, ROW = 10 };   // the row is a hidden parameter
    const uint16_t* P = param[w];
    switch (p) {
        case DELAY_TIME:
            delay[var::KARAOKE_SLOTS[1]] = delay[var::KARAOKE_SLOTS[0]] + toSamples(rom::PITCH_DELAY[P[DELAY_TIME]]);
            writeDelays(1, var::KARAOKE_SLOTS + 1);
            break;
        case FB_LEVEL:
            coef[var::KARAOKE_LEVEL[1]] = coef[var::KARAOKE_LEVEL[0]] = (P[FB_LEVEL] - 64) * 250;
            writeCoefs(2, var::KARAOKE_LEVEL);
            break;
        case HPF_CUTOFF:
            designFilter(1, 1, HPF1, P[HPF_CUTOFF], 64, 0, var::KARAOKE_HPF);
            break;
        case LPF_CUTOFF:
            designFilter(1, 1, LPF1, P[LPF_CUTOFF], 64, 0, var::KARAOKE_LPF);
            break;
        case DRY_WET:
            dryWet(w, var::KARAOKE_PAN, true, rom::ER_REV);
            break;
        case ROW:
            diffusers(std::min<int>(P[ROW], 3), 5, 3, 4, 0, var::KARAOKE_ROW_STEPS, var::KARAOKE_ROW_SLOTS);   // the table has four rows
            writeCoefs(16, var::KARAOKE_ROW_STEPS);
            writeDelays(8, var::KARAOKE_ROW_SLOTS);
            break;
        case ALL:
            applyAll(w, &EffectHost::karaoke, {DELAY_TIME, FB_LEVEL, HPF_CUTOFF, LPF_CUTOFF, DRY_WET, ROW});
            break;
    }
}

}  // namespace fs1r::vop3host

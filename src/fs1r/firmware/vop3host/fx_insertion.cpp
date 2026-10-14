// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 1: the modulation group, the delays, the early reflections and
// the cross delay.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ins;

// Chorus and Celeste (types 1, 2; msb 0 lsb 2-9)
void EffectHost::fxInsChorus(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = int(PLOU(*this, 1) << 1);
        storeDepth(x, MOD_LEVEL);
        storeDepth(x, MOD_LEVEL + 2);
        storeDepth(x, MOD_LEVEL + 4);
        break;
    }
    case 2: {
        const uint32_t x = bipolar376(P(*this, 2));
        coef[MOD_WORDS[9]] = x;
        coef[MOD_WORDS[8]] = x;
        writeCoefs(2, MOD_WORDS + 8);
        break;
    }
    case 3: case 14: {
        const int32_t t = ms(*this, rom::MOD_DELAY, 3);
        int32_t base = delay[MOD_SLOTS[0]];
        const int32_t d = t + base;
        delay[MOD_SLOTS[2]] = d + 1;
        delay[MOD_SLOTS[1]] = d + 1;
        delay[MOD_SLOTS[4]] = d + 65;
        delay[MOD_SLOTS[3]] = d + 65;
        if (P(*this, 14) == 0) {
            coef[MOD_WORDS[12]] = 0x4000;
            coef[MOD_WORDS[13]] = 0x4000;
            delay[MOD_SLOTS[5]] = 0;
        } else {
            coef[MOD_WORDS[12]] = 0x7FFF;
            coef[MOD_WORDS[13]] = 0;
            base = int32_t(half_area());
            delay[MOD_SLOTS[5]] = base;
        }
        const int32_t d2 = t + base;
        delay[MOD_SLOTS[7]] = d2 + 1;
        delay[MOD_SLOTS[6]] = d2 + 1;
        delay[MOD_SLOTS[9]] = d2 + 65;
        delay[MOD_SLOTS[8]] = d2 + 65;
        writeCoefs(2, MOD_WORDS + 12);
        writeDelays(4, MOD_SLOTS + 1);
        writeDelays(5, MOD_SLOTS + 5);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MOD_WORDS, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, MOD_WORDS);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10})
            fxInsChorus(q);
        hold = false;
        break;
    }
}

// type 3; msb 0 lsb 10-12
void EffectHost::fxInsFlanger(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        storeDepth(P(*this, 1), MOD_LEVEL);
        storeDepth(P(*this, 1), MOD_LEVEL + 2);
        break;
    case 2: {
        const uint32_t x = bipolar500(P(*this, 2));
        coef[MOD_WORDS[9]] = x;
        coef[MOD_WORDS[8]] = x;
        writeCoefs(2, MOD_WORDS + 8);
        break;
    }
    case 3: {
        const int32_t t = ms(*this, rom::MOD_DELAY, 3);
        const int32_t d = delay[MOD_SLOTS[0]] + t + 1;
        delay[MOD_SLOTS[2]] = d;
        delay[MOD_SLOTS[1]] = d;
        const int32_t d2 = t + delay[MOD_SLOTS[5]] + 1;
        delay[MOD_SLOTS[7]] = d2;
        delay[MOD_SLOTS[6]] = d2;
        writeCoefs(2, MOD_WORDS + 12);
        writeDelays(2, MOD_SLOTS + 1);
        writeDelays(2, MOD_SLOTS + 6);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MOD_WORDS, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, MOD_WORDS);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 13:
        storeBipolar(273, P(*this, 13), BAL_STEP_C);
        break;
    case 255:
        coef[MOD_WORDS[12]] = 0x7FFF;
        coef[MOD_WORDS[13]] = 0;
        delay[MOD_SLOTS[5]] = half_area();
        hold = true;
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10, 13})
            fxInsFlanger(q);
        hold = false;
        break;
    }
}

// type 4; msb 0 lsb 13
void EffectHost::fxInsSymphonic(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = int(((5u * PU(*this, 1)) >> 2) >> 1);
        storeDepth(x, MOD_LEVEL);
        storeDepth(x, MOD_LEVEL + 2);
        storeDepth(x, MOD_LEVEL + 4);
        const int y = int(((3u * PU(*this, 1)) >> 2) >> 1);
        storeDepth(y, MOD_LEVEL + 1);
        storeDepth(y, MOD_LEVEL + 3);
        storeDepth(y, MOD_LEVEL + 5);
        break;
    }
    case 2: {
        const int32_t d = ms(*this, rom::MOD_DELAY, 2) + delay[MOD_SLOTS[0]];
        delay[MOD_SLOTS[2]] = d + 2;
        delay[MOD_SLOTS[1]] = d + 2;
        delay[MOD_SLOTS[4]] = d + 66;
        delay[MOD_SLOTS[3]] = d + 66;
        delay[MOD_SLOTS[7]] = d + 130;
        delay[MOD_SLOTS[6]] = d + 130;
        writeDelays(4, MOD_SLOTS + 1);
        writeDelays(2, MOD_SLOTS + 6);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MOD_WORDS, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, MOD_WORDS);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 5, 7, 9, 10})
            fxInsSymphonic(q);
        hold = false;
        break;
    }
}

// type 9; msb 1 lsb 1
void EffectHost::fxInsRotarySp(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = PU(*this, 1) < 32 ? (PLO(*this, 1) << 1) : 64;
        storeNegDepth(x, ROT_LEVEL);
        storeNegDepth(x, ROT_LEVEL + 2);
        storeNegDepth(x, ROT_LEVEL + 4);
        const int y = int(PLOU(*this, 1) >> 2);
        storeDepth(y, ROT_LEVEL + 1);
        storeDepth(y, ROT_LEVEL + 3);
        storeDepth(y, ROT_LEVEL + 5);
        storeDepth(y, ROT_LEVEL + 7);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 9:
        wet_words(*this, ROT_WORDS, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, ROT_WORDS);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_ROT, EQ_ROT_C);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 5, 7, 9, 10})
            fxInsRotarySp(q);
        hold = false;
        break;
    }
}

// type 10; msb 1 lsb 2; no parameter 9 case
void EffectHost::fxIns2WayRotary(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        storeNegDepth(P(*this, 1), ROT_LEVEL);
        storeNegDepth(P(*this, 1), ROT_LEVEL + 4);
        storeDepth(int((PLOU(*this, 1) >> 2) >> 1), ROT_LEVEL + 1);
        storeDepth(int((PLOU(*this, 1) >> 2) >> 1), ROT_LEVEL + 5);
        break;
    case 2:
        storeNegDepth(P(*this, 2), ROT_LEVEL + 2);
        storeNegDepth(P(*this, 2), ROT_LEVEL + 6);
        storeDepth(int((PLOU(*this, 2) >> 2) >> 2), ROT_LEVEL + 3);
        storeDepth(int((PLOU(*this, 2) >> 2) >> 2), ROT_LEVEL + 7);
        break;
    case 3: {
        const uint32_t a = rom::PAN_LAW[128 - PU(*this, 3)];
        coef[ROT_PAN[3]] = a;
        coef[ROT_PAN[0]] = a;
        const uint32_t b = rom::PAN_LAW[PU(*this, 3)];
        coef[ROT_PAN[2]] = b;
        coef[ROT_PAN[1]] = b;
        writeCoefs(4, ROT_PAN);
        break;
    }
    case 5: case 6:
        designFilter(1, 2, 2, P(*this, 5), P(*this, 6), 0, HPF_ROT);
        break;
    case 7: case 8:
        designFilter(1, 2, 3, P(*this, 7), P(*this, 8), 0, HPF_ROT_C);
        break;
    case 10:
        designFilter(1, 1, 0, P(*this, 10), 64, 7, LPF_ROT);
        designFilter(1, 1, 1, P(*this, 10), 64, 7, LPF_ROT_C);
        break;
    case 11:
        storeBipolar(273, PLO(*this, 11) + 64, BAL_STEP_B);
        storeBipolar(273, PLO(*this, 11) + 4, BAL_STEP_A);
        break;
    case 255: {
        const uint32_t x = typeGain(INSERTION, 8192);
        coef[ROT_WORDS[3]] = x;
        coef[ROT_WORDS[2]] = x;
        hold = true;
        designFilter(2, 1, 5, 8, 64, 7, EQ_ROT);
        designFilter(2, 1, 10, 45, 70, 10, EQ_ROT_C);
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10, 11})
            fxIns2WayRotary(q);
        hold = false;
        break;
    }
    }
}

// type 11; msb 1 lsb 3
void EffectHost::fxInsTremolo(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        storeNegDepth(P(*this, 1), ROT_LEVEL);
        storeNegDepth(P(*this, 1), ROT_LEVEL + 2);
        break;
    case 2:
        storeDepth(int(PLOU(*this, 2) >> 2), ROT_LEVEL + 1);
        storeDepth(int(PLOU(*this, 2) >> 2), ROT_LEVEL + 3);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_ROT, EQ_ROT_C);
        break;
    case 13:
        storeBipolar(273, P(*this, 13), BAL_STEP_A);
        break;
    case 14: {
        auto id = [&](int i) { return TREM_SLOTS[i]; };
        if (P(*this, 14) == 0) {
            coef[TREM_WORDS[0]] = 0x4000;
            coef[TREM_WORDS[1]] = 0x4000;
            delay[id(5)] = 0;
            delay[id(6)] = delay[id(0)] + 1;
            delay[id(7)] = delay[id(0)] + 1;
        } else {
            coef[TREM_WORDS[0]] = 0x7FFF;
            coef[TREM_WORDS[1]] = 0;
            delay[id(5)] = 1024;
            delay[id(6)] = 1025;
            delay[id(7)] = 1025;
        }
        writeCoefs(2, TREM_WORDS);
        writeDelays(3, TREM_SLOTS + 5);
        break;
    }
    case 255: {
        const uint32_t x = typeGain(INSERTION, 8192);
        coef[ROT_WORDS[3]] = x;
        coef[ROT_WORDS[2]] = x;
        hold = true;
        for (int q : {0, 1, 2, 5, 7, 10, 13, 14})
            fxInsTremolo(q);
        hold = false;
        break;
    }
    }
}

// type 12; msb 1 lsb 4; parameter 3 re-runs 1 and 2; no parameter 9 case
void EffectHost::fxInsAutoPan(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        storeNegDepth(P(*this, 1), ROT_LEVEL + 4);
        storeNegDepth(P(*this, 1), ROT_LEVEL + 6);
        break;
    case 2: {
        const int32_t m = P(*this, 3);
        const int y = (m == 3 || m == 4) ? int(PLOU(*this, 2) >> 2) : 0;
        storeDepth(y, ROT_LEVEL + 5);
        storeDepth(y, ROT_LEVEL + 7);
        break;
    }
    case 3: {
        const uint32_t m = PU(*this, 3);
        if (m < 5) {
            static const int A[5] = {64, 84, 64, 94, 64};
            static const int B[5] = {124, 64, 84, 64, 94};
            storeBipolar(273, A[m], BAL_STEP_B);
            storeBipolar(273, B[m], BAL_STEP_D);
            storeModePair(1, MODE_STEPS_A);
            storeModePair(1, MODE_STEPS_B);
        } else {
            storeBipolar(273, 64, BAL_STEP_B);
            storeBipolar(273, 124, BAL_STEP_D);
            storeModePair(2, MODE_STEPS_A);
            storeModePair(2, MODE_STEPS_B);
        }
        fxInsAutoPan(1);
        fxInsAutoPan(2);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_ROT, EQ_ROT_C);
        break;
    case 255: {
        const uint32_t x = typeGain(INSERTION, 8192);
        coef[ROT_WORDS[3]] = x;
        coef[ROT_WORDS[2]] = x;
        hold = true;
        for (int q : {0, 3, 5, 7, 9, 10})
            fxInsAutoPan(q);
        hold = false;
        break;
    }
    }
}

// type 13; msb 1 lsb 5
void EffectHost::fxInsAmbience(int p)
{
    switch (p) {
    case 0: {
        const uint32_t t = uint32_t(ms(*this, rom::MOD_DELAY, 0)) & 0xFFFF;
        const int32_t d = int32_t(uint32_t(delay[TREM_SLOTS[0]]) + t + 1);
        delay[TREM_SLOTS[7]] = d;
        delay[TREM_SLOTS[6]] = d;
        delay[TREM_SLOTS[2]] = d;
        delay[TREM_SLOTS[1]] = d;
        writeDelays(2, TREM_SLOTS + 1);
        writeDelays(2, TREM_SLOTS + 6);
        break;
    }
    case 1:
        if (PU(*this, 1) == 1) {
            coef[ROT_PAN[0]] = 0x8000;
            coef[ROT_PAN[1]] = 0x7FFF;
        } else {
            coef[ROT_PAN[0]] = 0x7FFF;
            coef[ROT_PAN[1]] = 0x8000;
        }
        writeCoefs(2, ROT_PAN);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 9:
        wet_words(*this, ROT_WORDS, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, ROT_WORDS);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 5, 7, 9})
            fxInsAmbience(q);
        hold = false;
        break;
    }
}

// type 5; msb 2 lsb 1; 10 (stages) re-runs 11, 11 (direction) re-runs 9
void EffectHost::fxInsPhaser1(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const uint32_t w = P(*this, 2) * 258;
        coef[PH_FB[1]] = w;
        coef[PH_FB[0]] = w;
        const int32_t d = int32_t(PU(*this, 1) * uint32_t(32767 - int16_t(w))) / 127;
        coef[PH_DEPTH[1]] = d;
        coef[PH_DEPTH[0]] = d;
        writeCoefs(2, PH_FB);
        writeCoefs(2, PH_DEPTH);
        break;
    }
    case 3:
        coef[PH_WIDTH1[0]] = bipolar500(P(*this, 3));
        writeCoefs(1, PH_WIDTH1);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_PH, LPF_PH_C, HPF_PH, HPF_PH_C);
        break;
    case 9: {
        const uint32_t wet = PU(*this, 9);
        const uint32_t g = rom::PAN_LAW[128 - wet] >> 2;
        coef[PH_WET[2]] = g;
        coef[PH_WET[0]] = g;
        int32_t x = typeGain(INSERTION, rom::PAN_LAW[wet] >> 2);
        coef[PH_WET[1]] = x;
        if (P(*this, 11) != 0)
            x = -int16_t(x);
        coef[PH_WET[3]] = x;
        writeCoefs(4, PH_WET);
        break;
    }
    case 10: {
        static const int PAIRS[9][2] = {{0, 5}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {1, 9}, {2, 9}, {3, 9}, {4, 9}};
        const uint32_t n = PU(*this, 10);
        const int k = n < 5 ? 0 : n < 12 ? int(n) - 4 : 8;
        for (int i = 0; i < 10; i++)
            coef[PH_STAGES[i]] = (i == PAIRS[k][0] || i == PAIRS[k][1]) ? 0x4000 : 0;
        writeCoefs(10, PH_STAGES);
        fxInsPhaser1(11);
        break;
    }
    case 11: {
        uint32_t w[3] = {0, 0, 0};
        if (P(*this, 11) == 0) w[0] = 0x4000;
        else if (PU(*this, 10) > 8) w[2] = 0x4000;
        else w[1] = 0x4000;
        for (int i = 0; i < 3; i++)
            coef[PH_DIR[i]] = w[i];
        writeCoefs(3, PH_DIR);
        fxInsPhaser1(9);
        break;
    }
    case 255:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 10})
            fxInsPhaser1(q);
        hold = false;
        break;
    }
}

// type 6; msb 2 lsb 2
void EffectHost::fxInsPhaser2(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const uint32_t w = P(*this, 2) * 258;
        coef[PH_FB[0]] = w;
        coef[PH_DEPTH[0]] = int32_t(PU(*this, 1) * uint32_t(32767 - int16_t(w))) / 127;
        const int32_t v10 = 242 * P(*this, 2);
        coef[PH_FB[1]] = v10;
        coef[PH_DEPTH[1]] = int32_t(PU(*this, 1) * uint32_t(32767 - int16_t(v10))) / 127;
        writeCoefs(2, PH_FB);
        writeCoefs(2, PH_DEPTH);
        break;
    }
    case 3: {
        const uint32_t x = bipolar500(P(*this, 3));
        coef[PH_WIDTH2[1]] = x;
        coef[PH_WIDTH2[0]] = x;
        writeCoefs(2, PH_WIDTH2);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_PH, LPF_PH_C, HPF_PH, HPF_PH_C);
        break;
    case 9:
        wet_words(*this, PH_WET, rom::PAN_LAW, PU(*this, 9), GXGX);
        writeCoefs(4, PH_WET);
        break;
    case 10: {
        const uint32_t n = PU(*this, 10);
        int a, b;
        if (n <= 3) { a = 1; b = 6; }
        else if (n == 4) { a = 2; b = 7; }
        else if (n == 5) { a = 3; b = 8; }
        else { a = 4; b = 9; }
        for (int i = 0; i < 10; i++)
            coef[PH_STAGES[i]] = (i == a || i == b) ? 0x4000 : 0;
        writeCoefs(10, PH_STAGES);
        break;
    }
    case 12:
        storeBipolar(546, P(*this, 12), PH_OFFSET);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 9, 10, 12})
            fxInsPhaser2(q);
        hold = false;
        break;
    }
}

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
    case 255:
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
    case 255:
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
    case 255:
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
    case 255:
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
        designFilter(2, 1, 1, P(*this, 5), 64, 0, ER_HPF);
        break;
    case 6:
        designFilter(2, 2, 0, P(*this, 6), 64, 0, ER_LPF_2);
        break;
    case 9:
        er_wet(*this);
        break;
    case 11:
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeCoefs(16, ER_ROW_STEPS);
        break;
    case 12:
        designFilter(1, 0, 0, rom::DAMP_FREQ[PU(*this, 12)], 64, 0, ER_LPF);
        break;
    case 255:
        hold = true;
        for (int q : {0, 2, 4, 5, 6, 9, 12})
            fxInsCrossDelayEr(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

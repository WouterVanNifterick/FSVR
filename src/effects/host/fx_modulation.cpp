// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The modulation effects: chorus, flanger, symphonic, the phasers, rotary speaker, tremolo,
// auto pan and ensemble detune of the variation and the insertion, and the insertion's 2-way
// rotary, ambience and pitch change.
#include "effects/host/fx_common.h"
#include <algorithm>
#include <cstdlib>

namespace fs1r::vop3host {

namespace {

// What the chorus, flanger and symphonic of a block write to
struct Chorus {
    const uint16_t *level, *feedback, *mix, *wet, *phase;   // 6, 2, 2, 4 and 1 steps
    const uint8_t* slots;                                   // 10 delay slots
    Shelves shelves;
    const uint16_t *eqMid, *eqMidCopy;                      // the insertion's mid band
};
const Chorus CHORUS[2] = {
    {var::MOD_LEVEL_STEPS, var::FB_STEPS,      var::MIX_STEPS,      var::PAN_STEPS, var::BAL_STEP_C, var::MOD_SLOTS,
     {var::LPF_A,   var::LPF_A_COPY, var::HPF_A,   var::HPF_A_COPY}, nullptr, nullptr},
    {ins::MOD_LEVEL,       ins::MOD_WORDS + 8, ins::MOD_WORDS + 12, ins::MOD_WORDS, ins::BAL_STEP_C, ins::MOD_SLOTS,
     {ins::LPF_MOD, ins::LPF_MOD_C,  ins::HPF_MOD, ins::HPF_MOD_C},  ins::EQ_MOD, ins::EQ_MOD_C},
};

// What the rotary speaker, tremolo and auto pan of a block write to. Their depths go to every other step.
struct Rotary {
    const uint16_t *amDepth, *pmDepth;             // the rotary's and the tremolo's
    const uint16_t *lrDepth, *frDepth;             // the auto pan's
    const uint16_t *tremoloPhase, *panBalance[2], *panMode[2];
    const uint16_t* wet;
    const uint16_t* tremoloMix;
    const uint8_t* tremoloSlots;
    Shelves shelves;
    const uint16_t *eqMid, *eqMidCopy;
};
const Rotary ROTARY[2] = {
    {var::ROT_STEPS + 1, var::ROT_STEPS,     var::ROT_STEPS + 1, var::ROT_STEPS,
     var::BAL_STEP_B, {var::BAL_STEP_A, var::BAL_STEP_B}, {var::MODE_STEPS_A, var::MODE_STEPS_B},
     var::PAN_STEPS_B, var::TREM_WORDS, var::TREM_SLOTS,
     {var::LPF_B,   var::LPF_B_COPY, var::HPF_B,   var::HPF_B_COPY}, nullptr, nullptr},
    {ins::ROT_LEVEL,     ins::ROT_LEVEL + 1, ins::ROT_LEVEL + 4, ins::ROT_LEVEL + 5,
     ins::BAL_STEP_A, {ins::BAL_STEP_B, ins::BAL_STEP_D}, {ins::MODE_STEPS_A, ins::MODE_STEPS_B},
     ins::ROT_WORDS,   ins::TREM_WORDS, ins::TREM_SLOTS,
     {ins::LPF_ROT, ins::LPF_ROT_C,  ins::HPF_ROT, ins::HPF_ROT_C},  ins::EQ_ROT, ins::EQ_ROT_C},
};

// What the phasers of a block write to. The variation's modulate through its LFOs' depth, the
// insertion's through a pair of constants.
struct Phaser {
    int stages;                                    // 8 or 10
    const uint16_t *feedback, *depth, *width1, *width2, *stageSteps, *direction, *wet, *offset;
    Shelves shelves;
};
const Phaser PHASER[2] = {
    {8,  var::PH_FB, nullptr,       var::PH_WIDTH1, var::PH_WIDTH2, var::PH_STAGES, var::PH_DIR, var::PH_PAN, nullptr,
     {var::PH_FILTER, var::PH_FILTER_3, var::PH_FILTER_2, var::PH_FILTER_4}},
    {10, ins::PH_FB, ins::PH_DEPTH, ins::PH_WIDTH1, ins::PH_WIDTH2, ins::PH_STAGES, ins::PH_DIR, ins::PH_WET, ins::PH_OFFSET,
     {ins::LPF_PH,    ins::LPF_PH_C,    ins::HPF_PH,    ins::HPF_PH_C}},
};

// What the ensemble detune of a block writes to
struct Detune {
    const uint16_t* steps;
    const uint8_t* slots;
    const uint16_t* wet;
    Shelves shelves;
};
const Detune DETUNE[2] = {
    {var::DETUNE_DET,    var::DETUNE_SLOTS, var::DETUNE_PAN,
     {var::DETUNE_LOW_SHELF, var::DETUNE_LOW_SHELF_COPY, var::DETUNE_HIGH_SHELF, var::DETUNE_HIGH_SHELF_COPY}},
    {ins::LFO_DIR_STEPS, ins::PC_SLOTS,     ins::PC_MIX + 3,
     {ins::EQ7_A,            ins::EQ7_A_C,               ins::EQ7_C,             ins::EQ7_C_C, 2, 4}},
};

// the insertion's mid band, parameters 10, 11 and 12
void eqMid(EffectHost& h, const uint16_t* steps, const uint16_t* copy)
{
    const uint16_t* P = h.param[INSERTION];
    h.designFilter(2, 1, PEAK, P[10], P[11], P[12], steps);
    h.copyCoefs(5, steps, copy);
}

}  // namespace

void EffectHost::chorus(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, FB_LEVEL, DELAY_OFFSET, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, EQ_MID_FREQ = 10, MODE = 14 };
    const Chorus& b = CHORUS[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case LFO_DEPTH:
            for (int i : {0, 2, 4})
                storeDepth((P[LFO_DEPTH] & 0xFF) << 1, b.level + i);
            break;
        case FB_LEVEL:
            coef[b.feedback[1]] = coef[b.feedback[0]] = bipolar376(P[FB_LEVEL]);
            writeCoefs(2, b.feedback);
            break;
        case DELAY_OFFSET:
        case MODE: {
            // two taps a channel; in stereo the second channel's delays sit in the upper half
            const bool stereo = P[MODE] != 0;
            const int32_t t = toSamples(rom::MOD_DELAY[P[DELAY_OFFSET]]);
            int32_t base = delay[b.slots[0]];
            delay[b.slots[2]] = delay[b.slots[1]] = base + t + 1;
            delay[b.slots[4]] = delay[b.slots[3]] = base + t + 65;
            coef[b.mix[0]] = stereo ? UNITY : HALF;
            coef[b.mix[1]] = stereo ? 0 : HALF;
            if (stereo)
                base = HALF_AREA;
            delay[b.slots[5]] = stereo ? base : 0;
            delay[b.slots[7]] = delay[b.slots[6]] = base + t + 1;
            delay[b.slots[9]] = delay[b.slots[8]] = base + t + 65;
            writeCoefs(2, b.mix);
            writeDelays(4, b.slots + 1);
            writeDelays(5, b.slots + 5);
            break;
        }
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case ALL:
            applyAll(w, &EffectHost::chorus, {LFO_FREQ, LFO_DEPTH, FB_LEVEL, DELAY_OFFSET, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, EQ_MID_FREQ});
            break;
    }
}

void EffectHost::flanger(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, FB_LEVEL, DELAY_OFFSET, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, EQ_MID_FREQ = 10, LFO_PHASE = 13 };
    const Chorus& b = CHORUS[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case LFO_DEPTH:
            storeDepth(P[LFO_DEPTH], b.level);
            storeDepth(P[LFO_DEPTH], b.level + 2);
            break;
        case FB_LEVEL:
            coef[b.feedback[1]] = coef[b.feedback[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(2, b.feedback);
            break;
        case DELAY_OFFSET: {
            const int32_t t = toSamples(rom::MOD_DELAY[P[DELAY_OFFSET]]);
            delay[b.slots[2]] = delay[b.slots[1]] = delay[b.slots[0]] + t + 1;
            delay[b.slots[7]] = delay[b.slots[6]] = delay[b.slots[5]] + t + 1;
            writeCoefs(2, b.mix);
            writeDelays(2, b.slots + 1);
            writeDelays(2, b.slots + 6);
            break;
        }
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case LFO_PHASE:
            storeBipolar(273, P[LFO_PHASE], b.phase);
            break;
        case ALL:
            if (w == INSERTION) {                   // the variation keeps its preset's mix
                coef[b.mix[0]] = UNITY;
                coef[b.mix[1]] = 0;
            }
            delay[b.slots[5]] = HALF_AREA;
            applyAll(w, &EffectHost::flanger, {LFO_FREQ, LFO_DEPTH, FB_LEVEL, DELAY_OFFSET, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, EQ_MID_FREQ, LFO_PHASE});
            break;
    }
}

void EffectHost::symphonic(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, DELAY_OFFSET, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, EQ_MID_FREQ = 10 };
    const Chorus& b = CHORUS[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case LFO_DEPTH:
            for (int i : {0, 2, 4})
                storeDepth((5 * P[LFO_DEPTH]) >> 3, b.level + i);
            for (int i : {1, 3, 5})
                storeDepth((3 * P[LFO_DEPTH]) >> 3, b.level + i);
            break;
        case DELAY_OFFSET: {
            const int32_t d = toSamples(rom::MOD_DELAY[P[DELAY_OFFSET]]) + delay[b.slots[0]];
            delay[b.slots[2]] = delay[b.slots[1]] = d + 2;
            delay[b.slots[4]] = delay[b.slots[3]] = d + 66;
            delay[b.slots[7]] = delay[b.slots[6]] = d + 130;
            writeDelays(4, b.slots + 1);
            writeDelays(2, b.slots + 6);
            break;
        }
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case ALL:
            applyAll(w, &EffectHost::symphonic, {LFO_FREQ, LFO_DEPTH, DELAY_OFFSET, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, EQ_MID_FREQ});
            break;
    }
}

void EffectHost::phaser1(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, PHASE_SHIFT, FB_LEVEL, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, STAGE = 10, DIFFUSE = 11 };
    // the two stage words open for each stage count: the first and the middle one, moving out
    static const uint8_t OPEN8[7][2]  = {{0, 4}, {4, 1}, {4, 2}, {4, 3}, {1, 7}, {2, 7}, {3, 7}};
    static const uint8_t OPEN10[9][2] = {{0, 5}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {1, 9}, {2, 9}, {3, 9}, {4, 9}};
    const Phaser& b = PHASER[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            if (w == VARIATION)
                setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            setLfoSpeed(P[LFO_FREQ], lfo(w, 1));
            break;
        case LFO_DEPTH:
        case PHASE_SHIFT: {
            const uint32_t shift = P[PHASE_SHIFT] * 258;
            coef[b.feedback[1]] = coef[b.feedback[0]] = shift;
            if (w == VARIATION) {
                writeCoefs(2, b.feedback);
                const int32_t d = int32_t(P[LFO_DEPTH] * (127u - P[PHASE_SHIFT])) / 127;
                setLfoDepth(d, lfo(w, 0));
                setLfoDepth(d, lfo(w, 1));
            } else {
                coef[b.depth[1]] = coef[b.depth[0]] = int32_t(P[LFO_DEPTH] * uint32_t(UNITY - int16_t(shift))) / 127;
                writeCoefs(2, b.feedback);
                writeCoefs(2, b.depth);
            }
            break;
        }
        case FB_LEVEL:
            coef[b.width1[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, b.width1);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET: {
            // diffuse turns the right channel's wet over
            const uint32_t wet = w == INSERTION ? P[DRY_WET] : 127;
            coef[b.wet[2]] = coef[b.wet[0]] = rom::PAN_LAW[128 - wet] >> 2;
            int32_t x = typeGain(w, rom::PAN_LAW[wet] >> 2);
            coef[b.wet[1]] = x;
            if (P[DIFFUSE] != 0)
                x = -int16_t(x);
            coef[b.wet[3]] = x;
            writeCoefs(4, b.wet);
            break;
        }
        case STAGE: {
            const int n = P[STAGE];
            const uint8_t* open = b.stages == 8 ? OPEN8[n < 5 ? 0 : std::min(n - 4, 6)] : OPEN10[n < 5 ? 0 : std::min(n - 4, 8)];
            for (int i = 0; i < b.stages; i++)
                coef[b.stageSteps[i]] = i == open[0] || i == open[1] ? HALF : 0;
            writeCoefs(b.stages, b.stageSteps);
            phaser1(w, DIFFUSE);
            break;
        }
        case DIFFUSE: {
            uint16_t d[3] = {0, 0, 0};
            d[P[DIFFUSE] == 0 ? 0 : P[STAGE] >= 4 + b.stages / 2 ? 2 : 1] = HALF;
            for (int i = 0; i < 3; i++)
                coef[b.direction[i]] = d[i];
            writeCoefs(3, b.direction);
            phaser1(w, DRY_WET);
            break;
        }
        case ALL:
            applyAll(w, &EffectHost::phaser1, {LFO_FREQ, LFO_DEPTH, FB_LEVEL, EQ_LOW_FREQ, EQ_HIGH_FREQ, STAGE});
            break;
    }
}

void EffectHost::phaser2(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, PHASE_SHIFT, FB_LEVEL, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, STAGE = 10, LFO_PHASE = 12 };
    const Phaser& b = PHASER[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            if (w == VARIATION)
                setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            setLfoSpeed(P[LFO_FREQ], lfo(w, 1));
            break;
        case LFO_DEPTH:
        case PHASE_SHIFT: {
            // the two channels' shifts, 258 and 242 a step
            const uint32_t a = P[PHASE_SHIFT] * 258, c = 242 * P[PHASE_SHIFT];
            coef[b.feedback[0]] = a;
            coef[b.feedback[1]] = c;
            if (w == VARIATION) {
                writeCoefs(2, b.feedback);
                setLfoDepth(int32_t(P[LFO_DEPTH] * (127u - P[PHASE_SHIFT])) / 127, lfo(w, 0));
                setLfoDepth(int32_t(P[LFO_DEPTH] * (127u - ((15u * P[PHASE_SHIFT]) >> 4))) / 127, lfo(w, 1));
            } else {
                coef[b.depth[0]] = int32_t(P[LFO_DEPTH] * uint32_t(UNITY - int16_t(a))) / 127;
                coef[b.depth[1]] = int32_t(P[LFO_DEPTH] * uint32_t(UNITY - int16_t(c))) / 127;
                writeCoefs(2, b.feedback);
                writeCoefs(2, b.depth);
            }
            break;
        }
        case FB_LEVEL:
            coef[b.width2[1]] = coef[b.width2[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(2, b.width2);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet, true);
            break;
        case STAGE: {
            // a stage word and its partner half the list on
            const int k = std::min(std::max(P[STAGE] - 2, 1), b.stages / 2 - 1);
            for (int i = 0; i < b.stages; i++)
                coef[b.stageSteps[i]] = i == k || i == k + b.stages / 2 ? HALF : 0;
            writeCoefs(b.stages, b.stageSteps);
            break;
        }
        case LFO_PHASE:
            if (w == VARIATION)
                setLfoPhase(P[LFO_PHASE], lfo(w, 1));
            else
                storeBipolar(546, P[LFO_PHASE], b.offset);
            break;
        case ALL:
            applyAll(w, &EffectHost::phaser2, {LFO_FREQ, LFO_DEPTH, FB_LEVEL, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, STAGE, LFO_PHASE});
            break;
    }
}

void EffectHost::ensembleDetune(int w, int p)
{
    enum { DETUNE_AMOUNT, INIT_DELAY_L, INIT_DELAY_R, DRY_WET = 9, EQ_LOW_FREQ = 10, EQ_HIGH_FREQ = 12 };
    const Detune& b = DETUNE[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case DETUNE_AMOUNT: {
            // two sawtooths running opposite ways, which way round by the sign of the detune
            const uint32_t v = P[DETUNE_AMOUNT];
            const double K = 65527.48886;
            const int lo = lfo(w, 0), hi = lfo(w, 1);
            if (v == 64) {
                for (int i = 0; i < 4; i++)
                    coef[b.steps[i]] = 0;
            } else {
                const bool down = v < 64;
                const double a = double(rom::DETUNE[114u - v]) * K, c = double(rom::DETUNE[v - 14u]) * K;
                const int32_t w0 = int(down ? a + -K : -a + K), w1 = int(down ? K - c : c + -K);
                coef[b.steps[1]] = coef[b.steps[0]] = int16_t(w0);
                coef[b.steps[3]] = coef[b.steps[2]] = int16_t(w1);
                setLfoWave(lo, down ? SAW_DOWN : SAW_UP);
                lfoReg[3][lo] = down ? UNITY : 0;
                setLfoWave(hi, down ? SAW_UP : SAW_DOWN);
                lfoReg[3][hi] = down ? 0 : UNITY;
            }
            writeCoefs(4, b.steps);
            writeLfo(LFO_WAVE, lo);
            writeLfo(LFO_WAVE, hi);
            writeLfo(LFO_DIRECTION, lo);
            writeLfo(LFO_DIRECTION, hi);
            break;
        }
        case INIT_DELAY_L: {
            const int32_t d = toSamples(rom::MOD_DELAY[P[INIT_DELAY_L]]) + delay[b.slots[0]] + 1;
            for (int i = 1; i <= 4; i++)
                delay[b.slots[i]] = d;
            writeDelays(5, b.slots);
            break;
        }
        case INIT_DELAY_R: {
            delay[b.slots[5]] = HALF_AREA;
            const int32_t d = toSamples(rom::MOD_DELAY[P[INIT_DELAY_R]]) + HALF_AREA + 1;
            for (int i = 6; i <= 9; i++)
                delay[b.slots[i]] = d;
            writeDelays(5, b.slots + 5);
            break;
        }
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case ALL:
            applyAll(w, &EffectHost::ensembleDetune, {DETUNE_AMOUNT, INIT_DELAY_L, INIT_DELAY_R, DRY_WET, EQ_LOW_FREQ, EQ_HIGH_FREQ});
            break;
    }
}

void EffectHost::rotarySpeaker(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9, EQ_MID_FREQ = 10 };
    const Rotary& b = ROTARY[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case LFO_DEPTH:
            for (int i : {0, 2, 4})
                storeNegDepth(P[LFO_DEPTH] < 32 ? int8_t(P[LFO_DEPTH]) << 1 : 64, b.amDepth + i);
            for (int i : {0, 2, 4, 6})
                storeDepth((P[LFO_DEPTH] & 0xFF) >> 2, b.pmDepth + i);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case ALL:
            applyAll(w, &EffectHost::rotarySpeaker, {LFO_FREQ, LFO_DEPTH, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, EQ_MID_FREQ});
            break;
    }
}

void EffectHost::tremolo(int w, int p)
{
    enum { LFO_FREQ, AM_DEPTH, PM_DEPTH, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, EQ_MID_FREQ = 10, LFO_PHASE = 13, MODE = 14 };
    const Rotary& b = ROTARY[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case AM_DEPTH:
            storeNegDepth(P[AM_DEPTH], b.amDepth);
            storeNegDepth(P[AM_DEPTH], b.amDepth + 2);
            break;
        case PM_DEPTH:
            storeDepth((P[PM_DEPTH] & 0xFF) >> 2, b.pmDepth);
            storeDepth((P[PM_DEPTH] & 0xFF) >> 2, b.pmDepth + 2);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case LFO_PHASE:
            storeBipolar(273, P[LFO_PHASE], b.tremoloPhase);
            break;
        case MODE: {
            const uint8_t* s = b.tremoloSlots;
            if (P[MODE] == 0) {
                coef[b.tremoloMix[0]] = HALF;
                coef[b.tremoloMix[1]] = HALF;
                delay[s[5]] = 0;
                delay[s[6]] = delay[s[0]] + 1;
                delay[s[7]] = delay[s[0]] + 1;
            } else {
                coef[b.tremoloMix[0]] = UNITY;
                coef[b.tremoloMix[1]] = 0;
                if (w == VARIATION)                 // its wet goes up with it
                    coef[b.wet[3]] = coef[b.wet[2]] = typeGain(w, UNITY);
                delay[s[5]] = 1024;
                delay[s[6]] = 1025;
                delay[s[7]] = 1025;
            }
            writeCoefs(2, b.tremoloMix);
            writeDelays(3, s + 5);
            break;
        }
        case ALL:
            coef[b.wet[3]] = coef[b.wet[2]] = typeGain(w, QUARTER);
            applyAll(w, &EffectHost::tremolo, {LFO_FREQ, AM_DEPTH, PM_DEPTH, EQ_LOW_FREQ, EQ_HIGH_FREQ, EQ_MID_FREQ, LFO_PHASE, MODE});
            break;
    }
}

void EffectHost::autoPan(int w, int p)
{
    enum { LFO_FREQ, LR_DEPTH, FR_DEPTH, PAN_DIRECTION, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, EQ_MID_FREQ = 10 };
    const Rotary& b = ROTARY[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, 0));
            break;
        case LR_DEPTH:
            storeNegDepth(P[LR_DEPTH], b.lrDepth);
            storeNegDepth(P[LR_DEPTH], b.lrDepth + 2);
            break;
        case FR_DEPTH: {
            // only the two directions that turn front to back
            const int d = P[PAN_DIRECTION] == 3 || P[PAN_DIRECTION] == 4 ? (P[FR_DEPTH] & 0xFF) >> 2 : 0;
            storeDepth(d, b.frDepth);
            storeDepth(d, b.frDepth + 2);
            break;
        }
        case PAN_DIRECTION: {
            static const int A[5] = {64, 84, 64, 94, 64};
            static const int B[5] = {124, 64, 84, 64, 94};
            const uint32_t m = P[PAN_DIRECTION];
            storeBipolar(273, m < 5 ? A[m] : 64, b.panBalance[0]);
            storeBipolar(273, m < 5 ? B[m] : 124, b.panBalance[1]);
            storeModePair(m < 5 ? 1 : 2, b.panMode[0]);
            storeModePair(m < 5 ? 1 : 2, b.panMode[1]);
            autoPan(w, LR_DEPTH);
            autoPan(w, FR_DEPTH);
            break;
        }
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case EQ_MID_FREQ: case EQ_MID_FREQ + 1: case EQ_MID_FREQ + 2:
            if (b.eqMid)
                eqMid(*this, b.eqMid, b.eqMidCopy);
            break;
        case ALL:
            coef[b.wet[3]] = coef[b.wet[2]] = typeGain(w, QUARTER);
            applyAll(w, &EffectHost::autoPan, {LFO_FREQ, PAN_DIRECTION, EQ_LOW_FREQ, EQ_HIGH_FREQ, EQ_MID_FREQ});
            break;
    }
}

// ---- the insertion's own ----

void EffectHost::twoWayRotary(int w, int p)
{
    enum { ROTOR_SPEED, DRIVE_LOW, DRIVE_HIGH, LOW_HIGH, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, CROSS_FREQ = 10, MIC_ANGLE = 11 };
    const uint16_t* P = param[w];
    const uint16_t* level = ins::ROT_LEVEL;
    switch (p) {
        case ROTOR_SPEED:
            setLfoSpeed(P[ROTOR_SPEED], lfo(w, 0));
            break;
        case DRIVE_LOW:
            storeNegDepth(P[DRIVE_LOW], level);
            storeNegDepth(P[DRIVE_LOW], level + 4);
            storeDepth((P[DRIVE_LOW] & 0xFF) >> 3, level + 1);
            storeDepth((P[DRIVE_LOW] & 0xFF) >> 3, level + 5);
            break;
        case DRIVE_HIGH:
            storeNegDepth(P[DRIVE_HIGH], level + 2);
            storeNegDepth(P[DRIVE_HIGH], level + 6);
            storeDepth((P[DRIVE_HIGH] & 0xFF) >> 4, level + 3);
            storeDepth((P[DRIVE_HIGH] & 0xFF) >> 4, level + 7);
            break;
        case LOW_HIGH:
            coef[ins::ROT_PAN[3]] = coef[ins::ROT_PAN[0]] = rom::PAN_LAW[128 - P[LOW_HIGH]];
            coef[ins::ROT_PAN[2]] = coef[ins::ROT_PAN[1]] = rom::PAN_LAW[P[LOW_HIGH]];
            writeCoefs(4, ins::ROT_PAN);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1:
            designFilter(1, 2, LOW_SHELF1, P[EQ_LOW_FREQ], P[EQ_LOW_FREQ + 1], 0, ins::HPF_ROT);
            break;
        case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            designFilter(1, 2, HIGH_SHELF1, P[EQ_HIGH_FREQ], P[EQ_HIGH_FREQ + 1], 0, ins::HPF_ROT_C);
            break;
        case CROSS_FREQ:
            designFilter(1, 1, LPF1, P[CROSS_FREQ], 64, 7, ins::LPF_ROT);
            designFilter(1, 1, HPF1, P[CROSS_FREQ], 64, 7, ins::LPF_ROT_C);
            break;
        case MIC_ANGLE:
            storeBipolar(273, int8_t(P[MIC_ANGLE]) + 64, ins::BAL_STEP_B);
            storeBipolar(273, int8_t(P[MIC_ANGLE]) + 4, ins::BAL_STEP_A);
            break;
        case ALL:
            coef[ins::ROT_WORDS[3]] = coef[ins::ROT_WORDS[2]] = typeGain(w, QUARTER);
            hold = true;
            designFilter(2, 1, HPF2, 8, 64, 7, ins::EQ_ROT);
            designFilter(2, 1, PEAK, 45, 70, 10, ins::EQ_ROT_C);
            for (int q : {ROTOR_SPEED, DRIVE_LOW, DRIVE_HIGH, LOW_HIGH, EQ_LOW_FREQ, EQ_HIGH_FREQ, CROSS_FREQ, MIC_ANGLE})
                twoWayRotary(w, q);
            hold = false;
            break;
    }
}

void EffectHost::ambience(int w, int p)
{
    enum { DELAY_TIME, PHASE, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9 };
    const uint16_t* P = param[w];
    const Rotary& b = ROTARY[1];
    switch (p) {
        case DELAY_TIME: {
            const uint8_t* s = ins::TREM_SLOTS;
            delay[s[7]] = delay[s[6]] = delay[s[2]] = delay[s[1]] = delay[s[0]] + toSamples(rom::MOD_DELAY[P[DELAY_TIME]]) + 1;
            writeDelays(2, s + 1);
            writeDelays(2, s + 6);
            break;
        }
        case PHASE: {
            const bool inverse = P[PHASE] == 1;
            coef[ins::ROT_PAN[0]] = inverse ? MINUS_ONE : UNITY;
            coef[ins::ROT_PAN[1]] = inverse ? UNITY : MINUS_ONE;
            writeCoefs(2, ins::ROT_PAN);
            break;
        }
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET:
            dryWet(w, b.wet);
            break;
        case ALL:
            applyAll(w, &EffectHost::ambience, {DELAY_TIME, PHASE, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET});
            break;
    }
}

namespace {

// a pitch change voice's LFO word, from the pitch and the voice's fine tuning
int32_t pitchRatio(const EffectHost& h, int fine)
{
    const int32_t d = fine - 64;
    const int32_t pitch = int16_t(h.param[INSERTION][0] - 64);
    if (pitch == 0)
        return rom::PITCH_FINE[std::abs(int16_t(d))];
    const int32_t semitones = rom::PITCH_SEMITONE[h.param[INSERTION][0] - 40];
    return pitch >= 0 ? semitones + d : semitones - d;
}

// one voice's LFO word pair, and its sawtooth's direction by the sign of the shift
void pitchVoice(EffectHost& h, int voice, int fine)
{
    const int lfo = h.lfo(INSERTION, voice);
    h.coef[ins::LFO_DIR_STEPS[2 * voice + 1]] = h.coef[ins::LFO_DIR_STEPS[2 * voice]] = pitchRatio(h, fine);
    h.writeCoefs(4, ins::LFO_DIR_STEPS);
    const bool up = 100 * (h.param[INSERTION][0] - 64) + int16_t(fine) - 64 > 0;
    h.setLfoWave(lfo, up ? SAW_DOWN : SAW_UP);
    h.lfoReg[3][lfo] = up ? UNITY : 0;
    h.writeLfo(LFO_WAVE, lfo);
    h.writeLfo(LFO_DIRECTION, lfo);
}

}  // namespace

void EffectHost::pitchChange(int w, int p)
{
    enum { PITCH, INIT_DELAY, FINE1, FINE2, FB_LEVEL, DRY_WET = 9, PAN1 = 10, LEVEL1, PAN2, LEVEL2 };
    const uint16_t* P = param[w];
    switch (p) {
        case PITCH: {
            pitchVoice(*this, 0, P[FINE1]);
            pitchVoice(*this, 1, P[FINE2]);
            const uint16_t speed = rom::PITCH_LFO_SPEED[P[PITCH] - 40];
            for (int i : {0, 1})
                lfoReg[0][lfo(w, i)] = (lfoReg[0][lfo(w, i)] & 0xFC00) | speed;
            writeLfo(LFO_SPEED, lfo(w, 0));
            writeLfo(LFO_SPEED, lfo(w, 1));
            break;
        }
        case INIT_DELAY: {
            // the first voice's taps, then the second's in the upper half
            const uint8_t* s = ins::PC_SLOTS;
            const int32_t t = toSamples(rom::PITCH_DELAY[P[INIT_DELAY]]);
            delay[s[2]] = delay[s[1]] = t + 1;
            delay[s[4]] = delay[s[3]] = t + 1 + 384;
            writeDelays(4, s + 1);
            delay[s[5]] = HALF_AREA;
            delay[s[7]] = delay[s[6]] = HALF_AREA + t + 1;
            delay[s[9]] = delay[s[8]] = HALF_AREA + t + 1 + 384;
            writeDelays(5, s + 5);
            break;
        }
        case FINE1:
            pitchVoice(*this, 0, P[FINE1]);
            break;
        case FINE2:
            pitchVoice(*this, 1, P[FINE2]);
            break;
        case FB_LEVEL: {
            const uint16_t* fb = ins::PC_FB;
            coef[fb[1]] = coef[fb[0]] = bipolar500(P[FB_LEVEL]);
            const uint32_t v = P[FB_LEVEL];
            if (v >= 32 && v <= 96) {
                coef[fb[4]] = coef[fb[2]] = uint16_t(-6144);
                coef[fb[5]] = coef[fb[3]] = uint16_t(-10240);
            } else {
                // towards either end the pair spreads, by up to 2048 either side of -8192
                const uint32_t i = uint32_t(int((v <= 31 ? double(v) - 1.0 : -double(v) + 127.0) * 2048.0 / 31.0)) & 0xFFFF;
                coef[fb[4]] = coef[fb[2]] = i - 8192u;
                coef[fb[5]] = coef[fb[3]] = uint32_t(-8192) - i;
            }
            writeCoefs(2, fb);
            writeCoefs(4, fb + 2);
            break;
        }
        case DRY_WET:
            coef[ins::PC_WET[1]] = coef[ins::PC_WET[0]] = rom::PAN_LAW[128 - P[DRY_WET]] >> 2;
            coef[ins::PC_MIX[6]] = coef[ins::PC_MIX[5]] = typeGain(w, rom::PAN_LAW[P[DRY_WET]] >> 2);
            writeCoefs(2, ins::PC_MIX + 5);
            writeCoefs(2, ins::PC_WET);
            break;
        case PAN1: case LEVEL1: {
            const float level = rom::LEVEL_CURVE[P[LEVEL1]];
            coef[ins::PC_VOICE1_A[0]] = int16_t(int(float(rom::PAN_LAW[128 - P[PAN1]]) * level));
            coef[ins::PC_VOICE1_B[0]] = int(float(rom::PAN_LAW[P[PAN1]]) * level);
            writeCoefs(1, ins::PC_VOICE1_A);
            writeCoefs(1, ins::PC_VOICE1_B);
            break;
        }
        case PAN2: case LEVEL2: {
            const float level = rom::LEVEL_CURVE[P[LEVEL2]];
            coef[ins::PC_VOICE2[1]] = int16_t(int(float(rom::PAN_LAW[128 - P[PAN2]]) * level));
            coef[ins::PC_VOICE2[0]] = int(float(rom::PAN_LAW[P[PAN2]]) * level);
            writeCoefs(2, ins::PC_VOICE2);
            break;
        }
        case ALL:
            applyAll(w, &EffectHost::pitchChange, {FINE1, FINE2, FB_LEVEL, PITCH, INIT_DELAY, DRY_WET, PAN1, PAN2});
            break;
    }
}

}  // namespace fs1r::vop3host

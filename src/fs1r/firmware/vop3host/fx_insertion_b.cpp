// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 2: the wah, distortion and dynamics types, pitch change, ensemble
// detune, the 3-band EQ and Lo-Fi.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ins;

namespace {

// the wah "cutoff" word: 3052 v / 16 + 7837
inline int32_t wah_cutoff(uint32_t v) { return int32_t((3052u * (v & 0xFFFF)) >> 4) + 7837; }
// the wah "sensitivity" pair: -327680 / v for v >= 11, else 0x8000
void wah_sense(EffectHost& h, int p)
{
    if (PU(h, p) < 11) {
        h.coef[WAH_SENSE[1]] = 0x8000;
        h.coef[WAH_SENSE[0]] = 0x8000;
    } else {
        const int32_t x = int(-327680.0 / double(float(PU(h, p))));
        h.coef[WAH_SENSE[1]] = int16_t(x);
        h.coef[WAH_SENSE[0]] = int16_t(x);
    }
    h.writeCoefs(2, WAH_SENSE);
}
// the wah + distortion Dry/Wet block (parameters 9, 10, 14): the drive words, the mode pair(s)
// and eight pan words whose layout follows whether the distortion is on (parameter 10)
void wah_dist_wet(EffectHost& h, int drive_index, bool second_pair)
{
    h.storeDrive(P(h, 10), drive_index, WAH_DRIVE);
    h.coef[WAH_MODE[1]] = 23170;
    h.coef[WAH_MODE[0]] = 23170;
    if (second_pair) { h.coef[WAH_MODE2[1]] = 0x5A81; h.coef[WAH_MODE2[0]] = 0x5A81; }
    const uint32_t wet = PU(h, 9);
    h.writeCoefs(2, WAH_MODE);
    if (second_pair)
        h.writeCoefs(2, WAH_MODE2);
    const uint32_t g = rom::PAN_LAW[128 - wet] >> 2;
    const uint32_t w = rom::PAN_LAW[wet] >> 2;
    h.coef[WAH_WET[4]] = g;
    h.coef[WAH_WET[0]] = g;
    if (PU(h, 10) == 0) {
        const uint32_t x = h.typeGain(INSERTION, int32_t(w));
        h.coef[WAH_WET[5]] = x;
        h.coef[WAH_WET[1]] = x;
        h.coef[WAH_WET[6]] = 8192;
        h.coef[WAH_WET[2]] = 8192;
        h.coef[WAH_WET[7]] = 0;
        h.coef[WAH_WET[3]] = 0;
    } else {
        h.coef[WAH_WET[6]] = g;
        h.coef[WAH_WET[2]] = g;
        h.coef[WAH_WET[5]] = w;
        h.coef[WAH_WET[1]] = w;
        const uint32_t x = h.typeGain(INSERTION, int32_t(w));
        h.coef[WAH_WET[7]] = x;
        h.coef[WAH_WET[3]] = x;
    }
    h.writeCoefs(8, WAH_WET);
}
// the distortion group's Dry/Wet words (parameter 9): the mode pair (a, b), then g, x, 0, g, x
// over DIST_WET
void dist_wet(EffectHost& h, uint32_t a, uint32_t b, bool two_lists)
{
    h.coef[DIST_MODE[0]] = a;
    h.coef[two_lists ? DIST_LEVEL[0] : DIST_MODE[1]] = b;
    const uint32_t wet = PU(h, 9);
    if (two_lists) {
        h.writeCoefs(1, DIST_MODE);
        h.writeCoefs(1, DIST_LEVEL);
    } else {
        h.writeCoefs(2, DIST_MODE);
    }
    const uint32_t g = rom::PAN_LAW[128 - wet] >> 2;
    h.coef[DIST_WET[2]] = g;
    h.coef[DIST_WET[0]] = g;
    const uint32_t x = h.typeGain(INSERTION, rom::PAN_LAW[wet] >> 2);
    h.coef[DIST_WET[4]] = x;
    h.coef[DIST_WET[1]] = x;
    h.coef[DIST_WET[3]] = 0;
    h.writeCoefs(5, DIST_WET);
}
// the compressor / noise gate output level words through table B
void dyn_level(EffectHost& h, int p)
{
    const int32_t i = int(double(rom::LEVEL_CURVE[PU(h, p)]) * 8192.0);
    const uint32_t x = h.typeGain(INSERTION, int16_t(i));
    h.coef[DIST_WET[3]] = x;
    h.coef[DIST_WET[1]] = x;
    h.coef[DIST_WET[4]] = 0;
    h.coef[DIST_WET[2]] = 0;
    h.coef[DIST_WET[0]] = 0;
    h.writeCoefs(5, DIST_WET);
}
void dyn_filters(EffectHost& h)
{
    h.designFilter(2, 1, HPF1, 0, 64, 0, DIST_LPF);
    h.copyCoefs(5, DIST_LPF, DIST_LPF_C);
    h.designFilter(1, 1, GAIN_ONLY, 0, 64, 0, DIST_HPF);
    h.copyCoefs(3, DIST_HPF, DIST_HPF_C);
    h.designFilter(2, 1, GAIN_ONLY, 0, 64, 0, DIST_PRE);
    h.copyCoefs(5, DIST_PRE, DIST_PRE_C);
    h.designFilter(2, 4, GAIN_ONLY, 0, 64, 0, DIST_EQ);
}
// the dynamics + delay types' Dry/Wet words over DD_WET (pan law >> 1)
void dd_wet(EffectHost& h, uint32_t wet, int shift)
{
    wet_words(h, DD_WET, rom::PAN_LAW, wet, GXGX, shift);
    h.writeCoefs(4, DD_WET);
}
}  // namespace

// A-Wah+Dist / A-Wah+Odrv (types 14, 15; msb 4 lsb 3, 4)
void EffectHost::fxInsAutoWahDist(int p)
{
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const int32_t w = wah_cutoff(PU(*this, 2));
        coef[WAH_CUTOFF[0]] = w;
        coef[WAH_DEPTH[0]] = PU(*this, 1) * uint32_t(32767 - int16_t(w)) / 127;
        writeCoefs(1, WAH_CUTOFF);
        writeCoefs(1, WAH_DEPTH);
        break;
    }
    case 3:
        wah_sense(*this, 3);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_WAH, LPF_WAH_C, HPF_WAH, HPF_WAH_C);
        break;
    case 9: case 10: case 14:
        wah_dist_wet(*this, int(PU(*this, 14) & 0xFF), false);
        break;
    case 11:
        designFilter(2, 4, LOW_SHELF2, t == 4 ? 24 : 20, P(*this, 11), 0, WAH_LOW_SHELF2);
        break;
    case 12:
        designFilter(2, 1, PEAK, t == 4 ? 41 : 43, P(*this, 12), 10, WAH_PEAK);
        break;
    case 13:
        designFilter(2, 1, LPF2, P(*this, 13), 64, 7, WAH_LPF2);
        break;
    case ALL:
        hold = true;
        if (t == 4) {
            storeEdge(104, WAH_BAND);
            designFilter(1, 1, HPF1, 8, 64, 0, WAH_HPF_2);
            designFilter(1, 1, LPF1, 44, 64, 0, WAH_LPF_2);
        } else {
            storeEdge(120, WAH_BAND);
            designFilter(1, 1, HPF1, 0, 64, 0, WAH_HPF_2);
            designFilter(1, 1, LPF1, 54, 64, 0, WAH_LPF_2);
        }
        for (int q : {0, 1, 3, 5, 7, 9, 11, 12, 13})
            fxInsAutoWahDist(q);
        hold = false;
        break;
    }
}

// T-Wah+Dist / T-Wah+Odrv (types 16, 17; msb 4 lsb 6, 7); 0 runs on into 1
void EffectHost::fxInsTouchWahDist(int p)
{
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0:
        coef[WAH_ATTACK[0]] = P(*this, 0) * 258;
        writeCoefs(1, WAH_ATTACK);
        [[fallthrough]];
    case 1:
        coef[WAH_CUTOFF[0]] = wah_cutoff(PU(*this, 1));
        writeCoefs(1, WAH_CUTOFF);
        break;
    case 2:
        wah_sense(*this, 2);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_WAH, LPF_WAH_C, HPF_WAH, HPF_WAH_C);
        break;
    case 9: case 10: case 14:
        wah_dist_wet(*this, int(PU(*this, 14) & 0xFF), true);
        break;
    case 11:
        designFilter(2, 4, LOW_SHELF2, t == 6 ? 24 : 20, P(*this, 11), 0, WAH_LOW_SHELF2);
        break;
    case 12:
        designFilter(2, 1, PEAK, t == 6 ? 41 : 43, P(*this, 12), 10, WAH_PEAK);
        break;
    case 13:
        designFilter(2, 1, LPF2, P(*this, 13), 64, 7, WAH_LPF2);
        break;
    case 15: {
        int32_t v = PLO(*this, 15) - 52;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        storeWahRelease(v, WAH_MIRROR);
        break;
    }
    case ALL:
        hold = true;
        if (t == 6) {
            designFilter(1, 1, HPF1, 8, 64, 0, WAH_HPF_2);
            designFilter(1, 1, LPF1, 44, 64, 0, WAH_LPF_2);
            storeEdge(104, WAH_BAND);
        } else {
            designFilter(1, 1, HPF1, 0, 64, 0, WAH_HPF_2);
            designFilter(1, 1, LPF1, 54, 64, 0, WAH_LPF_2);
            storeEdge(120, WAH_BAND);
        }
        designFilter(1, 1, HPF1, 0, 64, 0, WAH_HPF);
        designFilter(1, 0, LPF1, 54, 64, 0, WAH_LPF);
        for (int q : {0, 1, 2, 5, 7, 9, 11, 12, 13, 15})
            fxInsTouchWahDist(q);
        hold = false;
        break;
    }
}

// Distortion / Overdrive (types 28, 30; msb 5 lsb 1, 2); 9 re-runs 0
void EffectHost::fxInsDistortion(int p)
{
    switch (p) {
    case 0: case 4:
        storeDrive(P(*this, 0), P(*this, 4), DIST_DRIVE);
        break;
    case 1: case 2:
        designFilter(2, 4, LOW_SHELF1, P(*this, 1), P(*this, 2), 0, DIST_EQ);
        break;
    case 3:
        designFilter(2, 1, LPF2, P(*this, 3), 64, 7, DIST_PRE);
        break;
    case 6: case 7: case 8:
        designFilter(2, 1, PEAK, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PRE_C);
        break;
    case 9:
        dist_wet(*this, 1448, 0, false);
        fxInsDistortion(0);
        break;
    case 10:
        storeEdge(P(*this, 10), DIST_FILL);
        break;
    case ALL:
        hold = true;
        storeGateAttack(0, DYN_THRESH);
        storeRelease(11, DYN_PAIR);
        storeGateThreshold(73, DYN_WORD);
        if (lsb(*this) == 1) {
            designFilter(2, 1, HPF1, 0, 64, 0, DIST_LPF);
            designFilter(1, 1, LPF1, 54, 64, 0, DIST_HPF);
        } else {
            designFilter(2, 1, HPF1, 8, 64, 0, DIST_LPF);
            designFilter(1, 1, LPF1, 44, 64, 0, DIST_HPF);
        }
        for (int q : {1, 3, 6, 9, 10})
            fxInsDistortion(q);
        hold = false;
        break;
    }
}

// type 32; msb 5 lsb 3; 1 (the amp type) re-runs 2, 9 re-runs 0
void EffectHost::fxInsAmpSim(int p)
{
    switch (p) {
    case 0: case 3:
        storeDrive(P(*this, 0), P(*this, 3), DIST_DRIVE);
        break;
    case 1: {
        const int8_t* row = rom::AMP_FILTERS + uint32_t(int8_t(14 * PLO(*this, 1)));
        designFilter(2, 1, row[0], row[2], row[1], 0, DIST_LPF);
        designFilter(1, 1, row[3], row[5], row[4], 0, DIST_HPF);
        designFilter(2, 1, row[6], row[8], row[7], 0, DIST_PRE_C);
        designFilter(2, 4, row[9], row[11], row[10], row[12], DIST_EQ);
        fxInsAmpSim(2);
        break;
    }
    case 2:
        if (P(*this, 1) == 1)
            designFilter(2, 1, LPF2, P(*this, 2), 56, 15, DIST_PRE);
        else
            designFilter(2, 1, LPF2, P(*this, 2), 64, P(*this, 1) == 2 ? 15 : 7, DIST_PRE);
        break;
    case 9:
        dist_wet(*this, 1448, 0, false);
        fxInsAmpSim(0);
        break;
    case 10:
        storeEdge(P(*this, 10), DIST_FILL);
        break;
    case ALL:
        hold = true;
        storeGateAttack(0, DYN_THRESH);
        storeRelease(11, DYN_PAIR);
        storeGateThreshold(73, DYN_WORD);
        for (int q : {1, 2, 9, 10})
            fxInsAmpSim(q);
        hold = false;
        break;
    }
}

// type 24; msb 5 lsb 4
void EffectHost::fxInsCompDist(int p)
{
    switch (p) {
    case 0: storeCompAttack(P(*this, 0), DYN_THRESH); break;
    case 1: storeRelease(P(*this, 1), DYN_PAIR); break;
    case 2: storeCompThreshold(P(*this, 2), DYN_WORD); break;
    case 3: storeRatio(P(*this, 3), DYN_ROW); break;
    case 4: dyn_level(*this, 4); break;
    case 9: {
        const uint32_t w = 7240;
        coef[DIST_LEVEL[1]] = w;
        coef[DIST_LEVEL[0]] = w;
        writeCoefs(2, DIST_LEVEL);
        break;
    }
    case ALL:
        hold = true;
        dyn_filters(*this);
        for (int q : {0, 1, 2, 3, 4, 9})
            fxInsCompDist(q);
        hold = false;
        break;
    }
}

// type 25 "Cmp+DS+Dly"; msb 5 lsb 5; 9 re-runs 0
void EffectHost::fxInsCompDistDelay(int p)
{
    switch (p) {
    case 0: case 4:
        storeDrive(P(*this, 0), P(*this, 4), DIST_DRIVE);
        break;
    case 1: case 2:
        designFilter(2, 4, LOW_SHELF1, P(*this, 1), P(*this, 2), 0, DIST_EQ);
        break;
    case 3:
        designFilter(2, 1, LPF2, P(*this, 3), 64, 7, DIST_PRE);
        break;
    case 6: case 7: case 8:
        designFilter(2, 1, PEAK, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PRE_C);
        break;
    case 9:
        dist_wet(*this, 5792, 14481, true);
        fxInsCompDistDelay(0);
        break;
    case 10:
        storeEdge(P(*this, 10), DIST_FILL);
        break;
    case 11: storeCompAttack(P(*this, 11), DYN_THRESH); break;
    case 12: storeRelease(P(*this, 12), DYN_PAIR); break;
    case 13: storeCompThreshold(P(*this, 13), DYN_WORD); break;
    case 14: storeRatio(P(*this, 14), DYN_ROW); break;
    case ALL:
        hold = true;
        designFilter(2, 1, HPF1, 0, 64, 0, DIST_LPF);
        designFilter(1, 1, LPF1, 54, 64, 0, DIST_HPF);
        for (int q : {1, 3, 6, 9, 10, 11, 12, 13, 14})
            fxInsCompDistDelay(q);
        hold = false;
        break;
    }
}

// type 22; msb 5 lsb 6
void EffectHost::fxInsHmEnhancer(int p)
{
    switch (p) {
    case 0:
        designFilter(2, 1, HPF2, P(*this, 0), 64, 7, DIST_LPF);
        copyCoefs(5, DIST_LPF, DIST_LPF_C);
        designFilter(1, 1, HPF1, P(*this, 0), 64, 0, DIST_HPF);
        copyCoefs(3, DIST_HPF, DIST_HPF_C);
        break;
    case 1: {
        const uint32_t w = P(*this, 1) * 258;
        coef[HM_DRIVE[1]] = w;
        coef[HM_DRIVE[0]] = w;
        writeCoefs(2, HM_DRIVE);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(typeGain(INSERTION, int32_t((PU(*this, 2) * 0x81u) >> 1)));
        coef[DIST_WET[3]] = x;
        coef[DIST_WET[1]] = x;
        writeCoefs(1, DIST_WET + 1);
        writeCoefs(1, DIST_WET + 3);
        break;
    }
    case 9: {
        const uint32_t w = 1448;
        coef[DIST_LEVEL[1]] = w;
        coef[DIST_LEVEL[0]] = w;
        writeCoefs(2, DIST_LEVEL);
        break;
    }
    case ALL:
        hold = true;
        designFilter(2, 1, GAIN_ONLY, 0, 64, 0, DIST_PRE);
        copyCoefs(5, DIST_PRE, DIST_PRE_C);
        designFilter(2, 4, GAIN_ONLY, 0, 64, 0, DIST_EQ);
        for (int q : {0, 1, 2, 9})
            fxInsHmEnhancer(q);
        hold = false;
        break;
    }
}

// type 23; msb 5 lsb 7
void EffectHost::fxInsNoiseGate(int p)
{
    switch (p) {
    case 0: storeGateAttack(P(*this, 0), DYN_THRESH); break;
    case 1: storeRelease(P(*this, 1), DYN_PAIR); break;
    case 2: storeGateThreshold(P(*this, 2), DYN_WORD); break;
    case 3: dyn_level(*this, 3); break;
    case ALL:
        hold = true;
        dyn_filters(*this);
        for (int q : {0, 1, 2, 3})
            fxInsNoiseGate(q);
        hold = false;
        break;
    }
}

// Wah+DS+Dly / Wah+OD+Dly (types 18, 19; msb 12 lsb 1, 2)
void EffectHost::fxInsWahDistDelay(int p)
{
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = toSamples(P(*this, 0)) + delay[WD_SL[0]];
        for (uint32_t i = 1; i <= 4; i++)
            delay[WD_SL[i]] = d;
        writeDelays(4, WD_SL + 1);
        break;
    }
    case 1:
        coef[WD_FB[0]] = bipolar500(P(*this, 1));
        writeCoefs(1, WD_FB);
        break;
    case 2: {
        const int32_t i = int(8192.0f * rom::LEVEL_CURVE[PU(*this, 2)]);
        coef[WD_LEVEL[2]] = i;
        coef[WD_LEVEL[0]] = i;
        writeCoefs(1, WD_LEVEL);
        writeCoefs(1, WD_LEVEL + 2);
        break;
    }
    case 3: case 4: case 5: case 6: case 9: {
        if (t == 2) {
            designFilter(1, 1, HPF1, 8, 64, 0, WD_HPF_2);
            designFilter(1, 1, LPF1, 44, 64, 0, WD_LPF_2);
            designFilter(1, 1, LPF1, 45, 64, 0, WD_LPF_3);
            designFilter(1, 2, LOW_SHELF1, 24, PLO(*this, 5), 0, WD_LOW_SHELF_2);
            designFilter(2, 1, PEAK, 41, PLO(*this, 6), 10, WD_PEAK);
            storeEdge(104, WD_BAND);
        } else if (t == 1) {
            designFilter(1, 1, HPF1, 0, 64, 0, WD_HPF_2);
            designFilter(1, 1, LPF1, 54, 64, 0, WD_LPF_2);
            designFilter(1, 1, LPF1, 53, 64, 0, WD_LPF_3);
            designFilter(1, 2, LOW_SHELF1, 20, PLO(*this, 5), 0, WD_LOW_SHELF_2);
            designFilter(2, 1, PEAK, 43, PLO(*this, 6), 10, WD_PEAK);
            storeEdge(120, WD_BAND);
        }
        storeDrive(P(*this, 3), P(*this, 4), WD_DRIVE);
        coef[WD_MODE[1]] = 23170; coef[WD_MODE[0]] = 23170;
        coef[WD_MODE2[1]] = 0x5A81; coef[WD_MODE2[0]] = 0x5A81;
        const uint32_t wet = PU(*this, 9);
        writeCoefs(2, WD_MODE);
        writeCoefs(2, WD_MODE2);
        const int32_t dry = rom::PAN_LAW[128 - wet];
        const int32_t wetw = rom::PAN_LAW[wet];
        coef[WD_WET[4]] = dry >> 2;
        coef[WD_WET[0]] = dry >> 2;
        if (P(*this, 3) == 0) {
            const uint32_t x = typeGain(INSERTION, wetw >> 2);
            coef[WD_WET[5]] = x;
            coef[WD_WET[1]] = x;
            coef[WD_WET[6]] = 0x4000;
            coef[WD_WET[2]] = 0x4000;
            coef[WD_WET[7]] = 0;
            coef[WD_WET[3]] = 0;
        } else {
            coef[WD_WET[6]] = dry >> 1;
            coef[WD_WET[2]] = dry >> 1;
            coef[WD_WET[5]] = wetw >> 2;
            coef[WD_WET[1]] = wetw >> 2;
            const uint32_t x = typeGain(INSERTION, wetw >> 1);
            coef[WD_WET[7]] = x;
            coef[WD_WET[3]] = x;
        }
        writeCoefs(8, WD_WET);
        break;
    }
    case 10:
        coef[WD_ATTACK[0]] = P(*this, 10) * 258;
        writeCoefs(1, WD_ATTACK);
        [[fallthrough]];
    case 12:
        if (PU(*this, 12) <= 10)
            coef[WD_SENSE[0]] = 0x8000;
        else
            coef[WD_SENSE[0]] = int16_t(int(-327680.0 / double(float(PU(*this, 12)))));
        writeCoefs(1, WD_SENSE);
        break;
    case 11:
        coef[WD_CUTOFF[0]] = int32_t((3052u * PU(*this, 11)) >> 4) + 7483;
        writeCoefs(1, WD_CUTOFF);
        break;
    case 13: {
        int32_t v = PLO(*this, 13) - 52;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        const int32_t w = rom::COMPLEMENT[uint32_t(int8_t(v))];
        coef[WD_MIRROR[1]] = w;
        coef[WD_MIRROR[0]] = w;
        coef[WD_MIRROR[2]] = (-32768 - w) * 2;
        writeCoefs(3, WD_MIRROR);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 10, 11, 12, 13})
            fxInsWahDistDelay(q);
        designFilter(1, 1, HPF1, 0, 64, 0, WD_HPF);
        designFilter(1, 0, LPF1, 54, 64, 0, WD_LPF);
        designFilter(1, 1, LOW_SHELF1, 28, 66, 0, WD_LOW_SHELF);
        copyCoefs(3, WD_LOW_SHELF, WD_LOW_SHELF_COPY);
        designFilter(1, 2, HIGH_SHELF1, 46, 64, 0, WD_HIGH_SHELF);
        copyCoefs(3, WD_HIGH_SHELF, WD_HIGH_SHELF_COPY);
        designFilter(1, 1, LPF1, 60, 64, 0, WD_LPF_4);
        hold = false;
        break;
    }
}

// Compressor / Cmp+OD+Dly (types 26, 27; msb 13 lsb 1, 2); 9 re-runs 0
void EffectHost::fxInsCompressor(int p)
{
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = toSamples(P(*this, 0)) + delay[DD_SLOTS[0]];
        for (uint32_t i = 1; i <= 4; i++)
            delay[DD_SLOTS[i]] = d;
        writeDelays(4, DD_SLOTS + 1);
        break;
    }
    case 1:
        coef[DD_FB[0]] = bipolar500(P(*this, 1));
        writeCoefs(1, DD_FB);
        break;
    case 2: {
        const int32_t i = int(8192.0f * rom::LEVEL_CURVE[PU(*this, 2)]);
        coef[DD_LEVEL[2]] = i;
        coef[DD_LEVEL[0]] = i;
        writeCoefs(1, DD_LEVEL);
        writeCoefs(1, DD_LEVEL + 2);
        break;
    }
    case 3: case 4: case 5: case 6:
        if (t == 2) {
            designFilter(2, 1, HPF1, 8, 64, 0, DD_HPF);
            designFilter(1, 1, LPF1, 44, 64, 0, DD_LPF);
            designFilter(2, 1, LPF2, 45, 64, 7, DD_LPF2);
            designFilter(2, 4, LOW_SHELF1, 24, PLO(*this, 5), 0, DD_LOW_SHELF);
            designFilter(2, 1, PEAK, 41, PLO(*this, 6), 10, DD_PEAK);
            storeEdge(104, DD_BAND);
        } else if (t == 1) {
            designFilter(2, 1, HPF1, 0, 64, 0, DD_HPF);
            designFilter(1, 1, LPF1, 54, 64, 0, DD_LPF);
            designFilter(2, 1, LPF2, 53, 64, 7, DD_LPF2);
            designFilter(2, 4, LOW_SHELF1, 20, PLO(*this, 5), 0, DD_LOW_SHELF);
            designFilter(2, 1, PEAK, 43, PLO(*this, 6), 10, DD_PEAK);
            storeEdge(120, DD_BAND);
        }
        storeDrive(P(*this, 3), P(*this, 4), DD_DRIVE);
        break;
    case 9: {
        coef[DD_MODE[0]] = 5792;
        coef[DD_MODE[2]] = 14481;
        const uint32_t wet = PU(*this, 9);
        writeCoefs(1, DD_MODE);
        writeCoefs(1, DD_MODE + 2);
        dd_wet(*this, wet, 1);
        fxInsCompressor(0);
        break;
    }
    case 10: storeCompAttack(P(*this, 10), DD_THRESH); break;
    case 11: storeRelease(P(*this, 11), DD_PAIR); break;
    case 12: storeCompThreshold(P(*this, 12), DD_WORD); break;
    case 13: storeRatio(P(*this, 13), DD_ROW); break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 9, 10, 11, 12, 13})
            fxInsCompressor(q);
        designFilter(1, 1, LPF1, 60, 64, 0, DD_LPF_2);
        hold = false;
        break;
    }
}

// Dist+Delay / Odrv+Delay (types 29, 31; msb 13 lsb 3, 4); 9 re-runs 0
void EffectHost::fxInsDistDelay(int p)
{
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = toSamples(P(*this, 0)) + delay[DD_SLOTS[0]];
        delay[DD_SLOTS[4]] = d;
        delay[DD_SLOTS[3]] = d;
        writeDelays(2, DD_SLOTS + 3);
        break;
    }
    case 1:
        delay[DD_SLOTS[2]] = toSamples(P(*this, 1)) + delay[DD_SLOTS[0]];
        writeDelays(1, DD_SLOTS + 2);
        break;
    case 2:
        delay[DD_SLOTS[1]] = toSamples(P(*this, 2)) + delay[DD_SLOTS[0]];
        writeDelays(1, DD_SLOTS + 1);
        break;
    case 3:
        coef[DD_FB[0]] = bipolar500(P(*this, 3));
        writeCoefs(1, DD_FB);
        break;
    case 4: {
        const int32_t i = int(8192.0f * rom::LEVEL_CURVE[PU(*this, 4)]);
        coef[DD_LEVEL[2]] = i;
        coef[DD_LEVEL[0]] = i;
        writeCoefs(1, DD_LEVEL);
        writeCoefs(1, DD_LEVEL + 2);
        break;
    }
    case 5: case 6:
        storeDrive(P(*this, 5), P(*this, 6), DD_DRIVE);
        break;
    case 7:
        designFilter(2, 4, LOW_SHELF1, t == 4 ? 24 : 20, PLO(*this, 7), 0, DD_LOW_SHELF);
        break;
    case 8:
        designFilter(2, 1, PEAK, t == 4 ? 41 : 43, PLO(*this, 8), 10, DD_PEAK);
        break;
    case 9: {
        coef[DD_MODE[0]] = 1448;
        coef[DD_MODE[1]] = 0;
        const uint32_t wet = PU(*this, 9);
        writeCoefs(2, DD_MODE);
        dd_wet(*this, wet, 1);
        fxInsDistDelay(0);
        break;
    }
    case ALL:
        hold = true;
        storeGateAttack(0, DD_THRESH);
        storeRelease(11, DD_PAIR);
        storeGateThreshold(73, DD_WORD);
        if (t == 3) {
            designFilter(2, 1, HPF1, 0, 64, 0, DD_HPF);
            designFilter(1, 1, LPF1, 54, 64, 0, DD_LPF);
            designFilter(2, 1, LPF2, 53, 64, 7, DD_LPF2);
            storeEdge(120, DD_BAND);
        } else {
            designFilter(2, 1, HPF1, 8, 64, 0, DD_HPF);
            designFilter(1, 1, LPF1, 44, 64, 0, DD_LPF);
            designFilter(2, 1, LPF2, 45, 64, 7, DD_LPF2);
            storeEdge(104, DD_BAND);
        }
        for (int q : {0, 1, 2, 3, 4, 5, 7, 8, 9})
            fxInsDistDelay(q);
        designFilter(1, 1, LPF1, 60, 64, 0, DD_LPF_2);
        hold = false;
        break;
    }
}

// ---- pitch change (msb 7) ----

// the pitch change's LFO word for one voice, from the pitch
// (parameter 0) and the voice's fine value v
int32_t EffectHost::pitchRatio(int v)
{
    const int32_t d = v - 64;
    const int32_t pitch = int16_t(P(*this, 0) - 64);
    if (pitch == 0) {
        const int32_t a = int16_t(d);
        return rom::PITCH_FINE[uint32_t(a >= 0 ? a : -a)];
    }
    const int32_t semi = rom::PITCH_SEMITONE[uint32_t(int32_t(PU(*this, 0)) - 40)];
    return pitch >= 0 ? semi + d : semi - d;
}

// the voice's LFO word pair and the LFO's direction bits
void EffectHost::pitchVoice(int lfo, int v)
{
    const uint32_t a = lfo == 12 ? 0 : 2;
    const uint32_t w = pitchRatio(v);
    coef[LFO_DIR_STEPS[a + 1]] = w;
    coef[LFO_DIR_STEPS[a]] = w;
    writeCoefs(4, LFO_DIR_STEPS);
    const uint32_t l = uint32_t(lfo) & 0xFF;
    const int32_t s = 100 * (P(*this, 0) - 64) + int16_t(v) - 64;
    if (s > 0) {
        lfoReg[1][l] = (lfoReg[1][l] & 0xFF1F) | 0x20;
        lfoReg[3][l] = 0x7FFF;
    } else {
        lfoReg[1][l] = (lfoReg[1][l] & 0xFF1F) | 0x40;
        lfoReg[3][l] = 0;
    }
    writeLfo(0x25, lfo);
    writeLfo(0x27, lfo);
}

// the first voice's delay slots from parameter 1
void EffectHost::pitchSlots1()
{
    const int32_t d = toSamples(rom::PITCH_DELAY[PU(*this, 1)]) + 1;
    delay[PC_SLOTS[2]] = d;
    delay[PC_SLOTS[1]] = d;
    delay[PC_SLOTS[4]] = d + 384;
    delay[PC_SLOTS[3]] = d + 384;
    writeDelays(4, PC_SLOTS + 1);
}

// the second voice's delay slots above half the area
void EffectHost::pitchSlots2(int v)
{
    const int32_t t = toSamples(rom::PITCH_DELAY[uint32_t(int16_t(v))]);
    const int32_t half = int32_t(half_area());
    const int32_t d = half + t + 1;
    delay[PC_SLOTS[5]] = half;
    delay[PC_SLOTS[7]] = d;
    delay[PC_SLOTS[6]] = d;
    delay[PC_SLOTS[9]] = d + 384;
    delay[PC_SLOTS[8]] = d + 384;
    writeDelays(5, PC_SLOTS + 5);
}

// type 7; msb 7 lsb 1
void EffectHost::fxInsPitchChange(int p)
{
    switch (p) {
    case 0: {
        pitchVoice(12, P(*this, 2));
        pitchVoice(13, P(*this, 3));
        auto set24 = [&](int lfo, uint32_t v) {
            lfoReg[0][lfo] = (lfoReg[0][lfo] & 0xFC00) | v;
        };
        const uint32_t v = rom::PITCH_LFO_SPEED[uint32_t(int32_t(PU(*this, 0)) - 40)];
        set24(12, v);
        set24(13, v);
        writeLfo(0x24, 12);
        writeLfo(0x24, 13);
        break;
    }
    case 1:
        pitchSlots1();
        pitchSlots2(P(*this, 1));
        break;
    case 2:
        pitchVoice(12, P(*this, 2));
        break;
    case 3:
        pitchVoice(13, P(*this, 3));
        break;
    case 4: {
        const uint32_t x = bipolar500(P(*this, 4));
        coef[PC_FB[1]] = x;
        coef[PC_FB[0]] = x;
        const uint32_t v = PU(*this, 4);
        if (v >= 32 && v <= 96) {
            coef[PC_FB[4]] = uint16_t(-6144);
            coef[PC_FB[2]] = uint16_t(-6144);
            coef[PC_FB[5]] = uint16_t(-10240);
            coef[PC_FB[3]] = uint16_t(-10240);
        }
        if (v <= 31) {
            const uint32_t i = uint32_t(int((double(v) - 1.0) * 2048.0 / 31.0)) & 0xFFFF;
            coef[PC_FB[4]] = i - 8192u;
            coef[PC_FB[2]] = i - 8192u;
            coef[PC_FB[5]] = uint32_t(-8192) - i;
            coef[PC_FB[3]] = uint32_t(-8192) - i;
        }
        if (v >= 97) {
            const uint32_t i = uint32_t(int((-double(v) + 127.0) * 2048.0 / 31.0)) & 0xFFFF;
            coef[PC_FB[4]] = i - 8192u;
            coef[PC_FB[2]] = i - 8192u;
            coef[PC_FB[5]] = uint32_t(-8192) - i;
            coef[PC_FB[3]] = uint32_t(-8192) - i;
        }
        writeCoefs(2, PC_FB);
        writeCoefs(4, PC_FB + 2);
        break;
    }
    case 9: {
        const uint32_t wet = PU(*this, 9);
        const float dry = float(rom::PAN_LAW[128 - wet] >> 2);
        const uint32_t g = int16_t(int(dry));
        coef[PC_WET[1]] = g;
        coef[PC_WET[0]] = g;
        const float wf = float(rom::PAN_LAW[wet] >> 2);
        const uint32_t x = uint32_t(typeGain(INSERTION, int16_t(int(wf))));
        coef[PC_MIX[6]] = x;
        coef[PC_MIX[5]] = x;
        writeCoefs(2, PC_MIX + 5);
        writeCoefs(2, PC_WET);
        break;
    }
    case 10: case 11: {
        const float c = rom::LEVEL_CURVE[PU(*this, 11)];
        coef[PC_VOICE1_A[0]] = int16_t(int(float(rom::PAN_LAW[128 - PU(*this, 10)]) * c));
        coef[PC_VOICE1_B[0]] = int(float(rom::PAN_LAW[PU(*this, 10)]) * c);
        writeCoefs(1, PC_VOICE1_A);
        writeCoefs(1, PC_VOICE1_B);
        break;
    }
    case 12: case 13: {
        const float c = rom::LEVEL_CURVE[PU(*this, 13)];
        coef[PC_VOICE2[1]] = int16_t(int(float(rom::PAN_LAW[128 - PU(*this, 12)]) * c));
        coef[PC_VOICE2[0]] = int(float(rom::PAN_LAW[PU(*this, 12)]) * c);
        writeCoefs(2, PC_VOICE2);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {2, 3, 4, 0, 1, 9, 10, 12})
            fxInsPitchChange(q);
        hold = false;
        break;
    }
}

// type 8; msb 7 lsb 3
void EffectHost::fxInsEnsDetune(int p)
{
    switch (p) {
    case 0: {
        const uint32_t v = PU(*this, 0);
        const double K = 65527.48886, NK = -65527.48886;
        uint16_t &lo = lfoReg[1][12], &hi = lfoReg[1][13];   // LFO 12 and 13
        if (v < 64) {
            const double a = double(rom::DETUNE[114u - v]) * K;
            const int32_t w0 = int(a + NK);
            const double b = double(rom::DETUNE[v - 14u]) * K;
            const int32_t w1 = int(K - b);
            coef[LFO_DIR_STEPS[1]] = int16_t(w0);
            coef[LFO_DIR_STEPS[0]] = int16_t(w0);
            coef[LFO_DIR_STEPS[3]] = int16_t(w1);
            coef[LFO_DIR_STEPS[2]] = int16_t(w1);
            lo = (lo & 0xFF1F) | 0x20;
            lfoReg[3][12] = 0x7FFF;
            hi = (hi & 0xFF1F) | 0x40;
            lfoReg[3][13] = 0;
        } else if (v > 64) {
            const double a = double(rom::DETUNE[114u - v]) * K;
            const int32_t w0 = int(-a + K);
            const double b = double(rom::DETUNE[v - 14u]) * K;
            const int32_t w1 = int(b + NK);
            coef[LFO_DIR_STEPS[1]] = int16_t(w0);
            coef[LFO_DIR_STEPS[0]] = int16_t(w0);
            coef[LFO_DIR_STEPS[3]] = int16_t(w1);
            coef[LFO_DIR_STEPS[2]] = int16_t(w1);
            lo = (lo & 0xFF1F) | 0x40;
            lfoReg[3][12] = 0;
            hi = (hi & 0xFF1F) | 0x20;
            lfoReg[3][13] = 0x7FFF;
        } else {
            for (int i = 0; i < 4; i++)
                coef[LFO_DIR_STEPS[i]] = 0;
        }
        writeCoefs(4, LFO_DIR_STEPS);
        writeLfo(0x25, 12);
        writeLfo(0x25, 13);
        writeLfo(0x27, 12);
        writeLfo(0x27, 13);
        break;
    }
    case 1: {
        const int32_t d = ms(*this, rom::MOD_DELAY, 1) + delay[PC_SLOTS[0]] + 1;
        for (uint32_t i = 1; i <= 4; i++)
            delay[PC_SLOTS[i]] = d;
        writeDelays(5, PC_SLOTS);
        break;
    }
    case 2: {
        const int32_t half = int32_t(half_area());
        delay[PC_SLOTS[5]] = half;
        const int32_t d = ms(*this, rom::MOD_DELAY, 2) + half + 1;
        for (uint32_t i = 6; i <= 9; i++)
            delay[PC_SLOTS[i]] = d;
        writeDelays(5, PC_SLOTS + 5);
        break;
    }
    case 9:
        wet_words(*this, PC_MIX + 3, rom::PAN_LAW, PU(*this, 9), GGXX);
        writeCoefs(4, PC_MIX + 3);
        break;
    case 10: case 11:
        designFilter(2, 1, LOW_SHELF1, P(*this, 10), P(*this, 11), 0, EQ7_A);
        copyCoefs(5, EQ7_A, EQ7_A_C);
        break;
    case 12: case 13:
        designFilter(2, 4, HIGH_SHELF1, P(*this, 12), P(*this, 13), 0, EQ7_C);
        copyCoefs(5, EQ7_C, EQ7_C_C);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 9, 10, 12})
            fxInsEnsDetune(q);
        hold = false;
        break;
    }
}

// type 21; msb 7 lsb 6
void EffectHost::fxIns3BandEq(int p)
{
    switch (p) {
    case 0: case 5:
        designFilter(2, 1, LOW_SHELF2, P(*this, 5), PLO(*this, 0), 0, EQ7_A);
        copyCoefs(5, EQ7_A, EQ7_A_C);
        break;
    case 1: case 2: case 3:
        designFilter(2, 1, PEAK, P(*this, 1), P(*this, 2), P(*this, 3), EQ7_B);
        copyCoefs(5, EQ7_B, EQ7_B_C);
        break;
    case 4: case 6:
        designFilter(2, 4, HIGH_SHELF2, P(*this, 6), P(*this, 4), 0, EQ7_C);
        copyCoefs(5, EQ7_C, EQ7_C_C);
        break;
    case 14:
        if (P(*this, 14) != 0) {
            coef[PC_MIX[0]] = 0x4000;
            coef[PC_MIX[1]] = 0;
            coef[PC_MIX[2]] = 0x4000;
            coef[PC_MIX[3]] = typeGain(INSERTION, 8192);
            coef[EQ3_LEVEL[0]] = typeGain(INSERTION, 0);
        } else {
            coef[PC_MIX[0]] = 0;
            coef[PC_MIX[1]] = 8192;
            coef[PC_MIX[2]] = 8192;
            coef[PC_MIX[3]] = typeGain(INSERTION, 0);
            coef[EQ3_LEVEL[0]] = typeGain(INSERTION, 8192);
        }
        writeCoefs(3, PC_MIX);
        writeCoefs(1, PC_MIX + 3);
        writeCoefs(1, EQ3_LEVEL);
        break;
    case ALL:
        coef[PC_MIX[4]] = typeGain(INSERTION, 8192);
        hold = true;
        for (int q : {5, 6, 1, 14})
            fxIns3BandEq(q);
        hold = false;
        break;
    }
}

// type 20; msb 10 lsb 1
void EffectHost::fxInsLoFi(int p)
{
    switch (p) {
    case 0:
        coef[LOFI_RATE[0]] = P(*this, 0);
        writeCoefs(1, LOFI_RATE);
        break;
    case 1: {
        const uint32_t v = PU(*this, 1);
        const int32_t s = int16_t(v);
        uint32_t w;
        if (v < 48) w = uint32_t(s + 15);
        else if (v < 64) w = uint32_t(((s - 47) << 1) + 62);
        else if (v < 80) w = uint32_t(((s - 63) << 2) + 94);
        else if (v < 96) w = uint32_t(((s - 79) << 3) + 158);
        else if (v < 112) w = uint32_t(((s - 95) << 4) + 286);
        else if (v < 127) w = uint32_t(((s - 111) << 5) + 542);
        else w = 32767;
        coef[LOFI_BITS[0]] = w;
        writeCoefs(1, LOFI_BITS);
        break;
    }
    case 3: case 5:
        designFilter(2, 1, LPF2, P(*this, 3), 64, P(*this, 5), LOFI_LPF2);
        break;
    case 4: {
        const int8_t* row = rom::LOFI_FILTERS + uint32_t(int8_t(10 * PLO(*this, 4)));
        designFilter(1, 1, row[0], row[1], row[2], 0, LOFI_FILTER);
        designFilter(2, 2, row[3], row[4], row[5], row[6], LOFI_FILTER_2);
        designFilter(1, 2, row[7], row[8], row[9], 0, LOFI_FILTER_3);
        break;
    }
    case 7:
        if (PU(*this, 7) == 1) {
            designFilter(1, 2, HIGH_SHELF1, 44, 76, 0, LOFI_HIGH_SHELF);
            designFilter(1, 2, HIGH_SHELF1, 44, 52, 0, LOFI_HIGH_SHELF_2);
        } else {
            designFilter(1, 2, GAIN_ONLY, 0, 64, 0, LOFI_HIGH_SHELF);
            designFilter(1, 2, GAIN_ONLY, 0, 64, 0, LOFI_HIGH_SHELF_2);
        }
        break;
    case 2: case 6: case 9: {
        const uint32_t wet = PU(*this, 9);
        const int32_t dry = rom::PAN_LAW[128 - wet] >> 1;
        const int32_t wetw = rom::PAN_LAW[wet] >> 1;
        coef[LOFI_WET[0]] = dry;
        coef[LOFI_WET[1]] = dry;
        coef[LOFI_WET2[0]] = typeGain(INSERTION, wetw);
        coef[LOFI_WET2[1]] = typeGain(INSERTION, wetw);
        const uint32_t sel = PU(*this, 6);
        const int16_t* lawrow = rom::LOFI_LAW + PU(*this, 2);
        int32_t a = lawrow[0], b;
        switch (sel) {
        case 0: b = 2048; break;
        case 1: b = 4096; break;
        case 2: b = 8192; break;
        case 3: b = 16384; break;
        case 4: b = 32767; break;
        case 5: a = lawrow[6]; b = 32767; break;
        case 6: a = lawrow[12]; b = 32767; break;
        default: b = wetw; break;
        }
        coef[LOFI_LEVEL[0]] = a;
        coef[LOFI_OUT[0]] = b;
        const int32_t gsh = (32767 >> sel) >> 1;
        coef[LOFI_GAIN[0]] = gsh;
        coef[LOFI_GAIN2[0]] = gsh;
        writeCoefs(1, LOFI_GAIN);
        writeCoefs(1, LOFI_GAIN2);
        writeCoefs(2, LOFI_WET);
        writeCoefs(2, LOFI_WET2);
        writeCoefs(1, LOFI_LEVEL);
        writeCoefs(1, LOFI_OUT);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 7})
            fxInsLoFi(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

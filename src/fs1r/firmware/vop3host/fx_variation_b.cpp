// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The rest of the variation handlers: phasers, ensemble detune, wahs, 3-band EQ, dynamics,
// distortion, amp simulator, karaoke and the variation reverb.
#include "effect_host.h"

namespace fs1r::vop3host {

using namespace rom::var;

namespace {

inline int32_t P(const EffectHost& h, int i) { return h.param[VARIATION][i]; }
inline uint32_t mod_delay(EffectHost& h, const int16_t* table, int p)
{
    return uint32_t(h.toSamples(table[P(h, p)])) & 0xFFFF;
}

// the pan words (parameter 9): the pan law at index 1 twice and the type scale of the law's top
// twice, laid out (a, x, a, x) when `aax` is set and (a, a, x, x) otherwise
void pan_words_b(EffectHost& h, const uint16_t* steps, bool aax)
{
    const uint32_t g = rom::PAN_LAW[1] >> 2;
    const uint32_t x = h.typeGain(VARIATION, rom::PAN_LAW[127] >> 2);
    if (aax) {
        h.coef[steps[2]] = g;
        h.coef[steps[0]] = g;
        h.coef[steps[3]] = x;
        h.coef[steps[1]] = x;
    } else {
        h.coef[steps[1]] = g;
        h.coef[steps[0]] = g;
        h.coef[steps[3]] = x;
        h.coef[steps[2]] = x;
    }
    h.writeCoefs(4, steps);
}

void filters_b(EffectHost& h, int p, const uint16_t* lpf, const uint16_t* lpf_copy, const uint16_t* hpf, const uint16_t* hpf_copy)
{
    if (p == 5 || p == 6) {
        h.designFilter(1, 1, LOW_SHELF1, P(h, 5), P(h, 6), 0, lpf);
        h.copyCoefs(3, lpf, lpf_copy);
    } else {
        h.designFilter(1, 2, HIGH_SHELF1, P(h, 7), P(h, 8), 0, hpf);
        h.copyCoefs(3, hpf, hpf_copy);
    }
}

// the wah "sensitivity" pair: -327680 / v for v >= 11, else 0x8000
void wah_sense(EffectHost& h, int p, const uint16_t* steps)
{
    if (h.param[VARIATION][p] < 11) {
        h.coef[steps[1]] = 0x8000;
        h.coef[steps[0]] = 0x8000;
    } else {
        const int32_t x = int(-327680.0 / double(float(h.param[VARIATION][p])));
        h.coef[steps[1]] = int16_t(x);
        h.coef[steps[0]] = int16_t(x);
    }
    h.writeCoefs(2, steps);
}

// the wah "cutoff" word: 3052 v / 16 + 7837
inline int32_t wah_cutoff(uint32_t v) { return int32_t((3052u * (v & 0xFFFF)) >> 4) + 7837; }
}  // namespace

// ---- store helpers ----

// a drive word (704 + 32063 v / 127) into two
// steps and a derived gain (saturating at v > 10, scaled by the level curve and sqrt 2, at
// least 1 unless 0) into the third
void EffectHost::storeDrive(int v, int index, const uint16_t* steps)
{
    const int32_t drive = 704 + int32_t(32063u * (uint32_t(v) & 0xFFFF)) / 127;
    coef[steps[1]] = drive;
    coef[steps[2]] = drive;
    const float f3 = float(int16_t(drive));
    double d;
    if ((uint32_t(v) & 0xFFFF) > 10)
        d = 21.0327;
    else
        d = double(f3 * f3 / 495616.0f);
    const float f10 = float(d);
    const float f11 = 8192.0f * rom::LEVEL_CURVE[index];
    float x = f11 / f10 * 0.5f;
    x = float(double(x) * 1.414213562373095);
    uint32_t word;
    if (0.0 == double(x))
        word = uint32_t(int16_t(int(x)));
    else if (1.0 > double(x))
        word = 1;
    else
        word = uint32_t(int16_t(int(x)));
    coef[steps[3]] = word;
    writeCoefs(3, steps + 1);
}

// 0x8000 - t / 2, twice, then t
void EffectHost::storeCompAttack(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[v];
    const uint32_t w = (0x8000 - t) >> 1;
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = t;
    writeCoefs(3, steps);
}

// -0x8000 - t * 32, twice, then t
void EffectHost::storeGateAttack(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[v];
    const uint32_t w = (-32768 - t) * 32;
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = t;
    writeCoefs(3, steps);
}

// t, t, (-0x8000 - t) * 2
void EffectHost::storeWahRelease(int v, const uint16_t* steps)
{
    const int32_t t = rom::COMPLEMENT[v];
    coef[steps[1]] = t;
    coef[steps[0]] = t;
    coef[steps[2]] = (-32768 - t) * 2;
    writeCoefs(3, steps);
}

// two (base, a, b) slot triples from the
// second word of each scaled table pair (the variation reverb's early reflections)
void EffectHost::combDelays2(int clampSrc, int sel, const uint8_t* ids)
{
    const int16_t* tbl = rom::SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 12;
    const uint32_t diff = rom::DIFFUSION[clampSrc];
    for (uint32_t i = 0; i < 2; i++) {
        const uint32_t w = uint32_t(scaleDelay(tbl[2 * i + 1])) & 0xFFFF;
        const uint32_t base = uint32_t(delay[ids[3 * i]]) & 0xFFFF;
        delay[ids[3 * i + 1]] = base + w;
        delay[ids[3 * i + 2]] = (w - diff) + base;
    }
}

// ---- handlers ----

// type 5; p10 (stages) re-runs p11, p11 (direction) re-runs p9
void EffectHost::fxVarPhaser1(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 8);
        setLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        const uint32_t w = P(*this, 2) * 258;
        coef[PH_FB[1]] = w;
        coef[PH_FB[0]] = w;
        writeCoefs(2, PH_FB);
        const int32_t d = int32_t((uint32_t(P(*this, 1)) & 0xFFFF) * (127u - (uint32_t(P(*this, 2)) & 0xFFFF))) / 127;
        setLfoDepth(d, 8);
        setLfoDepth(d, 9);
        break;
    }
    case 3:
        coef[PH_WIDTH1[0]] = bipolar500(P(*this, 3));
        writeCoefs(1, PH_WIDTH1);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, PH_FILTER, PH_FILTER_3, PH_FILTER_2, PH_FILTER_4);
        break;
    case 9: {
        const uint32_t g = rom::PAN_LAW[1] >> 2;
        coef[PH_PAN[2]] = g;
        coef[PH_PAN[0]] = g;
        int32_t x = typeGain(VARIATION, rom::PAN_LAW[127] >> 2);
        coef[PH_PAN[1]] = x;
        if (P(*this, 11) != 0)
            x = -int16_t(x);
        coef[PH_PAN[3]] = x;
        writeCoefs(4, PH_PAN);
        break;
    }
    case 10: {
        const uint32_t n = param[VARIATION][10];
        int a, b;   // the two of the eight words set to 0x4000
        if (n <= 4) { a = 0; b = 4; }
        else if (n == 5) { a = 4; b = 1; }
        else if (n == 6) { a = 4; b = 2; }
        else if (n == 7) { a = 4; b = 3; }
        else if (n == 8) { a = 1; b = 7; }
        else if (n == 9) { a = 2; b = 7; }
        else { a = 3; b = 7; }
        for (int i = 0; i < 8; i++)
            coef[PH_STAGES[i]] = (i == a || i == b) ? 0x4000 : 0;
        writeCoefs(8, PH_STAGES);
        fxVarPhaser1(11);
        break;
    }
    case 11: {
        const int32_t dir = P(*this, 11);
        const uint32_t n = param[VARIATION][10];
        uint32_t w[3] = {0, 0, 0};
        if (dir == 0) w[0] = 0x4000;
        else if (n < 8) w[1] = 0x4000;
        else w[2] = 0x4000;
        for (int i = 0; i < 3; i++)
            coef[PH_DIR[i]] = w[i];
        writeCoefs(3, PH_DIR);
        fxVarPhaser1(9);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 10})
            fxVarPhaser1(q);
        hold = false;
        break;
    }
}

// type 6
void EffectHost::fxVarPhaser2(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 8);
        setLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        coef[PH_FB[0]] = P(*this, 2) * 258;
        coef[PH_FB[1]] = 242 * P(*this, 2);
        writeCoefs(2, PH_FB);
        const uint32_t p1 = uint32_t(P(*this, 1)) & 0xFFFF, p2 = uint32_t(P(*this, 2)) & 0xFFFF;
        setLfoDepth(int32_t(p1 * (127u - p2)) / 127, 8);
        const uint32_t v27 = (p2 << 4) - p2;
        setLfoDepth(int32_t(p1 * (127u - (v27 >> 4))) / 127, 9);
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
        filters_b(*this, p, PH_FILTER, PH_FILTER_3, PH_FILTER_2, PH_FILTER_4);
        break;
    case 9:
        pan_words_b(*this, PH_PAN, true);
        break;
    case 10: {
        const int32_t n = P(*this, 10);
        int a, b;
        if (n >= 0 && n <= 3) { a = 1; b = 5; }
        else if (n == 4) { a = 2; b = 6; }
        else { a = 3; b = 7; }
        for (int i = 0; i < 8; i++)
            coef[PH_STAGES[i]] = (i == a || i == b) ? 0x4000 : 0;
        writeCoefs(8, PH_STAGES);
        break;
    }
    case 12:
        setLfoPhase(P(*this, 12), 9);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 9, 10, 12})
            fxVarPhaser2(q);
        hold = false;
        break;
    }
}

// type 7
void EffectHost::fxVarEnsDetune(int p)
{
    switch (p) {
    case 0: {
        const uint32_t v = param[VARIATION][0];
        const double K = 65527.48886, NK = -65527.48886;
        uint16_t &lo = lfoReg[1][8], &hi = lfoReg[1][9];   // LFO 8 and 9
        if (v < 0x40) {
            const double a = double(rom::DETUNE[114u - v]) * K;
            const int32_t w0 = int(a + NK);
            const double b = double(rom::DETUNE[v - 14u]) * K;
            const int32_t w1 = int(K - b);
            coef[DETUNE_DET[1]] = int16_t(w0);
            coef[DETUNE_DET[0]] = int16_t(w0);
            coef[DETUNE_DET[3]] = int16_t(w1);
            coef[DETUNE_DET[2]] = int16_t(w1);
            lo = (lo & 0xFF1F) | 0x20;
            lfoReg[3][8] = 0x7FFF;
            hi = (hi & 0xFF1F) | 0x40;
            lfoReg[3][9] = 0;
        } else if (v < 0x41) {
            for (int i = 0; i < 4; i++)
                coef[DETUNE_DET[i]] = 0;
        } else {
            const double a = double(rom::DETUNE[114u - v]) * K;
            const int32_t w0 = int(-a + K);
            const double b = double(rom::DETUNE[v - 14u]) * K;
            const int32_t w1 = int(b + NK);
            coef[DETUNE_DET[1]] = int16_t(w0);
            coef[DETUNE_DET[0]] = int16_t(w0);
            coef[DETUNE_DET[3]] = int16_t(w1);
            coef[DETUNE_DET[2]] = int16_t(w1);
            lo = (lo & 0xFF1F) | 0x40;
            lfoReg[3][8] = 0;
            hi = (hi & 0xFF1F) | 0x20;
            lfoReg[3][9] = 0x7FFF;
        }
        writeCoefs(4, DETUNE_DET);
        writeLfo(0x25, 8);
        writeLfo(0x25, 9);
        writeLfo(0x27, 8);
        writeLfo(0x27, 9);
        break;
    }
    case 1: {
        const uint32_t t = mod_delay(*this, rom::MOD_DELAY, 1);
        const int32_t d = int32_t(t + uint32_t(delay[DETUNE_SLOTS[0]]) + 1);
        for (int i = 1; i <= 4; i++)
            delay[DETUNE_SLOTS[i]] = d;
        writeDelays(5, DETUNE_SLOTS);
        break;
    }
    case 2: {
        const uint32_t t = mod_delay(*this, rom::MOD_DELAY, 2);
        const int32_t base = int32_t(((TOP - AREA[4]) >> 1) + 1);
        delay[DETUNE_SLOTS[5]] = base;
        const int32_t d = int32_t(t + uint32_t(base) + 1);
        for (int i = 6; i <= 9; i++)
            delay[DETUNE_SLOTS[i]] = d;
        writeDelays(5, DETUNE_SLOTS + 5);
        break;
    }
    case 9:
        pan_words_b(*this, DETUNE_PAN, false);
        break;
    case 10: case 11:
        designFilter(1, 1, LOW_SHELF1, P(*this, 10), P(*this, 11), 0, DETUNE_LOW_SHELF);
        copyCoefs(3, DETUNE_LOW_SHELF, DETUNE_LOW_SHELF_COPY);
        break;
    case 12: case 13:
        designFilter(1, 2, HIGH_SHELF1, P(*this, 12), P(*this, 13), 0, DETUNE_HIGH_SHELF);
        copyCoefs(3, DETUNE_HIGH_SHELF, DETUNE_HIGH_SHELF_COPY);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 9, 10, 12})
            fxVarEnsDetune(q);
        hold = false;
        break;
    }
}

// type 11
void EffectHost::fxVarAutoWah(int p)
{
    switch (p) {
    case 0:
        setLfoSpeed(P(*this, 0), 8);
        break;
    case 1: case 2: {
        const int32_t w = wah_cutoff(uint32_t(P(*this, 2)));
        coef[WAH_CUTOFF[0]] = w;
        const uint32_t v14 = (uint32_t(P(*this, 1)) & 0xFFFF) * uint32_t(32767 - int16_t(w));
        coef[WAH_DEPTH[0]] = v14 / 127;
        writeCoefs(1, WAH_CUTOFF);
        writeCoefs(1, WAH_DEPTH);
        break;
    }
    case 3:
        wah_sense(*this, 3, WAH_SENSE);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, WAH_FILTER, WAH_FILTER_3, WAH_FILTER_2, WAH_FILTER_4);
        break;
    case 9:
        pan_words_b(*this, WAH_PAN, true);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 9})
            fxVarAutoWah(q);
        hold = false;
        break;
    }
}

// type 12
void EffectHost::fxVarTouchWah(int p)
{
    switch (p) {
    case 0:
        coef[WAH_ATTACK[0]] = P(*this, 0) * 258;
        writeCoefs(1, WAH_ATTACK);
        [[fallthrough]];   // the firmware runs on into the cutoff word
    case 1:
        coef[WAH_CUTOFF[0]] = wah_cutoff(uint32_t(P(*this, 1)));
        writeCoefs(1, WAH_CUTOFF);
        break;
    case 2:
        wah_sense(*this, 2, WAH_SENSE);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, WAH_FILTER, WAH_FILTER_3, WAH_FILTER_2, WAH_FILTER_4);
        break;
    case 9:
        pan_words_b(*this, WAH_PAN, true);
        break;
    case 15: {
        int32_t v = int8_t(param[VARIATION][15]) - 0x34;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        storeWahRelease(v, WAH_MIRROR);
        break;
    }
    case ALL:
        hold = true;
        designFilter(1, 1, HPF1, 0, 64, 0, WAH_HPF);
        designFilter(1, 0, LPF1, 54, 64, 0, WAH_LPF);
        for (int q : {0, 1, 2, 5, 7, 9, 15})
            fxVarTouchWah(q);
        hold = false;
        break;
    }
}

// type 13
void EffectHost::fxVar3BandEq(int p)
{
    switch (p) {
    case 0: case 5:
        designFilter(2, 1, LOW_SHELF2, P(*this, 5), int8_t(param[VARIATION][0]), 0, EQ3_LOW_SHELF2);
        copyCoefs(5, EQ3_LOW_SHELF2, EQ3_LOW_SHELF2_COPY);
        break;
    case 1: case 2: case 3:
        designFilter(2, 1, PEAK, P(*this, 1), P(*this, 2), P(*this, 3), EQ3_PEAK);
        copyCoefs(5, EQ3_PEAK, EQ3_PEAK_COPY);
        break;
    case 4: case 6:
        designFilter(2, 4, HIGH_SHELF2, P(*this, 6), P(*this, 4), 0, EQ3_HIGH_SHELF2);
        copyCoefs(5, EQ3_HIGH_SHELF2, EQ3_HIGH_SHELF2_COPY);
        break;
    case 14: {
        const uint32_t x = typeGain(VARIATION, 8192);
        if (P(*this, 14) == 0) {
            coef[EQ3_A[1]] = 8192;
            coef[EQ3_A[0]] = 8192;
            coef[EQ3_A[2]] = 0x4000;
            coef[EQ3_B[1]] = x;
            coef[EQ3_B[0]] = x;
            coef[EQ3_B[2]] = 0;
        } else {
            coef[EQ3_A[0]] = 0x4000;
            coef[EQ3_A[1]] = 0;
            coef[EQ3_A[2]] = 0x4000;
            coef[EQ3_B[2]] = x;
            coef[EQ3_B[0]] = x;
            coef[EQ3_B[1]] = 0;
        }
        writeCoefs(2, EQ3_A);
        writeCoefs(2, EQ3_B + 1);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {0, 1, 4, 14})
            fxVar3BandEq(q);
        hold = false;
        break;
    }
}

// type 14
void EffectHost::fxVarHmEnhancer(int p)
{
    switch (p) {
    case 0:
        designFilter(2, 1, HPF2, P(*this, 0), 64, 7, DYN_HPF2);
        copyCoefs(5, DYN_HPF2, DYN_HPF2_COPY);
        designFilter(1, 1, HPF1, P(*this, 0), 64, 0, DYN_HPF);
        copyCoefs(3, DYN_HPF, DYN_HPF_COPY);
        break;
    case 1: {
        const uint32_t w = P(*this, 1) * 0x102;
        coef[HM_DRIVE[1]] = w;
        coef[HM_DRIVE[0]] = w;
        writeCoefs(2, HM_DRIVE);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(typeGain(VARIATION, int32_t((uint32_t(P(*this, 2)) & 0xFFFF) * 0x81) >> 1));
        coef[DYN_OUT[3]] = x;
        coef[DYN_OUT[1]] = x;
        writeCoefs(1, DYN_OUT + 1);
        writeCoefs(1, DYN_OUT + 3);
        break;
    }
    case ALL:
        hold = true;
        for (int q : {0, 1, 2})
            fxVarHmEnhancer(q);
        hold = false;
        break;
    }
}

namespace {
// the compressor / noise gate output level word pair: level curve * 8192 through table D
void dynamics_level(EffectHost& h, int p)
{
    const int32_t i = int(double(rom::LEVEL_CURVE[P(h, p)]) * 8192.0);
    const uint32_t x = h.typeGain(VARIATION, int16_t(i));
    h.coef[DYN_OUT[3]] = x;
    h.coef[DYN_OUT[1]] = x;
    h.coef[DYN_OUT[2]] = 0;
    h.coef[DYN_OUT[0]] = 0;
    h.writeCoefs(4, DYN_OUT);
}
void dynamics_filters(EffectHost& h)
{
    h.designFilter(2, 1, HPF1, 0, 64, 0, DYN_HPF2);
    h.copyCoefs(5, DYN_HPF2, DYN_HPF2_COPY);
    h.designFilter(1, 1, GAIN_ONLY, 0, 64, 0, DYN_HPF);
    h.copyCoefs(3, DYN_HPF, DYN_HPF_COPY);
}
}  // namespace

// type 16
void EffectHost::fxVarCompressor(int p)
{
    switch (p) {
    case 0: storeCompAttack(P(*this, 0), DYN_ATTACK); break;
    case 1: storeRelease(P(*this, 1), DYN_RELEASE); break;
    case 2: storeCompThreshold(P(*this, 2), DYN_THRESHOLD); break;
    case 3: storeRatio(P(*this, 3), DYN_RATIO); break;
    case 4: dynamics_level(*this, 4); break;
    case ALL:
        hold = true;
        dynamics_filters(*this);
        for (int q : {0, 1, 2, 3, 4})
            fxVarCompressor(q);
        hold = false;
        break;
    }
}

// type 15
void EffectHost::fxVarNoiseGate(int p)
{
    switch (p) {
    case 0: storeGateAttack(P(*this, 0), DYN_ATTACK); break;
    case 1: storeRelease(P(*this, 1), DYN_RELEASE); break;
    case 2: storeGateThreshold(P(*this, 2), DYN_THRESHOLD); break;
    case 3: dynamics_level(*this, 3); break;
    case ALL:
        hold = true;
        dynamics_filters(*this);
        for (int q : {0, 1, 2, 3})
            fxVarNoiseGate(q);
        hold = false;
        break;
    }
}

namespace {
// the distortion group's pan words, laid out a, x, a, x
void dist_pan_words(EffectHost& h)
{
    const uint32_t g = rom::PAN_LAW[1] >> 2;
    h.coef[DIST_PAN[2]] = g;
    h.coef[DIST_PAN[0]] = g;
    const uint32_t x = h.typeGain(VARIATION, rom::PAN_LAW[127] >> 2);
    h.coef[DIST_PAN[3]] = x;
    h.coef[DIST_PAN[1]] = x;
    h.writeCoefs(4, DIST_PAN);
}
}  // namespace

// Distortion (17) and Overdrive (18)
void EffectHost::fxVarDistortion(int p)
{
    switch (p) {
    case 0: case 4:
        storeDrive(P(*this, 0), P(*this, 4), DIST_DRIVE);
        break;
    case 1: case 2:
        designFilter(2, 4, LOW_SHELF1, P(*this, 1), P(*this, 2), 0, DIST_LOW_SHELF);
        break;
    case 3:
        designFilter(2, 1, LPF2, P(*this, 3), 64, 7, DIST_LPF2);
        break;
    case 6: case 7: case 8:
        designFilter(2, 1, PEAK, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PEAK);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        storeEdge(P(*this, 10), DIST_BAND);
        break;
    case ALL: {
        hold = true;
        int f;
        if (code[VARIATION].lsb == 1) {
            designFilter(1, 1, HPF1, 0, 64, 0, DIST_HPF);
            f = 0x36;
        } else {
            designFilter(1, 1, HPF1, 8, 64, 0, DIST_HPF);
            f = 0x2C;
        }
        designFilter(1, 1, LPF1, f, 64, 0, DIST_LPF);
        for (int q : {0, 1, 3, 6, 9, 10})
            fxVarDistortion(q);
        hold = false;
        break;
    }
    }
}

// type 19; p1 (the amp type) re-runs p2
void EffectHost::fxVarAmpSim(int p)
{
    switch (p) {
    case 0: case 3:
        storeDrive(P(*this, 0), P(*this, 3), DIST_DRIVE);
        break;
    case 1: {
        const int8_t* row = rom::AMP_FILTERS + uint32_t(int8_t((param[VARIATION][1] & 0xFF) * 14));
        designFilter(1, 1, row[0], row[2], row[1], 0, DIST_HPF);
        designFilter(1, 1, row[3], row[5], row[4], 0, DIST_LPF);
        designFilter(2, 1, row[6], row[8], row[7], 0, DIST_PEAK);
        designFilter(2, 4, row[9], row[11], row[10], row[12], DIST_LOW_SHELF);
        fxVarAmpSim(2);
        break;
    }
    case 2:
        if (P(*this, 1) == 1)
            designFilter(2, 1, LPF2, P(*this, 2), 0x38, 0xF, DIST_LPF2);
        else
            designFilter(2, 1, LPF2, P(*this, 2), 0x40, P(*this, 1) == 2 ? 0xF : 7, DIST_LPF2);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        storeEdge(P(*this, 10), DIST_BAND);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 9, 10})
            fxVarAmpSim(q);
        hold = false;
        break;
    }
}

// type 24
void EffectHost::fxVarKaraoke(int p)
{
    switch (p) {
    case 0: {
        const uint32_t t = mod_delay(*this, rom::PITCH_DELAY, 0);
        delay[KARAOKE_SLOTS[1]] = uint32_t(delay[KARAOKE_SLOTS[0]]) + t;
        writeDelays(1, KARAOKE_SLOTS + 1);
        break;
    }
    case 1: {
        const uint32_t w = (P(*this, 1) - 64) * 250;
        coef[KARAOKE_LEVEL[1]] = w;
        coef[KARAOKE_LEVEL[0]] = w;
        writeCoefs(2, KARAOKE_LEVEL);
        break;
    }
    case 2:
        designFilter(1, 1, HPF1, P(*this, 2), 64, 0, KARAOKE_HPF);
        break;
    case 3:
        designFilter(1, 1, LPF1, P(*this, 3), 64, 0, KARAOKE_LPF);
        break;
    case 9: {
        const uint32_t g = rom::ER_REV[1] >> 2;
        coef[KARAOKE_PAN[2]] = g;
        coef[KARAOKE_PAN[0]] = g;
        const uint32_t x = typeGain(VARIATION, rom::ER_REV[127] >> 2);
        coef[KARAOKE_PAN[3]] = x;
        coef[KARAOKE_PAN[1]] = x;
        writeCoefs(4, KARAOKE_PAN);
        break;
    }
    case 10:
        diffusers(P(*this, 10) > 3 ? 3 : P(*this, 10), 5, 3, 4, 0, KARAOKE_ROW_STEPS, KARAOKE_ROW_SLOTS);   // a hidden parameter; the table has four rows
        writeCoefs(16, KARAOKE_ROW_STEPS);
        writeDelays(8, KARAOKE_ROW_SLOTS);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 9, 10})
            fxVarKaraoke(q);
        hold = false;
        break;
    }
}

// Hall, Room, Stage, Plate (types 25-28); the comb selector row is
// indexed by the type's lsb byte; p1 re-runs p12
void EffectHost::fxVarReverb(int p)
{
    const int16_t* sel = rom::COMB_SELECT + 3 * code[VARIATION].lsb;
    switch (p) {
    case 0: case 13:
        designCombs(P(*this, 0), P(*this, 13), 2, sel[1], REVERB_COMBS);
        writeCoefs(8, REVERB_COMBS);
        break;
    case 1: {
        combDelays2(P(*this, 1), sel[1], REVERB_ER_IDS);
        const uint8_t ids[2] = {uint8_t(REVERB_ER_IDS[2]), uint8_t(REVERB_ER_IDS[5])};
        writeDelays(2, ids);
        fxVarReverb(12);
        break;
    }
    case 2: {
        const uint32_t t = toSamples(rom::DELAY_WORD[P(*this, 2)]);
        delay[REVERB_PRE[1]] = (uint32_t(delay[REVERB_PRE[0]]) + t) & 0xFFFF;
        writeDelays(1, REVERB_PRE + 1);
        break;
    }
    case 3:
        designFilter(1, 0, HPF1, P(*this, 3), 64, 0, REVERB_HPF);
        break;
    case 4:
        designFilter(1, 1, LPF1, P(*this, 4), 64, 0, REVERB_LPF);
        break;
    case 9: {
        const uint32_t g = rom::ER_REV[1] >> 2;
        coef[rom::var::REVERB_PAN[1]] = g;
        coef[rom::var::REVERB_PAN[0]] = g;
        const uint32_t x = typeGain(VARIATION, rom::ER_REV[127] >> 2);
        coef[rom::var::REVERB_PAN[3]] = x;
        coef[rom::var::REVERB_PAN[2]] = x;
        writeCoefs(4, rom::var::REVERB_PAN);
        break;
    }
    case 11:
        diffusers(P(*this, 11), P(*this, 1), sel[2], 2, 2, REVERB_ROW_STEPS, REVERB_ROW_SLOTS);
        writeCoefs(8, REVERB_ROW_STEPS);
        break;
    case 12: {
        const int32_t rowword = int16_t(rom::SPAN_ROWS[18 * (uint32_t(sel[0]) & 0xFFFF) + 6]);
        const uint32_t p12 = uint32_t(P(*this, 12)) & 0xFFFF;
        const int32_t v3 = rowword * rom::ER_REV[128u - p12];
        const uint32_t w = int16_t(uint32_t(v3) >> 16);
        coef[REVERB_W[0]] = w;
        for (int i = 1; i <= 5; i++)
            coef[REVERB_W[i]] = 0;
        const int32_t k = P(*this, 1);
        const int idx = k == 0 ? 1 : k == 1 ? 2 : k == 2 ? 3 : k == 3 ? 4 : 5;
        coef[REVERB_W[idx]] = w;
        const int32_t er = rom::ER_REV[p12];
        const int32_t w2 = ((3 * er) << 13) >> 15;   // shll8, shll2, shll2, then shll in the jsr delay slot, asr15
        for (int i = 6; i <= 9; i++)
            coef[REVERB_W[i]] = w2;
        writeCoefs(10, REVERB_W);
        break;
    }
    case 14:
        coef[REVERB_WIDTH[0]] = bipolar500(P(*this, 14));
        writeCoefs(1, REVERB_WIDTH);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 9, 11, 14})
            fxVarReverb(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

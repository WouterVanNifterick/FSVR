// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The rest of the variation handlers: phasers, ensemble detune, wahs, 3-band EQ, dynamics,
// distortion, amp simulator, karaoke and the variation reverb.
#include "host.h"

namespace fs1r::vop3host {

using namespace rom::var;

namespace {

inline int32_t P(const Host& h, int i) { return h.param[VARIATION][i]; }
inline uint32_t mod_delay(Host& h, const int16_t* table, int p)
{
    return uint32_t(h.fxMsToSamples(table[uint32_t(P(h, p)) & 0xFFFF])) & 0xFFFF;
}
constexpr uint32_t TOP = 0x40000;   // where the variation's delay memory ends

// the pan words (parameter 9) of the group whose list holds (a, x, a, x) with the pan law
// at index 1 and the type scale of the law's top; `a_first` selects the (a, a, x, x) layout
void pan_words_b(Host& h, const uint16_t* steps, bool aax)
{
    const uint32_t g = uint32_t(rom::PAN_LAW[1] >> 2);
    const uint32_t x = uint32_t(h.fxMulTypeTableD(rom::PAN_LAW[127] >> 2));
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
    h.writeSweWordListSub(4, steps);
}

void filters_b(Host& h, int p, const uint16_t* lpf, const uint16_t* lpf_copy, const uint16_t* hpf, const uint16_t* hpf_copy)
{
    if (p == 5 || p == 6) {
        h.fxDesignFilter(1, 1, 2, P(h, 5), P(h, 6), 0, lpf);
        h.fxCopyWords3(lpf, lpf_copy);
    } else {
        h.fxDesignFilter(1, 2, 3, P(h, 7), P(h, 8), 0, hpf);
        h.fxCopyWords3(hpf, hpf_copy);
    }
}

// the wah "sensitivity" pair: -327680 / v for v >= 11, else 0x8000
void wah_sense(Host& h, int p, const uint16_t* steps)
{
    if (h.param[VARIATION][p] < 11) {
        h.coef[steps[1]] = 0x8000;
        h.coef[steps[0]] = 0x8000;
    } else {
        const int32_t x = int(-327680.0 / double(float(h.param[VARIATION][p])));
        h.coef[steps[1]] = uint32_t(int16_t(x));
        h.coef[steps[0]] = uint32_t(int16_t(x));
    }
    h.writeSweWordListSub(2, steps);
}

// the wah "cutoff" word: 3052 v / 16 + 7837
inline int32_t wah_cutoff(uint32_t v) { return int32_t((3052u * (v & 0xFFFF)) >> 4) + 7837; }
}  // namespace

// ---- store helpers (fx_insertion.c) ----

// 0x3A4F00 fxStoreScaled(v, index, steps): a drive word (704 + 32063 v / 127) into two
// steps and a derived gain (saturating at v > 10, scaled by the level curve and sqrt 2, at
// least 1 unless 0) into the third
void Host::fxStoreScaled(int v, int index, const uint16_t* steps)
{
    const int32_t drive = 704 + int32_t(32063u * (uint32_t(v) & 0xFFFF)) / 127;
    coef[steps[1]] = uint32_t(drive);
    coef[steps[2]] = uint32_t(drive);
    const float f3 = float(int16_t(drive));
    double d;
    if ((uint32_t(v) & 0xFFFF) > 10)
        d = 21.0327;
    else
        d = double(f3 * f3 / 495616.0f);
    const float f10 = float(d);
    const float f11 = 8192.0f * rom::LEVEL_CURVE[uint32_t(index) & 0xFFFF];
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
    writeSweWordListSub(3, steps + 1);
}

// 0x3A5090 fxStoreMirrorHalf(v, steps): (0x8000 - t) / 2, twice, then t
void Host::fxStoreMirrorHalf(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[uint32_t(v) & 0xFFFF];
    const uint32_t w = uint32_t((0x8000 - t) >> 1);
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = uint32_t(t);
    writeSweWordListSub(3, steps);
}

// 0x3A50D8 fxStoreMirrorX32(v, steps): (-0x8000 - t) * 32, twice, then t
void Host::fxStoreMirrorX32(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[uint32_t(v) & 0xFFFF];
    const uint32_t w = uint32_t((-32768 - t) * 32);
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = uint32_t(t);
    writeSweWordListSub(3, steps);
}

// 0x3ACA08 fxStoreMirror3(v, steps): t, t, (-0x8000 - t) * 2
void Host::fxStoreMirror3(int v, const uint16_t* steps)
{
    const int32_t t = rom::COMPLEMENT[uint32_t(v) & 0xFF];
    coef[steps[1]] = uint32_t(t);
    coef[steps[0]] = uint32_t(t);
    coef[steps[2]] = uint32_t((-32768 - t) * 2);
    writeSweWordListSub(3, steps);
}

// 0x3A259C fxAddScaledPairs2(clampSrc, sel, ids): two (base, a, b) slot triples from the
// second word of each scaled table pair (the variation reverb's early reflections)
void Host::fxAddScaledPairs2(int clampSrc, int sel, const uint8_t* ids)
{
    const int16_t* tbl = rom::SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 12;
    const uint32_t diff = rom::DIFFUSION[uint32_t(clampSrc) & 0xFFFF];
    for (uint32_t i = 0; i < 2; i++) {
        const uint32_t w = uint32_t(fxMulShift16(tbl[2 * i + 1])) & 0xFFFF;
        const uint32_t base = uint32_t(delay[ids[3 * i]]) & 0xFFFF;
        delay[ids[3 * i + 1]] = int32_t(base + w);
        delay[ids[3 * i + 2]] = int32_t((w - diff) + base);
    }
}

// ---- handlers ----

// 0x3A1B70 fxVarPhaser1 (type 5); p10 (stages) re-runs p11, p11 (direction) re-runs p9
void Host::fxVarPhaser1(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        fxLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        const uint32_t w = uint32_t(P(*this, 2) * 258);
        coef[PH_FB[1]] = w;
        coef[PH_FB[0]] = w;
        writeSweWordListSub(2, PH_FB);
        const int32_t d = int32_t((uint32_t(P(*this, 1)) & 0xFFFF) * (127u - (uint32_t(P(*this, 2)) & 0xFFFF))) / 127;
        fxLfoDepth(d, 8);
        fxLfoDepth(d, 9);
        break;
    }
    case 3:
        coef[PH_WIDTH1[0]] = uint32_t(fxBipolar500(P(*this, 3)));
        writeSweWordListSub(1, PH_WIDTH1);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, PH_FILTER, PH_FILTER_3, PH_FILTER_2, PH_FILTER_4);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::PAN_LAW[1] >> 2);
        coef[PH_PAN[2]] = g;
        coef[PH_PAN[0]] = g;
        int32_t x = fxMulTypeTableD(rom::PAN_LAW[127] >> 2);
        coef[PH_PAN[1]] = uint32_t(x);
        if (P(*this, 11) != 0)
            x = -int16_t(x);
        coef[PH_PAN[3]] = uint32_t(x);
        writeSweWordListSub(4, PH_PAN);
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
        writeSweWordListSub(8, PH_STAGES);
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
        writeSweWordListSub(3, PH_DIR);
        fxVarPhaser1(9);
        break;
    }
    case 255:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 10})
            fxVarPhaser1(q);
        hold = false;
        break;
    }
}

// 0x3A2144 fxVarPhaser2 (type 6)
void Host::fxVarPhaser2(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        fxLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        coef[PH_FB[0]] = uint32_t(P(*this, 2) * 258);
        coef[PH_FB[1]] = uint32_t(242 * P(*this, 2));
        writeSweWordListSub(2, PH_FB);
        const uint32_t p1 = uint32_t(P(*this, 1)) & 0xFFFF, p2 = uint32_t(P(*this, 2)) & 0xFFFF;
        fxLfoDepth(int32_t(p1 * (127u - p2)) / 127, 8);
        const uint32_t v27 = (p2 << 4) - p2;
        fxLfoDepth(int32_t(p1 * (127u - (v27 >> 4))) / 127, 9);
        break;
    }
    case 3: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 3)));
        coef[PH_WIDTH2[1]] = x;
        coef[PH_WIDTH2[0]] = x;
        writeSweWordListSub(2, PH_WIDTH2);
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
        writeSweWordListSub(8, PH_STAGES);
        break;
    }
    case 12:
        fxLfoPhase(P(*this, 12), 9);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 9, 10, 12})
            fxVarPhaser2(q);
        hold = false;
        break;
    }
}

// 0x3A160C fxVarEnsDetune (type 7)
void Host::fxVarEnsDetune(int p)
{
    p &= 0xFF;
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
            coef[DETUNE_DET[1]] = uint32_t(int16_t(w0));
            coef[DETUNE_DET[0]] = uint32_t(int16_t(w0));
            coef[DETUNE_DET[3]] = uint32_t(int16_t(w1));
            coef[DETUNE_DET[2]] = uint32_t(int16_t(w1));
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
            coef[DETUNE_DET[1]] = uint32_t(int16_t(w0));
            coef[DETUNE_DET[0]] = uint32_t(int16_t(w0));
            coef[DETUNE_DET[3]] = uint32_t(int16_t(w1));
            coef[DETUNE_DET[2]] = uint32_t(int16_t(w1));
            lo = (lo & 0xFF1F) | 0x40;
            lfoReg[3][8] = 0;
            hi = (hi & 0xFF1F) | 0x20;
            lfoReg[3][9] = 0x7FFF;
        }
        writeSweWordListSub(4, DETUNE_DET);
        writeSwe25Sub(8);
        writeSwe25Sub(9);
        writeSwe27Sub(8);
        writeSwe27Sub(9);
        break;
    }
    case 1: {
        const uint32_t t = mod_delay(*this, rom::MOD_DELAY, 1);
        const int32_t d = int32_t(t + uint32_t(delay[DETUNE_SLOTS[0]]) + 1);
        for (int i = 1; i <= 4; i++)
            delay[DETUNE_SLOTS[uint32_t(i)]] = d;
        writeSweDeSub(5, DETUNE_SLOTS);
        break;
    }
    case 2: {
        const uint32_t t = mod_delay(*this, rom::MOD_DELAY, 2);
        const int32_t base = int32_t(((TOP - AREA[4]) >> 1) + 1);
        delay[DETUNE_SLOTS[5]] = base;
        const int32_t d = int32_t(t + uint32_t(base) + 1);
        for (int i = 6; i <= 9; i++)
            delay[DETUNE_SLOTS[uint32_t(i)]] = d;
        writeSweDeSub(5, DETUNE_SLOTS + 5);
        break;
    }
    case 9:
        pan_words_b(*this, DETUNE_PAN, false);
        break;
    case 10: case 11:
        fxDesignFilter(1, 1, 2, P(*this, 10), P(*this, 11), 0, DETUNE_LOW_SHELF);
        fxCopyWords3(DETUNE_LOW_SHELF, DETUNE_LOW_SHELF_COPY);
        break;
    case 12: case 13:
        fxDesignFilter(1, 2, 3, P(*this, 12), P(*this, 13), 0, DETUNE_HIGH_SHELF);
        fxCopyWords3(DETUNE_HIGH_SHELF, DETUNE_HIGH_SHELF_COPY);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 9, 10, 12})
            fxVarEnsDetune(q);
        hold = false;
        break;
    }
}

// 0x3A29F8 fxVarAutoWah (type 11)
void Host::fxVarAutoWah(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1: case 2: {
        const int32_t w = wah_cutoff(uint32_t(P(*this, 2)));
        coef[WAH_CUTOFF[0]] = uint32_t(w);
        const uint32_t v14 = (uint32_t(P(*this, 1)) & 0xFFFF) * uint32_t(32767 - int16_t(w));
        coef[WAH_DEPTH[0]] = v14 / 127;
        writeSweWordListSub(1, WAH_CUTOFF);
        writeSweWordListSub(1, WAH_DEPTH);
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
    case 255:
        hold = true;
        for (int q : {0, 1, 3, 5, 7, 9})
            fxVarAutoWah(q);
        hold = false;
        break;
    }
}

// 0x3A2CB0 fxVarTouchWah (type 12)
void Host::fxVarTouchWah(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        coef[WAH_ATTACK[0]] = uint32_t(P(*this, 0) * 258);
        writeSweWordListSub(1, WAH_ATTACK);
        [[fallthrough]];   // the firmware runs on into the cutoff word
    case 1:
        coef[WAH_CUTOFF[0]] = uint32_t(wah_cutoff(uint32_t(P(*this, 1))));
        writeSweWordListSub(1, WAH_CUTOFF);
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
        fxStoreMirror3(v, WAH_MIRROR);
        break;
    }
    case 255:
        hold = true;
        fxDesignFilter(1, 1, 1, 0, 64, 0, WAH_HPF);
        fxDesignFilter(1, 0, 0, 54, 64, 0, WAH_LPF);
        for (int q : {0, 1, 2, 5, 7, 9, 15})
            fxVarTouchWah(q);
        hold = false;
        break;
    }
}

// 0x39FB58 fxVar3BandEq (type 13)
void Host::fxVar3BandEq(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 5:
        fxDesignFilter(2, 1, 6, P(*this, 5), int8_t(param[VARIATION][0]), 0, EQ3_LOW_SHELF2);
        fxCopyWords5(EQ3_LOW_SHELF2, EQ3_LOW_SHELF2_COPY);
        break;
    case 1: case 2: case 3:
        fxDesignFilter(2, 1, 10, P(*this, 1), P(*this, 2), P(*this, 3), EQ3_PEAK);
        fxCopyWords5(EQ3_PEAK, EQ3_PEAK_COPY);
        break;
    case 4: case 6:
        fxDesignFilter(2, 4, 7, P(*this, 6), P(*this, 4), 0, EQ3_HIGH_SHELF2);
        fxCopyWords5(EQ3_HIGH_SHELF2, EQ3_HIGH_SHELF2_COPY);
        break;
    case 14: {
        const uint32_t x = uint32_t(fxMulTypeTableD(8192));
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
        writeSweWordListSub(2, EQ3_A);
        writeSweWordListSub(2, EQ3_B + 1);
        break;
    }
    case 255:
        hold = true;
        for (int q : {0, 1, 4, 14})
            fxVar3BandEq(q);
        hold = false;
        break;
    }
}

// 0x39FD74 fxVarHmEnhancer (type 14)
void Host::fxVarHmEnhancer(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxDesignFilter(2, 1, 5, P(*this, 0), 64, 7, DYN_HPF2);
        fxCopyWords5(DYN_HPF2, DYN_HPF2_COPY);
        fxDesignFilter(1, 1, 1, P(*this, 0), 64, 0, DYN_HPF);
        fxCopyWords3(DYN_HPF, DYN_HPF_COPY);
        break;
    case 1: {
        const uint32_t w = uint32_t(P(*this, 1) * 0x102);
        coef[HM_DRIVE[1]] = w;
        coef[HM_DRIVE[0]] = w;
        writeSweWordListSub(2, HM_DRIVE);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(fxMulTypeTableD(int32_t((uint32_t(P(*this, 2)) & 0xFFFF) * 0x81) >> 1));
        coef[DYN_OUT[3]] = x;
        coef[DYN_OUT[1]] = x;
        writeSweWordListSub(1, DYN_OUT + 1);
        writeSweWordListSub(1, DYN_OUT + 3);
        break;
    }
    case 255:
        hold = true;
        for (int q : {0, 1, 2})
            fxVarHmEnhancer(q);
        hold = false;
        break;
    }
}

namespace {
// the compressor / noise gate output level word pair: level curve * 8192 through table D
void dynamics_level(Host& h, int p)
{
    const int32_t i = int(double(rom::LEVEL_CURVE[uint32_t(P(h, p)) & 0xFFFF]) * 8192.0);
    const uint32_t x = uint32_t(h.fxMulTypeTableD(int16_t(i)));
    h.coef[DYN_OUT[3]] = x;
    h.coef[DYN_OUT[1]] = x;
    h.coef[DYN_OUT[2]] = 0;
    h.coef[DYN_OUT[0]] = 0;
    h.writeSweWordListSub(4, DYN_OUT);
}
void dynamics_filters(Host& h)
{
    h.fxDesignFilter(2, 1, 1, 0, 64, 0, DYN_HPF2);
    h.fxCopyWords5(DYN_HPF2, DYN_HPF2_COPY);
    h.fxDesignFilter(1, 1, 11, 0, 64, 0, DYN_HPF);
    h.fxCopyWords3(DYN_HPF, DYN_HPF_COPY);
}
}  // namespace

// 0x39FED4 fxVarCompressor (type 16)
void Host::fxVarCompressor(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: fxStoreMirrorHalf(P(*this, 0), DYN_ATTACK); break;
    case 1: fxWriteComplementPair(P(*this, 1), DYN_RELEASE); break;
    case 2: fxWriteWord(P(*this, 2), DYN_THRESHOLD); break;
    case 3: fxWriteRow8(P(*this, 3), DYN_RATIO); break;
    case 4: dynamics_level(*this, 4); break;
    case 255:
        hold = true;
        dynamics_filters(*this);
        for (int q : {0, 1, 2, 3, 4})
            fxVarCompressor(q);
        hold = false;
        break;
    }
}

// 0x3A00B0 fxVarNoiseGate (type 15)
void Host::fxVarNoiseGate(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: fxStoreMirrorX32(P(*this, 0), DYN_ATTACK); break;
    case 1: fxWriteComplementPair(P(*this, 1), DYN_RELEASE); break;
    case 2: fxWriteNegated(P(*this, 2), DYN_THRESHOLD); break;
    case 3: dynamics_level(*this, 3); break;
    case 255:
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
void dist_pan_words(Host& h)
{
    const uint32_t g = uint32_t(rom::PAN_LAW[1] >> 2);
    h.coef[DIST_PAN[2]] = g;
    h.coef[DIST_PAN[0]] = g;
    const uint32_t x = uint32_t(h.fxMulTypeTableD(rom::PAN_LAW[127] >> 2));
    h.coef[DIST_PAN[3]] = x;
    h.coef[DIST_PAN[1]] = x;
    h.writeSweWordListSub(4, DIST_PAN);
}
}  // namespace

// 0x39F520 fxVarDistortion: Distortion (17) and Overdrive (18)
void Host::fxVarDistortion(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 4:
        fxStoreScaled(P(*this, 0), P(*this, 4), DIST_DRIVE);
        break;
    case 1: case 2:
        fxDesignFilter(2, 4, 2, P(*this, 1), P(*this, 2), 0, DIST_LOW_SHELF);
        break;
    case 3:
        fxDesignFilter(2, 1, 4, P(*this, 3), 64, 7, DIST_LPF2);
        break;
    case 6: case 7: case 8:
        fxDesignFilter(2, 1, 10, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PEAK);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        fxFillBand(P(*this, 10), DIST_BAND);
        break;
    case 255: {
        hold = true;
        int f;
        if (code[VARIATION].lsb == 1) {
            fxDesignFilter(1, 1, 1, 0, 64, 0, DIST_HPF);
            f = 0x36;
        } else {
            fxDesignFilter(1, 1, 1, 8, 64, 0, DIST_HPF);
            f = 0x2C;
        }
        fxDesignFilter(1, 1, 0, f, 64, 0, DIST_LPF);
        for (int q : {0, 1, 3, 6, 9, 10})
            fxVarDistortion(q);
        hold = false;
        break;
    }
    }
}

// 0x39F760 fxVarAmpSim (type 19); p1 (the amp type) re-runs p2
void Host::fxVarAmpSim(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 3:
        fxStoreScaled(P(*this, 0), P(*this, 3), DIST_DRIVE);
        break;
    case 1: {
        const int8_t* row = rom::AMP_FILTERS + uint32_t(int8_t((param[VARIATION][1] & 0xFF) * 14));
        fxDesignFilter(1, 1, row[0], row[2], row[1], 0, DIST_HPF);
        fxDesignFilter(1, 1, row[3], row[5], row[4], 0, DIST_LPF);
        fxDesignFilter(2, 1, row[6], row[8], row[7], 0, DIST_PEAK);
        fxDesignFilter(2, 4, row[9], row[11], row[10], row[12], DIST_LOW_SHELF);
        fxVarAmpSim(2);
        break;
    }
    case 2:
        if (P(*this, 1) == 1)
            fxDesignFilter(2, 1, 4, P(*this, 2), 0x38, 0xF, DIST_LPF2);
        else
            fxDesignFilter(2, 1, 4, P(*this, 2), 0x40, P(*this, 1) == 2 ? 0xF : 7, DIST_LPF2);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        fxFillBand(P(*this, 10), DIST_BAND);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 9, 10})
            fxVarAmpSim(q);
        hold = false;
        break;
    }
}

// 0x3A02A4 fxVarKaraoke (type 24)
void Host::fxVarKaraoke(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: {
        const uint32_t t = mod_delay(*this, rom::PITCH_DELAY, 0);
        delay[KARAOKE_SLOTS[1]] = int32_t(uint32_t(delay[KARAOKE_SLOTS[0]]) + t);
        writeSweDeSub(1, KARAOKE_SLOTS + 1);
        break;
    }
    case 1: {
        const uint32_t w = uint32_t((P(*this, 1) - 64) * 250);
        coef[KARAOKE_LEVEL[1]] = w;
        coef[KARAOKE_LEVEL[0]] = w;
        writeSweWordListSub(2, KARAOKE_LEVEL);
        break;
    }
    case 2:
        fxDesignFilter(1, 1, 1, P(*this, 2), 64, 0, KARAOKE_HPF);
        break;
    case 3:
        fxDesignFilter(1, 1, 0, P(*this, 3), 64, 0, KARAOKE_LPF);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::ER_REV[1] >> 2);
        coef[KARAOKE_PAN[2]] = g;
        coef[KARAOKE_PAN[0]] = g;
        const uint32_t x = uint32_t(fxMulTypeTableD(rom::ER_REV[127] >> 2));
        coef[KARAOKE_PAN[3]] = x;
        coef[KARAOKE_PAN[1]] = x;
        writeSweWordListSub(4, KARAOKE_PAN);
        break;
    }
    case 10:
        fxScaleRow(P(*this, 10) > 3 ? 3 : P(*this, 10), 5, 3, 4, 0, KARAOKE_ROW_STEPS, KARAOKE_ROW_SLOTS);   // a hidden parameter; the table has four rows
        writeSweWordListSub(16, KARAOKE_ROW_STEPS);
        writeSweDeSub(8, KARAOKE_ROW_SLOTS);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 3, 9, 10})
            fxVarKaraoke(q);
        hold = false;
        break;
    }
}

// 0x3A2628 fxVarReverb: Hall, Room, Stage, Plate (types 25-28); the comb selector row is
// indexed by the type's lsb byte; p1 re-runs p12
void Host::fxVarReverb(int p)
{
    p &= 0xFF;
    const int16_t* sel = rom::COMB_SELECT + 3 * code[VARIATION].lsb;
    switch (p) {
    case 0: case 13:
        fxDesignCombs(P(*this, 0), P(*this, 13), 2, sel[1], REVERB_COMBS);
        writeSweWordListSub(8, REVERB_COMBS);
        break;
    case 1: {
        fxAddScaledPairs2(P(*this, 1), sel[1], REVERB_ER_IDS);
        const uint8_t ids[2] = {uint8_t(REVERB_ER_IDS[2]), uint8_t(REVERB_ER_IDS[5])};
        writeSweDeSub(2, ids);
        fxVarReverb(12);
        break;
    }
    case 2: {
        const uint32_t t = uint32_t(fxMsToSamples(rom::DELAY_WORD[uint32_t(P(*this, 2)) & 0xFFFF]));
        delay[REVERB_PRE[1]] = int32_t((uint32_t(delay[REVERB_PRE[0]]) + t) & 0xFFFF);
        writeSweDeSub(1, REVERB_PRE + 1);
        break;
    }
    case 3:
        fxDesignFilter(1, 0, 1, P(*this, 3), 64, 0, REVERB_HPF);
        break;
    case 4:
        fxDesignFilter(1, 1, 0, P(*this, 4), 64, 0, REVERB_LPF);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::ER_REV[1] >> 2);
        coef[REVERB_PAN[1]] = g;
        coef[REVERB_PAN[0]] = g;
        const uint32_t x = uint32_t(fxMulTypeTableD(rom::ER_REV[127] >> 2));
        coef[REVERB_PAN[3]] = x;
        coef[REVERB_PAN[2]] = x;
        writeSweWordListSub(4, REVERB_PAN);
        break;
    }
    case 11:
        fxScaleRow(P(*this, 11), P(*this, 1), sel[2], 2, 2, REVERB_ROW_STEPS, REVERB_ROW_SLOTS);
        writeSweWordListSub(8, REVERB_ROW_STEPS);
        break;
    case 12: {
        const int32_t rowword = int16_t(rom::SPAN_ROWS[18 * (uint32_t(sel[0]) & 0xFFFF) + 6]);
        const uint32_t p12 = uint32_t(P(*this, 12)) & 0xFFFF;
        const int32_t v3 = rowword * rom::ER_REV[128u - p12];
        const uint32_t w = uint32_t(int16_t(uint32_t(v3) >> 16));
        coef[REVERB_W[0]] = w;
        for (int i = 1; i <= 5; i++)
            coef[REVERB_W[i]] = 0;
        const int32_t k = P(*this, 1);
        const int idx = k == 0 ? 1 : k == 1 ? 2 : k == 2 ? 3 : k == 3 ? 4 : 5;
        coef[REVERB_W[idx]] = w;
        const int32_t er = rom::ER_REV[p12];
        const int32_t w2 = ((3 * er) << 13) >> 15;   // shll8, shll2, shll2, then shll in the jsr delay slot, asr15
        for (int i = 6; i <= 9; i++)
            coef[REVERB_W[i]] = uint32_t(w2);
        writeSweWordListSub(10, REVERB_W);
        break;
    }
    case 14:
        coef[REVERB_WIDTH[0]] = uint32_t(fxBipolar500(P(*this, 14)));
        writeSweWordListSub(1, REVERB_WIDTH);
        break;
    case 255:
        hold = true;
        for (int q : {0, 1, 2, 3, 4, 9, 11, 14})
            fxVarReverb(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

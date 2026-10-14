// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The rest of the variation handlers: phasers, ensemble detune, wahs, 3-band EQ, dynamics,
// distortion, amp simulator, karaoke and the variation reverb.
#include "host.h"

namespace fs1r::vop3host {

namespace {
constexpr uint32_t MOD_DELAY_TABLE = 0x371BD4;     // u16: modulation delay byte -> 0.1 ms word
constexpr uint32_t KARAOKE_DELAY_TABLE = 0x371DD4; // u16: karaoke delay byte -> 0.1 ms word
constexpr uint32_t DETUNE_TABLE = 0x36F408;        // f32: detune ratio per index
constexpr uint32_t FX_LEVEL_CURVE = 0x3717D0;      // f32 x 128: 0..1 law
constexpr uint32_t MIRROR_TABLE = 0x36F67C;        // s16 (fxStoreMirrorHalf / X32)
constexpr uint32_t COMPLEMENT_TABLE = 0x36F6A4;    // s16 (fxStoreMirror3)
constexpr uint32_t AMP_ROWS = 0x36F5B8;            // s8 rows of 14: the amp simulator's filter sets
constexpr uint32_t SCALED_PAIRS = 0x36FE3C;        // s16 rows, stride 0x18 (fxAddScaledPairs2)
constexpr uint32_t ER_ROW_WORDS = 0x36FF56;        // s16, stride 0x24: variation reverb ER gain per comb row

inline int32_t P(const Host& h, int i) { return h.param[VARIATION][i]; }
inline uint32_t mod_delay(Host& h, uint32_t table, int p)
{
    return uint32_t(h.fxMsToSamples(rom::s16(table + 2u * (uint32_t(P(h, p)) & 0xFFFF)))) & 0xFFFF;
}
constexpr uint32_t TOP = 0x40000;   // where the variation's delay memory ends

// the pan words (parameter 9) of the group whose list holds (a, x, a, x) with the pan law
// at index 1 and the type scale of the law's top; `a_first` selects the (a, a, x, x) layout
void pan_words_b(Host& h, uint32_t steps, bool aax)
{
    const uint32_t g = uint32_t(rom::s16(rom::FX_PAN_LAW + 2) >> 2);
    const uint32_t x = uint32_t(h.fxMulTypeTableD(rom::s16(0x371ACE) >> 2));
    if (aax) {
        h.coef[rom::u16(steps + 4)] = g;
        h.coef[rom::u16(steps)] = g;
        h.coef[rom::u16(steps + 6)] = x;
        h.coef[rom::u16(steps + 2)] = x;
    } else {
        h.coef[rom::u16(steps + 2)] = g;
        h.coef[rom::u16(steps)] = g;
        h.coef[rom::u16(steps + 6)] = x;
        h.coef[rom::u16(steps + 4)] = x;
    }
    h.writeSweWordListSub(4, steps);
}

void filters_b(Host& h, int p, uint32_t lpf, uint32_t lpf_copy, uint32_t hpf, uint32_t hpf_copy)
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
void wah_sense(Host& h, int p, uint32_t steps)
{
    if (h.param[VARIATION][p] < 11) {
        h.coef[rom::u16(steps + 2)] = 0x8000;
        h.coef[rom::u16(steps)] = 0x8000;
    } else {
        const int32_t x = int(-327680.0 / double(float(h.param[VARIATION][p])));
        h.coef[rom::u16(steps + 2)] = uint32_t(int16_t(x));
        h.coef[rom::u16(steps)] = uint32_t(int16_t(x));
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
void Host::fxStoreScaled(int v, int index, uint32_t steps)
{
    const int32_t drive = 704 + int32_t(32063u * (uint32_t(v) & 0xFFFF)) / 127;
    coef[rom::u16(steps + 2)] = uint32_t(drive);
    coef[rom::u16(steps + 4)] = uint32_t(drive);
    const float f3 = float(int16_t(drive));
    double d;
    if ((uint32_t(v) & 0xFFFF) > 10)
        d = 21.0327;
    else
        d = double(f3 * f3 / 495616.0f);
    const float f10 = float(d);
    const float f11 = 8192.0f * rom::f32(FX_LEVEL_CURVE + 4u * (uint32_t(index) & 0xFFFF));
    float x = f11 / f10 * 0.5f;
    x = float(double(x) * 1.414213562373095);
    uint32_t word;
    if (0.0 == double(x))
        word = uint32_t(int16_t(int(x)));
    else if (1.0 > double(x))
        word = 1;
    else
        word = uint32_t(int16_t(int(x)));
    coef[rom::u16(steps + 6)] = word;
    writeSweWordListSub(3, steps + 2);
}

// 0x3A5090 fxStoreMirrorHalf(v, steps): (0x8000 - t) / 2, twice, then t
void Host::fxStoreMirrorHalf(int v, uint32_t steps)
{
    const int32_t t = rom::s16(MIRROR_TABLE + 2u * (uint32_t(v) & 0xFFFF));
    const uint32_t w = uint32_t((0x8000 - t) >> 1);
    coef[rom::u16(steps + 2)] = w;
    coef[rom::u16(steps)] = w;
    coef[rom::u16(steps + 4)] = uint32_t(t);
    writeSweWordListSub(3, steps);
}

// 0x3A50D8 fxStoreMirrorX32(v, steps): (-0x8000 - t) * 32, twice, then t
void Host::fxStoreMirrorX32(int v, uint32_t steps)
{
    const int32_t t = rom::s16(MIRROR_TABLE + 2u * (uint32_t(v) & 0xFFFF));
    const uint32_t w = uint32_t((-32768 - t) * 32);
    coef[rom::u16(steps + 2)] = w;
    coef[rom::u16(steps)] = w;
    coef[rom::u16(steps + 4)] = uint32_t(t);
    writeSweWordListSub(3, steps);
}

// 0x3ACA08 fxStoreMirror3(v, steps): t, t, (-0x8000 - t) * 2
void Host::fxStoreMirror3(int v, uint32_t steps)
{
    const int32_t t = rom::s16(COMPLEMENT_TABLE + 2u * (uint32_t(v) & 0xFF));
    coef[rom::u16(steps + 2)] = uint32_t(t);
    coef[rom::u16(steps)] = uint32_t(t);
    coef[rom::u16(steps + 4)] = uint32_t((-32768 - t) * 2);
    writeSweWordListSub(3, steps);
}

// 0x3A259C fxAddScaledPairs2(clampSrc, sel, ids): two (base, a, b) slot triples from the
// second word of each scaled table pair (the variation reverb's early reflections)
void Host::fxAddScaledPairs2(int clampSrc, int sel, uint32_t ids)
{
    const uint32_t tbl = SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 0x18;
    const uint32_t diff = rom::u8(rom::FX_DIFFUSION + (uint32_t(clampSrc) & 0xFFFF));
    for (uint32_t i = 0; i < 2; i++) {
        const uint32_t w = uint32_t(fxMulShift16(rom::s16(tbl + 4 * i + 2))) & 0xFFFF;
        const uint32_t base = uint32_t(delay[rom::u8(ids + 3 * i)]) & 0xFFFF;
        delay[rom::u8(ids + 3 * i + 1)] = int32_t(base + w);
        delay[rom::u8(ids + 3 * i + 2)] = int32_t((w - diff) + base);
    }
}

// ---- handlers ----

// 0x3A1B70 fxVarPhaser1 (type 5); p10 (stages) re-runs p11, p11 (direction) re-runs p9
void Host::fxVarPhaser1(int p)
{
    p &= 0xFF;
    constexpr uint32_t FB = 0x35D960, WIDTH = 0x35D97C, STAGES = 0x35D99C, DIR = 0x35D9AE, PAN = 0x35DA62;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        fxLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        const uint32_t w = uint32_t(P(*this, 2) * 258);
        coef[rom::u16(FB + 2)] = w;
        coef[rom::u16(FB)] = w;
        writeSweWordListSub(2, FB);
        const int32_t d = int32_t((uint32_t(P(*this, 1)) & 0xFFFF) * (127u - (uint32_t(P(*this, 2)) & 0xFFFF))) / 127;
        fxLfoDepth(d, 8);
        fxLfoDepth(d, 9);
        break;
    }
    case 3:
        coef[rom::u16(WIDTH)] = uint32_t(fxBipolar500(P(*this, 3)));
        writeSweWordListSub(1, WIDTH);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, 0x35E746, 0x35E752, 0x35E74C, 0x35E758);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::s16(rom::FX_PAN_LAW + 2) >> 2);
        coef[rom::u16(PAN + 4)] = g;
        coef[rom::u16(PAN)] = g;
        int32_t x = fxMulTypeTableD(rom::s16(0x371ACE) >> 2);
        coef[rom::u16(PAN + 2)] = uint32_t(x);
        if (P(*this, 11) != 0)
            x = -int16_t(x);
        coef[rom::u16(PAN + 6)] = uint32_t(x);
        writeSweWordListSub(4, PAN);
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
            coef[rom::u16(STAGES + 2u * uint32_t(i))] = (i == a || i == b) ? 0x4000 : 0;
        writeSweWordListSub(8, STAGES);
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
            coef[rom::u16(DIR + 2u * uint32_t(i))] = w[i];
        writeSweWordListSub(3, DIR);
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
    constexpr uint32_t FB = 0x35D960, WIDTH = 0x35D978, STAGES = 0x35D99C, PAN = 0x35DA62;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        fxLfoSpeed(P(*this, 0), 9);
        break;
    case 1: case 2: {
        coef[rom::u16(FB)] = uint32_t(P(*this, 2) * 258);
        coef[rom::u16(FB + 2)] = uint32_t(242 * P(*this, 2));
        writeSweWordListSub(2, FB);
        const uint32_t p1 = uint32_t(P(*this, 1)) & 0xFFFF, p2 = uint32_t(P(*this, 2)) & 0xFFFF;
        fxLfoDepth(int32_t(p1 * (127u - p2)) / 127, 8);
        const uint32_t v27 = (p2 << 4) - p2;
        fxLfoDepth(int32_t(p1 * (127u - (v27 >> 4))) / 127, 9);
        break;
    }
    case 3: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 3)));
        coef[rom::u16(WIDTH + 2)] = x;
        coef[rom::u16(WIDTH)] = x;
        writeSweWordListSub(2, WIDTH);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, 0x35E746, 0x35E752, 0x35E74C, 0x35E758);
        break;
    case 9:
        pan_words_b(*this, PAN, true);
        break;
    case 10: {
        const int32_t n = P(*this, 10);
        int a, b;
        if (n >= 0 && n <= 3) { a = 1; b = 5; }
        else if (n == 4) { a = 2; b = 6; }
        else { a = 3; b = 7; }
        for (int i = 0; i < 8; i++)
            coef[rom::u16(STAGES + 2u * uint32_t(i))] = (i == a || i == b) ? 0x4000 : 0;
        writeSweWordListSub(8, STAGES);
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
    constexpr uint32_t DET = 0x35D828, SLOTS = 0x35D922, PAN = 0x35D91A;
    switch (p) {
    case 0: {
        const uint32_t v = param[VARIATION][0];
        const double K = 65527.48886, NK = -65527.48886;
        uint16_t &lo = lfoReg[1][8], &hi = lfoReg[1][9];   // LFO 8 and 9
        if (v < 0x40) {
            const double a = double(rom::f32(DETUNE_TABLE + 4u * (114u - v))) * K;
            const int32_t w0 = int(a + NK);
            const double b = double(rom::f32(DETUNE_TABLE + 4u * (v - 14u))) * K;
            const int32_t w1 = int(K - b);
            coef[rom::u16(DET + 2)] = uint32_t(int16_t(w0));
            coef[rom::u16(DET)] = uint32_t(int16_t(w0));
            coef[rom::u16(DET + 6)] = uint32_t(int16_t(w1));
            coef[rom::u16(DET + 4)] = uint32_t(int16_t(w1));
            lo = (lo & 0xFF1F) | 0x20;
            lfoReg[3][8] = 0x7FFF;
            hi = (hi & 0xFF1F) | 0x40;
            lfoReg[3][9] = 0;
        } else if (v < 0x41) {
            for (int i = 0; i < 4; i++)
                coef[rom::u16(DET + 2u * uint32_t(i))] = 0;
        } else {
            const double a = double(rom::f32(DETUNE_TABLE + 4u * (114u - v))) * K;
            const int32_t w0 = int(-a + K);
            const double b = double(rom::f32(DETUNE_TABLE + 4u * (v - 14u))) * K;
            const int32_t w1 = int(b + NK);
            coef[rom::u16(DET + 2)] = uint32_t(int16_t(w0));
            coef[rom::u16(DET)] = uint32_t(int16_t(w0));
            coef[rom::u16(DET + 6)] = uint32_t(int16_t(w1));
            coef[rom::u16(DET + 4)] = uint32_t(int16_t(w1));
            lo = (lo & 0xFF1F) | 0x40;
            lfoReg[3][8] = 0;
            hi = (hi & 0xFF1F) | 0x20;
            lfoReg[3][9] = 0x7FFF;
        }
        writeSweWordListSub(4, DET);
        writeSwe25Sub(8);
        writeSwe25Sub(9);
        writeSwe27Sub(8);
        writeSwe27Sub(9);
        break;
    }
    case 1: {
        const uint32_t t = mod_delay(*this, MOD_DELAY_TABLE, 1);
        const int32_t d = int32_t(t + uint32_t(delay[rom::u8(SLOTS)]) + 1);
        for (int i = 1; i <= 4; i++)
            delay[rom::u8(SLOTS + uint32_t(i))] = d;
        writeSweDeSub(5, SLOTS);
        break;
    }
    case 2: {
        const uint32_t t = mod_delay(*this, MOD_DELAY_TABLE, 2);
        const int32_t base = int32_t(((TOP - AREA[4]) >> 1) + 1);
        delay[rom::u8(SLOTS + 5)] = base;
        const int32_t d = int32_t(t + uint32_t(base) + 1);
        for (int i = 6; i <= 9; i++)
            delay[rom::u8(SLOTS + uint32_t(i))] = d;
        writeSweDeSub(5, SLOTS + 5);
        break;
    }
    case 9:
        pan_words_b(*this, PAN, false);
        break;
    case 10: case 11:
        fxDesignFilter(1, 1, 2, P(*this, 10), P(*this, 11), 0, 0x35E75E);
        fxCopyWords3(0x35E75E, 0x35E76A);
        break;
    case 12: case 13:
        fxDesignFilter(1, 2, 3, P(*this, 12), P(*this, 13), 0, 0x35E764);
        fxCopyWords3(0x35E764, 0x35E770);
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
    constexpr uint32_t CUTOFF = 0x35D95E, DEPTH = 0x35D956, SENSE = 0x35D96C, PAN = 0x35DA5A;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1: case 2: {
        const int32_t w = wah_cutoff(uint32_t(P(*this, 2)));
        coef[rom::u16(CUTOFF)] = uint32_t(w);
        const uint32_t v14 = (uint32_t(P(*this, 1)) & 0xFFFF) * uint32_t(32767 - int16_t(w));
        coef[rom::u16(DEPTH)] = v14 / 127;
        writeSweWordListSub(1, CUTOFF);
        writeSweWordListSub(1, DEPTH);
        break;
    }
    case 3:
        wah_sense(*this, 3, SENSE);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, 0x35E72E, 0x35E73A, 0x35E734, 0x35E740);
        break;
    case 9:
        pan_words_b(*this, PAN, true);
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
    constexpr uint32_t ATTACK = 0x35D9B6, CUTOFF = 0x35D95E, SENSE = 0x35D96C, PAN = 0x35DA5A;
    constexpr uint32_t MIRROR = 0x35DA0C;
    switch (p) {
    case 0:
        coef[rom::u16(ATTACK)] = uint32_t(P(*this, 0) * 258);
        writeSweWordListSub(1, ATTACK);
        [[fallthrough]];   // the firmware runs on into the cutoff word
    case 1:
        coef[rom::u16(CUTOFF)] = uint32_t(wah_cutoff(uint32_t(P(*this, 1))));
        writeSweWordListSub(1, CUTOFF);
        break;
    case 2:
        wah_sense(*this, 2, SENSE);
        break;
    case 5: case 6: case 7: case 8:
        filters_b(*this, p, 0x35E72E, 0x35E73A, 0x35E734, 0x35E740);
        break;
    case 9:
        pan_words_b(*this, PAN, true);
        break;
    case 15: {
        int32_t v = int8_t(param[VARIATION][15]) - 0x34;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        fxStoreMirror3(v, MIRROR);
        break;
    }
    case 255:
        hold = true;
        fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35E722);
        fxDesignFilter(1, 0, 0, 54, 64, 0, 0x35E728);
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
    constexpr uint32_t A = 0x35EA12, B = 0x35EA18;
    switch (p) {
    case 0: case 5:
        fxDesignFilter(2, 1, 6, P(*this, 5), int8_t(param[VARIATION][0]), 0, 0x35E8EA);
        fxCopyWords5(0x35E8EA, 0x35E908);
        break;
    case 1: case 2: case 3:
        fxDesignFilter(2, 1, 10, P(*this, 1), P(*this, 2), P(*this, 3), 0x35E8F4);
        fxCopyWords5(0x35E8F4, 0x35E912);
        break;
    case 4: case 6:
        fxDesignFilter(2, 4, 7, P(*this, 6), P(*this, 4), 0, 0x35E8FE);
        fxCopyWords5(0x35E8FE, 0x35E91C);
        break;
    case 14: {
        const uint32_t x = uint32_t(fxMulTypeTableD(8192));
        if (P(*this, 14) == 0) {
            coef[rom::u16(A + 2)] = 8192;
            coef[rom::u16(A)] = 8192;
            coef[rom::u16(A + 4)] = 0x4000;
            coef[rom::u16(B + 2)] = x;
            coef[rom::u16(B)] = x;
            coef[rom::u16(B + 4)] = 0;
        } else {
            coef[rom::u16(A)] = 0x4000;
            coef[rom::u16(A + 2)] = 0;
            coef[rom::u16(A + 4)] = 0x4000;
            coef[rom::u16(B + 4)] = x;
            coef[rom::u16(B)] = x;
            coef[rom::u16(B + 2)] = 0;
        }
        writeSweWordListSub(2, A);
        writeSweWordListSub(2, B + 2);
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
    constexpr uint32_t DRIVE = 0x35DA18, MIX = 0x35DA6C;
    switch (p) {
    case 0:
        fxDesignFilter(2, 1, 5, P(*this, 0), 64, 7, 0x35E8D6);
        fxCopyWords5(0x35E8D6, 0x35E8E0);
        fxDesignFilter(1, 1, 1, P(*this, 0), 64, 0, 0x35E710);
        fxCopyWords3(0x35E710, 0x35E716);
        break;
    case 1: {
        const uint32_t w = uint32_t(P(*this, 1) * 0x102);
        coef[rom::u16(DRIVE + 2)] = w;
        coef[rom::u16(DRIVE)] = w;
        writeSweWordListSub(2, DRIVE);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(fxMulTypeTableD(int32_t((uint32_t(P(*this, 2)) & 0xFFFF) * 0x81) >> 1));
        coef[rom::u16(MIX + 4)] = x;
        coef[rom::u16(MIX)] = x;
        writeSweWordListSub(1, MIX);
        writeSweWordListSub(1, MIX + 4);
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
    constexpr uint32_t OUT = 0x35DA6A;
    const int32_t i = int(double(rom::f32(FX_LEVEL_CURVE + 4u * (uint32_t(P(h, p)) & 0xFFFF))) * 8192.0);
    const uint32_t x = uint32_t(h.fxMulTypeTableD(int16_t(i)));
    h.coef[rom::u16(OUT + 6)] = x;
    h.coef[rom::u16(OUT + 2)] = x;
    h.coef[rom::u16(OUT + 4)] = 0;
    h.coef[rom::u16(OUT)] = 0;
    h.writeSweWordListSub(4, OUT);
}
void dynamics_filters(Host& h)
{
    h.fxDesignFilter(2, 1, 1, 0, 64, 0, 0x35E8D6);
    h.fxCopyWords5(0x35E8D6, 0x35E8E0);
    h.fxDesignFilter(1, 1, 11, 0, 64, 0, 0x35E710);
    h.fxCopyWords3(0x35E710, 0x35E716);
}
}  // namespace

// 0x39FED4 fxVarCompressor (type 16)
void Host::fxVarCompressor(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: fxStoreMirrorHalf(P(*this, 0), 0x35E71C); break;
    case 1: fxWriteComplementPair(P(*this, 1), 0x35D9D8); break;
    case 2: fxWriteWord(P(*this, 2), 0x35D9E2); break;
    case 3: fxWriteRow8(P(*this, 3), 0x35D9F4); break;
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
    case 0: fxStoreMirrorX32(P(*this, 0), 0x35E71C); break;
    case 1: fxWriteComplementPair(P(*this, 1), 0x35D9D8); break;
    case 2: fxWriteNegated(P(*this, 2), 0x35D9E2); break;
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
// the distortion group's pan words (a, x, a, x layout over 0x35DA52)
void dist_pan_words(Host& h)
{
    constexpr uint32_t PAN = 0x35DA52;
    const uint32_t g = uint32_t(rom::s16(rom::FX_PAN_LAW + 2) >> 2);
    h.coef[rom::u16(PAN + 4)] = g;
    h.coef[rom::u16(PAN)] = g;
    const uint32_t x = uint32_t(h.fxMulTypeTableD(rom::s16(0x371ACE) >> 2));
    h.coef[rom::u16(PAN + 6)] = x;
    h.coef[rom::u16(PAN + 2)] = x;
    h.writeSweWordListSub(4, PAN);
}
}  // namespace

// 0x39F520 fxVarDistortion: Distortion (17) and Overdrive (18)
void Host::fxVarDistortion(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 4:
        fxStoreScaled(P(*this, 0), P(*this, 4), 0x35D9C8);
        break;
    case 1: case 2:
        fxDesignFilter(2, 4, 2, P(*this, 1), P(*this, 2), 0, 0x35E8CC);
        break;
    case 3:
        fxDesignFilter(2, 1, 4, P(*this, 3), 64, 7, 0x35E8B8);
        break;
    case 6: case 7: case 8:
        fxDesignFilter(2, 1, 10, P(*this, 6), P(*this, 7), P(*this, 8), 0x35E8C2);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        fxFillBand(P(*this, 10), 0x35E9FA);
        break;
    case 255: {
        hold = true;
        int f;
        if (code[VARIATION].lsb == 1) {
            fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35E6FE);
            f = 0x36;
        } else {
            fxDesignFilter(1, 1, 1, 8, 64, 0, 0x35E6FE);
            f = 0x2C;
        }
        fxDesignFilter(1, 1, 0, f, 64, 0, 0x35E704);
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
        fxStoreScaled(P(*this, 0), P(*this, 3), 0x35D9C8);
        break;
    case 1: {
        const uint32_t row = AMP_ROWS + uint32_t(int8_t((param[VARIATION][1] & 0xFF) * 14));
        fxDesignFilter(1, 1, rom::s8(row), rom::s8(row + 2), rom::s8(row + 1), 0, 0x35E6FE);
        fxDesignFilter(1, 1, rom::s8(row + 3), rom::s8(row + 5), rom::s8(row + 4), 0, 0x35E704);
        fxDesignFilter(2, 1, rom::s8(row + 6), rom::s8(row + 8), rom::s8(row + 7), 0, 0x35E8C2);
        fxDesignFilter(2, 4, rom::s8(row + 9), rom::s8(row + 11), rom::s8(row + 10), rom::s8(row + 12), 0x35E8CC);
        fxVarAmpSim(2);
        break;
    }
    case 2:
        if (P(*this, 1) == 1)
            fxDesignFilter(2, 1, 4, P(*this, 2), 0x38, 0xF, 0x35E8B8);
        else
            fxDesignFilter(2, 1, 4, P(*this, 2), 0x40, P(*this, 1) == 2 ? 0xF : 7, 0x35E8B8);
        break;
    case 9:
        dist_pan_words(*this);
        break;
    case 10:
        fxFillBand(P(*this, 10), 0x35E9FA);
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
    constexpr uint32_t SLOT = 0x35DB6A, LEVEL = 0x35E7A2, PAN = 0x35DF80, ROW_STEPS = 0x35DC74, ROW_SLOTS = 0x35DBEC;
    switch (p) {
    case 0: {
        const uint32_t t = mod_delay(*this, KARAOKE_DELAY_TABLE, 0);
        delay[rom::u8(SLOT)] = int32_t(uint32_t(delay[rom::u8(SLOT - 1)]) + t);
        writeSweDeSub(1, SLOT);
        break;
    }
    case 1: {
        const uint32_t w = uint32_t((P(*this, 1) - 64) * 250);
        coef[rom::u16(LEVEL + 2)] = w;
        coef[rom::u16(LEVEL)] = w;
        writeSweWordListSub(2, LEVEL);
        break;
    }
    case 2:
        fxDesignFilter(1, 1, 1, P(*this, 2), 64, 0, 0x35E796);
        break;
    case 3:
        fxDesignFilter(1, 1, 0, P(*this, 3), 64, 0, 0x35E79C);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::s16(rom::FX_ER_REV + 2) >> 2);
        coef[rom::u16(PAN + 4)] = g;
        coef[rom::u16(PAN)] = g;
        const uint32_t x = uint32_t(fxMulTypeTableD(rom::s16(rom::FX_ER_REV + 254) >> 2));
        coef[rom::u16(PAN + 6)] = x;
        coef[rom::u16(PAN + 2)] = x;
        writeSweWordListSub(4, PAN);
        break;
    }
    case 10:
        fxScaleRow(P(*this, 10) > 3 ? 3 : P(*this, 10), 5, 3, 4, 0, ROW_STEPS, ROW_SLOTS);   // a hidden parameter; the table has four rows
        writeSweWordListSub(16, ROW_STEPS);
        writeSweDeSub(8, ROW_SLOTS);
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
    const uint32_t sel = rom::FX_COMB_SELECT + uint32_t(int8_t(6 * code[VARIATION].lsb));
    constexpr uint32_t COMBS = 0x35DD70, ER_IDS = 0x35DD6A, PRE = 0x35DD94, HPF = 0x35DD52, LPF = 0x35DD58;
    constexpr uint32_t PAN = 0x35DDAC, ROW_STEPS = 0x35DD84, ROW_SLOTS = 0x35DD80, W = 0x35DD96, WIDTH = 0x35DDAA;
    switch (p) {
    case 0: case 13:
        fxDesignCombs(P(*this, 0), P(*this, 13), 2, rom::s16(sel + 2), COMBS);
        writeSweWordListSub(8, COMBS);
        break;
    case 1: {
        fxAddScaledPairs2(P(*this, 1), rom::s16(sel + 2), ER_IDS);
        const uint8_t ids[2] = {uint8_t(rom::u8(ER_IDS + 2)), uint8_t(rom::u8(ER_IDS + 5))};
        writeSweDeSubIds(2, ids);
        fxVarReverb(12);
        break;
    }
    case 2: {
        const uint32_t t = uint32_t(fxMsToSamples(rom::s16(rom::FX_DELAY_WORD + 2u * (uint32_t(P(*this, 2)) & 0xFFFF))));
        delay[rom::u8(PRE + 1)] = int32_t((uint32_t(delay[rom::u8(PRE)]) + t) & 0xFFFF);
        writeSweDeSub(1, PRE + 1);
        break;
    }
    case 3:
        fxDesignFilter(1, 0, 1, P(*this, 3), 64, 0, HPF);
        break;
    case 4:
        fxDesignFilter(1, 1, 0, P(*this, 4), 64, 0, LPF);
        break;
    case 9: {
        const uint32_t g = uint32_t(rom::s16(rom::FX_ER_REV + 2) >> 2);
        coef[rom::u16(PAN + 2)] = g;
        coef[rom::u16(PAN)] = g;
        const uint32_t x = uint32_t(fxMulTypeTableD(rom::s16(rom::FX_ER_REV + 254) >> 2));
        coef[rom::u16(PAN + 6)] = x;
        coef[rom::u16(PAN + 4)] = x;
        writeSweWordListSub(4, PAN);
        break;
    }
    case 11:
        fxScaleRow(P(*this, 11), P(*this, 1), rom::s16(sel + 4), 2, 2, ROW_STEPS, ROW_SLOTS);
        writeSweWordListSub(8, ROW_STEPS);
        break;
    case 12: {
        const int32_t rowword = rom::s16(ER_ROW_WORDS + 0x24u * (uint32_t(rom::s16(sel)) & 0xFFFF));
        const uint32_t p12 = uint32_t(P(*this, 12)) & 0xFFFF;
        const int32_t v3 = rowword * rom::s16(rom::FX_ER_REV + 2u * (128u - p12));
        const uint32_t w = uint32_t(int16_t(uint32_t(v3) >> 16));
        coef[rom::u16(W)] = w;
        for (int i = 1; i <= 5; i++)
            coef[rom::u16(W + 2u * uint32_t(i))] = 0;
        const int32_t k = P(*this, 1);
        const int idx = k == 0 ? 1 : k == 1 ? 2 : k == 2 ? 3 : k == 3 ? 4 : 5;
        coef[rom::u16(W + 2u * uint32_t(idx))] = w;
        const int32_t er = rom::s16(rom::FX_ER_REV + 2u * p12);
        const int32_t w2 = ((3 * er) << 13) >> 15;   // shll8, shll2, shll2, then shll in the jsr delay slot, asr15
        for (int i = 6; i <= 9; i++)
            coef[rom::u16(W + 2u * uint32_t(i))] = uint32_t(w2);
        writeSweWordListSub(10, W);
        break;
    }
    case 14:
        coef[rom::u16(WIDTH)] = uint32_t(fxBipolar500(P(*this, 14)));
        writeSweWordListSub(1, WIDTH);
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

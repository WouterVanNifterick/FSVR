// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 1: the modulation group, the delays, the early reflections and
// the cross delay.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ram;
using namespace ins;

namespace {
// the modulation group's lists: words at 0x35D830 (15), slots at 0x35D84E (10)
constexpr uint32_t MW = 0x35D830, MS = 0x35D84E;
inline uint32_t mw(int i) { return MW + 2u * uint32_t(i); }
inline uint32_t msl(int i) { return MS + uint32_t(i); }
inline uint32_t mslot(int i) { return rom::u8(msl(i)); }
constexpr uint32_t MOD_LEVEL = 0x35D7C4;              // u16 x 6 (fxStoreUdiv)
constexpr uint32_t ROT_LEVEL = 0x35D7D0;              // u16 x 8 (rotary / tremolo / auto pan)
constexpr uint32_t ROT_WORDS = 0x35D858;              // the rotary group's Dry/Wet words
constexpr uint32_t ROT_PAN = 0x35D860;                // the 2-way rotary / ambience pan words
constexpr uint32_t TREM_WORDS = 0x35D86C, TREM_SLOTS = 0x35D870;
constexpr uint32_t PH_FB = 0x35D95A, PH_DEPTH = 0x35D952, PH_WIDTH1 = 0x35D974, PH_WIDTH2 = 0x35D970;
constexpr uint32_t PH_STAGES = 0x35D980, PH_DIR = 0x35D996, PH_WET = 0x35DA40, PH_OFFSET = 0x35D966;
}  // namespace

// 0x3A6FD8 fxInsChorus: Chorus and Celeste (types 1, 2; msb 0 lsb 2-9)
void Host::fxInsChorus(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = int(PLOU(*this, 1) << 1);
        fxStoreUdiv(x, MOD_LEVEL);
        fxStoreUdiv(x, MOD_LEVEL + 4);
        fxStoreUdiv(x, MOD_LEVEL + 8);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(fxBipolar376(P(*this, 2)));
        set_coef(rom::u16(mw(9)), x);
        set_coef(rom::u16(mw(8)), x);
        writeSweWordListSub(2, mw(8));
        break;
    }
    case 3: case 14: {
        const int32_t t = ms(*this, MOD_DELAY_TABLE, 3);
        int32_t base = offs(mslot(0));
        const int32_t d = t + base;
        set_offs(mslot(2), d + 1);
        set_offs(mslot(1), d + 1);
        set_offs(mslot(4), d + 65);
        set_offs(mslot(3), d + 65);
        if (P(*this, 14) == 0) {
            set_coef(rom::u16(mw(12)), 0x4000);
            set_coef(rom::u16(mw(13)), 0x4000);
            set_offs(mslot(5), 0);
        } else {
            set_coef(rom::u16(mw(12)), 0x7FFF);
            set_coef(rom::u16(mw(13)), 0);
            base = int32_t(half_area(*this));
            set_offs(mslot(5), base);
        }
        const int32_t d2 = t + base;
        set_offs(mslot(7), d2 + 1);
        set_offs(mslot(6), d2 + 1);
        set_offs(mslot(9), d2 + 65);
        set_offs(mslot(8), d2 + 65);
        writeSweWordListSub(2, mw(12));
        writeSweDeSub(4, msl(1));
        writeSweDeSub(5, msl(5));
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MW, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, MW);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10})
            fxInsChorus(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A73E8 fxInsFlanger (type 3; msb 0 lsb 10-12)
void Host::fxInsFlanger(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        fxStoreUdiv(P(*this, 1), MOD_LEVEL);
        fxStoreUdiv(P(*this, 1), MOD_LEVEL + 4);
        break;
    case 2: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 2)));
        set_coef(rom::u16(mw(9)), x);
        set_coef(rom::u16(mw(8)), x);
        writeSweWordListSub(2, mw(8));
        break;
    }
    case 3: {
        const int32_t t = ms(*this, MOD_DELAY_TABLE, 3);
        const int32_t d = offs(mslot(0)) + t + 1;
        set_offs(mslot(2), d);
        set_offs(mslot(1), d);
        const int32_t d2 = t + offs(mslot(5)) + 1;
        set_offs(mslot(7), d2);
        set_offs(mslot(6), d2);
        writeSweWordListSub(2, mw(12));
        writeSweDeSub(2, msl(1));
        writeSweDeSub(2, msl(6));
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MW, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, MW);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 13:
        fxStoreBipolar500(P(*this, 13), 0x35D732);
        break;
    case 255:
        set_coef(rom::u16(mw(12)), 0x7FFF);
        set_coef(rom::u16(mw(13)), 0);
        set_offs(mslot(5), int32_t(half_area(*this)));
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10, 13})
            fxInsFlanger(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A77C8 fxInsSymphonic (type 4; msb 0 lsb 13)
void Host::fxInsSymphonic(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = int(((5u * PU(*this, 1)) >> 2) >> 1);
        fxStoreUdiv(x, MOD_LEVEL);
        fxStoreUdiv(x, MOD_LEVEL + 4);
        fxStoreUdiv(x, MOD_LEVEL + 8);
        const int y = int(((3u * PU(*this, 1)) >> 2) >> 1);
        fxStoreUdiv(y, MOD_LEVEL + 2);
        fxStoreUdiv(y, MOD_LEVEL + 6);
        fxStoreUdiv(y, MOD_LEVEL + 10);
        break;
    }
    case 2: {
        const int32_t d = ms(*this, MOD_DELAY_TABLE, 2) + offs(mslot(0));
        set_offs(mslot(2), d + 2);
        set_offs(mslot(1), d + 2);
        set_offs(mslot(4), d + 66);
        set_offs(mslot(3), d + 66);
        set_offs(mslot(7), d + 130);
        set_offs(mslot(6), d + 130);
        writeSweDeSub(4, msl(1));
        writeSweDeSub(2, msl(6));
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C);
        break;
    case 9:
        wet_words(*this, MW, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, MW);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_MOD, EQ_MOD_C);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 5, 7, 9, 10})
            fxInsSymphonic(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A8B30 fxInsRotarySp (type 9; msb 1 lsb 1)
void Host::fxInsRotarySp(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1: {
        const int x = PU(*this, 1) < 32 ? (PLO(*this, 1) << 1) : 64;
        fxStoreNegMul(x, ROT_LEVEL);
        fxStoreNegMul(x, ROT_LEVEL + 4);
        fxStoreNegMul(x, ROT_LEVEL + 8);
        const int y = int(PLOU(*this, 1) >> 2);
        fxStoreUdiv(y, ROT_LEVEL + 2);
        fxStoreUdiv(y, ROT_LEVEL + 6);
        fxStoreUdiv(y, ROT_LEVEL + 10);
        fxStoreUdiv(y, ROT_LEVEL + 14);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 9:
        wet_words(*this, ROT_WORDS, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, ROT_WORDS);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_ROT, EQ_ROT_C);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 5, 7, 9, 10})
            fxInsRotarySp(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A8DC8 fxIns2WayRotary (type 10; msb 1 lsb 2); no parameter 9 case
void Host::fxIns2WayRotary(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        fxStoreNegMul(P(*this, 1), ROT_LEVEL);
        fxStoreNegMul(P(*this, 1), ROT_LEVEL + 8);
        fxStoreUdiv(int((PLOU(*this, 1) >> 2) >> 1), ROT_LEVEL + 2);
        fxStoreUdiv(int((PLOU(*this, 1) >> 2) >> 1), ROT_LEVEL + 10);
        break;
    case 2:
        fxStoreNegMul(P(*this, 2), ROT_LEVEL + 4);
        fxStoreNegMul(P(*this, 2), ROT_LEVEL + 12);
        fxStoreUdiv(int((PLOU(*this, 2) >> 2) >> 2), ROT_LEVEL + 6);
        fxStoreUdiv(int((PLOU(*this, 2) >> 2) >> 2), ROT_LEVEL + 14);
        break;
    case 3: {
        const uint32_t a = uint32_t(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(PU(*this, 3))))));
        set_coef(rom::u16(ROT_PAN + 6), a);
        set_coef(rom::u16(ROT_PAN), a);
        const uint32_t b = uint32_t(rom::s16(PAN_LAW + 2u * PU(*this, 3)));
        set_coef(rom::u16(ROT_PAN + 4), b);
        set_coef(rom::u16(ROT_PAN + 2), b);
        writeSweWordListSub(4, ROT_PAN);
        break;
    }
    case 5: case 6:
        fxDesignFilter(1, 2, 2, P(*this, 5), P(*this, 6), 0, HPF_ROT);
        break;
    case 7: case 8:
        fxDesignFilter(1, 2, 3, P(*this, 7), P(*this, 8), 0, HPF_ROT_C);
        break;
    case 10:
        fxDesignFilter(1, 1, 0, P(*this, 10), 64, 7, LPF_ROT);
        fxDesignFilter(1, 1, 1, P(*this, 10), 64, 7, LPF_ROT_C);
        break;
    case 11:
        fxStoreBipolar500(PLO(*this, 11) + 64, 0x35D73C);
        fxStoreBipolar500(PLO(*this, 11) + 4, 0x35D73A);
        break;
    case 255: {
        const uint32_t x = uint32_t(fxMulTypeTableB(8192));
        set_coef(rom::u16(ROT_WORDS + 6), x);
        set_coef(rom::u16(ROT_WORDS + 4), x);
        ram.w8(SWE_SUB_HOLD, 1);
        fxDesignFilter(2, 1, 5, 8, 64, 7, EQ_ROT);
        fxDesignFilter(2, 1, 10, 45, 70, 10, EQ_ROT_C);
        for (int q : {0, 1, 2, 3, 5, 7, 9, 10, 11})
            fxIns2WayRotary(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    }
}

// 0x3A90AE fxInsTremolo (type 11; msb 1 lsb 3)
void Host::fxInsTremolo(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        fxStoreNegMul(P(*this, 1), ROT_LEVEL);
        fxStoreNegMul(P(*this, 1), ROT_LEVEL + 4);
        break;
    case 2:
        fxStoreUdiv(int(PLOU(*this, 2) >> 2), ROT_LEVEL + 2);
        fxStoreUdiv(int(PLOU(*this, 2) >> 2), ROT_LEVEL + 6);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 10: case 11: case 12:
        eq_band(*this, EQ_ROT, EQ_ROT_C);
        break;
    case 13:
        fxStoreBipolar500(P(*this, 13), 0x35D73A);
        break;
    case 14: {
        auto id = [&](int i) { return rom::u8(TREM_SLOTS + uint32_t(i)); };
        if (P(*this, 14) == 0) {
            set_coef(rom::u16(TREM_WORDS), 0x4000);
            set_coef(rom::u16(TREM_WORDS + 2), 0x4000);
            set_offs(id(5), 0);
            set_offs(id(6), offs(id(0)) + 1);
            set_offs(id(7), offs(id(0)) + 1);
        } else {
            set_coef(rom::u16(TREM_WORDS), 0x7FFF);
            set_coef(rom::u16(TREM_WORDS + 2), 0);
            set_offs(id(5), 1024);
            set_offs(id(6), 1025);
            set_offs(id(7), 1025);
        }
        writeSweWordListSub(2, TREM_WORDS);
        writeSweDeSub(3, TREM_SLOTS + 5);
        break;
    }
    case 255: {
        const uint32_t x = uint32_t(fxMulTypeTableB(8192));
        set_coef(rom::u16(ROT_WORDS + 6), x);
        set_coef(rom::u16(ROT_WORDS + 4), x);
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 5, 7, 10, 13, 14})
            fxInsTremolo(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    }
}

// 0x3A9444 fxInsAutoPan (type 12; msb 1 lsb 4); parameter 3 re-runs 1 and 2; no parameter 9 case
void Host::fxInsAutoPan(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 12);
        break;
    case 1:
        fxStoreNegMul(P(*this, 1), ROT_LEVEL + 8);
        fxStoreNegMul(P(*this, 1), ROT_LEVEL + 12);
        break;
    case 2: {
        const int32_t m = P(*this, 3);
        const int y = (m == 3 || m == 4) ? int(PLOU(*this, 2) >> 2) : 0;
        fxStoreUdiv(y, ROT_LEVEL + 10);
        fxStoreUdiv(y, ROT_LEVEL + 14);
        break;
    }
    case 3: {
        const uint32_t m = PU(*this, 3);
        if (m < 5) {
            static const int A[5] = {64, 84, 64, 94, 64};
            static const int B[5] = {124, 64, 84, 64, 94};
            fxStoreBipolar500(A[m], 0x35D73C);
            fxStoreBipolar500(B[m], 0x35D73E);
            fxStoreModePair(1, 0x35D77C);
            fxStoreModePair(1, 0x35D780);
        } else {
            fxStoreBipolar500(64, 0x35D73C);
            fxStoreBipolar500(124, 0x35D73E);
            fxStoreModePair(2, 0x35D77C);
            fxStoreModePair(2, 0x35D780);
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
        const uint32_t x = uint32_t(fxMulTypeTableB(8192));
        set_coef(rom::u16(ROT_WORDS + 6), x);
        set_coef(rom::u16(ROT_WORDS + 4), x);
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 3, 5, 7, 9, 10})
            fxInsAutoPan(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    }
}

// 0x3A9734 fxInsAmbience (type 13; msb 1 lsb 5)
void Host::fxInsAmbience(int p)
{
    p &= 0xFF;
    constexpr uint32_t SL = 0x35D870;
    switch (p) {
    case 0: {
        const uint32_t t = uint32_t(ms(*this, MOD_DELAY_TABLE, 0)) & 0xFFFF;
        const int32_t d = int32_t(uint32_t(offs(rom::u8(SL))) + t + 1);
        set_offs(rom::u8(SL + 7), d);
        set_offs(rom::u8(SL + 6), d);
        set_offs(rom::u8(SL + 2), d);
        set_offs(rom::u8(SL + 1), d);
        writeSweDeSub(2, SL + 1);
        writeSweDeSub(2, SL + 6);
        break;
    }
    case 1:
        if (PU(*this, 1) == 1) {
            set_coef(rom::u16(ROT_PAN), 0x8000);
            set_coef(rom::u16(ROT_PAN + 2), 0x7FFF);
        } else {
            set_coef(rom::u16(ROT_PAN), 0x7FFF);
            set_coef(rom::u16(ROT_PAN + 2), 0x8000);
        }
        writeSweWordListSub(2, ROT_PAN);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_ROT, LPF_ROT_C, HPF_ROT, HPF_ROT_C);
        break;
    case 9:
        wet_words(*this, ROT_WORDS, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, ROT_WORDS);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 5, 7, 9})
            fxInsAmbience(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3AADB4 fxInsPhaser1 (type 5; msb 2 lsb 1); 10 (stages) re-runs 11, 11 (direction) re-runs 9
void Host::fxInsPhaser1(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const uint32_t w = uint32_t(P(*this, 2) * 258);
        set_coef(rom::u16(PH_FB + 2), w);
        set_coef(rom::u16(PH_FB), w);
        const int32_t d = sh::sdiv3000(int32_t(PU(*this, 1) * uint32_t(32767 - sh::s16(w))), 127);
        set_coef(rom::u16(PH_DEPTH + 2), uint32_t(d));
        set_coef(rom::u16(PH_DEPTH), uint32_t(d));
        writeSweWordListSub(2, PH_FB);
        writeSweWordListSub(2, PH_DEPTH);
        break;
    }
    case 3:
        set_coef(rom::u16(PH_WIDTH1), uint32_t(fxBipolar500(P(*this, 3))));
        writeSweWordListSub(1, PH_WIDTH1);
        break;
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_PH, LPF_PH_C, HPF_PH, HPF_PH_C);
        break;
    case 9: {
        const uint32_t wet = PU(*this, 9);
        const uint32_t g = uint32_t(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> 2);
        set_coef(rom::u16(PH_WET + 4), g);
        set_coef(rom::u16(PH_WET), g);
        int32_t x = fxMulTypeTableB(rom::s16(PAN_LAW + 2u * wet) >> 2);
        set_coef(rom::u16(PH_WET + 2), uint32_t(x));
        if (P(*this, 11) != 0)
            x = -sh::s16(uint32_t(x));
        set_coef(rom::u16(PH_WET + 6), uint32_t(x));
        writeSweWordListSub(4, PH_WET);
        break;
    }
    case 10: {
        static const int PAIRS[9][2] = {{0, 5}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {1, 9}, {2, 9}, {3, 9}, {4, 9}};
        const uint32_t n = PU(*this, 10);
        const int k = n < 5 ? 0 : n < 12 ? int(n) - 4 : 8;
        for (int i = 0; i < 10; i++)
            set_coef(rom::u16(PH_STAGES + 2u * uint32_t(i)), (i == PAIRS[k][0] || i == PAIRS[k][1]) ? 0x4000 : 0);
        writeSweWordListSub(10, PH_STAGES);
        fxInsPhaser1(11);
        break;
    }
    case 11: {
        uint32_t w[3] = {0, 0, 0};
        if (P(*this, 11) == 0) w[0] = 0x4000;
        else if (PU(*this, 10) > 8) w[2] = 0x4000;
        else w[1] = 0x4000;
        for (int i = 0; i < 3; i++)
            set_coef(rom::u16(PH_DIR + 2u * uint32_t(i)), w[i]);
        writeSweWordListSub(3, PH_DIR);
        fxInsPhaser1(9);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 3, 5, 7, 10})
            fxInsPhaser1(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3AB4E4 fxInsPhaser2 (type 6; msb 2 lsb 2)
void Host::fxInsPhaser2(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const uint32_t w = uint32_t(P(*this, 2) * 258);
        set_coef(rom::u16(PH_FB), w);
        set_coef(rom::u16(PH_DEPTH), uint32_t(sh::sdiv3000(int32_t(PU(*this, 1) * uint32_t(32767 - sh::s16(w))), 127)));
        const int32_t v10 = sh::muls_w(242, P(*this, 2));
        set_coef(rom::u16(PH_FB + 2), uint32_t(v10));
        set_coef(rom::u16(PH_DEPTH + 2), uint32_t(sh::sdiv3000(int32_t(PU(*this, 1) * uint32_t(32767 - sh::s16(uint32_t(v10)))), 127)));
        writeSweWordListSub(2, PH_FB);
        writeSweWordListSub(2, PH_DEPTH);
        break;
    }
    case 3: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 3)));
        set_coef(rom::u16(PH_WIDTH2 + 2), x);
        set_coef(rom::u16(PH_WIDTH2), x);
        writeSweWordListSub(2, PH_WIDTH2);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, 5, LPF_PH, LPF_PH_C, HPF_PH, HPF_PH_C);
        break;
    case 9:
        wet_words(*this, PH_WET, PAN_LAW, PU(*this, 9), GXGX);
        writeSweWordListSub(4, PH_WET);
        break;
    case 10: {
        const uint32_t n = PU(*this, 10);
        int a, b;
        if (n <= 3) { a = 1; b = 6; }
        else if (n == 4) { a = 2; b = 7; }
        else if (n == 5) { a = 3; b = 8; }
        else { a = 4; b = 9; }
        for (int i = 0; i < 10; i++)
            set_coef(rom::u16(PH_STAGES + 2u * uint32_t(i)), (i == a || i == b) ? 0x4000 : 0);
        writeSweWordListSub(10, PH_STAGES);
        break;
    }
    case 12:
        fxStoreBipolar376(P(*this, 12), PH_OFFSET);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 3, 5, 7, 9, 10, 12})
            fxInsPhaser2(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// ---- the delays (msb 0 lsb 14-17) ----
namespace {
// offs[S[dst]] = offs[S[ref]] + samples(p), then the slot
void ins_delay_slot(Host& h, int dst, int ref, int p)
{
    h.set_offs(mslot(dst), h.offs(mslot(ref)) + h.fxMsToSamples(P(h, p)));
    h.writeSweDeSub(1, msl(dst));
}
void ins_delay_filters(Host& h, int p) { filters(h, p, 12, LPF_MOD, LPF_MOD_C, HPF_MOD, HPF_MOD_C); }
void ins_delay_wet(Host& h)
{
    wet_words(h, MW, ER_REV, PU(h, 9), GGXX);
    h.writeSweWordListSub(4, MW);
}
}  // namespace

// 0x3A7AFE fxInsDelayLR (type 33; msb 0 lsb 14): the three-tap layout
void Host::fxInsDelayLR(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: ins_delay_slot(*this, 3, 0, 0); break;
    case 1: ins_delay_slot(*this, 6, 0, 1); break;
    case 2: ins_delay_slot(*this, 8, 0, 2); break;
    case 3: ins_delay_slot(*this, 1, 0, 3); break;
    case 4:
        set_coef(rom::u16(mw(8)), uint32_t(fxBipolar500(P(*this, 4))));
        writeSweWordListSub(1, mw(8));
        break;
    case 5: {
        const int32_t x = sh::dtoi(sh::dmul(sh::ftod(rom::f32(LEVEL_CURVE + 4u * PU(*this, 5))), 32767.0));
        set_coef(rom::u16(mw(7)), uint32_t(sh::s16(uint32_t(x))));
        set_coef(rom::u16(mw(5)), uint32_t(sh::s16(uint32_t(x))));
        writeSweWordListSub(1, mw(5));
        writeSweWordListSub(1, mw(7));
        break;
    }
    case 6:
        fxFloatCurveB(1, P(*this, 6), 64, 0x35E53A);
        break;
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 6, 9, 12, 14})
            fxInsDelayLR(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A7EA2 fxInsDelayLCR (type 34; msb 0 lsb 15)
void Host::fxInsDelayLCR(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: ins_delay_slot(*this, 3, 0, 0); break;
    case 1: ins_delay_slot(*this, 8, 0, 1); break;
    case 2: ins_delay_slot(*this, 1, 0, 2); break;
    case 3: ins_delay_slot(*this, 6, 0, 3); break;
    case 4: {
        const int32_t x = fxBipolar500(P(*this, 4));
        set_coef(rom::u16(mw(10)), uint32_t(sh::s16(uint32_t(x)) >> 1));
        set_coef(rom::u16(mw(8)), uint32_t(sh::s16(uint32_t(x)) >> 1));
        writeSweWordListSub(1, mw(8));
        writeSweWordListSub(1, mw(10));
        break;
    }
    case 5:
        fxFloatCurveB(1, P(*this, 5), 64, 0x35E53A);
        fxCopyWords2(0x35E53A, 0x35E540);
        break;
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 9, 12, 14})
            fxInsDelayLCR(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A8210 fxInsEcho (type 35; msb 0 lsb 16)
void Host::fxInsEcho(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: ins_delay_slot(*this, 1, 0, 0); break;
    case 5: ins_delay_slot(*this, 3, 0, 5); break;
    case 2: ins_delay_slot(*this, 6, 5, 2); break;
    case 6: ins_delay_slot(*this, 8, 5, 6); break;
    case 1:
        set_coef(rom::u16(mw(8)), uint32_t(fxBipolar500(P(*this, 1))));
        writeSweWordListSub(1, mw(8));
        break;
    case 3:
        set_coef(rom::u16(mw(9)), uint32_t(fxBipolar500(P(*this, 3))));
        writeSweWordListSub(1, mw(9));
        break;
    case 4:
        fxFloatCurveB(1, P(*this, 4), 64, 0x35E53A);
        fxCopyWords2(0x35E53A, 0x35E540);
        break;
    case 7: {
        const int32_t i = sh::ftoi(sh::fmul(32767.0f, rom::f32(LEVEL_CURVE + 4u * PU(*this, 7))));
        const uint32_t x = uint32_t(fxMulTypeTableB(sh::s16(uint32_t(i))));
        set_coef(rom::u16(mw(5)), x);
        set_coef(rom::u16(mw(4)), x);
        writeSweWordListSub(2, mw(4));
        break;
    }
    case 9:
        ins_delay_wet(*this);
        break;
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case 255:
        set_offs(mslot(5), int32_t(half_area(*this)));
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 14})
            fxInsEcho(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A8628 fxInsEr1 (type 36 "ER 1"; msb 0 lsb 17): the cross-fed pair with routing mode
// parameter 3; 3 re-runs 2
void Host::fxInsEr1(int p)
{
    p &= 0xFF;
    const int32_t mode = P(*this, 3);
    switch (p) {
    case 0: {
        const int ref = mode == 1 ? 5 : 0;
        set_offs(mslot(6), offs(mslot(ref)) + fxMsToSamples(P(*this, 0)));
        writeSweDeSub(1, msl(6));
        break;
    }
    case 1: {
        const int ref = mode == 1 ? 0 : 5;
        set_offs(mslot(1), offs(mslot(ref)) + fxMsToSamples(P(*this, 1)));
        writeSweDeSub(1, msl(1));
        break;
    }
    case 2:
        if (mode == 1) {
            set_coef(rom::u16(mw(9)), 0);
            set_coef(rom::u16(mw(8)), 0);
            const uint32_t x = uint32_t(fxBipolar500(P(*this, 2)));
            set_coef(rom::u16(mw(11)), x);
            set_coef(rom::u16(mw(10)), x);
            writeSweWordListSub(4, mw(8));
        } else {
            const uint32_t x = uint32_t(fxBipolar500(P(*this, 2)));
            set_coef(rom::u16(mw(9)), x);
            set_coef(rom::u16(mw(8)), x);
            set_coef(rom::u16(mw(11)), 0);
            set_coef(rom::u16(mw(10)), 0);
            writeSweWordListSub(2, mw(10));
            writeSweWordListSub(2, mw(8));
        }
        break;
    case 3: {
        const int32_t half = int32_t(half_area(*this));
        if (mode == 0) {
            set_coef(rom::u16(mw(12)), 0x4000); set_coef(rom::u16(mw(13)), 0x4000); set_coef(rom::u16(mw(14)), 0);
            set_offs(mslot(0), 1);
            set_offs(mslot(5), half);
        } else if (mode == 1) {
            set_coef(rom::u16(mw(12)), 0x4000); set_coef(rom::u16(mw(13)), 0x4000); set_coef(rom::u16(mw(14)), 0);
            set_offs(mslot(0), half);
            set_offs(mslot(5), 1);
        } else {
            set_coef(rom::u16(mw(12)), 0x7FFF); set_coef(rom::u16(mw(13)), 0); set_coef(rom::u16(mw(14)), 0x7FFF);
            set_offs(mslot(0), 1);
            set_offs(mslot(5), half);
        }
        writeSweWordListSub(3, mw(12));
        writeSweDeSub(1, msl(0));
        writeSweDeSub(1, msl(5));
        fxInsEr1(2);
        break;
    }
    case 4:
        fxFloatCurveB(1, P(*this, 4), 64, 0x35E53A);
        fxCopyWords2(0x35E53A, 0x35E540);
        break;
    case 9: {
        const uint32_t wet = PU(*this, 9);
        wet_words(*this, MW, ER_REV, wet, GGXX);
        writeSweWordListSub(4, MW);
        break;
    }
    case 12: case 13: case 14: case 15:
        ins_delay_filters(*this, p);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {3, 0, 1, 4, 9, 12, 14})
            fxInsEr1(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// ---- the early reflection types (msb 3) ----
namespace {
constexpr uint32_t ER_SLOTS = 0x35DCF4;        // u8 x 22: the reflection slots (+20, 21 the pre-delay pair)
constexpr uint32_t ER_STEPS = 0x35DD0E;        // u16 x 20: the reflection gains
constexpr uint32_t ER_PATTERNS = 0x370660;     // s16 rows of 20 (time, gain) pairs, stride 80
constexpr uint32_t ER_DENSITY = 0x370980;      // u8: density byte -> time scale / 10
constexpr uint32_t ER_LEVEL = 0x370A00;        // u8: ER level byte -> gain offset + 128
constexpr uint32_t ER_ROWS = 0x36F872;         // s16 rows of 6 per type lsb: the room row per size
constexpr uint32_t ER_ROW_STEPS = 0x35DC54, ER_ROW_SLOTS = 0x35DBE4;
constexpr uint32_t ER_WET = 0x35DF6C;
inline int32_t er_row(const Host& h) { return rom::s16(ER_ROWS + uint32_t(sh::s8(uint32_t(12 * lsb(h)))) + 2u * PU(h, 0)); }
void er_wet(Host& h)
{
    wet_words(h, ER_WET, ER_REV, PU(h, 9), GXGX);
    h.writeSweWordListSub(4, ER_WET);
}
}  // namespace

// 0x3A6338 fxInsErGain(x): the comb-gain table's loss word at x clamped to 0..255
int32_t Host::fxInsErGain(int x)
{
    int32_t v = sh::s16(uint32_t(x));
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return rom::s16(rom::FX_COMB_GAIN + 4u * uint32_t(v));
}

// 0x3A6354 fxInsErPattern: the 20 early reflections of the pattern row
// (parameter 0, +6 for type lsb 3, +8 for lsb 4) scaled by the density (parameter 1) after the
// pre-delay (parameter 3), gains from the level (parameter 10), clamped to the delay area
void Host::fxInsErPattern()
{
    const uint32_t t = lsb(*this);
    const uint32_t rowi = (PU(*this, 0) + (t == 3 ? 6u : t == 4 ? 8u : 0u)) & 0xFFFF;
    uint32_t row = ER_PATTERNS + rowi * 80;
    const uint32_t density = rom::u8(ER_DENSITY + PU(*this, 1));
    const int32_t level = int32_t(rom::u8(ER_LEVEL + PU(*this, 10))) - 128;
    const int32_t base = fxMsToSamples(rom::s16(rom::FX_DELAY_WORD + 2u * PU(*this, 3))) + offs(rom::u8(ER_SLOTS + 21));
    const int32_t limit = int32_t(ram.u32(FX_BASES + 12) - ram.u32(FX_BASES + 8)) - 1;
    for (uint32_t i = 0; i < 20; i++, row += 4) {
        const uint32_t w = uint32_t(fxMulShift16(rom::s16(row))) & 0xFFFF;
        int32_t pos = base + sh::sdiv3000(int32_t(density * w), 10);
        int32_t gain;
        if (pos > limit) {
            pos = limit;
            gain = 0;
        } else {
            const int32_t g = rom::s16(row + 2);
            if (g > 0)
                gain = sh::s16(uint32_t(fxInsErGain(g + level)));
            else
                gain = -sh::s16(uint32_t(fxInsErGain(level - g)));
        }
        set_offs(rom::u8(ER_SLOTS + i), pos);
        set_coef(rom::u16(ER_STEPS + 2 * i), uint32_t((gain + (gain < 0 ? 1 : 0)) >> 1));
        if (i == 19)
            set_offs(rom::u8(ER_SLOTS + 20), pos > 0x8000 ? 0x8000 : pos);
    }
    set_coef(rom::u16(ER_STEPS + 38), 0);
    if (t == 3)
        for (uint32_t k = 30; k <= 36; k += 2)
            set_coef(rom::u16(ER_STEPS + k), 0);
}

// 0x3A6580 fxInsCrossDelayEr: Cross Delay, ER 2, Gate Rev, Reverse Gate (types 37-40; msb 3 lsb 1-4)
void Host::fxInsCrossDelayEr(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxInsErPattern();
        fxScaleRow(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeSweWordListSub(20, ER_STEPS);
        writeSweDeSub(21, ER_SLOTS);
        writeSweWordListSub(16, ER_ROW_STEPS);
        writeSweDeSub(8, ER_ROW_SLOTS);
        break;
    case 1: case 3:
        fxInsErPattern();
        writeSweWordListSub(20, ER_STEPS);
        writeSweDeSub(21, ER_SLOTS);
        break;
    case 10:
        fxInsErPattern();
        writeSweWordListSub(20, ER_STEPS);
        break;
    case 2: {
        const int32_t diff = int32_t(rom::u8(rom::FX_DIFFUSION + PU(*this, 2)));
        set_offs(rom::u8(0x35DD38), offs(rom::u8(0x35DD36)) + diff + 1);
        set_offs(rom::u8(0x35DD39), offs(rom::u8(0x35DD37)) + diff + 1);
        writeSweDeSub(2, 0x35DD38);
        fxScaleRow(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeSweDeSub(1, ER_ROW_SLOTS + 7);
        break;
    }
    case 4:
        set_coef(rom::u16(0x35DCF0), uint32_t(fxBipolar500(P(*this, 4))));
        writeSweWordListSub(1, 0x35DCF0);
        break;
    case 5:
        fxDesignFilter(2, 1, 1, P(*this, 5), 64, 0, 0x35E7DC);
        break;
    case 6:
        fxDesignFilter(2, 2, 0, P(*this, 6), 64, 0, 0x35E7F0);
        break;
    case 9:
        er_wet(*this);
        break;
    case 11:
        fxScaleRow(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeSweWordListSub(16, ER_ROW_STEPS);
        break;
    case 12:
        fxDesignFilter(1, 0, 0, rom::s8(rom::FX_DAMP_FREQ + PU(*this, 12)), 64, 0, 0x35E576);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 2, 4, 5, 6, 9, 12})
            fxInsCrossDelayEr(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

}  // namespace fs1r::vop3host

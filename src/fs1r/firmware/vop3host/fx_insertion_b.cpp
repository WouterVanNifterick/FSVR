// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 2: the wah, distortion and dynamics types, the hall and room
// reverbs, pitch change, ensemble detune, the EQs and Lo-Fi.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ram;
using namespace ins;

namespace {
constexpr uint32_t WAH_CUTOFF = 0x35D958, WAH_DEPTH = 0x35D950, WAH_SENSE = 0x35D968, WAH_ATTACK = 0x35D9B4;
constexpr uint32_t WAH_DRIVE = 0x35D9B8, WAH_MODE = 0x35DA74, WAH_MODE2 = 0x35DA78, WAH_WET = 0x35DA30, WAH_MIRROR = 0x35DA04;
constexpr uint32_t DIST_DRIVE = 0x35D9C0, DIST_WET = 0x35DA48, DIST_MODE = 0x35DA7C, DIST_LEVEL = 0x35DA80;
constexpr uint32_t DIST_LPF = 0x35E818, DIST_LPF_C = 0x35E822, DIST_HPF = 0x35E5AC, DIST_HPF_C = 0x35E5B2;
constexpr uint32_t DIST_PRE = 0x35E82C, DIST_PRE_C = 0x35E836, DIST_EQ = 0x35E840, DIST_FILL = 0x35E9CA;
constexpr uint32_t DYN_THRESH = 0x35E5B8, DYN_PAIR = 0x35D9D0, DYN_WORD = 0x35D9E0, DYN_ROW = 0x35D9E4;
constexpr uint32_t DD_WET = 0x35DE60, DD_MODE = 0x35DE70, DD_LEVEL = 0x35DE6A, DD_FB = 0x35DE7A, DD_SLOTS = 0x35DE7C;
constexpr uint32_t DD_DRIVE = 0x35DE56, DD_THRESH = 0x35DEFA, DD_PAIR = 0x35DE3C, DD_WORD = 0x35DE44, DD_ROW = 0x35DE46;
constexpr uint32_t PC_PAN = 0x35D880, PC_FB = 0x35D898, PC_WET = 0x35D894, PC_LEVEL = 0x35D884;
constexpr uint32_t EQ7_A = 0x35E84A, EQ7_A_C = 0x35E868, EQ7_B = 0x35E854, EQ7_B_C = 0x35E872, EQ7_C = 0x35E85E, EQ7_C_C = 0x35E87C;
constexpr uint32_t AMP_ROWS = 0x36F5B8;          // s8 rows of 14: the amp simulator's filter sets
constexpr uint32_t DETUNE_TABLE = 0x36F408;      // f32: detune ratio per index
constexpr uint32_t LFO_DIR_STEPS = 0x35D7E0;     // u16 x 4: the pitch change LFO words

// the wah "cutoff" word: 3052 v / 16 + 7837
inline int32_t wah_cutoff(uint32_t v) { return int32_t((3052u * (v & 0xFFFF)) >> 4) + 7837; }
// the wah "sensitivity" pair: -327680 / v for v >= 11, else 0x8000
void wah_sense(Host& h, int p)
{
    if (PU(h, p) < 11) {
        h.set_coef(rom::u16(WAH_SENSE + 2), 0x8000);
        h.set_coef(rom::u16(WAH_SENSE), 0x8000);
    } else {
        const int32_t x = sh::dtoi(sh::ddiv(-327680.0, sh::ftod(sh::utof(PU(h, p)))));
        h.set_coef(rom::u16(WAH_SENSE + 2), uint32_t(int16_t(x)));
        h.set_coef(rom::u16(WAH_SENSE), uint32_t(int16_t(x)));
    }
    h.writeSweWordListSub(2, WAH_SENSE);
}
// the wah + distortion Dry/Wet block (parameters 9, 10, 14): the drive words, the mode pair(s)
// and eight pan words whose layout follows whether the distortion is on (parameter 10)
void wah_dist_wet(Host& h, int drive_index, bool second_pair)
{
    h.fxStoreScaled(P(h, 10), drive_index, WAH_DRIVE, 0);
    h.set_coef(rom::u16(WAH_MODE + 2), 23170);
    h.set_coef(rom::u16(WAH_MODE), 23170);
    if (second_pair) { h.set_coef(rom::u16(WAH_MODE2 + 2), 0x5A81); h.set_coef(rom::u16(WAH_MODE2), 0x5A81); }
    const uint32_t wet = PU(h, 9);
    h.writeSweWordListSub(2, WAH_MODE);
    if (second_pair)
        h.writeSweWordListSub(2, WAH_MODE2);
    const uint32_t g = uint32_t(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> 2);
    const uint32_t w = uint32_t(rom::s16(PAN_LAW + 2u * wet) >> 2);
    h.set_coef(rom::u16(WAH_WET + 8), g);
    h.set_coef(rom::u16(WAH_WET), g);
    if (PU(h, 10) == 0) {
        const uint32_t x = uint32_t(h.fxMulTypeTableB(int32_t(w)));
        h.set_coef(rom::u16(WAH_WET + 10), x);
        h.set_coef(rom::u16(WAH_WET + 2), x);
        h.set_coef(rom::u16(WAH_WET + 12), 8192);
        h.set_coef(rom::u16(WAH_WET + 4), 8192);
        h.set_coef(rom::u16(WAH_WET + 14), 0);
        h.set_coef(rom::u16(WAH_WET + 6), 0);
    } else {
        h.set_coef(rom::u16(WAH_WET + 12), g);
        h.set_coef(rom::u16(WAH_WET + 4), g);
        h.set_coef(rom::u16(WAH_WET + 10), w);
        h.set_coef(rom::u16(WAH_WET + 2), w);
        const uint32_t x = uint32_t(h.fxMulTypeTableB(int32_t(w)));
        h.set_coef(rom::u16(WAH_WET + 14), x);
        h.set_coef(rom::u16(WAH_WET + 6), x);
    }
    h.writeSweWordListSub(8, WAH_WET);
}
// the distortion group's Dry/Wet words (parameter 9): the mode pair (a, b), then g, x, 0, g, x
// over DIST_WET
void dist_wet(Host& h, uint32_t a, uint32_t b, bool two_lists)
{
    h.set_coef(rom::u16(DIST_MODE), a);
    h.set_coef(rom::u16(two_lists ? DIST_LEVEL : DIST_MODE + 2), b);
    const uint32_t wet = PU(h, 9);
    if (two_lists) {
        h.writeSweWordListSub(1, DIST_MODE);
        h.writeSweWordListSub(1, DIST_LEVEL);
    } else {
        h.writeSweWordListSub(2, DIST_MODE);
    }
    const uint32_t g = uint32_t(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> 2);
    h.set_coef(rom::u16(DIST_WET + 4), g);
    h.set_coef(rom::u16(DIST_WET), g);
    const uint32_t x = uint32_t(h.fxMulTypeTableB(rom::s16(PAN_LAW + 2u * wet) >> 2));
    h.set_coef(rom::u16(DIST_WET + 8), x);
    h.set_coef(rom::u16(DIST_WET + 2), x);
    h.set_coef(rom::u16(DIST_WET + 6), 0);
    h.writeSweWordListSub(5, DIST_WET);
}
// the compressor / noise gate output level words through table B
void dyn_level(Host& h, int p)
{
    const int32_t i = sh::dtoi(sh::dmul(sh::ftod(rom::f32(LEVEL_CURVE + 4u * PU(h, p))), 8192.0));
    const uint32_t x = uint32_t(h.fxMulTypeTableB(int16_t(i)));
    h.set_coef(rom::u16(DIST_WET + 6), x);
    h.set_coef(rom::u16(DIST_WET + 2), x);
    h.set_coef(rom::u16(DIST_WET + 8), 0);
    h.set_coef(rom::u16(DIST_WET + 4), 0);
    h.set_coef(rom::u16(DIST_WET), 0);
    h.writeSweWordListSub(5, DIST_WET);
}
void dyn_filters(Host& h)
{
    h.fxDesignFilter(2, 1, 1, 0, 64, 0, DIST_LPF);
    h.fxCopyWords5(DIST_LPF, DIST_LPF_C);
    h.fxDesignFilter(1, 1, 11, 0, 64, 0, DIST_HPF);
    h.fxCopyWords3(DIST_HPF, DIST_HPF_C);
    h.fxDesignFilter(2, 1, 11, 0, 64, 0, DIST_PRE);
    h.fxCopyWords5(DIST_PRE, DIST_PRE_C);
    h.fxDesignFilter(2, 4, 11, 0, 64, 0, DIST_EQ);
}
// the dynamics + delay types' Dry/Wet words over DD_WET (pan law >> 1)
void dd_wet(Host& h, uint32_t wet, int shift)
{
    wet_words(h, DD_WET, PAN_LAW, wet, GXGX, shift);
    h.writeSweWordListSub(4, DD_WET);
}
}  // namespace

// 0x3ACA4E fxInsAutoWahDist: A-Wah+Dist / A-Wah+Odrv (types 14, 15; msb 4 lsb 3, 4)
void Host::fxInsAutoWahDist(int p)
{
    p &= 0xFF;
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 13);
        break;
    case 1: case 2: {
        const int32_t w = wah_cutoff(PU(*this, 2));
        set_coef(rom::u16(WAH_CUTOFF), uint32_t(w));
        set_coef(rom::u16(WAH_DEPTH), PU(*this, 1) * uint32_t(32767 - int16_t(w)) / 127);
        writeSweWordListSub(1, WAH_CUTOFF);
        writeSweWordListSub(1, WAH_DEPTH);
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
        fxDesignFilter(2, 4, 6, t == 4 ? 24 : 20, P(*this, 11), 0, 0x35E80E);
        break;
    case 12:
        fxDesignFilter(2, 1, 10, t == 4 ? 41 : 43, P(*this, 12), 10, 0x35E804);
        break;
    case 13:
        fxDesignFilter(2, 1, 4, P(*this, 13), 64, 7, 0x35E7FA);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        if (t == 4) {
            fxFillBand(104, 0x35E9E2);
            fxDesignFilter(1, 1, 1, 8, 64, 0, 0x35E594);
            fxDesignFilter(1, 1, 0, 44, 64, 0, 0x35E59A);
        } else {
            fxFillBand(120, 0x35E9E2);
            fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35E594);
            fxDesignFilter(1, 1, 0, 54, 64, 0, 0x35E59A);
        }
        for (int q : {0, 1, 3, 5, 7, 9, 11, 12, 13})
            fxInsAutoWahDist(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3ACFEC fxInsTouchWahDist: T-Wah+Dist / T-Wah+Odrv (types 16, 17; msb 4 lsb 6, 7); 0 runs on into 1
void Host::fxInsTouchWahDist(int p)
{
    p &= 0xFF;
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0:
        set_coef(rom::u16(WAH_ATTACK), uint32_t(P(*this, 0) * 258));
        writeSweWordListSub(1, WAH_ATTACK);
        [[fallthrough]];
    case 1:
        set_coef(rom::u16(WAH_CUTOFF), uint32_t(wah_cutoff(PU(*this, 1))));
        writeSweWordListSub(1, WAH_CUTOFF);
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
        fxDesignFilter(2, 4, 6, t == 6 ? 24 : 20, P(*this, 11), 0, 0x35E80E);
        break;
    case 12:
        fxDesignFilter(2, 1, 10, t == 6 ? 41 : 43, P(*this, 12), 10, 0x35E804);
        break;
    case 13:
        fxDesignFilter(2, 1, 4, P(*this, 13), 64, 7, 0x35E7FA);
        break;
    case 15: {
        int32_t v = PLO(*this, 15) - 52;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        fxStoreMirror3(v, WAH_MIRROR);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        if (t == 6) {
            fxDesignFilter(1, 1, 1, 8, 64, 0, 0x35E594);
            fxDesignFilter(1, 1, 0, 44, 64, 0, 0x35E59A);
            fxFillBand(104, 0x35E9E2);
        } else {
            fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35E594);
            fxDesignFilter(1, 1, 0, 54, 64, 0, 0x35E59A);
            fxFillBand(120, 0x35E9E2);
        }
        fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35E57C);
        fxDesignFilter(1, 0, 0, 54, 64, 0, 0x35E582);
        for (int q : {0, 1, 2, 5, 7, 9, 11, 12, 13, 15})
            fxInsTouchWahDist(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A52F4 fxInsDistortion: Distortion / Overdrive (types 28, 30; msb 5 lsb 1, 2); 9 re-runs 0
void Host::fxInsDistortion(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 4:
        fxStoreScaled(P(*this, 0), P(*this, 4), DIST_DRIVE, 0);
        break;
    case 1: case 2:
        fxDesignFilter(2, 4, 2, P(*this, 1), P(*this, 2), 0, DIST_EQ);
        break;
    case 3:
        fxDesignFilter(2, 1, 4, P(*this, 3), 64, 7, DIST_PRE);
        break;
    case 6: case 7: case 8:
        fxDesignFilter(2, 1, 10, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PRE_C);
        break;
    case 9:
        dist_wet(*this, 1448, 0, false);
        fxInsDistortion(0);
        break;
    case 10:
        fxFillBand(P(*this, 10), DIST_FILL);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        fxStoreMirrorX32(0, DYN_THRESH);
        fxWriteComplementPair(11, DYN_PAIR);
        fxWriteNegated(73, DYN_WORD);
        if (lsb(*this) == 1) {
            fxDesignFilter(2, 1, 1, 0, 64, 0, DIST_LPF);
            fxDesignFilter(1, 1, 0, 54, 64, 0, DIST_HPF);
        } else {
            fxDesignFilter(2, 1, 1, 8, 64, 0, DIST_LPF);
            fxDesignFilter(1, 1, 0, 44, 64, 0, DIST_HPF);
        }
        for (int q : {1, 3, 6, 9, 10})
            fxInsDistortion(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A55AC fxInsAmpSim (type 32; msb 5 lsb 3); 1 (the amp type) re-runs 2, 9 re-runs 0
void Host::fxInsAmpSim(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 3:
        fxStoreScaled(P(*this, 0), P(*this, 3), DIST_DRIVE, 0);
        break;
    case 1: {
        const uint32_t row = AMP_ROWS + uint32_t(int8_t(14 * PLO(*this, 1)));
        fxDesignFilter(2, 1, rom::s8(row), rom::s8(row + 2), rom::s8(row + 1), 0, DIST_LPF);
        fxDesignFilter(1, 1, rom::s8(row + 3), rom::s8(row + 5), rom::s8(row + 4), 0, DIST_HPF);
        fxDesignFilter(2, 1, rom::s8(row + 6), rom::s8(row + 8), rom::s8(row + 7), 0, DIST_PRE_C);
        fxDesignFilter(2, 4, rom::s8(row + 9), rom::s8(row + 11), rom::s8(row + 10), rom::s8(row + 12), DIST_EQ);
        fxInsAmpSim(2);
        break;
    }
    case 2:
        if (P(*this, 1) == 1)
            fxDesignFilter(2, 1, 4, P(*this, 2), 56, 15, DIST_PRE);
        else
            fxDesignFilter(2, 1, 4, P(*this, 2), 64, P(*this, 1) == 2 ? 15 : 7, DIST_PRE);
        break;
    case 9:
        dist_wet(*this, 1448, 0, false);
        fxInsAmpSim(0);
        break;
    case 10:
        fxFillBand(P(*this, 10), DIST_FILL);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        fxStoreMirrorX32(0, DYN_THRESH);
        fxWriteComplementPair(11, DYN_PAIR);
        fxWriteNegated(73, DYN_WORD);
        for (int q : {1, 2, 9, 10})
            fxInsAmpSim(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A58D8 fxInsCompDist (type 24; msb 5 lsb 4)
void Host::fxInsCompDist(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: fxStoreMirrorHalf(P(*this, 0), DYN_THRESH); break;
    case 1: fxWriteComplementPair(P(*this, 1), DYN_PAIR); break;
    case 2: fxWriteWord(P(*this, 2), DYN_WORD); break;
    case 3: fxWriteRow8(P(*this, 3), DYN_ROW); break;
    case 4: dyn_level(*this, 4); break;
    case 9: {
        const uint32_t w = 7240;
        set_coef(rom::u16(DIST_LEVEL + 2), w);
        set_coef(rom::u16(DIST_LEVEL), w);
        writeSweWordListSub(2, DIST_LEVEL);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        dyn_filters(*this);
        for (int q : {0, 1, 2, 3, 4, 9})
            fxInsCompDist(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A5B94 fxInsCompDistDelay (type 25 "Cmp+DS+Dly"; msb 5 lsb 5); 9 re-runs 0
void Host::fxInsCompDistDelay(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: case 4:
        fxStoreScaled(P(*this, 0), P(*this, 4), DIST_DRIVE, 0);
        break;
    case 1: case 2:
        fxDesignFilter(2, 4, 2, P(*this, 1), P(*this, 2), 0, DIST_EQ);
        break;
    case 3:
        fxDesignFilter(2, 1, 4, P(*this, 3), 64, 7, DIST_PRE);
        break;
    case 6: case 7: case 8:
        fxDesignFilter(2, 1, 10, P(*this, 6), P(*this, 7), P(*this, 8), DIST_PRE_C);
        break;
    case 9:
        dist_wet(*this, 5792, 14481, true);
        fxInsCompDistDelay(0);
        break;
    case 10:
        fxFillBand(P(*this, 10), DIST_FILL);
        break;
    case 11: fxStoreMirrorHalf(P(*this, 11), DYN_THRESH); break;
    case 12: fxWriteComplementPair(P(*this, 12), DYN_PAIR); break;
    case 13: fxWriteWord(P(*this, 13), DYN_WORD); break;
    case 14: fxWriteRow8(P(*this, 14), DYN_ROW); break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        fxDesignFilter(2, 1, 1, 0, 64, 0, DIST_LPF);
        fxDesignFilter(1, 1, 0, 54, 64, 0, DIST_HPF);
        for (int q : {1, 3, 6, 9, 10, 11, 12, 13, 14})
            fxInsCompDistDelay(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A5EF0 fxInsHmEnhancer (type 22; msb 5 lsb 6)
void Host::fxInsHmEnhancer(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxDesignFilter(2, 1, 5, P(*this, 0), 64, 7, DIST_LPF);
        fxCopyWords5(DIST_LPF, DIST_LPF_C);
        fxDesignFilter(1, 1, 1, P(*this, 0), 64, 0, DIST_HPF);
        fxCopyWords3(DIST_HPF, DIST_HPF_C);
        break;
    case 1: {
        const uint32_t w = uint32_t(P(*this, 1) * 258);
        set_coef(rom::u16(0x35DA16), w);
        set_coef(rom::u16(0x35DA14), w);
        writeSweWordListSub(2, 0x35DA14);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(fxMulTypeTableB(int32_t((PU(*this, 2) * 0x81u) >> 1)));
        set_coef(rom::u16(DIST_WET + 6), x);
        set_coef(rom::u16(DIST_WET + 2), x);
        writeSweWordListSub(1, DIST_WET + 2);
        writeSweWordListSub(1, DIST_WET + 6);
        break;
    }
    case 9: {
        const uint32_t w = 1448;
        set_coef(rom::u16(DIST_LEVEL + 2), w);
        set_coef(rom::u16(DIST_LEVEL), w);
        writeSweWordListSub(2, DIST_LEVEL);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        fxDesignFilter(2, 1, 11, 0, 64, 0, DIST_PRE);
        fxCopyWords5(DIST_PRE, DIST_PRE_C);
        fxDesignFilter(2, 4, 11, 0, 64, 0, DIST_EQ);
        for (int q : {0, 1, 2, 9})
            fxInsHmEnhancer(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A60F4 fxInsNoiseGate (type 23; msb 5 lsb 7)
void Host::fxInsNoiseGate(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: fxStoreMirrorX32(P(*this, 0), DYN_THRESH); break;
    case 1: fxWriteComplementPair(P(*this, 1), DYN_PAIR); break;
    case 2: fxWriteNegated(P(*this, 2), DYN_WORD); break;
    case 3: dyn_level(*this, 3); break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        dyn_filters(*this);
        for (int q : {0, 1, 2, 3})
            fxInsNoiseGate(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3AD6E0 fxInsWahDistDelay: Wah+DS+Dly / Wah+OD+Dly (types 18, 19; msb 12 lsb 1, 2)
void Host::fxInsWahDistDelay(int p)
{
    p &= 0xFF;
    constexpr uint32_t SL = 0x35DE36, FB = 0x35DE34, LEVEL = 0x35DE22, DRIVE = 0x35DDFE;
    constexpr uint32_t MODE = 0x35DE28, MODE2 = 0x35DE30, WET = 0x35DE10;
    constexpr uint32_t WD_CUTOFF = 0x35DDF8, WD_SENSE = 0x35DDFA, WD_ATTACK = 0x35DDFC, WD_MIRROR = 0x35DE06;
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = fxMsToSamples(P(*this, 0)) + offs(rom::u8(SL));
        for (uint32_t i = 1; i <= 4; i++)
            set_offs(rom::u8(SL + i), d);
        writeSweDeSub(4, SL + 1);
        break;
    }
    case 1:
        set_coef(rom::u16(FB), uint32_t(fxBipolar500(P(*this, 1))));
        writeSweWordListSub(1, FB);
        break;
    case 2: {
        const int32_t i = sh::ftoi(sh::fmul(8192.0f, rom::f32(LEVEL_CURVE + 4u * PU(*this, 2))));
        set_coef(rom::u16(LEVEL + 4), uint32_t(i));
        set_coef(rom::u16(LEVEL), uint32_t(i));
        writeSweWordListSub(1, LEVEL);
        writeSweWordListSub(1, LEVEL + 4);
        break;
    }
    case 3: case 4: case 5: case 6: case 9: {
        if (t == 2) {
            fxDesignFilter(1, 1, 1, 8, 64, 0, 0x35DED0);
            fxDesignFilter(1, 1, 0, 44, 64, 0, 0x35DED6);
            fxDesignFilter(1, 1, 0, 45, 64, 0, 0x35DEDC);
            fxDesignFilter(1, 2, 2, 24, PLO(*this, 5), 0, 0x35DEE2);
            fxDesignFilter(2, 1, 10, 41, PLO(*this, 6), 10, 0x35DF16);
            fxFillBand(104, 0x35DE82);
        } else if (t == 1) {
            fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35DED0);
            fxDesignFilter(1, 1, 0, 54, 64, 0, 0x35DED6);
            fxDesignFilter(1, 1, 0, 53, 64, 0, 0x35DEDC);
            fxDesignFilter(1, 2, 2, 20, PLO(*this, 5), 0, 0x35DEE2);
            fxDesignFilter(2, 1, 10, 43, PLO(*this, 6), 10, 0x35DF16);
            fxFillBand(120, 0x35DE82);
        }
        fxStoreScaled(P(*this, 3), P(*this, 4), DRIVE, 0);
        set_coef(rom::u16(MODE + 2), 23170); set_coef(rom::u16(MODE), 23170);
        set_coef(rom::u16(MODE2 + 2), 0x5A81); set_coef(rom::u16(MODE2), 0x5A81);
        const uint32_t wet = PU(*this, 9);
        writeSweWordListSub(2, MODE);
        writeSweWordListSub(2, MODE2);
        const int32_t dry = rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet))));
        const int32_t wetw = rom::s16(PAN_LAW + 2u * wet);
        set_coef(rom::u16(WET + 8), uint32_t(dry >> 2));
        set_coef(rom::u16(WET), uint32_t(dry >> 2));
        if (P(*this, 3) == 0) {
            const uint32_t x = uint32_t(fxMulTypeTableB(wetw >> 2));
            set_coef(rom::u16(WET + 10), x);
            set_coef(rom::u16(WET + 2), x);
            set_coef(rom::u16(WET + 12), 0x4000);
            set_coef(rom::u16(WET + 4), 0x4000);
            set_coef(rom::u16(WET + 14), 0);
            set_coef(rom::u16(WET + 6), 0);
        } else {
            set_coef(rom::u16(WET + 12), uint32_t(dry >> 1));
            set_coef(rom::u16(WET + 4), uint32_t(dry >> 1));
            set_coef(rom::u16(WET + 10), uint32_t(wetw >> 2));
            set_coef(rom::u16(WET + 2), uint32_t(wetw >> 2));
            const uint32_t x = uint32_t(fxMulTypeTableB(wetw >> 1));
            set_coef(rom::u16(WET + 14), x);
            set_coef(rom::u16(WET + 6), x);
        }
        writeSweWordListSub(8, WET);
        break;
    }
    case 10:
        set_coef(rom::u16(WD_ATTACK), uint32_t(P(*this, 10) * 258));
        writeSweWordListSub(1, WD_ATTACK);
        [[fallthrough]];
    case 12:
        if (PU(*this, 12) <= 10)
            set_coef(rom::u16(WD_SENSE), 0x8000);
        else
            set_coef(rom::u16(WD_SENSE), uint32_t(int16_t(sh::dtoi(sh::ddiv(-327680.0, sh::ftod(sh::utof(PU(*this, 12))))))));
        writeSweWordListSub(1, WD_SENSE);
        break;
    case 11:
        set_coef(rom::u16(WD_CUTOFF), uint32_t(int32_t((3052u * PU(*this, 11)) >> 4) + 7483));
        writeSweWordListSub(1, WD_CUTOFF);
        break;
    case 13: {
        int32_t v = PLO(*this, 13) - 52;
        const int8_t c = int8_t(v);
        if (c < 0 || c > 15)
            v = c < 0 ? 0 : 15;
        const int32_t w = rom::s16(0x36F6A4 + 2u * uint32_t(int8_t(v)));
        set_coef(rom::u16(WD_MIRROR + 2), uint32_t(w));
        set_coef(rom::u16(WD_MIRROR), uint32_t(w));
        set_coef(rom::u16(WD_MIRROR + 4), uint32_t((-32768 - w) * 2));
        writeSweWordListSub(3, WD_MIRROR);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 10, 11, 12, 13})
            fxInsWahDistDelay(q);
        fxDesignFilter(1, 1, 1, 0, 64, 0, 0x35DEB8);
        fxDesignFilter(1, 0, 0, 54, 64, 0, 0x35DEBE);
        fxDesignFilter(1, 1, 2, 28, 66, 0, 0x35DEC4);
        fxCopyWords3(0x35DEC4, 0x35DEEE);
        fxDesignFilter(1, 2, 3, 46, 64, 0, 0x35DECA);
        fxCopyWords3(0x35DECA, 0x35DEF4);
        fxDesignFilter(1, 1, 0, 60, 64, 0, 0x35DEE8);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A4544 fxInsCompressor: Compressor / Cmp+OD+Dly (types 26, 27; msb 13 lsb 1, 2); 9 re-runs 0
void Host::fxInsCompressor(int p)
{
    p &= 0xFF;
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = fxMsToSamples(P(*this, 0)) + offs(rom::u8(DD_SLOTS));
        for (uint32_t i = 1; i <= 4; i++)
            set_offs(rom::u8(DD_SLOTS + i), d);
        writeSweDeSub(4, DD_SLOTS + 1);
        break;
    }
    case 1:
        set_coef(rom::u16(DD_FB), uint32_t(fxBipolar500(P(*this, 1))));
        writeSweWordListSub(1, DD_FB);
        break;
    case 2: {
        const int32_t i = sh::ftoi(sh::fmul(8192.0f, rom::f32(LEVEL_CURVE + 4u * PU(*this, 2))));
        set_coef(rom::u16(DD_LEVEL + 4), uint32_t(i));
        set_coef(rom::u16(DD_LEVEL), uint32_t(i));
        writeSweWordListSub(1, DD_LEVEL);
        writeSweWordListSub(1, DD_LEVEL + 4);
        break;
    }
    case 3: case 4: case 5: case 6:
        if (t == 2) {
            fxDesignFilter(2, 1, 1, 8, 64, 0, 0x35DF20);
            fxDesignFilter(1, 1, 0, 44, 64, 0, 0x35DF00);
            fxDesignFilter(2, 1, 4, 45, 64, 7, 0x35DF2A);
            fxDesignFilter(2, 4, 2, 24, PLO(*this, 5), 0, 0x35DF3E);
            fxDesignFilter(2, 1, 10, 41, PLO(*this, 6), 10, 0x35DF34);
            fxFillBand(104, 0x35DE9A);
        } else if (t == 1) {
            fxDesignFilter(2, 1, 1, 0, 64, 0, 0x35DF20);
            fxDesignFilter(1, 1, 0, 54, 64, 0, 0x35DF00);
            fxDesignFilter(2, 1, 4, 53, 64, 7, 0x35DF2A);
            fxDesignFilter(2, 4, 2, 20, PLO(*this, 5), 0, 0x35DF3E);
            fxDesignFilter(2, 1, 10, 43, PLO(*this, 6), 10, 0x35DF34);
            fxFillBand(120, 0x35DE9A);
        }
        fxStoreScaled(P(*this, 3), P(*this, 4), DD_DRIVE, 0);
        break;
    case 9: {
        set_coef(rom::u16(DD_MODE), 5792);
        set_coef(rom::u16(DD_MODE + 4), 14481);
        const uint32_t wet = PU(*this, 9);
        writeSweWordListSub(1, DD_MODE);
        writeSweWordListSub(1, DD_MODE + 4);
        dd_wet(*this, wet, 1);
        fxInsCompressor(0);
        break;
    }
    case 10: fxStoreMirrorHalf(P(*this, 10), DD_THRESH); break;
    case 11: fxWriteComplementPair(P(*this, 11), DD_PAIR); break;
    case 12: fxWriteWord(P(*this, 12), DD_WORD); break;
    case 13: fxWriteRow8(P(*this, 13), DD_ROW); break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 9, 10, 11, 12, 13})
            fxInsCompressor(q);
        fxDesignFilter(1, 1, 0, 60, 64, 0, 0x35DF06);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A4A28 fxInsDistDelay: Dist+Delay / Odrv+Delay (types 29, 31; msb 13 lsb 3, 4); 9 re-runs 0
void Host::fxInsDistDelay(int p)
{
    p &= 0xFF;
    const uint32_t t = lsb(*this);
    switch (p) {
    case 0: {
        const int32_t d = fxMsToSamples(P(*this, 0)) + offs(rom::u8(DD_SLOTS));
        set_offs(rom::u8(DD_SLOTS + 4), d);
        set_offs(rom::u8(DD_SLOTS + 3), d);
        writeSweDeSub(2, DD_SLOTS + 3);
        break;
    }
    case 1:
        set_offs(rom::u8(DD_SLOTS + 2), fxMsToSamples(P(*this, 1)) + offs(rom::u8(DD_SLOTS)));
        writeSweDeSub(1, DD_SLOTS + 2);
        break;
    case 2:
        set_offs(rom::u8(DD_SLOTS + 1), fxMsToSamples(P(*this, 2)) + offs(rom::u8(DD_SLOTS)));
        writeSweDeSub(1, DD_SLOTS + 1);
        break;
    case 3:
        set_coef(rom::u16(DD_FB), uint32_t(fxBipolar500(P(*this, 3))));
        writeSweWordListSub(1, DD_FB);
        break;
    case 4: {
        const int32_t i = sh::ftoi(sh::fmul(8192.0f, rom::f32(LEVEL_CURVE + 4u * PU(*this, 4))));
        set_coef(rom::u16(DD_LEVEL + 4), uint32_t(i));
        set_coef(rom::u16(DD_LEVEL), uint32_t(i));
        writeSweWordListSub(1, DD_LEVEL);
        writeSweWordListSub(1, DD_LEVEL + 4);
        break;
    }
    case 5: case 6:
        fxStoreScaled(P(*this, 5), P(*this, 6), DD_DRIVE, 0);
        break;
    case 7:
        fxDesignFilter(2, 4, 2, t == 4 ? 24 : 20, PLO(*this, 7), 0, 0x35DF3E);
        break;
    case 8:
        fxDesignFilter(2, 1, 10, t == 4 ? 41 : 43, PLO(*this, 8), 10, 0x35DF34);
        break;
    case 9: {
        set_coef(rom::u16(DD_MODE), 1448);
        set_coef(rom::u16(DD_MODE + 2), 0);
        const uint32_t wet = PU(*this, 9);
        writeSweWordListSub(2, DD_MODE);
        dd_wet(*this, wet, 1);
        fxInsDistDelay(0);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        fxStoreMirrorX32(0, DD_THRESH);
        fxWriteComplementPair(11, DD_PAIR);
        fxWriteNegated(73, DD_WORD);
        if (t == 3) {
            fxDesignFilter(2, 1, 1, 0, 64, 0, 0x35DF20);
            fxDesignFilter(1, 1, 0, 54, 64, 0, 0x35DF00);
            fxDesignFilter(2, 1, 4, 53, 64, 7, 0x35DF2A);
            fxFillBand(120, 0x35DE9A);
        } else {
            fxDesignFilter(2, 1, 1, 8, 64, 0, 0x35DF20);
            fxDesignFilter(1, 1, 0, 44, 64, 0, 0x35DF00);
            fxDesignFilter(2, 1, 4, 45, 64, 7, 0x35DF2A);
            fxFillBand(104, 0x35DE9A);
        }
        for (int q : {0, 1, 2, 3, 4, 5, 7, 8, 9})
            fxInsDistDelay(q);
        fxDesignFilter(1, 1, 0, 60, 64, 0, 0x35DF06);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// ---- pitch change (msb 7) ----

// 0x3A99FC fxInsPitchRatio: the pitch change's LFO word for one voice, from the pitch
// (parameter 0) and the voice's fine value v
int32_t Host::fxInsPitchRatio(int v)
{
    const int32_t d = v - 64;
    const int32_t pitch = int16_t(P(*this, 0) - 64);
    if (pitch == 0) {
        const int32_t a = int16_t(d);
        return rom::s16(0x36F218 + 2u * uint32_t(a >= 0 ? a : -a));
    }
    const int32_t semi = rom::s16(0x36F154 + 2u * uint32_t(int32_t(PU(*this, 0)) - 40));
    return pitch >= 0 ? semi + d : semi - d;
}

// 0x3A9B8A fxLfoDirection(lfo, v): the voice's LFO word pair and the LFO's direction bits
void Host::fxLfoDirection(int lfo, int v)
{
    const uint32_t a = lfo == 12 ? LFO_DIR_STEPS : LFO_DIR_STEPS + 4;
    const uint32_t w = uint32_t(fxInsPitchRatio(v));
    set_coef(rom::u16(a + 2), w);
    set_coef(rom::u16(a), w);
    writeSweWordListSub(4, LFO_DIR_STEPS);
    const uint32_t l = uint32_t(lfo) & 0xFF;
    const int32_t s = 100 * (P(*this, 0) - 64) + int16_t(v) - 64;
    if (s > 0) {
        ram.w16(FX_LFO25 + 2 * l, (ram.u16(FX_LFO25 + 2 * l) & 0xFF1F) | 0x20);
        ram.w16(FX_LFO27 + 2 * l, 0x7FFF);
    } else {
        ram.w16(FX_LFO25 + 2 * l, (ram.u16(FX_LFO25 + 2 * l) & 0xFF1F) | 0x40);
        ram.w16(FX_LFO27 + 2 * l, 0);
    }
    writeSwe25Sub(lfo);
    writeSwe27Sub(lfo);
}

// 0x3A9CC6 fxInsPitchSlots: the first voice's delay slots from parameter 1
void Host::fxInsPitchSlots()
{
    constexpr uint32_t SL = 0x35D8A4;
    const int32_t d = fxMsToSamples(rom::s16(PITCH_DELAY_TABLE + 2u * PU(*this, 1))) + 1;
    set_offs(rom::u8(SL + 2), d);
    set_offs(rom::u8(SL + 1), d);
    set_offs(rom::u8(SL + 4), d + 384);
    set_offs(rom::u8(SL + 3), d + 384);
    writeSweDeSub(4, SL + 1);
}

// 0x3A9D58 fxStoreSpan5(v): the second voice's delay slots above half the area
void Host::fxStoreSpan5(int v)
{
    constexpr uint32_t SL = 0x35D8A4;
    const int32_t t = fxMsToSamples(rom::s16(PITCH_DELAY_TABLE + 2u * uint32_t(int16_t(v))));
    const int32_t half = int32_t(half_area(*this));
    const int32_t d = half + t + 1;
    set_offs(rom::u8(SL + 5), half);
    set_offs(rom::u8(SL + 7), d);
    set_offs(rom::u8(SL + 6), d);
    set_offs(rom::u8(SL + 9), d + 384);
    set_offs(rom::u8(SL + 8), d + 384);
    writeSweDeSub(5, SL + 5);
}

// 0x3A9DD0 fxInsPitchChange (type 7; msb 7 lsb 1)
void Host::fxInsPitchChange(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0: {
        fxLfoDirection(12, P(*this, 2));
        fxLfoDirection(13, P(*this, 3));
        auto set24 = [&](int lfo, uint32_t v) {
            ram.w16(FX_LFO24 + 2u * uint32_t(lfo), (ram.u16(FX_LFO24 + 2u * uint32_t(lfo)) & 0xFC00) | v);
        };
        const uint32_t v = uint32_t(rom::s16(0x36F1B6 + 2u * uint32_t(int32_t(PU(*this, 0)) - 40)));
        set24(12, v);
        set24(13, v);
        writeSwe24Sub(12);
        writeSwe24Sub(13);
        break;
    }
    case 1:
        fxInsPitchSlots();
        fxStoreSpan5(P(*this, 1));
        break;
    case 2:
        fxLfoDirection(12, P(*this, 2));
        break;
    case 3:
        fxLfoDirection(13, P(*this, 3));
        break;
    case 4: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 4)));
        set_coef(rom::u16(PC_FB + 2), x);
        set_coef(rom::u16(PC_FB), x);
        const uint32_t v = PU(*this, 4);
        if (v >= 32 && v <= 96) {
            set_coef(rom::u16(PC_FB + 8), uint32_t(-6144));
            set_coef(rom::u16(PC_FB + 4), uint32_t(-6144));
            set_coef(rom::u16(PC_FB + 10), uint32_t(-10240));
            set_coef(rom::u16(PC_FB + 6), uint32_t(-10240));
        }
        if (v <= 31) {
            const uint32_t i = uint32_t(sh::dtoi(sh::ddiv(sh::dmul(sh::dadd(sh::utod(v), -1.0), 2048.0), 31.0))) & 0xFFFF;
            set_coef(rom::u16(PC_FB + 8), i - 8192u);
            set_coef(rom::u16(PC_FB + 4), i - 8192u);
            set_coef(rom::u16(PC_FB + 10), uint32_t(-8192) - i);
            set_coef(rom::u16(PC_FB + 6), uint32_t(-8192) - i);
        }
        if (v >= 97) {
            const uint32_t i = uint32_t(sh::dtoi(sh::ddiv(sh::dmul(sh::dadd(sh::dneg(sh::utod(v)), 127.0), 2048.0), 31.0))) & 0xFFFF;
            set_coef(rom::u16(PC_FB + 8), i - 8192u);
            set_coef(rom::u16(PC_FB + 4), i - 8192u);
            set_coef(rom::u16(PC_FB + 10), uint32_t(-8192) - i);
            set_coef(rom::u16(PC_FB + 6), uint32_t(-8192) - i);
        }
        writeSweWordListSub(2, PC_FB);
        writeSweWordListSub(4, PC_FB + 4);
        break;
    }
    case 9: {
        const uint32_t wet = PU(*this, 9);
        const float dry = sh::itof(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> 2);
        const uint32_t g = uint32_t(int16_t(sh::ftoi(dry)));
        set_coef(rom::u16(PC_WET + 2), g);
        set_coef(rom::u16(PC_WET), g);
        const float wf = sh::itof(rom::s16(PAN_LAW + 2u * wet) >> 2);
        const uint32_t x = uint32_t(fxMulTypeTableB(int16_t(sh::ftoi(wf))));
        set_coef(rom::u16(PC_LEVEL + 2), x);
        set_coef(rom::u16(PC_LEVEL), x);
        writeSweWordListSub(2, PC_LEVEL);
        writeSweWordListSub(2, PC_WET);
        break;
    }
    case 10: case 11: {
        const float c = rom::f32(LEVEL_CURVE + 4u * PU(*this, 11));
        set_coef(rom::u16(0x35D888), uint32_t(int16_t(sh::ftoi(sh::fmul(sh::itof(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(PU(*this, 10)))))), c)))));
        set_coef(rom::u16(0x35D88E), uint32_t(sh::ftoi(sh::fmul(sh::itof(rom::s16(PAN_LAW + 2u * PU(*this, 10))), c))));
        writeSweWordListSub(1, 0x35D888);
        writeSweWordListSub(1, 0x35D88E);
        break;
    }
    case 12: case 13: {
        const float c = rom::f32(LEVEL_CURVE + 4u * PU(*this, 13));
        set_coef(rom::u16(0x35D88C), uint32_t(int16_t(sh::ftoi(sh::fmul(sh::itof(rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(PU(*this, 12)))))), c)))));
        set_coef(rom::u16(0x35D88A), uint32_t(sh::ftoi(sh::fmul(sh::itof(rom::s16(PAN_LAW + 2u * PU(*this, 12))), c))));
        writeSweWordListSub(2, 0x35D88A);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {2, 3, 4, 0, 1, 9, 10, 12})
            fxInsPitchChange(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3AA392 fxInsEnsDetune (type 8; msb 7 lsb 3)
void Host::fxInsEnsDetune(int p)
{
    p &= 0xFF;
    constexpr uint32_t SL = 0x35D8A4;
    switch (p) {
    case 0: {
        const uint32_t v = PU(*this, 0);
        const double K = sh::from_bits64(0x40EFFEEFA4BDBA0AULL), NK = sh::from_bits64(0xC0EFFEEFA4BDBA0AULL);
        const uint32_t lo = FX_LFO25 + 24, hi = FX_LFO25 + 26;   // LFO 12 and 13
        if (v < 64) {
            const double a = sh::dmul(sh::ftod(rom::f32(DETUNE_TABLE + 4u * (114u - v))), K);
            const int32_t w0 = sh::dtoi(sh::dadd(a, NK));
            const double b = sh::dmul(sh::ftod(rom::f32(DETUNE_TABLE + 4u * (v - 14u))), K);
            const int32_t w1 = sh::dtoi(sh::dsub(K, b));
            set_coef(rom::u16(LFO_DIR_STEPS + 2), uint32_t(int16_t(w0)));
            set_coef(rom::u16(LFO_DIR_STEPS), uint32_t(int16_t(w0)));
            set_coef(rom::u16(LFO_DIR_STEPS + 6), uint32_t(int16_t(w1)));
            set_coef(rom::u16(LFO_DIR_STEPS + 4), uint32_t(int16_t(w1)));
            ram.w16(lo, (ram.u16(lo) & 0xFF1F) | 0x20);
            ram.w16(FX_LFO27 + 24, 0x7FFF);
            ram.w16(hi, (ram.u16(hi) & 0xFF1F) | 0x40);
            ram.w16(FX_LFO27 + 26, 0);
        } else if (v > 64) {
            const double a = sh::dmul(sh::ftod(rom::f32(DETUNE_TABLE + 4u * (114u - v))), K);
            const int32_t w0 = sh::dtoi(sh::dadd(sh::dneg(a), K));
            const double b = sh::dmul(sh::ftod(rom::f32(DETUNE_TABLE + 4u * (v - 14u))), K);
            const int32_t w1 = sh::dtoi(sh::dadd(b, NK));
            set_coef(rom::u16(LFO_DIR_STEPS + 2), uint32_t(int16_t(w0)));
            set_coef(rom::u16(LFO_DIR_STEPS), uint32_t(int16_t(w0)));
            set_coef(rom::u16(LFO_DIR_STEPS + 6), uint32_t(int16_t(w1)));
            set_coef(rom::u16(LFO_DIR_STEPS + 4), uint32_t(int16_t(w1)));
            ram.w16(lo, (ram.u16(lo) & 0xFF1F) | 0x40);
            ram.w16(FX_LFO27 + 24, 0);
            ram.w16(hi, (ram.u16(hi) & 0xFF1F) | 0x20);
            ram.w16(FX_LFO27 + 26, 0x7FFF);
        } else {
            for (int i = 0; i < 4; i++)
                set_coef(rom::u16(LFO_DIR_STEPS + 2u * uint32_t(i)), 0);
        }
        writeSweWordListSub(4, LFO_DIR_STEPS);
        writeSwe25Sub(12);
        writeSwe25Sub(13);
        writeSwe27Sub(12);
        writeSwe27Sub(13);
        break;
    }
    case 1: {
        const int32_t d = ms(*this, MOD_DELAY_TABLE, 1) + offs(rom::u8(SL)) + 1;
        for (uint32_t i = 1; i <= 4; i++)
            set_offs(rom::u8(SL + i), d);
        writeSweDeSub(5, SL);
        break;
    }
    case 2: {
        const int32_t half = int32_t(half_area(*this));
        set_offs(rom::u8(SL + 5), half);
        const int32_t d = ms(*this, MOD_DELAY_TABLE, 2) + half + 1;
        for (uint32_t i = 6; i <= 9; i++)
            set_offs(rom::u8(SL + i), d);
        writeSweDeSub(5, SL + 5);
        break;
    }
    case 9:
        wet_words(*this, PC_PAN, PAN_LAW, PU(*this, 9), GGXX);
        writeSweWordListSub(4, PC_PAN);
        break;
    case 10: case 11:
        fxDesignFilter(2, 1, 2, P(*this, 10), P(*this, 11), 0, EQ7_A);
        fxCopyWords5(EQ7_A, EQ7_A_C);
        break;
    case 12: case 13:
        fxDesignFilter(2, 4, 3, P(*this, 12), P(*this, 13), 0, EQ7_C);
        fxCopyWords5(EQ7_C, EQ7_C_C);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 9, 10, 12})
            fxInsEnsDetune(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3AAB70 fxIns3BandEq (type 21; msb 7 lsb 6)
void Host::fxIns3BandEq(int p)
{
    p &= 0xFF;
    constexpr uint32_t MIX = 0x35D87A;
    switch (p) {
    case 0: case 5:
        fxDesignFilter(2, 1, 6, P(*this, 5), PLO(*this, 0), 0, EQ7_A);
        fxCopyWords5(EQ7_A, EQ7_A_C);
        break;
    case 1: case 2: case 3:
        fxDesignFilter(2, 1, 10, P(*this, 1), P(*this, 2), P(*this, 3), EQ7_B);
        fxCopyWords5(EQ7_B, EQ7_B_C);
        break;
    case 4: case 6:
        fxDesignFilter(2, 4, 7, P(*this, 6), P(*this, 4), 0, EQ7_C);
        fxCopyWords5(EQ7_C, EQ7_C_C);
        break;
    case 14:
        if (P(*this, 14) != 0) {
            set_coef(rom::u16(MIX), 0x4000);
            set_coef(rom::u16(MIX + 2), 0);
            set_coef(rom::u16(MIX + 4), 0x4000);
            set_coef(rom::u16(MIX + 6), uint32_t(fxMulTypeTableB(8192)));
            set_coef(rom::u16(0x35D890), uint32_t(fxMulTypeTableB(0)));
        } else {
            set_coef(rom::u16(MIX), 0);
            set_coef(rom::u16(MIX + 2), 8192);
            set_coef(rom::u16(MIX + 4), 8192);
            set_coef(rom::u16(MIX + 6), uint32_t(fxMulTypeTableB(0)));
            set_coef(rom::u16(0x35D890), uint32_t(fxMulTypeTableB(8192)));
        }
        writeSweWordListSub(3, MIX);
        writeSweWordListSub(1, MIX + 6);
        writeSweWordListSub(1, 0x35D890);
        break;
    case 255:
        set_coef(rom::u16(PC_PAN + 2), uint32_t(fxMulTypeTableB(8192)));
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {5, 6, 1, 14})
            fxIns3BandEq(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// 0x3A6B88 fxInsLoFi (type 20; msb 10 lsb 1)
void Host::fxInsLoFi(int p)
{
    p &= 0xFF;
    constexpr uint32_t RATE = 0x35DDB4, BITS = 0x35DDB6, LEVEL = 0x35DDB8, WET = 0x35DDBA, WET2 = 0x35DDBE;
    constexpr uint32_t GAIN = 0x35DDC2, GAIN2 = 0x35DDC4, OUT = 0x35DDF4;
    switch (p) {
    case 0:
        set_coef(rom::u16(RATE), uint32_t(P(*this, 0)));
        writeSweWordListSub(1, RATE);
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
        set_coef(rom::u16(BITS), w);
        writeSweWordListSub(1, BITS);
        break;
    }
    case 3: case 5:
        fxDesignFilter(2, 1, 4, P(*this, 3), 64, P(*this, 5), 0x35DDC8);
        break;
    case 4: {
        const uint32_t row = 0x370FDE + uint32_t(int8_t(10 * PLO(*this, 4)));
        fxDesignFilter(1, 1, rom::s8(row), rom::s8(row + 1), rom::s8(row + 2), 0, 0x35DDDE);
        fxDesignFilter(2, 2, rom::s8(row + 3), rom::s8(row + 4), rom::s8(row + 5), rom::s8(row + 6), 0x35DDE4);
        fxDesignFilter(1, 2, rom::s8(row + 7), rom::s8(row + 8), rom::s8(row + 9), 0, 0x35DDEE);
        break;
    }
    case 7:
        if (PU(*this, 7) == 1) {
            fxDesignFilter(1, 2, 3, 44, 76, 0, 0x35DDD2);
            fxDesignFilter(1, 2, 3, 44, 52, 0, 0x35DDD8);
        } else {
            fxDesignFilter(1, 2, 11, 0, 64, 0, 0x35DDD2);
            fxDesignFilter(1, 2, 11, 0, 64, 0, 0x35DDD8);
        }
        break;
    case 2: case 6: case 9: {
        const uint32_t wet = PU(*this, 9);
        const int32_t dry = rom::s16(PAN_LAW + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> 1;
        const int32_t wetw = rom::s16(PAN_LAW + 2u * wet) >> 1;
        set_coef(rom::u16(WET), uint32_t(dry));
        set_coef(rom::u16(WET + 2), uint32_t(dry));
        set_coef(rom::u16(WET2), uint32_t(fxMulTypeTableB(wetw)));
        set_coef(rom::u16(WET2 + 2), uint32_t(fxMulTypeTableB(wetw)));
        const uint32_t sel = PU(*this, 6);
        const uint32_t lawrow = 0x370F70 + 2u * PU(*this, 2);
        int32_t a = rom::s16(lawrow), b;
        switch (sel) {
        case 0: b = 2048; break;
        case 1: b = 4096; break;
        case 2: b = 8192; break;
        case 3: b = 16384; break;
        case 4: b = 32767; break;
        case 5: a = rom::s16(lawrow + 12); b = 32767; break;
        case 6: a = rom::s16(lawrow + 24); b = 32767; break;
        default: b = wetw; break;
        }
        set_coef(rom::u16(LEVEL), uint32_t(a));
        set_coef(rom::u16(OUT), uint32_t(b));
        const int32_t gsh = (32767 >> sel) >> 1;
        set_coef(rom::u16(GAIN), uint32_t(gsh));
        set_coef(rom::u16(GAIN2), uint32_t(gsh));
        writeSweWordListSub(1, GAIN);
        writeSweWordListSub(1, GAIN2);
        writeSweWordListSub(2, WET);
        writeSweWordListSub(2, WET2);
        writeSweWordListSub(1, LEVEL);
        writeSweWordListSub(1, OUT);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 7})
            fxInsLoFi(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

}  // namespace fs1r::vop3host

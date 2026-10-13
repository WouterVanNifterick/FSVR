// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Variation handlers. The variation block's parameters live at FX_PARAMS + 0x80 and its
// type-scale table is fxMulTypeTableD. The four delay types are in fx_reverb_delay.cpp.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {
constexpr uint32_t VP = FX_PARAMS + 0x80;          // the variation parameter block
constexpr uint32_t MOD_DELAY_TABLE = 0x371BD4;     // u16: modulation delay byte -> 0.1 ms word
constexpr uint32_t MOD_LEVEL_STEPS = 0x35D80C;     // u16 x 6: modulation depth steps (fxStoreUdiv)
constexpr uint32_t ROT_STEPS = 0x35D818;           // u16 x 7: rotary / tremolo / auto pan depth steps
constexpr uint32_t PAN_STEPS = 0x35D8EA;           // u16 x 4: the pan words (parameter 9), modulation group
constexpr uint32_t PAN_STEPS_B = 0x35D904;         // u16 x 4: the pan words of the rotary group
constexpr uint32_t FB_STEPS = 0x35D8F2;            // u16 x 2: feedback
constexpr uint32_t MIX_STEPS = 0x35D8F6;           // u16 x 2: the delay-line mix words
constexpr uint32_t MOD_SLOTS = 0x35D8FA;           // u8 x 10: the modulation delay lines' slots
constexpr uint32_t TREM_WORDS = 0x35D90C;          // u16 x 2
constexpr uint32_t TREM_SLOTS = 0x35D910;          // u8 x 8
constexpr uint32_t LPF_A = 0x35E6CE, LPF_A_COPY = 0x35E6DA, HPF_A = 0x35E6D4, HPF_A_COPY = 0x35E6E0;
constexpr uint32_t LPF_B = 0x35E6E6, LPF_B_COPY = 0x35E6F2, HPF_B = 0x35E6EC, HPF_B_COPY = 0x35E6F8;
constexpr uint32_t BAL_STEP_A = 0x35D762, BAL_STEP_B = 0x35D764, BAL_STEP_C = 0x35D75A;
constexpr uint32_t MODE_STEPS_A = 0x35D7A8, MODE_STEPS_B = 0x35D7AC;

// the pan/level words shared by the variation effects (parameter 9)
void pan_words(Host& h, uint32_t steps)
{
    const uint32_t g = uint32_t(rom::s16(rom::FX_PAN_LAW + 2) >> 2);
    h.set_coef(rom::u16(steps + 2), g);
    h.set_coef(rom::u16(steps), g);
    const uint32_t x = uint32_t(h.fxMulTypeTableD(rom::s16(rom::FX_PAN_LAW + 254) >> 2));
    h.set_coef(rom::u16(steps + 6), x);
    h.set_coef(rom::u16(steps + 4), x);
    h.writeSweWordListSub(4, steps);
}

// parameters 5/6 (LPF) and 7/8 (HPF) of the modulation effects
void filters(Host& h, int p, uint32_t lpf, uint32_t lpf_copy, uint32_t hpf, uint32_t hpf_copy)
{
    if (p == 5 || p == 6) {
        h.fxDesignFilter(1, 1, 2, h.ram.s16(VP + 10), h.ram.s16(VP + 12), 0, lpf);
        h.fxCopyWords3(lpf, lpf_copy);
    } else {
        h.fxDesignFilter(1, 2, 3, h.ram.s16(VP + 14), h.ram.s16(VP + 16), 0, hpf);
        h.fxCopyWords3(hpf, hpf_copy);
    }
}

// the memory top above area 4
uint32_t top(const Host& h)
{
    return h.ram.u8(FX_MEM_FLAG) != 0 ? 0x10000u : 0x40000u;
}

int32_t P(const Host& h, int i) { return h.ram.s16(VP + 2 * i); }
uint32_t mod_slot(int i) { return rom::u8(MOD_SLOTS + uint32_t(i)); }
uint32_t mod_delay(Host& h, int p)
{
    return uint32_t(h.fxMsToSamples(rom::s16(MOD_DELAY_TABLE + 2u * (uint32_t(P(h, p)) & 0xFFFF)))) & 0xFFFF;
}
}  // namespace

// 0x3A04AC fxVarChorus: Chorus (type 1) and Celeste (type 2)
void Host::fxVarChorus(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1: {
        const int x = int(ram.u8(VP + 3) << 1);
        fxStoreUdiv(x, MOD_LEVEL_STEPS);
        fxStoreUdiv(x, MOD_LEVEL_STEPS + 4);
        fxStoreUdiv(x, MOD_LEVEL_STEPS + 8);
        break;
    }
    case 2: {
        const uint32_t x = uint32_t(fxBipolar376(P(*this, 2)));
        set_coef(rom::u16(FB_STEPS + 2), x);
        set_coef(rom::u16(FB_STEPS), x);
        writeSweWordListSub(2, FB_STEPS);
        break;
    }
    case 3: case 14: {
        const uint32_t t = mod_delay(*this, 3);
        int32_t base = offs(mod_slot(0));
        const int32_t d = int32_t(t + uint32_t(base));
        set_offs(mod_slot(2), d + 1);
        set_offs(mod_slot(1), d + 1);
        set_offs(mod_slot(4), d + 0x41);
        set_offs(mod_slot(3), d + 0x41);
        if (P(*this, 14) == 0) {
            set_coef(rom::u16(MIX_STEPS), 0x4000);
            set_coef(rom::u16(MIX_STEPS + 2), 0x4000);
            set_offs(mod_slot(5), 0);
        } else {
            set_coef(rom::u16(MIX_STEPS), 0x7FFF);
            set_coef(rom::u16(MIX_STEPS + 2), 0);
            base = int32_t(((top(*this) - ram.u32(FX_BASES + 16)) >> 1) + 1);
            set_offs(mod_slot(5), base);
        }
        const int32_t d2 = int32_t(t + uint32_t(base));
        set_offs(mod_slot(7), d2 + 1);
        set_offs(mod_slot(6), d2 + 1);
        set_offs(mod_slot(9), d2 + 0x41);
        set_offs(mod_slot(8), d2 + 0x41);
        writeSweWordListSub(2, MIX_STEPS);
        writeSweDeSub(4, MOD_SLOTS + 1);
        writeSweDeSub(5, MOD_SLOTS + 5);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_A, LPF_A_COPY, HPF_A, HPF_A_COPY);
        break;
    case 9:
        pan_words(*this, PAN_STEPS);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 5, 7, 9})
            fxVarChorus(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    default:
        break;
    }
}

// 0x3A0840 fxVarFlanger: Flanger (type 3)
void Host::fxVarFlanger(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1:
        fxStoreUdiv(P(*this, 1), MOD_LEVEL_STEPS);
        fxStoreUdiv(P(*this, 1), MOD_LEVEL_STEPS + 4);
        break;
    case 2: {
        const uint32_t x = uint32_t(fxBipolar500(P(*this, 2)));
        set_coef(rom::u16(FB_STEPS + 2), x);
        set_coef(rom::u16(FB_STEPS), x);
        writeSweWordListSub(2, FB_STEPS);
        break;
    }
    case 3: {
        const uint32_t t = mod_delay(*this, 3);
        const int32_t d = int32_t(uint32_t(offs(mod_slot(0))) + t + 1);
        set_offs(mod_slot(2), d);
        set_offs(mod_slot(1), d);
        const int32_t d2 = int32_t(t + uint32_t(offs(mod_slot(5))) + 1);
        set_offs(mod_slot(7), d2);
        set_offs(mod_slot(6), d2);
        writeSweWordListSub(2, MIX_STEPS);
        writeSweDeSub(2, MOD_SLOTS + 1);
        writeSweDeSub(2, MOD_SLOTS + 6);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_A, LPF_A_COPY, HPF_A, HPF_A_COPY);
        break;
    case 9:
        pan_words(*this, PAN_STEPS);
        break;
    case 13:
        fxStoreBipolar500(P(*this, 13), BAL_STEP_C);
        break;
    case 255:
        set_offs(mod_slot(5), int32_t(((top(*this) - ram.u32(FX_BASES + 16)) >> 1) + 1));
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 5, 7, 9, 13})
            fxVarFlanger(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    default:
        break;
    }
}

// 0x3A0B4E fxVarSymphonic: Symphonic (type 4)
void Host::fxVarSymphonic(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1: {
        const int x = int((uint32_t(P(*this, 1)) & 0xFFFF) * 5) >> 3;
        fxStoreUdiv(x, MOD_LEVEL_STEPS);
        fxStoreUdiv(x, MOD_LEVEL_STEPS + 4);
        fxStoreUdiv(x, MOD_LEVEL_STEPS + 8);
        const int y = int((uint32_t(P(*this, 1)) & 0xFFFF) * 3) >> 3;
        fxStoreUdiv(y, MOD_LEVEL_STEPS + 2);
        fxStoreUdiv(y, MOD_LEVEL_STEPS + 6);
        fxStoreUdiv(y, MOD_LEVEL_STEPS + 10);
        break;
    }
    case 2: {
        const uint32_t t = mod_delay(*this, 2);
        const int32_t d = int32_t(t + uint32_t(offs(mod_slot(0))));
        set_offs(mod_slot(2), d + 2);
        set_offs(mod_slot(1), d + 2);
        set_offs(mod_slot(4), d + 0x42);
        set_offs(mod_slot(3), d + 0x42);
        set_offs(mod_slot(7), d + 130);
        set_offs(mod_slot(6), d + 130);
        writeSweDeSub(4, MOD_SLOTS + 1);
        writeSweDeSub(2, MOD_SLOTS + 6);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_A, LPF_A_COPY, HPF_A, HPF_A_COPY);
        break;
    case 9:
        pan_words(*this, PAN_STEPS);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 5, 7, 9})
            fxVarSymphonic(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    default:
        break;
    }
}

// 0x3A0E30 fxVarRotarySp: Rotary SP (type 8)
void Host::fxVarRotarySp(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1: {
        const uint32_t v = ram.u16(VP + 2);
        const int x = v < 0x20 ? (sh::s8(v) << 1) : 0x40;
        fxStoreNegMul(x, ROT_STEPS + 2);
        fxStoreNegMul(x, ROT_STEPS + 6);
        fxStoreNegMul(x, ROT_STEPS + 10);
        const int y = int(ram.u8(VP + 3) >> 2);
        fxStoreUdiv(y, ROT_STEPS);
        fxStoreUdiv(y, ROT_STEPS + 4);
        fxStoreUdiv(y, ROT_STEPS + 8);
        fxStoreUdiv(y, ROT_STEPS + 12);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_B, LPF_B_COPY, HPF_B, HPF_B_COPY);
        break;
    case 9:
        pan_words(*this, PAN_STEPS_B);
        break;
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 5, 7, 9})
            fxVarRotarySp(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    default:
        break;
    }
}

// 0x3A1044 fxVarTremolo: Tremolo (type 9)
void Host::fxVarTremolo(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1:
        fxStoreNegMul(P(*this, 1), ROT_STEPS + 2);
        fxStoreNegMul(P(*this, 1), ROT_STEPS + 6);
        break;
    case 2: {
        const int y = int(ram.u8(VP + 5) >> 2);
        fxStoreUdiv(y, ROT_STEPS);
        fxStoreUdiv(y, ROT_STEPS + 4);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_B, LPF_B_COPY, HPF_B, HPF_B_COPY);
        break;
    case 13:
        fxStoreBipolar500(P(*this, 13), BAL_STEP_B);
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
            const uint32_t x = uint32_t(fxMulTypeTableD(0x7FFF));
            set_coef(rom::u16(PAN_STEPS_B + 6), x);
            set_coef(rom::u16(PAN_STEPS_B + 4), x);
            set_offs(id(5), 1024);
            set_offs(id(6), 1025);
            set_offs(id(7), 1025);
        }
        writeSweWordListSub(2, TREM_WORDS);
        writeSweDeSub(3, TREM_SLOTS + 5);
        break;
    }
    case 255: {
        const uint32_t x = uint32_t(fxMulTypeTableD(8192));
        set_coef(rom::u16(PAN_STEPS_B + 6), x);
        set_coef(rom::u16(PAN_STEPS_B + 4), x);
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 5, 7, 13, 14})
            fxVarTremolo(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    default:
        break;
    }
}

// 0x3A1368 fxVarAutoPan: Auto Pan (type 10); p3 (the direction) re-runs p1 and p2
void Host::fxVarAutoPan(int p)
{
    p &= 0xFF;
    switch (p) {
    case 0:
        fxLfoSpeed(P(*this, 0), 8);
        break;
    case 1:
        fxStoreNegMul(P(*this, 1), ROT_STEPS + 2);
        fxStoreNegMul(P(*this, 1), ROT_STEPS + 6);
        break;
    case 2: {
        const int32_t m = P(*this, 3);
        const int y = (m == 3 || m == 4) ? int(ram.u8(VP + 5) >> 2) : 0;
        fxStoreUdiv(y, ROT_STEPS);
        fxStoreUdiv(y, ROT_STEPS + 4);
        break;
    }
    case 3: {
        const uint32_t m = ram.u16(VP + 6);
        if (m < 5) {
            static const int A[5] = {0x40, 0x54, 0x40, 0x5E, 0x40};
            static const int B[5] = {0x7C, 0x40, 0x54, 0x40, 0x5E};
            fxStoreBipolar500(A[m], BAL_STEP_A);
            fxStoreBipolar500(B[m], BAL_STEP_B);
            fxStoreModePair(1, MODE_STEPS_A);
            fxStoreModePair(1, MODE_STEPS_B);
        } else {
            fxStoreBipolar500(0x40, BAL_STEP_A);
            fxStoreBipolar500(0x7C, BAL_STEP_B);
            fxStoreModePair(2, MODE_STEPS_A);
            fxStoreModePair(2, MODE_STEPS_B);
        }
        fxVarAutoPan(1);
        fxVarAutoPan(2);
        break;
    }
    case 5: case 6: case 7: case 8:
        filters(*this, p, LPF_B, LPF_B_COPY, HPF_B, HPF_B_COPY);
        break;
    case 255: {
        const uint32_t x = uint32_t(fxMulTypeTableD(8192));
        set_coef(rom::u16(PAN_STEPS_B + 6), x);
        set_coef(rom::u16(PAN_STEPS_B + 4), x);
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 3, 5, 7, 9})
            fxVarAutoPan(q);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    default:
        break;
    }
}

}  // namespace fs1r::vop3host

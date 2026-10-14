// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Window reset, master EQ, system parameters and the master gain.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {
constexpr uint32_t RESET_SLOTS_REVERB = 0x35DAE4;      // u8 x 14
constexpr uint32_t RESET_SLOTS_REVERB_DELAY = 0x35DAFC; // u8 x 12
constexpr uint32_t RESET_SLOTS_VARIATION = 0x35DB08;   // u8 x 12
constexpr uint32_t RESET_SLOTS_INSERTION = 0x35DB14;   // u8 x 24
}  // namespace

// 0x3A3308 fxResetWindow: silence a window (zero its constants), spread its delay slots
// evenly over the window's area, and for kinds 2-4 load the "clear" program.
void Host::fxResetWindow(int kind)
{
    kind &= 0xFF;
    auto zero_steps = [&](uint32_t n, uint32_t list) {
        for (uint32_t i = 0; i < n; i++)
            set_coef(rom::u16(list + 2 * i), 0);
        writeSweWordListSub(n, list);
    };
    // offs[slot_i] = ((span / n) * (n - 1 - i)), unsigned divide, 32-bit product
    auto spread = [&](uint32_t n, uint32_t slots, uint32_t span) {
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t q = span / n;
            set_offs(rom::u8(slots + i), int32_t(q * (n - 1 - i)));
        }
        writeSweDeSub(n, slots);
    };
    const uint32_t b0 = ram.u32(FX_BASES + 0), b1 = ram.u32(FX_BASES + 4), b2 = ram.u32(FX_BASES + 8);
    const uint32_t b3 = ram.u32(FX_BASES + 12), b4 = ram.u32(FX_BASES + 16);
    switch (kind) {
    case 0:
        // the second tag image behind the base tags: the base program's "clear" tags
        for (uint32_t i = 0; i < 232; i++)
            ram.w8(FX_TAGS + i, rom::u8(rom::FX_TAGS_BASE + 232 + i));
        writeSwe0CSub(0x5E, rom::FX_STEPS_REVERB);
        zero_steps(0x5E, rom::FX_STEPS_REVERB);
        spread(14, RESET_SLOTS_REVERB, b1 - b0);
        break;
    case 1:
        break;
    case 2:
        zero_steps(0x60, rom::FX_STEPS_INSERTION);
        spread(24, RESET_SLOTS_INSERTION, b3 - b2);
        loadSubCoeffBlock(2, 1);
        break;
    case 3:
        zero_steps(0x48, rom::FX_STEPS_REVERB_DELAY);
        spread(12, RESET_SLOTS_REVERB_DELAY, b4 - b3);
        loadSubCoeffBlock(3, 1);
        break;
    case 4:
        zero_steps(0x48, rom::FX_STEPS_VARIATION);
        spread(12, RESET_SLOTS_VARIATION, 0x3FFFF - b4);
        loadSubCoeffBlock(4, 1);
        break;
    }
}

// 0x3A356C fxMasterEq: the performance EQ, one biquad per band through fxDesignFilter, then the
// band's five words are copied to the other channel's steps. p selects the band by its
// parameter group (0-3 low, 4-6 low-mid, 8-10 mid, 12-14 high-mid, 16-19 high); 255 does all.
void Host::fxMasterEq(int p)
{
    p &= 0xFF;
    auto eq = [&](int i) -> int32_t { return ram.s16(FX_PARAMS_EQ + 2 * i); };
    if (p <= 3) {
        const int kind = eq(3) == 0 ? 2 : 10;
        fxDesignFilter(2, 2, kind, eq(1), ram.s8(FX_PARAMS_EQ + 1), eq(2), 0x35E926);
        fxCopyWords5(0x35E926, 0x35E958);
    } else if (p >= 4 && p <= 6) {
        fxDesignFilter(2, 2, 10, eq(5), eq(4), eq(6), 0x35E944);
        fxCopyWords5(0x35E944, 0x35E976);
    } else if (p >= 8 && p <= 10) {
        fxDesignFilter(2, 2, 10, eq(9), eq(8), eq(10), 0x35E930);
        fxCopyWords5(0x35E930, 0x35E962);
    } else if (p >= 12 && p <= 14) {
        fxDesignFilter(2, 2, 10, eq(13), eq(12), eq(14), 0x35E94E);
        fxCopyWords5(0x35E94E, 0x35E980);
    } else if (p >= 16 && p <= 19) {
        const int kind = eq(19) == 0 ? 7 : 10;
        fxDesignFilter(2, 4, kind, eq(17), eq(16), eq(18), 0x35E93A);
        fxCopyWords5(0x35E93A, 0x35E96C);
    } else if (p == 255) {
        ram.w8(SWE_SUB_HOLD, 1);
        fxMasterEq(0);
        fxMasterEq(4);
        fxMasterEq(8);
        fxMasterEq(12);
        fxMasterEq(16);
        ram.w8(SWE_SUB_HOLD, 0);
    }
}

// flash 0x3A2EC fxSystemParam: the system parameters into the mixer steps of the base program.
//   p 0/1 reverb pan + level (quarter-sine pan law scaled by level, >> 3), 2 reverb return pan,
//   3 reverb send level (>> 1), 4 insertion send (<< 1), 5 insertion return pan, 6 variation
//   send (>> 1), 7 variation return (<< 1), 8 variation to reverb (x 5792/127), 9 = 6, 7, 8 and
//   insertion parameter 9, 10/11 pans, 12/13 pans, 14 master volume (quadratic law over
//   FX_SUB_LEVEL), 255 everything plus the mixer's DC blocker.
// Each block's enable byte, when nonzero, writes zeros instead (the block is muted).
void Host::fxSystemParam(int p)
{
    p &= 0xFF;
    constexpr uint32_t A = 0x35DF88, B = 0x35DFA0, C = 0x35DFC0;   // step lists
    auto sys = [&](int i) -> uint32_t { return ram.u16(FX_PARAMS_SYS + 2 * i); };
    auto step = [&](uint32_t list, int i) -> uint32_t { return rom::u16(list + 2 * i); };
    auto enabled = [&](int e) -> bool { return ram.u8(FX_ENABLE0 + e) != 0; };
    auto law = [&](int32_t index) -> uint32_t { return rom::u16(rom::FX_PAN_LAW + uint32_t(index) * 2); };
    auto pan_pair = [&](uint32_t list, int i, uint32_t v) {
        set_coef(step(list, i), law(128 - int32_t(v)));
        set_coef(step(list, i + 1), law(int32_t(v)));
    };
    auto zero_pair = [&](uint32_t list, int i) {
        set_coef(step(list, i), 0);
        set_coef(step(list, i + 1), 0);
    };
    // floatToInt(uintToFloat(v) * scale / 127)
    auto level = [&](uint32_t v, float scale) -> int32_t {
        const float f = sh::fmul(sh::utof(v), scale);
        return sh::ftoi(sh::fdiv(127.0f, f));
    };

    switch (p) {
    case 0: case 1: {
        if (enabled(0)) {
            zero_pair(B, 0);
        } else {
            const uint32_t pan = sys(0);
            const float gl = sh::fdiv(127.0f, sh::itof(rom::s16(rom::FX_PAN_LAW + uint32_t(128 - int32_t(pan)) * 2)));
            const float gr = sh::fdiv(127.0f, sh::itof(rom::s16(rom::FX_PAN_LAW + pan * 2)));
            const int32_t l = sh::ftoi(sh::fmul(sh::utof(sys(1)), gl));
            set_coef(step(B, 0), uint32_t(int16_t(l) >> 3));
            const int32_t r = sh::ftoi(sh::fmul(sh::utof(sys(1)), gr));
            set_coef(step(B, 1), uint32_t(int16_t(r) >> 3));
        }
        writeSweWordListSub(2, B);
        break;
    }
    case 2:
        if (enabled(1)) zero_pair(A, 0); else pan_pair(A, 0, sys(2));
        writeSweWordListSub(2, A);
        break;
    case 3:
        set_coef(step(B, 2), uint32_t(int16_t(level(sys(3), 8192.0f)) >> 1));
        set_coef(step(B, 3), uint32_t(int16_t(level(sys(3), 8192.0f)) >> 1));
        writeSweWordListSub(2, B + 4);
        break;
    case 4:
        set_coef(step(C, 6), uint32_t(int16_t(level(sys(4), 8192.0f)) << 1));
        set_coef(rom::u16(0x35DFDA), uint32_t(int16_t(level(sys(4), 8192.0f)) << 1));
        writeSweWordListSub(1, C + 12);
        writeSweWordListSub(1, 0x35DFDA);
        break;
    case 5:
        if (enabled(2)) zero_pair(A, 2); else pan_pair(A, 2, sys(5));
        writeSweWordListSub(2, A + 4);
        break;
    case 6:
        set_coef(step(B, 4), uint32_t(int16_t(level(sys(6), 8192.0f)) >> 1));
        set_coef(step(B, 5), uint32_t(int16_t(level(sys(6), 8192.0f)) >> 1));
        writeSweWordListSub(2, B + 8);
        break;
    case 7:
        set_coef(step(C, 1), uint32_t(int16_t(level(sys(7), 8192.0f)) << 1));
        set_coef(rom::u16(0x35DFD0), uint32_t(int16_t(level(sys(7), 8192.0f)) << 1));
        writeSweWordListSub(1, C + 2);
        writeSweWordListSub(1, 0x35DFD0);
        break;
    case 8:
        set_coef(step(C, 15), uint32_t(level(sys(8), 5792.0f)));
        set_coef(rom::u16(0x35DFEA), uint32_t(level(sys(8), 5792.0f)));
        writeSweWordListSub(1, C + 30);
        writeSweWordListSub(1, 0x35DFEA);
        break;
    case 9:
        fxSystemParam(6);
        fxSystemParam(7);
        fxSystemParam(8);
        fxParamEntry(2, 9);
        break;
    case 10:
        if (enabled(3)) zero_pair(A, 4); else pan_pair(A, 4, sys(10));
        writeSweWordListSub(2, A + 8);
        break;
    case 11:
        if (enabled(4)) zero_pair(A, 6); else pan_pair(A, 6, sys(11));
        writeSweWordListSub(2, A + 12);
        break;
    case 12:
        pan_pair(A, 8, sys(12));
        writeSweWordListSub(2, A + 16);
        break;
    case 13:
        pan_pair(A, 10, sys(13));
        writeSweWordListSub(2, A + 20);
        break;
    case 14: {
        if (enabled(5)) {
            zero_pair(B, 14);
        } else {
            // v * v * level / 127^2, the 32-bit product divided as signed
            const uint32_t v = sys(14);
            const uint32_t prod = v * v * ram.u16(FX_SUB_LEVEL);
            const int32_t q = int32_t(prod) / 16129;
            set_coef(step(B, 15), uint32_t(q));
            set_coef(step(B, 14), uint32_t(q));
        }
        writeSweWordListSub(2, B + 28);
        break;
    }
    case 255:
        ram.w8(SWE_SUB_HOLD, 1);
        for (int q = 0; q <= 14; q++)
            if (q != 1)
                fxSystemParam(q);
        fxDesignFilter(1, 0, 1, 0, 64, 0, 0x35E7A8);
        fxCopyWords3(0x35E7A8, 0x35E7AE);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// flash 0x3BCD0 setSubGain8: 0 dB, word 21, into the eight output gain steps
void Host::setSubGain8()
{
    constexpr uint32_t STEPS = 0x3721DC;
    for (int i = 0; i < 8; i++)
        set_coef(rom::u16(STEPS + 2 * i), 21);
    writeSweWordListSub(8, STEPS);
}

}  // namespace fs1r::vop3host

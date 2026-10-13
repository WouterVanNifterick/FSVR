// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Shared by the insertion handlers. The insertion block's parameters live at FX_PARAMS + 0x40,
// its type bytes at FX_TYPE1 + 2 and + 3, its delay memory in areas 2..3. Parameter 9 of nearly
// every type is Dry/Wet, which also drives the insertion Thru flag.
#pragma once
#include "host.h"

namespace fs1r::vop3host::ins {

using namespace ram;

constexpr uint32_t IP = FX_PARAMS + 0x40;            // the insertion parameter block
constexpr uint32_t MOD_DELAY_TABLE = 0x371BD4;        // u16: modulation delay byte -> 0.1 ms word
constexpr uint32_t PITCH_DELAY_TABLE = 0x371DD4;      // u16: pitch change / karaoke delay byte -> 0.1 ms word
constexpr uint32_t LEVEL_CURVE = 0x3717D0;            // f32 x 128: 0..1 law
constexpr uint32_t PAN_LAW = rom::FX_PAN_LAW;         // 0x3719D0
constexpr uint32_t ER_REV = rom::FX_ER_REV;           // 0x371AD2

// the modulation group's filter steps (chorus, flanger, symphonic, delays)
constexpr uint32_t LPF_MOD = 0x35E522, LPF_MOD_C = 0x35E52E, HPF_MOD = 0x35E528, HPF_MOD_C = 0x35E534;
constexpr uint32_t EQ_MOD = 0x35E7B4, EQ_MOD_C = 0x35E7BE;
// the rotary group (rotary, 2-way rotary, tremolo, auto pan, ambience)
constexpr uint32_t LPF_ROT = 0x35E546, LPF_ROT_C = 0x35E552, HPF_ROT = 0x35E54C, HPF_ROT_C = 0x35E558;
constexpr uint32_t EQ_ROT = 0x35E7C8, EQ_ROT_C = 0x35E7D2;
// the phasers
constexpr uint32_t LPF_PH = 0x35E55E, LPF_PH_C = 0x35E56A, HPF_PH = 0x35E564, HPF_PH_C = 0x35E570;
// the wah + distortion types
constexpr uint32_t LPF_WAH = 0x35E588, LPF_WAH_C = 0x35E5A0, HPF_WAH = 0x35E58E, HPF_WAH_C = 0x35E5A6;

inline int32_t P(const Host& h, int i) { return h.ram.s16(IP + 2 * i); }
inline uint32_t PU(const Host& h, int i) { return h.ram.u16(IP + 2 * i); }
inline int32_t PLO(const Host& h, int i) { return h.ram.s8(IP + 2 * i + 1); }     // low byte, signed
inline uint32_t PLOU(const Host& h, int i) { return h.ram.u8(IP + 2 * i + 1); }   // low byte
inline uint32_t lsb(const Host& h) { return h.ram.u8(FX_TYPE1 + 3); }
inline uint32_t sys9(const Host& h) { return h.ram.u16(FX_PARAMS_SYS + 18); }
inline uint32_t half_area(const Host& h) { return ((h.ram.u32(FX_BASES + 12) - h.ram.u32(FX_BASES + 8)) >> 1) + 1; }
inline int32_t ms(Host& h, uint32_t table, int p) { return h.fxMsToSamples(rom::s16(table + 2u * PU(h, p))); }
inline void thru(Host& h, int v) { h.ram.w8(FX_INS_THRU, uint8_t(v)); }

// the Dry/Wet rule shared by most types: system parameter 9 odd -> Thru 1 and fully wet,
// else Thru 0 and the type's own parameter 9
inline uint32_t wet_thru(Host& h)
{
    if (sys9(h) & 1) { thru(h, 1); return 127; }
    thru(h, 0);
    return PU(h, 9);
}
inline uint32_t wet_only(const Host& h) { return (sys9(h) & 1) ? 127u : PU(h, 9); }

// the four Dry/Wet words: the dry gain s16(table[128 - wet]) >> shift into steps g0 and g1,
// the wet gain (through the type-scale table B) into steps x0 and x1
struct WetLayout { int g0, g1, x0, x1; };
inline void wet_words(Host& h, uint32_t list, uint32_t table, uint32_t wet, WetLayout L, int shift = 2)
{
    const uint32_t g = uint32_t(rom::s16(table + 2u * uint32_t(int32_t(128 - int32_t(wet)))) >> shift);
    h.set_coef(rom::u16(list + 2u * uint32_t(L.g0)), g);
    h.set_coef(rom::u16(list + 2u * uint32_t(L.g1)), g);
    const uint32_t x = uint32_t(h.fxMulTypeTableB(rom::s16(table + 2u * wet) >> shift));
    h.set_coef(rom::u16(list + 2u * uint32_t(L.x0)), x);
    h.set_coef(rom::u16(list + 2u * uint32_t(L.x1)), x);
}
constexpr WetLayout GGXX{1, 0, 3, 2};   // g, g, x, x in list order
constexpr WetLayout GXGX{2, 0, 3, 1};   // g, x, g, x

// parameters pa, pa+1 (LPF) and pa+2, pa+3 (HPF) of the modulation style types
inline void filters(Host& h, int p, int pa, uint32_t lpf, uint32_t lpf_copy, uint32_t hpf, uint32_t hpf_copy)
{
    if (p == pa || p == pa + 1) {
        h.fxDesignFilter(1, 1, 2, P(h, pa), P(h, pa + 1), 0, lpf);
        h.fxCopyWords3(lpf, lpf_copy);
    } else {
        h.fxDesignFilter(1, 2, 3, P(h, pa + 2), P(h, pa + 3), 0, hpf);
        h.fxCopyWords3(hpf, hpf_copy);
    }
}
// parameters 10, 11, 12: the parametric EQ band
inline void eq_band(Host& h, uint32_t steps, uint32_t copy)
{
    h.fxDesignFilter(2, 1, 10, P(h, 10), P(h, 11), P(h, 12), steps);
    h.fxCopyWords5(steps, copy);
}

}  // namespace fs1r::vop3host::ins

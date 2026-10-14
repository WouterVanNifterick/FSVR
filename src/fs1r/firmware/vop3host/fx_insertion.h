// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Shared by the insertion handlers. The insertion's delay memory is areas 2..3. Parameter 9 of
// nearly every type is Dry/Wet.
#pragma once
#include "host.h"

namespace fs1r::vop3host::ins {

using namespace rom::ins;

inline int32_t P(const Host& h, int i) { return h.param[INSERTION][i]; }
inline uint32_t PU(const Host& h, int i) { return h.param[INSERTION][i]; }
inline int32_t PLO(const Host& h, int i) { return int8_t(h.param[INSERTION][i]); }     // low byte, signed
inline uint32_t PLOU(const Host& h, int i) { return (h.param[INSERTION][i] & 0xFF); }   // low byte
inline uint32_t lsb(const Host& h) { return h.code[INSERTION].lsb; }
inline uint32_t half_area(const Host& h) { return ((AREA[3] - AREA[2]) >> 1) + 1; }
inline int32_t ms(Host& h, const int16_t* table, int p) { return h.fxMsToSamples(table[PU(h, p)]); }

// the four Dry/Wet words: the dry gain s16(table[128 - wet]) >> shift into steps g0 and g1,
// the wet gain (through the type-scale table B) into steps x0 and x1
struct WetLayout { int g0, g1, x0, x1; };
inline void wet_words(Host& h, const uint16_t* list, const int16_t* table, uint32_t wet, WetLayout L, int shift = 2)
{
    const uint32_t g = uint32_t(table[uint32_t(int32_t(128 - int32_t(wet)))] >> shift);
    h.coef[list[uint32_t(L.g0)]] = g;
    h.coef[list[uint32_t(L.g1)]] = g;
    const uint32_t x = uint32_t(h.fxMulTypeTableB(table[wet] >> shift));
    h.coef[list[uint32_t(L.x0)]] = x;
    h.coef[list[uint32_t(L.x1)]] = x;
}
constexpr WetLayout GGXX{1, 0, 3, 2};   // g, g, x, x in list order
constexpr WetLayout GXGX{2, 0, 3, 1};   // g, x, g, x

// parameters pa, pa+1 (LPF) and pa+2, pa+3 (HPF) of the modulation style types
inline void filters(Host& h, int p, int pa, const uint16_t* lpf, const uint16_t* lpf_copy, const uint16_t* hpf, const uint16_t* hpf_copy)
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
inline void eq_band(Host& h, const uint16_t* steps, const uint16_t* copy)
{
    h.fxDesignFilter(2, 1, 10, P(h, 10), P(h, 11), P(h, 12), steps);
    h.fxCopyWords5(steps, copy);
}

}  // namespace fs1r::vop3host::ins

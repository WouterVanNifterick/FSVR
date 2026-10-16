// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Shared by the effect handlers. A handler takes its window w and a parameter p; the variation
// and the insertion run the same handler over their own steps where they share a type.
#pragma once
#include "effects/host/effect_host.h"

namespace fsvr::vop3host {

namespace var = rom::var;
namespace ins = rom::ins;
namespace rev = rom::rev;

// Q15 words
constexpr uint16_t UNITY = 0x7FFF, HALF = 0x4000, QUARTER = 0x2000, MINUS_ONE = 0x8000;
constexpr int SQRT_HALF = 23170;

// half a block's delay memory, plus one: where its second channel's delays start
constexpr int32_t HALF_AREA = ((AREA[3] - AREA[2]) >> 1) + 1;

// the wah's cutoff word, 3052 v / 16 above a base
inline int32_t wahCutoff(uint32_t v, int32_t base) { return int32_t((3052u * (v & 0xFFFF)) >> 4) + base; }

}  // namespace fsvr::vop3host

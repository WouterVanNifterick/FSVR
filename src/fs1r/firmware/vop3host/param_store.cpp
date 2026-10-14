// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A performance byte as a parameter: its value, clamped to the range its descriptor gives.
// A descriptor is 16 bytes in the EPROM: +6 minimum, +8 maximum, +12 type. The effect
// parameters take the descriptor table of the current effect type.
#include "host.h"

namespace fs1r::vop3host {

// 0x34478
uint32_t Host::paramDescAddr(uint32_t index)
{
    // the table of the current type
    auto typed = [&](uint32_t type_byte, uint32_t tables, uint32_t first) -> uint32_t {
        return rom::u32(tables + (perf[type_byte] << 2)) + ((index - first) << 4);
    };
    if (index >= 0x50 && index <= 0x67) return typed(168, rom::PARAM_DESC_REVERB, 0x50);
    if (index >= 0x68 && index <= 134) return typed(171, rom::PARAM_DESC_VARIATION, 0x68);
    if (index >= 136 && index <= 166) return typed(175, rom::PARAM_DESC_INSERTION, 136);
    return rom::PARAM_DESC_COMMON + (index << 4);
}

// 0x34B00: the stored bytes as a number, by descriptor type: 8 is one byte, 13 two 7-bit bytes.
// The effect bytes use no other type.
int32_t Host::readStoredParam(uint32_t desc, uint32_t index)
{
    switch (rom::u8(desc + 12) & 0x3F) {
    case 8: return perf[index];
    case 13: return (perf[index] << 7) + perf[index + 1];
    default: return 0;
    }
}

// 0x30390
int32_t Host::readParameter(uint32_t id)
{
    const uint32_t index = id & 0xFF, desc = paramDescAddr(index);
    int32_t v = readStoredParam(desc, index);
    if (v > rom::s16(desc + 8)) v = rom::s16(desc + 8);
    if (v < rom::s16(desc + 6)) v = rom::s16(desc + 6);
    return v;
}

}  // namespace fs1r::vop3host

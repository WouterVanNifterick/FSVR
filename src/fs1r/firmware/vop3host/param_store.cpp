// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A performance byte as a parameter: its value, clamped to the range its descriptor gives.
// A descriptor is 16 bytes in the EPROM: +6 minimum, +8 maximum, +12 type. The effect
// parameters take the descriptor table of the current effect type.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

// 0x34478
uint32_t Host::paramDescAddr(uint32_t index)
{
    // the table of the current type, clamped to the last type
    auto typed = [&](uint32_t type_byte, uint32_t last, uint32_t tables, uint32_t first) -> uint32_t {
        uint32_t t = ram.u8(PERFORMANCE + type_byte);
        if (t > last) t = last;
        return rom::u32(tables + (t << 2)) + ((index - first) << 4);
    };
    if (!(0x50 > index) && !(index > 0x67)) return typed(168, 16, rom::PARAM_DESC_REVERB, 0x50);
    if (!(0x68 > index) && !(index > 134)) return typed(171, 28, rom::PARAM_DESC_VARIATION, 0x68);
    if (!(136 > index) && !(index > 166)) return typed(175, 40, rom::PARAM_DESC_INSERTION, 136);
    return rom::PARAM_DESC_COMMON + (index << 4);
}

// 0x34B00: the stored bytes as a number, by descriptor type
//   8  one byte, 7 bits               9  bit 0 of the first byte above 7 bits of the second
//   13 two 7-bit bytes, or (with a negative minimum) a signed 14-bit pair like type 45
//   14 two full bytes                 40 a signed 7-bit byte        41 a signed 8-bit pair
int32_t Host::readStoredParam(uint32_t desc, uint32_t addr)
{
    const uint32_t b0 = ram.u8(addr), b1 = ram.u8(addr + 1);
    switch (rom::u8(desc + 12) & 0x3F) {
    case 8: return int32_t(b0 & 0x7F);
    case 9: return int32_t(sh::u8((b1 & 0x7F) + ((b0 & 1) << 7)));
    case 13:
        if (rom::s16(desc + 6) >= 0)
            return int32_t(((b0 & 0x7F) << 7) + (b1 & 0x7F));
        [[fallthrough]];
    case 45: return int32_t(b1 + ((b0 & 31) << 8) - ((b0 & 32) << 8));
    case 14: return int32_t(b1 | (uint32_t(sh::s16(b0)) << 8));
    case 40: return sh::s8((b0 & 0x3F) - (b0 & 0x40));
    case 41: return sh::s8(uint32_t(sh::muls_w(-128, int32_t(b0 & 1))) + (b1 & 0x7F));
    default: return 0;
    }
}

// 0x30390
int32_t Host::readParameter(uint32_t id)
{
    const uint32_t index = id & 0xFF, desc = paramDescAddr(index);
    if (desc == 0)
        return -1;
    const uint32_t type = rom::u8(desc + 12);
    int32_t v = sh::s16(uint32_t(readStoredParam(desc, PERFORMANCE + index)));
    if ((type & 15) == 14)
        return v;
    if (v > rom::s16(desc + 8)) v = rom::s16(desc + 8);
    if (!(sh::s16(uint32_t(v)) >= rom::s16(desc + 6))) v = rom::s16(desc + 6);
    return v;
}

}  // namespace fs1r::vop3host

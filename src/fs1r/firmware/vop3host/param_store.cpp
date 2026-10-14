// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A performance byte as a parameter: its value, clamped to the range its descriptor gives.
// The effect parameters take the descriptors of the current effect type.
#include "host.h"

namespace fs1r::vop3host {

// 0x34478
const rom::ParamRange& Host::descriptor(uint32_t index) const
{
    if (index >= 80 && index <= 103) return rom::REVERB_PARAMS[perf[168]][index - 80];
    if (index >= 104 && index <= 134) return rom::VARIATION_PARAMS[perf[171]][index - 104];
    if (index >= 136 && index <= 166) return rom::INSERTION_PARAMS[perf[175]][index - 136];
    return rom::COMMON_PARAMS[index];
}

// 0x30390, and 0x34B00 for the stored bytes: one, or two of seven bits with the high one first
int32_t Host::readParameter(uint32_t id)
{
    const uint32_t index = id & 0xFF;
    const rom::ParamRange& d = descriptor(index);
    int32_t v = d.bytes == 2 ? (perf[index] << 7) + perf[index + 1] : d.bytes == 1 ? perf[index] : 0;
    if (v > d.max) v = d.max;
    if (v < d.min) v = d.min;
    return v;
}

}  // namespace fs1r::vop3host

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A performance byte as a parameter: its value, clamped to the range its descriptor gives.
// The effect parameters take the descriptors of the current effect type.
#include "effect_host.h"

namespace fs1r::vop3host {

const rom::ParamRange& EffectHost::descriptor(uint32_t index) const
{
    if (index >= REVERB_PARAM_BYTES    && index < REVERB_PARAM_BYTES    + 24) return rom::REVERB_PARAMS   [perf[REVERB_TYPE]   ][index - REVERB_PARAM_BYTES   ];
    if (index >= VARIATION_PARAM_BYTES && index < VARIATION_PARAM_BYTES + 31) return rom::VARIATION_PARAMS[perf[VARIATION_TYPE]][index - VARIATION_PARAM_BYTES];
    if (index >= INSERTION_PARAM_BYTES && index < INSERTION_PARAM_BYTES + 31) return rom::INSERTION_PARAMS[perf[INSERTION_TYPE]][index - INSERTION_PARAM_BYTES];
    return rom::COMMON_PARAMS[index];
}

int32_t EffectHost::readParameter(uint32_t id)
{
    const uint32_t index = id & 0xFF;
    const rom::ParamRange& d = descriptor(index);
    int32_t v = d.bytes == 2 ? (perf[index] << 7) + perf[index + 1] : d.bytes == 1 ? perf[index] : 0;
    if (v > d.max) v = d.max;
    if (v < d.min) v = d.min;
    return v;
}

}  // namespace fs1r::vop3host

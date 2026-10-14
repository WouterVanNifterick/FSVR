// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// LFO register helpers and single-word stores.
#include "effect_host.h"

namespace fs1r::vop3host {

// the speed word, keeping bits 10-11
void EffectHost::setLfoSpeed(int v, int lfo)
{
    uint16_t& w = lfoReg[0][lfo];
    w = rom::LFO_SPEED[v] | (w & 3072);
    writeLfo(LFO_SPEED, lfo);
}

// the start phase, the low five bits of LFO_WAVE
void EffectHost::setLfoPhase(int v, int lfo)
{
    uint16_t& w = lfoReg[1][lfo];
    w = rom::LFO_PHASE[v] | (w & 0xFFE0);
    writeLfo(LFO_WAVE, lfo);
}

// v * 32768 / 127, 0x7FFF at 127
void EffectHost::setLfoDepth(int v, int lfo)
{
    const uint32_t b = uint32_t(v) & 0xFF;
    const uint32_t w = b == 127 ? 0x7FFF : (b << 15) / 127;
    lfoReg[2][lfo] = w;
    writeLfo(LFO_DEPTH, lfo);
}

// a phase difference, (v - 64) * scale
void EffectHost::storeBipolar(int scale, int v, const uint16_t* step)
{
    coef[step[0]] = scale * ((v & 0xFF) - 64);
    writeCoefs(1, step);
}

void EffectHost::storeModePair(int mode, const uint16_t* steps)
{
    switch (mode & 0xFF) {
    case 1: coef[steps[0]] = 1536; coef[steps[1]] = 0xE000; break;
    case 2: coef[steps[0]] = 0x7C00; coef[steps[1]] = 0; break;
    }
    writeCoefs(2, steps);
}

// v * 512 / 127
void EffectHost::storeDepth(int v, const uint16_t* step)
{
    coef[step[0]] = ((uint32_t(v) & 0xFF) << 9) / 127;
    writeCoefs(1, step);
}

// -(v * 32768 / 127) for a signed byte, 0x7FFF at -127
void EffectHost::storeNegDepth(int v, const uint16_t* step)
{
    const int32_t b = int8_t(v);
    const uint32_t w = b == -127 ? 0x7FFF : uint32_t(-(b * 32768 / 127));
    coef[step[0]] = w;
    writeCoefs(1, step);
}

// t, t and 0x8000 - t
void EffectHost::storeRelease(int v, const uint16_t* steps)
{
    const int32_t t = rom::COMPLEMENT[v];
    coef[steps[1]] = t;
    coef[steps[0]] = t;
    coef[steps[2]] = -32768 - t;
    writeCoefs(3, steps);
}

// the octave table word for (v - 80) % 6 shifted right by
// (v - 80) / 6; 0x7FFF below 80
void EffectHost::storeCompThreshold(int v, const uint16_t* step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 80;
    uint32_t w = 0x7FFF;
    if (d >= 0)
        w = uint32_t(rom::OCTAVE[uint32_t(d % 6)]) >> (d / 6);
    coef[step[0]] = w;
    writeCoefs(1, step);
}

// the same from 56, negated; 0x8000 below 56
void EffectHost::storeGateThreshold(int v, const uint16_t* step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 56;
    uint32_t w = 0x8000;
    if (d >= 0)
        w = uint32_t(-int32_t(uint32_t(rom::OCTAVE[uint32_t(d % 6)]) >> (d / 6)));
    coef[step[0]] = w;
    writeCoefs(1, step);
}

// eight words of a 16-byte table row
void EffectHost::storeRatio(int row, const uint16_t* steps)
{
    const uint16_t* r = rom::COMP_RATIO + 8 * (uint32_t(row) & 0xFF);
    for (uint32_t i = 0; i < 8; i++)
        coef[steps[i]] = r[i];
    writeCoefs(8, steps);
}

}  // namespace fs1r::vop3host

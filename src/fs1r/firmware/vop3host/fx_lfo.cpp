// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// LFO register helpers and single-word stores.
#include "host.h"

namespace fs1r::vop3host {

namespace {
}  // namespace

// 0x3A3070 fxLfoSpeed(v, lfo): speed word from FX_LFO_SPEED, keeping bits 10-11
void Host::fxLfoSpeed(int v, int lfo)
{
    uint16_t& w = lfoReg[0][lfo];
    w = rom::LFO_SPEED[uint32_t(v) & 0xFF] | (w & 3072);
    writeSwe24Sub(uint32_t(int8_t(lfo)));
}

// 0x3A30C2 fxLfoPhase(v, lfo): low 5 bits of the register 0x25 word from FX_LFO_PHASE
void Host::fxLfoPhase(int v, int lfo)
{
    uint16_t& w = lfoReg[1][lfo];
    w = rom::LFO_PHASE[uint32_t(v) & 0xFF] | (w & 0xFFE0);
    writeSwe25Sub(uint32_t(int8_t(lfo)));
}

// 0x3A3128 fxLfoDepth(v, lfo): v * 32768 / 127, 0x7FFF at 127
void Host::fxLfoDepth(int v, int lfo)
{
    const uint32_t b = uint32_t(v) & 0xFF;
    const uint32_t w = b == 127 ? 0x7FFF : (b << 15) / 127;
    lfoReg[2][lfo] = w;
    writeSwe26Sub(uint32_t(int8_t(lfo)));
}

// 0x3A31CC / 0x3A31EA: (v - 64) * 546 and (v - 64) * 273
void Host::fxStoreBipolar376(int v, const uint16_t* step)
{
    coef[step[0]] = uint32_t(546 * ((v & 0xFF) - 64));
    writeSweWordListSub(1, step);
}
void Host::fxStoreBipolar500(int v, const uint16_t* step)
{
    coef[step[0]] = uint32_t(273 * ((v & 0xFF) - 64));
    writeSweWordListSub(1, step);
}

// 0x3A3208 fxStoreModePair(mode, steps)
void Host::fxStoreModePair(int mode, const uint16_t* steps)
{
    switch (mode & 0xFF) {
    case 1: coef[steps[0]] = 1536; coef[steps[1]] = 0xE000; break;
    case 2: coef[steps[0]] = 0x7C00; coef[steps[1]] = 0; break;
    }
    writeSweWordListSub(2, steps);
}

// 0x3A328E fxStoreUdiv(v, step): v * 512 / 127
void Host::fxStoreUdiv(int v, const uint16_t* step)
{
    coef[step[0]] = ((uint32_t(v) & 0xFF) << 9) / 127;
    writeSweWordListSub(1, step);
}

// 0x3A32B0 fxStoreNegMul(v, step): -(v * 32768 / 127) for a signed byte, 0x7FFF at -127
void Host::fxStoreNegMul(int v, const uint16_t* step)
{
    const int32_t b = int8_t(v);
    const uint32_t w = b == -127 ? 0x7FFF : uint32_t(-(b * 32768 / 127));
    coef[step[0]] = w;
    writeSweWordListSub(1, step);
}

// 0x3A5124 fxWriteComplementPair(v, steps): t, t and 0x8000 - t
void Host::fxWriteComplementPair(int v, const uint16_t* steps)
{
    const int32_t t = rom::COMPLEMENT[uint32_t(v) & 0xFFFF];
    coef[steps[1]] = uint32_t(t);
    coef[steps[0]] = uint32_t(t);
    coef[steps[2]] = uint32_t(-32768 - t);
    writeSweWordListSub(3, steps);
}

// 0x3A5168 fxWriteWord(v, step): the octave table word for (v - 80) % 6 shifted right by
// (v - 80) / 6; 0x7FFF below 80
void Host::fxWriteWord(int v, const uint16_t* step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 80;
    uint32_t w = 0x7FFF;
    if (d >= 0)
        w = uint32_t(rom::OCTAVE[uint32_t(d % 6)]) >> (d / 6);
    coef[step[0]] = w;
    writeSweWordListSub(1, step);
}

// 0x3A51E4 fxWriteNegated(v, step): the same from 56, negated; 0x8000 below 56
void Host::fxWriteNegated(int v, const uint16_t* step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 56;
    uint32_t w = 0x8000;
    if (d >= 0)
        w = uint32_t(-int32_t(uint32_t(rom::OCTAVE[uint32_t(d % 6)]) >> (d / 6)));
    coef[step[0]] = w;
    writeSweWordListSub(1, step);
}

// 0x3A522E fxWriteRow8(row, steps): eight words of a 16-byte table row
void Host::fxWriteRow8(int row, const uint16_t* steps)
{
    const uint16_t* r = rom::COMP_RATIO + 8 * (uint32_t(row) & 0xFF);
    for (uint32_t i = 0; i < 8; i++)
        coef[steps[i]] = r[i];
    writeSweWordListSub(8, steps);
}

}  // namespace fs1r::vop3host

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// LFO register helpers and single-word stores.
#include "host.h"

namespace fs1r::vop3host {

namespace {
constexpr uint32_t COMPLEMENT_TABLE = 0x36F6A4;   // s16 per byte value (fxWriteComplementPair)
constexpr uint32_t OCTAVE_TABLE = 0x36F670;       // s16 x 6: one octave in six steps (fxWriteWord/Negated)
constexpr uint32_t ROW8_TABLE = 0x36F5F0;         // u16 rows of 8 (fxWriteRow8)
}  // namespace

// 0x3A3070 fxLfoSpeed(v, lfo): speed word from FX_LFO_SPEED, keeping bits 10-11
void Host::fxLfoSpeed(int v, int lfo)
{
    uint16_t& w = lfoReg[0][lfo];
    w = rom::u16(rom::FX_LFO_SPEED + 2u * (uint32_t(v) & 0xFF)) | (w & 3072);
    writeSwe24Sub(uint32_t(int8_t(lfo)));
}

// 0x3A30C2 fxLfoPhase(v, lfo): low 5 bits of the register 0x25 word from FX_LFO_PHASE
void Host::fxLfoPhase(int v, int lfo)
{
    uint16_t& w = lfoReg[1][lfo];
    w = rom::u16(rom::FX_LFO_PHASE + 2u * (uint32_t(v) & 0xFF)) | (w & 0xFFE0);
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
void Host::fxStoreBipolar376(int v, uint32_t step)
{
    coef[rom::u16(step)] = uint32_t(546 * ((v & 0xFF) - 64));
    writeSweWordListSub(1, step);
}
void Host::fxStoreBipolar500(int v, uint32_t step)
{
    coef[rom::u16(step)] = uint32_t(273 * ((v & 0xFF) - 64));
    writeSweWordListSub(1, step);
}

// 0x3A3208 fxStoreModePair(mode, steps)
void Host::fxStoreModePair(int mode, uint32_t steps)
{
    switch (mode & 0xFF) {
    case 1: coef[rom::u16(steps)] = 1536; coef[rom::u16(steps + 2)] = 0xE000; break;
    case 2: coef[rom::u16(steps)] = 0x7C00; coef[rom::u16(steps + 2)] = 0; break;
    }
    writeSweWordListSub(2, steps);
}

// 0x3A328E fxStoreUdiv(v, step): v * 512 / 127
void Host::fxStoreUdiv(int v, uint32_t step)
{
    coef[rom::u16(step)] = ((uint32_t(v) & 0xFF) << 9) / 127;
    writeSweWordListSub(1, step);
}

// 0x3A32B0 fxStoreNegMul(v, step): -(v * 32768 / 127) for a signed byte, 0x7FFF at -127
void Host::fxStoreNegMul(int v, uint32_t step)
{
    const int32_t b = int8_t(v);
    const uint32_t w = b == -127 ? 0x7FFF : uint32_t(-(b * 32768 / 127));
    coef[rom::u16(step)] = w;
    writeSweWordListSub(1, step);
}

// 0x3A5124 fxWriteComplementPair(v, steps): t, t and 0x8000 - t
void Host::fxWriteComplementPair(int v, uint32_t steps)
{
    const int32_t t = rom::s16(COMPLEMENT_TABLE + 2u * (uint32_t(v) & 0xFFFF));
    coef[rom::u16(steps + 2)] = uint32_t(t);
    coef[rom::u16(steps)] = uint32_t(t);
    coef[rom::u16(steps + 4)] = uint32_t(-32768 - t);
    writeSweWordListSub(3, steps);
}

// 0x3A5168 fxWriteWord(v, step): the octave table word for (v - 80) % 6 shifted right by
// (v - 80) / 6; 0x7FFF below 80
void Host::fxWriteWord(int v, uint32_t step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 80;
    uint32_t w = 0x7FFF;
    if (d >= 0)
        w = uint32_t(rom::s16(OCTAVE_TABLE + 2u * uint32_t(d % 6))) >> (d / 6);
    coef[rom::u16(step)] = w;
    writeSweWordListSub(1, step);
}

// 0x3A51E4 fxWriteNegated(v, step): the same from 56, negated; 0x8000 below 56
void Host::fxWriteNegated(int v, uint32_t step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 56;
    uint32_t w = 0x8000;
    if (d >= 0)
        w = uint32_t(-int32_t(uint32_t(rom::s16(OCTAVE_TABLE + 2u * uint32_t(d % 6))) >> (d / 6)));
    coef[rom::u16(step)] = w;
    writeSweWordListSub(1, step);
}

// 0x3A522E fxWriteRow8(row, steps): eight words of a 16-byte table row
void Host::fxWriteRow8(int row, uint32_t steps)
{
    const uint32_t r = ROW8_TABLE + 16u * (uint32_t(row) & 0xFF);
    for (uint32_t i = 0; i < 8; i++)
        coef[rom::u16(steps + 2 * i)] = rom::u16(r + 2 * i);
    writeSweWordListSub(8, steps);
}

}  // namespace fs1r::vop3host

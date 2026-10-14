// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// What the handlers share: their arithmetic, the LFO registers, the stores of one parameter into its
// steps, dry and wet, the shelving pair, and every parameter at once.
#include "fx_common.h"

namespace fs1r::vop3host {

// ---- arithmetic the handlers share ----
int32_t EffectHost::bipolar500(int v) { return 500 * (v - 64); }
int32_t EffectHost::bipolar376(int v) { return 376 * (v - 64); }
int32_t EffectHost::lessDiffusion(int x, int v)
{
    const int32_t d = x - int32_t(rom::DIFFUSION[v]);
    return d > 0 ? d : 1;
}
int32_t EffectHost::toSamples(int v) { return int32_t((19660u * (uint32_t(v) & 0xFFFF)) >> 12); }
int32_t EffectHost::scaleDelay(int v) { return int16_t((31457u * (uint32_t(v) & 0xFFFF)) >> 16); }
// x at the level the window's type runs at, in 64ths
int32_t EffectHost::typeGain(int window, int x)
{
    const TypeCode c = code[window];
    const int level = window == REVERB ? rom::TYPE_LEVEL[c.lsb]
                    : window == INSERTION ? rom::TYPE_SCALE_INSERTION[18 * c.msb + c.lsb]
                    : rom::TYPE_SCALE_DELAY[14 * c.msb + c.lsb];
    return (level * int16_t(x)) >> 6;
}

// the speed word, keeping bits 10-11
void EffectHost::setLfoSpeed(int v, int lfo)
{
    uint16_t& w = lfoReg[0][lfo];
    w = rom::LFO_SPEED[v] | (w & 0x0C00);
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
    const uint32_t w = b == 127 ? UNITY : (b << 15) / 127;
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
    const uint32_t w = b == -127 ? UNITY : uint32_t(-(b * 32768 / 127));
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
    uint32_t w = UNITY;
    if (d >= 0)
        w = uint32_t(rom::OCTAVE[uint32_t(d % 6)]) >> (d / 6);
    coef[step[0]] = w;
    writeCoefs(1, step);
}

// the same from 56, negated; 0x8000 below 56
void EffectHost::storeGateThreshold(int v, const uint16_t* step)
{
    const int32_t d = int32_t(uint32_t(v) & 0xFF) - 56;
    uint32_t w = MINUS_ONE;
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

// a drive word, 704 + 32063 v / 127, into two steps, and into the third a gain from it (fixed
// from v > 10 on), scaled by the level curve and the square root of 2, at least 1 unless 0
void EffectHost::storeDrive(int v, int index, const uint16_t* steps)
{
    const int32_t drive = 704 + int32_t(32063u * (uint32_t(v) & 0xFFFF)) / 127;
    coef[steps[1]] = drive;
    coef[steps[2]] = drive;
    const float f3 = float(int16_t(drive));
    double d;
    if ((uint32_t(v) & 0xFFFF) > 10)
        d = 21.0327;
    else
        d = double(f3 * f3 / 495616.0f);
    const float f10 = float(d);
    const float f11 = 8192.0f * rom::LEVEL_CURVE[index];
    float x = f11 / f10 * 0.5f;
    x = float(double(x) * 1.414213562373095);
    uint32_t word;
    if (0.0 == double(x))
        word = uint32_t(int16_t(int(x)));
    else if (1.0 > double(x))
        word = 1;
    else
        word = uint32_t(int16_t(int(x)));
    coef[steps[3]] = word;
    writeCoefs(3, steps + 1);
}

// (0x8000 - t) / 2 twice, then t
void EffectHost::storeCompAttack(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[v];
    const uint32_t w = (0x8000 - t) >> 1;
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = t;
    writeCoefs(3, steps);
}

// (-0x8000 - t) * 32 twice, then t
void EffectHost::storeGateAttack(int v, const uint16_t* steps)
{
    const int32_t t = rom::MIRROR[v];
    const uint32_t w = (-32768 - t) * 32;
    coef[steps[1]] = w;
    coef[steps[0]] = w;
    coef[steps[2]] = t;
    writeCoefs(3, steps);
}

// t twice, then (-0x8000 - t) * 2
void EffectHost::storeWahRelease(int v, const uint16_t* steps)
{
    const int32_t t = rom::COMPLEMENT[v];
    coef[steps[1]] = t;
    coef[steps[0]] = t;
    coef[steps[2]] = (-32768 - t) * 2;
    writeCoefs(3, steps);
}

// ---- shared by the handlers ----

void EffectHost::applyAll(int w, Handler handler, std::initializer_list<int> ps)
{
    hold = true;
    for (int p : ps)
        (this->*handler)(w, p);
    hold = false;
}

// Dry and wet from the insertion's Dry/Wet parameter; the other blocks run fully wet. The dry
// level, law[128 - wet] >> shift, goes into steps 0 and 1 and the wet level at the window's type
// gain into 2 and 3, or interleaved: dry, wet, dry, wet. Then the four are written.
void EffectHost::dryWet(int w, const uint16_t* s, bool interleaved, const int16_t* law, int shift)
{
    const uint32_t wet = w == INSERTION ? param[INSERTION][DRY_WET] : 127;
    const uint32_t dry = law[128 - wet] >> shift, x = typeGain(w, law[wet] >> shift);
    coef[s[0]] = dry;
    coef[s[interleaved ? 2 : 1]] = dry;
    coef[s[interleaved ? 1 : 2]] = x;
    coef[s[3]] = x;
    writeCoefs(4, s);
}

// parameters first and first + 1, the low shelf's frequency and gain, and first + 2 and first + 3,
// the high shelf's; each into its steps and copied to the other channel's
void EffectHost::shelves(int w, int p, int first, const Shelves& s)
{
    const uint16_t* P = param[w];
    const int words = s.order == 1 ? 3 : 5;
    if (p == first || p == first + 1) {
        designFilter(s.order, 1, LOW_SHELF1, P[first], P[first + 1], 0, s.low);
        copyCoefs(words, s.low, s.lowCopy);
    } else {
        designFilter(s.order, s.highScale, HIGH_SHELF1, P[first + 2], P[first + 3], 0, s.high);
        copyCoefs(words, s.high, s.highCopy);
    }
}

void EffectHost::setLfoWave(int lfo, int wave)
{
    lfoReg[1][lfo] = uint16_t((lfoReg[1][lfo] & 0xFF1F) | wave << 5);
}

}  // namespace fs1r::vop3host

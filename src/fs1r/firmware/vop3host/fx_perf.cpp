// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The performance into the effect chip: applyChanges, which the effect task runs for
// every message posted to it, and the functions it reaches. Mask 0x8000 is a performance load.
//
// The caches hold the parameters as read from the performance; the blocks the per-type
// handlers read (param[kind]) are filled from them here.
#include "effect_host.h"
#include <algorithm>

namespace fs1r::vop3host {

using namespace rom::mix;

namespace {


}  // namespace

// flash 0x3AC88 applyChanges
//   0x8000  read the performance into the cache, then as 0x80
//   0x80    everything: the four windows with their parameters, the mixer, the master EQ
//   0x40    reverb type            0x20  dirty reverb parameters
//   0x10    variation type         0x08  dirty variation parameters
//   0x04    insertion type         0x02  dirty insertion parameters, with their controller offsets
//   0x200   dirty mixer parameters (handlers at 0x3F0B0)
//   0x100   dirty master EQ parameters (handlers at 0x3F104)
void EffectHost::applyChanges(uint32_t mask)
{
    // the reverb's type map says which window the type runs in: the reverb's, or the delay's
    auto is_delay = [&](uint32_t type) -> bool { return rom::TYPE_MAP_REVERB[type][0] != 0; };

    if ((mask & 0x80) || (mask & 0x8000)) {
        if (mask & 0x8000) {
            for (uint32_t i = 0; i < 16; i++) {
                if (i < 8)
                    reverbCache[i] = uint32_t(readParameter(0x50 + 2 * i));
                else
                    reverbCache[i] = uint32_t(readParameter(0x58 + i));
                variationCache[i] = uint32_t(readParameter(0x68 + 2 * i));
                insertionCache[i] = uint32_t(readParameter(136 + 2 * i));
            }
            for (uint32_t i = 0; i < 10; i++)
                eqCache[i] = uint32_t(readParameter(180 + i));
        }
        reverbCache[9] = 127;
        variationCache[9] = 127;
        setMute(0, 1);
        setMute(2, 1);
        setMute(3, 1);
        setMute(4, 1);
        for (uint32_t i = 0; i < 16; i++)
            controlOffset[i] = 0;
        const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
        mixParam[4] = 90;
        applyParam(MIXER, 4);
        int delay_entry;
        if (!is_delay(rev)) {
            std::copy_n(reverbCache, 16, param[REVERB]);
            code[REVERB].lsb = rom::TYPE_MAP_REVERB[rev][1];
            applyParam(REVERB, 254);
            code[REVERB_DELAY].msb = 0;
            code[REVERB_DELAY].lsb = 1;
            delay_entry = 255;
        } else {
            std::copy_n(reverbCache, 16, param[REVERB_DELAY]);
            code[REVERB].lsb = 0;
            applyParam(REVERB, 255);
            code[REVERB_DELAY].msb = 8;
            code[REVERB_DELAY].lsb = rom::TYPE_MAP_REVERB[rev][1];
            delay_entry = 254;
        }
        applyParam(REVERB_DELAY, delay_entry);

        const uint32_t var = perf[171];
        std::copy_n(variationCache, 16, param[VARIATION]);
        code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
        code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
        applyParam(VARIATION, 254);

        const uint32_t ins = perf[175];
        std::copy_n(insertionCache, 16, param[INSERTION]);
        code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
        code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
        applyParam(INSERTION, 254);

        // the mixer: reverb pan and return go to the words of the reverb or of the delay block
        const uint32_t rtype = perf[168];
        const uint32_t pan = perf[169];
        if (!is_delay(rtype)) {
            mixParam[0] = pan;
            applyParam(MIXER, 0);
            mixParam[10] = 64;
        } else {
            mixParam[0] = 64;
            applyParam(MIXER, 0);
            mixParam[10] = pan;
        }
        applyParam(MIXER, 10);
        const uint32_t ret = perf[170];
        if (!is_delay(rtype)) {
            mixParam[1] = ret;
            applyParam(MIXER, 1);
            mixParam[3] = 0;
        } else {
            mixParam[1] = 0;
            applyParam(MIXER, 1);
            mixParam[3] = ret;
        }
        applyParam(MIXER, 3);
        mixParam[11] = perf[172];
        applyParam(MIXER, 11);
        mixParam[6] = perf[173];
        applyParam(MIXER, 6);
        mixParam[8] = perf[174];
        applyParam(MIXER, 8);
        mixParam[5] = perf[176];
        applyParam(MIXER, 5);
        mixParam[14] = perf[16];
        setMasterLevel();
        setInsertionToReverb(perf[177]);
        setInsertionToVariation(perf[178]);
        setInsertionLevel(perf[179]);
        setIndividualOut(perf[20]);
        copyEqParams();
        applyParam(MASTER_EQ, 0);
        applyParam(MASTER_EQ, 8);
        applyParam(MASTER_EQ, 16);
        uploadWindow(REVERB);
        uploadWindow(INSERTION);
        uploadWindow(REVERB_DELAY);
        uploadWindow(VARIATION);
        setMute(0, 0);
        setMute(2, 0);
        setMute(3, 0);
        if ((uint32_t(readParameter(171)) & 0xFF) != 0)
            setMute(4, 0);
    }

    if (mask & 0x40) {
        setMute(0, 1);
        setMute(3, 1);
        const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
        mixParam[4] = 90;
        applyParam(MIXER, 4);
        if (!is_delay(rev)) {
            code[REVERB].lsb = rom::TYPE_MAP_REVERB[rev][1];
            applyParam(REVERB, 255);
            code[REVERB_DELAY].msb = 0;
            code[REVERB_DELAY].lsb = 1;
            applyParam(REVERB_DELAY, 255);
            mixParam[0] = uint32_t(readParameter(169)) & 0xFF;
            applyParam(MIXER, 0);
            mixParam[10] = 64;
            applyParam(MIXER, 10);
            mixParam[1] = uint32_t(readParameter(170)) & 0xFF;
            applyParam(MIXER, 1);
            mixParam[3] = 0;
        } else {
            code[REVERB].lsb = 0;
            applyParam(REVERB, 255);
            code[REVERB_DELAY].msb = 8;
            code[REVERB_DELAY].lsb = rom::TYPE_MAP_REVERB[rev][1];
            applyParam(REVERB_DELAY, 255);
            const uint32_t pan = uint32_t(readParameter(169)) & 0xFF;
            mixParam[0] = 64;
            applyParam(MIXER, 0);
            mixParam[10] = pan;
            applyParam(MIXER, 10);
            const uint32_t ret = uint32_t(readParameter(170)) & 0xFF;
            mixParam[1] = 0;
            applyParam(MIXER, 1);
            mixParam[3] = ret;
        }
        applyParam(MIXER, 3);
        uploadWindow(REVERB);
        uploadWindow(REVERB_DELAY);
        setMute(0, 0);
        setMute(3, 0);
    }

    if (mask & 0x20) {
        const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
        for (uint32_t i = 0; i < 16; i++) {
            if (reverbChanged[i] != 1)
                continue;
            const uint32_t w = reverbCache[i];
            reverbChanged[i] = 0;
            if (!is_delay(rev)) {
                param[REVERB][i] = w;
                applyParam(REVERB, int(i));
            } else {
                param[REVERB_DELAY][i] = w;
                applyParam(REVERB_DELAY, int(i));
            }
        }
    }

    if (mask & 0x10) {
        setMute(4, 1);
        const uint32_t var = uint32_t(readParameter(171)) & 0xFF;
        code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
        code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
        applyParam(VARIATION, 255);
        uploadWindow(VARIATION);
        if (var != 0)
            setMute(4, 0);
    }

    if (mask & 0x08) {
        for (uint32_t i = 0; i < 16; i++) {
            if (variationChanged[i] != 1)
                continue;
            const uint32_t w = variationCache[i];
            variationChanged[i] = 0;
            param[VARIATION][i] = w;
            applyParam(VARIATION, int(i));
        }
    }

    if (mask & 0x04) {
        setMute(2, 1);
        for (uint32_t i = 0; i < 16; i++)
            controlOffset[i] = 0;
        const uint32_t ins = uint32_t(readParameter(175)) & 0xFF;
        code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
        code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
        applyParam(INSERTION, 255);
        uploadWindow(INSERTION);
        setMute(2, 0);
    }

    if (mask & 0x02) {
        // a dirty insertion parameter is its cached value plus the controller offset scaled
        // to the range of the parameter, offset * (max - min) / 127, clamped to that range (0x306C0)
        for (uint32_t i = 0; i < 16; i++) {
            if (insertionChanged[i] != 1)
                continue;
            const rom::ParamRange& range = rom::INSERTION_PARAMS[perf[175]][2 * i];
            const int32_t lo = uint16_t(range.min), hi = uint16_t(range.max);
            const int32_t span = controlOffset[i] * (hi - lo);
            const int32_t add = int16_t(span / 127);
            int32_t v = int16_t(insertionCache[i] + add);
            if (!(v > lo)) v = lo;
            v = int16_t(v);
            const int32_t out = v >= hi ? hi : v;
            insertionChanged[i] = 0;
            param[INSERTION][i] = uint32_t(out);
            applyParam(INSERTION, int(i));
        }
    }

    if (mask & 0x200) {
        // The firmware clears the insertion flags here, and past the sixteenth the EQ's, never the mixer's (0x3B63C).
        for (uint32_t i = 0; i < 21; i++) {
            if (mixerChanged[i] != 1)
                continue;
            applyMixer(i);
            if (i < 16) insertionChanged[i] = 0; else eqChanged[i - 16] = 0;
        }
    }

    if (mask & 0x100) {
        for (uint32_t i = 0; i < 11; i++)
            if (eqChanged[i] == 1)
                applyEq(i);
    }
}

// the handler table at flash 0x3F0B0, indexed like mixerChanged: the system parameter
// number for 0..14, then 16 send A, 17 send B, 18 insertion level, 19 individual out, 20 master shift
void EffectHost::applyMixer(uint32_t i)
{
    switch (i) {
    case 0: case 1: applyReverbPanReturn(); break;
    case 5: applyInsertionPan(); break;
    case 6: applyVariationReturn(); break;
    case 8: applyVariationToReverb(); break;
    case 11: applyVariationPan(); break;
    case 14: applyVolume(); break;
    case 16: applyInsertionToReverb(); break;
    case 17: applyInsertionToVariation(); break;
    case 18: applyInsertionLevel(); break;
    case 19: applyIndividualOut(); break;
    }
}

// the handler table at flash 0x3F104, indexed like eqChanged (master EQ bytes 180..190)
void EffectHost::applyEq(uint32_t i)
{
    if (i < 4) applyEqLow(i);
    else if (i < 7) applyEqMid(i);
    else applyEqHigh(i);
}

// flash 0x3B766 applyReverbPanReturn: reverb pan and return from the performance
void EffectHost::applyReverbPanReturn()
{
    const bool isDelay = rom::TYPE_MAP_REVERB[perf[168]][0] != 0;
    const uint32_t pan = perf[169];
    if (!isDelay) {
        mixParam[0] = pan;
        applyParam(MIXER, 0);
        mixParam[10] = 64;
    } else {
        mixParam[0] = 64;
        applyParam(MIXER, 0);
        mixParam[10] = pan;
    }
    applyParam(MIXER, 10);
    const uint32_t ret = perf[170];
    if (!isDelay) {
        mixParam[1] = ret;
        applyParam(MIXER, 1);
        mixParam[3] = 0;
    } else {
        mixParam[1] = 0;
        applyParam(MIXER, 1);
        mixParam[3] = ret;
    }
    applyParam(MIXER, 3);
}

void EffectHost::applyVariationPan()                                                    // flash 0x3B81C
{
    mixParam[11] = perf[172];
    applyParam(MIXER, 11);
}
void EffectHost::applyVariationReturn()                                                 // flash 0x3B82E
{
    mixParam[6] = perf[173];
    applyParam(MIXER, 6);
}
void EffectHost::applyVariationToReverb()                                               // flash 0x3B840
{
    mixParam[8] = perf[174];
    applyParam(MIXER, 8);
}
void EffectHost::applyInsertionPan()                                                    // flash 0x3B852
{
    mixParam[5] = perf[176];
    applyParam(MIXER, 5);
}
void EffectHost::applyVolume()                                                // flash 0x3B864
{
    mixParam[14] = perf[16];
    applyParam(MIXER, 14);
}

// flash 0x3B8BC applyInsertionToReverb: insertion to reverb send, the performance byte plus the
// controller offset, clamped to 0..127
void EffectHost::applyInsertionToReverb()
{
    int32_t v = int16_t(perf[177] + uint32_t(sendReverbOffset));
    if (!(v > 0)) v = 0;
    setInsertionToReverb(uint32_t(v >= 127 ? 127 : v));
}
// flash 0x3B8EC applyInsertionToVariation: the same for the insertion to variation send
void EffectHost::applyInsertionToVariation()
{
    int32_t v = int16_t(perf[178] + uint32_t(sendVariationOffset));
    if (!(v > 0)) v = 0;
    setInsertionToVariation(uint32_t(v >= 127 ? 127 : v));
}

void EffectHost::applyInsertionLevel() { setInsertionLevel(perf[179]); }  // flash 0x3B91C
void EffectHost::applyIndividualOut() { setIndividualOut(perf[20]); }   // flash 0x3B92C

// flash 0x3B940 applyEqLow: cached EQ word i (bytes 180..183) to master EQ parameter i
void EffectHost::applyEqLow(uint32_t i)
{
    eqParam[i] = eqCache[i];
    applyParam(MASTER_EQ, int(i));
}
// flash 0x3B95C applyEqMid: cached EQ word i (bytes 184..186) to master EQ parameter i + 4
void EffectHost::applyEqMid(uint32_t i)
{
    eqParam[4 + i] = eqCache[i];
    applyParam(MASTER_EQ, int(i + 4));
}
// flash 0x3B97E applyEqHigh: cached EQ word i (bytes 187..190) to master EQ parameter i + 9
void EffectHost::applyEqHigh(uint32_t i)
{
    eqParam[9 + i] = i < 10 ? eqCache[i] : eqHighShape();
    applyParam(MASTER_EQ, int(i + 9));
}

// flash 0x3B9CC copyEqParams: the eleven cached EQ words to the master EQ block (parameters
// 0-3, 8-10, 16-19)
void EffectHost::copyEqParams()
{
    for (int i = 0; i < 4; i++) eqParam[i] = eqCache[i];
    for (int i = 0; i < 3; i++) eqParam[8 + i] = eqCache[4 + i];
    for (int i = 0; i < 3; i++) eqParam[16 + i] = eqCache[7 + i];
    eqParam[19] = eqHighShape();
}

// flash 0x3BA2C setInsertionToReverb: insertion to reverb send, v * 2896 / 127, into two mixer steps
void EffectHost::setInsertionToReverb(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 2896.0f / 127.0f));
    coef[MIXER_C[17]] = w;
    coef[MIXER_C[23]] = w;
    writeCoefs(1, MIXER_C + 17);
    writeCoefs(1, MIXER_C + 23);
}
// flash 0x3BA8E setInsertionToVariation: insertion to variation send, the same law, two other steps
void EffectHost::setInsertionToVariation(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 2896.0f / 127.0f));
    coef[MIXER_C[28]] = w;
    coef[MIXER_C[33]] = w;
    writeCoefs(1, MIXER_C + 28);
    writeCoefs(1, MIXER_C + 33);
}
// flash 0x3BB30 setInsertionLevel: insertion level, (v * 8192 / 127) >> 2, into two mixer steps
void EffectHost::setInsertionLevel(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 8192.0f / 127.0f)) >> 2;
    coef[MIXER_B[8]] = w;
    coef[MIXER_B[9]] = w;
    writeCoefs(1, MIXER_B + 8);
    writeCoefs(1, MIXER_B + 9);
}

// flash 0x3BBBA setIndividualOut: individual out. 0 none, 1 the first pair of steps at 0x0B50,
// 2 the second pair; other values rewrite the four steps as they are.
void EffectHost::setIndividualOut(uint32_t mode)
{
    auto step = [&](int i) -> uint32_t { return rom::STEPS_INDIVIDUAL_OUT[i]; };
    switch (mode & 0xFF) {
    case 0: coef[step(0)] = 0; coef[step(1)] = 0; coef[step(2)] = 0; coef[step(3)] = 0; break;
    case 1: coef[step(0)] = 0x0B50; coef[step(1)] = 0x0B50; coef[step(2)] = 0; coef[step(3)] = 0; break;
    case 2: coef[step(0)] = 0; coef[step(1)] = 0; coef[step(2)] = 0x0B50; coef[step(3)] = 0x0B50; break;
    }
    writeCoefs(4, rom::STEPS_INDIVIDUAL_OUT);
}

// flash 0x3BCA0 setMasterLevel: the master level word, then the master volume step
void EffectHost::setMasterLevel()
{
    masterLevel = 0x1FFF;
    applyParam(MIXER, 14);
}

// ---- changes after the load ----
//
// The effect task sleeps on an event flag. Whoever changes something posts a mask (the bits
// accumulate) and the task takes the whole pattern at once, waiting on bit 0, which is why
// every posted mask has bit 0 set. Here post keeps the pattern in posted and
// runPosted is one turn of the task.

void EffectHost::post(uint32_t mask)                       // 0x9FA4
{
    posted |= mask;
}

// flash 0x3C994 runPosted, one turn: false when nothing was posted
bool EffectHost::runPosted()
{
    if (!(posted & 1))
        return false;
    const uint32_t mask = posted;
    posted = 0;
    applyChanges(mask);
    return true;
}

// flash 0x3C1D2 loadTypeDefaults: the 16 default parameters of a window's current type
void EffectHost::loadTypeDefaults(uint32_t kind)
{
    std::copy_n(presets(int(kind)).defaults[code[kind].lsb], 16, param[kind]);
}

// The three type changes: the new type's bytes, its default parameters into the block, and
// from there back into the performance (two 7-bit bytes per word for the first parameters, one
// byte for reverb parameters 9-16) and into the cache, with the dirty flags cleared.

// flash 0x3BDFC reverbTypeChanged: performance byte 168
void EffectHost::reverbTypeChanged()
{
    const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
    const uint8_t* map = rom::TYPE_MAP_REVERB[rev];
    const bool isDelay = map[0] != 0;
    if (!isDelay) {
        code[REVERB].lsb = map[1];
        loadTypeDefaults(REVERB);
        code[REVERB_DELAY].msb = 0;
        code[REVERB_DELAY].lsb = 1;
        loadTypeDefaults(REVERB_DELAY);
    } else {
        code[REVERB].lsb = 0;
        loadTypeDefaults(REVERB);
        code[REVERB_DELAY].msb = 8;
        code[REVERB_DELAY].lsb = map[1];
        loadTypeDefaults(REVERB_DELAY);
    }
    const uint16_t* blk = param[isDelay ? REVERB_DELAY : REVERB];
    for (uint32_t i = 0; i < 8; i++) {
        perf[80 + 2 * i + 1] = (blk[i] & 0xFF) & 0x7F;
        perf[80 + 2 * i] = (blk[i] & 0x3F80) >> 7;
        reverbCache[i] = blk[i];
        reverbChanged[i] = 0;
    }
    for (uint32_t i = 8; i < 16; i++) {
        perf[96 + i - 8] = (blk[i] & 0xFF);
        reverbCache[i] = blk[i];
        reverbChanged[i] = 0;
    }
    if (isDelay) {
        perf[97] = 0;
        reverbCache[9] = 127;
    }
}

// flash 0x3C018 variationTypeChanged: performance byte 171, the variation type
void EffectHost::variationTypeChanged()
{
    const uint32_t var = uint32_t(readParameter(171)) & 0xFF;
    code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
    code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
    loadTypeDefaults(VARIATION);
    if (var != 0)
        loadTypeDefaults(VARIATION);
    for (uint32_t i = 0; i < 16; i++) {
        perf[104 + 2 * i + 1] = (param[VARIATION][i] & 0xFF) & 0x7F;
        perf[104 + 2 * i] = (param[VARIATION][i] & 0x3F80) >> 7;
        variationCache[i] = param[VARIATION][i];
        variationChanged[i] = 0;
    }
    perf[122] = 0;
    variationCache[9] = 127;
}

// flash 0x3C118 insertionTypeChanged: performance byte 175, the insertion type
void EffectHost::insertionTypeChanged()
{
    const uint32_t ins = uint32_t(readParameter(175)) & 0xFF;
    code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
    code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
    loadTypeDefaults(INSERTION);
    for (uint32_t i = 0; i < 16; i++) {
        perf[136 + 2 * i + 1] = (param[INSERTION][i] & 0xFF) & 0x7F;
        perf[136 + 2 * i] = (param[INSERTION][i] & 0x3F80) >> 7;
        insertionCache[i] = param[INSERTION][i];
        insertionChanged[i] = 0;
    }
}

// flash 0x3C4A8 byteChanged(id): a performance common byte changed. Reads it into the
// cache or marks its mixer flag, and posts the mask that makes the task apply it. `id` is the
// byte's index; either byte of a two-byte parameter stands for the parameter.
void EffectHost::byteChanged(uint32_t id)
{
    int32_t n = int32_t(id & 0xFFFF);
    if ((n >= 80 && n <= 95) || (n >= 104 && n <= 167)) n &= ~1;
    auto half = [](int32_t d) -> uint32_t { return uint32_t((d + (0 > d ? 1 : 0)) >> 1); };
    auto mixer = [&](uint32_t flag) {
        mixerChanged[flag] = 1;
        post(513);
    };
    if (n >= 80 && !(n > 94)) {
        reverbChanged[half(n - 80)] = 1;
        reverbCache[half(n - 80)] = uint32_t(readParameter(uint32_t(n)));
        post(33);
    }
    if (n >= 96 && !(n > 103)) {
        reverbChanged[8 + uint32_t(n - 96)] = 1;
        reverbCache[uint32_t(n - 96 + 8)] = uint32_t(readParameter(uint32_t(n)));
        post(33);
    }
    if (n >= 104 && !(n > 134)) {
        variationChanged[half(n - 104)] = 1;
        variationCache[half(n - 104)] = uint32_t(readParameter(uint32_t(n)));
        post(9);
    }
    if (n >= 136 && !(n > 166)) {
        insertionChanged[half(n - 136)] = 1;
        insertionCache[half(n - 136)] = uint32_t(readParameter(uint32_t(n)));
        post(3);
    }
    if (n == 168) {
        reverbTypeChanged();
        post(65);
    }
    if (n == 169) mixer(0);
    if (n == 170) mixer(1);
    if (n == 171) {
        variationTypeChanged();
        post(17);
    }
    if (n == 172) mixer(11);
    if (n == 173) mixer(6);
    if (n == 174) mixer(8);
    if (n == 175) {
        insertionTypeChanged();
        post(5);
    }
    if (n == 176) mixer(5);
    if (n == 16) mixer(14);
    if (n == 177) mixer(16);
    if (n == 178) mixer(17);
    if (n == 179) mixer(18);
    if (n == 20) mixer(19);
    if (n >= 180 && n <= 190) {
        // The firmware keeps ten EQ words where it uses eleven: the eleventh, the high band's
        // shape, lands on the first two reverb flags and is read back from them.
        eqChanged[n - 180] = 1;
        const int v = readParameter(uint32_t(n));
        if (n < 190) eqCache[n - 180] = uint16_t(v);
        else { reverbChanged[0] = uint8_t(v >> 8); reverbChanged[1] = uint8_t(v); }
        post(257);
    }
}

// flash 0x3C8D4 setControlOffset: a controller's offset for insertion parameter i
void EffectHost::setControlOffset(uint32_t i, uint32_t v)
{
    controlOffset[i & 15] = int16_t(v);
    insertionChanged[i & 15] = 1;
    post(3);
}
// flash 0x3C8EC setSendVariationOffset: a controller's offset for the insertion to variation send
void EffectHost::setSendVariationOffset(uint32_t v)
{
    sendVariationOffset = v;
    mixerChanged[17] = 1;
    post(513);
}
// flash 0x3C8FE setSendReverbOffset: the same for the insertion to reverb send
void EffectHost::setSendReverbOffset(uint32_t v)
{
    sendReverbOffset = v;
    mixerChanged[16] = 1;
    post(513);
}

}  // namespace fs1r::vop3host

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The performance into the effect chip: applySubDirtyMask, which the effect task runs for
// every message posted to it, and the functions it reaches. Mask 0x8000 is a performance load.
//
// The caches hold the parameters as read from the performance; the blocks the per-type
// handlers read (param[kind]) are filled from them here.
#include "host.h"
#include <algorithm>

namespace fs1r::vop3host {

using namespace rom::mix;

namespace {


}  // namespace

// flash 0x3AC88 applySubDirtyMask
//   0x8000  read the performance into the cache, then as 0x80
//   0x80    everything: the four windows with their parameters, the mixer, the master EQ
//   0x40    reverb type            0x20  dirty reverb parameters
//   0x10    variation type         0x08  dirty variation parameters
//   0x04    insertion type         0x02  dirty insertion parameters, with their controller offsets
//   0x200   dirty mixer parameters (handlers at 0x3F0B0)
//   0x100   dirty master EQ parameters (handlers at 0x3F104)
void Host::applySubDirtyMask(uint32_t mask)
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
        setSubFlag(0, 1);
        setSubFlag(2, 1);
        setSubFlag(3, 1);
        setSubFlag(4, 1);
        for (uint32_t i = 0; i < 16; i++)
            controlOffset[i] = 0;
        const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
        mixParam[4] = 90;
        fxParamEntry(6, 4);
        int delay_entry;
        if (!is_delay(rev)) {
            std::copy_n(reverbCache, 16, param[REVERB]);
            code[REVERB].lsb = rom::TYPE_MAP_REVERB[rev][1];
            fxParamEntry(0, 254);
            code[REVERB_DELAY].msb = 0;
            code[REVERB_DELAY].lsb = 1;
            delay_entry = 255;
        } else {
            std::copy_n(reverbCache, 16, param[REVERB_DELAY]);
            code[REVERB].lsb = 0;
            fxParamEntry(0, 255);
            code[REVERB_DELAY].msb = 8;
            code[REVERB_DELAY].lsb = rom::TYPE_MAP_REVERB[rev][1];
            delay_entry = 254;
        }
        fxParamEntry(3, delay_entry);

        const uint32_t var = perf[171];
        std::copy_n(variationCache, 16, param[VARIATION]);
        code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
        code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
        fxParamEntry(4, 254);

        const uint32_t ins = perf[175];
        std::copy_n(insertionCache, 16, param[INSERTION]);
        code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
        code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
        fxParamEntry(2, 254);

        // the mixer: reverb pan and return go to the words of the reverb or of the delay block
        const uint32_t rtype = perf[168];
        const uint32_t pan = perf[169];
        if (!is_delay(rtype)) {
            mixParam[0] = pan;
            fxParamEntry(6, 0);
            mixParam[10] = 64;
        } else {
            mixParam[0] = 64;
            fxParamEntry(6, 0);
            mixParam[10] = pan;
        }
        fxParamEntry(6, 10);
        const uint32_t ret = perf[170];
        if (!is_delay(rtype)) {
            mixParam[1] = ret;
            fxParamEntry(6, 1);
            mixParam[3] = 0;
        } else {
            mixParam[1] = 0;
            fxParamEntry(6, 1);
            mixParam[3] = ret;
        }
        fxParamEntry(6, 3);
        mixParam[11] = perf[172];
        fxParamEntry(6, 11);
        mixParam[6] = perf[173];
        fxParamEntry(6, 6);
        mixParam[8] = perf[174];
        fxParamEntry(6, 8);
        mixParam[5] = perf[176];
        fxParamEntry(6, 5);
        mixParam[14] = perf[16];
        setSubMasterShift();
        setSubInsToReverb(perf[177]);
        setSubInsToVariation(perf[178]);
        setSubInsLevel(perf[179]);
        setSubIndividualOut(perf[20]);
        copySubEqWords();
        fxParamEntry(5, 0);
        fxParamEntry(5, 8);
        fxParamEntry(5, 16);
        loadSubEffectType(0);
        loadSubEffectType(2);
        loadSubEffectType(3);
        loadSubEffectType(4);
        setSubFlag(0, 0);
        setSubFlag(2, 0);
        setSubFlag(3, 0);
        if ((uint32_t(readParameter(171)) & 0xFF) != 0)
            setSubFlag(4, 0);
    }

    if (mask & 0x40) {
        setSubFlag(0, 1);
        setSubFlag(3, 1);
        const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
        mixParam[4] = 90;
        fxParamEntry(6, 4);
        if (!is_delay(rev)) {
            code[REVERB].lsb = rom::TYPE_MAP_REVERB[rev][1];
            fxParamEntry(0, 255);
            code[REVERB_DELAY].msb = 0;
            code[REVERB_DELAY].lsb = 1;
            fxParamEntry(3, 255);
            mixParam[0] = uint32_t(readParameter(169)) & 0xFF;
            fxParamEntry(6, 0);
            mixParam[10] = 64;
            fxParamEntry(6, 10);
            mixParam[1] = uint32_t(readParameter(170)) & 0xFF;
            fxParamEntry(6, 1);
            mixParam[3] = 0;
        } else {
            code[REVERB].lsb = 0;
            fxParamEntry(0, 255);
            code[REVERB_DELAY].msb = 8;
            code[REVERB_DELAY].lsb = rom::TYPE_MAP_REVERB[rev][1];
            fxParamEntry(3, 255);
            const uint32_t pan = uint32_t(readParameter(169)) & 0xFF;
            mixParam[0] = 64;
            fxParamEntry(6, 0);
            mixParam[10] = pan;
            fxParamEntry(6, 10);
            const uint32_t ret = uint32_t(readParameter(170)) & 0xFF;
            mixParam[1] = 0;
            fxParamEntry(6, 1);
            mixParam[3] = ret;
        }
        fxParamEntry(6, 3);
        loadSubEffectType(0);
        loadSubEffectType(3);
        setSubFlag(0, 0);
        setSubFlag(3, 0);
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
                fxParamEntry(0, int(i));
            } else {
                param[REVERB_DELAY][i] = w;
                fxParamEntry(3, int(i));
            }
        }
    }

    if (mask & 0x10) {
        setSubFlag(4, 1);
        const uint32_t var = uint32_t(readParameter(171)) & 0xFF;
        code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
        code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
        fxParamEntry(4, 255);
        loadSubEffectType(4);
        if (var != 0)
            setSubFlag(4, 0);
    }

    if (mask & 0x08) {
        for (uint32_t i = 0; i < 16; i++) {
            if (variationChanged[i] != 1)
                continue;
            const uint32_t w = variationCache[i];
            variationChanged[i] = 0;
            param[VARIATION][i] = w;
            fxParamEntry(4, int(i));
        }
    }

    if (mask & 0x04) {
        setSubFlag(2, 1);
        for (uint32_t i = 0; i < 16; i++)
            controlOffset[i] = 0;
        const uint32_t ins = uint32_t(readParameter(175)) & 0xFF;
        code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
        code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
        fxParamEntry(2, 255);
        loadSubEffectType(2);
        setSubFlag(2, 0);
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
            fxParamEntry(2, int(i));
        }
    }

    if (mask & 0x200) {
        // The firmware clears the insertion flags here, and past the sixteenth the EQ's, never the mixer's (0x3B63C).
        for (uint32_t i = 0; i < 21; i++) {
            if (mixerChanged[i] != 1)
                continue;
            subMixerHandler(i);
            if (i < 16) insertionChanged[i] = 0; else eqChanged[i - 16] = 0;
        }
    }

    if (mask & 0x100) {
        for (uint32_t i = 0; i < 11; i++)
            if (eqChanged[i] == 1)
                subEqHandler(i);
    }
}

// the handler table at flash 0x3F0B0, indexed like mixerChanged: the system parameter
// number for 0..14, then 16 send A, 17 send B, 18 insertion level, 19 individual out, 20 master shift
void Host::subMixerHandler(uint32_t i)
{
    switch (i) {
    case 0: case 1: applySubReverbPanReturn(); break;
    case 5: applySubInsPan(); break;
    case 6: applySubVarReturn(); break;
    case 8: applySubVarToReverb(); break;
    case 11: applySubVarPan(); break;
    case 14: applySubPerfVolume(); break;
    case 16: applySubInsToReverb(); break;
    case 17: applySubInsToVariation(); break;
    case 18: applySubInsLevel(); break;
    case 19: applySubIndividualOut(); break;
    }
}

// the handler table at flash 0x3F104, indexed like eqChanged (master EQ bytes 180..190)
void Host::subEqHandler(uint32_t i)
{
    if (i < 4) copySubEqLow(i);
    else if (i < 7) copySubEqMid(i);
    else copySubEqHigh(i);
}

// flash 0x3B766 applySubReverbPanReturn: reverb pan and return from the performance
void Host::applySubReverbPanReturn()
{
    const bool isDelay = rom::TYPE_MAP_REVERB[perf[168]][0] != 0;
    const uint32_t pan = perf[169];
    if (!isDelay) {
        mixParam[0] = pan;
        fxParamEntry(6, 0);
        mixParam[10] = 64;
    } else {
        mixParam[0] = 64;
        fxParamEntry(6, 0);
        mixParam[10] = pan;
    }
    fxParamEntry(6, 10);
    const uint32_t ret = perf[170];
    if (!isDelay) {
        mixParam[1] = ret;
        fxParamEntry(6, 1);
        mixParam[3] = 0;
    } else {
        mixParam[1] = 0;
        fxParamEntry(6, 1);
        mixParam[3] = ret;
    }
    fxParamEntry(6, 3);
}

void Host::applySubVarPan()                                                    // flash 0x3B81C
{
    mixParam[11] = perf[172];
    fxParamEntry(6, 11);
}
void Host::applySubVarReturn()                                                 // flash 0x3B82E
{
    mixParam[6] = perf[173];
    fxParamEntry(6, 6);
}
void Host::applySubVarToReverb()                                               // flash 0x3B840
{
    mixParam[8] = perf[174];
    fxParamEntry(6, 8);
}
void Host::applySubInsPan()                                                    // flash 0x3B852
{
    mixParam[5] = perf[176];
    fxParamEntry(6, 5);
}
void Host::applySubPerfVolume()                                                // flash 0x3B864
{
    mixParam[14] = perf[16];
    fxParamEntry(6, 14);
}

// flash 0x3B8BC applySubInsToReverb: insertion to reverb send, the performance byte plus the
// controller offset, clamped to 0..127
void Host::applySubInsToReverb()
{
    int32_t v = int16_t(perf[177] + uint32_t(sendReverbOffset));
    if (!(v > 0)) v = 0;
    setSubInsToReverb(uint32_t(v >= 127 ? 127 : v));
}
// flash 0x3B8EC applySubInsToVariation: the same for the insertion to variation send
void Host::applySubInsToVariation()
{
    int32_t v = int16_t(perf[178] + uint32_t(sendVariationOffset));
    if (!(v > 0)) v = 0;
    setSubInsToVariation(uint32_t(v >= 127 ? 127 : v));
}

void Host::applySubInsLevel() { setSubInsLevel(perf[179]); }  // flash 0x3B91C
void Host::applySubIndividualOut() { setSubIndividualOut(perf[20]); }   // flash 0x3B92C

// flash 0x3B940 copySubEqLow: cached EQ word i (bytes 180..183) to master EQ parameter i
void Host::copySubEqLow(uint32_t i)
{
    eqParam[i] = eqCache[i];
    fxParamEntry(5, int(i));
}
// flash 0x3B95C copySubEqMid: cached EQ word i (bytes 184..186) to master EQ parameter i + 4
void Host::copySubEqMid(uint32_t i)
{
    eqParam[4 + i] = eqCache[i];
    fxParamEntry(5, int(i + 4));
}
// flash 0x3B97E copySubEqHigh: cached EQ word i (bytes 187..190) to master EQ parameter i + 9
void Host::copySubEqHigh(uint32_t i)
{
    eqParam[9 + i] = i < 10 ? eqCache[i] : eqHighShape();
    fxParamEntry(5, int(i + 9));
}

// flash 0x3B9CC copySubEqWords: the eleven cached EQ words to the master EQ block (parameters
// 0-3, 8-10, 16-19)
void Host::copySubEqWords()
{
    for (int i = 0; i < 4; i++) eqParam[i] = eqCache[i];
    for (int i = 0; i < 3; i++) eqParam[8 + i] = eqCache[4 + i];
    for (int i = 0; i < 3; i++) eqParam[16 + i] = eqCache[7 + i];
    eqParam[19] = eqHighShape();
}

// flash 0x3BA2C setSubInsToReverb: insertion to reverb send, v * 2896 / 127, into two mixer steps
void Host::setSubInsToReverb(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 2896.0f / 127.0f));
    coef[MIXER_C[17]] = uint32_t(w);
    coef[MIXER_C[23]] = uint32_t(w);
    writeSweWordListSub(1, MIXER_C + 17);
    writeSweWordListSub(1, MIXER_C + 23);
}
// flash 0x3BA8E setSubInsToVariation: insertion to variation send, the same law, two other steps
void Host::setSubInsToVariation(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 2896.0f / 127.0f));
    coef[MIXER_C[28]] = uint32_t(w);
    coef[MIXER_C[33]] = uint32_t(w);
    writeSweWordListSub(1, MIXER_C + 28);
    writeSweWordListSub(1, MIXER_C + 33);
}
// flash 0x3BB30 setSubInsLevel: insertion level, (v * 8192 / 127) >> 2, into two mixer steps
void Host::setSubInsLevel(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 8192.0f / 127.0f)) >> 2;
    coef[MIXER_B[8]] = uint32_t(w);
    coef[MIXER_B[9]] = uint32_t(w);
    writeSweWordListSub(1, MIXER_B + 8);
    writeSweWordListSub(1, MIXER_B + 9);
}

// flash 0x3BBBA setSubIndividualOut: individual out. 0 none, 1 the first pair of steps at 0x0B50,
// 2 the second pair; other values rewrite the four steps as they are.
void Host::setSubIndividualOut(uint32_t mode)
{
    auto step = [&](int i) -> uint32_t { return rom::STEPS_INDIVIDUAL_OUT[i]; };
    switch (mode & 0xFF) {
    case 0: coef[step(0)] = 0; coef[step(1)] = 0; coef[step(2)] = 0; coef[step(3)] = 0; break;
    case 1: coef[step(0)] = 0x0B50; coef[step(1)] = 0x0B50; coef[step(2)] = 0; coef[step(3)] = 0; break;
    case 2: coef[step(0)] = 0; coef[step(1)] = 0; coef[step(2)] = 0x0B50; coef[step(3)] = 0x0B50; break;
    }
    writeSweWordListSub(4, rom::STEPS_INDIVIDUAL_OUT);
}

// flash 0x3BCA0 setSubMasterShift: the master level word, then the master volume step
void Host::setSubMasterShift()
{
    masterLevel = 0x1FFF;
    fxParamEntry(6, 14);
}

// ---- changes after the load ----
//
// The effect task sleeps on an event flag. Whoever changes something posts a mask (the bits
// accumulate) and the task takes the whole pattern at once, waiting on bit 0, which is why
// every posted mask has bit 0 set. Here postSubMask keeps the pattern in fx_flags and
// pollSubDirtyLoop is one turn of the task.

void Host::postSubMask(uint32_t mask)                       // 0x9FA4
{
    fx_flags |= mask;
}

// flash 0x3C994 pollSubDirtyLoop, one turn: false when nothing was posted
bool Host::pollSubDirtyLoop()
{
    if (!(fx_flags & 1))
        return false;
    const uint32_t mask = fx_flags;
    fx_flags = 0;
    applySubDirtyMask(mask);
    return true;
}

// flash 0x3C1D2 loadSubTypeDefaults: the 16 default parameters of a window's current type
void Host::loadSubTypeDefaults(uint32_t kind)
{
    std::copy_n(presets(int(kind)).defaults[code[kind].lsb], 16, param[kind]);
}

// The three type changes: the new type's bytes, its default parameters into the block, and
// from there back into the performance (two 7-bit bytes per word for the first parameters, one
// byte for reverb parameters 9-16) and into the cache, with the dirty flags cleared.

// flash 0x3BDFC syncSubReverbType: performance byte 168
void Host::syncSubReverbType()
{
    const uint32_t rev = uint32_t(readParameter(168)) & 0xFF;
    const uint8_t* map = rom::TYPE_MAP_REVERB[rev];
    const bool isDelay = map[0] != 0;
    if (!isDelay) {
        code[REVERB].lsb = map[1];
        loadSubTypeDefaults(0);
        code[REVERB_DELAY].msb = 0;
        code[REVERB_DELAY].lsb = 1;
        loadSubTypeDefaults(3);
    } else {
        code[REVERB].lsb = 0;
        loadSubTypeDefaults(0);
        code[REVERB_DELAY].msb = 8;
        code[REVERB_DELAY].lsb = map[1];
        loadSubTypeDefaults(3);
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

// flash 0x3C018 syncSubVariationType: performance byte 171, the variation type
void Host::syncSubVariationType()
{
    const uint32_t var = uint32_t(readParameter(171)) & 0xFF;
    code[VARIATION].msb = rom::TYPE_MAP_VARIATION[var][0];
    code[VARIATION].lsb = rom::TYPE_MAP_VARIATION[var][1];
    loadSubTypeDefaults(4);
    if (var != 0)
        loadSubTypeDefaults(4);
    for (uint32_t i = 0; i < 16; i++) {
        perf[104 + 2 * i + 1] = (param[VARIATION][i] & 0xFF) & 0x7F;
        perf[104 + 2 * i] = (param[VARIATION][i] & 0x3F80) >> 7;
        variationCache[i] = param[VARIATION][i];
        variationChanged[i] = 0;
    }
    perf[122] = 0;
    variationCache[9] = 127;
}

// flash 0x3C118 syncSubInsertionType: performance byte 175, the insertion type
void Host::syncSubInsertionType()
{
    const uint32_t ins = uint32_t(readParameter(175)) & 0xFF;
    code[INSERTION].msb = rom::TYPE_MAP_INSERTION[ins][0];
    code[INSERTION].lsb = rom::TYPE_MAP_INSERTION[ins][1];
    loadSubTypeDefaults(2);
    for (uint32_t i = 0; i < 16; i++) {
        perf[136 + 2 * i + 1] = (param[INSERTION][i] & 0xFF) & 0x7F;
        perf[136 + 2 * i] = (param[INSERTION][i] & 0x3F80) >> 7;
        insertionCache[i] = param[INSERTION][i];
        insertionChanged[i] = 0;
    }
}

// flash 0x3C4A8 markSubParamDirty(id): a performance common byte changed. Reads it into the
// cache or marks its mixer flag, and posts the mask that makes the task apply it. `id` is the
// byte's index; either byte of a two-byte parameter stands for the parameter.
void Host::markSubParamDirty(uint32_t id)
{
    int32_t n = int32_t(id & 0xFFFF);
    if ((n >= 80 && n <= 95) || (n >= 104 && n <= 167)) n &= ~1;
    auto half = [](int32_t d) -> uint32_t { return uint32_t((d + (0 > d ? 1 : 0)) >> 1); };
    auto mixer = [&](uint32_t flag) {
        mixerChanged[flag] = 1;
        postSubMask(513);
    };
    if (n >= 80 && !(n > 94)) {
        reverbChanged[half(n - 80)] = 1;
        reverbCache[half(n - 80)] = uint32_t(readParameter(uint32_t(n)));
        postSubMask(33);
    }
    if (n >= 96 && !(n > 103)) {
        reverbChanged[8 + uint32_t(n - 96)] = 1;
        reverbCache[uint32_t(n - 96 + 8)] = uint32_t(readParameter(uint32_t(n)));
        postSubMask(33);
    }
    if (n >= 104 && !(n > 134)) {
        variationChanged[half(n - 104)] = 1;
        variationCache[half(n - 104)] = uint32_t(readParameter(uint32_t(n)));
        postSubMask(9);
    }
    if (n >= 136 && !(n > 166)) {
        insertionChanged[half(n - 136)] = 1;
        insertionCache[half(n - 136)] = uint32_t(readParameter(uint32_t(n)));
        postSubMask(3);
    }
    if (n == 168) {
        syncSubReverbType();
        postSubMask(65);
    }
    if (n == 169) mixer(0);
    if (n == 170) mixer(1);
    if (n == 171) {
        syncSubVariationType();
        postSubMask(17);
    }
    if (n == 172) mixer(11);
    if (n == 173) mixer(6);
    if (n == 174) mixer(8);
    if (n == 175) {
        syncSubInsertionType();
        postSubMask(5);
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
        postSubMask(257);
    }
}

// flash 0x3C8D4 setSubInsCtlOffset: a controller's offset for insertion parameter i
void Host::setSubInsCtlOffset(uint32_t i, uint32_t v)
{
    controlOffset[i & 15] = int16_t(v);
    insertionChanged[i & 15] = 1;
    postSubMask(3);
}
// flash 0x3C8EC setSubSendBOffset: a controller's offset for the insertion to variation send
void Host::setSubSendBOffset(uint32_t v)
{
    sendVariationOffset = v;
    mixerChanged[17] = 1;
    postSubMask(513);
}
// flash 0x3C8FE setSubSendAOffset: the same for the insertion to reverb send
void Host::setSubSendAOffset(uint32_t v)
{
    sendReverbOffset = v;
    mixerChanged[16] = 1;
    postSubMask(513);
}

}  // namespace fs1r::vop3host

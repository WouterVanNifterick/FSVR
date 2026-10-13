// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The performance into the effect chip: applySubDirtyMask, which the effect task runs for
// every message posted to it, and the functions it reaches. Mask 0x8000 is a performance load.
//
// The parameter cache holds 16-bit words: reverb at +0, insertion at +0x20, variation at
// +0x40, master EQ at +0x60. The blocks the per-type handlers read (FX_PARAMS + 0x20 * kind)
// are filled from it here.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {

// per type byte of the performance, two bytes: the reverb map gives [0] 0 = reverb window
// (kind 0), else delay window (kind 3), and [1] the type byte; the others give msb, lsb
constexpr uint32_t TYPE_MAP_REVERB = rom::FX_TYPE_MAP_REVERB, TYPE_MAP_VARIATION = rom::FX_TYPE_MAP_VARIATION,
                   TYPE_MAP_INSERTION = rom::FX_TYPE_MAP_INSERTION;
constexpr uint32_t STEPS_B = rom::FX_STEPS_MIXER_B, STEPS_C = rom::FX_STEPS_MIXER_C;
constexpr uint32_t STEPS_ROUTE = rom::FX_STEPS_INDIVIDUAL_OUT;

// parameter block words, as offsets from SWE_SHADOW
constexpr uint32_t BLK_REVERB = SWE_SHADOW + 2304, BLK_INSERTION = SWE_SHADOW + 2368, BLK_DELAY = SWE_SHADOW + 2400,
                   BLK_VARIATION = SWE_SHADOW + 2432, BLK_EQ = SWE_SHADOW + 2464, BLK_SYS = SWE_SHADOW + 2512;

}  // namespace

// flash 0x306C0 through 0x3074C (which = 0, the minimum) and 0x30740 (which = 1, the maximum):
// the range of an insertion parameter from the descriptor table of the current insertion type
int32_t Host::insertionParamBound(uint32_t id, uint32_t which)
{
    uint32_t type = ram.u8(PERFORMANCE + 175);
    if (type > 40) type = 40;
    const uint32_t desc = rom::u32(rom::PARAM_DESC_INSERTION + 4 * type) + (((sh::u16(id) & 255) - 136) << 4);
    return sh::u16(which) == 0 ? rom::s16(desc + 6) : rom::s16(desc + 8);
}
int32_t Host::insertionParamMax(uint32_t id) { return insertionParamBound(id, 1); }     // flash 0x30740
int32_t Host::insertionParamMin(uint32_t id) { return insertionParamBound(id, 0); }     // flash 0x3074C

// flash 0x3AC88 applySubDirtyMask
//   0x8000  read the performance into the cache, then as 0x80
//   0x80    everything: the four windows with their parameters, the mixer, the master EQ
//   0x40    reverb type            0x20  dirty reverb parameters
//   0x10    variation type         0x08  dirty variation parameters
//   0x04    insertion type         0x02  dirty insertion parameters, with their controller offsets
//   0x200   dirty mixer parameters (handlers at 0x3F0B0)
//   0x100   dirty master EQ parameters (handlers at 0x3F104)
//   0x400   controller reset: the insertion offsets and the two send offsets
void Host::applySubDirtyMask(uint32_t mask)
{
    const uint32_t cache = FX_PARAM_CACHE;
    auto perf = [&](uint32_t i) -> uint32_t { return ram.u8(PERFORMANCE + (i & 255)); };
    auto copy16 = [&](uint32_t dst, uint32_t src) {
        for (uint32_t i = 0; i < 16; i++)
            ram.w16(dst + 2 * i, ram.u16(src + 2 * i));
    };
    auto is_delay = [&](uint32_t type) -> bool { return rom::u8(TYPE_MAP_REVERB + (sh::u8(type) << 1)) != 0; };

    if ((mask & 0x80) || (mask & 0x8000)) {
        if (mask & 0x8000) {
            for (uint32_t i = 0; i < 16; i++) {
                if (i < 8)
                    ram.w16(cache + 2 * i, uint32_t(readParameter(0x50 + 2 * i)));
                else
                    ram.w16(cache + 2 * i, uint32_t(readParameter(0x58 + i)));
                ram.w16(cache + 0x40 + 2 * i, uint32_t(readParameter(0x68 + 2 * i)));
                ram.w16(cache + 0x20 + 2 * i, uint32_t(readParameter(136 + 2 * i)));
            }
            for (uint32_t i = 0; i < 10; i++)
                ram.w16(cache + 0x60 + 2 * i, uint32_t(readParameter(180 + i)));
        }
        ram.w16(cache + 18, 127);
        ram.w16(cache + 82, 127);
        setSubFlag(0, 1);
        setSubFlag(2, 1);
        setSubFlag(3, 1);
        setSubFlag(4, 1);
        for (uint32_t i = 0; i < 16; i++)
            ram.w16(FX_VAR_WORD + 2 * i, 0);
        const uint32_t rev = sh::u8(uint32_t(readParameter(168)));
        ram.w16(BLK_SYS + 2 * 4, 90);
        fxParamEntry(6, 4);
        int delay_entry;
        if (!is_delay(rev)) {
            copy16(BLK_REVERB, cache);
            ram.w8(SWE_SHADOW + 0x21, rom::u8(TYPE_MAP_REVERB + 1 + (rev << 1)));
            fxParamEntry(0, 254);
            ram.w8(SWE_SHADOW + 0x26, 0);
            ram.w8(SWE_SHADOW + 0x27, 1);
            delay_entry = 255;
        } else {
            copy16(BLK_DELAY, cache);
            ram.w8(SWE_SHADOW + 0x21, 0);
            fxParamEntry(0, 255);
            ram.w8(SWE_SHADOW + 0x26, 8);
            ram.w8(SWE_SHADOW + 0x27, rom::u8(TYPE_MAP_REVERB + 1 + (rev << 1)));
            delay_entry = 254;
        }
        fxParamEntry(3, delay_entry);

        const uint32_t var = perf(171);
        copy16(BLK_VARIATION, cache + 0x40);
        ram.w8(SWE_SHADOW + 0x28, rom::u8(TYPE_MAP_VARIATION + (var << 1)));
        ram.w8(SWE_SHADOW + 0x29, rom::u8(TYPE_MAP_VARIATION + (var << 1) + 1));
        fxParamEntry(4, 254);

        const uint32_t ins = perf(175);
        copy16(BLK_INSERTION, cache + 0x20);
        ram.w8(SWE_SHADOW + 0x24, rom::u8(TYPE_MAP_INSERTION + (ins << 1)));
        ram.w8(SWE_SHADOW + 0x25, rom::u8(TYPE_MAP_INSERTION + (ins << 1) + 1));
        fxParamEntry(2, 254);

        // the mixer: reverb pan and return go to the words of the reverb or of the delay block
        const uint32_t rtype = perf(168);
        const uint32_t pan = perf(169);
        if (!is_delay(rtype)) {
            ram.w16(BLK_SYS + 2 * 0, pan);
            fxParamEntry(6, 0);
            ram.w16(BLK_SYS + 2 * 10, 64);
        } else {
            ram.w16(BLK_SYS + 2 * 0, 64);
            fxParamEntry(6, 0);
            ram.w16(BLK_SYS + 2 * 10, pan);
        }
        fxParamEntry(6, 10);
        const uint32_t ret = perf(170);
        if (!is_delay(rtype)) {
            ram.w16(BLK_SYS + 2 * 1, ret);
            fxParamEntry(6, 1);
            ram.w16(BLK_SYS + 2 * 3, 0);
        } else {
            ram.w16(BLK_SYS + 2 * 1, 0);
            fxParamEntry(6, 1);
            ram.w16(BLK_SYS + 2 * 3, ret);
        }
        fxParamEntry(6, 3);
        ram.w16(BLK_SYS + 2 * 11, perf(172));
        fxParamEntry(6, 11);
        ram.w16(BLK_SYS + 2 * 6, perf(173));
        fxParamEntry(6, 6);
        ram.w16(BLK_SYS + 2 * 8, perf(174));
        fxParamEntry(6, 8);
        ram.w16(BLK_SYS + 2 * 5, perf(176));
        fxParamEntry(6, 5);
        ram.w16(BLK_SYS + 2 * 14, perf(16));
        setSubMasterShift(2);
        setSubInsToReverb(perf(177));
        setSubInsToVariation(perf(178));
        setSubInsLevel(perf(179));
        setSubIndividualOut(perf(20));
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
        if (sh::u8(uint32_t(readParameter(171))) != 0)
            setSubFlag(4, 0);
    }

    if (mask & 0x40) {
        setSubFlag(0, 1);
        setSubFlag(3, 1);
        const uint32_t rev = sh::u8(uint32_t(readParameter(168)));
        ram.w16(BLK_SYS + 2 * 4, 90);
        fxParamEntry(6, 4);
        if (!is_delay(rev)) {
            ram.w8(SWE_SHADOW + 0x21, rom::u8(TYPE_MAP_REVERB + 1 + (rev << 1)));
            fxParamEntry(0, 255);
            ram.w8(SWE_SHADOW + 0x26, 0);
            ram.w8(SWE_SHADOW + 0x27, 1);
            fxParamEntry(3, 255);
            ram.w16(BLK_SYS + 2 * 0, sh::u8(uint32_t(readParameter(169))));
            fxParamEntry(6, 0);
            ram.w16(BLK_SYS + 2 * 10, 64);
            fxParamEntry(6, 10);
            ram.w16(BLK_SYS + 2 * 1, sh::u8(uint32_t(readParameter(170))));
            fxParamEntry(6, 1);
            ram.w16(BLK_SYS + 2 * 3, 0);
        } else {
            ram.w8(SWE_SHADOW + 0x21, 0);
            fxParamEntry(0, 255);
            ram.w8(SWE_SHADOW + 0x26, 8);
            ram.w8(SWE_SHADOW + 0x27, rom::u8(TYPE_MAP_REVERB + 1 + (rev << 1)));
            fxParamEntry(3, 255);
            const uint32_t pan = sh::u8(uint32_t(readParameter(169)));
            ram.w16(BLK_SYS + 2 * 0, 64);
            fxParamEntry(6, 0);
            ram.w16(BLK_SYS + 2 * 10, pan);
            fxParamEntry(6, 10);
            const uint32_t ret = sh::u8(uint32_t(readParameter(170)));
            ram.w16(BLK_SYS + 2 * 1, 0);
            fxParamEntry(6, 1);
            ram.w16(BLK_SYS + 2 * 3, ret);
        }
        fxParamEntry(6, 3);
        loadSubEffectType(0);
        loadSubEffectType(3);
        setSubFlag(0, 0);
        setSubFlag(3, 0);
    }

    if (mask & 0x20) {
        const uint32_t rev = sh::u8(uint32_t(readParameter(168)));
        for (uint32_t i = 0; i < 16; i++) {
            if (ram.u8(FX_DIRTY_REVERB + i) != 1)
                continue;
            const uint32_t w = ram.u16(cache + 2 * i);
            ram.w8(FX_DIRTY_REVERB + i, 0);
            if (!is_delay(rev)) {
                ram.w16(BLK_REVERB + 2 * i, w);
                fxParamEntry(0, int(i));
            } else {
                ram.w16(BLK_DELAY + 2 * i, w);
                fxParamEntry(3, int(i));
            }
        }
    }

    if (mask & 0x10) {
        setSubFlag(4, 1);
        const uint32_t var = sh::u8(uint32_t(readParameter(171)));
        ram.w8(SWE_SHADOW + 0x28, rom::u8(TYPE_MAP_VARIATION + (var << 1)));
        ram.w8(SWE_SHADOW + 0x29, rom::u8(TYPE_MAP_VARIATION + (var << 1) + 1));
        fxParamEntry(4, 255);
        loadSubEffectType(4);
        if (var != 0)
            setSubFlag(4, 0);
    }

    if (mask & 0x08) {
        readParameter(171);
        for (uint32_t i = 0; i < 16; i++) {
            if (ram.u8(FX_DIRTY_VARIATION + i) != 1)
                continue;
            const uint32_t w = ram.u16(cache + 0x40 + 2 * i);
            ram.w8(FX_DIRTY_VARIATION + i, 0);
            ram.w16(BLK_VARIATION + 2 * i, w);
            fxParamEntry(4, int(i));
        }
    }

    if (mask & 0x04) {
        setSubFlag(2, 1);
        for (uint32_t i = 0; i < 16; i++)
            ram.w16(FX_VAR_WORD + 2 * i, 0);
        const uint32_t ins = sh::u8(uint32_t(readParameter(175)));
        ram.w8(SWE_SHADOW + 0x24, rom::u8(TYPE_MAP_INSERTION + (ins << 1)));
        ram.w8(SWE_SHADOW + 0x25, rom::u8(TYPE_MAP_INSERTION + (ins << 1) + 1));
        fxParamEntry(2, 255);
        loadSubEffectType(2);
        setSubFlag(2, 0);
    }

    if (mask & 0x02) {
        // a dirty insertion parameter is its cached value plus the controller offset scaled
        // to the range of the parameter, offset * (max - min) / 127, clamped to that range
        readParameter(175);
        for (uint32_t i = 0; i < 16; i++) {
            if (ram.u8(FX_DIRTY_INSERTION + i) != 1)
                continue;
            const int32_t lo = int32_t(sh::u16(uint32_t(insertionParamMin(136 + 2 * i))));
            const int32_t hi = int32_t(sh::u16(uint32_t(insertionParamMax(136 + 2 * i))));
            const int32_t span = int32_t(sh::mulsi3(uint32_t(ram.s16(FX_VAR_WORD + 2 * i)), uint32_t(hi - lo)));
            const int32_t add = sh::s16(uint32_t(sh::sdiv3000(span, 127)));
            int32_t v = sh::s16(uint32_t(ram.s16(cache + 0x20 + 2 * i) + add));
            if (!(v > lo)) v = lo;
            v = sh::s16(uint32_t(v));
            const int32_t out = v >= hi ? hi : v;
            ram.w8(FX_DIRTY_INSERTION + i, 0);
            ram.w16(BLK_INSERTION + 2 * i, uint32_t(out));
            fxParamEntry(2, int(i));
        }
    }

    if (mask & 0x200) {
        // The firmware clears byte i of the insertion flags here, not of the mixer flags (0x3B63C, 0x3B644).
        for (uint32_t i = 0; i < 21; i++) {
            if (ram.u8(FX_DIRTY_MIXER + i) != 1)
                continue;
            subMixerHandler(i);
            ram.w8(FX_DIRTY_INSERTION + i, 0);
        }
    }

    if (mask & 0x100) {
        for (uint32_t i = 0; i < 11; i++)
            if (ram.u8(FX_DIRTY_EQ + i) == 1)
                subEqHandler(i);
    }

    if (mask & 0x400) {
        // every insertion word gets performance byte 136 (the index is not added, flash 0x3B6AC)
        for (uint32_t i = 0; i < 16; i++) {
            ram.w16(FX_VAR_WORD + 2 * i, 0);
            ram.w16(BLK_INSERTION + 2 * i, perf(136));
            fxParamEntry(2, int(i));
        }
        ram.w16(FX_SENDB_OFFSET, 0);
        setSubInsToVariation(perf(178));
        ram.w16(FX_SENDA_OFFSET, 0);
        setSubInsToReverb(perf(177));
    }
}

// the handler table at flash 0x3F0B0, indexed like FX_DIRTY_MIXER: the system parameter
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
    case 20: applySubMasterShift(); break;
    default: subParamNop(); break;
    }
}

// the handler table at flash 0x3F104, indexed like FX_DIRTY_EQ (master EQ bytes 180..190)
void Host::subEqHandler(uint32_t i)
{
    if (i < 4) copySubEqLow(i);
    else if (i < 7) copySubEqMid(i);
    else copySubEqHigh(i);
}

void Host::subParamNop() {}                                                    // flash 0x3B762

// flash 0x3B766 applySubReverbPanReturn: reverb pan and return from the performance
void Host::applySubReverbPanReturn()
{
    const uint32_t rtype = ram.u8(PERFORMANCE + 168);
    const bool delay = rom::u8(TYPE_MAP_REVERB + (rtype << 1)) != 0;
    const uint32_t pan = ram.u8(PERFORMANCE + 169);
    if (!delay) {
        ram.w16(BLK_SYS + 2 * 0, pan);
        fxParamEntry(6, 0);
        ram.w16(BLK_SYS + 2 * 10, 64);
    } else {
        ram.w16(BLK_SYS + 2 * 0, 64);
        fxParamEntry(6, 0);
        ram.w16(BLK_SYS + 2 * 10, pan);
    }
    fxParamEntry(6, 10);
    const uint32_t ret = ram.u8(PERFORMANCE + 170);
    if (!delay) {
        ram.w16(BLK_SYS + 2 * 1, ret);
        fxParamEntry(6, 1);
        ram.w16(BLK_SYS + 2 * 3, 0);
    } else {
        ram.w16(BLK_SYS + 2 * 1, 0);
        fxParamEntry(6, 1);
        ram.w16(BLK_SYS + 2 * 3, ret);
    }
    fxParamEntry(6, 3);
}

void Host::applySubVarPan()                                                    // flash 0x3B81C
{
    ram.w16(BLK_SYS + 2 * 11, ram.u8(PERFORMANCE + 172));
    fxParamEntry(6, 11);
}
void Host::applySubVarReturn()                                                 // flash 0x3B82E
{
    ram.w16(BLK_SYS + 2 * 6, ram.u8(PERFORMANCE + 173));
    fxParamEntry(6, 6);
}
void Host::applySubVarToReverb()                                               // flash 0x3B840
{
    ram.w16(BLK_SYS + 2 * 8, ram.u8(PERFORMANCE + 174));
    fxParamEntry(6, 8);
}
void Host::applySubInsPan()                                                    // flash 0x3B852
{
    ram.w16(BLK_SYS + 2 * 5, ram.u8(PERFORMANCE + 176));
    fxParamEntry(6, 5);
}
void Host::applySubPerfVolume()                                                // flash 0x3B864
{
    ram.w16(BLK_SYS + 2 * 14, ram.u8(PERFORMANCE + 16));
    fxParamEntry(6, 14);
}

// flash 0x3B8BC applySubInsToReverb: insertion to reverb send, the performance byte plus the
// controller offset, clamped to 0..127
void Host::applySubInsToReverb()
{
    int32_t v = sh::s16(ram.u8(PERFORMANCE + 177) + uint32_t(ram.s16(FX_SENDA_OFFSET)));
    if (!(v > 0)) v = 0;
    setSubInsToReverb(uint32_t(v >= 127 ? 127 : v));
}
// flash 0x3B8EC applySubInsToVariation: the same for the insertion to variation send
void Host::applySubInsToVariation()
{
    int32_t v = sh::s16(ram.u8(PERFORMANCE + 178) + uint32_t(ram.s16(FX_SENDB_OFFSET)));
    if (!(v > 0)) v = 0;
    setSubInsToVariation(uint32_t(v >= 127 ? 127 : v));
}

void Host::applySubInsLevel() { setSubInsLevel(ram.u8(PERFORMANCE + 179)); }  // flash 0x3B91C
void Host::applySubIndividualOut() { setSubIndividualOut(ram.u8(PERFORMANCE + 20)); }   // flash 0x3B92C
void Host::applySubMasterShift() { setSubMasterShift(2); }                     // flash 0x3B93C

// flash 0x3B940 copySubEqLow: cached EQ word i (bytes 180..183) to master EQ parameter i
void Host::copySubEqLow(uint32_t i)
{
    i = sh::u16(i);
    ram.w16(FX_PARAMS_EQ + (i << 1), ram.u16(FX_PARAM_CACHE + 0x60 + (i << 1)));
    fxParamEntry(5, int(sh::s16(i)));
}
// flash 0x3B95C copySubEqMid: cached EQ word i (bytes 184..186) to master EQ parameter i + 4
void Host::copySubEqMid(uint32_t i)
{
    ram.w16(FX_PARAMS_EQ + 0x10 + ((sh::u16(i) - 4) << 1), ram.u16(FX_PARAM_CACHE + 0x60 + (sh::u16(i) << 1)));
    fxParamEntry(5, int(i + 4));
}
// flash 0x3B97E copySubEqHigh: cached EQ word i (bytes 187..190) to master EQ parameter i + 9
void Host::copySubEqHigh(uint32_t i)
{
    ram.w16(FX_PARAMS_EQ + 0x20 + ((sh::u16(i) - 7) << 1), ram.u16(FX_PARAM_CACHE + 0x60 + (sh::u16(i) << 1)));
    fxParamEntry(5, int(i + 9));
}

// flash 0x3B9CC copySubEqWords: the eleven cached EQ words to the master EQ block (parameters
// 0-3, 8-10, 16-19)
void Host::copySubEqWords()
{
    const uint32_t src = FX_PARAM_CACHE + 0x60;
    for (uint32_t i = 0; i < 4; i++)
        ram.w16(BLK_EQ + 2 * i, ram.u16(src + 2 * i));
    for (uint32_t i = 0; i < 3; i++)
        ram.w16(BLK_EQ + 16 + 2 * i, ram.u16(src + 8 + 2 * i));
    for (uint32_t i = 0; i < 4; i++)
        ram.w16(BLK_EQ + 32 + 2 * i, ram.u16(src + 14 + 2 * i));
}

// flash 0x3BA2C setSubInsToReverb: insertion to reverb send, v * 2896 / 127, into two mixer steps
void Host::setSubInsToReverb(uint32_t v)
{
    const int32_t w = sh::s16(uint32_t(sh::ftoi(sh::fdiv(sh::from_bits(0x42FE0000), sh::fmul(sh::utof(sh::u8(v)), sh::from_bits(0x45350000))))));
    set_coef(rom::u16(STEPS_C + 34), uint32_t(w));
    set_coef(rom::u16(STEPS_C + 46), uint32_t(w));
    writeSweWordListSub(1, STEPS_C + 34);
    writeSweWordListSub(1, STEPS_C + 46);
}
// flash 0x3BA8E setSubInsToVariation: insertion to variation send, the same law, two other steps
void Host::setSubInsToVariation(uint32_t v)
{
    const int32_t w = sh::s16(uint32_t(sh::ftoi(sh::fdiv(sh::from_bits(0x42FE0000), sh::fmul(sh::utof(sh::u8(v)), sh::from_bits(0x45350000))))));
    set_coef(rom::u16(STEPS_C + 56), uint32_t(w));
    set_coef(rom::u16(STEPS_C + 66), uint32_t(w));
    writeSweWordListSub(1, STEPS_C + 56);
    writeSweWordListSub(1, STEPS_C + 66);
}
// flash 0x3BB30 setSubInsLevel: insertion level, (v * 8192 / 127) >> 2, into two mixer steps
void Host::setSubInsLevel(uint32_t v)
{
    const int32_t w = sh::s16(uint32_t(sh::ftoi(sh::fdiv(sh::from_bits(0x42FE0000), sh::fmul(sh::utof(sh::u8(v)), sh::from_bits(0x46000000)))))) >> 2;
    set_coef(rom::u16(STEPS_B + 16), uint32_t(w));
    set_coef(rom::u16(STEPS_B + 18), uint32_t(w));
    writeSweWordListSub(1, STEPS_B + 16);
    writeSweWordListSub(1, STEPS_B + 18);
}

// flash 0x3BBBA setSubIndividualOut: individual out. 0 none, 1 the first pair of steps at 0x0B50,
// 2 the second pair; other values rewrite the four steps as they are.
void Host::setSubIndividualOut(uint32_t mode)
{
    auto step = [&](int i) -> uint32_t { return rom::u16(STEPS_ROUTE + 2 * i); };
    switch (sh::u8(mode)) {
    case 0: set_coef(step(0), 0); set_coef(step(1), 0); set_coef(step(2), 0); set_coef(step(3), 0); break;
    case 1: set_coef(step(0), 0x0B50); set_coef(step(1), 0x0B50); set_coef(step(2), 0); set_coef(step(3), 0); break;
    case 2: set_coef(step(0), 0); set_coef(step(1), 0); set_coef(step(2), 0x0B50); set_coef(step(3), 0x0B50); break;
    default: break;
    }
    writeSweWordListSub(4, STEPS_ROUTE);
}

// flash 0x3BCA0 setSubMasterShift: FX_SUB_LEVEL = 0x7FFF >> (4 - v) for v 0..4, else 0x2000,
// then the master volume step
void Host::setSubMasterShift(uint32_t v)
{
    const int32_t n = int32_t(sh::u8(v));
    if (n >= 0 && !(n > 4))
        ram.w16(FX_SUB_LEVEL, sh::lshrsi3(0x7FFF, 4 - n));
    else
        ram.w16(FX_SUB_LEVEL, 0x2000);
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

// flash 0x3C1D2 loadSubTypeDefaults: the 16 default parameters of a kind's current type, from the
// EPROM into the kind's parameter block (the tables fxParamEntry reads for an odd p)
void Host::loadSubTypeDefaults(uint32_t kind)
{
    uint32_t src, dst;
    switch (sh::u8(kind)) {
    case 0:
        src = rom::FX_DEFAULTS_REVERB + (ram.u8(SWE_SHADOW + 0x21) << 5);
        dst = BLK_REVERB;
        break;
    case 1:
        src = 0x3610F4 + (ram.u8(SWE_SHADOW + 0x23) << 5);
        dst = SWE_SHADOW + 2336;
        break;
    case 2: {
        const uint32_t lsb = ram.u8(SWE_SHADOW + 0x25) << 5;
        switch (ram.u8(SWE_SHADOW + 0x24)) {
        case 1: src = 0x3614F4; break;
        case 2: src = 0x36319C; break;
        case 3: src = 0x3650EA; break;
        case 4: src = 0x3631FC; break;
        case 5: src = 0x3632FC; break;
        case 6: src = 0x3651EA; break;
        case 7: src = 0x3615B4; break;
        case 10: src = 0x36538A; break;
        case 12: src = 0x3653CA; break;
        case 13: src = 0x36542A; break;
        default: src = 0x3612B4; break;
        }
        src += lsb;
        dst = BLK_INSERTION;
        break;
    }
    case 3:
        src = (ram.u8(SWE_SHADOW + 0x26) == 8 ? 0x361914 : 0x361694) + (ram.u8(SWE_SHADOW + 0x27) << 5);
        dst = BLK_DELAY;
        break;
    case 4: {
        const uint32_t lsb = ram.u8(SWE_SHADOW + 0x29) << 5;
        switch (ram.u8(SWE_SHADOW + 0x28)) {
        case 1: src = 0x361854; break;
        case 2: src = 0x3633FC; break;
        case 3: src = 0x36347C; break;
        case 4: src = 0x3634FC; break;
        case 5: src = 0x36357C; break;
        case 6: src = 0x3618D4; break;
        case 7: src = 0x3657AC; break;
        case 8: src = 0x361914; break;
        case 9: src = 0x3654CA; break;
        case 10: src = 0x36554A; break;
        default: src = 0x361694; break;
        }
        src += lsb;
        dst = BLK_VARIATION;
        break;
    }
    default:
        return;
    }
    for (uint32_t i = 0; i < 16; i++)
        ram.w16(dst + 2 * i, rom::u16(src + 2 * i));
}

// The three type changes: the new type's bytes, its default parameters into the block, and
// from there back into the performance (two 7-bit bytes per word for the first parameters, one
// byte for reverb parameters 9-16) and into the cache, with the dirty flags cleared.

// flash 0x3BDFC syncSubReverbType: performance byte 168
void Host::syncSubReverbType()
{
    const uint32_t rev = sh::u8(uint32_t(readParameter(168)));
    const uint32_t map = TYPE_MAP_REVERB + (rev << 1);
    const bool delay = rom::u8(map) != 0;
    if (!delay) {
        ram.w8(SWE_SHADOW + 0x21, rom::u8(map + 1));
        loadSubTypeDefaults(0);
        ram.w8(SWE_SHADOW + 0x26, 0);
        ram.w8(SWE_SHADOW + 0x27, 1);
        loadSubTypeDefaults(3);
    } else {
        ram.w8(SWE_SHADOW + 0x21, 0);
        loadSubTypeDefaults(0);
        ram.w8(SWE_SHADOW + 0x26, 8);
        ram.w8(SWE_SHADOW + 0x27, rom::u8(map + 1));
        loadSubTypeDefaults(3);
    }
    const uint32_t blk = delay ? BLK_DELAY : BLK_REVERB;
    for (uint32_t i = 0; i < 8; i++) {
        ram.w8(PERFORMANCE + 80 + 2 * i + 1, ram.u8(blk + 2 * i + 1) & 0x7F);
        ram.w8(PERFORMANCE + 80 + 2 * i, uint32_t(sh::ashrsi3(int32_t(ram.u16(blk + 2 * i) & 0x3F80), 7)));
        ram.w16(FX_PARAM_CACHE + 2 * i, ram.u16(blk + 2 * i));
        ram.w8(FX_DIRTY_REVERB + i, 0);
    }
    for (uint32_t i = 8; i < 16; i++) {
        ram.w8(PERFORMANCE + 96 + i - 8, ram.u8(blk + 2 * i + 1));
        ram.w16(FX_PARAM_CACHE + 2 * i, ram.u16(blk + 2 * i));
        ram.w8(FX_DIRTY_REVERB + i, 0);
    }
    if (delay) {
        ram.w8(PERFORMANCE + 97, 0);
        ram.w16(FX_PARAM_CACHE + 18, 127);
    }
}

// flash 0x3C018 syncSubVariationType: performance byte 171, the variation type
void Host::syncSubVariationType()
{
    const uint32_t var = sh::u8(uint32_t(readParameter(171)));
    ram.w8(SWE_SHADOW + 0x28, rom::u8(TYPE_MAP_VARIATION + (var << 1)));
    ram.w8(SWE_SHADOW + 0x29, rom::u8(TYPE_MAP_VARIATION + (var << 1) + 1));
    loadSubTypeDefaults(4);
    if (var != 0)
        loadSubTypeDefaults(4);
    for (uint32_t i = 0; i < 16; i++) {
        ram.w8(PERFORMANCE + 104 + 2 * i + 1, ram.u8(BLK_VARIATION + 2 * i + 1) & 0x7F);
        ram.w8(PERFORMANCE + 104 + 2 * i, uint32_t(sh::ashrsi3(int32_t(ram.u16(BLK_VARIATION + 2 * i) & 0x3F80), 7)));
        ram.w16(FX_PARAM_CACHE + 0x40 + 2 * i, ram.u16(BLK_VARIATION + 2 * i));
        ram.w8(FX_DIRTY_VARIATION + i, 0);
    }
    ram.w8(PERFORMANCE + 122, 0);
    ram.w16(FX_PARAM_CACHE + 0x52, 127);
}

// flash 0x3C118 syncSubInsertionType: performance byte 175, the insertion type
void Host::syncSubInsertionType()
{
    const uint32_t ins = sh::u8(uint32_t(readParameter(175)));
    ram.w8(SWE_SHADOW + 0x24, rom::u8(TYPE_MAP_INSERTION + (ins << 1)));
    ram.w8(SWE_SHADOW + 0x25, rom::u8(TYPE_MAP_INSERTION + (ins << 1) + 1));
    loadSubTypeDefaults(2);
    for (uint32_t i = 0; i < 16; i++) {
        ram.w8(PERFORMANCE + 136 + 2 * i + 1, ram.u8(BLK_INSERTION + 2 * i + 1) & 0x7F);
        ram.w8(PERFORMANCE + 136 + 2 * i, uint32_t(sh::ashrsi3(int32_t(ram.u16(BLK_INSERTION + 2 * i) & 0x3F80), 7)));
        ram.w16(FX_PARAM_CACHE + 0x20 + 2 * i, ram.u16(BLK_INSERTION + 2 * i));
        ram.w8(FX_DIRTY_INSERTION + i, 0);
    }
}

// flash 0x3C4A8 markSubParamDirty(id): a performance common byte changed. Reads it into the
// cache or marks its mixer flag, and posts the mask that makes the task apply it. `id` is the
// byte's index (the first byte of a two-byte parameter).
void Host::markSubParamDirty(uint32_t id)
{
    const int32_t n = int32_t(sh::u16(id));
    auto half = [](int32_t d) -> uint32_t { return uint32_t((d + (0 > d ? 1 : 0)) >> 1); };
    auto mixer = [&](uint32_t flag) {
        ram.w8(FX_DIRTY_MIXER + flag, 1);
        postSubMask(513);
    };
    if (n >= 80 && !(n > 94)) {
        ram.w8(FX_DIRTY_REVERB + half(n - 80), 1);
        ram.w16(FX_PARAM_CACHE + (half(n - 80) << 1), uint32_t(readParameter(uint32_t(n))));
        postSubMask(33);
    }
    if (n >= 96 && !(n > 103)) {
        ram.w8(FX_DIRTY_REVERB + 8 + uint32_t(n - 96), 1);
        ram.w16(FX_PARAM_CACHE + (uint32_t(n - 96 + 8) << 1), uint32_t(readParameter(uint32_t(n))));
        postSubMask(33);
    }
    if (n >= 104 && !(n > 134)) {
        ram.w8(FX_DIRTY_VARIATION + half(n - 104), 1);
        ram.w16(FX_PARAM_CACHE + 0x40 + (half(n - 104) << 1), uint32_t(readParameter(uint32_t(n))));
        postSubMask(9);
    }
    if (n >= 136 && !(n > 166)) {
        ram.w8(FX_DIRTY_INSERTION + half(n - 136), 1);
        ram.w16(FX_PARAM_CACHE + 0x20 + (half(n - 136) << 1), uint32_t(readParameter(uint32_t(n))));
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
    if (n >= 180 && !(n > 190)) {
        ram.w8(FX_DIRTY_EQ + uint32_t(n - 180), 1);
        ram.w16(FX_PARAM_CACHE + 0x60 + (uint32_t(n - 180) << 1), uint32_t(readParameter(uint32_t(n))));
        postSubMask(257);
    }
}

// flash 0x3C8D4 setSubInsCtlOffset: a controller's offset for insertion parameter i
void Host::setSubInsCtlOffset(uint32_t i, uint32_t v)
{
    ram.w16(FX_VAR_WORD + (sh::u8(i) << 1), v);
    ram.w8(FX_DIRTY_INSERTION + sh::u8(i), 1);
    postSubMask(3);
}
// flash 0x3C8EC setSubSendBOffset: a controller's offset for the insertion to variation send
void Host::setSubSendBOffset(uint32_t v)
{
    ram.w16(FX_SENDB_OFFSET, v);
    ram.w8(FX_DIRTY_MIXER + 17, 1);
    postSubMask(513);
}
// flash 0x3C8FE setSubSendAOffset: the same for the insertion to reverb send
void Host::setSubSendAOffset(uint32_t v)
{
    ram.w16(FX_SENDA_OFFSET, v);
    ram.w8(FX_DIRTY_MIXER + 16, 1);
    postSubMask(513);
}
void Host::clearSubSendBOffset()                                               // flash 0x3C932
{
    ram.w16(FX_SENDB_OFFSET, 0);
    ram.w8(FX_DIRTY_MIXER + 17, 1);
    postSubMask(513);
}
void Host::clearSubSendAOffset()                                               // flash 0x3C980
{
    ram.w16(FX_SENDA_OFFSET, 0);
    ram.w8(FX_DIRTY_MIXER + 16, 1);
    postSubMask(513);
}

}  // namespace fs1r::vop3host

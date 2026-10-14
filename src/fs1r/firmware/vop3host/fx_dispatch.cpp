// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Parameter entry and the per-kind dispatch.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {

// A kind's preset blocks in the EPROM: `base + s16(index[lsb] * stride)` holds the block
// (step words in list order, slot words in list order, then 4 x 4 LFO words for kinds 1-4);
// `params + lsb * 0x20` holds the 16 default parameters.
struct Preset {
    uint32_t base, index, params;
};

constexpr Preset INSERTION_DEFAULT{0x35ECB4, 0x3619C2, 0x3612B4};
Preset insertion_preset(uint32_t msb)
{
    switch (msb) {
    case 0: return INSERTION_DEFAULT;
    case 1: return {0x35F634, 0x3619D4, 0x3614F4};
    case 2: return {0x3619FC, 0x3635DC, 0x36319C};
    case 3: return {0x363AA0, 0x364F1D, 0x3650EA};
    case 4: return {0x361D8C, 0x3635DF, 0x3631FC};
    case 5: return {0x36211C, 0x3635E7, 0x3632FC};
    case 7: return {0x35FD54, 0x3619DA, 0x3615B4};
    case 10: return {0x3642F0, 0x364F32, 0x36538A};
    case 12: return {0x364550, 0x364F34, 0x3653CA};
    case 13: return {0x3647B0, 0x364F37, 0x36542A};
    default: return INSERTION_DEFAULT;
    }
}

constexpr Preset DELAY_DEFAULT{0x360474, 0x3619E1, 0x361694};
Preset reverb_delay_preset(uint32_t msb)
{
    switch (msb) {
    case 0: return DELAY_DEFAULT;
    case 8: return {0x360D0C, 0x3619F5, 0x361914};
    default: return DELAY_DEFAULT;
    }
}

Preset variation_preset(uint32_t msb)
{
    switch (msb) {
    case 0: return DELAY_DEFAULT;
    case 1: return {0x36085C, 0x3619EF, 0x361854};
    case 2: return {0x36283C, 0x3635EF, 0x3633FC};
    case 3: return {0x3629CC, 0x3635F3, 0x36347C};
    case 4: return {0x362F44, 0x3635F7, 0x3634FC};
    case 5: return {0x362CEC, 0x3635FB, 0x36357C};
    case 6: return {0x360B7C, 0x3619F3, 0x3618D4};
    case 7: return {0x3656E0, 0x3657A8, 0x3657AC};   // index table is s8 and the offset wraps at a byte
    case 8: return {0x360D0C, 0x3619F5, 0x361914};
    case 9: return {0x364B40, 0x364F3C, 0x3654CA};
    case 10: return {0x364CD0, 0x364F40, 0x36554A};
    default: return DELAY_DEFAULT;
    }
}

}  // namespace

// 0x39B09C fxParamEntry(kind, p): p >= 0xFE resets the window first; p >= 0xFC reloads the
// kind's preset block for its current type bytes (odd p also restores the 16 default
// parameters) and then runs the kind's handler for "all parameters" (255); any other p is a
// single parameter change for the kind's handler.
void Host::fxParamEntry(int kind, int p)
{
    kind = int8_t(kind);
    p &= 0xFF;
    if (p >= 254)
        fxResetWindow(kind);
    if (p < 252) {
        fxDispatchKind(kind, p);
        return;
    }

    auto copy_steps = [&](uint32_t& blk, uint32_t n, uint32_t list) {
        for (uint32_t i = 0; i < n; i++, blk += 2)
            set_coef(rom::u16(list + 2 * i), rom::u16(blk));
    };
    auto copy_slots = [&](uint32_t& blk, uint32_t n, uint32_t list) {
        for (uint32_t i = 0; i < n; i++, blk += 2)
            set_offs(rom::u8(list + i), rom::s16(blk));
    };
    auto copy_lfo = [&](uint32_t& blk, uint32_t ids) {
        for (uint32_t cache : {FX_LFO24, FX_LFO25, FX_LFO26, FX_LFO27})
            for (uint32_t i = 0; i < 4; i++, blk += 2)
                ram.w16(cache + 2 * rom::u8(ids + i), rom::u16(blk));
    };
    auto copy_params = [&](uint32_t src, uint32_t n) {
        if (p & 1)
            for (uint32_t i = 0; i < n; i++)
                set_param(kind, int(i), rom::u16(src + 2 * i));
    };
    // the block for a kind whose type is (msb, lsb) through its index table, s16 stride product
    auto block = [&](const Preset& pr, uint32_t lsb, uint32_t stride) -> uint32_t {
        return pr.base + uint32_t(int16_t(rom::u8(pr.index + lsb) * stride));
    };

    switch (kind) {
    case 0: {
        const uint32_t type = ram.u8(FX_TYPE0);
        uint32_t blk = rom::FX_PRESET_REVERB + uint32_t(int16_t(rom::u8(rom::FX_PRESET_INDEX_REVERB + type) * 296));
        copy_steps(blk, 0x5E, rom::FX_STEPS_REVERB);
        copy_slots(blk, 0x36, rom::FX_SLOTS_REVERB);
        copy_params(0x364F4A + type * 0x20, 16);
        break;
    }
    case 1: {
        const uint32_t lsb = ram.u8(FX_TYPE1 + 1);
        const uint32_t idx = rom::u8(0x3619B4 + lsb);
        uint32_t blk = 0x35EA20 + uint32_t(int16_t(idx * 132));
        copy_steps(blk, 0x28, rom::FX_STEPS_AUX);
        copy_slots(blk, 10, rom::FX_SLOTS_AUX);
        copy_lfo(blk, 0x35E178);
        copy_params(0x3610F4 + lsb * 0x20, 16);
        break;
    }
    case 2: {
        const uint32_t msb = ram.u8(FX_TYPE1 + 2), lsb = ram.u8(FX_TYPE1 + 3);
        const Preset pr = insertion_preset(msb);
        uint32_t blk = block(pr, lsb, 304);
        copy_steps(blk, 0x60, rom::FX_STEPS_INSERTION);
        copy_slots(blk, 0x28, rom::FX_SLOTS_INSERTION);
        copy_lfo(blk, 0x35E264);
        copy_params(pr.params + lsb * 0x20, 16);
        break;
    }
    case 3: {
        const uint32_t msb = ram.u8(FX_TYPE1 + 4), lsb = ram.u8(FX_TYPE1 + 5);
        const Preset pr = reverb_delay_preset(msb);
        uint32_t blk = block(pr, lsb, 200);
        copy_steps(blk, 0x48, rom::FX_STEPS_REVERB_DELAY);
        copy_slots(blk, 0x0C, rom::FX_SLOTS_REVERB_DELAY);
        copy_lfo(blk, 0x35E304);
        copy_params(pr.params + lsb * 0x20, 16);
        break;
    }
    case 4: {
        const uint32_t msb = ram.u8(FX_TYPE1 + 6), lsb = ram.u8(FX_TYPE1 + 7);
        const Preset pr = variation_preset(msb);
        uint32_t blk;
        if (msb == 7)
            blk = pr.base + (((uint32_t(rom::s8(pr.index + lsb)) & 0xFFFF) * 200) & 0xFF);
        else
            blk = block(pr, lsb, 200);
        copy_steps(blk, 0x48, rom::FX_STEPS_VARIATION);
        copy_slots(blk, 0x0C, rom::FX_SLOTS_VARIATION);
        copy_lfo(blk, 0x35E3A4);
        copy_params(pr.params + lsb * 0x20, 16);
        break;
    }
    case 5:
        if (p & 1)
            for (uint32_t i = 0; i < 20; i++)
                ram.w16(FX_PARAMS_EQ + 2 * i, rom::u16(0x36580C + 2 * i));
        break;
    default: {
        uint32_t blk = rom::FX_SYSTEM_DEFAULTS;
        copy_steps(blk, 0x58, rom::FX_STEPS_SYSTEM);
        if (p & 1)
            for (uint32_t i = 0; i < 16; i++)
                ram.w16(FX_PARAMS_SYS + 2 * i, rom::u16(0x36566A + 2 * i));
        break;
    }
    }
    fxDispatchKind(kind, 255);
}

// 0x39C0B2 fxDispatchKind
void Host::fxDispatchKind(int kind, int p)
{
    switch (kind) {
    case 0: fxDispatchReverb(p); break;
    case 1: break;                           // the aux window has no parameters
    case 2: fxDispatchInsertion(p); break;
    case 3: fxDispatchReverbDelay(p); break;
    case 4: fxDispatchVariation(p); break;
    case 5: fxMasterEq(p); break;
    default: fxSystemParam(p); break;
    }
}

// 0x39C144 fxDispatchReverb: reverb types 1-8 are halls, 9-12 rooms (jump table 0x39C164)
void Host::fxDispatchReverb(int p)
{
    const uint32_t t = ram.u8(FX_TYPE0) - 1;
    if (t >= 12)
        return;
    if (t < 8)
        fxRevHall(p);
    else
        fxRevRoom(p);
}

// 0x39C1AC fxDispatchInsertion: insertion types by (msb, lsb) of the type bytes
void Host::fxDispatchInsertion(int p)
{
    const uint32_t msb = ram.u8(FX_TYPE1 + 2), lsb = ram.u8(FX_TYPE1 + 3);
    switch (msb) {
    case 0:   // jump table 0x39C1D8
        if (lsb >= 2 && lsb <= 9) fxInsChorus(p);
        else if (lsb >= 10 && lsb <= 12) fxInsFlanger(p);
        else if (lsb == 13) fxInsSymphonic(p);
        else if (lsb == 14) fxInsDelayLR(p);
        else if (lsb == 15) fxInsDelayLCR(p);
        else if (lsb == 16) fxInsEcho(p);
        else if (lsb == 17) fxInsEr1(p);
        break;
    case 1:
        if (lsb == 1) fxInsRotarySp(p);
        else if (lsb == 2) fxIns2WayRotary(p);
        else if (lsb == 3) fxInsTremolo(p);
        else if (lsb == 4) fxInsAutoPan(p);
        else if (lsb == 5) fxInsAmbience(p);
        break;
    case 2:
        if (lsb == 1) fxInsPhaser1(p);
        else if (lsb == 2) fxInsPhaser2(p);
        break;
    case 3:
        if (lsb >= 1 && lsb <= 4) fxInsCrossDelayEr(p);
        break;
    case 4:
        if (lsb == 3 || lsb == 4) fxInsAutoWahDist(p);
        else if (lsb == 6 || lsb == 7) fxInsTouchWahDist(p);
        break;
    case 5:
        if (lsb == 1 || lsb == 2) fxInsDistortion(p);
        else if (lsb == 3) fxInsAmpSim(p);
        else if (lsb == 4) fxInsCompDist(p);
        else if (lsb == 5) fxInsCompDistDelay(p);
        else if (lsb == 6) fxInsHmEnhancer(p);
        else if (lsb == 7) fxInsNoiseGate(p);
        break;
    case 7:
        if (lsb == 1) fxInsPitchChange(p);
        else if (lsb == 3) fxInsEnsDetune(p);
        else if (lsb == 6) fxIns3BandEq(p);
        break;
    case 10:
        if (lsb == 1) fxInsLoFi(p);
        break;
    case 12:
        if (lsb == 1 || lsb == 2) fxInsWahDistDelay(p);
        break;
    case 13:
        if (lsb == 1 || lsb == 2) fxInsCompressor(p);
        else if (lsb == 3 || lsb == 4) fxInsDistDelay(p);
        break;
    }
}

// 0x39C48A fxDispatchReverbDelay: reverb types 13-16 are (msb 8, lsb 1-4)
void Host::fxDispatchReverbDelay(int p)
{
    const uint32_t msb = ram.u8(FX_TYPE1 + 4), lsb = ram.u8(FX_TYPE1 + 5);
    if (msb != 8)
        return;
    if (lsb == 1) fxRevDelayLCR(p);
    else if (lsb == 2) fxRevDelayLR(p);
    else if (lsb == 3) fxRevEcho(p);
    else if (lsb == 4) fxRevCrossDelay(p);
}

// 0x39C51C fxDispatchVariation: variation types by (msb, lsb)
void Host::fxDispatchVariation(int p)
{
    const uint32_t msb = ram.u8(FX_TYPE1 + 6), lsb = ram.u8(FX_TYPE1 + 7);
    switch (msb) {
    case 0:   // jump table 0x39C548; lsb 1 (No Effect) returns
        if (lsb >= 2 && lsb <= 9) fxVarChorus(p);
        else if (lsb >= 10 && lsb <= 12) fxVarFlanger(p);
        else if (lsb == 13) fxVarSymphonic(p);
        break;
    case 1:
        if (lsb == 1) fxVarRotarySp(p);
        else if (lsb == 2) fxVarTremolo(p);
        else if (lsb == 3) fxVarAutoPan(p);
        break;
    case 2:
        if (lsb == 1 || lsb == 2) fxVarDistortion(p);
        else if (lsb == 3) fxVarAmpSim(p);
        break;
    case 3:
        if (lsb == 1) fxVarHmEnhancer(p);
        else if (lsb == 2) fxVarCompressor(p);
        else if (lsb == 3) fxVarNoiseGate(p);
        break;
    case 4:
        if (lsb == 1) fxVarAutoWah(p);
        else if (lsb == 3) fxVarTouchWah(p);
        break;
    case 5:
        if (lsb == 1) fxVarPhaser1(p);
        else if (lsb == 2) fxVarPhaser2(p);
        break;
    case 6:
        if (lsb == 1) fxVarEnsDetune(p);
        break;
    case 7:
        if (lsb == 2) fxVar3BandEq(p);
        break;
    case 8:
        if (lsb == 1) fxVarDelayLCR(p);
        else if (lsb == 2) fxVarDelayLR(p);
        else if (lsb == 3) fxVarEcho(p);
        else if (lsb == 4) fxVarCrossDelay(p);
        break;
    case 9:
        if (lsb >= 1 && lsb <= 3) fxVarKaraoke(p);
        break;
    case 10:
        if (lsb >= 1 && lsb <= 8) fxVarReverb(p);
        break;
    }
}

}  // namespace fs1r::vop3host

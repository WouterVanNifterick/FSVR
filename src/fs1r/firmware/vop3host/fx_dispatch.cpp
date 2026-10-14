// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Parameter entry and the per-kind dispatch.
#include "effect_host.h"
#include <algorithm>

namespace fs1r::vop3host {

// the presets of a window's current type msb
const rom::PresetGroup& EffectHost::presets(int kind) const
{
    switch (kind) {
    case REVERB: return rom::REVERB_PRESETS;
    case AUX: return rom::AUX_PRESETS;
    case INSERTION: return rom::INSERTION_PRESETS[code[INSERTION].msb];
    case REVERB_DELAY: return rom::REVERB_DELAY_PRESETS[code[REVERB_DELAY].msb];
    default: return rom::VARIATION_PRESETS[code[VARIATION].msb];
    }
}

// 0x39B09C applyParam(kind, p): p >= 0xFE resets the window first; p >= 0xFC reloads the
// kind's preset block for its current type (odd p also restores the 16 default parameters)
// and then runs the kind's handler for "all parameters" (255); any other p is a single
// parameter change for the kind's handler.
void EffectHost::applyParam(int kind, int p)
{
    kind = int8_t(kind);
    p &= 0xFF;
    if (p >= 254)
        clearWindow(kind);
    if (p < 252) {
        dispatch(kind, p);
        return;
    }
    if (kind >= REVERB && kind <= VARIATION) {
        // a block: the window's constants in the order of its step list, its delay offsets, then
        // the four LFO registers of its four LFOs
        const Window& w = WINDOWS[kind];
        const rom::PresetGroup& g = presets(kind);
        const uint32_t lsb = code[kind].lsb;
        const uint16_t* v = g.blocks + g.blockOf[lsb] * (w.nsteps + w.nslots + (w.lfos ? 16 : 0));
        for (int i = 0; i < w.nsteps; i++)
            coef[w.steps[i]] = *v++;
        for (int i = 0; i < w.nslots; i++)
            delay[w.slots[i]] = int16_t(*v++);
        if (w.lfos)
            for (auto& reg : lfoReg)
                for (int i = 0; i < 4; i++)
                    reg[w.lfos[i]] = *v++;
        if (p & 1)
            std::copy_n(g.defaults[lsb], 16, param[kind]);
    } else if (kind == MASTER_EQ) {
        if (p & 1)
            std::copy_n(rom::EQ_DEFAULTS, 20, eqParam);
    } else {
        for (int i = 0; i < 88; i++)
            coef[rom::STEPS_SYSTEM[i]] = rom::SYSTEM_COEF_DEFAULTS[i];
        if (p & 1)
            std::copy_n(rom::SYSTEM_PARAM_DEFAULTS, 16, mixParam);
    }
    dispatch(kind, 255);
}

// 0x39C0B2 dispatch
void EffectHost::dispatch(int kind, int p)
{
    switch (kind) {
    case 0: dispatchReverb(p); break;
    case 1: break;                           // the aux window has no parameters
    case 2: dispatchInsertion(p); break;
    case 3: dispatchReverbDelay(p); break;
    case 4: dispatchVariation(p); break;
    case 5: masterEq(p); break;
    default: mixerParam(p); break;
    }
}

// 0x39C144 dispatchReverb: reverb types 1-8 are halls, 9-12 rooms (jump table 0x39C164)
void EffectHost::dispatchReverb(int p)
{
    const uint32_t t = code[REVERB].lsb - 1;
    if (t >= 12)
        return;
    if (t < 8)
        fxRevHall(p);
    else
        fxRevRoom(p);
}

// 0x39C1AC dispatchInsertion: insertion types by (msb, lsb) of the type bytes
void EffectHost::dispatchInsertion(int p)
{
    const uint32_t msb = code[INSERTION].msb, lsb = code[INSERTION].lsb;
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

// 0x39C48A dispatchReverbDelay: reverb types 13-16 are (msb 8, lsb 1-4)
void EffectHost::dispatchReverbDelay(int p)
{
    const uint32_t msb = code[REVERB_DELAY].msb, lsb = code[REVERB_DELAY].lsb;
    if (msb != 8)
        return;
    if (lsb == 1) fxRevDelayLCR(p);
    else if (lsb == 2) fxRevDelayLR(p);
    else if (lsb == 3) fxRevEcho(p);
    else if (lsb == 4) fxRevCrossDelay(p);
}

// 0x39C51C dispatchVariation: variation types by (msb, lsb)
void EffectHost::dispatchVariation(int p)
{
    const uint32_t msb = code[VARIATION].msb, lsb = code[VARIATION].lsb;
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

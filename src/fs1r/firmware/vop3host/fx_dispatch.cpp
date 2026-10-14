// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Parameter entry, and which handler runs each type.
#include "effect_host.h"
#include <algorithm>

namespace fs1r::vop3host {

namespace {

// The handler of a type, by the code its chip program is filed under: msb, and a range of lsb
struct Route { uint8_t msb, first, last; EffectHost::Handler run; };

const Route REVERB_ROUTES[] = {
    {0,  1,  8, &EffectHost::hall},                       // Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate
    {0,  9, 12, &EffectHost::room},                       // White Room, Tunnel, Basement, Canyon
};
const Route DELAY_ROUTES[] = {                            // reverb types 13-16, in the delay window
    {8,  1,  1, &EffectHost::delayLCR},
    {8,  2,  2, &EffectHost::delayLR},
    {8,  3,  3, &EffectHost::echo},
    {8,  4,  4, &EffectHost::crossDelay},
};
const Route VARIATION_ROUTES[] = {                        // msb 0 lsb 1 is No Effect
    {0,  2,  9, &EffectHost::chorus},                     // Chorus, Celeste
    {0, 10, 12, &EffectHost::flanger},
    {0, 13, 13, &EffectHost::symphonic},
    {1,  1,  1, &EffectHost::rotarySpeaker},
    {1,  2,  2, &EffectHost::tremolo},
    {1,  3,  3, &EffectHost::autoPan},
    {2,  1,  2, &EffectHost::distortion},                 // Distortion, Overdrive
    {2,  3,  3, &EffectHost::ampSimulator},
    {3,  1,  1, &EffectHost::enhancer},
    {3,  2,  2, &EffectHost::compressor},
    {3,  3,  3, &EffectHost::noiseGate},
    {4,  1,  1, &EffectHost::autoWah},
    {4,  3,  3, &EffectHost::touchWah},
    {5,  1,  1, &EffectHost::phaser1},
    {5,  2,  2, &EffectHost::phaser2},
    {6,  1,  1, &EffectHost::ensembleDetune},
    {7,  2,  2, &EffectHost::threeBandEq},
    {8,  1,  1, &EffectHost::delayLCR},
    {8,  2,  2, &EffectHost::delayLR},
    {8,  3,  3, &EffectHost::echo},
    {8,  4,  4, &EffectHost::crossDelay},
    {9,  1,  3, &EffectHost::karaoke},
    {10, 1,  8, &EffectHost::variationReverb},            // Hall, Room, Stage, Plate
};
const Route INSERTION_ROUTES[] = {
    {0,  2,  9, &EffectHost::chorus},                     // Chorus, Celeste
    {0, 10, 12, &EffectHost::flanger},
    {0, 13, 13, &EffectHost::symphonic},
    {0, 14, 14, &EffectHost::delayLCR},
    {0, 15, 15, &EffectHost::delayLR},
    {0, 16, 16, &EffectHost::echo},
    {0, 17, 17, &EffectHost::crossDelay},
    {1,  1,  1, &EffectHost::rotarySpeaker},
    {1,  2,  2, &EffectHost::twoWayRotary},
    {1,  3,  3, &EffectHost::tremolo},
    {1,  4,  4, &EffectHost::autoPan},
    {1,  5,  5, &EffectHost::ambience},
    {2,  1,  1, &EffectHost::phaser1},
    {2,  2,  2, &EffectHost::phaser2},
    {3,  1,  4, &EffectHost::earlyReflections},           // ER 1, ER 2, Gate Reverb, Reverse Gate
    {4,  3,  4, &EffectHost::autoWah},                    // with distortion, with overdrive
    {4,  6,  7, &EffectHost::touchWah},                   // the same
    {5,  1,  2, &EffectHost::distortion},                 // Distortion, Overdrive
    {5,  3,  3, &EffectHost::ampSimulator},
    {5,  4,  4, &EffectHost::compressor},
    {5,  5,  5, &EffectHost::compressorDistortion},
    {5,  6,  6, &EffectHost::enhancer},
    {5,  7,  7, &EffectHost::noiseGate},
    {7,  1,  1, &EffectHost::pitchChange},
    {7,  3,  3, &EffectHost::ensembleDetune},
    {7,  6,  6, &EffectHost::threeBandEq},
    {10, 1,  1, &EffectHost::loFi},
    {12, 1,  2, &EffectHost::wahDistortionDelay},         // Wah+DS+Dly, Wah+OD+Dly
    {13, 1,  2, &EffectHost::compressorDistortionDelay},  // Cmp+DS+Dly, Cmp+OD+Dly
    {13, 3,  4, &EffectHost::distortionDelay},            // Dist+Delay, Odrv+Delay
};

template <size_t N>
void route(EffectHost& h, int w, int p, const Route (&routes)[N])
{
    const TypeCode c = h.code[w];
    for (const Route& r : routes)
        if (c.msb == r.msb && c.lsb >= r.first && c.lsb <= r.last) {
            (h.*r.run)(w, p);
            return;
        }
}

}  // namespace

// the presets of a window's current type msb
const rom::PresetGroup& EffectHost::presets(int kind) const
{
    switch (kind) {
        case REVERB:       return rom::REVERB_PRESETS;
        case AUX:          return rom::AUX_PRESETS;
        case INSERTION:    return rom::INSERTION_PRESETS[code[INSERTION].msb];
        case REVERB_DELAY: return rom::REVERB_DELAY_PRESETS[code[REVERB_DELAY].msb];
        default:           return rom::VARIATION_PRESETS[code[VARIATION].msb];
    }
}

// The window silenced, its type's preset loaded and every parameter applied; with `defaults` the type's
// sixteen default parameters first.
void EffectHost::resetWindow(int kind, bool defaults)
{
    clearWindow(kind);
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
        if (defaults)
            std::copy_n(g.defaults[lsb], 16, param[kind]);
    } else if (kind == MASTER_EQ) {
        if (defaults)
            std::copy_n(rom::EQ_DEFAULTS, 20, eqParam);
    } else {
        for (int i = 0; i < int(std::size(rom::STEPS_SYSTEM)); i++)
            coef[rom::STEPS_SYSTEM[i]] = rom::SYSTEM_COEF_DEFAULTS[i];
        if (defaults)
            std::copy_n(rom::SYSTEM_PARAM_DEFAULTS, 16, mixParam);
    }
    applyParam(kind, ALL);
}

void EffectHost::applyParam(int kind, int p)
{
    switch (kind) {
        case REVERB:       route(*this, kind, p, REVERB_ROUTES);    break;
        case AUX:                                                   break;   // the aux window has no parameters
        case INSERTION:    route(*this, kind, p, INSERTION_ROUTES); break;
        case REVERB_DELAY: route(*this, kind, p, DELAY_ROUTES);     break;
        case VARIATION:    route(*this, kind, p, VARIATION_ROUTES); break;
        case MASTER_EQ:    masterEq(p);                             break;
        default:           mixerParam(p);                           break;
    }
}

}  // namespace fs1r::vop3host

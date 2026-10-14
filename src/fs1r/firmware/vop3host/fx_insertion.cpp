// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Insertion handlers, part 1: the modulation group, the delays, the early reflections and
// the cross delay.
#include "fx_insertion.h"

namespace fs1r::vop3host {

using namespace ins;

// ---- the early reflection types (msb 3) ----
namespace {
inline int32_t er_row(const EffectHost& h) { return rom::ER_ROWS[6 * lsb(h) + PU(h, 0)]; }
void er_wet(EffectHost& h)
{
    wet_words(h, ER_WET, rom::ER_REV, PU(h, 9), GXGX);
    h.writeCoefs(4, ER_WET);
}
}  // namespace

// the comb-gain table's loss word at x clamped to 0..255
int32_t EffectHost::erGain(int x)
{
    int32_t v = int16_t(x);
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return int16_t(rom::COMB_GAIN[2 * v]);
}

// the 20 early reflections of the pattern row
// (parameter 0, +6 for type lsb 3, +8 for lsb 4) scaled by the density (parameter 1) after the
// pre-delay (parameter 3), gains from the level (parameter 10), clamped to the delay area
void EffectHost::erPattern()
{
    const uint32_t t = lsb(*this);
    const uint32_t rowi = (PU(*this, 0) + (t == 3 ? 6u : t == 4 ? 8u : 0u)) & 0xFFFF;
    const int16_t* row = rom::ER_PATTERNS + rowi * 40;
    const uint32_t density = rom::ER_DENSITY[PU(*this, 1)];
    const int32_t level = int32_t(rom::ER_LEVEL[PU(*this, 10)]) - 128;
    const int32_t base = toSamples(rom::DELAY_WORD[PU(*this, 3)]) + delay[ER_SLOTS[21]];
    const int32_t limit = int32_t(AREA[3] - AREA[2]) - 1;
    for (uint32_t i = 0; i < 20; i++, row += 2) {
        const uint32_t w = uint32_t(scaleDelay(row[0])) & 0xFFFF;
        int32_t pos = base + int32_t(density * w) / 10;
        int32_t gain;
        if (pos > limit) {
            pos = limit;
            gain = 0;
        } else {
            const int32_t g = row[1];
            if (g > 0)
                gain = int16_t(uint32_t(erGain(g + level)));
            else
                gain = -int16_t(uint32_t(erGain(level - g)));
        }
        delay[ER_SLOTS[i]] = pos;
        coef[ER_STEPS[i]] = (gain + (gain < 0 ? 1 : 0)) >> 1;
        if (i == 19)
            delay[ER_SLOTS[20]] = pos > 0x8000 ? 0x8000 : pos;
    }
    coef[ER_STEPS[19]] = 0;
    if (t == 3)
        for (uint32_t k = 15; k <= 18; k++)
            coef[ER_STEPS[k]] = 0;
}

// Cross Delay, ER 2, Gate Rev, Reverse Gate (types 37-40; msb 3 lsb 1-4)
void EffectHost::fxInsCrossDelayEr(int p)
{
    switch (p) {
    case 0:
        erPattern();
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeCoefs(20, ER_STEPS);
        writeDelays(21, ER_SLOTS);
        writeCoefs(16, ER_ROW_STEPS);
        writeDelays(8, ER_ROW_SLOTS);
        break;
    case 1: case 3:
        erPattern();
        writeCoefs(20, ER_STEPS);
        writeDelays(21, ER_SLOTS);
        break;
    case 10:
        erPattern();
        writeCoefs(20, ER_STEPS);
        break;
    case 2: {
        const int32_t diff = int32_t(rom::DIFFUSION[PU(*this, 2)]);
        delay[ER_DIFF_SLOTS[2]] = delay[ER_DIFF_SLOTS[0]] + diff + 1;
        delay[ER_DIFF_SLOTS[3]] = delay[ER_DIFF_SLOTS[1]] + diff + 1;
        writeDelays(2, ER_DIFF_SLOTS + 2);
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeDelays(1, ER_ROW_SLOTS + 7);
        break;
    }
    case 4:
        coef[ER_FEEDBACK[0]] = bipolar500(P(*this, 4));
        writeCoefs(1, ER_FEEDBACK);
        break;
    case 5:
        designFilter(2, 1, HPF1, P(*this, 5), 64, 0, ER_HPF);
        break;
    case 6:
        designFilter(2, 2, LPF1, P(*this, 6), 64, 0, ER_LPF_2);
        break;
    case 9:
        er_wet(*this);
        break;
    case 11:
        diffusers(P(*this, 11), P(*this, 2), er_row(*this), 4, 0, ER_ROW_STEPS, ER_ROW_SLOTS);
        writeCoefs(16, ER_ROW_STEPS);
        break;
    case 12:
        designFilter(1, 0, LPF1, rom::DAMP_FREQ[PU(*this, 12)], 64, 0, ER_LPF);
        break;
    case ALL:
        hold = true;
        for (int q : {0, 2, 4, 5, 6, 9, 12})
            fxInsCrossDelayEr(q);
        hold = false;
        break;
    }
}

}  // namespace fs1r::vop3host

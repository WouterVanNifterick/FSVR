// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Built without FMA contraction, like the hosts.
#include "filters/filter.h"
#include "fs1r/cal.h"
#include <cmath>

namespace {
using fs1r::filters::VoiceFilter;
namespace word = fs1r::word;

// OPEN: the level a channel reaches the filter at. A channel at its clip level is taken to be a full-scale
// serial word. What comes back is scaled so that the passband loses cal::FLT_LOSS, as measured on the unit.
const double TO_FILTER = cal::WORD / cal::CHAN_CLIP;
const double FROM_FILTER = cal::CHAN_CLIP * cal::FLT_LOSS / cal::WORD;

// The unit's serial schedule. The chip runs sixteen voices in four pages over one pass and moves each voice's
// input, EG level and LFO2 in, and its output out, at fixed steps; where that step falls on the wrong side of
// the voice's page, the voice hears or answers a sample late. By the slot's chip channel, (s + 2) & 15, read
// off the firmware's filter program.
struct Late { bool in, eg, lfo, out; };
constexpr Late LATE[16] = {
    {0,0,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,1,1,0}, {0,0,0,1}, {0,0,0,1},
    {0,0,1,1}, {0,1,1,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,1,1,1}, {1,1,1,0}, {1,1,1,0},
};
}

// A register write from the host (FilterHost, in the firmware's layout of the constants). Constants land in
// the slot's VoiceFilter; the EG and LFO registers go to the modulators.
void Filter::Bank::write(uint32_t reg, uint32_t value) {
    using namespace vop3;
    const uint16_t v = (uint16_t)value;
    reg &= 0xff;
    if (reg == SELECT) { select = v; return; }
    const int step = select & 511;
    if (select & AUTO_INCREMENT) select = (uint16_t)((select & ~511) | ((step + 1) & 511));
    if (reg == COEF) coef(step, v);
    else mod.write(reg, step, v);   // CONFIG, the slots and the rest mean nothing here
}

void Filter::Bank::clear(int s) {
    VoiceFilter& f = voice[s];
    f.x1 = f.s2 = f.s3 = f.s4 = f.s5 = 0.0;
}

// A constant: the program's shared ones in steps 11 to 14, else one of a slot's words in its page.
void Filter::Bank::coef(int step, uint16_t v) {
    using namespace word;
    const double k = int16_t(v) / 32768.0;
    if (step < 16) {
        if (step == 11) shared.c63 = cell(wrap_sum(128 * k + 0.0));
        if (step == 12) shared.eg_zero = wrap(128 * k + 0.0);
        if (step == 13) shared.c62 = cell(wrap_sum(128 * k + 0.0));
        if (step == 14) shared.c61 = cell(wrap_sum(128 * k + 0.0));
        for (VoiceFilter& f : voice) f.memo_fc = NAN;
        return;
    }
    // slot s has the page at 16 + 124 * (((s >> 2) + 1) & 3) and the lane s & 3 in it
    const int g = (step - 16) / 124, rel = (step - 16) % 124;
    VoiceFilter* lanes = voice + 4 * ((g + 3) & 3);
    struct Word { int first, stride; double VoiceFilter::*field; };
    static constexpr Word WORDS[] = {
        {0, 3, &VoiceFilter::eg_depth},   {1, 3, &VoiceFilter::lfo_depth}, {2, 3, &VoiceFilter::cutoff},
        {12, 1, &VoiceFilter::exp_slope}, {28, 1, &VoiceFilter::type_k},   {32, 1, &VoiceFilter::reso_a},
        {40, 1, &VoiceFilter::reso_bias}, {44, 1, &VoiceFilter::fb_scale}, {52, 4, &VoiceFilter::gain},
        {54, 4, &VoiceFilter::reso_b},    {71, 4, &VoiceFilter::stage_k},  {84, 2, &VoiceFilter::damp_k},
        {96, 1, &VoiceFilter::pole_k},    {116, 1, &VoiceFilter::ramp},
    };
    for (const Word& w : WORDS) {
        const int i = rel - w.first;
        if (i >= 0 && i % w.stride == 0 && i / w.stride < 4) { VoiceFilter& f = lanes[i / w.stride]; f.*w.field = k; f.memo_fc = NAN; return; }
    }
    // the rest: the second serial bank's gain, always 0 on the FS1R, and words no step multiplies by
}

// The slot's output: which tap the type reads, and its scale. The BPF's is x1, the others x2.
void Filter::Bank::setType(int s, int type) {
    using namespace fs1r::vop3host::filter;
    VoiceFilter& f = voice[s];
    f.route = type == BPF ? 1 : 2;
    // ponytail: a type byte past BEF makes the unit read whatever register the byte names, which is another
    // voice's state and not a filter. It plays silence here (docs/Differences.md).
    static constexpr VoiceFilter::Tap TAP[6] = {VoiceFilter::LPF24, VoiceFilter::LPF18, VoiceFilter::LPF12,
                                                 VoiceFilter::HPF, VoiceFilter::BPF, VoiceFilter::BEF};
    f.tap = type >= 0 && type <= BEF ? TAP[type] : VoiceFilter::NONE;
}

void Filter::Bank::sync(int p, const uint8_t* voiceBytes, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed) {
    for (int i = 0; i < host.VOICE_BYTES; i++) host.setByte(host.VOICE + i, p, voiceBytes[i]);
    for (int i = 0; i < host.PART_BYTES; i++) host.setByte(host.PART + i, p, rec[i]);
    if (cutoffMod != host.partCutoffMod[p]) host.setPartCutoffMod(p, cutoffMod);
    if (lfo2Depth != host.partLfo2Depth[p]) host.setPartLfo2Depth(p, lfo2Depth);
    if (lfo2Speed != host.partLfo2Speed[p]) host.setPartLfo2Speed(p, lfo2Speed);
    host.flush();
}

// The modulators step, then every slot's voice. An EG that ended its segment is served in the sample it reports.
void Filter::Bank::run(double v[SLOTS]) {
    mod.step();
    for (int s = 0; s < 16; s++) {
        const int ch = (s + 2) & 15;
        const Late& late = LATE[ch];
        const double in = v[s] * TO_FILTER, eg = mod.egOut[ch], lfo = mod.lfoOut[ch];
        const double out = voice[s].run(late.in ? inPrev[s] : in, late.eg ? egPrev[s] : eg, late.lfo ? lfoPrev[s] : lfo, shared);
        inPrev[s] = in; egPrev[s] = eg; lfoPrev[s] = lfo;
        v[s] = (late.out ? outPrev[s] : out) * FROM_FILTER;
        outPrev[s] = out;
    }
    if (mod.read(vop3::EG_STATUS)) host.egInterrupt();
}

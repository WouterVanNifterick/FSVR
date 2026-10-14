// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Built with strict floating point, like the hosts.
#include "fs1r/filter.h"
#include "fs1r/chips/cal.h"

namespace {
// OPEN: the level a channel reaches the chip at. A channel at its clip level is taken to be a full-scale
// serial word. What comes back is scaled so that the passband loses cal::FLT_LOSS, as measured on the unit.
const double IN = Vop3::WORD / cal::CHAN_CLIP;
const double OUT = cal::CHAN_CLIP * cal::FLT_LOSS / Vop3::WORD;
}

void Filter::sync(int p, const uint8_t* voice, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed) {
    for (int i = 0; i < 112; i++)
        if ((voice[i] & 0x7f) != host.voice[p][i]) { host.voice[p][i] = voice[i] & 0x7f; host.mark(host.VOICE + i, p); }
    for (int i = 0; i < 52; i++)
        if ((rec[i] & 0x7f) != host.part[p][i]) { host.part[p][i] = rec[i] & 0x7f; host.mark(host.PART + i, p); }
    if (cutoffMod != host.partCutoffMod[p]) host.setPartCutoffMod(p, cutoffMod);
    if (lfo2Depth != host.partLfo2Depth[p]) host.setPartLfo2Depth(p, lfo2Depth);
    if (lfo2Speed != host.partLfo2Speed[p]) host.setPartLfo2Speed(p, lfo2Speed);
    host.flush();
}

// Slot n is fed on serial slot (n + 2) & 15 and comes back on (n + 12) & 15. An EG that ended its segment
// is served in the sample it reports.
void Filter::run(double v[16]) {
    yss236_core& core = chip.core;
    for (int n = 0; n < 16; n++) core.in[0][(n + 2) & 15] = v[n] * IN;
    chip.run();
    if (chip.read(fs1r::vop3host::EG_STATUS)) host.egInterrupt();
    for (int n = 0; n < 16; n++) v[n] = core.out[(n + 12) & 15] * OUT;
}

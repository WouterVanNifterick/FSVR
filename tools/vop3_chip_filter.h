// vop3_chip_filter.h - the voice filters as the FS1R runs them: the filter host driving an emulated VOP3-1,
// the firmware's program on the interpreter. The reference filters/filter.h (VoiceFilter, no chip) is checked
// against bit for bit by vop3_native_check. Not built into the engine.
#pragma once
#include <cstdint>
#include "fs1r/chips/cal.h"
#include "fs1r/chips/vop3.h"
#include "fs1r/firmware/vop3host/filter_host.h"

struct ChipFilter final : fs1r::vop3host::WriteSink {
    fs1r::vop3host::FilterHost host;
    Vop3 chip;

    ChipFilter() { host.sink = this; host.boot(); reset(); }
    ChipFilter(const ChipFilter&) = delete;
    ChipFilter& operator=(const ChipFilter&) = delete;
    void write(uint32_t reg, uint32_t v) override { chip.write(reg, (uint16_t)v); }
    uint32_t read(uint32_t reg) override { return chip.read(reg); }
    void reset() { host.freeSlots(); host.clearStates(); }
    void sync(int p, const uint8_t* voice, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed) {
        for (int i = 0; i < host.VOICE_BYTES; i++) host.setByte(host.VOICE + i, p, voice[i]);
        for (int i = 0; i < host.PART_BYTES; i++) host.setByte(host.PART + i, p, rec[i]);
        if (cutoffMod != host.partCutoffMod[p]) host.setPartCutoffMod(p, cutoffMod);
        if (lfo2Depth != host.partLfo2Depth[p]) host.setPartLfo2Depth(p, lfo2Depth);
        if (lfo2Speed != host.partLfo2Speed[p]) host.setPartLfo2Speed(p, lfo2Speed);
        host.flush();
    }
    // Slot n is fed on serial slot (n + 2) & 15 and comes back on (n + 12) & 15.
    void run(double v[16]) {
        const double TO_CHIP = Vop3::WORD / cal::CHAN_CLIP, FROM_CHIP = cal::CHAN_CLIP * cal::FLT_LOSS / Vop3::WORD;
        yss236_core& core = chip.core;
        for (int n = 0; n < 16; n++) core.in[0][(n + 2) & 15] = v[n] * TO_CHIP;
        chip.run();
        if (chip.read(vop3::EG_STATUS)) host.egInterrupt();
        for (int n = 0; n < 16; n++) v[n] = core.out[(n + 12) & 15] * FROM_CHIP;
    }
};

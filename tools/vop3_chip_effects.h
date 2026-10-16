// vop3_chip_effects.h - the effect section as the FS1R runs it: the effect host driving an emulated VOP3-2,
// the firmware's programs on the chip interpreter. The reference effects/effects.h (Effects, no chip) is
// checked against, every sample (tools/vop3_native_check), the way vop3_chip_filter.h is for the filter.
#pragma once
#include <cstdint>
#include "fs1r/chips/cal.h"
#include "fs1r/chips/vop3.h"
#include "fs1r/firmware/vop3host/effect_host.h"

struct ChipEffects final : fs1r::vop3host::WriteSink {
    fs1r::vop3host::EffectHost host;
    Vop3 chip;

    ChipEffects() { host.sink = this; host.init(); }
    ChipEffects(const ChipEffects&) = delete;
    ChipEffects& operator=(const ChipEffects&) = delete;
    void write(uint32_t reg, uint32_t v) override { chip.write(reg, (uint16_t)v); }
    void load(const uint8_t* common) { host.load(common); }
    // The board around the chip. Serial channels k and k + 4 are the two halves of a frame and the left DAC
    // plays channel 0, so a bus's left goes to channel k + 4.
    void run(const double bus[8], double& l, double& r) {
        const double BUS = cal::OUT_GAIN / cal::FX_DRY_GAIN * Vop3::WORD;
        yss236_core& core = chip.core;
        for (int b = 0; b < 4; b++) {
            core.in[0][2 * (4 + b)] = core.in[0][2 * (4 + b) + 1] = bus[2 * b] * BUS;
            core.in[0][2 * b] = core.in[0][2 * b + 1] = bus[2 * b + 1] * BUS;
        }
        chip.run();
        l = yss236_core::dac(core.out[0]) / Vop3::WORD;
        r = yss236_core::dac(core.out[8]) / Vop3::WORD;
    }
};

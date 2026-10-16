// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Built without FMA contraction, like the host.
#include "fs1r/effects.h"
#include <cstring>
#include "fs1r/chips/cal.h"
#include "fs1r/native/native.h"

namespace vop3host = fs1r::vop3host;
namespace native = fs1r::native;

namespace {
const int TYPE_BYTE[3] = {vop3host::REVERB_TYPE, vop3host::VARIATION_TYPE, vop3host::INSERTION_TYPE};
// Bus sample to serial word, so that a dry performance leaves the chip at cal::OUT_GAIN.
const double BUS = cal::OUT_GAIN / cal::FX_DRY_GAIN * Vop3::WORD;
}

void Effects::sync(uint8_t* c) {
    uint8_t was[vop3host::COMMON_BYTES]; std::memcpy(was, host.perf, sizeof was);
    bool type = false;
    for (int i : TYPE_BYTE)                      // types first: they load defaults, which the other changes then override
        if (c[i] != was[i]) { host.setByte(i, was[i] = c[i]); type = true; }
    for (uint32_t i = 0; i < vop3host::COMMON_BYTES; i++)
        if ((c[i] & 0x7f) != was[i]) { host.store(i, c[i]); host.byteChanged(i); }
    host.runPosted();
    const uint32_t from = vop3host::REVERB_PARAM_BYTES, to = vop3host::EQ_HIGH_SHAPE + 1;   // every effect byte
    if (type) std::memcpy(c + from, host.perf + from, to - from);   // the new type's defaults
}

void Effects::control(int dest, int offset) {
    int16_t* at = dest == 15 ? &host.sendReverbOffset : &host.sendVariationOffset;
    int param = 0;
    if (dest <= 14) {                            // an insertion parameter, by the type's row of the controller map
        param = vop3host::rom::INSERTION_CONTROL[host.readParameter(vop3host::INSERTION_TYPE)][dest - 1];
        if (param == 0xff) return;
        at = &host.controlOffset[param];
    }
    if (*at == offset) return;
    if (dest <= 14) host.setControlOffset(param, offset);
    else if (dest == 15) host.setSendReverbOffset(offset);   // insertion to reverb
    else host.setSendVariationOffset(offset);                   // insertion to variation
    host.runPosted();
}

// The board around the chip. Serial channels k and k + 4 are the two halves of a frame and the
// left DAC plays channel 0, so a bus's left goes to channel k + 4.
void Effects::run(const double bus[8], double& l, double& r) {
    yss236_core& core = chip.core;
    if (chip.programDirty) {                 // the host uploaded a program: pick our steps to match it
        chip.programDirty = false;
        native::install_effect(core);
    }
    for (int b = 0; b < 4; b++) {
        core.in[0][2 * (4 + b)] = core.in[0][2 * (4 + b) + 1] = bus[2 * b] * BUS;
        core.in[0][2 * b] = core.in[0][2 * b + 1] = bus[2 * b + 1] * BUS;
    }
    chip.run();
    l = yss236_core::dac(core.out[0]) / Vop3::WORD;
    r = yss236_core::dac(core.out[8]) / Vop3::WORD;
}

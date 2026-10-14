// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Built with strict floating point, like the host.
#include "fs1r/effects.h"
#include <cstring>
#include "fs1r/chips/cal.h"

namespace vop3 = fs1r::vop3host;

namespace {
const int TYPE_BYTE[3] = {168, 171, 175};       // reverb, variation, insertion
// Bus sample to serial word, so that a dry performance leaves the chip at cal::OUT_GAIN.
const double BUS = cal::OUT_GAIN / cal::FX_DRY_GAIN * Vop3::WORD;
}

void Effects::sync(uint8_t* c) {
    uint8_t was[192]; std::memcpy(was, host.perf, 192);
    bool type = false;
    for (int i : TYPE_BYTE)                      // types first: they load defaults, which the other changes then override
        if (c[i] != was[i]) { host.setByte(i, was[i] = c[i]); type = true; }
    for (int i = 0; i < 192; i++)
        if ((c[i] & 0x7f) != was[i]) { host.store(i, c[i]); host.byteChanged(i); }
    host.runPosted();
    if (type) std::memcpy(c + 80, host.perf + 80, 111);   // the new type's defaults
}

void Effects::setType(Block b, int type) { host.setByte(TYPE_BYTE[b], type); }

// Reverb slots 1-8 are two bytes at 0x50 and 9-16 one byte at 0x60. Variation and insertion slots are pairs at 0x68 and 136.
void Effects::setParam(Block b, int slot, int word) {
    if (b == REVERB && slot > 8) { host.setByte(0x60 + slot - 9, word); return; }
    int i = (b == REVERB ? 0x50 : b == VARIATION ? 0x68 : 136) + 2 * (slot - 1);
    host.store(i + 1, word & 0x7f);
    host.setByte(i, word >> 7);
}

void Effects::control(int dest, int offset) {
    int16_t* at = dest == 15 ? &host.sendReverbOffset : &host.sendVariationOffset;
    int param = 0;
    if (dest <= 14) {                            // an insertion parameter, by the type's row of the controller map
        param = vop3::rom::INSERTION_CONTROL[host.readParameter(175)][dest - 1];
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
    for (int b = 0; b < 4; b++) {
        core.in[0][2 * (4 + b)] = core.in[0][2 * (4 + b) + 1] = bus[2 * b] * BUS;
        core.in[0][2 * b] = core.in[0][2 * b + 1] = bus[2 * b + 1] * BUS;
    }
    chip.run();
    l = yss236_core::dac(core.out[0]) / Vop3::WORD;
    r = yss236_core::dac(core.out[8]) / Vop3::WORD;
}

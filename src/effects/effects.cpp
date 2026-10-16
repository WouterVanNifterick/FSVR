// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Built without FMA contraction, like the host.
#include "effects/effects.h"
#include <algorithm>
#include <cstring>
#include "fs1r/chips/cal.h"
#include "fs1r/chips/vop3_regs.h"

namespace vop3host = fs1r::vop3host;
namespace effects = fs1r::effects;

namespace {
const int TYPE_BYTE[3] = {vop3host::REVERB_TYPE, vop3host::VARIATION_TYPE, vop3host::INSERTION_TYPE};
// Bus sample to serial word, so that a dry performance leaves the chip at cal::OUT_GAIN.
const double BUS = cal::OUT_GAIN / cal::FX_DRY_GAIN * Vop3::WORD;

// The programs the host loads, by the window that holds them and the type msb they are filed under, with
// each window's clear entry (WINDOWS[w].clearEntry). Nothing else is ever loaded: vop3_native_check runs
// every type.
struct Loadable { int msb; effects::Program run; };
const Loadable DELAY[] = {{0, effects::dly_m00}, {8, effects::dly_m08}, {11, effects::dly_m11}};
const Loadable VARIATION[] = {
    {0, effects::var_m00}, {1, effects::var_m01}, {2, effects::var_m02}, {3, effects::var_m03}, {4, effects::var_m04},
    {5, effects::var_m05}, {6, effects::var_m06}, {7, effects::var_m07}, {8, effects::var_m08}, {9, effects::var_m09},
    {10, effects::var_m10}, {11, effects::var_m11}};
const Loadable INSERTION[] = {
    {0, effects::ins_m00}, {1, effects::ins_m01}, {2, effects::ins_m02}, {3, effects::ins_m03}, {4, effects::ins_m04},
    {5, effects::ins_m05}, {7, effects::ins_m07}, {10, effects::ins_m10}, {12, effects::ins_m12}, {13, effects::ins_m13},
    {14, effects::ins_m14}};
}

// A register write from the host. Program words say which programs to run, the constants are the programs'
// parameters, the slots their delay offsets, and the rest is the LFOs (or nothing the programs see).
void Effects::write(uint32_t reg, uint32_t value) {
    using namespace vop3;
    const uint16_t v = (uint16_t)value;
    reg &= 0xff;
    if (reg == SELECT) { select = v; return; }
    const int step = select & 511;
    if (select & AUTO_INCREMENT) select = (uint16_t)((select & ~511) | ((step + 1) & 511));
    switch (reg) {
        case PROGRAM - 4: case PROGRAM - 3: case PROGRAM - 2: case PROGRAM - 1:
        case PROGRAM:   prog[step][PROGRAM - reg] = v; loaded = false; break;
        case COEF:      state.k[step] = int16_t(v) / 32768.0; break;
        case SLOT_HIGH:
        case SLOT_LOW: {
            uint16_t (&s)[2] = slot[step & 127];
            s[reg - SLOT_HIGH] = v;
            state.offset[step & 127] = (uint32_t(s[0] & 0x1ff) << 9) | (s[1] & 0x1ff);
            break;
        }
        default:        mod.write(reg, step, v); break;   // TAG, CONFIG and the mute mean nothing here
    }
}

// Which program each window holds, from its words
void Effects::identify() {
    auto holds = [&](int w, int msb) {
        const vop3host::Window& win = vop3host::WINDOWS[w];
        for (int i = 0; i < win.nsteps; i++)
            if (std::memcmp(prog[win.steps[i]], win.programs[msb * win.nsteps + i], sizeof prog[0])) return false;
        return true;
    };
    auto find = [&](int w, auto const& list) -> effects::Program {
        for (const Loadable& p : list) if (holds(w, p.msb)) return p.run;
        return nullptr;   // ponytail: a window part-way through an upload runs nothing; the host never runs one
    };
    stage[0] = effects::base;
    stage[1] = effects::aux;
    stage[2] = find(vop3host::REVERB_DELAY, DELAY);
    stage[3] = find(vop3host::VARIATION, VARIATION);
    stage[4] = find(vop3host::INSERTION, INSERTION);
    loaded = true;
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

// One sample. Serial channels k and k + 4 are the two halves of a frame and the left DAC plays channel 0, so a
// bus's left goes to channel k + 4.
void Effects::run(const double bus[8], double& l, double& r) {
    if (!loaded) identify();
    effects::State& s = state;
    for (int b = 0; b < 4; b++) {
        s.in[0][2 * (4 + b)] = s.in[0][2 * (4 + b) + 1] = bus[2 * b] * BUS;
        s.in[0][2 * b] = s.in[0][2 * b + 1] = bus[2 * b + 1] * BUS;
    }
    mod.step();
    std::copy(mod.egOut, mod.egOut + 16, s.in[3]);
    std::copy(mod.lfoOut, mod.lfoOut + 16, s.lfo);
    for (effects::Program p : stage) if (p) p(s);
    s.ptr = (s.ptr - 1) & (effects::State::RAM_WORDS - 1);
    l = fs1r::word::dac(s.out[0]) / Vop3::WORD;
    r = fs1r::word::dac(s.out[8]) / Vop3::WORD;
}

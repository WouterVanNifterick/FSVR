// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's effect section: the firmware's effect host, the LFOs it programs, and the effect programs it
// loads (effects/programs.h), run as plain DSP. No chip is emulated: the host's register writes are read for
// what they mean to the programs. In: a performance's 192 common bytes and four stereo buses. Out: the main
// stereo pair.
#pragma once
#include <cstdint>
#include "modulators/modulators.h"
#include "effects/host/effect_host.h"
#include "effects/programs.h"

struct Effects final : fs1r::vop3host::WriteSink {
    fs1r::vop3host::EffectHost host;
    Modulators mod;                              // the LFOs the host programs
    fs1r::effects::State state;                  // what the programs share
    // the programs loaded: base, aux, reverb delay, variation, insertion
    fs1r::effects::Program stage[5] = {fs1r::effects::base, fs1r::effects::aux};

    Effects() { host.sink = this; host.init(); }
    Effects(const Effects&) = delete;             // the host writes to this section
    Effects& operator=(const Effects&) = delete;
    void write(uint32_t reg, uint32_t v) override;
    void load(int window, int program) override;
    void load(const uint8_t* common) { host.load(common); }   // a whole performance
    void sync(uint8_t* common);                  // what changed since; a new type's defaults are written back
    void control(int dest, int offset);          // controller destinations 1..16, offset -128..127
    void run(const double bus[8], double& l, double& r);   // buses: dry, insertion, variation send, reverb send; L then R each

private:
    uint16_t select = 0;                         // the register latch
    uint16_t slot[128][2] = {};                  // the delay slots' offsets as written, high and low
};

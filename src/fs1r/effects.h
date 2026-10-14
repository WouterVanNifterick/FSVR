// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's effect section: the effect host driving the VOP3 chip.
// In: a performance's 192 common bytes and four stereo buses. Out: the main stereo pair.
#pragma once
#include <cstdint>
#include "chips/vop3.h"
#include "firmware/vop3host/effect_host.h"

struct Effects final : fs1r::vop3host::WriteSink {
    fs1r::vop3host::EffectHost host;
    Vop3 chip;

    Effects() { host.sink = this; host.init(); }
    void write(uint32_t reg, uint32_t v) override { chip.write(reg, (uint16_t)v); }
    void load(const uint8_t* common) { host.load(common); }   // a whole performance
    void sync(uint8_t* common);                  // what changed since; a new type's defaults are written back
    void control(int dest, int offset);          // controller destinations 1..16, offset -128..127
    void run(const double bus[8], double& l, double& r);   // buses: dry, insertion, variation send, reverb send; L then R each
};

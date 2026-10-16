// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The YSS236 ("VOP3") as a chip: its register file, LFOs and EG generators around the microcode core.
// Audio goes in and out through the core's serial slots, core.in and core.out.
#pragma once
#include <cstdint>
#include "vop3_regs.h"
#include "yss236_core.h"

// The chip's sixteen EG generators and sixteen LFOs, as their registers program them. Separate from the
// microcode, so the native filter (filters/voice_filter.h) runs the same modulators with no chip around them.
struct Modulators {
    uint16_t lfoReg[16][4] = {};            // LFO_SPEED, LFO_WAVE, LFO_DEPTH and LFO_DIRECTION
    uint32_t phase[16] = {}, rnd = 1;
    float hold[16] = {};                    // sample and hold
    // An EG generator: its EG_RATE, EG_TARGET and EG_AIM.
    struct Eg { double level = 0, k = 0; int16_t target = 0, aim = 0; uint8_t rate = 0; int8_t side = 0; int flat = 0; };
    Eg eg[16];
    uint16_t done = 0, enable = 0;          // the segments that ended, and EG_STATUS as written
    double egOut[16] = {}, lfoOut[16] = {}; // this sample's levels, in register units (+-128)

    bool write(uint32_t reg, int select, uint16_t v);   // false: not a modulator register
    uint16_t read(uint32_t reg) const;      // EG_STATUS, the one register the hosts read
    void egWrite(int n, uint32_t reg, uint16_t v);
    float lfo(int n) const;                 // an LFO's value, -1..1 times its depth
    void step();                            // one sample of every EG and LFO into egOut and lfoOut
};

struct Vop3 : Modulators {
    static constexpr double WORD = 8.0;     // a full-scale serial word, in the core's units

    yss236_core core;
    uint16_t select = 0;                    // the register latch

    void write(uint32_t reg, uint16_t v);
    void run();                             // one sample: the EGs, the LFOs, then the program over core.in into core.out
};

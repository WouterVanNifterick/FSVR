// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The YSS236 ("VOP3") as a chip: its register file, LFOs and EG generators around the microcode core.
// Audio goes in and out through the core's serial slots, core.in and core.out.
#pragma once
#include <cstdint>
#include "vop3_regs.h"
#include "yss236_core.h"

struct Vop3 {
    static constexpr double WORD = 8.0;     // a full-scale serial word, in the core's units

    yss236_core core;
    uint16_t select = 0;                    // the register latch
    uint16_t lfoReg[yss236_core::LFOS][4] = {};   // LFO_SPEED, LFO_WAVE, LFO_DEPTH and LFO_DIRECTION
    uint32_t phase[yss236_core::LFOS] = {}, rnd = 1;
    float hold[yss236_core::LFOS] = {};     // sample and hold
    // An EG generator: its EG_RATE, EG_TARGET and EG_AIM.
    struct Eg { double level = 0, k = 0; int16_t target = 0, aim = 0; uint8_t rate = 0; int8_t side = 0; int flat = 0; };
    Eg eg[16];
    uint16_t done = 0, enable = 0;          // the segments that ended, and EG_STATUS as written

    void write(uint32_t reg, uint16_t v);
    uint16_t read(uint32_t reg) const;      // EG_STATUS, the one register the hosts read
    void egWrite(int n, uint32_t reg, uint16_t v);
    float lfo(int n) const;                 // an LFO's value, -1..1 times its depth
    void run();                             // one sample: the EGs, the LFOs, then the program over core.in into core.out
};

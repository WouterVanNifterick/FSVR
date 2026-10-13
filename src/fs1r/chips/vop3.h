// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The YSS236 ("VOP3") as a chip: its register file and LFOs around the microcode core.
// Audio goes in and out through the core's serial slots, core.in and core.out.
#pragma once
#include <cmath>
#include <cstdint>
#include "yss236_core.h"

struct Vop3 {
    static constexpr double WORD = 8.0;     // a full-scale serial word, in the core's units

    yss236_core core;
    uint16_t select = 0;                    // the register latch
    uint32_t phase[yss236_core::LFOS] = {}, rnd = 1;
    float hold[yss236_core::LFOS] = {};     // sample and hold

    void write(uint32_t reg, uint16_t v) {
        reg &= 0xff;
        if (reg == 0) { select = v; return; }
        const int step = select & (yss236_core::STEPS - 1);
        if (select & 0x8000) select = (uint16_t)((select & ~(yss236_core::STEPS - 1)) | ((step + 1) & (yss236_core::STEPS - 1)));   // auto-increment
        switch (reg) {
        case 0x06: case 0x07: case 0x08: case 0x09: case 0x0a: core.prog[step][10 - reg] = v; core.decode(step); break;
        case 0x0b: core.coef[step] = v; break;
        case 0x0c: core.tag[step] = (uint8_t)v; break;
        case 0x0d: case 0x0e: core.slot[step & (yss236_core::SLOTS - 1)][reg - 0x0d] = v; break;
        case 0x20:                          // LFOs back to their start phase, register 0x25's low 5 bits
            for (int n = 0; n < yss236_core::LFOS; n++) if (v >> n & 1) phase[n] = (uint32_t)(core.lfo_reg[n][1] & 0x1f) << 27;
            break;
        case 0x24: case 0x25: case 0x26: case 0x27: core.lfo_reg[step & (yss236_core::LFOS - 1)][reg - 0x24] = v; break;
        default: break;                     // serial port setup and the mute: not modelled
        }
    }
    // An LFO's value, -1..1 times its depth. Registers 0x24..0x27: speed, wave and start phase, depth, direction.
    float lfo(int n) const {
        const uint16_t* reg = core.lfo_reg[n];
        const float x = (float)phase[n] * (1.0f / 4294967296.0f);
        float v;
        switch ((reg[1] >> 5) & 7) {
        case 1: v = 1.0f - 2.0f * x; break;
        case 2: v = 2.0f * x - 1.0f; break;
        case 3: v = x < 0.5f ? 1.0f : -1.0f; break;
        case 4: v = std::sin(2.0f * 3.14159265f * x); break;
        case 5: v = hold[n]; break;
        default: v = x < 0.25f ? 4.0f * x : x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f; break;
        }
        if (reg[3] & 0x8000) v = -v;
        return v * ((float)reg[2] * (1.0f / 32768.0f));
    }
    // One sample: step the LFOs, then run the program over core.in into core.out.
    // OPEN: the speed word is 10 bits, cycles per sample * 2^20, and a full LFO fills a register at 128.
    void run() {
        for (int n = 0; n < yss236_core::LFOS; n++) {
            const uint32_t was = phase[n];
            phase[n] = was + ((uint32_t)(core.lfo_reg[n][0] & 0x3ff) << 12);
            if (((core.lfo_reg[n][1] >> 5) & 7) == 5 && phase[n] < was) {
                rnd = rnd * 1664525u + 1013904223u;
                hold[n] = (float)(int16_t)(rnd >> 16) * (1.0f / 32768.0f);
            }
            core.lfo[n] = lfo(n) * 128.0;
        }
        core.pass();
    }
};

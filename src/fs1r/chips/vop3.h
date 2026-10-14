// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The YSS236 ("VOP3") as a chip: its register file, LFOs and EG generators around the microcode core.
// Audio goes in and out through the core's serial slots, core.in and core.out.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../hardware.h"
#include "cal.h"
#include "yss236_core.h"

struct Vop3 {
    static constexpr double WORD = 8.0;     // a full-scale serial word, in the core's units

    yss236_core core;
    uint16_t select = 0;                    // the register latch
    uint32_t phase[yss236_core::LFOS] = {}, rnd = 1;
    float hold[yss236_core::LFOS] = {};     // sample and hold
    bool filter = false;                    // the board's filter chip: picks the reading of register 0x27, see lfo()
    // An EG generator. Registers 0x2B, 0x2C, 0x2D: its rate, the level that ends the segment, the level it heads for.
    struct Eg { double level = 0, k = 0; int16_t target = 0, aim = 0; uint8_t rate = 0; int8_t side = 0; int flat = 0; };
    Eg eg[16];
    uint16_t done = 0, enable = 0;          // the segments that ended, and register 0x21 as written

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
        case 0x14: core.trigger(); break;
        case 0x20:                          // LFOs back to their start phase, register 0x25's low 5 bits
            for (int n = 0; n < yss236_core::LFOS; n++) if (v >> n & 1) phase[n] = (uint32_t)(core.lfo_reg[n][1] & 0x1f) << 27;
            break;
        case 0x21: enable = v; break;
        case 0x24: case 0x25: case 0x26: case 0x27: core.lfo_reg[step & (yss236_core::LFOS - 1)][reg - 0x24] = v; break;
        case 0x2b: case 0x2c: case 0x2d: if (step < 16) egWrite(step, reg, v); break;
        default: break;                     // serial port setup and the mute: not modelled
        }
    }
    // Register 0x21 read: bit 4 and the lowest channel whose segment ended, of those enabled.
    uint16_t read(uint32_t reg) const {
        const uint16_t p = done & enable;
        if ((reg & 0xff) != 0x21 || !p) return 0;
        int n = 0;
        while (!(p >> n & 1)) n++;
        return (uint16_t)(0x10 | n);
    }
    // Rate 0 jumps to the target and 255 holds. Any other rate approaches the aim, covering two thirds of the way
    // in cal::FEG_SEG_S at rate 0 and in twice that every cal::FEG_RATE_OCT. A segment ends when the level
    // crosses its target.
    // OPEN: when a segment arms. Here the target write arms it and the level moves whenever the rate lets it,
    // so a segment whose rate is written last starts as well.
    // OPEN: a segment armed within two units of its target ends after cal::FEG_FLAT_S whatever its rate.
    // That is what the unit was measured to do. How it does it is not known.
    void egWrite(int n, uint32_t reg, uint16_t v) {
        Eg& g = eg[n];
        if (reg == 0x2d) { g.aim = (int16_t)v; return; }
        if (reg == 0x2b) {
            g.rate = (uint8_t)v;
            g.k = 1.0 - std::exp(-std::log(3.0) / (cal::FEG_SEG_S * SR * std::exp2(g.rate / cal::FEG_RATE_OCT)));
            done &= ~(1u << n);             // a rate write acknowledges the segment
        } else g.target = (int16_t)v;
        if (!g.rate) g.level = g.target;
        if (reg == 0x2c) {
            const double d = g.level - g.target;
            g.side = d > 0 ? 1 : d < 0 ? -1 : 0;
            g.flat = g.side && std::fabs(d) <= 2 ? (int)(cal::FEG_FLAT_S * SR) : 0;
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
        // OPEN: register 0x27. The two programs send the same words and cannot mean the same by them: the effect
        // programs' 0x8000 reads as a negation, the filter program's 0x4000 as a halving. Neither is measured.
        if (filter) { if (reg[3] & 0x4000) v *= 0.5f; } else if (reg[3] & 0x8000) v = -v;
        return v * ((float)reg[2] * (1.0f / 32768.0f));
    }
    // One sample: step the EGs and the LFOs, then run the program over core.in into core.out.
    // OPEN: the speed word is 10 bits, cycles per sample * 2^20, and a full LFO fills a register at 128.
    // OPEN: the EGs step once a sample here; the chip's own step rate is not known.
    void run() {
        for (int n = 0; n < 16; n++) {
            Eg& g = eg[n];
            if (g.rate && g.rate != 255) {
                if (g.flat) { if (!--g.flat) g.level = g.target; }
                else g.level = std::clamp(g.level + (g.aim - g.level) * g.k, -512.0, 511.0);
                if (g.side && !g.flat && (g.level - g.target) * g.side <= 0) { g.level = g.target; g.side = 0; done |= 1u << n; }
            }
            core.in[3][n] = g.level * (yss236_core::RANGE / 512);
        }
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

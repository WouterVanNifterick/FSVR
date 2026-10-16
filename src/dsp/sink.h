// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A host's side of its chip: where the writes go, the one register that is read, and the writes both
// hosts make the same way.
#pragma once
#include <cstdint>
#include "fs1r/vop3_regs.h"

namespace fsvr::vop3host {

using namespace ::vop3;     // the chip's registers and LFO waves, by name

struct WriteSink {
    virtual void write(uint32_t reg, uint32_t value) = 0;
    virtual uint32_t read(uint32_t) { return 0; }       // the filter's EG_STATUS
    // Where the firmware uploads DSP program words, the section is told what they mean instead:
    virtual void load(int /*window*/, int /*program*/) {}   // the effect host: a window's program, by type msb
    virtual void setType(int /*slot*/, int /*type*/) {}     // the filter host: a slot's filter type
    virtual void clear(int /*slot*/) {}                     // and a slot's filter state, zeroed
};

// A register, an entry of CONFIG, an LFO register from its shadow, and an LFO phase reset
struct ChipHost {
    WriteSink* sink = nullptr;
    uint16_t lfoReg[4][16] = {};    // LFO_SPEED, LFO_WAVE, LFO_DEPTH and LFO_DIRECTION of the sixteen LFOs

    void writeReg(uint32_t reg, uint32_t value) { sink->write(reg, value & 0xFFFF); }
    void writeConfig(uint32_t index, uint32_t value)
    {
        writeReg(SELECT, index + 1);
        writeReg(CONFIG, value);
    }
    void writeLfo(Reg reg, uint32_t lfo)                // reg is one of the four above
    {
        writeReg(SELECT, lfo);
        writeReg(reg, lfoReg[reg - LFO_SPEED][lfo]);
    }
    // the LFOs of a mask back to their start phase, then register 0x1C cleared, as the firmware does
    void resetLfoPhase(uint32_t mask)
    {
        writeReg(LFO_RESET, mask);
        writeReg(0x1C, 0);
    }
};

}  // namespace fsvr::vop3host

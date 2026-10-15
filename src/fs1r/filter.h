// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's voice filters: the filter host driving the board's first VOP3 chip.
// In: the tone generator's first sixteen channels. Out: the same sixteen, filtered.
#pragma once
#include <cstdint>
#include "chips/vop3.h"
#include "firmware/vop3host/filter_host.h"

struct Filter final : fs1r::vop3host::WriteSink {
    fs1r::vop3host::FilterHost host;
    Vop3 chip;

    Filter() { host.sink = this; host.boot(); reset(); }
    Filter(const Filter&) = delete;                          // the host writes to this board's chip
    Filter& operator=(const Filter&) = delete;
    void write(uint32_t reg, uint32_t v) override { chip.write(reg, (uint16_t)v); }
    uint32_t read(uint32_t reg) override { return chip.read(reg); }
    void reset() { host.freeSlots(); host.clearStates(); }   // as at a performance's start
    // A part's voice and record, and its three controller offsets: what changed is refreshed on the slots it plays.
    void sync(int part, const uint8_t* voice, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed);
    void run(double v[16]);                                  // one sample of the sixteen channels, in place
};

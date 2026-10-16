// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's voice filters: one fs1r::filters::VoiceFilter per tone generator channel, programmed by the
// firmware's filter host. In: the channels' samples. Out: the same channels, filtered. No chip is emulated:
// the host's register writes are read for what they mean to a voice (Filter::write).
//
// The firmware's host drives sixteen slots, the most the unit's one filter chip has. A Filter has 32 voices
// in two banks of sixteen, each with its own host and modulators, so channel n is slot n & 15 of bank n >> 4.
// The unit uses bank 0 only; bank 1 is ours (docs/Differences.md, 32 filtered voices).
#pragma once
#include <cstdint>
#include "fs1r/chips/vop3.h"
#include "fs1r/firmware/vop3host/filter_host.h"
#include "filters/voice_filter.h"

struct Filter {
    static constexpr int SLOTS = 16, BANKS = 2, VOICES = SLOTS * BANKS;

    // A bank: the firmware's host, the sixteen EGs and LFO2s it programs, and sixteen voices.
    struct Bank final : fs1r::vop3host::WriteSink {
        fs1r::vop3host::FilterHost host;
        Modulators mod;
        fs1r::filters::VoiceFilter voice[SLOTS];
        fs1r::filters::VoiceFilter::Shared shared = {};

        Bank() { host.sink = this; host.boot(); reset(); }
        Bank(const Bank&) = delete;                          // the host writes to this bank
        Bank& operator=(const Bank&) = delete;
        void write(uint32_t reg, uint32_t v) override;
        uint32_t read(uint32_t reg) override { return mod.read(reg); }
        void reset() { host.freeSlots(); host.clearStates(); }   // as at a performance's start
        void sync(int part, const uint8_t* voice, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed);
        void run(double v[SLOTS]);                           // one sample of the sixteen slots, in place

    private:
        uint16_t select = 0;
        uint16_t prog[512][5] = {};                          // the program words as written: the taps and the clear
        double inPrev[SLOTS] = {}, egPrev[SLOTS] = {}, lfoPrev[SLOTS] = {}, outPrev[SLOTS] = {};
        void coef(int step, uint16_t v);
        void program(int step);
    };

    Bank bank[BANKS];

    fs1r::vop3host::FilterHost& host(int ch) { return bank[ch >> 4].host; }   // the host of channel ch's slot
    void reset() { for (Bank& b : bank) b.reset(); }
    // A part's voice and record, and its three controller offsets, into the first `banks` banks.
    void sync(int banks, int part, const uint8_t* voice, const uint8_t* rec, int cutoffMod, int lfo2Depth, int lfo2Speed)
    {
        for (int b = 0; b < banks; b++) bank[b].sync(part, voice, rec, cutoffMod, lfo2Depth, lfo2Speed);
    }
    void flush(int banks) { for (int b = 0; b < banks; b++) bank[b].host.flush(); }
    // One sample of the first 16 * banks channels, in place.
    void run(int banks, double* v) { for (int b = 0; b < banks; b++) bank[b].run(v + SLOTS * b); }
};

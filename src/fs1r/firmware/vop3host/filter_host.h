// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's filter host: what the CPU does to run the sixteen voice filters on their chip.
// A slot is one filter. Slot s takes tone generator channel s and runs on the chip's channel (s + 2) & 15.
#pragma once
#include <cstdint>

#include "sink.h"

namespace fs1r::vop3host {

struct FilterHost {
    enum { VOICE = 0x1000, PART = 0x8000 };              // mark's ids: a voice byte or a part byte, plus its place
    enum Stage : uint8_t { START, ATTACK, DECAY1, DECAY2, SUSTAIN, RELEASE, DONE };   // where a slot's EG is

    WriteSink* sink = nullptr;
    uint8_t voice[4][112] = {}, part[4][52] = {};        // each part's voice and record, as far as they are read here

    void boot();                                         // the program; every slot muted and every EG held
    void freeSlots();
    void clearStates();                                  // every filter's state registers
    void noteOn(int slot, int part, int note, int vel);
    void noteOff(int slot);                              // the EG's release
    void kill(int slot);
    void egInterrupt();                                  // the next segment of every EG that reports
    void mark(int id, int part);                         // VOICE + n or PART + n changed
    void setSlotCutoffMod(int slot, int v);
    void setPartCutoffMod(int part, int v);
    void setPartLfo2Depth(int part, int v);
    void setPartLfo2Speed(int part, int v);
    void flush();                                        // applies what was marked

    // ---- state ----
    uint16_t prog[512][5] = {}, coef[512] = {};          // the program and its constants, as uploaded and patched since
    uint8_t tag[512] = {};
    uint16_t lfoReg[4][16] = {};                         // LFO_SPEED..LFO_DIRECTION of each channel
    uint8_t slotPart[16] = {}, note[16] = {}, vel[16] = {};   // what each slot plays; part 16 is a free slot
    uint8_t type[16] = {}, stage[16] = {};               // its filter type, and its EG's Stage
    int8_t slotMod[16] = {}, ksDepth[16] = {};
    uint8_t ksPoint[16] = {};
    int8_t partCutoffMod[4] = {}, partLfo2Depth[4] = {}, partLfo2Speed[4] = {};
    bool typeChanged[4] = {}, cutoffChanged[4] = {}, lfoChanged[4] = {}, egChanged[4] = {}, slotChanged[16] = {};
};

}  // namespace fs1r::vop3host

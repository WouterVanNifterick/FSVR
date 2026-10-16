// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's filter host: what the CPU does to run the sixteen voice filters.
// A slot is one filter. Slot s takes tone generator channel s, which the unit runs on DSP channel (s + 2) & 15.
#pragma once
#include <cstdint>

#include "sink.h"

namespace fs1r::vop3host {

namespace filter {

// The voice bytes the filter reads, by their place in the voice (the Data List's names)
enum VoiceByte {
    LFO2_WAVE        = 0x18,
    LFO2_SPEED       = 0x19,
    LFO2_PHASE       = 0x1C,
    LFO2_KEY_SYNC    = 0x1D,
    FILTER_TYPE      = 0x54,
    RESONANCE        = 0x55,
    RESONANCE_VEL    = 0x56,
    CUTOFF           = 0x57,
    EG_DEPTH_VEL     = 0x58,
    CUTOFF_LFO1      = 0x59,
    CUTOFF_LFO2      = 0x5A,
    KEY_SCALE_DEPTH  = 0x5B,
    KEY_SCALE_POINT  = 0x5C,
    INPUT_GAIN       = 0x5D,
    EG_DEPTH         = 0x64,
    EG_L4            = 0x65,   // where the EG starts and ends
    EG_L1, EG_L2, EG_L3,
    EG_T1, EG_T2, EG_T3, EG_T4,
    EG_SCALING       = 0x6E,   // the time scaling in bits 0-2, the attack's velocity in 3-5
};
// and the part bytes
enum PartByte {
    PART_CUTOFF      = 0x18,
    PART_RESONANCE   = 0x19,
    PART_ATTACK      = 0x1A,
    PART_DECAY       = 0x1B,
    PART_RELEASE     = 0x1C,
    PART_EG_DEPTH    = 0x1F,
    PART_LFO2_SPEED  = 0x2E,
    PART_LFO2_DEPTH  = 0x2F,
};
// and the filter types
enum FilterType { LPF24, LPF18, LPF12, HPF, BPF, BEF };

}  // namespace filter

struct FilterHost : ChipHost {
    enum { VOICE = 0x1000, PART = 0x8000 };              // a byte's id: VOICE or PART plus its place
    static constexpr int VOICE_BYTES = 112, PART_BYTES = 52;   // as far as they are read here
    enum Stage : uint8_t { START, ATTACK, DECAY1, DECAY2, SUSTAIN, RELEASE, DONE };   // where a slot's EG is

    uint8_t voice[4][VOICE_BYTES] = {}, part[4][PART_BYTES] = {};   // each part's voice and record

    void boot();                                         // the constants; every slot muted and every EG held
    void freeSlots();
    void clearStates();                                  // every filter's state
    void noteOn(int slot, int part, int note, int vel);
    void noteOff(int slot);                              // the EG's release
    void kill(int slot);
    void egInterrupt();                                  // the next segment of every EG that reports
    void setByte(int id, int part, int value);           // seven bits; marked when it changed
    void mark(int id, int part);                         // the byte changed
    void setSlotCutoffMod(int slot, int v);
    void setPartCutoffMod(int part, int v);
    void setPartLfo2Depth(int part, int v);
    void setPartLfo2Speed(int part, int v);
    void flush();                                        // applies what was marked

    // ---- state ----
    uint8_t slotPart[16] = {}, note[16] = {}, vel[16] = {};   // what each slot plays; part 16 is a free slot
    uint8_t type[16] = {}, stage[16] = {};               // its filter type, and its EG's Stage
    int8_t slotMod[16] = {}, ksDepth[16] = {};
    uint8_t ksPoint[16] = {};
    int8_t partCutoffMod[4] = {}, partLfo2Depth[4] = {}, partLfo2Speed[4] = {};
    bool typeChanged[4] = {}, cutoffChanged[4] = {}, lfoChanged[4] = {}, egChanged[4] = {}, slotChanged[16] = {};
};

}  // namespace fs1r::vop3host

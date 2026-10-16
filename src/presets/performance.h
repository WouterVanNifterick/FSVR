// presets/performance.h - performance, part and Fseq data. KNOWN.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "presets/voice.h"

struct Fseq {
    bool valid = false; char name[9] = "";
    int nframes = 0, loopStart = 0, loopEnd = 0, loopMode = 0, speedAdj = 64, velTempo = 0, pitchMode = 0, noteAssign = 60, tuning = 63, delay = 0, endStep = 0;
    uint8_t frame[512][50];   // pitch hi/lo, 8 voiced freq hi, 8 lo, 8 voiced level, 8 unvoiced hi, 8 lo, 8 unvoiced level
    void from_bytes(const uint8_t* h, const uint8_t* f, int frames) {
        memcpy(name, h, 8); name[8] = 0;
        loopStart = h[0x10] << 7 | h[0x11]; loopEnd = h[0x12] << 7 | h[0x13]; loopMode = h[0x14] & 1; speedAdj = h[0x15] & 0x7F;
        velTempo = h[0x16] & 7; pitchMode = h[0x17] & 1; noteAssign = h[0x18] & 0x7F; tuning = h[0x19] & 0x7F; delay = h[0x1A];
        nframes = std::min(frames, 128 * ((h[0x1B] & 3) + 1)); endStep = std::min(nframes - 1, h[0x1E] << 7 | h[0x1F]);
        memcpy(frame, f, (size_t)nframes * 50); valid = true;
    }
};
// Performance banks in the EPROM table of 384: the three preset banks, 128 each, in order A, B, C.
// The Data List's performance lists match them entry for entry - "Zap !" is Preset A 1, "Sweepy Voice"
// Preset B 1 and "UprightPiano" Preset C 1 - and the manual's own description of Preset C settles that
// last one on its own: it is the bank for the G50 guitar controller, "the maximum MIDI receive channel
// for these voices is 6, and the pitch bend range is -12 ... +12" (owner's manual page 21), which is
// exactly what entries 256-383 hold and nothing else does. INTERNAL is the user's own battery-backed
// bank, not ROM, so a bank select for it lands on Preset A, which is what the unit ships holding.

struct Part {
    uint8_t p[52]; Voice voice;
    // controller state (MIDI)
    int bend = 0;            // (msb - 64) * 16 + (lsb >> 3), the handler at 0x15B80 (DAT_010293F4)
    int bendLsb = 0;         // ours: the whole LSB, for a 14-bit bend (bend_word)
    int expr = 254;          // DAT_010297fa
    // FUN_0000edf0, the per-part controller reset: expression back to 0xFE, bend and every controller
    // source to zero. Reset All Controllers runs it, and so does loading a performance, which walks the
    // four parts calling it (FUN_0000f2f0). MEASURED on the unit the same way: 08_panlevel_2 sweeps
    // expression down to 112 and then sends a performance bulk, and every balance segment after it reads
    // full level on the hardware where the engine stayed 3.01 dB down for the rest of the file.
    void reset_ctl();
    // Controller sources in the firmware's own bit order (FUN_000191C0): KN1-4, MC1, MC2, PB, CAT,
    // PAT, FC, BC, MC3, MW, MC4. The knobs and MIDI controls are stored bipolar as (v - 64) * 2, the
    // physical controllers as the raw value, so the range is -128..127 either way.
    int src[14] = {};
    int held[32]; int nheld = 0; int lastPitch = -1;   // mono handling and portamento start
    int rpnM = 127, rpnL = 127; bool nrpnSel = false;   // RPN / NRPN selection state
    bool sustain = false;
    int rcv() const { return p[1] == 0 ? 0x7F : p[4]; }   // Voice Bank "off" (byte 1) receives nothing, whatever the channel (owner's manual p.63)
};
struct Perf {
    uint8_t c[80]; uint8_t fx[112]; Part part[4];
    char name[13];
};
static_assert(offsetof(Perf, fx) == 80, "the 192 common bytes, as the effect host reads them in place");

int bank_perf_index(int lsb, int prog);
void init_perf(Perf& P);

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The register map the firmware's filter and effect hosts write (the YSS236 DSP's), and the LFO waves.
#pragma once
#include <cstdint>

namespace vop3 {

// The chip's registers, by the number the firmware writes to. 0x01, 0x05, 0x1C and 0x28..0x2A
// are written at boot or after a phase reset and are not named anywhere.
enum Reg : uint32_t {
    SELECT          = 0x00,     // the step, slot, LFO or EG the next writes go to
    COEF            = 0x0B,     // a step's constant
    SLOT_HIGH       = 0x0D,     // a delay slot's offset, bits 17-9
    SLOT_LOW        = 0x0E,     //                        bits 8-0
    CONFIG          = 0x16,     // a fifteen-entry table: entry i is written after SELECT i + 1
    UPLOADING       = 0x17,     // 0x23 while a window is rewritten, 0 after
    LFO_RESET       = 0x20,     // the LFOs of a mask back to their start phase
    EG_STATUS       = 0x21,     // written, the EGs that may report; read, the lowest that did
    LFO_SPEED       = 0x24,
    LFO_WAVE        = 0x25,     // and the start phase in the low five bits
    LFO_DEPTH       = 0x26,
    LFO_DIRECTION   = 0x27,
    EG_RATE         = 0x2B,
    EG_TARGET       = 0x2C,     // the level at which a segment ends
    EG_AIM          = 0x2D,     // the level it heads for
};
constexpr uint32_t AUTO_INCREMENT = 0x8000;    // on a SELECT: each write moves on to the next step

// An LFO's wave, in bits 5-7 of LFO_WAVE
enum Wave { TRIANGLE, SAW_DOWN, SAW_UP, SQUARE, SINE, SAMPLE_HOLD };

}  // namespace vop3

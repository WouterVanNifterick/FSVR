// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The parts of the FS1R v1.20 EPROM the effect host reads, by CPU address. The bytes are in
// rom_tables.cpp, which tools/extract_vop3_rom.py writes.
#pragma once
#include <cstdint>
#include <cstring>

namespace fs1r::vop3host::rom {

struct Range { uint32_t start, end; const uint8_t* data; };
constexpr int RANGE_COUNT = 2;
extern const Range RANGES[RANGE_COUNT];

// Big-endian reads at a CPU address. Outside the ranges they give 0.
inline uint32_t u8(uint32_t cpu) {
    for (const Range& r : RANGES)
        if (cpu - r.start < r.end - r.start) return r.data[cpu - r.start];
    return 0;
}
inline int32_t s8(uint32_t cpu) { return int8_t(u8(cpu)); }
inline uint32_t u16(uint32_t cpu) { return u8(cpu) << 8 | u8(cpu + 1); }
inline int32_t s16(uint32_t cpu) { return int16_t(u16(cpu)); }
inline uint32_t u32(uint32_t cpu) { return u16(cpu) << 16 | u16(cpu + 2); }
inline float f32(uint32_t cpu) { uint32_t b = u32(cpu); float f; std::memcpy(&f, &b, 4); return f; }

// Tables, with their element type.
constexpr uint32_t FX_COMB_STEPS = 0x35DB6C;  // u16: the six combs' step list (g, x, x, a) x 6 then the damping LPF's 3 steps
constexpr uint32_t FX_STEPS_MIXER_B = 0x35DFA0;  // u16: mixer step list B (fxSystemParam); entries 8, 9 are the insertion level
constexpr uint32_t FX_STEPS_MIXER_C = 0x35DFC0;  // u16: mixer step list C; entries 17, 23 the insertion to reverb send, 28, 33 the insertion to variation send
constexpr uint32_t FX_STEPS_REVERB = 0x35E02C;  // u16: kind 0 step list, 0x5E entries
constexpr uint32_t FX_SLOTS_REVERB = 0x35E0E8;  // u8: kind 0 slot list, 0x36 entries
constexpr uint32_t FX_STEPS_AUX = 0x35E11E;  // u16: kind 1 step list, 0x28 entries
constexpr uint32_t FX_SLOTS_AUX = 0x35E16E;  // u8: kind 1 slot list, 10 entries
constexpr uint32_t FX_STEPS_INSERTION = 0x35E17C;  // u16: kind 2 step list, 0x60 entries
constexpr uint32_t FX_SLOTS_INSERTION = 0x35E23C;  // u8: kind 2 slot list, 0x28 entries
constexpr uint32_t FX_STEPS_REVERB_DELAY = 0x35E268;  // u16: kind 3 step list, 0x48 entries
constexpr uint32_t FX_SLOTS_REVERB_DELAY = 0x35E2F8;  // u8: kind 3 slot list, 0xC entries
constexpr uint32_t FX_STEPS_VARIATION = 0x35E308;  // u16: kind 4 step list, 0x48 entries
constexpr uint32_t FX_SLOTS_VARIATION = 0x35E398;  // u8: kind 4 slot list, 0xC entries
constexpr uint32_t FX_STEPS_SYSTEM = 0x35E40C;  // u16: system step list, 0x58 entries
constexpr uint32_t FX_PRESET_REVERB = 0x363600;  // u8: reverb 1-12 preset blocks, 0x128 bytes each, indexed through FX_PRESET_INDEX_REVERB
constexpr uint32_t FX_SYSTEM_DEFAULTS = 0x364E60;  // u16: defaults for the system step list
constexpr uint32_t FX_PRESET_INDEX_REVERB = 0x364F10;  // u8: reverb type -> preset block index
constexpr uint32_t FX_DEFAULTS_REVERB = 0x364F4A;  // u16: default parameters of the reverb window, 16 words a type
constexpr uint32_t FX_PROG_BASE = 0x3658D4;  // u16: base program, 232 steps x 5 words
constexpr uint32_t FX_TAGS_BASE = 0x3661E4;  // u8: base program tags
constexpr uint32_t FX_LFO_SPEED = 0x36EF54;  // u16: LFO frequency byte -> register 0x24 word
constexpr uint32_t FX_LFO_PHASE = 0x36F054;  // u16: LFO phase byte -> register 0x25 low bits
constexpr uint32_t FX_COMB_SELECT = 0x36F824;  // s16: per reverb type: comb class selector, 3 words a type
constexpr uint32_t FX_RT60_SECONDS = 0x36F8B0;  // f32: Reverb Time index 0..69 in seconds
constexpr uint32_t FX_TIME_ROW = 0x36F9CC;  // u8: Reverb Time index -> decay row of FX_COMB_GAIN
constexpr uint32_t FX_COMB_GAIN = 0x36FA12;  // u16: 256 rows of (loss gain, feedback gain) for the reverb combs (RT60 law)
constexpr uint32_t FX_DAMP_FREQ = 0x36FE12;  // s8: High Damp byte -> damping LPF frequency index
constexpr uint32_t FX_COMB_CLASS = 0x36FE1D;  // u8: comb length classes, 6 a selector
constexpr uint32_t FX_DIFFUSION = 0x37006A;  // u8: Diffusion byte -> delay trim
constexpr uint32_t FX_ROOM_C = 0x3703BE;  // u16: room design table C (fxRoomPairs)
constexpr uint32_t FX_ROOM_A = 0x37054E;  // u16: room design table A (fxDesignRooms)
constexpr uint32_t FX_ROOM_B = 0x37061C;  // u16: room design table B (White Room .. Canyon)
constexpr uint32_t FX_CFG16 = 0x37101A;  // u8: effect chip register 0x16 configuration, 15 bytes
constexpr uint32_t FX_TAN = 0x371614;  // f32: tan(pi f / 44100) for the 1/6-octave frequency series, index 0..60 (20 Hz .. 20 kHz)
constexpr uint32_t FX_DB = 0x371708;  // f32: 10^((g - 64) / 20) for gain bytes 52..76 (-12 .. +12 dB)
constexpr uint32_t FX_PAN_LAW = 0x3719D0;  // u16: quarter-sine pan law, index 0..128
constexpr uint32_t FX_ER_REV = 0x371AD2;  // u16: ER/Rev balance byte -> gain word
constexpr uint32_t FX_DELAY_WORD = 0x371CD4;  // u16: delay time byte -> 0.1 ms word
constexpr uint32_t FX_SLOT_AREA = 0x3720A4;  // u8: delay slot -> area base selector (0..4), 128 slots
constexpr uint32_t FX_TYPE_MAP_REVERB = 0x372124;  // u8: Data List reverb type -> (window: 0 reverb, 8 delay; type byte), 17 pairs
constexpr uint32_t FX_TYPE_MAP_VARIATION = 0x372146;  // u8: Data List variation type -> (msb, lsb), 29 pairs
constexpr uint32_t FX_TYPE_MAP_INSERTION = 0x372180;  // u8: Data List insertion type -> (msb, lsb), 41 pairs
constexpr uint32_t FX_STEPS_INDIVIDUAL_OUT = 0x3721D4;  // u16: the four individual-out steps (setSubIndividualOut)
constexpr uint32_t PARAM_DESC_COMMON = 0x37E654;  // u8: parameter descriptors of the performance common block, 16 bytes a byte index (+6 min, +8 max, +12 type)
constexpr uint32_t PARAM_DESC_REVERB = 0x388DBC;  // u32: per reverb type (0..16): descriptors of performance bytes 0x50..0x67
constexpr uint32_t PARAM_DESC_VARIATION = 0x388E00;  // u32: per variation type (0..28): descriptors of performance bytes 0x68..0x86
constexpr uint32_t PARAM_DESC_INSERTION = 0x388E74;  // u32: per insertion type (0..40): descriptors of performance bytes 136..166

}  // namespace fs1r::vop3host::rom

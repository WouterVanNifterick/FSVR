// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The CPU's work RAM as the effect host uses it, at the firmware's addresses.
#pragma once
#include <array>
#include <cstdint>

namespace fs1r::vop3host {

class Ram {
public:
    static constexpr uint32_t BASE = 0x01000000;
    static constexpr uint32_t SIZE = 0x80000;      // 512 KB, as on the board

    Ram() { d_.fill(0); }

    // big-endian, by CPU address; out-of-range reads give 0, writes are dropped
    uint32_t u8(uint32_t a) const { return in(a, 1) ? d_[a - BASE] : 0; }
    int32_t s8(uint32_t a) const { return int8_t(u8(a)); }
    uint32_t u16(uint32_t a) const { return in(a, 2) ? (uint32_t(d_[a - BASE]) << 8 | d_[a - BASE + 1]) : 0; }
    int32_t s16(uint32_t a) const { return int16_t(u16(a)); }
    uint32_t u32(uint32_t a) const { return in(a, 4) ? (u16(a) << 16 | u16(a + 2)) : 0; }
    int32_t s32(uint32_t a) const { return int32_t(u32(a)); }

    void w8(uint32_t a, uint32_t v) { if (in(a, 1)) d_[a - BASE] = uint8_t(v); }
    void w16(uint32_t a, uint32_t v) { if (in(a, 2)) { d_[a - BASE] = uint8_t(v >> 8); d_[a - BASE + 1] = uint8_t(v); } }
    void w32(uint32_t a, uint32_t v) { if (in(a, 4)) { w16(a, v >> 16); w16(a + 2, v & 0xFFFF); } }

    const uint8_t* bytes() const { return d_.data(); }

private:
    static bool in(uint32_t a, uint32_t n) { return a >= BASE && a - BASE + n <= SIZE; }
    std::array<uint8_t, SIZE> d_;
};

namespace ram {
constexpr uint32_t SWE_SUB_HOLD   = 0x010683E0;   // nonzero while the effect chip is being rewritten
constexpr uint32_t SWE_SHADOW     = 0x010683EC;   // the effect host's block
constexpr uint32_t FX_BASES       = SWE_SHADOW + 0x0C;    // 5 x s32 delay area bases
constexpr uint32_t FX_TYPE0       = 0x0106840D;   // kind 0 (reverb 1-12) type byte
constexpr uint32_t FX_TYPE1       = 0x0106840E;   // kind 1 msb, +1 lsb; kind 2 at +2/+3, kind 3 at +4/+5, kind 4 at +6/+7
constexpr uint32_t FX_COEF        = 0x0106842C;   // 512 x u16 step constants (register 0xB)
constexpr uint32_t FX_OFFS        = 0x0106882C;   // 128 x s32 delay offsets before the area base
constexpr uint32_t FX_LFO24       = 0x01068A2C;   // 16 x u16 LFO register caches, 0x24..0x27
constexpr uint32_t FX_LFO25       = 0x01068A4C;
constexpr uint32_t FX_LFO26       = 0x01068A6C;
constexpr uint32_t FX_LFO27       = 0x01068A8C;
constexpr uint32_t FX_CFG28       = SWE_SHADOW + 1728;   // 16 bytes each
constexpr uint32_t FX_CFG29       = SWE_SHADOW + 1744;
constexpr uint32_t FX_CFG2A       = SWE_SHADOW + 1760;
constexpr uint32_t FX_TAGS        = SWE_SHADOW + 1776;   // 512 tag bytes (register 0xC)
constexpr uint32_t FX_CFG16       = SWE_SHADOW + 2288;   // 16 bytes (register 0x16)
constexpr uint32_t FX_PARAMS      = 0x01068CEC;   // parameter blocks: kind 0 here, kind k at +0x20*k (16 x u16); master EQ 20, system 15
constexpr uint32_t FX_PARAMS_EQ   = 0x01068D8C;
constexpr uint32_t FX_PARAMS_SYS  = 0x01068DBC;
constexpr uint32_t FX_SUB_LEVEL   = 0x010683DC;   // master level word, 8192 at init
constexpr uint32_t FX_ENABLE0     = 0x010683E1;   // one byte per effect block (0 reverb, 1 aux, 2 insertion, 3 reverb delay, 4 variation, 5 system)
constexpr uint32_t FX_INS_THRU    = 0x010683E9;   // cleared when the insertion type is Thru
constexpr uint32_t FX_MEM_FLAG    = SWE_SHADOW + 0x09;   // 0: the variation area spans to 0x3FFFF, else to 0xFFFF
constexpr uint32_t FX_EQ_TYPE     = SWE_SHADOW + 0x2B;   // master EQ preset selector (0x01068417)
constexpr uint32_t FX_PARAM_CACHE = 0x01068E2C;   // 16 x u16 reverb, +0x20 insertion, +0x40 variation, +0x60 master EQ (11)
constexpr uint32_t FX_DIRTY_REVERB    = 0x01068EA0;   // 16 x u8, one per reverb parameter
constexpr uint32_t FX_DIRTY_VARIATION = 0x01068EB0;   // 16 x u8
constexpr uint32_t FX_DIRTY_INSERTION = 0x01068EC0;   // 16 x u8
constexpr uint32_t FX_DIRTY_EQ        = 0x01068ED0;   // 11 x u8, master EQ bytes 180..190
constexpr uint32_t FX_DIRTY_MIXER     = 0x01068EDB;   // 21 x u8 (system parameters 0..14, 16 send A, 17 send B, 18 insertion level, 19 individual out, 20 master shift)
constexpr uint32_t FX_VAR_WORD    = 0x01068EF0;   // 16 x s16 controller offsets of the insertion parameters
constexpr uint32_t FX_SENDB_OFFSET = 0x01068F10;  // s16 controller offset of the insertion to variation send
constexpr uint32_t FX_SENDA_OFFSET = 0x01068F12;  // s16, insertion to reverb send
constexpr uint32_t PERFORMANCE    = 0x01000000;   // the current performance
}  // namespace ram

}  // namespace fs1r::vop3host

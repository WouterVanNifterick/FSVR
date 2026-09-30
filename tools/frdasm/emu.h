// Minimal shim so MAME's frdasm.cpp builds standalone (no emu.h).
// Build: g++ -O2 -std=c++17 -I. -o frdasm frdasm_main.cpp frdasm.cpp
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef u32 offs_t;
template<typename T> constexpr T BIT(T x, int n) { return (x >> n) & T(1); }
template<typename T> constexpr T BIT(T x, int n, int w) { return (x >> n) & ((T(1) << w) - 1); }

namespace util {
// good-enough printf-style formatter: only %d %X %x %s %c %u with widths/flags used by frdasm
template<typename... Args> void stream_format(std::ostream &s, const char *fmt, Args... args);
inline std::string vfmt(const char *fmt, ...) { char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); return b; }
template<typename T> auto cvt(T v) { return v; }
inline const char *cvt(const std::string &v) { return v.c_str(); }
template<typename... Args> void stream_format(std::ostream &s, const char *fmt, Args... args) { s << vfmt(fmt, cvt(args)...); }

class disasm_interface {
public:
	enum { SUPPORTED = 0x80000000, STEP_OVER = 0x20000000, STEP_OUT = 0x40000000, STEP_COND = 0x10000000, LENGTHMASK = 0xffff };
	static constexpr u32 step_over_extra(u32) { return 0; }
	struct data_buffer {
		const std::vector<u8> *rom; u32 base;
		u8 r8(offs_t a) const { return (*rom)[a - base]; }
		u16 r16(offs_t a) const { return (r8(a) << 8) | r8(a + 1); }           // FR is big-endian
		u32 r32(offs_t a) const { return (u32(r16(a)) << 16) | r16(a + 2); }
	};
	virtual ~disasm_interface() {}
	virtual u32 opcode_alignment() const = 0;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) = 0;
};
}

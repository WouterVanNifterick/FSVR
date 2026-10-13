// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// SH-2 integer semantics: 16-bit loads extend, products wrap at 32 bits, divides truncate
// toward zero, and a variable shift of 32 or more gives 0 (or the sign).
#pragma once
#include <cstdint>

namespace fs1r::vop3host::sh {

inline int32_t s16(uint32_t v) { return int16_t(v); }          // mov.w @.., exts.w
inline uint32_t u16(uint32_t v) { return v & 0xFFFF; }          // extu.w
inline int32_t s8(uint32_t v) { return int8_t(v); }             // mov.b @.., exts.b
inline uint32_t u8(uint32_t v) { return v & 0xFF; }             // extu.b

inline uint32_t mulsi3(uint32_t a, uint32_t b) { return a * b; }               // mul.l, 32-bit wrap
// 0x3000: signed divide, r1 / r0, truncating toward zero
inline int32_t sdiv3000(int32_t r1_dividend, int32_t r0_divisor) {
    if (r0_divisor == 0) return 0;
    if (r1_dividend == INT32_MIN && r0_divisor == -1) return INT32_MIN;
    return r1_dividend / r0_divisor;
}
inline int32_t muls_w(int32_t a, int32_t b) { return int16_t(a) * int16_t(b); } // muls.w (16x16 -> 32)
inline uint32_t mulu_w(uint32_t a, uint32_t b) { return (a & 0xFFFF) * (b & 0xFFFF); }

inline uint32_t udivsi3(uint32_t r1_dividend, uint32_t r0_divisor) {           // 0x30B4
    return r0_divisor ? r1_dividend / r0_divisor : 0xFFFFFFFFu;
}
// 0x315C: signed remainder, r1 % r0, with the dividend's sign
inline int32_t smod315C(int32_t r1_dividend, int32_t r0_divisor) {
    if (r0_divisor == 0) return r1_dividend;
    if (r1_dividend == INT32_MIN && r0_divisor == -1) return 0;
    return r1_dividend % r0_divisor;
}

inline uint32_t lshrsi3(uint32_t v, int32_t n) { return n < 0 ? 0 : n >= 32 ? 0 : v >> n; }
inline int32_t ashrsi3(int32_t v, int32_t n) { return n < 0 ? v : n >= 32 ? (v < 0 ? -1 : 0) : v >> n; }

}  // namespace fs1r::vop3host::sh

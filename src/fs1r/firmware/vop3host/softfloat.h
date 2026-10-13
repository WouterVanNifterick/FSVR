// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The firmware's soft-float routines as float and double operations. All are IEEE
// round-to-nearest-even, so the results match bit for bit as long as the compiler neither
// contracts a*b+c into an FMA nor keeps excess precision.
//
// Operand order is the firmware's. Doubles: (first pushed, last pushed), so
// dsub(first, last) = first - last. Floats: (r0, r1), so fsub(r0, r1) = r1 - r0 and
// fdiv(r0, r1) = r1 / r0.
#pragma once
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

static_assert(FLT_EVAL_METHOD == 0, "vop3host needs float/double arithmetic without excess precision");

namespace fs1r::vop3host::sh {

// ---- doubles (frame [ptr][last.hi][last.lo][first.hi][first.lo]) ----
inline double dadd(double first, double last) { return first + last; }      // 0x38D8 addDouble
inline double dsub(double first, double last) { return first - last; }      // 0x3820 subDouble
inline double dmul(double first, double last) { return first * last; }      // 0x4778 mulDouble
inline double ddiv(double first, double last) { return first / last; }      // 0x3DB0 divDouble
inline double dneg(double x) { return -x; }                                 // 0x4B60 negDouble
// compares take [a][b] with a at the lower address (pushed last); no result pointer
inline int dgt(double a, double b) { return a > b ? 1 : 0; }                // 0x45B4 gtDouble
inline int dlt(double a, double b) { return a < b ? 1 : 0; }                // 0x4370 ltDouble
inline int dne(double a, double b) { return a != b ? 1 : 0; }               // 0x4ACC neDouble
inline int deq(double a, double b) { return a == b ? 1 : 0; }               // 0x4234 eqDouble

// ---- floats (r0, r1 -> r0) ----
inline float fadd(float r0, float r1) { return r0 + r1; }                   // 0x3ABE addFloat
inline float fsub(float r0, float r1) { return r1 - r0; }                   // 0x3AB8 subFloat
inline float fmul(float r0, float r1) { return r0 * r1; }                   // 0x496C mulFloat
inline float fdiv(float r0, float r1) { return r1 / r0; }                   // 0x3F84 divFloat
inline float fneg(float x) { return -x; }                                   // 0x4B80 negFloat
inline int fgt(float r0, float r1) { return r0 > r1 ? 1 : 0; }              // 0x465C gtFloat
inline int flt(float r0, float r1) { return r0 < r1 ? 1 : 0; }              // 0x4418 ltFloat
inline int fge(float r0, float r1) { return r0 >= r1 ? 1 : 0; }             // 0x453C geFloat

// a float from its IEEE bit pattern
inline float from_bits(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
inline double from_bits64(uint64_t bits) { double d; std::memcpy(&d, &bits, 8); return d; }

// ---- conversions ----
inline float itof(int32_t i) { return float(i); }                           // 0x44E4 intToFloat
inline float utof(uint32_t u) { return float(u); }                          // 0x4CA4 uintToFloat
inline double utod(uint32_t u) { return double(u); }                        // 0x4C64 uintToDouble
inline double ftod(float f) { return double(f); }                           // 0x4B88 floatToDouble
inline float dtof(double d) { return float(d); }                            // 0x4150 doubleToFloat (RNE)

// Truncation toward zero. Out of range, the integer part modulo 2^32; NaN and infinities give 0.
template <int MANT>
inline uint32_t trunc_mod32(double x) {
    if (!std::isfinite(x)) return 0;
    int e = 0;
    const double m = std::frexp(std::fabs(x), &e);        // |x| = m * 2^e, m in [0.5, 1)
    const uint64_t M = uint64_t(std::ldexp(m, MANT));     // the integer mantissa (exact)
    const int k = e - MANT;                                // |x| = M * 2^k
    uint64_t v;
    if (k >= 32) v = 0;
    else if (k >= 0) v = (M << k) & 0xFFFFFFFFu;
    else if (-k >= 64) v = 0;
    else v = M >> (-k);
    const uint32_t r = uint32_t(v);
    return x < 0 ? uint32_t(0u - r) : r;
}
inline int32_t ftoi(float f) { return int32_t(trunc_mod32<24>(double(f))); }   // 0x4C10 floatToInt
inline int32_t dtoi(double d) { return int32_t(trunc_mod32<53>(d)); }          // 0x40C8 doubleToInt

}  // namespace fs1r::vop3host::sh

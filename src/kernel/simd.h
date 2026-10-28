// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// kernel/simd.h - a few lanes of doubles, written as the scalar arithmetic they stand in for. Ours.
//
// A kernel file is compiled once per instruction set, FSVR_ISA picking the lane type and the namespace it lands
// in (0 scalar, 1 sse2, 2 avx2, 3 neon; kernel/README.md). Every operation here does per lane exactly what the
// scalar code does, in the same order, so a kernel over W slots or channels renders their samples to the bit;
// the hashes (dsp_check, tools/abcheck.py) hold it to that. The rules the exactness rests on:
//   - min(a, b) is a < b ? a : b and max(a, b) is a > b ? a : b, the second operand on a tie or a NaN, as the
//     x86 instructions are; the scalar code's clamps are written that way round (dsp/word.h).
//   - select(m, a, b) is a where the mask is set, b elsewhere: a masked path is never arithmetic with a 0 or 1.
//   - a + 0.0 stays an add (it turns a -0.0 into +0.0, as the chip does).
//   - nothing is fused: the build keeps a multiply and an add apart (CMakeLists.txt, build.bat).
#pragma once
#include <cstdint>
#include <cstring>

#if FSVR_ISA == 0
#define FSVR_ISA_NS scalar
#elif FSVR_ISA == 1
#define FSVR_ISA_NS sse2
#elif FSVR_ISA == 2
#define FSVR_ISA_NS avx2
#elif FSVR_ISA == 3
#define FSVR_ISA_NS neon
#else
#error "FSVR_ISA is 0 scalar, 1 sse2, 2 avx2 or 3 neon"
#endif

// The lane type: the instruction set asked for where the target has it, the target's own otherwise (a universal
// macOS build compiles every kernel for both slices; the dispatch never picks a namespace the slice cannot run).
#if FSVR_ISA != 0 && (defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__))
#include <immintrin.h>
#if FSVR_ISA == 2
#ifndef __AVX2__
#error "the avx2 kernel is compiled with /arch:AVX2 or -mavx2"
#endif
#define FSVR_LANES_AVX2 1
#else
#define FSVR_LANES_SSE2 1
#endif
#elif FSVR_ISA != 0 && (defined(__aarch64__) || defined(_M_ARM64))
#include <arm_neon.h>
#define FSVR_LANES_NEON 1
#else
#define FSVR_LANES_SCALAR 1
#endif

namespace fsvr::simd::FSVR_ISA_NS {

#if FSVR_LANES_AVX2
constexpr int W = 4;
struct vd { __m256d v; };
struct vm { __m256d v; };                      // a mask: every bit of a lane set, or none
inline vd load(const double* p) { return {_mm256_loadu_pd(p)}; }
inline void store(double* p, vd a) { _mm256_storeu_pd(p, a.v); }
inline vd set1(double x) { return {_mm256_set1_pd(x)}; }
inline vd add(vd a, vd b) { return {_mm256_add_pd(a.v, b.v)}; }
inline vd sub(vd a, vd b) { return {_mm256_sub_pd(a.v, b.v)}; }
inline vd mul(vd a, vd b) { return {_mm256_mul_pd(a.v, b.v)}; }
inline vd div(vd a, vd b) { return {_mm256_div_pd(a.v, b.v)}; }
inline vd min(vd a, vd b) { return {_mm256_min_pd(a.v, b.v)}; }
inline vd max(vd a, vd b) { return {_mm256_max_pd(a.v, b.v)}; }
inline vd neg(vd a) { return {_mm256_xor_pd(a.v, _mm256_set1_pd(-0.0))}; }
inline vd abs(vd a) { return {_mm256_andnot_pd(_mm256_set1_pd(-0.0), a.v)}; }
inline vm lt(vd a, vd b) { return {_mm256_cmp_pd(a.v, b.v, _CMP_LT_OQ)}; }
inline vm ge(vd a, vd b) { return {_mm256_cmp_pd(a.v, b.v, _CMP_GE_OQ)}; }
inline vm gt(vd a, vd b) { return {_mm256_cmp_pd(a.v, b.v, _CMP_GT_OQ)}; }
inline vm ne(vd a, vd b) { return {_mm256_cmp_pd(a.v, b.v, _CMP_NEQ_UQ)}; }   // true when either is a NaN, as != is
inline vm isnan(vd a) { return {_mm256_cmp_pd(a.v, a.v, _CMP_UNORD_Q)}; }
inline vm mand(vm a, vm b) { return {_mm256_and_pd(a.v, b.v)}; }
inline vm mor(vm a, vm b) { return {_mm256_or_pd(a.v, b.v)}; }
inline vd select(vm m, vd a, vd b) { return {_mm256_blendv_pd(b.v, a.v, m.v)}; }
inline vm loadmask(const uint64_t* p) { return {_mm256_castsi256_pd(_mm256_loadu_si256((const __m256i*)p))}; }
inline int movemask(vm m) { return _mm256_movemask_pd(m.v); }
inline bool any(vm m) { return movemask(m) != 0; }
inline bool all(vm m) { return movemask(m) == 15; }

#elif FSVR_LANES_SSE2
constexpr int W = 2;
struct vd { __m128d v; };
struct vm { __m128d v; };
inline vd load(const double* p) { return {_mm_loadu_pd(p)}; }
inline void store(double* p, vd a) { _mm_storeu_pd(p, a.v); }
inline vd set1(double x) { return {_mm_set1_pd(x)}; }
inline vd add(vd a, vd b) { return {_mm_add_pd(a.v, b.v)}; }
inline vd sub(vd a, vd b) { return {_mm_sub_pd(a.v, b.v)}; }
inline vd mul(vd a, vd b) { return {_mm_mul_pd(a.v, b.v)}; }
inline vd div(vd a, vd b) { return {_mm_div_pd(a.v, b.v)}; }
inline vd min(vd a, vd b) { return {_mm_min_pd(a.v, b.v)}; }
inline vd max(vd a, vd b) { return {_mm_max_pd(a.v, b.v)}; }
inline vd neg(vd a) { return {_mm_xor_pd(a.v, _mm_set1_pd(-0.0))}; }
inline vd abs(vd a) { return {_mm_andnot_pd(_mm_set1_pd(-0.0), a.v)}; }
inline vm lt(vd a, vd b) { return {_mm_cmplt_pd(a.v, b.v)}; }
inline vm ge(vd a, vd b) { return {_mm_cmpge_pd(a.v, b.v)}; }
inline vm gt(vd a, vd b) { return {_mm_cmpgt_pd(a.v, b.v)}; }
inline vm ne(vd a, vd b) { return {_mm_cmpneq_pd(a.v, b.v)}; }
inline vm isnan(vd a) { return {_mm_cmpunord_pd(a.v, a.v)}; }
inline vm mand(vm a, vm b) { return {_mm_and_pd(a.v, b.v)}; }
inline vm mor(vm a, vm b) { return {_mm_or_pd(a.v, b.v)}; }
inline vd select(vm m, vd a, vd b) { return {_mm_or_pd(_mm_and_pd(m.v, a.v), _mm_andnot_pd(m.v, b.v))}; }
inline vm loadmask(const uint64_t* p) { return {_mm_castsi128_pd(_mm_loadu_si128((const __m128i*)p))}; }
inline int movemask(vm m) { return _mm_movemask_pd(m.v); }
inline bool any(vm m) { return movemask(m) != 0; }
inline bool all(vm m) { return movemask(m) == 3; }

#elif FSVR_LANES_NEON
constexpr int W = 2;
struct vd { float64x2_t v; };
struct vm { uint64x2_t v; };
inline vd load(const double* p) { return {vld1q_f64(p)}; }
inline void store(double* p, vd a) { vst1q_f64(p, a.v); }
inline vd set1(double x) { return {vdupq_n_f64(x)}; }
inline vd add(vd a, vd b) { return {vaddq_f64(a.v, b.v)}; }
inline vd sub(vd a, vd b) { return {vsubq_f64(a.v, b.v)}; }
inline vd mul(vd a, vd b) { return {vmulq_f64(a.v, b.v)}; }
inline vd div(vd a, vd b) { return {vdivq_f64(a.v, b.v)}; }
// Written as the compares they stand for, so a tie goes the x86 way (the second operand) on this side too.
inline vd min(vd a, vd b) { return {vbslq_f64(vcltq_f64(a.v, b.v), a.v, b.v)}; }
inline vd max(vd a, vd b) { return {vbslq_f64(vcgtq_f64(a.v, b.v), a.v, b.v)}; }
inline vd neg(vd a) { return {vnegq_f64(a.v)}; }
inline vd abs(vd a) { return {vabsq_f64(a.v)}; }
inline vm lt(vd a, vd b) { return {vcltq_f64(a.v, b.v)}; }
inline vm ge(vd a, vd b) { return {vcgeq_f64(a.v, b.v)}; }
inline vm gt(vd a, vd b) { return {vcgtq_f64(a.v, b.v)}; }
inline vm ne(vd a, vd b) { return {veorq_u64(vceqq_f64(a.v, b.v), vdupq_n_u64(~0ull))}; }
inline vm isnan(vd a) { return {veorq_u64(vceqq_f64(a.v, a.v), vdupq_n_u64(~0ull))}; }
inline vm mand(vm a, vm b) { return {vandq_u64(a.v, b.v)}; }
inline vm mor(vm a, vm b) { return {vorrq_u64(a.v, b.v)}; }
inline vd select(vm m, vd a, vd b) { return {vbslq_f64(m.v, a.v, b.v)}; }
inline vm loadmask(const uint64_t* p) { return {vld1q_u64(p)}; }
inline int movemask(vm m) { return (int)(vgetq_lane_u64(m.v, 0) >> 63) | (int)(vgetq_lane_u64(m.v, 1) >> 63) << 1; }
inline bool any(vm m) { return movemask(m) != 0; }
inline bool all(vm m) { return movemask(m) == 3; }

#else
constexpr int W = 1;
struct vd { double v; };
struct vm { uint64_t v; };
inline vd load(const double* p) { return {*p}; }
inline void store(double* p, vd a) { *p = a.v; }
inline vd set1(double x) { return {x}; }
inline vd add(vd a, vd b) { return {a.v + b.v}; }
inline vd sub(vd a, vd b) { return {a.v - b.v}; }
inline vd mul(vd a, vd b) { return {a.v * b.v}; }
inline vd div(vd a, vd b) { return {a.v / b.v}; }
inline vd min(vd a, vd b) { return {a.v < b.v ? a.v : b.v}; }
inline vd max(vd a, vd b) { return {a.v > b.v ? a.v : b.v}; }
inline vd neg(vd a) { return {-a.v}; }
inline vd abs(vd a) { return {a.v < 0 ? -a.v : a.v == 0 ? 0.0 : a.v}; }
inline vm mk(bool b) { return {b ? ~0ull : 0ull}; }
inline vm lt(vd a, vd b) { return mk(a.v < b.v); }
inline vm ge(vd a, vd b) { return mk(a.v >= b.v); }
inline vm gt(vd a, vd b) { return mk(a.v > b.v); }
inline vm ne(vd a, vd b) { return mk(a.v != b.v); }
inline vm isnan(vd a) { return mk(a.v != a.v); }
inline vm mand(vm a, vm b) { return {a.v & b.v}; }
inline vm mor(vm a, vm b) { return {a.v | b.v}; }
inline vd select(vm m, vd a, vd b) { return m.v ? a : b; }
inline vm loadmask(const uint64_t* p) { return {*p}; }
inline int movemask(vm m) { return m.v ? 1 : 0; }
inline bool any(vm m) { return m.v != 0; }
inline bool all(vm m) { return m.v != 0; }
#endif

// A lane out and in, for the rare scalar fixup.
inline double lane(vd a, int i) { double t[W]; store(t, a); return t[i]; }
inline vd setlane(vd a, int i, double x) { double t[W]; store(t, a); t[i] = x; return load(t); }

}  // namespace fsvr::simd::FSVR_ISA_NS

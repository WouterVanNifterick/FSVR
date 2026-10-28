// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The choice of kernels: what the CPU has, unless FSVR_SIMD asks for less. Compiled once, with no lane code in it.
#define _CRT_SECURE_NO_WARNINGS
#include "kernel/dispatch.h"
#include "math/floor.h"
#include <cstdlib>
#include <cstring>
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

// floor.h's test for the rounding instruction, made once (fm::floor_exact reads it on the x64 baseline)
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
const bool fm::g_sse41 = fm::has_sse41();
#endif

#ifndef FSVR_HAVE_SSE2
#define FSVR_HAVE_SSE2 0
#endif
#ifndef FSVR_HAVE_AVX2
#define FSVR_HAVE_AVX2 0
#endif
#ifndef FSVR_HAVE_NEON
#define FSVR_HAVE_NEON 0
#endif

namespace fsvr::kernel {
namespace scalar { const Kernels& table(); }
#if FSVR_HAVE_SSE2
namespace sse2 { const Kernels& table(); }
#endif
#if FSVR_HAVE_AVX2
namespace avx2 { const Kernels& table(); }
#endif
#if FSVR_HAVE_NEON
namespace neon { const Kernels& table(); }
#endif
}  // namespace fsvr::kernel

namespace {

using fsvr::Kernels;
using fsvr::Isa;
constexpr Isa SCALAR = Isa::scalar, SSE2 = Isa::sse2, AVX2 = Isa::avx2, NEON = Isa::neon;

// What the CPU runs: AVX2 wants the OS to save the wide registers too (OSXSAVE and XGETBV bits 1 and 2).
Isa cpu_isa() {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    int r[4] = {};
#ifdef _MSC_VER
    __cpuid(r, 1);
    const bool osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1;
    unsigned long long xcr0 = osxsave ? _xgetbv(0) : 0;
    __cpuidex(r, 7, 0);
    const bool avx2 = (r[1] >> 5) & 1;
#else
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return SSE2;
    const bool osxsave = (c >> 27) & 1, avx = (c >> 28) & 1;
    unsigned long long xcr0 = 0;
    if (osxsave) { unsigned lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); xcr0 = (unsigned long long)hi << 32 | lo; }
    const bool avx2 = __get_cpuid_count(7, 0, &a, &b, &c, &d) && ((b >> 5) & 1);
#endif
    return avx && avx2 && (xcr0 & 6) == 6 ? AVX2 : SSE2;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return NEON;
#else
    return SCALAR;
#endif
}

const Kernels& table(Isa isa) {
    switch (isa) {
#if FSVR_HAVE_AVX2
        case AVX2: return fsvr::kernel::avx2::table();
#endif
#if FSVR_HAVE_SSE2
        case SSE2: return fsvr::kernel::sse2::table();
#endif
#if FSVR_HAVE_NEON
        case NEON: return fsvr::kernel::neon::table();
#endif
        default:   return fsvr::kernel::scalar::table();
    }
}

Isa choose() {
    Isa have = cpu_isa();
    if (have == AVX2 && !FSVR_HAVE_AVX2) have = SSE2;
    if (have == SSE2 && !FSVR_HAVE_SSE2) have = SCALAR;
    if (have == NEON && !FSVR_HAVE_NEON) have = SCALAR;
    Isa want = have;
    if (const char* s = getenv("FSVR_SIMD")) {
        if (!strcmp(s, "scalar")) want = SCALAR;
        else if (!strcmp(s, "sse2")) want = SSE2;
        else if (!strcmp(s, "avx2")) want = AVX2;
        else if (!strcmp(s, "neon")) want = NEON;
    }
    // Asked for one the CPU lacks: the best it has, except that scalar is always there.
    if (want == NEON && have != NEON) want = have;
    if (want == AVX2 && have != AVX2) want = have == NEON ? SCALAR : have;
    if (want == SSE2 && (have == NEON || have == SCALAR)) want = SCALAR;
    return want;
}

}  // namespace

fsvr::Isa fsvr::isa() {
    static const Isa i = choose();
    return i;
}

const fsvr::Kernels& fsvr::kernels() {
    static const Kernels& k = table(isa());
    return k;
}

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// This instruction set's table of kernels, and the check that its build fused nothing. Compiled once per set.
#include "kernel/dispatch.h"
#include "kernel/simd.h"

namespace fsvr::kernel::FSVR_ISA_NS {

void filter_bank(fsvr::filters::Lanes& L, const fsvr::filters::Shared& g, const double* in, const double* eg, const double* lfo, double* out);

namespace {
// a * a + c with a = 1 + 2^-27 and c = -1 is 2^-26 in two roundings and 2^-26 + 2^-54 in one (math/fastmath.cpp has the scalar twin)
bool fma_contracted() {
    using namespace fsvr::simd::FSVR_ISA_NS;
    volatile double av = 1.0 + 1.0 / (1 << 27), cv = -1.0;
    const vd a = set1(av), c = set1(cv);
    return lane(add(mul(a, a), c), 0) != 1.0 / (1 << 26);
}
#define FSVR_STR_(x) #x
#define FSVR_STR(x) FSVR_STR_(x)
const fsvr::Kernels TABLE = {FSVR_STR(FSVR_ISA_NS), fsvr::simd::FSVR_ISA_NS::W, &filter_bank, &fma_contracted};
}

const fsvr::Kernels& table() { return TABLE; }

}  // namespace fsvr::kernel::FSVR_ISA_NS

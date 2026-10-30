// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The tone generator's table of kernels for the instruction set fsvr::isa() chose. Compiled once, no lane code in it.
#include "kernel/tonegen.h"

namespace fsvr::kernel {
namespace scalar { const TgKernels& tg_table(); }
#if FSVR_HAVE_SSE2
namespace sse2 { const TgKernels& tg_table(); }
#endif
#if FSVR_HAVE_AVX2
namespace avx2 { const TgKernels& tg_table(); }
#endif
#if FSVR_HAVE_NEON
namespace neon { const TgKernels& tg_table(); }
#endif
}  // namespace fsvr::kernel

const fsvr::TgKernels& fsvr::tg_kernels() {
    switch (isa()) {
#if FSVR_HAVE_AVX2
        case Isa::avx2: return kernel::avx2::tg_table();
#endif
#if FSVR_HAVE_SSE2
        case Isa::sse2: return kernel::sse2::tg_table();
#endif
#if FSVR_HAVE_NEON
        case Isa::neon: return kernel::neon::tg_table();
#endif
        default:        return kernel::scalar::tg_table();
    }
}

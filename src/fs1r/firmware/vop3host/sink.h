// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A host's side of its chip: where the writes go, and the one register that is read.
#pragma once
#include <cstdint>
#include "../../chips/vop3_regs.h"

namespace fs1r::vop3host {

using namespace ::vop3;     // the chip's registers and LFO waves, by name

struct WriteSink {
    virtual void write(uint32_t reg, uint32_t value) = 0;
    virtual uint32_t read(uint32_t) { return 0; }       // the filter chip's EG_STATUS
};

}  // namespace fs1r::vop3host

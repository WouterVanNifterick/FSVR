// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// A host's side of its chip: where the register writes go, and the one register that is read.
#pragma once
#include <cstdint>

namespace fs1r::vop3host {

struct WriteSink {
    virtual void write(uint32_t reg, uint32_t value) = 0;
    virtual uint32_t read(uint32_t) { return 0; }       // the filter chip's status, register 0x21
};

}  // namespace fs1r::vop3host

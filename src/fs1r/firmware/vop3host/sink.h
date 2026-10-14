// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Where a host's register writes go.
#pragma once
#include <cstdint>

namespace fs1r::vop3host {

struct WriteSink {
    virtual void write(uint32_t reg, uint32_t value) = 0;
};

}  // namespace fs1r::vop3host

// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// kernel/tonegen.h - the tone generator's lane kernel, chosen by the same instruction-set choice as the DSP's. Ours.
//
// The group kernel reads the tone generator's tables and the channel's control refresh, so it is the engine's
// (fs1rLib), where the filter bank's kernel is the DSP library's (dispatch.h); dsp_check links the DSP alone.
// Both are compiled once per instruction set, and fsvr::isa() picks the same one for both.
#pragma once
#include "kernel/dispatch.h"

namespace fsvr::tonegen { struct Group; }

namespace fsvr {

struct TgKernels {
    const char* name;
    int lanes;
    // A group of channels over a segment of n samples (tonegen/lanes.h, engine/render.cpp).
    void (*render_group)(tonegen::Group& G, int n);
};

const TgKernels& tg_kernels();

}  // namespace fsvr

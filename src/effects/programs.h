// effects/programs.h - the FS1R's effect programs as plain DSP, and the state they share. Ours.
//
// The firmware runs its effects as programs on a DSP: the base program (the mixer, the sends and returns,
// the master EQ and the reverb), and four windows into it that each hold one of the EPROM's programs for
// the effect types selected: aux, the reverb delay, the variation and the insertion. Each program below is
// one of those, as the arithmetic it computes, one function a sample. Effects (effects.h) runs the five
// the host has loaded, in the order the chip runs them, on one State.
//
// The programs talk to each other only through the State: the stereo sends in r[3]..r[8], the returns in
// r[9]..r[18], a few cells and the delay memory. Their arithmetic is the unit's, in its number format
// (dsp/word.h) and its order, so the section is bit for bit the unit's DSP; tools/dsp_check pins it.
//
// Most programs are still straight-line arithmetic over numbered registers. aux_window.cpp and var_m07.cpp show the
// form they are headed for: name the DSP blocks, keep every operation and its order, run dsp_check.
#pragma once

#include <cstdint>
#include <vector>

#include "dsp/word.h"

namespace fsvr::effects {

namespace word = fsvr::word;

// What the programs share, by the names the chip and the host use for it.
struct State {
	static constexpr int RAM_WORDS = 1 << 18;

	double r[128] = {};          // the registers: the buses between programs, and each program's own state
	double m[64] = {};           // the cells: the serial inputs, the LFOs and the delay taps as they arrive
	double t[16] = {};           // the multiplier registers: modulated gains and the delay lines' fractions
	double k[512] = {};          // the parameter words the host writes, as fractions, by step
	double in[4][16] = {};       // the serial inputs (in[0]: the buses) and in[3] the EG levels, by slot
	double lfo[16] = {};         // the sixteen LFOs, this sample
	double out[16] = {};         // the serial outputs: out[0] and out[8] are the DAC's left and right

	uint32_t offset[128] = {};   // each delay slot's offset, as the host sets it
	int32_t ptr = 0;             // the delay memory's ring pointer, one word back a sample
	std::vector<double> ram = std::vector<double>(RAM_WORDS);

	// A delay slot's word, `older` words back: on the ring, or (table) at the slot's own offset.
	uint32_t at(int slot, int older = 0) const { return uint32_t(ptr + int32_t(offset[slot]) + older) & (RAM_WORDS - 1); }
	uint32_t table(int slot, int older = 0) const { return uint32_t(int32_t(offset[slot]) + older) & (RAM_WORDS - 1); }
};

using Program = void (*)(State&);

// The base program, steps 0x000-0x0e7: the mixer, sends, returns, master EQ and the reverb
void base(State&);
// The aux window, 0x0e8-0x10f: eight smoothed send levels
void aux(State&);
// The reverb delay window, 0x110-0x157, by type msb: the reverb's delay half, the delay types, and the clear
void dly_m00(State&); void dly_m08(State&); void dly_m11(State&);
// The variation window, 0x158-0x19f, by type msb (fx_dispatch.cpp's VARIATION_ROUTES); m11 is the clear
void var_m00(State&); void var_m01(State&); void var_m02(State&); void var_m03(State&);
void var_m04(State&); void var_m05(State&); void var_m06(State&); void var_m07(State&);
void var_m08(State&); void var_m09(State&); void var_m10(State&); void var_m11(State&);
// The insertion window, 0x1a0-0x1ff, by type msb (INSERTION_ROUTES); m14 is the clear
void ins_m00(State&); void ins_m01(State&); void ins_m02(State&); void ins_m03(State&);
void ins_m04(State&); void ins_m05(State&); void ins_m07(State&); void ins_m10(State&);
void ins_m12(State&); void ins_m13(State&); void ins_m14(State&);

}  // namespace fsvr::effects

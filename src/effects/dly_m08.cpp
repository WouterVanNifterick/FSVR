// effects/dly_m08.cpp - the reverb delay window's program for the four delay types.
//
// rom::REVERB_DELAY_PROGRAMS[8], the program the window holds for reverb types 13 to 16 (Delay L,C,R,
// Delay L,R, Echo, Cross Delay). Types 0 to 12 run dly-m00's program in this window instead, under the
// same keys' first entry, and fx_delay.cpp is the host that feeds both. Under these four types the
// DELAY_ROUTES entry (fx_dispatch.cpp) matches and one of EffectHost::delayLCR, delayLR, echo or
// crossDelay runs, so every one of them rewrites this window's constants and delay offsets when a
// parameter moves. Which of the four is running is not a code difference here: the program words are
// the same under all of them and only the constants (rom::rev::DELAY_WORDS and the damp curve) and the
// six delay offsets (rom::rev::DELAY_SLOTS) differ. The code reads them where they live all the same,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
//   0x110-0x11d  the two serial inputs and the host's four tone filters, rev::DELAY_LPF, rev::DELAY_HPF
//                and their copies, one three-step one-pole each. r[88] and r[91] are the two first
//                poles, r[89] and r[92] the two second ones, and the second pole of a side is fed from
//                the first. The three reads that ride on the first step of each group (the slots 0x44,
//                0x45 and 0x46, in the reverb's area) are published in m[22], m[23] and m[24].
//   0x11e-0x129  three of the four delay taps, read here and taken up by cells five steps later, and the
//                three captures the windows outside this one read (m[24], m[56] and m[57]). Only one
//                step of the twelve computes anything: 0x128 copies m[56] into r[93].
//   0x12a-0x12f  the fourth tap's read at 0x12b and the two damped feedbacks. rev::DELAY_CURVE and
//                rev::DELAY_CURVE_COPY are a pair of words each on the same operand, r[93] and r[94],
//                into r[119] and r[120]; 0x12c copies m[57] into r[94] between them.
//   0x130-0x140  the two line writes, the two wet sums and the dry/wet mix. 0x133 carries w1[13] and
//                is itself a writer, so it writes its own result into the slot 0x4c (rev::DELAY_SLOTS[0],
//                the left line); 0x139 only latches, and the write at 0x13b into the slot 0x4e
//                (rev::DELAY_SLOTS[3]) takes that latch rather than its own result. r[124] and r[125]
//                are the two wet sums, and r[13] and r[14] the block's outputs after the dry/wet mix.
//   0x141-0x157  six steps of nothing (they only move the z^-1 latch), then three more reads of the
//                block's own area (the slots 0x51, 0x52 and 0x53) published in m[25], m[26] and m[27],
//                the LFO values m[10], and the block's output, the cell m[41] to the serial out at
//                0x156.
//
// The registers: r[3] and r[4] the two inputs of the block, r[87] to r[92] the tone filters' inputs
// and states, r[93] and r[94] the first two taps, r[119] and r[120] those two taps damped, r[124] and
// r[125] the two wet sums, r[13] and r[14] the outputs. The cells: m[1] and m[5] the two serial inputs
// (which no step of this program reads; the filters work on r[3] and r[4] instead), m[8], m[9] and m[10]
// the LFO values it hands on, m[22] to m[27] the six reads it publishes, m[41] the block's output, m[56]
// to m[59] the four taps. There is no t register anywhere in this program: no step has tw or tfrac set,
// and no step takes a multiplier from the t file.
//
// A delay RAM read is made at one step and taken up by a cell 5 to 11 steps later, which is what
// yss236_core::captured() returns and what an msrc of 4 queues. Every read here is five steps ahead of
// its cell, so each capture takes the read its window names. The coefficients are spread one to a step
// rather than one to a group, which is how the host gets a word of its own for each of them: the two
// halves of a damp pair multiply the same operand, the pole of a tone filter is one step and its gain
// the next, and the dry/wet mix is the same three-step shape with a multiply by four instead of two.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void dly_m08(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& out = s.out;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x44)];
	double const d1 = s.ram[s.at(0x45)];
	double const d2 = s.ram[s.at(0x46)];
	double const d3 = s.ram[s.at(0x47)];
	double const d4 = s.ram[s.at(0x48)];
	double const d5 = s.ram[s.at(0x49)];
	double const d6 = s.ram[s.at(0x4a)];
	double const v0 = sat(((k[0x110] * r[3]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x113] * v0) + sum(k[0x111] * r[88], k[0x112] * r[87])) * 2.0);
	double const v2 = sat(((k[0x116] * v1) + sum(k[0x114] * r[89], k[0x115] * r[88])) * 4.0);
	double const v3 = sat(((k[0x117] * r[4]) + 0.0) * 2.0);
	double const v4 = sat(((k[0x11a] * v3) + sum(k[0x118] * r[91], k[0x119] * r[90])) * 2.0);
	double const v5 = sat(((k[0x11d] * v4) + sum(k[0x11b] * r[92], k[0x11c] * r[91])) * 4.0);
	double const v6 = wrap(d3 + 0.0);
	double const v7 = sat((k[0x12b] * v6) + sum(k[0x12a] * r[93]));
	double const v8 = wrap(d4 + 0.0);
	double const v9 = sat((k[0x12f] * v8) + sum(k[0x12e] * r[94]));
	s.ram[s.at(0x4c)] = clip32(sat((k[0x133] * v9) + sum(k[0x130] * v2, k[0x131] * v5, k[0x132] * v7)));
	s.ram[s.at(0x4e)] = clip32(sat((k[0x139] * v7) + sum(k[0x137] * v5, k[0x138] * v9)));
	double const d7 = s.ram[s.at(0x51)];
	double const d8 = s.ram[s.at(0x52)];
	double const d9 = s.ram[s.at(0x53)];
	double const v10 = sat((k[0x136] * d6) + sum(k[0x134] * d3, k[0x135] * d5));
	double const v11 = sat((k[0x13c] * d5) + sum(k[0x13a] * d4, k[0x13b] * d6));
	r[13] = wrap(((k[0x13e] * v10) + sum(k[0x13d] * v2)) * 4.0);
	r[14] = wrap(((k[0x140] * v11) + sum(k[0x13f] * v5)) * 4.0);
	r[87] = v0;
	r[88] = v1;
	r[89] = v2;
	r[90] = v3;
	r[91] = v4;
	r[92] = v5;
	r[93] = v6;
	r[94] = v8;
	r[119] = v7;
	r[120] = v9;
	r[124] = v10;
	r[125] = v11;
	m[1] = in[0][8];
	m[5] = in[1][8];
	m[8] = lfo[8];
	m[9] = lfo[9];
	m[10] = lfo[10];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d7;
	m[26] = d8;
	m[27] = d9;
	m[56] = d3;
	m[57] = d4;
	m[58] = d5;
	m[59] = d6;
	out[10] = m[41];
}

}  // namespace fs1r::effects

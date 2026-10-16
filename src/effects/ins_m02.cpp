// effects/ins_m02.cpp - the insertion window's program for type msb 02.
//
// rom::INSERTION_PROGRAMS[2], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 2: ins-05 Phaser1 and ins-06 Phaser2, programmed by
// EffectHost::phaser1() and phaser2() in fx_modulation.cpp. Like ins_m01 it is one of the fifteen
// programs the window shares, and the same words run under both types: what the two differ by is
// which coefficients they write, phaser1() its ten stage words and one feedback word, phaser2() the
// same ten, two feedback words and the LFO phase. Nothing here knows which one is running.
//
// What the 96 steps compute, in the groups the comments open each of. The names are the host's:
// phaser1() and phaser2() name every step they program, through the tables in rom_tables.cpp.
//   0x1a0-0x1b2  the two signals in and the two shelves. r[7] and r[8] are the two the base program
//                leaves the window; r[67] and r[70] take them, r[68] and r[71] are the low shelves'
//                outputs (ins::LPF_PH[0], [1], [2] and ins::LPF_PH_C[0], [1], [2]) and r[69] and
//                r[72] the high shelves' (ins::HPF_PH[0], [1], [2] and ins::HPF_PH_C[0], [1], [2]).
//                0x1aa and 0x1ab take the window's own LFO cell, m[13], into r[122] and r[123], the
//                two sweep registers.
//   0x1b3-0x1c2  the swept coefficient of each cascade. ins::PH_DEPTH[0] and [1], 0x1b3 and 0x1b5,
//                rectify and scale the LFO the register holds, ins::PH_FB[0] and [1], 0x1b4 and
//                0x1b6, add the phase shift and register it, and 0x1c1 and 0x1c2 multiply the register
//                by itself through t[10] and t[11]: the sweep is squared. 0x1b4, 0x1b6, 0x1c1 and
//                0x1c2 each queue t[10] or t[11] three steps ahead, which is how the sections from
//                0x1d0 on come to read them.
//   0x1b7-0x1c0  the ten stage words, ins::PH_STAGES: five taps of each cascade, r[75] to r[79] and
//                r[82] to r[86], summed a half at a time into r[120] and r[121]. phaser1() and
//                phaser2() open one word of each cascade and leave the other four at zero, so each
//                cascade's output is the one stage the Stage parameter names, the 2 and the half
//                cancelling, and walking that parameter walks the output along the cascade.
//   0x1c3-0x1c6  the diffuse taps, ins::PH_DIR: r[81] and r[85] of the second cascade and its
//                selected stage r[121] into r[119], the signal the left output carries. phaser1()'s
//                Diffuse parameter opens one of the three; phaser2() has none, so under it the ROM's
//                words stand and r[119] is r[81], the second cascade's first section.
//   0x1c7-0x1ca  the tail of the sweep. 0x1c7 and 0x1c9 load the offset, and 0x1c8 and 0x1ca take it
//                off the square of 0x1c1 and queue t[10] and t[11] again, so the coefficient the
//                sections run is the fourth power of the phase shift plus depth term less that offset.
//   0x1cb-0x1ce  the cascade on r[74] to r[79]'s input: r[69] and r[72], the shelves' outputs, come
//                in at a quarter and a half, and the other cascade's selected stage r[121] through
//                ins::PH_WIDTH1[0], the one word phaser1() writes. The matching word for the cascade
//                on r[81] to r[86] is 0x1e2, ins::PH_WIDTH2[1], which adds r[121] to the 0x1e3 that
//                takes r[120], and phaser2() writes ins::PH_WIDTH2[0] at 0x1cd as well, so under it
//                each cascade takes the other's output back: that is the FB Level, and the two
//                cascades feed each other by it.
//   0x1cf-0x1e0  the first cascade, six allpass sections r[74] to r[79] on t[10], three steps
//                each: y = t[10] * x - y a step back, r[74] taking r[73] and r[79] taking r[78]. The
//                class-3 step of each group hands the section's output on as the running value.
//   0x1e1-0x1f5  the second cascade, the same six sections r[81] to r[86] on t[11], behind its own
//                input at 0x1e1 to 0x1e3.
//   0x1f6-0x1f9  the dry and wet mix, ins::PH_WET: r[69] and r[72], the shelves' outputs, at
//                ins::PH_WET[0] and [2] against the wet at [1] and [3], into r[17] and r[18], the two
//                outputs, which the base program reads back.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] and r[70]
// the shelves' inputs, r[68], r[69], r[71] and r[72] the two shelves of each channel; r[122] and
// r[123] the two sweep registers, the LFO at 0x1aa and 0x1ab, the phase shift plus depth term at
// 0x1b4 and 0x1b6 and its square at 0x1c1 and 0x1c2, which 0x1c8 and 0x1ca raise to the fourth power;
// r[73] and r[80] the cascades' inputs and r[74] to r[79] and r[81] to r[86] their twelve sections;
// r[120], r[121] and r[119] each cascade's selected stage and the diffuse tap; r[17] and r[18] the
// two outputs.
//
// The old model in vop3_effects.h has a case for each of these two types, and it is a loop of
// `stages` allpasses whose frequency the LFO sweeps through a tan formula: no delay RAM either, but
// no shelves worth the name, no stage crossfade, no cross-cascade feedback and no diffuse taps. It
// is no reference for any number here, and where the two differ, the program above is what runs.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m02(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat(((k[0x1a3] * r[7]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x1a6] * v0) + sum(k[0x1a4] * r[67], k[0x1a5] * r[68])) * 2.0);
	double const v2 = sat(((k[0x1a9] * v1) + sum(k[0x1a7] * r[68], k[0x1a8] * r[69])) * 4.0);
	double const v3 = sat(((k[0x1bb] * r[79]) + sum(k[0x1b7] * r[75], k[0x1b8] * r[76], k[0x1b9] * r[77], k[0x1ba] * r[78])) * 2.0);
	double const v4 = sat(((k[0x1c0] * r[86]) + sum(k[0x1bc] * r[82], k[0x1bd] * r[83], k[0x1be] * r[84], k[0x1bf] * r[85])) * 2.0);
	double const v5 = sat(((k[0x1c6] * r[85]) + sum(k[0x1c3] * v3, k[0x1c4] * v4, k[0x1c5] * r[81])) * 2.0);
	double const v6 = sat(((k[0x1ac] * r[8]) + 0.0) * 2.0);
	double const v7 = sat(((k[0x1af] * v6) + sum(k[0x1ad] * r[70], k[0x1ae] * r[71])) * 2.0);
	double const v8 = sat(((k[0x1b2] * v7) + sum(k[0x1b0] * r[71], k[0x1b1] * r[72])) * 4.0);
	double const v9 = sat((128.0 * k[0x1b4]) + mag((k[0x1b3] * wrap((128.0 * k[0x1aa]) + lfo[13])) + 0.0));
	double const v10 = sat((q15(v9) * v9) + 0.0);
	double const v11 = q15(sat((q15(v10) * v10) - sum(128.0 * k[0x1c7])));
	double const v12 = sat((k[0x1ce] * v4) + sum_on(wrap_sum(((k[0x1cc] * v8) + sum(k[0x1cb] * v2)) * 2.0), k[0x1cd] * v3));
	double const v13 = sat((v11 * v12) - wrap_sum((v11 * r[74]) - sum(r[73])));
	double const v14 = sat((v11 * v13) - wrap_sum((v11 * r[75]) - sum(r[74])));
	double const v15 = sat((v11 * v14) - wrap_sum((v11 * r[76]) - sum(r[75])));
	double const v16 = sat((v11 * v15) - wrap_sum((v11 * r[77]) - sum(r[76])));
	double const v17 = sat((v11 * v16) - wrap_sum((v11 * r[78]) - sum(r[77])));
	double const v18 = sat((128.0 * k[0x1b6]) + mag((k[0x1b5] * wrap((128.0 * k[0x1ab]) + lfo[13])) + 0.0));
	double const v19 = sat((q15(v18) * v18) + 0.0);
	double const v20 = q15(sat((q15(v19) * v19) - sum(128.0 * k[0x1c9])));
	double const v21 = sat((k[0x1e3] * v3) + sum_on(wrap_sum(((k[0x1e1] * v8) + 0.0) * 2.0), k[0x1e2] * v4));
	double const v22 = sat((v20 * v21) - wrap_sum((v20 * r[81]) - sum(r[80])));
	double const v23 = sat((v20 * v22) - wrap_sum((v20 * r[82]) - sum(r[81])));
	double const v24 = sat((v20 * v23) - wrap_sum((v20 * r[83]) - sum(r[82])));
	double const v25 = sat((v20 * v24) - wrap_sum((v20 * r[84]) - sum(r[83])));
	double const v26 = sat((v20 * v25) - wrap_sum((v20 * r[85]) - sum(r[84])));
	r[17] = sat(((k[0x1f7] * v5) + sum(k[0x1f6] * v2)) * 4.0);
	r[18] = sat(((k[0x1f9] * v4) + sum(k[0x1f8] * v8)) * 4.0);
	r[67] = v0;
	r[68] = v1;
	r[69] = v2;
	r[70] = v6;
	r[71] = v7;
	r[72] = v8;
	r[79] = sat((v11 * v17) - wrap_sum((v11 * r[79]) - sum(r[78])));
	r[86] = sat((v20 * v26) - wrap_sum((v20 * r[86]) - sum(r[85])));
	r[119] = v5;
	r[120] = v3;
	r[121] = v4;
	r[122] = v10;
	r[123] = v19;
	m[3] = in[0][13];
	m[4] = in[0][15];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[42] = cell(sum_on(-wrap_sum((k[0x1a1] * m[42]) - (sum(128.0 * k[0x1a0]) / 32768.0)), 128.0 * k[0x1a2]));
	t[10] = v11;
	t[11] = v20;
	r[78] = sat((v11 * v16) - wrap_sum((v11 * r[78]) - sum(r[77])));
	r[85] = sat((v20 * v25) - wrap_sum((v20 * r[85]) - sum(r[84])));
	r[77] = sat((v11 * v15) - wrap_sum((v11 * r[77]) - sum(r[76])));
	r[84] = sat((v20 * v24) - wrap_sum((v20 * r[84]) - sum(r[83])));
	r[76] = sat((v11 * v14) - wrap_sum((v11 * r[76]) - sum(r[75])));
	r[83] = sat((v20 * v23) - wrap_sum((v20 * r[83]) - sum(r[82])));
	r[75] = sat((v11 * v13) - wrap_sum((v11 * r[75]) - sum(r[74])));
	r[82] = sat((v20 * v22) - wrap_sum((v20 * r[82]) - sum(r[81])));
	r[74] = sat((v11 * v12) - wrap_sum((v11 * r[74]) - sum(r[73])));
	r[81] = sat((v20 * v21) - wrap_sum((v20 * r[81]) - sum(r[80])));
	r[73] = sat((k[0x1ce] * v4) + sum_on(wrap_sum(((k[0x1cc] * v8) + sum(k[0x1cb] * v2)) * 2.0), k[0x1cd] * v3));
	r[80] = sat((k[0x1e3] * v3) + sum_on(wrap_sum(((k[0x1e1] * v8) + 0.0) * 2.0), k[0x1e2] * v4));
}

}  // namespace fs1r::effects

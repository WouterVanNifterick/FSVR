// effects/ins_m05.cpp - the insertion window's program for Distortion, Overdrive, Amp Sim,
// Compressor, Comp+Dist, HM Enhancer and Noise Gate: its 96 steps.
//
// rom::INSERTION_PROGRAMS[5], the program the insertion window holds under the seven types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 5: ins-28 Distortion and ins-30 Overdrive on
// EffectHost::distortion, ins-32 Amp Sim on EffectHost::ampSimulator, ins-24 Compressor on
// EffectHost::compressor, ins-25 Comp+Dist on EffectHost::compressorDistortion, ins-22 HM Enhancer on
// EffectHost::enhancer and ins-23 Noise Gate on EffectHost::noiseGate. One program, seven types: the
// handlers differ only in which words they write, so nothing here knows which of them is running.
// Nothing in this program touches the delay RAM, and no step of it is a control step.
//
// So the seven types are one chain: a fixed two-channel input filter, a peak detector on the sum of
// the two channels, an eight-segment gain curve off the detector, a smoother, a five-multiply shape,
// three tone filters and a dry and wet mix. The gate and the compressor use the whole of it; the
// distortion and the enhancer reach it through ins::DIST_MODE. The old model in vop3_effects.h runs
// tanh and a power law for the shape, one envelope and a hard ratio for the dynamics, and a shelf
// plus a peak; it has none of this program's words, so it is no reference for any number here.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[1] to r[6] the
// shape's chain and then the curve's breakpoints; r[17] and r[18] the two outputs; r[67] to r[74] the
// two input filters and their states; r[75], r[76] and r[121] the detector's states; r[79] the
// shaper's output; r[80], r[82], r[84], r[86] the three tone filters' delays; r[81], r[83] the first
// two tone filters' outputs and r[85] the third's; r[119] to r[121] the detector's input scale; r[123]
// the thresholded level; r[124], r[125] the level times the smoother, and then the curve's last two
// breakpoints; r[126], r[127] the output's input scale. The cells: m[3], m[7] the serial inputs this
// block takes for the base program, m[4] from page 14 and m[13] the page 13's LFO; m[42], m[43] the
// curve's output and that one pass old; m[44] the smoother's state; m[45] the left output filter's
// state; m[46] the right one's; m[47], m[48] the two channels' dry signals; m[49] the level.
//
// The three t registers the program uses are t[9], written at 0x1ba, read at 0x1c9 and 0x1ca, the
// smoother's gain; and t[10] and t[11], written at 0x1c9 and 0x1ca and read at 0x1dc and 0x1e6, that
// same gain squared onto the two channels.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m05(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat(((k[0x1a1] * r[8]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x1a6] * v0) + sum(k[0x1a2] * r[71], k[0x1a3] * r[72], k[0x1a4] * r[73], k[0x1a5] * r[74])) * 2.0);
	double const v2 = sat(((k[0x1b1] * v1) + 0.0) * 2.0);
	double const v3 = sat(((k[0x1a9] * r[8]) + sum(k[0x1a8] * r[7])) * 2.0);
	double const v4 = sat(((k[0x1af] * v3) + sum(k[0x1ab] * r[67], k[0x1ac] * r[68], k[0x1ad] * r[69], k[0x1ae] * r[70])) * 2.0);
	double const v5 = sat(((k[0x1b2] * v4) + 0.0) * 2.0);
	double const v6 = mag(((k[0x1b5] * v5) + sum(k[0x1b4] * v2)) * 16.0);
	double const v7 = cell(v6);
	double const v8 = pos((k[0x1b6] * r[75]) - v6);
	double const v9 = sat(v7 + v8);
	double const v10 = sat(v7 + pos((k[0x1c0] * v9) - wrap_sum(v7 - sum_on(sum(k[0x1bd] * r[76]) / 32768.0, k[0x1be] * r[76]))));
	double const v11 = sat((k[0x1cc] * v10) + 0.0);
	double const v12 = sat(((k[0x1aa] * sat(((k[0x1a7] * sat(((k[0x1a0] * m[45]) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v13 = sat(((k[0x1b0] * v12) + 0.0) * 16.0);
	double const v14 = sat(((k[0x1b3] * v13) + 0.0) * 16.0);
	double const v15 = sat(((k[0x1b7] * v14) + 0.0) * 16.0);
	double const v16 = sat(((k[0x1bb] * v15) + 0.0) * 16.0);
	double const v17 = sat((k[0x1cb] * sat((k[0x1c8] * sat(((k[0x1c2] * v16) + 0.0) * 16.0)) + sum(k[0x1c3] * v12, k[0x1c4] * v13, k[0x1c5] * v14, k[0x1c6] * v15, k[0x1c7] * v16))) + 0.0);
	double const v18 = sat(((k[0x1d1] * v17) + sum(k[0x1cd] * r[79], k[0x1ce] * r[80], k[0x1cf] * r[81], k[0x1d0] * r[82])) * 2.0);
	double const v19 = sat(((k[0x1d6] * v18) + sum(k[0x1d2] * r[81], k[0x1d3] * r[82], k[0x1d4] * r[83], k[0x1d5] * r[84])) * 2.0);
	double const v20 = sat(((k[0x1db] * v19) + sum(k[0x1d7] * r[83], k[0x1d8] * r[84], k[0x1d9] * r[85], k[0x1da] * r[86])) * 16.0);
	double const v21 = sat((k[0x1ba] * m[42]) + sum(k[0x1b8] * m[43], k[0x1b9] * m[44]));
	double const v22 = q15(v21);
	double const v23 = sat((v22 * v5) + 0.0);
	double const v24 = q15(v23);
	double const v25 = sat((v24 * v5) + 0.0);
	double const v26 = sat((v22 * v2) + 0.0);
	double const v27 = q15(v26);
	double const v28 = sat((v27 * v2) + 0.0);
	double const v29 = pos((128.0 * k[0x1f3]) + v11);
	double const v30 = pos((128.0 * k[0x1f4]) + v11);
	double const v31 = pos((128.0 * k[0x1ed]) + v11);
	double const v32 = pos((128.0 * k[0x1ef]) + v11);
	double const v33 = pos((128.0 * k[0x1f0]) + v11);
	double const v34 = pos((128.0 * k[0x1ee]) + v11);
	double const v35 = pos((128.0 * k[0x1f1]) + v11);
	double const v36 = pos((128.0 * k[0x1f2]) + v11);
	double const v37 = sat(((k[0x1df] * v25) + sum(k[0x1dd] * v23, k[0x1de] * v5)) * 16.0);
	double const v38 = sat(((k[0x1e9] * v28) + sum(k[0x1e7] * v26, k[0x1e8] * v2)) * 16.0);
	r[1] = pos((128.0 * k[0x1ed]) + v11);
	r[2] = pos((128.0 * k[0x1ee]) + v11);
	r[3] = pos((128.0 * k[0x1ef]) + v11);
	r[4] = pos((128.0 * k[0x1f0]) + v11);
	r[5] = pos((128.0 * k[0x1f1]) + v11);
	r[6] = pos((128.0 * k[0x1f2]) + v11);
	r[17] = sat(((k[0x1e3] * v20) + m[47]) * 4.0);
	r[18] = sat(((k[0x1e5] * m[46]) + sum_on(m[48], k[0x1e4] * v20)) * 4.0);
	r[68] = r[67];
	r[70] = r[69];
	r[72] = r[71];
	r[74] = r[73];
	r[75] = v9;
	r[76] = v10;
	r[80] = r[79];
	r[82] = r[81];
	r[84] = r[83];
	r[86] = r[85];
	r[119] = v5;
	r[120] = v2;
	r[121] = v8;
	r[123] = v11;
	r[124] = pos((128.0 * k[0x1f3]) + v11);
	r[125] = pos((128.0 * k[0x1f4]) + v11);
	r[126] = v25;
	r[127] = v28;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[43] = m[42];
	m[44] = cell(v21);
	m[45] = cell(sat(((k[0x1e2] * v37) + sum(k[0x1e0] * r[77], k[0x1e1] * m[45])) * 2.0));
	m[46] = cell(sat(((k[0x1ec] * v38) + sum(k[0x1ea] * r[78], k[0x1eb] * m[46])) * 2.0));
	m[47] = cell(sat((k[0x1fe] * r[7]) + 0.0));
	m[48] = cell(sat((k[0x1ff] * r[8]) + 0.0));
	m[49] = v7;
	t[9] = v22;
	t[10] = v24;
	t[11] = v27;
	r[67] = v3;
	r[69] = v4;
	r[71] = v0;
	r[73] = v1;
	r[77] = sat(((k[0x1df] * v25) + sum(k[0x1dd] * v23, k[0x1de] * v5)) * 16.0);
	r[78] = sat(((k[0x1e9] * v28) + sum(k[0x1e7] * v26, k[0x1e8] * v2)) * 16.0);
	r[79] = v17;
	r[81] = v18;
	r[83] = v19;
	r[85] = v20;
	m[42] = cell(pos((128.0 * k[0x1fd]) + sum_on(wrap_sum(((k[0x1fa] * v36) + wrap_sum(((k[0x1f9] * v35) + wrap_sum(((k[0x1f8] * v34) + wrap_sum(((k[0x1f7] * v33) + wrap_sum(((k[0x1f6] * v32) + wrap_sum(((k[0x1f5] * v31) + 0.0) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 4.0), k[0x1fb] * v29, k[0x1fc] * v30)));
}

}  // namespace fs1r::effects

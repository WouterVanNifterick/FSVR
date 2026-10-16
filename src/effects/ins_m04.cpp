// effects/ins_m04.cpp - the insertion window's program for Auto Wah + Distortion, Auto Wah +
// Overdrive, Touch Wah + Distortion and Touch Wah + Overdrive: its 96 steps.
//
// rom::INSERTION_PROGRAMS[4], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 4: ins-14 A-Wah+Dist and ins-15 A-Wah+Odrv on
// EffectHost::autoWah, ins-16 T-Wah+Dist and ins-17 T-Wah+Odrv on EffectHost::touchWah. The same 96
// words run under every one of them; what the type changes is which words fx_drive.cpp's two handlers
// write, so nothing here knows which one is running. Nothing in this program touches the delay RAM.
//
//   0x1a0-0x1ad  the two channels' EQ, two order-1 shelves each, the low and the high shelf of
//                fx_common.cpp's shelves() over ins::LPF_WAH, ins::HPF_WAH and their copies
//                ins::LPF_WAH_C and ins::HPF_WAH_C. 0x1a0 and 0x1a7 take the two inputs the base
//                program leaves the window into r[67] and r[69], and 0x1a5, 0x1a6 and 0x1a7 take the
//                serial inputs of page 13 and that page's LFO into m[3], m[7] and m[13]. The high
//                shelves' outputs are r[68] and r[70] and the low shelves' are the cells m[42] and
//                m[43].
//   0x1ae-0x1b0  ins::WAH_MODE's pair: r[76] takes the square root of a half times m[48] and m[49],
//                the two channels' wah outputs summed into one signal for the band filters, and
//                0x1af and ins::WAH_HPF_2's b1 step at 0x1b0 publish m[50] and m[51], that mix one
//                pass old, for the dry side of the output below.
//   0x1b0-0x1b2  ins::WAH_HPF_2, the distortion's fixed input highpass, over r[76] into r[77].
//   0x1b3-0x1b5  ins::WAH_LPF_2, its fixed input lowpass, over r[77] into r[78].
//   0x1b6-0x1bb  ins::WAH_MODE2's pair, which only touchWah() writes, and ins::WAH_HPF, the input
//                highpass it designs, over r[71] into r[72]. Under Auto Wah the pair's two words are
//                zero, r[71] and r[72] stay zero and this path is silent.
//   0x1b8-0x1c0  the drive. ins::WAH_DRIVE's first word at 0x1b8 carries the signal into r[122], and
//                the two words fx_common.cpp's storeDrive() writes, 0x1bc and 0x1c0, square it.
//   0x1bd-0x1c2  ins::WAH_SENSE's pair on each channel: acc = k * m[44] - r[68] and then the sense
//                word times r[74] less that, which is what the resonator's feedback is taken from, so
//                0x1c1 and 0x1c2 are the same two steps on the right over m[45] and r[70].
//   0x1bf        ins::WAH_ATTACK, the Touch Wah's sensitivity word times the input highpass's output,
//                rectified into m[56].
//   0x1c3-0x1cc  ins::WAH_MIRROR's three words, the rectifier round m[56], and ins::WAH_LPF, the
//                input lowpass touchWah() designs, which runs over that level in m[46] and m[47].
//   0x1cd-0x1dc  four of ins::WAH_BAND's twelve words, the waveshaper's: 0x1cd is the gain
//                fx_design.cpp's storeEdge() derives and 0x1d1, 0x1d4, 0x1d8 and 0x1dc are the shape
//                word it cascades, the last of them landing in m[57].
//   0x1ce-0x1d7  the sweep. 0x1ce is ins::WAH_CUTOFF's class-1 load, 0x1cf ins::WAH_DEPTH's word
//                times the page's LFO on top of it, and 0x1d0 to 0x1d7 the one-pole smoother that
//                turns that, with the level in m[47], into t[9], with 0x1d2, 0x1d5 and 0x1d9 the
//                padding between them.
//   0x1da-0x1db  the resonator itself: r[74] and r[75] take t[9] times m[58] and m[59] plus
//                themselves, which is the state the wah filters with.
//   0x1dc-0x1e2  the rest of ins::WAH_BAND: 0x1dd carries the zero storeEdge() leaves in its seventh
//                word, and 0x1de to 0x1e2 are the five terms of the series, each scaled by the
//                6553 storeEdge() writes, summed into r[122].
//   0x1e3-0x1e5  the resonator's two feedback accumulators m[44] and m[45], which 0x1bd and 0x1c1
//                subtract, and ins::WAH_DRIVE's fourth word, the gain storeDrive() derives from the
//                drive, into r[79].
//   0x1e6-0x1ea  ins::WAH_LPF2, the lowpass fx_drive.cpp's wahDistortionTone() designs: b0 at 0x1ea,
//                b1 at 0x1e7, b2 at 0x1e6, -a1 at 0x1e9 and -a2 at 0x1e8, over r[79] into r[81].
//   0x1eb-0x1ef  ins::WAH_PEAK, the same five steps over r[81] into r[83].
//   0x1f0-0x1f4  ins::WAH_LOW_SHELF2, the same five steps over r[83] into r[85].
//   0x1f5-0x1fc  ins::WAH_WET's eight words. 0x1f5 and 0x1f6 mix the dry word times r[68] with the
//                wet word times r[74] into m[48], and 0x1f7 and 0x1f8 the same for the right channel
//                into m[49]; 0x1f9 to 0x1fc take the dry words on m[50] and m[51] instead and the
//                wet word on the tone chain's output r[85], and put the sums into the window's outputs
//                r[17] and r[18].
//   0x1fd-0x1ff  three class-0 steps with no ALU, so only the x latch runs.
//
// So the wah and the distortion are two chains and not one: the EQ feeds the resonator r[74] and
// r[75], whose state the band filters and then the drive and the waveshaper take, and the output mix
// puts the resonator's own signal back beside the shaper's. The old model in vop3_effects.h runs a
// biquad whose cutoff the LFO or the envelope sweeps and then a waveshaper on its output, with the
// drive and the edge hand-written as a power law; it has none of this program's words, so it is no
// reference for any number here.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] and r[69] the
// two channels' EQ inputs and r[68] and r[70] the two channels' EQ outputs; r[71], r[72] the Touch
// Wah's input highpass and its output; r[73] the rectifier's output; r[74], r[75] the resonator's two
// states; r[76] the mode pair, r[77] the band highpass and r[78] the band lowpass; r[79] the drive's
// gain, r[81], r[83] and r[85] the three tone filters' outputs; r[80], r[82], r[84] and r[86] their
// four delay registers; r[121] the smoother's state; r[122] the drive, then the waveshaper's output;
// r[123] to r[125] the shaper's cascade; r[17] and r[18] the two outputs. The cells: m[3], m[4] the
// serial inputs this block takes for the base program, m[7] the second bank's, m[13] the page's LFO,
// m[42] and m[43] the two low shelves' outputs, m[44] and m[45] the resonator's feedback
// accumulators, m[46] and m[47] the level pair, m[48] and m[49] the two channels' wah outputs, m[50]
// and m[51] the same mix one pass old, m[56] the sensitivity level, m[57] the shaper's fourth term and
// m[58], m[59] the sense feedback.
//
// The one t register the program uses is t[9], written at 0x1d0, 0x1d3 and 0x1d7 and read at 0x1d3,
// 0x1d6, 0x1da, 0x1db, 0x1e3 and 0x1e4: the cutoff the host wrote at 0x1ce, smoothed, and then the
// gain the resonator runs at.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m04(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat((k[0x1af] * m[49]) + sum(k[0x1ae] * m[48]));
	double const v1 = sat(((k[0x1b2] * v0) + sum(k[0x1b0] * r[76], k[0x1b1] * r[77])) * 2.0);
	double const v2 = sat(((k[0x1b5] * v1) + sum_on(sat((k[0x1b3] * r[77]) + 0.0), k[0x1b4] * r[78])) * 2.0);
	double const v3 = sat(((k[0x1c0] * sat(((k[0x1bc] * sat(((k[0x1b8] * v2) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v4 = sat(((k[0x1cd] * v3) + 0.0) * 16.0);
	double const v5 = sat(((k[0x1d1] * v4) + 0.0) * 16.0);
	double const v6 = sat(((k[0x1d4] * v5) + 0.0) * 16.0);
	double const v7 = cell(sat(((k[0x1d8] * v6) + 0.0) * 16.0));
	double const v8 = sat((k[0x1e2] * sat(((k[0x1dc] * v7) + 0.0) * 16.0)) + sum(k[0x1dd] * v3, k[0x1de] * v4, k[0x1df] * v5, k[0x1e0] * v6, k[0x1e1] * v7));
	double const v9 = sat((k[0x1e5] * v8) + 0.0);
	double const v10 = sat(((k[0x1ea] * v9) + sum(k[0x1e6] * r[80], k[0x1e7] * r[79], k[0x1e8] * r[82], k[0x1e9] * r[81])) * 2.0);
	double const v11 = sat(((k[0x1ef] * v10) + sum(k[0x1eb] * r[81], k[0x1ec] * r[82], k[0x1ed] * r[84], k[0x1ee] * r[83])) * 2.0);
	double const v12 = sat(((k[0x1f4] * v11) + sum(k[0x1f0] * r[83], k[0x1f1] * r[84], k[0x1f2] * r[85], k[0x1f3] * r[86])) * 16.0);
	double const v13 = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	double const v14 = cell(sat(((k[0x1a3] * v13) + sum(k[0x1a1] * r[67], k[0x1a2] * m[42])) * 2.0));
	double const v15 = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	double const v16 = cell(sat(((k[0x1aa] * v15) + sum(k[0x1a8] * r[69], k[0x1a9] * m[43])) * 2.0));
	double const v17 = sat(((k[0x1a6] * v14) + sum(k[0x1a4] * m[42], k[0x1a5] * r[68])) * 4.0);
	double const v18 = sat(((k[0x1ad] * v16) + sum(k[0x1ab] * m[43], k[0x1ac] * r[70])) * 4.0);
	double const v19 = sat((k[0x1b7] * v18) + sum(k[0x1b6] * v17));
	double const v20 = sat(((k[0x1bb] * v19) + sum(k[0x1b9] * r[71], k[0x1ba] * r[72])) * 2.0);
	double const v21 = cell(mag(((k[0x1bf] * v20) + 0.0) * 16.0));
	double const v22 = cell(sat((k[0x1be] * r[74]) - wrap_sum((k[0x1bd] * m[44]) - v17)));
	double const v23 = sat((k[0x1c4] * v21) + pos((k[0x1c3] * r[73]) - v21));
	double const v24 = cell(sat((k[0x1c9] * v21) + pos((k[0x1c8] * v23) - wrap_sum((k[0x1c7] * v21) - sum_on(sum(k[0x1c5] * m[46]) / 32768.0, k[0x1c6] * m[46])))));
	double const v25 = cell(sat((k[0x1cc] * v24) + sum(k[0x1ca] * m[46], k[0x1cb] * m[47])));
	double const v26 = sat((k[0x1d0] * v25) + sum(128.0 * k[0x1ce], k[0x1cf] * lfo[13]));
	double const v27 = (q15(v26) * v26) + 0.0;
	double const v28 = wrap(v27);
	double const v29 = q15(sat((128.0 * k[0x1d7]) + sum(q15(sum(q15(v26) * v26)) * v28)));
	double const v30 = cell(sat((k[0x1c2] * r[75]) - wrap_sum((k[0x1c1] * m[45]) - v18)));
	double const v31 = sat((v29 * v22) + r[74]);
	double const v32 = sat((v29 * v30) + r[75]);
	r[17] = sat(((k[0x1fa] * v12) + sum(k[0x1f9] * m[48])) * 4.0);
	r[18] = sat(((k[0x1fc] * v12) + sum(k[0x1fb] * m[49])) * 4.0);
	r[67] = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	r[68] = sat(((k[0x1a6] * v14) + sum(k[0x1a4] * m[42], k[0x1a5] * r[68])) * 4.0);
	r[69] = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	r[70] = sat(((k[0x1ad] * v16) + sum(k[0x1ab] * m[43], k[0x1ac] * r[70])) * 4.0);
	r[72] = sat(((k[0x1bb] * v19) + sum(k[0x1b9] * r[71], k[0x1ba] * r[72])) * 2.0);
	r[73] = sat((k[0x1c4] * v21) + pos((k[0x1c3] * r[73]) - v21));
	r[74] = sat((v29 * v22) + r[74]);
	r[75] = sat((v29 * v30) + r[75]);
	r[76] = v0;
	r[77] = v1;
	r[78] = v2;
	r[80] = r[79];
	r[82] = r[81];
	r[84] = r[83];
	r[86] = r[85];
	r[121] = v28;
	r[122] = v8;
	r[123] = v4;
	r[124] = v5;
	r[125] = v6;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[42] = v14;
	m[43] = v16;
	m[44] = cell(sat((v29 * v31) + m[44]));
	m[45] = cell(sat((v29 * v32) + m[45]));
	m[46] = v24;
	m[47] = v25;
	m[50] = m[48];
	m[51] = m[49];
	m[56] = v21;
	m[57] = v7;
	m[58] = v22;
	m[59] = v30;
	t[9] = v29;
	r[71] = sat((k[0x1b7] * v18) + sum(k[0x1b6] * v17));
	r[79] = v9;
	r[81] = v10;
	r[83] = v11;
	r[85] = v12;
	m[48] = cell(sat(((k[0x1f6] * v31) + sum(k[0x1f5] * v17)) * 4.0));
	m[49] = cell(sat(((k[0x1f8] * v32) + sum(k[0x1f7] * v18)) * 4.0));
}

}  // namespace fs1r::effects

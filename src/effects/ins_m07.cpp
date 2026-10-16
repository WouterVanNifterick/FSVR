// effects/ins_m07.cpp - the insertion window's program for Pitch Chng, Ens Detune and 3-Band EQ:
// its 96 steps.
//
// rom::INSERTION_PROGRAMS[7], the program the insertion window holds under the three types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 7: ins-07 Pitch Chng on EffectHost::pitchChange,
// ins-08 Ens Detune on EffectHost::ensembleDetune and ins-21 3-Band EQ on EffectHost::threeBandEq. One
// program, three types: the handlers differ only in which words they write, so nothing here knows which of
// them is running. No step of it is a control step.
//
// The ROM names the steps, and the lists agree with the program word for word:
//
//   LFO_DIR_STEPS  {0x1A0,0x1A8,0x1B0,0x1B7}        the four read positions
//   PC_FB          {0x1F9,0x1FE,0x1A7,0x1AA,0x1AF,0x1B1}    the delay feedback
//   PC_MIX         {0x1BA,0x1CB,0x1CA,0x1E9,0x1EE,0x1EB,0x1F0}
//   PC_WET         {0x1E8,0x1ED}
//   PC_VOICE1_A    {0x1E4}   PC_VOICE1_B {0x1E7}   PC_VOICE2 {0x1E6,0x1E5}
//   EQ3_LEVEL      {0x1EA}
//   EQ7_A          {0x1BF,0x1BB,0x1BC,0x1BD,0x1BE}   the low band: b0, b1, b2, -a1, -a2
//   EQ7_B          {0x1C4,0x1C0,0x1C1,0x1C2,0x1C3}   the middle band, the same five
//   EQ7_C          {0x1C9,0x1C5,0x1C6,0x1C7,0x1C8}   the high band, the same five
//   EQ7_A_C        {0x1D0,0x1CC,0x1CD,0x1CE,0x1CF}   and each band's copy for the other channel
//   EQ7_B_C        {0x1D5,0x1D1,0x1D2,0x1D3,0x1D4}
//   EQ7_C_C        {0x1DA,0x1D6,0x1D7,0x1D8,0x1D9}
//   PC_SLOTS       {0x7E,0x68,0x69,0x6A,0x6B,0x7F,0x6C,0x6D,0x6E,0x6F}
//
// fx_tone.cpp's threeBandEq() designs each band with designFilter(2, 1, ...) for the two lower ones and
// designFilter(2, 4, ...) for the high one, which writes b0, b1, b2, -a1 and -a2 into the list in that
// order and copies them to the other channel's list. Each word is the designed coefficient times
// 2048 * (16 >> scale), which the chip's s16() halves, and the b0 step's own route multiplies the whole
// section it closes: a shift of one on 0x1BF, 0x1C4, 0x1D0 and 0x1D5, and a shift of three on 0x1C9 and
// 0x1DA, so RSCALE's 2 and 16 restore the factor the designer's scale took out. What each section computes
// is then
//
//   y[n] = b0 x[n-1] + b1 x[n-1] + b2 x[n-2] + (-a1) y[n-1] + (-a2) y[n-2]
//
// six order-2 sections, three bands per channel, each on the previous band's output. The gain path runs one
// sample behind the input because the b1 step reads the input register the step before the b0 step
// refilled, which is what r[68] and r[70] and r[72] are for: each is an rcopy step, so it takes the value
// the step before it latched.
//
// The mode parameter is the one input mix. ins::PC_MIX[0] (0x1BA) is the stereo switch on the left input,
// ins::PC_MIX[1] and [2] (0x1CB, 0x1CA) sum the two inputs into r[75], and ins::PC_MIX[3] (0x1E9) against
// ins::EQ3_LEVEL (0x1EA) choose which chain joins the dry signal, so in stereo the left chain does and in
// mono the summed one. The old model in vop3_effects.h runs this as two detuned fractional taps crossfaded
// in floating point over a shelf, a peak and a high shelf; it has none of these words, so it is no reference
// for any number here.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m07(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = pos((k[0x1a0] * m[12]) + 0.0);
	double const v1 = index(v0);
	double const d0 = s.ram[s.at(0x68, int(v1))];
	double const d1 = s.ram[s.at(0x69, 1 + int(v1))];
	double const v2 = wrap((m[12] + 0.0) * 2.0);
	double const v3 = pos((k[0x1a8] * pos((128.0 * k[0x1a5]) + sum(k[0x1a4] * v2))) + 0.0);
	double const v4 = index(v3);
	double const d2 = s.ram[s.at(0x6a, int(v4))];
	double const d3 = s.ram[s.at(0x6b, 1 + int(v4))];
	double const v5 = pos((k[0x1b0] * lfo[13]) + 0.0);
	double const v6 = index(v5);
	double const d4 = s.ram[s.at(0x6c, int(v6))];
	double const d5 = s.ram[s.at(0x6d, 1 + int(v6))];
	double const v7 = wrap((lfo[13] + 0.0) * 2.0);
	double const v8 = pos((128.0 * k[0x1ad]) + sum(k[0x1ac] * v7));
	double const v9 = pos((k[0x1b7] * v8) + 0.0);
	double const v10 = index(v9);
	double const d6 = s.ram[s.at(0x6e, int(v10))];
	double const d7 = s.ram[s.at(0x6f, 1 + int(v10))];
	double const d8 = s.ram[s.at(0x70)];
	double const d9 = s.ram[s.at(0x71)];
	s.ram[s.at(0x7b)] = clip32(sum(d8));
	s.ram[s.at(0x7c)] = clip32(sum(d9));
	double const v11 = sat(((k[0x1ba] * r[7]) + 0.0) * 2.0);
	double const v12 = sat(((k[0x1bf] * v11) + sum(k[0x1bb] * r[67], k[0x1bc] * r[68], k[0x1bd] * r[69], k[0x1be] * r[70])) * 2.0);
	double const v13 = sat(((k[0x1c4] * v12) + sum(k[0x1c0] * r[69], k[0x1c1] * r[70], k[0x1c2] * r[71], k[0x1c3] * r[72])) * 2.0);
	double const v14 = sat(((k[0x1c9] * v13) + sum(k[0x1c5] * r[71], k[0x1c6] * r[72], k[0x1c7] * r[73], k[0x1c8] * r[74])) * 16.0);
	double const v15 = sat(((k[0x1cb] * r[7]) + sum(k[0x1ca] * r[8])) * 2.0);
	double const v16 = sat(((k[0x1d0] * v15) + sum(k[0x1cc] * r[75], k[0x1cd] * r[76], k[0x1ce] * r[77], k[0x1cf] * r[78])) * 2.0);
	double const v17 = sat(((k[0x1d5] * v16) + sum(k[0x1d1] * r[77], k[0x1d2] * r[78], k[0x1d3] * r[79], k[0x1d4] * r[80])) * 2.0);
	double const v18 = sat(((k[0x1da] * v17) + sum(k[0x1d6] * r[79], k[0x1d7] * r[80], k[0x1d8] * r[81], k[0x1d9] * r[82])) * 16.0);
	double const v19 = frac(v3);
	double const v20 = sat((v19 * d3) - wrap_sum((v19 * d2) - d2));
	double const v21 = mag(v2 + 0.0);
	double const v22 = q15(sat(((k[0x1b2] * pos((128.0 * k[0x1aa]) + v21)) + 0.0) * 16.0));
	double const v23 = frac(v0);
	double const v24 = pos((128.0 * k[0x1a7]) + v21);
	double const v25 = q15(sat(((k[0x1ab] * v24) + 0.0) * 16.0));
	double const v26 = sat((v25 * sat((v23 * d1) - wrap_sum((v23 * d0) - d0))) - wrap_sum((v22 * v20) - v20));
	double const v27 = frac(v9);
	double const v28 = sat((v27 * d7) - wrap_sum((v27 * d6) - d6));
	double const v29 = mag(v7 + 0.0);
	double const v30 = pos((128.0 * k[0x1b1]) + v29);
	double const v31 = q15(sat(((k[0x1b4] * v30) + 0.0) * 16.0));
	double const v32 = frac(v5);
	double const v33 = pos((128.0 * k[0x1af]) + v29);
	double const v34 = q15(sat(((k[0x1b3] * v33) + 0.0) * 16.0));
	double const v35 = sat((v34 * sat((v32 * d5) - wrap_sum((v32 * d4) - d4))) - wrap_sum((v31 * v28) - v28));
	s.ram[s.at(0x7e)] = clip32(sat((k[0x1fa] * v35) + sum(k[0x1f7] * v14, k[0x1f8] * v18, k[0x1f9] * v26)));
	s.ram[s.at(0x7f)] = clip32(sat((k[0x1ff] * v26) + sum(k[0x1fd] * v18, k[0x1fe] * v35)));
	double const v36 = sat((k[0x1e5] * v35) + sum(k[0x1e4] * v26));
	double const v37 = sat((k[0x1e7] * v26) + sum(k[0x1e6] * v35));
	r[17] = sat(((k[0x1eb] * v36) + sum(k[0x1e8] * r[7], k[0x1e9] * v14, k[0x1ea] * v18)) * 4.0);
	r[18] = sat(((k[0x1f0] * v37) + sum(k[0x1ed] * r[8], k[0x1ee] * v18, k[0x1ef] * v14)) * 4.0);
	r[68] = r[67];
	r[70] = r[69];
	r[72] = r[71];
	r[74] = r[73];
	r[76] = r[75];
	r[78] = r[77];
	r[80] = r[79];
	r[82] = r[81];
	r[119] = v36;
	r[120] = v37;
	r[121] = v26;
	r[122] = v35;
	r[123] = v24;
	r[124] = v30;
	r[125] = v8;
	r[126] = v33;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[14] = lfo[14];
	m[15] = lfo[15];
	m[42] = d8;
	m[43] = d9;
	m[56] = d0;
	m[57] = d1;
	m[58] = d4;
	m[59] = d5;
	m[60] = d2;
	m[61] = d3;
	m[62] = d6;
	m[63] = d7;
	t[3] = v27;
	t[9] = v25;
	t[10] = v23;
	t[11] = v22;
	t[12] = v19;
	t[13] = v34;
	t[14] = v32;
	t[15] = v31;
	r[67] = v11;
	r[69] = v12;
	r[71] = v13;
	r[73] = v14;
	r[75] = v15;
	r[77] = v16;
	r[79] = v17;
	r[81] = v18;
}

}  // namespace fs1r::effects

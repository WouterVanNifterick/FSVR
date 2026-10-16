// effects/var_m03.cpp - the variation window's program for HM Enhancer, Noise Gate and
// Compressor: its 72 steps.
//
//   DYN_HPF2      {0x15E, 0x15A, 0x15B, 0x15C, 0x15D}   the left input's filter
//   DYN_HPF2_COPY {0x164, 0x160, 0x161, 0x162, 0x163}   the right input's, the same five words
//   DYN_THRESHOLD {0x165}
//   HM_DRIVE      {0x166, 0x167}                        the enhancer's drive only
//   DYN_RATIO     {0x170, 0x173, 0x171, 0x172, 0x174, 0x175, 0x176, 0x177}
//   DYN_ATTACK    {0x17B, 0x179, 0x17A}
//   DYN_RELEASE   {0x17E, 0x183, 0x185}
//   DYN_HPF       {0x18E, 0x18C, 0x18D}                  the left output's filter
//   DYN_HPF_COPY  {0x194, 0x192, 0x193}                  the right output's
//   DYN_OUT       {0x195, 0x196, 0x197, 0x199}           dry left, wet left, dry right, wet right
//
// fx_design.cpp's designFilter writes an order-2 filter as b0, b1, b2, -a1, -a2 in that list order, and
// an order-1 one as b0, b1, -a1. The two input filters are designed as HPF2 for the enhancer and as HPF1
// for the compressor and the gate, and HPF1 leaves b2 and -a2 at zero, so the five words carry three
// coefficients and two zeros for those two types. The two output filters are HPF1 for the enhancer and
// GAIN_ONLY for the other two, over the same three words.
//
// The registers: r[5] and r[6] the two serial inputs the base program writes, r[15] and r[16] the two
// outputs, r[1] and r[2] the two driven inputs, r[103] and r[107] the two filter inputs doubled, r[104]
// and r[108] their one-pass delays, r[105] and r[109] their states and outputs, r[106] and r[110] their
// outputs delayed again, r[111] and r[113] the output filters' states with r[112] and r[114] their
// sums and one-pass delays, r[119] the left input's gain at first and the level from 0x17d on, r[120]
// and r[121] the two channels after t[9], r[122] and r[123] the two after t[10] and t[11], r[126] the
// level the threshold scales, and r[124] and r[125] the ladder's two taps this row's constants hold at
// zero. The cells: m[2] and m[6] the two serial inputs, m[11] and m[12] the two LFO values, m[28] to
// m[33] the six published delay RAM reads, m[54] and m[55] the release chain's two accumulators. The
// delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring pointer with no index; this
// program writes none of them, so the lines behind these taps are filled elsewhere. Each capture takes
// the read five steps before it (captured()'s lower bound), so m[28] carries the slot 0x56 word, m[29]
// the slot 0x57 word, m[30] the slot 0x58 word, and m[31], m[32] and m[33] the slots 0x63, 0x64 and
// 0x65 words.
//
// The three t registers the program uses are t[9], written at 0x17b and read at 0x17f and 0x180, and
// t[10] and t[11], written at 0x17f and 0x180 and read at 0x187 and 0x188.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m03(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const d3 = s.ram[s.at(0x63)];
	double const d4 = s.ram[s.at(0x64)];
	double const d5 = s.ram[s.at(0x65)];
	double const v0 = sat(((k[0x159] * r[6]) + sum(k[0x158] * r[5])) * 2.0);
	double const v1 = sat(((k[0x15e] * v0) + sum(k[0x15a] * r[103], k[0x15b] * r[104], k[0x15c] * r[105], k[0x15d] * r[106])) * 2.0);
	double const v2 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v3 = sat(((k[0x164] * v2) + sum(k[0x160] * r[107], k[0x161] * r[108], k[0x162] * r[109], k[0x163] * r[110])) * 2.0);
	double const v4 = sat(((k[0x166] * v1) + 0.0) * 2.0);
	double const v5 = sat((k[0x165] * m[55]) + 0.0);
	double const v6 = pos((128.0 * k[0x16e]) + v5);
	double const v7 = pos((128.0 * k[0x16f]) + v5);
	double const v8 = pos((128.0 * k[0x16d]) + v5);
	double const v9 = pos((128.0 * k[0x178]) + sum_on(wrap_sum(((k[0x175] * v8) + wrap_sum(((k[0x174] * pos((128.0 * k[0x16c]) + v5)) + wrap_sum(((k[0x173] * pos((128.0 * k[0x169]) + v5)) + wrap_sum(((k[0x172] * pos((128.0 * k[0x16b]) + v5)) + wrap_sum(((k[0x171] * pos((128.0 * k[0x16a]) + v5)) + wrap_sum(((k[0x170] * pos((128.0 * k[0x168]) + v5)) + 0.0) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 4.0), k[0x176] * v6, k[0x177] * v7));
	double const v10 = sat((k[0x17b] * v9) + sum(k[0x179] * r[115], k[0x17a] * r[116]));
	double const v11 = q15(v10);
	double const v12 = sat((v11 * v4) + 0.0);
	double const v13 = q15(v12);
	double const v14 = sat((v13 * v4) + 0.0);
	double const v15 = sat(((k[0x18b] * v14) + sum(k[0x189] * v4, k[0x18a] * v12)) * 16.0);
	double const v16 = sat(((k[0x18e] * v15) + sum(k[0x18c] * r[111], k[0x18d] * r[112])) * 2.0);
	double const v17 = sat(((k[0x167] * v3) + 0.0) * 2.0);
	double const v18 = sat((v11 * v17) + 0.0);
	double const v19 = q15(v18);
	double const v20 = sat((v19 * v17) + 0.0);
	double const v21 = sat(((k[0x191] * v20) + sum(k[0x18f] * v17, k[0x190] * v18)) * 16.0);
	double const v22 = sat(((k[0x194] * v21) + sum(k[0x192] * r[113], k[0x193] * r[114])) * 2.0);
	double const v23 = mag(((k[0x17d] * v17) + sum(k[0x17c] * v4)) * 16.0);
	double const v24 = cell(sat(v23 + cell(pos((k[0x17e] * m[54]) - v23))));
	r[1] = sat(((k[0x166] * v1) + 0.0) * 2.0);
	r[2] = sat(((k[0x167] * v3) + 0.0) * 2.0);
	r[15] = sat(((k[0x196] * v16) + sum(k[0x195] * r[5])) * 4.0);
	r[16] = sat(((k[0x199] * v22) + sum(k[0x197] * r[6], k[0x198] * v16)) * 4.0);
	r[104] = r[103];
	r[106] = r[105];
	r[108] = r[107];
	r[110] = r[109];
	r[111] = v15;
	r[112] = v16;
	r[113] = v21;
	r[114] = v22;
	r[115] = v9;
	r[116] = v10;
	r[119] = mag(((k[0x17d] * v17) + sum(k[0x17c] * v4)) * 16.0);
	r[120] = v12;
	r[121] = v18;
	r[122] = v14;
	r[123] = v20;
	r[124] = v8;
	r[125] = v6;
	r[126] = v7;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d3;
	m[32] = d4;
	m[33] = d5;
	m[54] = cell(sat(v23 + cell(pos((k[0x17e] * m[54]) - v23))));
	m[55] = cell(sat(v23 + pos((k[0x185] * v24) - wrap_sum(v23 - sum_on(sum(k[0x182] * m[55]) / 32768.0, k[0x183] * m[55])))));
	t[9] = v11;
	t[10] = v13;
	t[11] = v19;
	r[103] = v0;
	r[105] = v1;
	r[107] = v2;
	r[109] = v3;
}

}  // namespace fs1r::effects

// effects/var_m05.cpp - the variation window's program for Phaser1 and Phaser2.
//
//   PH_FILTER     {0x15B, 0x159, 0x15A}         the left channel's low shelf: b0, b1, -a1
//   PH_FILTER_2   {0x15E, 0x15C, 0x15D}         the left channel's high shelf, the same three
//   PH_FILTER_3   {0x162, 0x160, 0x161}         the right channel's low shelf
//   PH_FILTER_4   {0x165, 0x163, 0x164}         the right channel's high shelf
//   PH_FB         {0x166, 0x17A}               the two channels' phase shift
//   PH_STAGES     {0x167, 0x168, ... 0x16E}    the eight section weights, one each
//   PH_DIR        {0x171, 0x172, 0x173}        the diffuse tap; only phaser1() writes these
//   PH_WIDTH1     {0x179}                      the feedback width, phaser1()'s one word
//   PH_WIDTH2     {0x178, 0x18B}               the feedback width, phaser2()'s two words
//   PH_PAN        {0x19C, 0x19D, 0x19E, 0x19F} the wet and dry pairs, left then right
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, each b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and one of
// the output, the low shelf on a step that doubles its result and the high shelf on one that quadruples
// it.
//
// What the two types differ by is which words they write. phaser1() puts the phase shift 258 a step into
// both feedback words, its width word at 0x179 alone, and one half at one of the three diffuse words;
// phaser2() splits the shift 258 and 242 between the two feedback words, its width word at 0x178 and
// 0x18B, and leaves the diffuse words at the ROM's. The ROM's own word at 0x172 is 0x4000, so under
// Phaser2, which has no Diffuse parameter, r[119] is the right cascade's first section and the left
// output's wet signal is that; under Phaser1 with Diffuse off it is zero and with it on it is one of
// r[121], r[114] or r[117]. PH_PAN's four words are the same list for both, except that phaser1()
// negates the last when Diffuse is on.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d write
// them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005).
// r[103] to r[105] the left channel's EQ: input, low shelf, high shelf; r[106] to r[108] the right
// channel's. r[109] the left cascade's first section, m[55] its second, r[110] to r[112] its third to
// fifth; r[113] the right cascade's input and r[114] to r[118] its five sections. r[119] the diffuse
// tap, r[120] and r[121] the two stage sums, r[122] the swept gain's scratch, which all six gain steps
// write in turn. The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[8]
// and m[9] the window's two LFOs, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the
// six published delay RAM reads, m[54] the cross-channel feedback, m[55] the left cascade's second
// section and m[56] the ROM's zero. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, read at the
// ring pointer with no index; this program writes none of them, so the lines behind these taps are
// filled elsewhere. Each capture takes the read five steps before it (captured()'s lower bound), so
// m[28] carries the slot 0x56 word, m[29] the slot 0x57 word, m[30] the slot 0x58 word, and m[31],
// m[32] and m[33] the slots 0x63, 0x64 and 0x65 words.
//
// The two t registers: t[10] the left cascade's gain, written at 0x166, 0x16f and 0x175 and read by its
// ten section steps and by the two gain steps; t[11] the right cascade's, written at 0x17a, 0x17d and
// 0x189 and read the same way. A t write saturates the accumulator and divides it by 128, so the left
// gain is the right gain less 1 wherever the two phase shift words and the two LFO depths agree, which
// is what phaser1() sets up and what makes the two sweeps run against each other; phaser2() splits the
// two apart, 258 and 242 a step, and gives the second LFO its own depth and phase. The sections
// themselves are first order and each one is its own register against the one before it, one pass back,
// which is the shape of the old model's `y = g * x + ap[i]; ap[i] = x - g * y` in git show
// 29f589c^:src/fs1r/chips/vop3_effects.h, written here with the signs and the scaling the words give it.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m05(State& s)
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
	double const v0 = sat(((k[0x158] * r[5]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x15b] * v0) + sum(k[0x159] * r[103], k[0x15a] * r[104])) * 2.0);
	double const v2 = sat(((k[0x15e] * v1) + sum(k[0x15c] * r[104], k[0x15d] * r[105])) * 4.0);
	double const v3 = sat(((k[0x16a] * r[112]) + sum(k[0x167] * m[55], k[0x168] * r[110], k[0x169] * r[111])) * 2.0);
	double const v4 = sat(((k[0x16e] * r[118]) + sum(k[0x16b] * r[115], k[0x16c] * r[116], k[0x16d] * r[117])) * 2.0);
	double const v5 = sat(((k[0x173] * r[117]) + sum(k[0x170] * v3, k[0x171] * v4, k[0x172] * r[114])) * 2.0);
	double const v6 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v7 = sat(((k[0x162] * v6) + sum(k[0x160] * r[106], k[0x161] * r[107])) * 2.0);
	double const v8 = sat(((k[0x165] * v7) + sum(k[0x163] * r[107], k[0x164] * r[108])) * 4.0);
	double const v9 = sat((128.0 * k[0x166]) + m[8]);
	double const v10 = sat((q15(v9) * v9) + 0.0);
	double const v11 = q15(sat((q15(v10) * v10) - sum(128.0 * k[0x174])));
	double const v12 = cell(sat((k[0x179] * v4) + sum_on(wrap_sum(((k[0x177] * v8) + sum(k[0x176] * v2)) * 2.0), k[0x178] * v3)));
	double const v13 = sat((v11 * v12) - wrap_sum((v11 * r[109]) - m[54]));
	double const v14 = cell(sat((v11 * v13) - wrap_sum((v11 * m[55]) - r[109])));
	double const v15 = sat((v11 * v14) - wrap_sum((v11 * r[110]) - m[55]));
	double const v16 = sat((v11 * v15) - wrap_sum((v11 * r[111]) - sum(r[110])));
	double const v17 = cell(sat((128.0 * k[0x180]) + 0.0));
	double const v18 = sat((128.0 * k[0x17a]) + m[9]);
	double const v19 = sat((q15(v18) * v18) + 0.0);
	double const v20 = q15(sat((q15(v19) * v19) - v17));
	double const v21 = sat((k[0x18c] * v3) + sum_on(wrap_sum(((k[0x18a] * v8) + 0.0) * 2.0), k[0x18b] * v4));
	double const v22 = sat((v20 * v21) - wrap_sum((v20 * r[114]) - sum(r[113])));
	double const v23 = sat((v20 * v22) - wrap_sum((v20 * r[115]) - sum(r[114])));
	double const v24 = sat((v20 * v23) - wrap_sum((v20 * r[116]) - sum(r[115])));
	double const v25 = sat((v20 * v24) - wrap_sum((v20 * r[117]) - sum(r[116])));
	r[15] = sat(((k[0x19d] * v5) + sum(k[0x19c] * v2)) * 4.0);
	r[16] = sat(((k[0x19f] * v4) + sum(k[0x19e] * v8)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v6;
	r[107] = v7;
	r[108] = v8;
	r[109] = sat((v11 * v12) - wrap_sum((v11 * r[109]) - m[54]));
	r[112] = sat((v11 * v16) - wrap_sum((v11 * r[112]) - sum(r[111])));
	r[118] = sat((v20 * v25) - wrap_sum((v20 * r[118]) - sum(r[117])));
	r[119] = v5;
	r[120] = v3;
	r[121] = v4;
	r[122] = v19;
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
	m[54] = v12;
	m[56] = v17;
	t[10] = v11;
	t[11] = v20;
	r[111] = sat((v11 * v15) - wrap_sum((v11 * r[111]) - sum(r[110])));
	r[117] = sat((v20 * v24) - wrap_sum((v20 * r[117]) - sum(r[116])));
	r[110] = sat((v11 * v14) - wrap_sum((v11 * r[110]) - m[55]));
	r[116] = sat((v20 * v23) - wrap_sum((v20 * r[116]) - sum(r[115])));
	m[55] = v14;
	r[115] = sat((v20 * v22) - wrap_sum((v20 * r[115]) - sum(r[114])));
	r[114] = sat((v20 * v21) - wrap_sum((v20 * r[114]) - sum(r[113])));
	r[113] = sat((k[0x18c] * v3) + sum_on(wrap_sum(((k[0x18a] * v8) + 0.0) * 2.0), k[0x18b] * v4));
}

}  // namespace fs1r::effects

// effects/var_m04.cpp - the variation window's program for Auto Wah and Touch Wah.
//
//   WAH_FILTER      {0x15B, 0x159, 0x15A}       the left channel's low shelf: b0, b1, -a1
//   WAH_FILTER_2    {0x15E, 0x15C, 0x15D}       the left channel's high shelf, the same three
//   WAH_FILTER_3    {0x162, 0x160, 0x161}       the right channel's low shelf
//   WAH_FILTER_4    {0x165, 0x163, 0x164}       the right channel's high shelf
//   WAH_CUTOFF      {0x166}                     the cutoff
//   WAH_DEPTH       {0x167}                     the LFO depth on it: autoWah() writes it, touchWah()
//                                                does not, so the sweep is the Auto Wah's
//   WAH_SENSE       {0x170, 0x175}              the resonance: wahSense() writes one word into both
//   WAH_ATTACK      {0x171}                     the Touch Wah's sensitivity, autoWah() leaves it
//   WAH_HPF         {0x16D, 0x16B, 0x16C}       touchWah()'s input highpass, its words only
//   WAH_LPF         {0x181, 0x17F, 0x180}       touchWah()'s input lowpass, its words only
//   WAH_PAN         {0x185, 0x184, 0x187, 0x186} the dry and wet, interleaved: dry, wet, dry, wet
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, so each is b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and
// one of the output. fx_design.cpp's designFilter scales its words by 2048 * (16 >> scale), and the step
// carries 2^scale as its result route, which is why the low shelf's b0 sits on a step that doubles and the
// high shelf's on one that quadruples: scale 1 against highScale's 2.
//
//   b1   the word on the section's own input, with no adder, which opens the sum
//   -a1  the word on the section's own output a pass back
//   b0   the word on the input again, closing the sum into the output with the section's route
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x1f2 write
// the first, 0x08d the second) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005).
// r[103] to r[105] the left channel's EQ: input, low shelf, high shelf; r[106] to r[108] the right
// channel's. r[109] and r[110] the highpass, r[112] and r[113] the lowpass, r[114] and r[115] the
// resonator's two states, r[119] the cutoff and then the smoother's state, r[111] the rectified level's
// partner. The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[8] the
// window's first LFO, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six published
// delay RAM reads, m[54] and m[55] the resonator's two feedback accumulators, m[56] and m[57] its two
// inputs and m[58] the sensitivity level. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, read at
// the ring pointer with no index; this program writes none of them, so the lines behind these taps are
// filled elsewhere. Each capture takes the read five steps before it (captured()'s lower bound), so
// m[28] carries the slot 0x56 word, m[29] the slot 0x57 word, m[30] the slot 0x58 word, and m[31], m[32]
// and m[33] the slots 0x63, 0x64 and 0x65 words.
//
// The one t register the program uses is t[9], written at 0x168, 0x16e and 0x173 and read at 0x16e, 0x172,
// 0x178, 0x179, 0x182 and 0x183: the cutoff, then the smoother's gain, then the resonator's gain.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m04(State& s)
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
	double const v3 = cell(sat((k[0x170] * r[114]) - wrap_sum((k[0x16f] * m[54]) - v2)));
	double const v4 = sat((k[0x168] * r[113]) + sum(128.0 * k[0x166], k[0x167] * m[8]));
	double const v5 = (q15(v4) * v4) + 0.0;
	double const v6 = wrap(v5);
	double const v7 = q15(sat((128.0 * k[0x173]) + sum(q15(sum(q15(v4) * v4)) * v6)));
	double const v8 = sat((v7 * v3) + r[114]);
	double const v9 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v10 = sat(((k[0x162] * v9) + sum(k[0x160] * r[106], k[0x161] * r[107])) * 2.0);
	double const v11 = sat(((k[0x165] * v10) + sum(k[0x163] * r[107], k[0x164] * r[108])) * 4.0);
	double const v12 = cell(sat((k[0x175] * r[115]) - wrap_sum((k[0x174] * m[55]) - v11)));
	double const v13 = sat((v7 * v12) + r[115]);
	double const v14 = sat((k[0x16a] * v11) + sum(k[0x169] * v2));
	double const v15 = sat(((k[0x16d] * v14) + sum(k[0x16b] * r[109], k[0x16c] * r[110])) * 2.0);
	double const v16 = cell(mag(((k[0x171] * v15) + 0.0) * 16.0));
	double const v17 = sat((k[0x177] * v16) + pos((k[0x176] * r[111]) - v16));
	double const v18 = sat((k[0x17e] * v16) + pos((k[0x17d] * v17) - wrap_sum((k[0x17c] * v16) - sum_on(sum(k[0x17a] * r[112]) / 32768.0, k[0x17b] * r[112]))));
	r[15] = sat(((k[0x185] * v2) + sum(k[0x184] * v8)) * 4.0);
	r[16] = sat(((k[0x187] * v11) + sum(k[0x186] * v13)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v9;
	r[107] = v10;
	r[108] = v11;
	r[110] = sat(((k[0x16d] * v14) + sum(k[0x16b] * r[109], k[0x16c] * r[110])) * 2.0);
	r[111] = sat((k[0x177] * v16) + pos((k[0x176] * r[111]) - v16));
	r[113] = sat((k[0x181] * v18) + sum(k[0x17f] * r[112], k[0x180] * r[113]));
	r[114] = v8;
	r[115] = v13;
	r[119] = v6;
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
	m[54] = cell(sat((v7 * v8) + m[54]));
	m[55] = cell(sat((v7 * v13) + m[55]));
	m[56] = v3;
	m[57] = v12;
	m[58] = v16;
	t[9] = v7;
	r[109] = sat((k[0x16a] * v11) + sum(k[0x169] * v2));
	r[112] = sat((k[0x17e] * v16) + pos((k[0x17d] * v17) - wrap_sum((k[0x17c] * v16) - sum_on(sum(k[0x17a] * r[112]) / 32768.0, k[0x17b] * r[112]))));
}

}  // namespace fs1r::effects

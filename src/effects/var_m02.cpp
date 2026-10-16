// effects/var_m02.cpp - the variation window's program for Distortion, Overdrive and Amp Sim:
// its 72 steps.
//
// The host writes this program's coefficients through the word lists in fx_drive.cpp's DRIVES[0] and
// rom_tables.cpp's var:: namespace, and those lists name most of the steps below: DIST_DRIVE
// {0x158, 0x15b, 0x15f, 0x180}, DIST_INPUT {0x15e, 0x15c, 0x15d}, DIST_OUTPUT {0x198, 0x196, 0x197},
// DIST_LOW_SHELF {0x18f, 0x18c, 0x18b, 0x18d, 0x18e}, DIST_PEAK {0x18a, 0x187, 0x186, 0x189, 0x188},
// DIST_LPF2 {0x185, 0x182, 0x181, 0x184, 0x183}, DIST_BAND {0x15f, 0x162, 0x166, 0x16c, 0x170, 0x173,
// 0x174, 0x175, 0x176, 0x177, 0x178, 0x179} and DIST_PAN {0x191, 0x192, 0x193, 0x194}. What the steps
// compute, in the groups the comments open:
//
//   0x158-0x15e  the drive and the input highpass. 0x158 scales the feedback register r[108] by the
//                drive gain the host derives from the Drive parameter, 0x159 and 0x15a add the two
//                serial inputs into r[103], and 0x15b is a pole on r[119] the same drive drives.
//                0x15c to 0x15e are the input highpass's three words (DIST_INPUT), a biquad whose two
//                input cells m[2] and m[6] this window publishes for it.
//   0x15f-0x166  the rectifier. 0x15f is the second pole the drive drives, 0x161 takes the running
//                value's absolute value into m[56], which is the level everything below works against,
//                0x162 and 0x166 are poles, 0x164 subtracts that level from the signal with negatives
//                clamped away, and 0x165 is the level's one-pole follower into r[105].
//   0x167-0x16e  the same pair again for the second half of the shaper: 0x168 to 0x16b mirror the
//                rectifier into r[106], 0x16c is a pole and 0x16e scales r[106] into r[124].
//   0x16f-0x178  the two class-1 loads and the band sum. 0x16f loads the running value into r[125] and
//                0x171 adds its own constant to r[124], both class-1 steps (s24: a class-1 load reads
//                back as v / 256, which is why the 128 * k form appears). 0x170 and 0x173 are poles, and
//                0x174 to 0x178 sum the five poles r[119] to r[123], the twelve band words DIST_BAND
//                ending here.
//   0x179-0x18f  the tone stack and the ladder. 0x180 is the gain the host derives from the drive and
//                the output level. 0x181 to 0x185 are the second lowpass (DIST_LPF2), 0x186 to 0x18a the
//                mid peak (DIST_PEAK) and 0x18b to 0x18f the low shelf (DIST_LOW_SHELF): three filters of
//                five words each, every one of them on the last one's output. The four rcopy steps 0x183,
//                0x188, 0x18d and 0x18e each write the previous step's x operand, and the first three are
//                what hold r[110], r[112] and r[114], the one-pass delays those two-tap structures close
//                on. r[116] is a fourth, on its own, holding the low shelf's own output a pass back.
//   0x190-0x19f  the dry and wet. 0x190 is the one step here the host parameterises for neither this
//                family nor the others: it takes r[118] into the t file as a 1.15 word, and 0x195 reads
//                t[9] back as the multiplier that scales r[104] into r[107]. 0x191 to 0x194 are the pan
//                and dry/wet words (DIST_PAN) and the two outputs r[15] and r[16]. 0x196 to 0x198 are the
//                output lowpass (DIST_OUTPUT), 0x199 and 0x19a are the two class-3 steps that publish the
//                inputs into m[54] and m[55], and 0x19c publishes the last read.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs the base program
// writes, r[15] and r[16] the two outputs, which the base program reads as the stereo pair after r[13]
// and r[14], r[103] the input sum and r[104] the input highpass's output, r[105] and r[106] the
// rectifier's two followers, r[107] the level term, r[108] the output filter's tail and the register the
// drive gain reads, r[109], r[111], r[113] and r[115] the ladder's four stage outputs with r[110], r[112],
// r[114] and r[116] the four one-pass delays between them, r[117] the biased signal and r[118] its
// one-pole, and r[119] to r[123] the five poles the band sum adds. The cells: m[2] and m[6] the serial
// inputs this window publishes, m[11] and m[12] the two LFO values, m[28] to m[30] and m[31] to m[33] the
// six published reads, m[54] and m[55] the two inputs echoed into cells, and m[56] the level the
// rectifier works against. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring
// pointer with no index; this program writes none of them, so the lines behind these taps are filled
// elsewhere.
//
// The one t register the program uses is t[9], written at 0x190 and read at 0x195.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m02(State& s)
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
	double const v0 = sat(((k[0x15f] * sat(((k[0x15b] * sat(((k[0x158] * r[108]) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v1 = sat(((k[0x162] * v0) + 0.0) * 16.0);
	double const v2 = sat(((k[0x166] * v1) + 0.0) * 16.0);
	double const v3 = sat(((k[0x16c] * v2) + 0.0) * 16.0);
	double const v4 = sat(((k[0x170] * v3) + 0.0) * 16.0);
	double const v5 = sat((k[0x179] * sat(((k[0x173] * v4) + 0.0) * 16.0)) + sum(k[0x174] * v0, k[0x175] * v1, k[0x176] * v2, k[0x177] * v3, k[0x178] * v4));
	double const v6 = sat((k[0x180] * v5) + 0.0);
	double const v7 = sat(((k[0x185] * v6) + sum(k[0x181] * r[110], k[0x182] * r[109], k[0x183] * r[112], k[0x184] * r[111])) * 2.0);
	double const v8 = sat(((k[0x18a] * v7) + sum(k[0x186] * r[112], k[0x187] * r[111], k[0x188] * r[114], k[0x189] * r[113])) * 2.0);
	double const v9 = sat(((k[0x18f] * v8) + sum(k[0x18b] * r[114], k[0x18c] * r[113], k[0x18d] * r[115], k[0x18e] * r[116])) * 16.0);
	double const v10 = sat(((k[0x15a] * r[6]) + sum(k[0x159] * r[5])) * 2.0);
	double const v11 = sat(((k[0x15e] * v10) + sum(k[0x15c] * r[103], k[0x15d] * r[104])) * 2.0);
	double const v12 = cell(mag(((k[0x161] * v11) + 0.0) * 16.0));
	double const v13 = sat((k[0x165] * v12) + pos((k[0x164] * r[105]) - v12));
	double const v14 = sat((k[0x16b] * v12) + pos((k[0x16a] * v13) - wrap_sum((k[0x169] * v12) - sum_on(sum(k[0x167] * r[106]) / 32768.0, k[0x168] * r[106]))));
	double const v15 = sat((k[0x16e] * v14) + 0.0);
	double const v16 = sat((128.0 * k[0x16f]) + v15);
	double const v17 = sat((128.0 * k[0x171]) + v15);
	double const v18 = pos(((128.0 * k[0x17c]) + sat(((k[0x17b] * v17) + sum(k[0x17a] * v16)) * 16.0)) * 16.0);
	double const v19 = sat((k[0x17f] * v18) + sum(k[0x17d] * r[117], k[0x17e] * r[118]));
	double const v20 = q15(sat(((k[0x190] * v19) + 0.0) * 2.0));
	double const v21 = sat((v20 * v11) + 0.0);
	r[15] = sat(((k[0x192] * v9) + sum(k[0x191] * m[54])) * 4.0);
	r[16] = sat(((k[0x194] * v9) + sum(k[0x193] * m[55])) * 4.0);
	r[104] = sat(((k[0x15e] * v10) + sum(k[0x15c] * r[103], k[0x15d] * r[104])) * 2.0);
	r[105] = sat((k[0x165] * v12) + pos((k[0x164] * r[105]) - v12));
	r[106] = sat((k[0x16b] * v12) + pos((k[0x16a] * v13) - wrap_sum((k[0x169] * v12) - sum_on(sum(k[0x167] * r[106]) / 32768.0, k[0x168] * r[106]))));
	r[108] = sat(((k[0x198] * v21) + sum(k[0x196] * r[107], k[0x197] * r[108])) * 2.0);
	r[110] = r[109];
	r[112] = r[111];
	r[114] = r[113];
	r[116] = r[115];
	r[117] = v18;
	r[118] = v19;
	r[119] = v5;
	r[120] = v1;
	r[121] = v2;
	r[122] = v3;
	r[123] = v4;
	r[124] = v17;
	r[125] = v16;
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
	m[54] = cell(sum(r[5]));
	m[55] = cell(sum(r[6]));
	m[56] = v12;
	t[9] = v20;
	r[103] = sat(((k[0x15a] * r[6]) + sum(k[0x159] * r[5])) * 2.0);
	r[107] = sat((v20 * v11) + 0.0);
	r[109] = v6;
	r[111] = v7;
	r[113] = v8;
	r[115] = v9;
}

}  // namespace fs1r::effects

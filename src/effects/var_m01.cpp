// effects/var_m01.cpp - the variation window's program for Rotary SP, Tremolo and Auto
// Pan: its 72 steps.
//
//   0x158-0x15a  the two serial inputs this window publishes, m[2] and m[6], and the three registers the
//                rest of the block works from, r[123], r[125] and r[1], each taken off the shared cell
//                m[8] and doubled.
//   0x15b-0x166  the three decay chains those three feed. Each folds a published slot's read through
//                |r|, then a saturating doubling that writes t[9], t[11] or t[13], then two multiplies
//                into r[124], r[126] and r[2]. m[28], m[29] and m[30] carry the three reads on.
//   0x167-0x16e  the fractional delay positions: t[15] and t[7] off the cell m[9], the first with an
//                index load of its own, and the two modulated positions r[123] and r[1], each clamped at
//                zero, which the taps below read at.
//   0x16f-0x17e  the left channel. The first tap (the slot 0x5b, read past the ring pointer) and the LFO
//                publish m[11] with t[9]; the second tap's capture m[62] and t[10] with its index load;
//                the input path r[5] to r[103], r[104] and r[105] against the taps 0x5c and 0x5d, which
//                are the two- and four-sample combs the delay line is written through; the captures m[56]
//                and m[57] with t[14] and t[12]; and the right position r[125] clamped at zero.
//   0x17f-0x185  the right channel, the mirror of the left one: the input path r[6] to r[106], r[107] and
//                r[108], the taps 0x5f and 0x60, the captures m[60], m[61] and m[58], and the tap read
//                0x61.
//   0x186-0x18e  the four interpolations, each written as its fraction times the second tap less the
//                fraction times the first, so m[56] and m[57] on t[10] make r[119], m[60] and m[61] on
//                t[14] make r[121] and m[62] and m[63] on t[7] make r[122]; then the three of them scaled
//                by (1 + t[9]), (1 + t[13]) and (1 + t[15]), which is where this block differs from the
//                chorus program's and what makes the amplitude and pitch move together.
//   0x18f-0x197  the second interpolation off the pair m[58] and m[59] on t[12] into r[120], its scaling by
//                (1 + t[11]), and the two output sums that add the three scaled values to the running
//                one.
//   0x198-0x19f  the two outputs r[15] and r[16], the captures m[32] and m[33] of the reads at 0x64 and
//                0x65, the two delay line writes into the slots 0x66 and 0x67, and the idle step 0x19e
//                between them.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs, r[103] and r[106]
// the scaled inputs, r[104], r[105], r[107] and r[108] the input accumulators, r[15] and r[16] the two
// outputs, which the base program reads at 0x018 and 0x03b as the later stereo pair after r[13] and
// r[14], r[119] to r[122] the four interpolated tap sums, r[123], r[125] and r[1] the three followers
// that end as two delay positions, r[124], r[126] and r[2] the products between them. The cells: m[8]
// the shared cell, m[2] and m[6] the serial inputs this window publishes, m[9] the cell the t[15] and
// t[7] positions scale off, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six
// published reads and m[56] to m[63] the eight taps. The delay slots: 0x66 and 0x67 the two delay lines,
// 0x5a to 0x61 the eight taps (rom::var::MOD_SLOTS), 0x56 to 0x58 and 0x63 to 0x65 the six reads it hands
// on.
//
// The t registers: t[7], t[10], t[12] and t[14] the four fractional tap positions, t[9], t[11], t[13] and
// t[15] the decay and scaling coefficients. Every one of the eight is written from a running value the
// step itself produced, so they move with the block and not with a parameter directly.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m01(State& s)
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
	double const v0 = sat((k[0x168] * m[9]) + 0.0);
	double const v1 = index(v0);
	double const d3 = s.ram[s.at(0x5a, int(v1))];
	double const d4 = s.ram[s.at(0x5b, 1 + int(v1))];
	double const v2 = sat(((128.0 * k[0x15c]) + mag(wrap(((128.0 * k[0x158]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v3 = q15(v2);
	double const v4 = sat((v3 * sat((v3 * v2) + 0.0)) + 0.0);
	double const v5 = pos((128.0 * k[0x16b]) + sum_on(sat(((k[0x169] * v2) + 0.0) * 16.0), k[0x16a] * v4));
	double const v6 = sat((k[0x170] * v5) + 0.0);
	double const v7 = index(v6);
	double const d5 = s.ram[s.at(0x5c, int(v7))];
	double const d6 = s.ram[s.at(0x5d, 1 + int(v7))];
	double const v8 = sat(((128.0 * k[0x160]) + mag(wrap(((128.0 * k[0x15a]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v9 = q15(v8);
	double const v10 = sat((v9 * sat((v9 * v8) + 0.0)) + 0.0);
	double const v11 = pos((128.0 * k[0x16e]) + sum_on(sat(((k[0x16c] * v8) + 0.0) * 16.0), k[0x16d] * v10));
	double const v12 = sat((k[0x178] * v11) + 0.0);
	double const v13 = index(v12);
	double const d7 = s.ram[s.at(0x5e, int(v13))];
	double const d8 = s.ram[s.at(0x5f, 1 + int(v13))];
	double const v14 = sat(((128.0 * k[0x15e]) + mag(wrap(((128.0 * k[0x159]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v15 = q15(v14);
	double const v16 = sat((v15 * sat((v15 * v14) + 0.0)) + 0.0);
	double const v17 = pos((128.0 * k[0x17b]) + sum_on(sat(((k[0x179] * v14) + 0.0) * 16.0), k[0x17a] * v16));
	double const v18 = sat((k[0x17e] * v17) + 0.0);
	double const v19 = index(v18);
	double const d9 = s.ram[s.at(0x60, int(v19))];
	double const d10 = s.ram[s.at(0x61, 1 + int(v19))];
	double const d11 = s.ram[s.at(0x63)];
	double const d12 = s.ram[s.at(0x64)];
	double const d13 = s.ram[s.at(0x65)];
	double const v20 = sat(((k[0x171] * r[5]) + 0.0) * 2.0);
	double const v21 = sat(((k[0x174] * v20) + sum(k[0x172] * r[104], k[0x173] * r[103])) * 2.0);
	double const v22 = sat(((k[0x177] * v21) + sum(k[0x175] * r[105], k[0x176] * r[104])) * 4.0);
	double const v23 = sat(((k[0x17f] * r[6]) + 0.0) * 2.0);
	double const v24 = sat(((k[0x182] * v23) + sum(k[0x180] * r[107], k[0x181] * r[106])) * 2.0);
	double const v25 = sat(((k[0x185] * v24) + sum(k[0x183] * r[108], k[0x184] * r[107])) * 4.0);
	s.ram[s.at(0x66)] = clip32(sat((k[0x19b] * v25) + sum(k[0x19a] * v22)));
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v25) + 0.0));
	double const v26 = frac(v6);
	double const v27 = sat((v26 * d6) - wrap_sum((v26 * d5) - d5));
	double const v28 = q15(sat((k[0x16f] * v5) + 0.0));
	double const v29 = frac(v12);
	double const v30 = sat((v29 * d8) - wrap_sum((v29 * d7) - d7));
	double const v31 = q15(sat((k[0x17c] * v11) + 0.0));
	double const v32 = sat((v31 * v30) + v30);
	double const v33 = frac(v0);
	double const v34 = sat((v33 * d4) - wrap_sum((v33 * d3) - d3));
	double const v35 = q15(sat((k[0x167] * m[9]) + 0.0));
	double const v36 = sat((v35 * v34) + v34);
	double const v37 = sat((k[0x193] * v36) + sum(k[0x191] * sat((v28 * v27) + v27), k[0x192] * v32));
	double const v38 = frac(v18);
	double const v39 = sat((v38 * d10) - wrap_sum((v38 * d9) - d9));
	double const v40 = q15(sat((k[0x17d] * v14) + 0.0));
	double const v41 = sat((k[0x197] * sat((v40 * v39) + v39)) + sum(k[0x195] * v32, k[0x196] * v36));
	r[1] = v11;
	r[2] = v10;
	r[15] = sat(((k[0x199] * v37) + sum(k[0x198] * v22)) * 4.0);
	r[16] = sat(((k[0x19d] * v41) + sum(k[0x19c] * v25)) * 4.0);
	r[103] = v20;
	r[104] = v21;
	r[105] = v22;
	r[106] = v23;
	r[107] = v24;
	r[108] = v25;
	r[119] = v37;
	r[120] = v41;
	r[121] = v32;
	r[122] = v36;
	r[123] = v5;
	r[124] = v4;
	r[125] = v17;
	r[126] = v16;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d11;
	m[32] = d12;
	m[33] = d13;
	m[56] = d5;
	m[57] = d6;
	m[58] = d9;
	m[59] = d10;
	m[60] = d7;
	m[61] = d8;
	m[62] = d3;
	m[63] = d4;
	t[7] = v33;
	t[9] = v28;
	t[10] = v26;
	t[11] = v40;
	t[12] = v38;
	t[13] = v31;
	t[14] = v29;
	t[15] = v35;
}

}  // namespace fs1r::effects

// effects/ins_m01.cpp - the insertion window's program for type msb 01.
//
// rom::INSERTION_PROGRAMS[1], the program the insertion window holds under the five types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 1: ins-09 Rotary SP, ins-10 2-Way Rotary,
// ins-11 Tremolo, ins-12 Auto Pan and ins-13 Ambience, programmed by EffectHost::rotarySpeaker(),
// twoWayRotary(), tremolo(), autoPan() and ambience(). Like ins-m00 it is one of the fifteen
// programs the window shares: nothing here knows which type is running, because the program words
// are fixed and only the constants and the delay offsets move, which fx_modulation.cpp and
// fx_delay.cpp write whenever a parameter does. The code reads them where they live on the pass, as
// step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 96 steps compute, in the groups the comments open each of. The host names every group it
// programs, in EffectHost::rotarySpeaker(), twoWayRotary(), tremolo(), autoPan() and ambience(), and in
// the tables they call:
//   0x1a0-0x1b3  the window's LFO cell through a rectifier into four accumulators, r[121], r[124],
//                r[122] and r[123], each of which names a delay length in samples: a register's value
//                times 256 is the delay, its integer part is the index the taps read at (queued at
//                0x1b4, 0x1bc, 0x1c4 and 0x1cc, in effect three steps later) and its fraction the
//                multiplier that interpolates between a line's two taps. Four envelopes, r[1], r[4],
//                r[3] and r[2], smooth each accumulator, and the two rectifiers at 0x1a8 and 0x1b1 feed
//                two of them back. ins::BAL_STEP_A, BAL_STEP_B, BAL_STEP_C, ins::MODE_STEPS_A and
//                MODE_STEPS_B, the tremolo's phase, the auto pan's direction and the 2-way rotary's mic
//                angle, are steps of this block. ins::LFO_DIR_STEPS, ins::PC_FB and ins::PC_MIX, which
//                name 0x1a0, 0x1a8, 0x1b0, 0x1b7, 0x1a7, 0x1aa, 0x1af, 0x1b1 and 0x1ba, are the
//                ensemble detune's and the pitch change's, so no handler that loads this program writes
//                them and under these five types they hold the words the preset left.
//   0x1b4-0x1ce  the four delay lengths. ins::ROT_LEVEL[1], [7], [5] and [3] are the depth words that
//                scale the accumulator into samples and queue its index; ins::ROT_LEVEL[0], [4], [2] and
//                [6] are the negative ones, which reach t[9], t[13], t[11] and t[15] as multipliers.
//                Eight reads of the delay RAM happen on the way past, the tap pairs at one word apart
//                and at the same index, one pair per delay length.
//   0x1ca-0x1ee  the four interpolations, each two cells taken five steps after the read that filled
//                them: m[56] and m[57] to r[119] on t[10], m[60] and m[61] to r[121] on t[14], m[62] and
//                m[63] to r[122] on t[3], m[58] and m[59] to r[120] on t[12]. The last two registers
//                reuse the accumulators' own, which have finished with them by then.
//   0x1cf-0x1e8  the two channels' tone shaping, three bands each: r[67] to r[68] through
//                ins::LPF_ROT, then to r[70] through ins::EQ_ROT, then to r[72] through ins::HPF_ROT,
//                and ins::LPF_ROT_C, ins::EQ_ROT_C and ins::HPF_ROT_C for r[73], r[74], r[76] and r[78].
//                Each shelf's own state a step back, in r[69], r[71], r[75] and r[77, is what the a-terms
//                ride on.
//   0x1ef-0x1f8  the four amplitude gains, and then the two wet sums: ins::ROT_PAN's four words weigh
//                the taps, which is the 2-way rotary's low/high balance and the ambience's phase.
//   0x1f9-0x1ff  the dry mix, ins::TREM_WORDS and ins::ROT_WORDS, the two outputs r[17] and r[18],
//                which the base program reads back, and the two delay lines' writes: the slot 0x7e takes
//                the tremolo mode's mix at 0x1fb and the slot 0x7f its own value at 0x1ff.
//
// The cells the program fills are its two serial inputs and its four LFOs, m[3] and m[7] from the banks
// at page 13 and m[4] from page 14, and m[13], m[14] and m[15] from the LFOs of pages 13, 14 and 15,
// which no step of this program reads; and the eight reads it takes up for itself, m[56] to m[63] from
// the eight swept taps, each five steps after the read that fills it, which is captured()'s window. The
// LFO cell this program sweeps, m[12], it only reads: the step that fills it is the variation window's
// at 0x18f, which takes lfo[12], this window's first LFO, out of the same hardware.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67], r[68], r[70]
// and r[72] the left channel's three bands and r[73], r[74], r[76] and r[78] the right's, with r[69],
// r[71], r[75] and r[77] their a-term copies a step back; r[1], r[4], r[3] and r[2] the four envelopes;
// r[121], r[124], r[122] and r[123] the four accumulators, each of which names one delay length and one
// tap pair; by 0x1ec and 0x1ee r[121] and r[122] have been reused for the third and the second tap, with
// r[119] holding the first and r[120] the fourth; r[17] and r[18] the two outputs. The slots: 0x6d and 0x6e, 0x6f and 0x70, 0x71 and
// 0x72 and 0x73 and 0x74 the four tap pairs, of which ins::TREM_SLOTS holds the first as its second and
// third, the third as its fourth and fifth and the fourth as its seventh and eighth, and the second
// pair not at all; and 0x7e and 0x7f the two lines' own writes, its zeroth and sixth. Of the eight
// offsets ins::TREM_SLOTS names, these five types are the only ones that move any: ambience()'s
// DELAY_TIME and tremolo()'s MODE, every other handler here writing coefficients and LFO words only.
//
// The old model in vop3_effects.h has a case for each of these five types, and each is a handful of
// lines over an FxLfo, a delay line and a pair of gains: no filter of three bands, no four taps and no
// accumulator. It is no reference for any number here, and where the two differ, the program above is
// what runs.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m01(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat(((k[0x1a4] * sat(((128.0 * k[0x1a1]) + mag((m[12] + 0.0) * 2.0)) * 2.0)) + 0.0) * 2.0);
	double const v1 = q15(v0);
	double const v2 = sat((v1 * sat((v1 * v0) + 0.0)) + 0.0);
	double const v3 = pos((128.0 * k[0x1af]) + sum_on(sat(((k[0x1ad] * v0) + 0.0) * 16.0), k[0x1ae] * v2));
	double const v4 = sat((k[0x1b4] * v3) + 0.0);
	double const v5 = index(v4);
	double const d0 = s.ram[s.at(0x6d, int(v5))];
	double const d1 = s.ram[s.at(0x6e, 1 + int(v5))];
	double const v6 = sat(((k[0x1ac] * sat(((128.0 * k[0x1a9]) + mag(wrap(((128.0 * k[0x1a3]) + wrap_sum(((k[0x1a2] * m[12]) + 0.0) * 16.0)) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v7 = q15(v6);
	double const v8 = sat((v7 * sat((v7 * v6) + 0.0)) + 0.0);
	double const v9 = pos((128.0 * k[0x1b8]) + sum_on(sat(((k[0x1b6] * v6) + 0.0) * 16.0), k[0x1b7] * v8));
	double const v10 = sat((k[0x1bc] * v9) + 0.0);
	double const v11 = index(v10);
	double const d2 = s.ram[s.at(0x6f, int(v11))];
	double const d3 = s.ram[s.at(0x70, 1 + int(v11))];
	double const v12 = sat(((k[0x1b5] * sat(((128.0 * k[0x1b2]) + mag(wrap(((128.0 * k[0x1ab]) + m[12]) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v13 = q15(v12);
	double const v14 = sat((v13 * sat((v13 * v12) + 0.0)) + 0.0);
	double const v15 = pos((128.0 * k[0x1c1]) + sum_on(sat(((k[0x1bf] * v12) + 0.0) * 16.0), k[0x1c0] * v14));
	double const v16 = sat((k[0x1c4] * v15) + 0.0);
	double const v17 = index(v16);
	double const d4 = s.ram[s.at(0x71, int(v17))];
	double const d5 = s.ram[s.at(0x72, 1 + int(v17))];
	double const v18 = sat(((k[0x1be] * sat(((128.0 * k[0x1bb]) + mag(wrap(((128.0 * k[0x1a6]) + wrap_sum(((k[0x1a5] * m[12]) + 0.0) * 16.0)) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v19 = q15(v18);
	double const v20 = sat((v19 * sat((v19 * v18) + 0.0)) + 0.0);
	double const v21 = pos((128.0 * k[0x1c9]) + sum_on(sat(((k[0x1c7] * v18) + 0.0) * 16.0), k[0x1c8] * v20));
	double const v22 = sat((k[0x1cc] * v21) + 0.0);
	double const v23 = index(v22);
	double const d6 = s.ram[s.at(0x73, int(v23))];
	double const d7 = s.ram[s.at(0x74, 1 + int(v23))];
	double const v24 = sat(((k[0x1d0] * r[8]) + sum(k[0x1cf] * r[7])) * 2.0);
	double const v25 = sat(((k[0x1d3] * v24) + sum(k[0x1d1] * r[67], k[0x1d2] * r[68])) * 2.0);
	double const v26 = sat(((k[0x1d8] * v25) + sum(k[0x1d4] * r[68], k[0x1d5] * r[69], k[0x1d6] * r[70], k[0x1d7] * r[71])) * 2.0);
	double const v27 = sat(((k[0x1db] * v26) + sum(k[0x1d9] * r[70], k[0x1da] * r[72])) * 4.0);
	double const v28 = sat(((k[0x1dd] * r[7]) + sum(k[0x1dc] * r[8])) * 2.0);
	double const v29 = sat(((k[0x1e0] * v28) + sum(k[0x1de] * r[73], k[0x1df] * r[74])) * 2.0);
	double const v30 = sat(((k[0x1e5] * v29) + sum(k[0x1e1] * r[74], k[0x1e2] * r[75], k[0x1e3] * r[76], k[0x1e4] * r[77])) * 2.0);
	double const v31 = sat(((k[0x1e8] * v30) + sum(k[0x1e6] * r[76], k[0x1e7] * r[78])) * 4.0);
	s.ram[s.at(0x7e)] = clip32(sat((k[0x1fa] * v31) + sum(k[0x1f9] * v27)));
	s.ram[s.at(0x7f)] = clip32(sat((k[0x1ff] * v31) + 0.0));
	double const v32 = frac(v4);
	double const v33 = sat((v32 * d1) - wrap_sum((v32 * d0) - d0));
	double const v34 = q15(sat((k[0x1c3] * v3) + 0.0));
	double const v35 = frac(v16);
	double const v36 = sat((v35 * d5) - wrap_sum((v35 * d4) - d4));
	double const v37 = q15(sat((k[0x1c6] * v15) + 0.0));
	double const v38 = sat((v37 * v36) + v36);
	double const v39 = frac(v10);
	double const v40 = sat((v39 * d3) - wrap_sum((v39 * d2) - d2));
	double const v41 = q15(sat((k[0x1ce] * v9) + 0.0));
	double const v42 = sat((v41 * v40) + v40);
	double const v43 = sat((k[0x1f5] * v42) + sum(k[0x1f3] * sat((v34 * v33) + v33), k[0x1f4] * v38));
	double const v44 = frac(v22);
	double const v45 = sat((v44 * d7) - wrap_sum((v44 * d6) - d6));
	double const v46 = q15(sat((k[0x1cd] * v21) + 0.0));
	double const v47 = sat((k[0x1f8] * v38) + sum(k[0x1f6] * sat((v46 * v45) + v45), k[0x1f7] * v42));
	r[1] = v2;
	r[2] = v20;
	r[3] = v14;
	r[4] = v8;
	r[17] = sat(((k[0x1fc] * v43) + sum(k[0x1fb] * v27)) * 4.0);
	r[18] = sat(((k[0x1fe] * v47) + sum(k[0x1fd] * v31)) * 4.0);
	r[67] = v24;
	r[69] = r[68];
	r[71] = r[70];
	r[72] = v27;
	r[73] = v28;
	r[75] = r[74];
	r[77] = r[76];
	r[78] = v31;
	r[119] = v43;
	r[120] = v47;
	r[121] = v38;
	r[122] = v42;
	r[123] = v15;
	r[124] = v9;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[14] = lfo[14];
	m[15] = lfo[15];
	m[56] = d0;
	m[57] = d1;
	m[58] = d6;
	m[59] = d7;
	m[60] = d4;
	m[61] = d5;
	m[62] = d2;
	m[63] = d3;
	t[3] = v39;
	t[9] = v34;
	t[10] = v32;
	t[11] = v46;
	t[12] = v44;
	t[13] = v37;
	t[14] = v35;
	t[15] = v41;
	r[68] = v25;
	r[70] = v26;
	r[74] = v29;
	r[76] = v30;
}

}  // namespace fs1r::effects

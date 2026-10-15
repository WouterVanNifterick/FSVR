// fs1r/native/ins_m01.cpp - VOP3-2, the insertion window's program for type msb 01: its 96 steps.
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
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset
// through c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is
// folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in the order yss236_core::step() runs its own.
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
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m01: identical, 5 types, 48000 native passes.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's.
void block_0x1a0(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double* const t = c.t;                 // the 16 t registers, the delay lengths' fractions
	double xv, k, m2, prod, a, y, outv, v; // the step's operands and its result

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x1a0 to 0x1b3, the LFO cell, the four sweep accumulators and the four envelopes --------
		case 0x1a0:  // acc = |2 * m[12]|: this window's LFO cell, rectified and doubled, into the running value. ins::LFO_DIR_STEPS[0], which the ensemble detune's pitch words use and no type on this program writes
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[12];
			m2 = c.src;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1a1:  // m[3] takes the left serial input of page 13, for the windows outside this one; r[121] = saturate(2 * (128 * k + acc)): the first sweep accumulator's step
			c.queue_d(3, c.in[0][13]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(121, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1a2:  // m[7] takes the right serial input of page 13; acc = 16 * k * m[12]: the LFO cell scaled, the offset the next accumulator takes
			c.queue_d(7, c.in[1][13]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[12];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a3:  // r[124] = wrap(2 * (128 * k + acc)): the second sweep accumulator, wrapped rather than saturated
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(124, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a4:  // r[121] = saturate(2 * k * r[121]), and t[9] the same value as a 1.15 multiplier: the first accumulator's feedback
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1a5:  // acc = 16 * k * m[12], the third accumulator's offset
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[12];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:  // ins::BAL_STEP_A, the tremolo's LFO phase and the 2-way rotary's mic angle; r[122] = wrap(2 * (128 * k + acc)): the third sweep accumulator
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(122, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a7:  // m[13] takes page 13's LFO, this window's second; r[1] = saturate(t[9] * r[121]): the first envelope, squared once
			c.queue_d(13, c.lfo[13]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a8:  // ins::LFO_DIR_STEPS[1]; acc = |r[124]|: the rectifier that feeds the second accumulator back
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1a9:  // r[124] = saturate(2 * (128 * k + acc)): the same accumulator, saturated rather than wrapped
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(124, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1aa:  // r[1] = saturate(t[9] * r[1]): the first envelope's smoothing
			xv = r[1];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ab:  // ins::BAL_STEP_B and ins::BAL_STEP_C, the auto pan's balance and the 2-way rotary's mic angle; r[123] = wrap(2 * (128 * k + m[12])): the fourth accumulator, straight off the LFO cell
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[12];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(123, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1ac:  // r[124] = saturate(2 * k * r[124]), and t[15] the same value as a multiplier
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1ad:  // acc = 16 * k * r[121]
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ae:  // acc = k * r[1] + acc: the first envelope's own share of the first accumulator's clamp
			xv = r[1];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1af:  // ins::PC_FB[4]; r[121] = clampneg(128 * k + acc): that accumulator clamped at zero, which is what turns it into an amplitude envelope
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(121, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1b0:  // ins::LFO_DIR_STEPS[2]; r[4] = saturate(t[15] * r[124]): the second envelope
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[15] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(4, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b1:  // ins::PC_FB[5]; acc = |r[123]|: the rectifier on the fourth accumulator
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1b2:  // r[123] = saturate(2 * (128 * k + acc))
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(123, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1b3:  // ins::MOD_LEVEL[1], the chorus's second delay depth, which no type here writes; r[4] = saturate(t[15] * r[4]): the second envelope's smoothing
			xv = r[4];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[15] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(4, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1b4 to 0x1c9, the four delay lengths: each a register times a depth word, in samples ------
		case 0x1b4:  // ins::ROT_LEVEL[1] and ins::MOD_LEVEL[0]: acc = saturate(k * r[121]), t[10] the fraction of that delay in samples, and the first index queued, which the slot 0x6d and 0x6e reads take three steps later
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1b5:  // r[123] = saturate(2 * k * r[123]), and t[13] the same value as a multiplier
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1b6:  // ins::MODE_STEPS_B[0], the auto pan's direction word; acc = 16 * k * r[124]
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b7:  // the read of the slot 0x6d at the first index, which m[56] takes up at 0x1bc; ins::MODE_STEPS_B[1] and ins::LFO_DIR_STEPS[3]: acc = k * r[4] + acc
			c.push_hist(st, c.ram[c.addr(0x6d, 0 + c.cur_idx(), false)]);
			xv = r[4];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b8:  // r[124] = clampneg(128 * k + acc): the second accumulator's envelope clamp
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(124, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1b9:  // r[3] = saturate(t[13] * r[123]): the third envelope
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[13] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(3, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ba:  // ins::PC_MIX[0]; acc = |r[122]|: the rectifier on the third accumulator
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1bb:  // the read of the slot 0x6e one word further on at the same index, which m[57] takes up at 0x1c0; r[122] = saturate(2 * (128 * k + acc))
			c.push_hist(st, c.ram[c.addr(0x6e, 1 + c.cur_idx(), false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(122, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1bc:  // ins::ROT_LEVEL[7] and ins::MOD_LEVEL[5]: m[56] takes up the read of 0x1b7, acc = saturate(k * r[124]), t[3] the fraction, and the second index, which the slot 0x6f and 0x70 reads take at 0x1bf and 0x1c3
			c.queue_d(56, c.captured(m[56], st));
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1bd:  // r[3] = saturate(t[13] * r[3]): the third envelope's smoothing
			xv = r[3];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[13] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(3, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1be:  // r[122] = saturate(2 * k * r[122]), and t[11] the same value as a multiplier
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1bf:  // ins::MODE_STEPS_A[0], the auto pan's direction word; the read of the slot 0x6f at the second index, which m[62] takes up at 0x1c4; acc = 16 * k * r[122]
			c.push_hist(st, c.ram[c.addr(0x6f, 0 + c.cur_idx(), false)]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c0:  // m[57] takes up the read of 0x1bb; acc = k * r[3] + acc
			c.queue_d(57, c.captured(m[57], st));
			xv = r[3];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c1:  // r[123] = clampneg(128 * k + acc): the fourth accumulator's envelope clamp
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1c2:  // r[2] = saturate(t[11] * r[122]): the fourth envelope
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(2, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c3:  // ins::ROT_LEVEL[0], the amplitude depth of r[121]; the read of the slot 0x70 one word further on at the second index, which m[63] takes up at 0x1c8; acc = saturate(k * r[121]) and t[9] the same value as a multiplier
			c.push_hist(st, c.ram[c.addr(0x70, 1 + c.cur_idx(), false)]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1c4:  // ins::ROT_LEVEL[5]: m[62] takes up the read of 0x1bf, acc = saturate(k * r[123]), t[14] the fraction, and the third index, which the slot 0x71 and 0x72 reads take at 0x1c7 and 0x1cb
			c.queue_d(62, c.captured(m[62], st));
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1c5:  // r[2] = saturate(t[11] * r[2]): the fourth envelope's smoothing
			xv = r[2];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(2, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c6:  // ins::ROT_LEVEL[4]: acc = saturate(k * r[123]) and t[13] the same value as a multiplier
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1c7:  // the read of the slot 0x71 at the third index, which m[60] takes up at 0x1cc; m[14] takes page 14's LFO, this window's third; acc = 16 * k * r[122]
			c.push_hist(st, c.ram[c.addr(0x71, 0 + c.cur_idx(), false)]);
			c.queue_d(14, c.lfo[14]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c8:  // m[63] takes up the read of 0x1c3; acc = k * r[2] + acc
			c.queue_d(63, c.captured(m[63], st));
			xv = r[2];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c9:  // r[122] = clampneg(128 * k + acc): the third accumulator's envelope clamp
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;

		// ---- 0x1ca to 0x1cb, the first tap pair's interpolation ------------------------------------------
		case 0x1ca:  // acc = t[10] * m[56] - m[56]: the first tap pair's weight, its own cell's share of unity taken off
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = t[10] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cb:  // the read of the slot 0x72 one word further on at the third index, which m[61] takes up at 0x1d0; r[119] = saturate(t[10] * m[57] - acc): the pair interpolated, the left channel's first wet tap
			c.push_hist(st, c.ram[c.addr(0x72, 1 + c.cur_idx(), false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1cc to 0x1ce, the fourth delay length and the two amplitude words off r[122] and r[124] ---
		case 0x1cc:  // ins::ROT_LEVEL[3]: m[60] takes up the read of 0x1c7, acc = saturate(k * r[122]), t[12] the fraction, and the fourth index, which the slot 0x73 and 0x74 reads take at 0x1cf and 0x1d3
			c.queue_d(60, c.captured(m[60], st));
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1cd:  // ins::ROT_LEVEL[2]: acc = saturate(k * r[122]) and t[11] the same value as a multiplier
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1ce:  // ins::ROT_LEVEL[6]: acc = saturate(k * r[124]) and t[15] the same value as a multiplier
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;

		// ---- 0x1cf to 0x1db, the left channel's three bands ---------------------------------------------
		case 0x1cf:  // the read of the slot 0x73 at the fourth index, which m[58] takes up at 0x1d4; acc = k * r[7] + acc: the left channel's input, off r[7], the register the base program leaves the window
			c.push_hist(st, c.ram[c.addr(0x73, 0 + c.cur_idx(), false)]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d0:  // m[61] takes up the read of 0x1cb; r[67] = saturate(2 * (k * r[8] + acc)): the left channel's input register, with r[8], the other signal the base program leaves, mixed in
			c.queue_d(61, c.captured(m[61], st));
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d1:  // ins::LPF_ROT[1]: acc = k * r[67], the low shelf's b1 on its own input
			c.queue_d(4, c.in[0][14]);
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d2:  // ins::LPF_ROT[2] and ins::MOD_LEVEL[3]: acc = k * r[68] + acc, the same shelf's a1 on its own output
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d3:  // ins::LPF_ROT[0] and ins::MOD_LEVEL[2]: the read of the slot 0x74 one word further on at the fourth index, which m[59] takes up at 0x1d8; r[68] = saturate(2 * (k * r[67] + acc)): the low shelf's output
			c.push_hist(st, c.ram[c.addr(0x74, 1 + c.cur_idx(), false)]);
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d4:  // ins::EQ_ROT[1]: m[58] takes up the read of 0x1cf; acc = k * r[68], the mid peak's b1
			c.queue_d(58, c.captured(m[58], st));
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d5:  // ins::EQ_ROT[2]: r[69] takes r[68] a step back, the copy the same peak's b2 rides on
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(69, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d6:  // ins::EQ_ROT[3]: acc = k * r[70] + acc, its a1 on its own output
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d7:  // ins::EQ_ROT[4]: acc = k * r[71] + acc, its a2 on the copy
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d8:  // ins::EQ_ROT[0]: m[59] takes up the read of 0x1d3; r[70] = saturate(2 * (k * r[68] + acc)): the mid peak's output
			c.queue_d(59, c.captured(m[59], st));
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d9:  // ins::HPF_ROT[1]: acc = k * r[70], the high shelf's b1
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1da:  // ins::HPF_ROT[2]: r[71] takes r[70] a step back, the copy its a1 rides on
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(71, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1db:  // ins::HPF_ROT[0]: r[72] = saturate(4 * (k * r[70] + acc)), the high shelf's half output: the left channel's tone-shaped signal
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1dc to 0x1e8, the right channel's three bands --------------------------------------------
		case 0x1dc:  // acc = k * r[8] + 0: the right channel's input, off the other of the two signals
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dd:  // r[73] = saturate(2 * (k * r[7] + acc)): the right channel's input register, with r[7] mixed in
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1de:  // ins::LPF_ROT_C[1]: acc = k * r[73]
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1df:  // ins::LPF_ROT_C[2]: acc = k * r[74] + acc
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e0:  // ins::LPF_ROT_C[0]: r[74] = saturate(2 * (k * r[73] + acc)), the right channel's low shelf output
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(74, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e1:  // ins::EQ_ROT_C[1]: acc = k * r[74]
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e2:  // ins::EQ_ROT_C[2]: r[75] takes r[74] a step back
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(75, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e3:  // ins::EQ_ROT_C[3]: acc = k * r[76] + acc
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e4:  // ins::EQ_ROT_C[4]: acc = k * r[77] + acc
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e5:  // ins::EQ_ROT_C[0]: r[76] = saturate(2 * (k * r[74] + acc)), the right channel's mid peak output
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e6:  // ins::HPF_ROT_C[1]: acc = k * r[76]
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e7:  // ins::HPF_ROT_C[2]: m[15] takes page 15's LFO, this window's fourth; r[77] takes r[76] a step back
			c.queue_d(15, c.lfo[15]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(77, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e8:  // ins::HPF_ROT_C[0]: r[78] = saturate(4 * (k * r[76] + acc)), the right channel's tone-shaped signal
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e9 to 0x1ee, the other three interpolations ---------------------------------------------
		case 0x1e9:  // acc = t[12] * m[58] - m[58]: the fourth tap pair's weight
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = t[12] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ea:  // ins::EQ3_LEVEL[0]: r[120] = saturate(t[12] * m[59] - acc): the slots 0x73 and 0x74 interpolated, the right channel's first wet tap
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = t[12] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1eb:  // acc = t[14] * m[60] - m[60]: the third tap pair's weight
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = t[14] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ec:  // r[121] = saturate(t[14] * m[61] - acc): the slots 0x71 and 0x72 interpolated into the register that held the first sweep accumulator until 0x1c3
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[61];
			m2 = c.src;
			prod = t[14] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ed:  // acc = t[3] * m[62] - m[62]: the second tap pair's weight
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[62];
			m2 = c.src;
			prod = t[3] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ee:  // ins::MOD_WORDS[4]: r[122] = saturate(t[3] * m[63] - acc): the slots 0x6f and 0x70 interpolated into the register that held the third sweep accumulator until 0x1cd
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[63];
			m2 = c.src;
			prod = t[3] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ef to 0x1f2, the four amplitude gains -----------------------------------------------
		case 0x1ef:  // ins::MOD_WORDS[7]: r[119] = saturate(t[9] * r[119] + r[119]): the first tap's amplitude. t[9] is the negative depth word ins::ROT_LEVEL[0], so this is unity less that depth
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[9] * m2;
			a = c.x;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f0:  // ins::PC_MIX[5]: r[120] = saturate(t[11] * r[120] + r[120]), the fourth tap's amplitude
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.x;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f1:  // ins::MOD_WORDS[6]: r[121] = saturate(t[13] * r[121] + r[121]), the third tap's amplitude
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[13] * m2;
			a = c.x;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f2:  // r[122] = saturate(t[15] * r[122] + r[122]), the second tap's amplitude
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[15] * m2;
			a = c.x;
			y = (prod + a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f3 to 0x1f8, the two wet sums, which the pan law words weight -----------------------
		case 0x1f3:  // ins::ROT_PAN[0] and ins::MOD_WORDS[0]: acc = k * r[119], the left channel's first share. The 2-way rotary's low/high balance and the ambience's phase write this word, and dryWet() the dry gain
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f4:  // acc = k * r[121] + acc, the third tap, which no table word weighs
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f5:  // ins::ROT_PAN[2] and ins::MOD_WORDS[2]: r[119] = saturate(k * r[122] + acc): the left channel's two taps summed
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f6:  // ins::ROT_PAN[1] and ins::MOD_WORDS[1]: acc = k * r[120], the right channel's first share
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f7:  // acc = k * r[122] + acc, the second tap, which no table word weighs
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f8:  // ins::ROT_PAN[3] and ins::MOD_WORDS[3]: r[120] = saturate(k * r[121] + acc): the right channel's two taps summed
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f9 to 0x1ff, the dry mix, the two outputs and the two delay lines' writes --------------
		case 0x1f9:  // ins::TREM_WORDS[0]: acc = k * r[72], the left channel's tone-shaped signal into the tremolo's mix. EffectHost::tremolo()'s MODE writes this pair: a half on both under mode 0, unity on this and zero on 0x1fa under mode 1
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fa:  // ins::TREM_WORDS[1]: acc = saturate(k * r[78] + acc), the right channel's tone-shaped signal added; this step carries w1[13] and is not a writer, so it latches that mix for the delay RAM write at 0x1fb
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.wlatch = outv; c.haswl = true;
			break;
		case 0x1fb:  // ins::ROT_WORDS[0], the left dry gain: acc = k * r[72] for the output below, and the write into the slot 0x7e, which spends the latch 0x1fa left, so the line takes the tremolo mode's mix of the two tone-shaped signals
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7e, 0, false)] = outv;
			break;
		case 0x1fc:  // ins::ROT_WORDS[2], the left wet gain: r[17] = saturate(4 * (k * r[119] + acc)), the wet and the dry summed and scaled: the window's left output, which the base program reads back
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1fd:  // ins::ROT_WORDS[1], the right dry gain: acc = k * r[78]
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fe:  // ins::ROT_WORDS[3], the right wet gain: r[18] = saturate(4 * (k * r[120] + acc)), the right output
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ff:  // the write into the slot 0x7f: this step carries w1[13] too, so it drops the latch and writes its own value, k * r[78]
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			c.ram[c.addr(0x7f, 0, false)] = outv;
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m01(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

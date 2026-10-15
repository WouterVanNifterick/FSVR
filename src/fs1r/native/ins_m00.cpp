// fs1r/native/ins_m00.cpp - VOP3-2, the insertion window's program for type msb 00: its 96 steps.
//
// rom::INSERTION_PROGRAMS[0], the program the insertion window holds under every type
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 0, and under Thru, which loads it with every
// coefficient at zero. The nine the tracker names for this row are Thru, Chorus, Celeste, Flanger,
// Symphonic, Delay L,C,R, Delay L,R, Echo and Cross Delay, programmed by EffectHost::chorus(),
// flanger(), symphonic(), delayLCR(), delayLR(), echo() and crossDelay(). It is one of the fifteen
// programs the window shares: nothing here knows which type is running, because the program words are
// fixed and only the constants and the delay offsets move, which fx_modulation.cpp and fx_delay.cpp
// write whenever a parameter does. The code reads them where they live on the pass, as step() reads
// them, because that is the contract and not because the numbers are known here.
//
// What the 96 steps compute, in the groups the comments below open each of. The host names every group
// it programs, in EffectHost::chorus(), flanger(), symphonic(), delayLCR(), delayLR(), echo(),
// crossDelay() and the tables they call:
//
//   0x1a0-0x1be  the LFO cell m[12] through a rectifier into three accumulators, r[121], r[125] and
//                r[123], each of which names a delay length in samples: a register's value times 256
//                is the delay, its integer part is the index the taps read at (queue_i at 0x1b4,
//                0x1bd and 0x1d3, in effect three steps later) and its fraction the multiplier that
//                interpolates between a line's two taps. The steps that seed the three accumulators,
//                0x1a7, 0x1aa, 0x1af and 0x1b1 and the same four around r[125] and r[123], take their
//                words from ins::PC_FB and ins::LFO_DIR_STEPS, which are the ensemble detune's, and
//                from ins::PH_DEPTH, the phaser's. None of the handlers that load this program writes
//                them, so under those types the three accumulators hold zero, the index is zero and
//                every tap reads at the offset the host wrote. That is what this program does with the
//                words it is given, and the check below is what judges it; the host's chorus and
//                flanger are a question of their own, which this row does not touch.
//   0x1bf-0x1ca  the left channel's three bands. r[7], which the base program leaves the window, goes
//                into r[67] and through ins::LPF_MOD into r[68], the low shelf's output; that into
//                r[70] through ins::EQ_MOD, the mid peak; and that into r[72] through ins::HPF_MOD,
//                the high shelf's half. The first two delay taps, the slots 0x6E and 0x6F and 0x70 and
//                0x71, are read on the way past, at the first accumulator's index.
//   0x1cb-0x1ea  the other two taps, the slots 0x72 and 0x73 and 0x75 and 0x76, each pair at one word
//                apart so that the pair interpolates a fractional delay, and the four
//                interpolations: m[56] and m[57] to r[79] on t[9], m[60] and m[61] to r[119] on t[11],
//                m[58] and m[59] to r[81] on t[10] and m[62] and m[63] to r[120] on t[11]. The fixed
//                tap of the slot 0x68 is latched at 0x1cc and spent by the write into the slot 0x74 at
//                0x1d3, and the one of the slot 0x69 is latched at 0x1ea and spent by the write into the
//                slot 0x7A at 0x1eb. ins::LPF_MOD_C brings the right channel's r[8] in, and
//                ins::DELAY_CURVE makes r[79] the left channel's damped feedback in r[80].
//   0x1eb-0x1e4  the right channel's mid band and high shelf, ins::EQ_MOD_C into r[76] and
//                ins::HPF_MOD_C into r[78].
//   0x1e5-0x1ef  ins::DELAY_CURVE_COPY makes r[81] the right channel's damped feedback in r[82], and
//                the two wet sums: r[4] of r[119], r[120] and r[79], and r[5] of r[119], r[120] and
//                r[81].
//   0x1f0-0x1ff  the dry and wet, ins::MOD_WORDS's four words, into the two outputs r[17] and r[18],
//                which the base program reads back, and the two delay lines' writes: the slot 0x7E
//                takes r[80] at 0x1fb and the slot 0x7F takes r[82] with r[78] and r[72] at 0x1ff.
//
// The cells the program fills are its two serial inputs and its LFO, m[3] and m[7] from the banks at
// page 13 and m[4] from page 14, and m[13], m[14] and m[15] the page's LFOs, for the windows outside
// this one; and the eight reads it takes up for itself, m[42] and m[43] from the fixed slots 0x68 and
// 0x69 and m[56] to m[63] from the six swept taps, each five to eleven steps after the read that fills
// it, which is captured()'s window. The registers: r[7] and r[8] the two signals the base program
// leaves the window; r[67], r[68], r[70] and r[72] the left channel's three bands and r[73], r[74],
// r[76] and r[78] the right's; r[79] and r[80] the left channel's tap and its damped feedback, r[81] and
// r[82] the right's; r[119] and r[120] the two fractional taps; r[4] and r[5] the two wet sums; r[17] and
// r[18] the two outputs; r[121], r[125] and r[123] the three accumulators, with r[122], r[124], r[126]
// and r[127] their companion registers, r[1], r[2] and r[3] the seeds' products and r[69], r[71], r[75]
// and r[77] the four z^-1 copies the two right-hand filter bands ride on. The slots: 0x68 and 0x69 the
// two fixed taps, 0x6E and 0x6F, 0x70 and 0x71, 0x72 and 0x73 and 0x75 and 0x76 the four tap pairs,
// 0x74 and 0x7A the two slots the fixed taps are written into, which no step of this program reads,
// and 0x7E and 0x7F the two delay lines' own writes, which ins::MOD_SLOTS calls slots 5 and 0.
//
// The old model in vop3_effects.h has no case for this program's types beyond its chorus, flanger and
// echo one-liners, each a single delay line with a feedback and a dry/wet, and is no reference for any
// number here.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic
// below in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m00: identical, 55 types, 528000 native passes.

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
		// ---- 0x1a0 to 0x1be, the LFO, the three accumulators and the three delay lengths ----------------
		case 0x1a0:  // r[121] = wrap(2 * (128 * k + m[12])): the LFO cell, doubled, into the first sweep register
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[12];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(121, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a1:  // m[3] takes the left serial input of page 13, for the windows outside this one; r[127] = wrap(16 * k * m[12])
			c.queue_d(3, c.in[0][13]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[12];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(127, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1a2:  // m[7] takes the right serial input of page 13; r[122] = wrap(2 * (128 * k + acc)), the first sweep register's companion
			c.queue_d(7, c.in[1][13]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(122, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a3:  // the read of the slot 0x68, a fixed tap, which m[42] takes up at 0x1cc; acc = |r[121]|
			c.push_hist(st, c.ram[c.addr(0x68, 0, false)]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1a4:  // r[121] = saturate(2 * (128 * k + acc)): the rectifier on that register, doubled
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(121, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1a5:  // r[125] = wrap(2 * (128 * k + m[12])): the second sweep register, off the same LFO cell
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[12];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(125, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a6:  // r[126] = wrap(2 * (128 * k + r[127]))
			xv = r[127];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(126, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a7:  // the read of the slot 0x69, the second fixed tap, which m[43] takes up at 0x1ea, and m[13] takes this page's LFO; r[121] = saturate(2 * k * r[121]) and t[9] the fraction of the delay in samples it names
			c.push_hist(st, c.ram[c.addr(0x69, 0, false)]);
			c.queue_d(13, c.lfo[13]);
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1a8:  // m[42] takes up the read of 0x1a3; acc = |r[125]|
			c.queue_d(42, c.captured(m[42], st));
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1a9:  // r[125] = saturate(2 * (128 * k + acc))
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(125, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1aa:  // r[1] = saturate(t[9] * r[121])
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ab:  // r[123] = wrap(2 * (128 * k + m[12])): the third sweep register
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[12];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(123, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1ac:  // m[43] takes up the read of 0x1a7; r[125] = saturate(2 * k * r[125]) and t[11] the fraction
			c.queue_d(43, c.captured(m[43], st));
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(125, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1ad:  // r[1] = saturate(t[9] * r[1]): the same word twice, r[1] carrying the first product
			xv = r[1];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ae:  // r[124] = wrap(2 * (128 * k + r[127]))
			xv = r[127];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(124, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1af:  // acc = saturate(16 * k * r[121])
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b0:  // acc = k * r[1] + acc
			xv = r[1];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b1:  // r[121] = clampneg(128 * k + acc): the first accumulator's next value
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			c.queue_r(121, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1b2:  // r[3] = saturate(t[11] * r[125])
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(3, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b3:  // ins::MOD_LEVEL[1]: acc = |k * r[122]|
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b4:  // ins::MOD_LEVEL[0], the first delay's depth: acc = clampneg(k * r[121] + acc), t[9] the fraction below one sample and the index the taps at 0x1bb to 0x1c7 read at
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1b5:  // r[3] = saturate(t[11] * r[3])
			xv = r[3];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(3, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b6:  // acc = |r[123]|
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b7:  // r[123] = saturate(2 * (128 * k + acc))
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 2.0;
			c.queue_r(123, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1b8:  // acc = saturate(16 * k * r[125])
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1b9:  // acc = k * r[3] + acc
			xv = r[3];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ba:  // r[125] = clampneg(128 * k + acc): the second accumulator's next value
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			c.queue_r(125, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1bb:  // the read of the slot 0x6e at that index; r[123] = saturate(2 * k * r[123]) and t[10] the fraction
			c.push_hist(st, c.ram[c.addr(0x6e, 0 + c.cur_idx(), false)]);
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1bc:  // ins::MOD_LEVEL[5]: acc = |k * r[126]|
			xv = r[126];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1bd:  // ins::MOD_LEVEL[4], the second delay's depth: acc = clampneg(k * r[125] + acc), t[11] the fraction and the index the taps at 0x1cb and 0x1cf read at
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			break;
		case 0x1be:  // r[2] = saturate(t[10] * r[123])
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(2, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x1bf to 0x1ca, the left channel's three bands, with the first two taps read on the way -----
		case 0x1bf:  // the read of the slot 0x6f one word further on, m[57] takes it up at 0x1c4; r[67] = saturate(2 * k * r[7]), the left channel's input into the low shelf
			c.push_hist(st, c.ram[c.addr(0x6f, 1 + c.cur_idx(), false)]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c0:  // m[56] takes up the read of 0x1bf; ins::LPF_MOD[2], the low shelf's -a1 on its own state r[68]
			c.queue_d(56, c.captured(m[56], st));
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c1:  // ins::LPF_MOD[1], b1 on the input
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c2:  // ins::LPF_MOD[0], b0 on the input, doubled and saturating, into r[68]: the low shelf's output
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c3:  // the read of the slot 0x70 at the first index, which m[60] takes up at 0x1c8
			c.push_hist(st, c.ram[c.addr(0x70, 0 + c.cur_idx(), false)]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c4:  // m[57] takes up the read of 0x1bf and r[69] takes r[68] a step back, the copy the mid band's b2 word rides on; ins::EQ_MOD[2]
			c.queue_d(57, c.captured(m[57], st));
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c5:  // ins::EQ_MOD[3], -a1 on the mid band's own state r[70]
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c6:  // ins::EQ_MOD[4], -a2 on the same state a step back
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c7:  // the read of the slot 0x71 one word further on and m[14] takes this page's LFO; ins::EQ_MOD[0], b0 on the low shelf's output, doubled and saturating, into r[70]: the mid band's output
			c.push_hist(st, c.ram[c.addr(0x71, 1 + c.cur_idx(), false)]);
			c.queue_d(14, c.lfo[14]);
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c8:  // m[60] takes up the read of 0x1c3; ins::HPF_MOD[1], the high shelf's b1 on the mid band's output
			c.queue_d(60, c.captured(m[60], st));
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1c9:  // r[71] takes r[70] a step back; ins::HPF_MOD[2], -a1 on the high shelf's own state r[72]
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ca:  // ins::HPF_MOD[0], b0 on the mid band's output, four times and saturating, into r[72]: the left channel's tone-shaped input
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x1cb to 0x1ea, the other two taps, the fixed taps and the left channel's damp --------------
		case 0x1cb:  // the read of the slot 0x72 at the second index, which m[62] takes up at 0x1d0; r[2] = saturate(t[10] * r[2])
			c.push_hist(st, c.ram[c.addr(0x72, 0 + c.cur_idx(), false)]);
			xv = r[2];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(2, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1cc:  // acc = m[42], the first fixed tap, latched as the write data, which 0x1d3 spends on the slot 0x74
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.wlatch = outv; c.haswl = true;
			break;
		case 0x1cd:  // m[61] takes up the read of 0x1c7; acc = saturate(16 * k * r[123])
			c.queue_d(61, c.captured(m[61], st));
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ce:  // acc = k * r[2] + acc
			xv = r[2];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1cf:  // the read of the slot 0x73 one word further on; r[123] = clampneg(128 * k + acc), the third accumulator's next value
			c.push_hist(st, c.ram[c.addr(0x73, 1 + c.cur_idx(), false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			c.queue_r(123, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1d0:  // m[62] takes up the read of 0x1cb; acc = t[11] * m[60] - m[60]
			c.queue_d(62, c.captured(m[62], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = t[11] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d1:  // m[4] takes the left serial input of page 14; r[119] = saturate(t[11] * m[61] - acc), which is m[60] + t[11] * (m[61] - m[60]): the first delay's fractional tap
			c.queue_d(4, c.in[0][14]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[61];
			m2 = c.src;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d2:  // ins::MOD_LEVEL[3]: acc = |k * r[124]|
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d3:  // ins::MOD_LEVEL[2], the third delay's depth: acc = clampneg(k * r[123] + acc), t[10] the fraction and the index the taps at 0x1d7 and 0x1db read at; this step is a writer without w1[13], so it spends the latch 0x1cc left, which is what the slot 0x74 takes
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.t_write(c.ops[st], st, y);
			c.queue_i(std::floor(y * 256.0));
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x74, 0, false)] = outv;
			break;
		case 0x1d4:  // m[63] takes up the read of 0x1cf; acc = t[9] * m[56] - m[56]
			c.queue_d(63, c.captured(m[63], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = t[9] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d5:  // r[79] = saturate(t[9] * m[57] - acc), which is m[56] + t[9] * (m[57] - m[56]): the second delay's fractional tap, the feedback line's
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = t[9] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d6:  // ins::DELAY_CURVE[1]: acc = k * r[79]
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d7:  // the read of the slot 0x75 at the third index, which m[58] takes up at 0x1e5; acc = k * r[80] + acc
			c.push_hist(st, c.ram[c.addr(0x75, 0 + c.cur_idx(), false)]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d8:  // ins::DELAY_CURVE[0]: r[80] = saturate(2 * (k * r[79] + acc)), the left channel's damped feedback
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1d9:  // r[73] = saturate(2 * k * r[8]), the right channel's input into its low shelf
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1da:  // ins::LPF_MOD_C[1]: acc = k * r[73]
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1db:  // the read of the slot 0x76 one word further on; ins::LPF_MOD_C[2], -a1 on r[74]
			c.push_hist(st, c.ram[c.addr(0x76, 1 + c.cur_idx(), false)]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1dc:  // m[58] takes up the read of 0x1d7; ins::LPF_MOD_C[0], b0 on r[73], doubled and saturating, into r[74]
			c.queue_d(58, c.captured(m[58], st));
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1dd:  // ins::EQ_MOD_C[1]: acc = k * r[74]
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1de:  // r[75] takes r[74] a step back; ins::EQ_MOD_C[2], b2 on the low shelf's output
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1df:  // ins::EQ_MOD_C[3], -a1 on the right channel's mid band state r[76]
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e0:  // m[59] takes up the read of 0x1db; ins::EQ_MOD_C[4], -a2 on the same state
			c.queue_d(59, c.captured(m[59], st));
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e1:  // ins::EQ_MOD_C[0], b0 on r[74], doubled and saturating, into r[76]: the right channel's mid band's output
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e2:  // ins::HPF_MOD_C[1]: acc = k * r[76]
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e3:  // r[77] takes r[76] a step back; ins::HPF_MOD_C[2], -a1 on r[78]
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e4:  // ins::HPF_MOD_C[0], b0 on r[76], four times and saturating, into r[78]: the right channel's tone-shaped input
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
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x1e5 to 0x1ef, the right channel's damp and the two wet sums -----------------------------
		case 0x1e5:  // acc = t[10] * m[58] - m[58]
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = t[10] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e6:  // r[81] = saturate(t[10] * m[59] - acc), which is m[58] + t[10] * (m[59] - m[58]): the third delay's fractional tap
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e7:  // m[15] takes this page's LFO; ins::DELAY_CURVE_COPY[1]: acc = k * r[81]
			c.queue_d(15, c.lfo[15]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e8:  // acc = k * r[82] + acc
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1e9:  // ins::DELAY_CURVE_COPY[0]: r[82] = saturate(2 * (k * r[81] + acc)), the right channel's damped feedback
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(82, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ea:  // acc = m[43], the second fixed tap, latched as the write data, which 0x1eb spends on the slot 0x7a
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.wlatch = outv; c.haswl = true;
			break;

		// ---- 0x1eb to 0x1e4, the second fixed tap and the right channel's mid band and high shelf -------
		case 0x1eb:  // acc = t[11] * m[62] - m[62]; the write into the slot 0x7a takes the latch 0x1ea left
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[62];
			m2 = c.src;
			prod = t[11] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7a, 0, false)] = outv;
			break;
		case 0x1ec:  // r[120] = saturate(t[11] * m[63] - acc), which is m[62] + t[11] * (m[63] - m[62]): the second delay's fractional tap
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[63];
			m2 = c.src;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ed:  // acc = k * r[79]
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ee:  // acc = k * r[119] + acc
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ef:  // r[4] = saturate(k * r[120] + acc): the left output's wet sum
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(4, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x1f0 to 0x1ff, the dry and wet into the two outputs, and the two delay lines' writes -------
		case 0x1f0:  // acc = k * r[81]
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f1:  // acc = k * r[120] + acc
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f2:  // r[5] = saturate(k * r[119] + acc): the right output's wet sum
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(5, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f3:  // ins::MOD_WORDS[0], the left output's dry word: acc = k * r[72]
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
			break;
		case 0x1f4:  // acc = k * r[78] + acc
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f5:  // ins::MOD_WORDS[2], the wet word: r[17] = saturate(4 * (k * r[4] + acc)), the left output
			xv = r[4];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f6:  // ins::MOD_WORDS[1], the right output's dry word: acc = k * r[78]
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f7:  // acc = k * r[72] + acc
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f8:  // ins::MOD_WORDS[3], the wet word: r[18] = saturate(4 * (k * r[5] + acc)), the right output
			xv = r[5];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1f9:  // acc = k * r[78]
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1fa:  // acc = k * r[82] + acc
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1fb:  // acc = saturate(k * r[80]); this step carries w1[13] and is a writer, so it drops the latch and writes its own value into the slot 0x7e: the second delay line's write
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			c.ram[c.addr(0x7e, 0, false)] = outv;
			break;
		case 0x1fc:  // acc = k * r[72]
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
			break;
		case 0x1fd:  // acc = k * r[78] + acc
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1fe:  // acc = k * r[80] + acc
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x1ff:  // acc = saturate(k * r[82] + acc); this step carries w1[13] and is a writer, so it drops the latch and writes its own value into the slot 0x7f: the first delay line's write
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
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
void ins_m00(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

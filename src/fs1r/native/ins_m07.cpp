// fs1r/native/ins_m07.cpp - VOP3-2, the insertion window's program for Pitch Chng, Ens Detune and 3-Band EQ:
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
// Every constant the program reads is read from c.coef[st] on the pass and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m07: identical, 3 types, 28800 native passes.
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
	double xv, k, m2, prod, a, y, v, outv; // the step's operands and its result

	// The step's x operand: the register r[sr] into the latch every step but a class-2/3 one moves, and
	// xv alongside it for the multiplier.
	auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };
	// The t file write of a step whose result is y, in the register form: t as a 1.15 multiplier.
	auto twr = [&](double yy, int tn) {
		double const w = yy * 256.0;
		c.queue_t(tn, (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0);
	};
	// The same write in the fraction form (w1[14]): the fraction below one delay sample, which is what
	// the tap a step's index load steers reads between.
	auto twf = [&](double yy, int tn) { double const w = yy * 256.0; c.queue_t(tn, w - std::floor(w)); };

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{

		// ---- 0x1a0 to 0x1af, the first voice's two read positions and the four taps ----
		// ins::LFO_DIR_STEPS[0]: acc = max(0, k * m[12]), t[10] takes the fraction below one sample of
		// that and the delay index takes the whole part, both three steps later
		case 0x1a0:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[12];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			twf(y, 10);
			c.queue_i(std::floor(y * 256.0));
			break;
		// m[3] takes the left serial input of page 13; class 3 takes m[12] at unity: r[121] = 2 * m[12]
		case 0x1a1:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[12];
			m2 = c.src; prod = m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(121, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[7] takes the right serial input of page 13. A class-0 step still moves the x latch, step()
		//'s one latch move outside the class test
		case 0x1a2:
			c.queue_d(7, c.in[1][13]);
			fetch(0);
			break;
		// the read of the slot 0x68 at the position 0x1a0 loaded; the capture at 0x1a8 takes it
		case 0x1a3:
			c.push_hist(st, c.ram[c.addr(0x68, c.cur_idx(), false)]);
			fetch(0);
			break;
		// the ROM's own word: acc = k * r[121]
		case 0x1a4:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: r[122] = max(0, 128 * k + acc)
		case 0x1a5:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// m[13] takes the LFO of page 13; class 3 at absolute value: r[121] = |r[121]|
		case 0x1a6:
			c.queue_d(13, c.lfo[13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 3));
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		// ins::PC_FB[2] and the read of the slot 0x69, one word older, at the same position:
		// r[123] = max(0, 128 * k + acc)
		case 0x1a7:
			c.push_hist(st, c.ram[c.addr(0x69, (c.cur_idx() + 1), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// m[56] takes up the read of 0x1a3; ins::LFO_DIR_STEPS[1] and ins::PC_FB's second position:
		// acc = max(0, k * r[122]), with t[12] the fraction and the index the whole part
		case 0x1a8:
			c.queue_d(56, c.captured(m[56], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			twf(y, 12);
			c.queue_i(std::floor(y * 256.0));
			break;
		// class 3: r[124] = 2 * m[13]
		case 0x1a9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[13];
			m2 = c.src; prod = m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(124, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_FB[3] off r[121] itself, the latch this step reads: r[121] = max(0, 128 * k + x)
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(121, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// the read of the slot 0x6a at the position 0x1a8 loaded: acc = 16 * (k * r[123]), and t[9]
		// takes it in the register form
		case 0x1ab:
			c.push_hist(st, c.ram[c.addr(0x6a, c.cur_idx(), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 9);
			break;
		// m[57] takes up the read of 0x1a7; the ROM's own word: acc = k * r[124]
		case 0x1ac:
			c.queue_d(57, c.captured(m[57], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: r[125] = max(0, 128 * k + acc)
		case 0x1ad:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(125, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// class 3 at absolute value: r[124] = |r[124]|
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 3));
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		// ins::PC_FB[4] and the read of the slot 0x6b, one word older, at the same position:
		// r[126] = max(0, 128 * k + acc)
		case 0x1af:
			c.push_hist(st, c.ram[c.addr(0x6b, (c.cur_idx() + 1), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(126, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;

		// ---- 0x1b0 to 0x1b4, the second voice's two read positions ----
		// m[60] takes up the read of 0x1ab; ins::LFO_DIR_STEPS[2]: acc = max(0, k * m[13]), t[14] the
		// fraction and the index the whole part
		case 0x1b0:
			c.queue_d(60, c.captured(m[60], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[13];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			twf(y, 14);
			c.queue_i(std::floor(y * 256.0));
			break;
		// ins::PC_FB[5] off the latch this step takes: r[124] = max(0, 128 * k + x)
		case 0x1b1:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * k + a;
			v *= 1.0;
			c.queue_r(124, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// acc = 16 * (k * r[121]), and t[11] takes it in the register form
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 11);
			break;
		// the read of the slot 0x6c at the position 0x1b0 loaded: acc = 16 * (k * r[126]), and t[13]
		// takes it in the register form
		case 0x1b3:
			c.push_hist(st, c.ram[c.addr(0x6c, c.cur_idx(), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(126);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 13);
			break;
		// m[61] takes up the read of 0x1af: acc = 16 * (k * r[124]), the adder input off, and t[15]
		// takes it in the register form
		case 0x1b4:
			c.queue_d(61, c.captured(m[61], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 15);
			break;

		// ---- 0x1b5 to 0x1b9, the four taps, each a pair of words crossfaded by its fraction ----
		// the first voice's first tap: acc = t[10] * m[56] - m[56]
		case 0x1b5:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src; prod = c.t[10] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// and the word one older: r[119] = saturate(t[10] * m[57] - acc), the tap itself
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[57];
			m2 = c.src; prod = c.t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::LFO_DIR_STEPS[3] and the read of the slot 0x6d, one word older: acc = max(0, k * r[125]),
		// t[3] the fraction and the index the whole part, so this read runs on the position the step
		// itself loads
		case 0x1b7:
			c.push_hist(st, c.ram[c.addr(0x6d, (c.cur_idx() + 1), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(125);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			twf(y, 3);
			c.queue_i(std::floor(y * 256.0));
			break;
		// m[58] takes up the read of 0x1b3; the first voice's second tap: acc = t[12] * m[60] - m[60]
		case 0x1b8:
			c.queue_d(58, c.captured(m[58], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[60];
			m2 = c.src; prod = c.t[12] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// and the word one older: r[121] = saturate(t[12] * m[61] - acc)
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[61];
			m2 = c.src; prod = c.t[12] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ba to 0x1c9, the left channel's three bands ----
		// ins::PC_MIX[0], the stereo switch on the left input: r[67] = saturate(2 * (k * r[7]))
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::EQ7_A[1], b1, and the read of the slot 0x6e: acc = k * r[67], the adder input off,
		// which is how each section's first word starts the section afresh
		case 0x1bb:
			c.push_hist(st, c.ram[c.addr(0x6e, c.cur_idx(), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[59] takes up the read of 0x1b7; ins::EQ7_A[2], b2, and r[68] takes the step before's x:
		// acc = k * r[68] + acc
		case 0x1bc:
			c.queue_d(59, c.captured(m[59], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(68, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A[3], -a1: acc = k * r[69] + acc
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A[4], -a2: acc = k * r[70] + acc
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A[0], b0, and the read of the slot 0x6f, one word older:
		// r[69] = saturate(2 * (k * r[67] + acc)), the band's own output a pass back for the next
		// section's -a1
		case 0x1bf:
			c.push_hist(st, c.ram[c.addr(0x6f, (c.cur_idx() + 1), false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[62] takes up the read of 0x1bb; ins::EQ7_B[1], b1: acc = k * r[69]
		case 0x1c0:
			c.queue_d(62, c.captured(m[62], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B[2], b2, and r[70] takes the step before's x: acc = k * r[70] + acc
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(70, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B[3], -a1: acc = k * r[71] + acc
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B[4], -a2, and the read of the slot 0x70: acc = k * r[72] + acc
		case 0x1c3:
			c.push_hist(st, c.ram[c.addr(0x70, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			fetch(72);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[63] takes up the read of 0x1bf; ins::EQ7_B[0], b0: r[71] = saturate(2 * (k * r[69] + acc))
		case 0x1c4:
			c.queue_d(63, c.captured(m[63], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::EQ7_C[1], b1: acc = k * r[71]
		case 0x1c5:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C[2], b2, and r[72] takes the step before's x: acc = k * r[72] + acc
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			fetch(72);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(72, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[14] takes the LFO of page 14; ins::EQ7_C[3], -a1, and the read of the slot 0x71:
		// acc = k * r[73] + acc
		case 0x1c7:
			c.push_hist(st, c.ram[c.addr(0x71, 0, false)]);
			c.queue_d(14, c.lfo[14]);
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[42] takes up the read of 0x1c3; ins::EQ7_C[4], -a2, and r[74] takes the step before's x:
		// acc = k * r[74] + acc
		case 0x1c8:
			c.queue_d(42, c.captured(m[42], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(74);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(74, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C[0], b0: r[73] = saturate(16 * (k * r[71] + acc)), the left chain's output
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ca to 0x1da, the input sum and the other channel's three bands ----
		// ins::PC_MIX[2]: acc = k * r[8]
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_MIX[1]: r[75] = saturate(2 * (k * r[7] + acc)), the summed input
		case 0x1cb:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[43] takes up the read of 0x1c7; ins::EQ7_A_C[1], b1: acc = k * r[75]
		case 0x1cc:
			c.queue_d(43, c.captured(m[43], st));
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A_C[2], b2, and r[76] takes the step before's x: acc = k * r[76] + acc
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A_C[3], -a1: acc = k * r[77] + acc
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A_C[4], -a2: acc = k * r[78] + acc
		case 0x1cf:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_A_C[0], b0: r[77] = saturate(2 * (k * r[75] + acc))
		case 0x1d0:
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[4] takes the left serial input of page 14; ins::EQ7_B_C[1], b1: acc = k * r[77]
		case 0x1d1:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B_C[2], b2, and r[78] takes the step before's x: acc = k * r[78] + acc
		case 0x1d2:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(78, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B_C[3], -a1: acc = k * r[79] + acc
		case 0x1d3:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B_C[4], -a2: acc = k * r[80] + acc
		case 0x1d4:
			k = yss236_core::s16(c.coef[st]);
			fetch(80);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_B_C[0], b0: r[79] = saturate(2 * (k * r[77] + acc))
		case 0x1d5:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::EQ7_C_C[1], b1: acc = k * r[79]
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C_C[2], b2, and r[80] takes the step before's x: acc = k * r[80] + acc
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			fetch(80);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(80, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C_C[3], -a1: acc = k * r[81] + acc
		case 0x1d8:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C_C[4], -a2, and r[82] takes the step before's x: acc = k * r[82] + acc
		case 0x1d9:
			k = yss236_core::s16(c.coef[st]);
			fetch(82);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(82, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ7_C_C[0], b0: r[81] = saturate(16 * (k * r[79] + acc)), the chain that feeds the delay
		// lines and, in mono, the dry signal too
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1db to 0x1e2, the second voice's two taps and the two crossfades ----
		// the second voice's tap at the position 0x1b7 loaded: acc = t[3] * m[62] - m[62]
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[62];
			m2 = c.src; prod = c.t[3] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// and the word one older: r[122] = saturate(t[3] * m[63] - acc)
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[63];
			m2 = c.src; prod = c.t[3] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the second voice's tap at the position 0x1b0 loaded: acc = t[14] * m[58] - m[58]
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[58];
			m2 = c.src; prod = c.t[14] * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// and the word one older: r[120] = saturate(t[14] * m[59] - acc)
		case 0x1de:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[59];
			m2 = c.src; prod = c.t[14] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the first crossfade: acc = t[11] * r[121] - x
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[11] * m2;
			a = c.x;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// r[121] = saturate(t[9] * r[119] - acc), the two voices' first tap crossfaded
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[9] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the second crossfade: acc = t[15] * r[122] - x
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[15] * m2;
			a = c.x;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// r[122] = saturate(t[13] * r[120] - acc), the two voices' second tap crossfaded
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[13] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e3 to 0x1f0, the two voice levels and the dry and wet mix ----
		case 0x1e3:
			fetch(0);
			break;
		// ins::PC_VOICE1_A: acc = k * r[121]
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[15] takes the LFO of page 14; ins::PC_VOICE2[1]: r[119] = saturate(k * r[122] + acc)
		case 0x1e5:
			c.queue_d(15, c.lfo[14]);
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::PC_VOICE2[0]: acc = k * r[122]
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_VOICE1_B: r[120] = saturate(k * r[121] + acc)
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::PC_WET[0]: acc = k * r[7], the left dry signal
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_MIX[3]: acc = k * r[73] + acc, the left chain
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::EQ3_LEVEL[0]: acc = k * r[81] + acc, the summed chain, which the mode parameter leaves
		// at a word of its own
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_MIX[5]: r[17] = saturate(4 * (k * r[119] + acc)), the left output
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// class 3 at unity: the value the slot 0x7b write below takes is m[42], the read 0x1c3 published
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[42];
			m2 = c.src; prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.wlatch = outv; c.haswl = true;
			break;
		// ins::PC_WET[1]: acc = k * r[8], the right dry signal
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_MIX[4]: acc = k * r[81] + acc
		case 0x1ee:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the write of the slot 0x7b, which takes the latch 0x1ec left rather than its own result
		case 0x1ef:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7b, 0, false)] = outv;
			break;
		// ins::PC_MIX[6]: r[18] = saturate(4 * (k * r[120] + acc)), the right output
		case 0x1f0:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f1 to 0x1ff, the second delay line's write and the first's feedback ----
		case 0x1f1:
		case 0x1f2:
			fetch(0);
			break;
		// class 3 at unity and a writer carrying w1[13], so it writes its own value: m[43], the read
		// 0x1c7 published, into the slot 0x7c
		case 0x1f3:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[43];
			m2 = c.src; prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			c.ram[c.addr(0x7c, 0, false)] = outv;
			break;
		case 0x1f4:
		case 0x1f5:
		case 0x1f6:
			fetch(0);
			break;
		// the ROM's own word: acc = k * r[73], which with the step's word zero clears the running value
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: acc = k * r[81] + acc, the signal going into the line
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_FB[0]: acc = k * r[121] + acc, the feedback off the first crossfade
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the running value into the write latch (w1[13]), which the class-0 writer 0x1fb takes
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.wlatch = outv; c.haswl = true;
			break;
		// the write of the slot 0x7e, the first voice's delay line
		case 0x1fb:
			fetch(0);
			if (c.haswl) { c.ram[c.addr(0x7e, 0, false)] = c.wlatch; c.haswl = false; }
			break;
		case 0x1fc:
			fetch(0);
			break;
		// the ROM's own word: acc = k * r[81], the second line's signal
		case 0x1fd:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::PC_FB[1]: acc = k * r[122] + acc, the feedback off the second crossfade
		case 0x1fe:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// a writer carrying w1[13] writes its own value: the slot 0x7f, the second voice's delay line
		case 0x1ff:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
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
void ins_m07(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
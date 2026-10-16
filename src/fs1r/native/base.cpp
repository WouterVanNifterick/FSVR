// fs1r/native/base.cpp - VOP3-2, the base program: its 138 steps outside the windows.
//
// The base program is the one every performance loads. EffectHost::uploadProgram's kind 0 writes
// rom::BASE_PROGRAM into the steps none of the five windows hold, which is what the checker's
// rangesFor(BASE) hands this function: every step 0x000 to 0x0ff minus the reverb's steps, the aux
// window's 0x0e8-0x10f and the three effect windows' steps. That is 138 steps in eleven runs, and
// this function runs them.
//
// Nothing here synthesises. The windows do that, and this program takes what they hand back, pans it,
// sums it, filters it and scales it. In step order:
//
//   0x000-0x007  the pan law. rom::mix::MIXER_A pairs the eight send registers r[11] to r[18] with the
//                eight pan words the host writes: the aux pan at steps 0 and 1, the reverb delay's at
//                2 and 3, the variation's at 4 and 5, the insertion's at 6 and 7, and the two mixer
//                parameters that have no performance byte, 13 at steps 8 and 9 and 12 at steps 0x0a and
//                0x0b. Each step multiplies its own register by the word in that step, so one pass pans
//                the eight sends. 0x000 takes the page 0 serial input into m[6] and 0x001 the second
//                serial bank into m[23]; Effects::run writes only the first bank, so m[23] is zero.
//                0x002 sends m[16] to out[0], the left DAC: Effects::run reads out[0] and out[8], and
//                the aux window's 0x10e puts m[17] on out[8].
//   0x008-0x00b  four multiplies on r[20] and r[22] into r[121] to r[124]. The words at 0x008 and
//                0x009 are the high-mid band's b1 and b2 (rom::mix::EQ_HIGH_MID is {0x7e,0x7b,0x7a,
//                0x7c,0x7d}, designFilter's b0, b1, -a1, b2, -a2 order), and the words at 0x00a and
//                0x00b are mixer 12's pan pair. Nothing writes r[19] to r[22]: they are what the
//                windows leave, and these four steps weight them into the sums below.
//   0x00c-0x019  the sends into the reverb and the variation. 0x00c takes mixer 4's level word
//                (rom::mix::MIXER_C[13], which EffectHost::mixerParam's case 4 writes) times r[14], the
//                reverb delay's right pan, and 0x011 adds mixer 7's (MIXER_C[8]) times r[16], the
//                variation's right pan; 0x012 scales the pair into r[120]. 0x013 and 0x018 do the same
//                for the left pans r[13] and r[15]: 0x017 writes the first of them into delay RAM slot
//                5, and 0x019 scales the second into r[119]. The steps between those sums, 0x00d, 0x00e,
//                0x00f, 0x010, 0x014, 0x015, 0x016, add coefficients no host write ever reaches, so
//                they leave the running value as it was; what they are for is their delay RAM. The
//                five reads of slots 0 to 4 at 0x003, 0x007, 0x00b, 0x00f and 0x013 are the reverb's
//                early reflection slots (rom::ER_BASE_SLOTS), and each feeds the capture five steps
//                later into m[56], m[57], m[58], m[59] and m[60].
//   0x03a-0x03d, 0x042-0x04b, 0x050-0x058, 0x05f-0x061, 0x089-0x08d, 0x092-0x096, 0x0d8-0x0df,
//   0x0e0-0x0e7  the aux window's eight differences, which aux.cpp names for that program: one run
//                each for r[46], r[34], r[44], r[32], r[45], r[33], r[43] and r[31], the eight smoothers'
//                difference registers (the diff column of its UNITS table), taken into the running
//                value and scaled by a route of 8, 16 or 4 into r[3] to r[8] and r[121] to r[124]. Each
//                run starts by clearing the running value, 0x03a and 0x042 excepted: those two multiply
//                a cell by a word the host leaves at zero.
//   0x066-0x074  the master EQ's first three bands on one channel and 0x0b6-0x0c4 the same three on
//                the other: low (rom::mix::EQ_LOW {0x6a,0x67,0x66,0x68,0x69}), mid (EQ_MID) and high
//                (EQ_HIGH), each an order-2 section of five steps, the words copied to the second
//                channel's steps by masterEq's copyCoefs. The shape is the same in all three. The -a1
//                step puts -a1 times the output delay into the running value, the b1 step adds b1 times
//                the input's second delay, the b2 step adds b2 times its first delay and carries the
//                rcopy that shifts the output delay along, the b0 step writes the section's input
//                scaled by the route into that input delay, and the -a2 step adds -a2 times the last
//                delay. Registers r[23] to r[30] on the first channel and r[35] to r[42] on the
//                second. The two steps that read the delay RAM are the reverb's slots 25 to 30 on the
//                first channel and the insertion's 46 to 50 on the second.
//   0x075-0x07e, 0x0c5-0x0ce  the other two bands' word steps, EQ_LOW_MID and EQ_HIGH_MID and their
//                copies. All twenty are class-0 steps with no adder, shift, clamp or sub, so they
//                compute nothing: they move the x latch and, where the field says so, a delay RAM read
//                or an m cell move (m[14], m[15], m[18] to m[21], m[56], m[57], m[59], m[60], m[63], and
//                two LFOs). Their words are read at 0x008 to 0x00b above, and the copies' at no step
//                here.
//   0x09b-0x0a2  the DC blocker, rom::mix::MIXER_HPF {0xa2,0xa1,0xa0} and its copy {0x9e,0x9d,0x9c},
//                one order-1 section per channel. 0x09f reads the first channel's input out of the
//                delay RAM at slot 39 into r[19], 0x09b writes the second's into slot 38 out of the
//                capture at 0x09c, and -a1, b1 and b0 follow in each half.
//   0x0cf-0x0d0  the master volume, rom::mix::MIXER_B[14] and [15], which mixerParam's MIX_VOLUME case
//                writes as v * v * masterLevel / 127^2, onto r[29] and r[41] by the route's 16 into
//                m[16] and m[17]: the two master outputs, m[16] on the left DAC at 0x002 and m[17] on
//                the right one at the aux window's 0x10e.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] two of the summed
// aux differences and r[9] and r[10] two more; r[11] to r[18] the panned sends; r[19] and r[20],
// r[21] and r[22] the windows' returns, which no step here writes; r[23] to r[30] the first channel's
// EQ delays and r[35] to r[42] the second's; r[31] to r[34] and r[43] to r[46] the aux window's eight
// differences; r[119] to r[124] the summed differences and the weighted returns. Of the cells,
// m[16] and m[17] are the two outputs, m[6] and m[23] the serial inputs of page 0, and m[56] to m[60]
// the early reflection taps this program captures; the rest are moved once each and read by a window.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no word the host moves is
// folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in yss236_core::step()'s order. The old model in vop3_effects.h runs this as a
// float mixer of pans, returns and sends into three Biq bands; its numbers were fitted to recordings
// and nothing here is one of them.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The 138 steps, one case each. The loop is one iteration per step and carries the m_skip guard
// step() opens with, so a cskip in the reverb's steps skips these as it would the interpreter's.
// There is no gate in this program; the skips come from the reverb's steps around them.
void base_steps(yss236_core& c, int from, int to)
{
	double* const r = c.r;             // the 128 registers
	double* const m = c.m;             // the 64 cells, which carry the values between the steps
	double xv, m2, prod, a, y, outv;   // the step's operands and its result

	for (int st = from; st < to; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{

	case 0x000: // k = 0x5a82
		// the aux pan's left word (rom::mix::MIXER_A[0]) on r[11]; m[6] takes the page 0 serial input
		c.queue_d(6, c.in[0][0]); // the serial input into the cell
		xv = r[11];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(11, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x001: // k = 0x5a82
		// the aux pan's right word (MIXER_A[1]) on r[12]; m[23] takes the second bank, which nothing writes
		c.queue_d(23, c.in[1][0]); // the second serial bank into the cell
		xv = r[12];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(12, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x002: // k = 0x5a82
		// the insertion pan's left word (MIXER_A[2]) on r[13], and the left DAC: out[0] = m[16]
		c.out[0] = m[16];
		xv = r[13];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[16];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(13, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x003: // k = 0x5a82
		// the insertion pan's right word (MIXER_A[3]) on r[14], and the read of slot 0
		// the delay RAM read of this step: slot 0, pushed on the capture history
		c.push_hist(3, c.ram[c.addr(0, 0, false)]);
		xv = r[14];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(14, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x004: // k = 0x0000
		// the reverb delay's left pan (MIXER_A[4]) on r[15]
		xv = r[15];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(15, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x005: // k = 0x0000
		// the reverb delay's right pan (MIXER_A[5]) on r[16]
		xv = r[16];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(16, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x006: // k = 0x7fff
		// the variation's left pan (MIXER_A[6]) on r[17]; m[8] takes the LFO of page 0
		c.queue_d(8, c.lfo[0]); // the LFO of this step's page
		xv = r[17];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(17, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x007: // k = 0x0000
		// the variation's right pan (MIXER_A[7]) on r[18], and the read of slot 1
		// the delay RAM read of this step: slot 1, pushed on the capture history
		c.push_hist(7, c.ram[c.addr(1, 0, false)]);
		xv = r[18];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(18, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x008: // k = 0x5a82
		// the high-mid band's b1 (0x7b) on r[22] into r[123]; m[56] captures the read of slot 0 at 0x003
		c.queue_d(56, c.captured(m[56], 8)); // the latest RAM read 5..11 steps back
		xv = r[22];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(123, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x009: // k = 0x5a82
		// the high-mid band's b2 (0x7c) on r[22] into r[124]
		xv = r[22];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(124, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x00a: // k = 0x5a82
		// mixer 12's left pan (MIXER_A[8]) on r[20] into r[121]
		xv = r[20];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(121, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x00b: // k = 0x5a82
		// mixer 12's right pan (MIXER_A[9]) on r[20] into r[122], and the read of slot 2
		// the delay RAM read of this step: slot 2, pushed on the capture history
		c.push_hist(11, c.ram[c.addr(2, 0, false)]);
		xv = r[20];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(122, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x00c: // k = 0x2d5a
		// mixer 4's level (MIXER_C[13]) times r[14]; m[57] captures the read of slot 1 at 0x007
		c.queue_d(57, c.captured(m[57], 12)); // the latest RAM read 5..11 steps back
		xv = r[14];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x00d: // k = 0x0000
		// a word no host write reaches: the running value passes on
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x00e: // k = 0x0000
		// another one: the running value passes on
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x00f: // k = 0x0000
		// and the read of slot 3
		// the delay RAM read of this step: slot 3, pushed on the capture history
		c.push_hist(15, c.ram[c.addr(3, 0, false)]);
		xv = r[18];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x010: // k = 0x0000
		// another one; m[58] captures the read of slot 2 at 0x00b
		c.queue_d(58, c.captured(m[58], 16)); // the latest RAM read 5..11 steps back
		xv = r[12];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x011: // k = 0x0000
		// mixer 7's level (MIXER_C[8]) times r[16] joins the sum
		xv = r[16];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x012: // k = 0x0000
		// r[120] = 4 * the sum: the send to the reverb and the variation, right pan
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(120, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x013: // k = 0x2d5a
		// mixer 4's level times r[13], and the read of slot 4
		// the delay RAM read of this step: slot 4, pushed on the capture history
		c.push_hist(19, c.ram[c.addr(4, 0, false)]);
		xv = r[13];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x014: // k = 0x0000
		// another one; m[59] captures the read of slot 3 at 0x00f
		c.queue_d(59, c.captured(m[59], 20)); // the latest RAM read 5..11 steps back
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x015: // k = 0x0000
		// another one: the running value passes on
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x016: // k = 0x0000
		// another one: the running value passes on
		xv = r[17];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x017: // k = 0x0000
		// and the read of slot 5, the reverb's sixth early reflection tap
		// the delay RAM read of this step: slot 5, pushed on the capture history
		c.push_hist(23, c.ram[c.addr(5, 0, false)]);
		xv = r[11];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x018: // k = 0x0000
		// mixer 7's level times r[15]; m[60] captures the read of slot 4 at 0x013, and the left pans' running value from 0x013 is dropped
		c.queue_d(60, c.captured(m[60], 24)); // the latest RAM read 5..11 steps back
		xv = r[15];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x019: // k = 0x0000
		// r[119] = 4 * mixer 7's level times r[15], the same send, left pan
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[1];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(119, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x03a: // k = 0x0000
		// the running value is cleared: zero times a cell, and the word at this step is never written
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[23];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x03b: // k = 0x0000
		// the running value passes on, and the read of slot 14
		// the delay RAM read of this step: slot 14, pushed on the capture history
		c.push_hist(59, c.ram[c.addr(14, 0, false)]);
		xv = r[16];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x03c: // k = 0x0000
		// the running value passes on
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x03d: // k = 0x0000
		// r[4] = 8 * the word at this step times r[124]
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(4, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x042: // k = 0x0000
		// the running value is cleared again
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[5];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x043: // k = 0x0000
		// the read of slot 16
		// the delay RAM read of this step: slot 16, pushed on the capture history
		c.push_hist(67, c.ram[c.addr(16, 0, false)]);
		c.queue_d(5, c.in[0][2]); // the serial input into the cell
		xv = r[15];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x044: // k = 0x0000
		// the running value passes on; m[60] takes the second serial bank of page 2
		c.queue_d(60, c.in[1][2]); // the second serial bank into the cell
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x045: // k = 0x0000
		// out[2] = m[0], the window's own serial output
		c.out[2] = m[40];
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[40];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(3, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x046: // k = 0x7fff
		// the aux window's unit 7 difference, r[46], into the running value
		xv = r[46];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x047: // k = 0x0000
		// the running value passes on; m[10] takes the LFO of page 1, and the read of slot 17
		// the delay RAM read of this step: slot 17, pushed on the capture history
		c.push_hist(71, c.ram[c.addr(17, 0, false)]);
		c.queue_d(10, c.lfo[2]); // the LFO of this step's page
		xv = r[16];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x048: // k = 0x0000
		// the running value passes on; m[57] takes the latest RAM read 5..11 steps back
		c.queue_d(57, c.captured(m[57], 72)); // the latest RAM read 5..11 steps back
		xv = r[12];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x049: // k = 0x0000
		// the running value passes on
		xv = r[18];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x04a: // k = 0x0000
		// the running value passes on
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x04b: // k = 0x0000
		// r[4] = 8 * the word here times r[124], and the write of slot 18
		// the delay RAM read of this step: slot 18, pushed on the capture history
		c.push_hist(75, c.ram[c.addr(18, 0, false)]);
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(4, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x050: // k = 0x7fff
		// the aux window's unit 6 difference, r[34]; m[59] captures the latest RAM read
		c.queue_d(59, c.captured(m[59], 80)); // the latest RAM read 5..11 steps back
		xv = r[34];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x051: // k = 0x0000
		// the running value passes on
		xv = r[15];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x052: // k = 0x0000
		// the running value passes on
		xv = r[11];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x053: // k = 0x0000
		// the running value passes on, and the read of slot 20
		// the delay RAM read of this step: slot 20, pushed on the capture history
		c.push_hist(83, c.ram[c.addr(20, 0, false)]);
		xv = r[17];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x054: // k = 0x0000
		// the running value passes on; m[63] takes the latest RAM read
		c.queue_d(63, c.captured(m[63], 84)); // the latest RAM read 5..11 steps back
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x055: // k = 0x0000
		// r[3] = 8 * the word here times r[123]
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(3, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x056: // k = 0x7fff
		// the aux window's unit 3 difference, r[44], into the running value
		xv = r[44];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x057: // k = 0x0000
		// the running value passes on, and the write of slot 21
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl) { outv = c.wlatch; c.haswl = false; } // wdata made another step's output the data
		c.ram[c.addr(21, 0, false)] = outv; // the delay RAM write of this step
		break;

	case 0x058: // k = 0x0000
		// r[8] = 8 * the word here times r[124]
		c.queue_d(56, c.captured(m[56], 88)); // the latest RAM read 5..11 steps back
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(8, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x05f: // k = 0x7fff
		// the aux window's unit 2 difference, r[32], and the read of slot 23
		// the delay RAM read of this step: slot 23, pushed on the capture history
		c.push_hist(95, c.ram[c.addr(23, 0, false)]);
		xv = r[32];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x060: // k = 0x0000
		// the running value passes on; m[56] takes the latest RAM read
		c.queue_d(56, c.captured(m[56], 96)); // the latest RAM read 5..11 steps back
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x061: // k = 0x0000
		// r[7] = 16 * the word here times r[123]
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(7, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x066: // k = 0x0000
		// the low band's -a1 (0x66) times the output delay r[24]
		xv = r[24];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x067: // k = 0xe05c
		// the low band's b1 (0x67) times r[23] joins it; m[11] takes the LFO of page 3
		// the delay RAM read of this step: slot 25, pushed on the capture history
		c.push_hist(103, c.ram[c.addr(25, 0, false)]);
		c.queue_d(11, c.lfo[3]); // the LFO of this step's page
		xv = r[23];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x068: // k = 0x1f6d
		// the low band's b2 (0x68) times r[25] joins it, and r[24] takes the rcopy
		xv = r[25];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(24, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x069: // k = 0x0000
		// the low band's -a2 (0x69) times r[26] joins it, and r[26] takes the rcopy
		xv = r[26];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(26, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x06a: // k = 0x1fc9
		// the low band's b0 (0x6a) times r[9] by the route's 4, into r[25]: the new input
		xv = r[9];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(25, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x06b: // k = 0x1727
		// the mid band's -a1 (0x6b) times r[26], and r[23] takes the rcopy; the read of slot 26
		// the delay RAM read of this step: slot 26, pushed on the capture history
		c.push_hist(107, c.ram[c.addr(26, 0, false)]);
		xv = r[26];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(23, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x06c: // k = 0xce25
		// the mid band's b1 (0x6c) times r[25] joins it; m[56] takes the latest RAM read
		c.queue_d(56, c.captured(m[56], 108)); // the latest RAM read 5..11 steps back
		xv = r[25];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x06d: // k = 0x31db
		// the mid band's b2 (0x6d) times r[27] joins it
		xv = r[27];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x06e: // k = 0xee24
		// the mid band's -a2 (0x6e) times r[28] joins it, and r[28] takes the rcopy
		xv = r[28];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(28, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x06f: // k = 0x1ab5
		// the mid band's b0 (0x6f) times r[25] by the route's 4 into r[27], and the read of slot 27
		// the delay RAM read of this step: slot 27, pushed on the capture history
		c.push_hist(111, c.ram[c.addr(27, 0, false)]);
		xv = r[25];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(27, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x070: // k = 0x01ce
		// the high band's -a1 (0x70) times r[28]; m[57] captures the latest RAM read
		c.queue_d(57, c.captured(m[57], 112)); // the latest RAM read 5..11 steps back
		xv = r[28];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x071: // k = 0xfc20
		// the high band's b1 (0x71) times r[27] joins it
		xv = r[27];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x072: // k = 0x0f70
		// the high band's b2 (0x72) times r[29] joins it
		xv = r[29];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x073: // k = 0xf88d
		// the high band's -a2 (0x73) times r[30] joins it, and r[30] takes the rcopy; the read of slot 28
		// the delay RAM read of this step: slot 28, pushed on the capture history
		c.push_hist(115, c.ram[c.addr(28, 0, false)]);
		xv = r[30];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(30, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x074: // k = 0x0215
		// the high band's b0 (0x74) times r[27] by the route's 16 into r[29]
		c.queue_d(58, c.captured(m[58], 116)); // the latest RAM read 5..11 steps back
		xv = r[27];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(29, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x075: // k = 0x1ce8
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x076: // k = 0xc340
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x077: // k = 0x3cc0
		// the delay RAM read of this step: slot 29, pushed on the capture history
		c.push_hist(119, c.ram[c.addr(29, 0, false)]);
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x078: // k = 0xe318
		c.queue_d(59, c.captured(m[59], 120)); // the latest RAM read 5..11 steps back
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x079: // k = 0x2000
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x07a: // k = 0x0e32
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x07b: // k = 0xd91b
		// the delay RAM read of this step: slot 30, pushed on the capture history
		c.push_hist(123, c.ram[c.addr(30, 0, false)]);
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x07c: // k = 0x26e5
		c.queue_d(60, c.captured(m[60], 124)); // the latest RAM read 5..11 steps back
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x07d: // k = 0xf1ce
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x07e: // k = 0x2000
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x089: // k = 0x7fff
		// the aux window's unit 5 difference, r[45]
		xv = r[45];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x08a: // k = 0x0000
		// the running value passes on
		xv = r[14];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x08b: // k = 0x0000
		// the running value passes on, and the read of slot 34
		// the delay RAM read of this step: slot 34, pushed on the capture history
		c.push_hist(139, c.ram[c.addr(34, 0, false)]);
		xv = r[18];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x08c: // k = 0x0000
		// the running value passes on; m[57] takes the latest RAM read
		c.queue_d(57, c.captured(m[57], 140)); // the latest RAM read 5..11 steps back
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x08d: // k = 0x0000
		// r[6] = 16 * the word here times r[124]
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(6, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x092: // k = 0x7fff
		// the aux window's unit 4 difference, r[33]
		xv = r[33];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x093: // k = 0x0000
		// the running value passes on, and the read of slot 36
		// the delay RAM read of this step: slot 36, pushed on the capture history
		c.push_hist(147, c.ram[c.addr(36, 0, false)]);
		xv = r[13];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x094: // k = 0x0000
		// the running value passes on
		xv = r[17];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x095: // k = 0x0000
		// the running value passes on
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x096: // k = 0x0000
		// r[5] = 16 * the word here times r[123]
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(5, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x09b: // k = 0x2000
		// the DC blocker's first channel: 16 * 4 * the word here times m[58], into r[21] and the write of slot 38
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(21, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl) { outv = c.wlatch; c.haswl = false; } // wdata made another step's output the data
		c.ram[c.addr(38, 0, false)] = outv; // the delay RAM write of this step
		break;

	case 0x09c: // k = 0x7fa3
		// its -a1 (0x9c) times r[22]; m[58] takes the latest RAM read
		c.queue_d(58, c.captured(m[58], 156)); // the latest RAM read 5..11 steps back
		xv = r[22];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x09d: // k = 0x802f
		// its b1 (0x9d) times r[21] joins it
		xv = r[21];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x09e: // k = 0x7fd1
		// its b0 (0x9e) times r[21] joins it, into r[22]
		xv = r[21];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(22, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x09f: // k = 0x2000
		// the copy's input: 16 * 4 * the word here times m[7], into r[19], and the read of slot 39
		// the delay RAM read of this step: slot 39, pushed on the capture history
		c.push_hist(159, c.ram[c.addr(39, 0, false)]);
		xv = 0.0; // no register
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[7];
		m2 = c.src; // the multiplier's second input is the cell
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(19, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0a0: // k = 0x7fa3
		// the copy's -a1 (0xa0) times r[20]
		xv = r[20];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0a1: // k = 0x802f
		// the copy's b1 (0xa1) times r[19] joins it
		xv = r[19];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0a2: // k = 0x7fd1
		// the copy's b0 (0xa2) times r[19] joins it, into r[20]
		xv = r[19];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(20, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0b6: // k = 0x0000
		// the low band's -a1 on the second channel, the copied word 0xb6, times r[36]
		xv = r[36];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0b7: // k = 0xe05c
		// the low band's b1 (0xb7) times r[35] joins it
		xv = r[35];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0b8: // k = 0x1f6d
		// the low band's b2 (0xb8) times r[37] joins it, and r[36] takes the rcopy
		xv = r[37];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(36, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0b9: // k = 0x0000
		// the low band's -a2 (0xb9) times r[38], and r[38] takes the rcopy; the read of slot 46
		// the delay RAM read of this step: slot 46, pushed on the capture history
		c.push_hist(185, c.ram[c.addr(46, 0, false)]);
		xv = r[38];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(38, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0ba: // k = 0x1fc9
		// the low band's b0 (0xba) times r[10] by the route's 4 into r[37]
		xv = r[10];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(37, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0bb: // k = 0x1727
		// the mid band's -a1 on the second channel, the copied word 0xbb, times r[38; r[35] takes the rcopy
		xv = r[38];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		c.queue_r(35, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0bc: // k = 0xce25
		// the mid band's -a1 (0xbc) times r[37]
		xv = r[37];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0bd: // k = 0x31db
		// the mid band's b1 (0xbd) times r[39] joins it, and the read of slot 47
		// the delay RAM read of this step: slot 47, pushed on the capture history
		c.push_hist(189, c.ram[c.addr(47, 0, false)]);
		xv = r[39];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0be: // k = 0xee24
		// the mid band's -a2 on the second channel, the copied word 0xbe, times r[40] joins it, r[40] takes the rcopy; m[18] takes the latest RAM read
		c.queue_d(18, c.captured(m[18], 190)); // the latest RAM read 5..11 steps back
		xv = r[40];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(40, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0bf: // k = 0x1ab5
		// the mid band's b0 (0xbf) times r[37] by the route's 4 into r[39]
		xv = r[37];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(39, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0c0: // k = 0x01ce
		// the high band's -a1 (0xc0) times r[40]; m[41] takes the second serial bank of page 6
		c.queue_d(41, c.in[0][6]); // the serial input into the cell
		xv = r[40];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0c1: // k = 0xfc20
		// the high band's b1 (0xc1) times r[39] joins it, and the read of slot 48
		// the delay RAM read of this step: slot 48, pushed on the capture history
		c.push_hist(193, c.ram[c.addr(48, 0, false)]);
		xv = r[39];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0c2: // k = 0x0f70
		// the high band's b2 (0xc2) times r[41] joins it; m[19] takes the latest RAM read
		c.queue_d(19, c.captured(m[19], 194)); // the latest RAM read 5..11 steps back
		xv = r[41];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0c3: // k = 0xf88d
		// the high band's -a2 (0xc3) times r[42], and r[42] takes the rcopy
		xv = r[42];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		c.queue_r(42, c.haspx ? c.px : xv); // the z^-1 copy, not this step's result
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0c4: // k = 0x0215
		// the high band's b0 (0xc4) times r[39] by the route's 16 into r[41]
		xv = r[39];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		c.queue_r(41, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0c5: // k = 0x1ce8
		// the delay RAM read of this step: slot 49, pushed on the capture history
		c.push_hist(197, c.ram[c.addr(49, 0, false)]);
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0c6: // k = 0xc340
		c.queue_d(20, c.captured(m[20], 198)); // the latest RAM read 5..11 steps back
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0c7: // k = 0x3cc0
		c.queue_d(14, c.lfo[6]); // the LFO of this step's page
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0c8: // k = 0xe318
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0c9: // k = 0x2000
		// the delay RAM read of this step: slot 50, pushed on the capture history
		c.push_hist(201, c.ram[c.addr(50, 0, false)]);
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0ca: // k = 0x0e32
		c.queue_d(21, c.captured(m[21], 202)); // the latest RAM read 5..11 steps back
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0cb: // k = 0xd91b
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0cc: // k = 0x26e5
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0cd: // k = 0xf1ce
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0ce: // k = 0x2000
		c.queue_d(56, c.captured(m[56], 206)); // the latest RAM read 5..11 steps back
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
		// class 0 with no adder, shift, clamp or sub: the step is empty
		break;

	case 0x0cf: // k = 0x0000
		// the master volume (MIXER_B[14]) times r[29] by the route's 16 into m[16]: the left output
		xv = r[29];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		y = c.acc = yss236_core::accmode(y, 1);
		c.queue_d(16, yss236_core::clip(y, yss236_core::DSAT)); // this step's own result
		break;

	case 0x0d0: // k = 0x0000
		// the master volume (MIXER_B[15]) times r[41] by the route's 16 into m[17]: the right output
		xv = r[41];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a) * yss236_core::RSCALE[3]; // route 3
		y = c.acc = yss236_core::accmode(y, 1);
		c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT)); // this step's own result
		break;

	case 0x0d8: // k = 0x5a30
		// the aux window's unit 1 difference, r[43]
		xv = r[43];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0d9: // k = 0x0000
		// the running value passes on
		xv = r[10];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0da: // k = 0x0000
		// the running value passes on
		xv = r[14];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0db: // k = 0x0000
		// the running value passes on
		xv = r[16];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0dc: // k = 0x0000
		// the running value passes on
		xv = r[12];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0dd: // k = 0x0000
		// the running value passes on
		xv = r[18];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0de: // k = 0x0000
		// the running value passes on
		xv = r[122];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0df: // k = 0x0000
		// r[10] = 4 * the word here times r[124], the sum
		xv = r[124];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(10, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;

	case 0x0e0: // k = 0x5a30
		// the aux window's unit 0 difference, r[31]
		xv = r[31];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = 0.0; // the adder is off
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e1: // k = 0x0000
		// the running value passes on
		xv = r[9];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e2: // k = 0x0000
		// the running value passes on
		xv = r[13];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e3: // k = 0x0000
		// the running value passes on
		xv = r[15];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e4: // k = 0x0000
		// the running value passes on
		xv = r[11];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e5: // k = 0x0000
		// the running value passes on
		xv = r[17];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e6: // k = 0x0000
		// the running value passes on; m[15] takes the LFO of page 7
		c.queue_d(15, c.lfo[7]); // the LFO of this step's page
		xv = r[121];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a);
		y = c.acc = yss236_core::accmode(y, 0);
		break;

	case 0x0e7: // k = 0x0000
		// r[9] = 4 * the word here times r[123], the sum
		xv = r[123];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[st]) * m2; // class 2, no t input: the step's own word
		a = c.acc; // the running value
		y = (prod + a) * yss236_core::RSCALE[2]; // route 2
		c.queue_r(9, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		break;
		default: break;
		}
	}
}

// The eleven runs of steps the base program owns, in step order.
const int RANGES[][2] = {
	{0x000, 0x01a},
	{0x03a, 0x03e},
	{0x042, 0x04c},
	{0x050, 0x059},
	{0x05f, 0x062},
	{0x066, 0x07f},
	{0x089, 0x08e},
	{0x092, 0x097},
	{0x09b, 0x0a3},
	{0x0b6, 0x0d1},
	{0x0d8, 0x0e8},
};

}  // namespace

// One run of the base program's steps, exactly as yss236_core::step() would run them. The check hands
// this one of the eleven runs above per pass; a range that is not one of them, which no host builds,
// falls back to the interpreter.
void base(yss236_core& c, int from, int to)
{
	for (auto const& rg : RANGES)
		if (rg[0] == from && rg[1] == to) { base_steps(c, from, to); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

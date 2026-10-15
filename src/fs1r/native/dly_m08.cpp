// fs1r/native/dly_m08.cpp - VOP3-2, the reverb delay window's program for the four delay types: its 72 steps.
//
// rom::REVERB_DELAY_PROGRAMS[8], the program the window holds for reverb types 13 to 16 (Delay L,C,R,
// Delay L,R, Echo, Cross Delay). Types 0 to 12 run dly-m00's program in this window instead, under the
// same keys' first entry, and fx_delay.cpp is the host that feeds both. Under these four types the
// DELAY_ROUTES entry (fx_dispatch.cpp) matches and one of EffectHost::delayLCR, delayLR, echo or
// crossDelay runs, so every one of them rewrites this window's constants and delay offsets when a
// parameter moves. Which of the four is running is not a code difference here: the program words are
// the same under all of them and only the constants (rom::rev::DELAY_WORDS and the damp curve) and the
// six delay offsets (rom::rev::DELAY_SLOTS) differ. The code reads them where they live all the same,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in three groups the comments below open each of:
//
//   0x110-0x11d  the two serial inputs and the host's four tone filters, rev::DELAY_LPF, rev::DELAY_HPF
//                and their copies, one three-step one-pole each. r[88] and r[91] are the two first
//                poles, r[89] and r[92] the two second ones, and the second pole of a side is fed from
//                the first. The three reads that ride on the first step of each group (the slots 0x44,
//                0x45 and 0x46, in the reverb's area) are published in m[22], m[23] and m[24].
//   0x11e-0x12f  the four delay taps and the two damped feedbacks. One read per tap, of the slots
//                0x47, 0x48, 0x49 and 0x4A, each taken up by a cell five steps later into m[56], m[57],
//                m[58] and m[59]; 0x128 and 0x12c copy the first two cells into r[93] and r[94] with a
//                class-3 step (unity product, adder off, which is a plain copy), and 0x12a, 0x12b and
//                0x12e, 0x12f put each of them through the host's damp pair, rev::DELAY_CURVE and
//                rev::DELAY_CURVE_COPY, into r[119] and r[120].
//   0x130-0x140  the two line writes, the two wet sums and the dry/wet mix. 0x133 carries w1[13] and
//                is itself a writer, so it writes its own result into the slot 0x4c (rev::DELAY_SLOTS[0],
//                the left line); 0x139 only latches, and the write at 0x13b into the slot 0x4e
//                (rev::DELAY_SLOTS[3]) takes that latch rather than its own result. r[124] and r[125]
//                are the two wet sums, and r[13] and r[14] the block's outputs after the dry/wet mix.
//   0x141-0x157  six steps of nothing (they only move the z^-1 latch), then three more reads of the
//                block's own area (the slots 0x51, 0x52 and 0x53) published in m[25], m[26] and m[27],
//                the LFO values m[10], and the block's output, the cell m[41] to the serial out at
//                0x156.
//
// The registers: r[3] and r[4] the two inputs of the block, r[87] to r[92] the tone filters' inputs
// and states, r[93] and r[94] the first two taps, r[119] and r[120] those two taps damped, r[124] and
// r[125] the two wet sums, r[13] and r[14] the outputs. The cells: m[1] and m[5] the two serial inputs
// (which no step of this program reads; the filters work on r[3] and r[4] instead), m[8], m[9] and m[10]
// the LFO values it hands on, m[22] to m[27] the six reads it publishes, m[41] the block's output, m[56]
// to m[59] the four taps. There is no t register anywhere in this program: no step has tw or tfrac set,
// and no step takes a multiplier from the t file.
//
// A delay RAM read is made at one step and taken up by a cell 5 to 11 steps later, which is what
// yss236_core::captured() returns and what an msrc of 4 queues. Every read here is five steps ahead of
// its cell, so each capture takes the read its window names. The coefficients are spread one to a step
// rather than one to a group, which is how the host gets a word of its own for each of them: the two
// halves of a damp pair multiply the same operand, the pole of a tone filter is one step and its gain
// the next, and the dry/wet mix is the same three-step shape with a multiply by four instead of two.
//
// Every step is one case of the switch, in the order yss236_core::step() runs its own arithmetic, with
// the step's fixed fields folded in. Constants come from c.coef[st] on the pass and delay offsets
// through c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is
// folded in here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x110 to 0x157, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's.
void block_0x110(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double xv, a, m2, prod, y, outv, k; // the step's operands and its result

	for (int st = 0x110; st < 0x158; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x110 to 0x11d, the two inputs and the four tone filters ---------------------------------------
		//
		// r[3] and r[4] are the two serial inputs of the block, written by the base program before this
		// window's steps. The host's four word lists hold one one-pole each, three words apiece, and each
		// pole is three steps: rev::DELAY_LPF (0x111, 0x112, 0x113) is the left channel's first pole,
		// rev::DELAY_HPF (0x114, 0x115, 0x116) its second, and rev::DELAY_LPF_COPY and rev::DELAY_HPF_COPY
		// (0x118 to 0x11a and 0x11b to 0x11d) the right channel's two. EffectHost::designFilter writes the
		// three words of a one-pole as b0, b1 and -a1, and the program takes them one to a step, the other
		// way round:
		//
		//     0x111  the running value is (-a1) * state
		//     0x112  plus b1 * x
		//     0x113  plus b0 * x, all of it times two, and that is the new state in r[88]
		//
		// so the pole reads y = b0 * x + b1 * x' + (-a1) * y', the second pole of a side fed from the first
		// (r[89] from r[88], r[92] from r[91]). The words are scaled by 2048 * (16 >> 1), half the
		// register's span, which is what that multiply by two puts back. The three reads that ride on the
		// first step of each group are the block's own three lines, and go out in m[22], m[23] and m[24].
		case 0x110:  // the left input in: m[1] from serial bank 0 page 8, and r[87] = 2 * (0.5 * r[3])
				c.queue_d(1, c.in[0][8]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[3];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 2.0;
				c.queue_r(87, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x111:  // rev::DELAY_LPF[2]: m[5] from serial bank 2 page 8, then pole * state
				c.queue_d(5, c.in[1][8]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[88];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x112:  // rev::DELAY_LPF[1]: -pole * x onto the running value, which 0x113 folds into r[88]
				k = yss236_core::s16(c.coef[st]);
				xv = r[87];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x113:  // rev::DELAY_LPF[0]: the first pole's state, and a read of the slot 0x44
				c.push_hist(st, c.ram[c.addr(0x44, 0, false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[87];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 2.0;
				c.queue_r(88, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x114:  // rev::DELAY_HPF[2]: pole * state of the second pole
				k = yss236_core::s16(c.coef[st]);
				xv = r[89];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x115:  // rev::DELAY_HPF[1]: -pole * x onto the running value, which 0x116 folds into r[89]
				k = yss236_core::s16(c.coef[st]);
				xv = r[88];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x116:  // rev::DELAY_HPF[0]: the second pole's state, into r[89]
				k = yss236_core::s16(c.coef[st]);
				xv = r[88];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(89, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x117:  // the right input: m[8] is the LFO 8 for another window, r[90] = 2 * (0.5 * r[4]), and a read of the slot 0x45
				c.push_hist(st, c.ram[c.addr(0x45, 0, false)]);
				c.queue_d(8, c.lfo[8]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[4];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 2.0;
				c.queue_r(90, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x118:  // rev::DELAY_LPF_COPY[2]: m[22] takes up the read of 0x113, then pole * state
				c.queue_d(22, c.captured(c.m[22], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[91];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x119:  // rev::DELAY_LPF_COPY[1]: -pole * x onto the running value
				k = yss236_core::s16(c.coef[st]);
				xv = r[90];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11a:  // rev::DELAY_LPF_COPY[0]: the copy's first pole state, into r[91]
				k = yss236_core::s16(c.coef[st]);
				xv = r[90];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 2.0;
				c.queue_r(91, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11b:  // rev::DELAY_HPF_COPY[2]: a read of the slot 0x46, then pole * state of the copy's second pole
				c.push_hist(st, c.ram[c.addr(0x46, 0, false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11c:  // rev::DELAY_HPF_COPY[1]: m[23] takes up the read of 0x117, then -pole * x onto the running value
				c.queue_d(23, c.captured(c.m[23], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[91];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11d:  // rev::DELAY_HPF_COPY[0]: the copy's second pole state, into r[92]
				k = yss236_core::s16(c.coef[st]);
				xv = r[91];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(92, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x11e to 0x12f, the four taps and the two damped feedbacks ------------------------------------
		//
		// The six slots rom::rev::DELAY_SLOTS names are {0x4c, 0x47, 0x49, 0x4e, 0x48, 0x4a}, and the host
		// gives each of the four delay types its own offsets into them (fx_delay.cpp: delayLCR, delayLR,
		// echo, crossDelay). This program reads four of the six and writes the other two. A read is made
		// on one step and the cell that takes it up is five steps later, so the four taps arrive as:
		//
		//   0x11f  the slot 0x47  ->  m[56] at 0x124
		//   0x123  the slot 0x48  ->  m[57] at 0x128
		//   0x127  the slot 0x49  ->  m[58] at 0x12c   (m[9] is the LFO 9 for another window)
		//   0x12b  the slot 0x4a  ->  m[59] at 0x130
		//
		// Under Delay L,C,R the four are the feedback, right, left and centre taps of the one line; under
		// Echo they are the two taps of each of the two lines; under Cross Delay they are the two feeds
		// across. Which is which is the host's offsets, not this program's.
		//
		// 0x128 and 0x12c are class-3 steps: a unity product with the adder off, which is step()'s plain
		// copy, and they are what puts m[56] into r[93] and m[57] into r[94]. The damp follows, as the
		// host's pair of words (rev::DELAY_CURVE, and rev::DELAY_CURVE_COPY for the other tap), one word
		// per step on the same operand: EffectHost::storeHighDamp writes c0 = k * DB[12] and
		// c1 = (1 - k) * DB[12] to 0x12b and 0x12a, so r[119] = (c0 + c1) * r[93] and r[120] likewise of
		// r[94]. Under Delay L,C,R the copy is not written and stays at the ROM's zero.
		case 0x11e:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x11f:  // the first tap, the slot 0x47
				c.push_hist(st, c.ram[c.addr(0x47, 0, false)]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x120:  // m[24] takes up the read of 0x11b, for a window outside this one
				c.queue_d(24, c.captured(c.m[24], st));
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x121:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x122:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x123:  // the second tap, the slot 0x48
				c.push_hist(st, c.ram[c.addr(0x48, 0, false)]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x124:  // m[56] takes up the read of 0x11f, the first tap
				c.queue_d(56, c.captured(c.m[56], st));
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x125:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x126:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x127:  // the third tap, the slot 0x49, and m[9] for another window
				c.push_hist(st, c.ram[c.addr(0x49, 0, false)]);
				c.queue_d(9, c.lfo[9]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x128:  // m[57] takes up the read of 0x123, and r[93] = m[56] (class 3, a plain copy)
				c.queue_d(57, c.captured(c.m[57], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[56];
				m2 = c.src;
				prod = m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(93, yss236_core::mode(y, 0));
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x129:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x12a:  // rev::DELAY_CURVE[1], the low half of the damp pair, onto the running value
				k = yss236_core::s16(c.coef[st]);
				xv = r[93];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12b:  // rev::DELAY_CURVE[0]: the damped first tap into r[119], and a read of the slot 0x4a
				c.push_hist(st, c.ram[c.addr(0x4a, 0, false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[93];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(119, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12c:  // m[58] takes up the read of 0x127, and r[94] = m[57] (class 3, a plain copy)
				c.queue_d(58, c.captured(c.m[58], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[57];
				m2 = c.src;
				prod = m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(94, yss236_core::mode(y, 0));
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12d:  // nothing but the z^-1 latch
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x12e:  // rev::DELAY_CURVE_COPY[1], the low half of the copy's damp pair
				k = yss236_core::s16(c.coef[st]);
				xv = r[94];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12f:  // rev::DELAY_CURVE_COPY[0]: the damped second tap into r[120]
				k = yss236_core::s16(c.coef[st]);
				xv = r[94];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(120, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x130 to 0x140, the two line writes, the two wet sums and the dry/wet mix -------------------
		//
		// The left line is the slot 0x4c (rom::rev::DELAY_SLOTS[0]). Four steps build its value out of the
		// two filtered inputs and the two damped feedbacks, and 0x133 is both the last of them and the
		// writer: it carries w1[13] itself, and step() gives a writer carrying that bit its own result
		// rather than the latch, so what lands in the RAM is r[124]'s own neighbour, the sum in the
		// running value at that step.
		//
		// The right line is the slot 0x4e (rom::rev::DELAY_SLOTS[3]) and is written a different way:
		// 0x137 to 0x139 build the value, 0x139 only latches it (w1[13] with no memory cycle), and 0x13b
		// is the writer, which finds the latch waiting and writes that. So the right line's value is the
		// one 0x139 computed, not the one 0x13b computed.
		//
		// The two wet sums are r[124] at 0x136 and r[125] at 0x13c: the feedback tap at full level, the
		// third tap at the host's level and the fourth at its own. Which level word lands on which step
		// is the host's: rev::DELAY_WORDS[4] and [6] for Delay L,C,R's centre level and Echo's second
		// level, and [5] for the same level on the other side.
		//
		// The dry/wet mix is the last four steps. EffectHost::dryWet(w, d.word(0), ...) writes the two dry
		// levels into rev::DELAY_WORDS[0] and [1], the steps 0x13d and 0x13f, and the two wet levels into
		// [2] and [3], the steps 0x13e and 0x140. The two channels are r[13] and r[14], and they go out
		// of the window from there.
		case 0x130:  // m[59] takes up the read of 0x12b, the fourth tap; the left input's share
				c.queue_d(59, c.captured(c.m[59], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[89];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x131:  // the right input's share, onto the running value
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x132:  // rev::DELAY_WORDS[7], the feedback level, on the damped first tap
				k = yss236_core::s16(c.coef[st]);
				xv = r[119];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x133:  // rev::DELAY_WORDS[9]: the second feedback level, and the write of the left line, the slot 0x4c
				k = yss236_core::s16(c.coef[st]);
				xv = r[120];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
				c.haswl = false;
				if (c.haswl) { outv = c.wlatch; c.haswl = false; }
				c.ram[c.addr(0x4c, 0, false)] = outv;
			break;
		case 0x134:  // the left wet sum's first term: the first tap at full level
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[56];
				m2 = c.src;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x135:  // rev::DELAY_WORDS[4]: Echo's second level on the third tap
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[58];
				m2 = c.src;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x136:  // rev::DELAY_WORDS[6]: the left wet sum, into r[124]
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[59];
				m2 = c.src;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(124, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x137:  // rev::DELAY_WORDS[13], Cross Delay's input select, on the right channel
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x138:  // rev::DELAY_WORDS[8], Echo's right feedback level, on the damped second tap
				k = yss236_core::s16(c.coef[st]);
				xv = r[120];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x139:  // rev::DELAY_WORDS[10], Cross Delay's other feedback level: the value the right line is written with
				k = yss236_core::s16(c.coef[st]);
				xv = r[119];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
				c.wlatch = outv; c.haswl = true;
			break;
		case 0x13a:  // the right wet sum's first term: the second tap at full level
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[57];
				m2 = c.src;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13b:  // rev::DELAY_WORDS[5]: the level on the fourth tap, and the write of the right line, the slot 0x4e, from the latch
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[59];
				m2 = c.src;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
				if (c.haswl) { outv = c.wlatch; c.haswl = false; }
				c.ram[c.addr(0x4e, 0, false)] = outv;
			break;
		case 0x13c:  // the right wet sum: the third tap at full level, into r[125]
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[58];
				m2 = c.src;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(125, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13d:  // rev::DELAY_WORDS[0], the left dry level, on the left filtered input
				k = yss236_core::s16(c.coef[st]);
				xv = r[89];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13e:  // rev::DELAY_WORDS[2], the left wet level, on r[124]: the left output into r[13]
				k = yss236_core::s16(c.coef[st]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(13, yss236_core::mode(y, 0));
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13f:  // rev::DELAY_WORDS[1], the right dry level, on the right filtered input
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x140:  // rev::DELAY_WORDS[3], the right wet level, on r[125]: the right output into r[14]
				k = yss236_core::s16(c.coef[st]);
				xv = r[125];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(14, yss236_core::mode(y, 0));
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;

		// ---- 0x141 to 0x157, the six empty steps, the three more reads and the output -----------------------
		//
		// 0x141 to 0x146 are class-0 steps with no adder, no shift and no clamp and no memory cycle, so
		// step() only moves the z^-1 latch for them, which is what they do here. 0x147 and 0x14b and 0x14f
		// read the block's own area again (the slots 0x51, 0x52 and 0x53) and hand the values on in m[25],
		// m[26] and m[27], and 0x147 takes the LFO 10 for another window. The block's output is the cell
		// m[41] to the serial out at 0x156, the page that step's own half of the program sits in.
		case 0x141:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x142:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x143:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x144:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x145:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x146:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x147:  // a read of the slot 0x51, and m[10] is the LFO 10 for another window
			c.push_hist(st, c.ram[c.addr(0x51, 0, false)]);
			c.queue_d(10, c.lfo[10]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x148:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x149:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14a:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14b:  // a read of the slot 0x52
			c.push_hist(st, c.ram[c.addr(0x52, 0, false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14c:  // m[25] takes up the read of 0x147
			c.queue_d(25, c.captured(c.m[25], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14d:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14e:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x14f:  // a read of the slot 0x53
			c.push_hist(st, c.ram[c.addr(0x53, 0, false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x150:  // m[26] takes up the read of 0x14b
			c.queue_d(26, c.captured(c.m[26], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x151:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x152:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x153:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x154:  // m[27] takes up the read of 0x14f
			c.queue_d(27, c.captured(c.m[27], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x155:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x156:  // the block's output: m[41] to the serial out
			c.out[10] = m[41];
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x157:  // nothing but the z^-1 latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		}
	}
}

}  // namespace

// One run of the reverb delay window's steps, exactly as yss236_core::step() would run them. The
// program is one run; a range no host builds falls back to the interpreter.
void dly_m08(yss236_core& c, int from, int to)
{
	if (from == 0x110 && to == 0x158) { block_0x110(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

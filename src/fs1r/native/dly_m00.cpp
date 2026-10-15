// fs1r/native/dly_m00.cpp - VOP3-2, the reverb delay window's first program: its 72 steps.
//
// Straight transcription of rom::REVERB_DELAY_PROGRAMS[0], the one the window holds for reverb types 0
// to 12 (No Effect, Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate, White Room, Tunnel, Basement,
// Canyon). The program is the delay block's: the same steps as the window's other programs, with the
// constants and delay offsets of the reverb type's preset (rom::REVERB_DELAY_0_BLOCKS) instead of the
// ones fx_delay.cpp writes for types 13 to 16.
//
// Every step below is one case of the switch, in the order yss236_core::step() runs its own arithmetic,
// with the step's fixed fields folded in. Everything the host can rewrite is read where it lives, as
// step() reads it: constants from c.coef[st] on the pass, delay offsets through c.addr() off the
// c.slot[] entry the host last wrote.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x110 to 0x157, the whole of the window's program. The loop is one iteration per step
// and carries the m_skip guard step() opens with, so a gate in another window skips these steps as it
// would the interpreter's.
void block_0x110(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double xv, a, m2, prod, y, outv, v, k; // the step's operands and its result

	for (int st = 0x110; st < 0x158; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		case 0x110:  // the reverb delay block's input: the serial in, scaled
				c.queue_d(1, c.in[0][8]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[12];
				a = c.src;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(124, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x111:  // rev::DELAY_LPF[2], the LPF's feed gain
				c.queue_d(5, c.in[1][8]);
				k = yss236_core::s16(c.coef[st]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[12];
				m2 = c.src;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 16.0;
				c.queue_r(123, yss236_core::mode(y, 0));
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x112:  // rev::DELAY_LPF[1], the LPF's state
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(125, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x113:  // rev::DELAY_LPF[0], the LPF line's RAM read
				c.push_hist(st, c.ram[c.addr(68, 0, false)]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x114:  // rev::DELAY_HPF[0], into t[12]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(124, yss236_core::mode(v, 1));
				c.acc = yss236_core::accmode(v, 1);
				c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x115:  // rev::DELAY_HPF[1]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[12];
				a = c.src;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(1, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x116:  // rev::DELAY_HPF[2]
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = r[123];
				c.psrc = c.src; c.src = m[0];
				a = c.x;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(2, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x117:
				c.push_hist(st, c.ram[c.addr(69, 0, false)]);
				c.queue_d(8, c.lfo[8]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[12] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(127, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x118:  // rev::DELAY_LPF_COPY[2]
				c.queue_d(22, c.captured(c.m[22], st));
				xv = r[1];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x119:  // rev::DELAY_LPF_COPY[1]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(1, yss236_core::mode(v, 1));
				c.acc = yss236_core::accmode(v, 1);
				c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x11a:  // rev::DELAY_LPF_COPY[0]
				xv = r[127];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[12] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(127, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11b:  // rev::DELAY_HPF_COPY[2]
				c.push_hist(st, c.ram[c.addr(70, 0, false)]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[12];
				a = c.src;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(126, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x11c:  // rev::DELAY_HPF_COPY[1]
				c.queue_d(23, c.captured(c.m[23], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 16.0;
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11d:  // rev::DELAY_HPF_COPY[0]
				k = yss236_core::s16(c.coef[st]);
				xv = r[127];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x11e:
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(124, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x11f:
				xv = r[1];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[13] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(119, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x120:
				c.queue_d(24, c.captured(c.m[24], st));
				xv = r[126];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x121:
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(126, yss236_core::mode(v, 1));
				c.acc = yss236_core::accmode(v, 1);
				c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x122:
				xv = r[119];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[13] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(119, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x123:
				k = yss236_core::s16(c.coef[st]);
				xv = r[125];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x124:
				k = yss236_core::s16(c.coef[st]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
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
		case 0x125:
				k = yss236_core::s16(c.coef[st]);
				xv = r[1];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 16.0;
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x126:
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
		case 0x127:
				c.push_hist(st, c.ram[c.addr(73, (c.cur_idx() + 0), false)]);
				c.queue_d(9, c.lfo[9]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(1, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x128:
				xv = r[126];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[12] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(124, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x129:
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = r[123];
				c.psrc = c.src; c.src = m[0];
				a = c.x;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(127, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x12a:  // rev::DELAY_CURVE[1]
				k = yss236_core::s16(c.coef[st]);
				xv = r[2];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12b:  // rev::DELAY_CURVE[0]
				c.push_hist(st, c.ram[c.addr(74, (c.cur_idx() + 1), false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[1];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
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
		case 0x12c:
				c.queue_d(56, c.captured(c.m[56], st));
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = c.t[12] * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				c.queue_r(124, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x12d:
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
		case 0x12e:  // rev::DELAY_CURVE_COPY[1]
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
		case 0x12f:  // rev::DELAY_CURVE_COPY[0]
				c.push_hist(st, c.ram[c.addr(75, (c.cur_idx() + 0), false)]);
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
		case 0x130:  // rev::DELAY_WORDS[11]
				c.queue_d(57, c.captured(c.m[57], st));
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
		case 0x131:  // rev::DELAY_WORDS[12]
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
		case 0x132:  // rev::DELAY_WORDS[7]
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
		case 0x133:  // rev::DELAY_WORDS[9]
				c.push_hist(st, c.ram[c.addr(76, (c.cur_idx() + 1), false)]);
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
		case 0x134:
				c.queue_d(60, c.captured(c.m[60], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[126];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 16.0;
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x135:  // rev::DELAY_WORDS[4]
				k = yss236_core::s16(c.coef[st]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x136:  // rev::DELAY_WORDS[6]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(126, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x137:  // rev::DELAY_WORDS[13]
				c.push_hist(st, c.ram[c.addr(77, (c.cur_idx() + 0), false)]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[56];
				m2 = c.src;
				prod = c.t[9] * m2;
				a = c.src;
				y = (prod - a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x138:  // rev::DELAY_WORDS[8]
				c.queue_d(61, c.captured(c.m[61], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[57];
				m2 = c.src;
				prod = c.t[9] * m2;
				a = c.acc;
				y = (prod - a) * 1.0;
				c.queue_r(119, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x139:  // rev::DELAY_WORDS[10]
				k = yss236_core::s16(c.coef[st]);
				xv = r[127];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 3);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13a:
				k = yss236_core::s16(c.coef[st]);
				xv = r[126];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
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
		case 0x13b:  // rev::DELAY_WORDS[5]
				c.push_hist(st, c.ram[c.addr(78, (c.cur_idx() + 1), false)]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[60];
				m2 = c.src;
				prod = c.t[11] * m2;
				a = c.src;
				y = (prod - a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13c:
				c.queue_d(62, c.captured(c.m[62], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[61];
				m2 = c.src;
				prod = c.t[11] * m2;
				a = c.acc;
				y = (prod - a) * 1.0;
				c.queue_r(121, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x13d:  // rev::DELAY_WORDS[0]
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
		case 0x13e:  // rev::DELAY_WORDS[2]
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
		case 0x13f:  // rev::DELAY_WORDS[1]
				c.push_hist(st, c.ram[c.addr(79, (c.cur_idx() + 0), false)]);
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
		case 0x140:  // rev::DELAY_WORDS[3]
				c.queue_d(63, c.captured(c.m[63], st));
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
		case 0x141:
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
		case 0x142:
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
		case 0x143:
				c.push_hist(st, c.ram[c.addr(80, (c.cur_idx() + 1), false)]);
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
		case 0x144:
				c.queue_d(58, c.captured(c.m[58], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[119];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x145:
				k = yss236_core::s16(c.coef[st]);
				xv = r[89];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x146:
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
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
		case 0x147:
				c.push_hist(st, c.ram[c.addr(81, 0, false)]);
				c.queue_d(10, c.lfo[10]);
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[62];
				m2 = c.src;
				prod = c.t[11] * m2;
				a = c.src;
				y = (prod - a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x148:
				c.queue_d(59, c.captured(c.m[59], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[63];
				m2 = c.src;
				prod = c.t[11] * m2;
				a = c.acc;
				y = (prod - a) * 1.0;
				c.queue_r(122, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x149:
				k = yss236_core::s16(c.coef[st]);
				xv = r[119];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x14a:
				k = yss236_core::s16(c.coef[st]);
				xv = r[121];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x14b:
				c.push_hist(st, c.ram[c.addr(82, 0, false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[122];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(124, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x14c:
				c.queue_d(25, c.captured(c.m[25], st));
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[58];
				m2 = c.src;
				prod = c.t[10] * m2;
				a = c.src;
				y = (prod - a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x14d:
				xv = 0.0;
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
				c.psrc = c.src; c.src = m[59];
				m2 = c.src;
				prod = c.t[10] * m2;
				a = c.acc;
				y = (prod - a) * 1.0;
				c.queue_r(120, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x14e:
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
		case 0x14f:
				c.push_hist(st, c.ram[c.addr(83, 0, false)]);
				k = yss236_core::s16(c.coef[st]);
				xv = r[124];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(13, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x150:
				c.queue_d(26, c.captured(c.m[26], st));
				k = yss236_core::s16(c.coef[st]);
				xv = r[121];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x151:
				k = yss236_core::s16(c.coef[st]);
				xv = r[122];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x152:
				k = yss236_core::s16(c.coef[st]);
				xv = r[120];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 1.0;
				c.queue_r(125, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x153:
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				if (c.haswl) { c.ram[c.addr(84, 0, false)] = c.wlatch; c.haswl = false; }
			break;
		case 0x154:
				c.queue_d(27, c.captured(c.m[27], st));
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
		case 0x155:
				k = yss236_core::s16(c.coef[st]);
				xv = r[125];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[0];
				m2 = xv;
				prod = k * m2;
				a = c.acc;
				y = (prod + a) * 4.0;
				c.queue_r(14, yss236_core::mode(y, 1));
				y = c.acc = yss236_core::accmode(y, 1);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x156:
				c.out[10] = m[41];
				k = yss236_core::s16(c.coef[st]);
				xv = r[92];
				c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
				c.psrc = c.src; c.src = m[41];
				m2 = xv;
				prod = k * m2;
				a = 0.0;
				y = (prod + a) * 1.0;
				y = c.acc = yss236_core::accmode(y, 0);
				outv = yss236_core::clip(y, yss236_core::SAT);
			break;
		case 0x157:
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
				c.ram[c.addr(85, 0, false)] = outv;
			break;
		}
	}
}

}  // namespace

// One run of the reverb delay window's steps, exactly as yss236_core::step() would run them. The
// program is one run; a range no host builds falls back to the interpreter.
void dly_m00(yss236_core& c, int from, int to)
{
	if (from == 0x110 && to == 0x158) { block_0x110(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

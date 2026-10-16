// fs1r/native/ins_m13.cpp - VOP3-2, the insertion window's program for Cmp+DS+Dly, Cmp+OD+Dly,
// Dist+Delay and Odrv+Delay: its 96 steps.
//
// rom::INSERTION_PROGRAMS[13], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 13: ins-26 Cmp+DS+Dly (lsb 1), ins-27 Cmp+OD+Dly
// (lsb 2), ins-29 Dist+Delay (lsb 3) and ins-31 Odrv+Delay (lsb 4), on
// EffectHost::compressorDistortionDelay and EffectHost::distortionDelay. Steps 0x1a0 to 0x1ff are the
// whole program. None of the 96 is a control step, a gate or a cskip, so the only thing the loop below
// carries out of step() is the m_skip guard a gate in the base program leaves behind.
//
// A straight transcription: one block per step, in step order, with each step's arithmetic written out
// for its fixed program words. Every constant is read from c.coef[st] on the pass, so nothing the
// firmware chose is folded in here. Which host table names a step is said on the step.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's. There is no gate or cskip step in this program, so the
// guard is the only control the loop needs.
void block_0x1a0(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double xv, k, m2, prod, a, y, v, outv; // the step's operands and its result

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		case 0x1a0:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a2:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a3:
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a4:
			// the delay RAM read for slot 0x69
			c.push_hist(st, c.ram[c.addr(0x69, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(69, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a8:
			// the delay RAM read for slot 0x6a
			c.push_hist(st, c.ram[c.addr(0x6a, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(71, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(60, yss236_core::clip(y, 128.0));
			break;
		case 0x1a9:
			c.queue_d(56, c.captured(m[56], st));
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ab:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ac:
			// the delay RAM read for slot 0x6b
			c.push_hist(st, c.ram[c.addr(0x6b, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(75, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ad:
			c.queue_d(57, c.captured(m[57], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1af:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b0:
			// the delay RAM read for slot 0x6c
			c.push_hist(st, c.ram[c.addr(0x6c, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 3);
			c.queue_d(60, yss236_core::clip(y, 128.0));
			break;
		case 0x1b1:
			c.queue_d(58, c.captured(m[58], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(125, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[60];
			m2 = xv;
			prod = k * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		case 0x1b4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b5:
			c.queue_d(59, c.captured(m[59], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(126, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[126];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1bb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(42, yss236_core::clip(y, 128.0));
			break;
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bf:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c0:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c3:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cb:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		case 0x1cc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cf:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(79, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(81, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(82, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(83, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(84, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(85, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(43, c.psrc);
			break;
		case 0x1de:
			k = yss236_core::s16(c.coef[st]);
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(86, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e3:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e5:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.haswl = false;
			outv = yss236_core::clip(y, 32.0);
			c.ram[c.addr(0x7a, 0, false)] = outv;
			break;
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ee:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(119, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1ef:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(121, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f0:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f1:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f2:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(124, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f3:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(125, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f4:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(126, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f5:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(127, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[126];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[127];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fe:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(74, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1ff:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m13(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

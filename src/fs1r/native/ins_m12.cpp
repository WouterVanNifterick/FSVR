// fs1r/native/ins_m12.cpp - VOP3-2, the insertion window's program for Wah+DS+Dly and Wah+OD+Dly:
// its 96 steps.
//
// rom::INSERTION_PROGRAMS[12], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 12: ins-18 Wah+DS+Dly and ins-19 Wah+OD+Dly on
// EffectHost::wahDist. Steps 0x1a0 to 0x1ff are the whole program, and every constant in it is read
// from c.coef[st] on the pass, so no number the firmware chose is folded in here: the program words are
// fixed per row and each step's fields are folded into the arithmetic below in the order
// yss236_core::step() runs its own.
//
// Straight transcription: one case per step in step order, nothing named or grouped yet. What it
// computes is in docs/vop3_native/README.md's terms to be filled in once the check says identical.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's. There is no gate or cskip step in this program,
// so the guard is the only control the loop needs.
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
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a3:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a4:
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ab:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ac:
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ad:
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1af:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[44];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(74, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1bc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = c.x;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(56, yss236_core::clip(y, 128.0));
			break;
		case 0x1bf:
			k = yss236_core::s16(c.coef[st]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 3);
			c.queue_d(57, yss236_core::clip(y, 128.0));
			break;
		case 0x1c0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[47];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(58, yss236_core::clip(y, 128.0));
			break;
		case 0x1c3:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[47];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[44];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(59, yss236_core::clip(y, 128.0));
			break;
		case 0x1c5:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[57];
			m2 = xv;
			prod = k * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		case 0x1c7:
			c.push_hist(st, c.ram[c.addr(0x71, (0), false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cb:
			c.push_hist(st, c.ram[c.addr(0x72, (0), false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		case 0x1cc:
			c.queue_d(60, c.captured(m[60], st));
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cf:
			c.push_hist(st, c.ram[c.addr(0x73, (0), false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d0:
			c.queue_d(61, c.captured(m[61], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d2:
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1d3:
			c.push_hist(st, c.ram[c.addr(0x74, (0), false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[12];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d4:
			c.queue_d(62, c.captured(m[62], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1d5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[48];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(48, c.psrc);
			break;
		case 0x1d8:
			c.queue_d(63, c.captured(m[63], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(85, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, 32.0);
			c.haswl = false;
			c.ram[c.addr(0x77, 0, false)] = outv;
			break;
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1de:
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 1);
			c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = c.t[9] * m2;
			a = c.x;
			y = (prod + a) * 1.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[63];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[62];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[61];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[62];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[42];
			m2 = xv;
			prod = c.t[9] * m2;
			a = c.src;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(42, yss236_core::clip(y, 128.0));
			break;
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(82, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ee:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ef:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(83, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(84, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f3:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[46];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[45];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(45, yss236_core::clip(y, 128.0));
			break;
		case 0x1f5:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[47];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(46, c.psrc);
			break;
		case 0x1f6:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[45];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[45];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(47, yss236_core::clip(y, 128.0));
			break;
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(43, yss236_core::clip(y, 128.0));
			break;
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(44, yss236_core::clip(y, 128.0));
			break;
		case 0x1fc:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1fe:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ff:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m12(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

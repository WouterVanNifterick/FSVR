// fs1r/native/ins_m10.cpp - VOP3-2, the insertion window's program for Lo-Fi: its 96 steps.
//
// Straight transcription: one block per step, in step order, with begin_step() and the core's own helpers,
// generated straight from the program words. The header is rewritten once the check is green.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

void block_0x1a0(yss236_core& c)
{
	double* const r = c.r;
	double* const m = c.m;
	double xv, k, m2, prod, a, y, v;

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		case 0x1a0:  // k = 0x0001
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(81, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1a1:  // k = 0x0010
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			c.queue_d(56, yss236_core::clip(c.acc, yss236_core::DSAT));
			break;
		case 0x1a2:  // k = 0x0002
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a3:  // k = 0x0000
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.m_flag = (y > 0) - (y < 0);
			break;
		case 0x1a4:  // k = 0x2000
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 4.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a5:  // k = 0x1fff
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:  // k = 0x1fff
			c.queue_d(3, c.in[0][13]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a7:  // k = 0x0000
			c.queue_d(7, c.in[1][13]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a8:  // k = 0x0000
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a9:  // k = 0x2000
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1aa:  // k = 0x8b38
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ab:  // k = 0x775d
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(71, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ac:  // k = 0x3baf
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(69, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ad:  // k = 0xc60e
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ae:  // k = 0x3bae
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1af:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1b0:  // k = 0x0000
			if (c.m_flag <= 0) c.m_skip = 0x1b4;
			break;
		case 0x1b1:  // k = 0x0000
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(42, yss236_core::clip(y, yss236_core::DSAT));
			break;
		case 0x1b2:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(81, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1b3:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1b4:  // k = 0x8000
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[42];
			m2 = xv;
			prod = k * xv;
			a = c.src;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b5:  // k = 0x0000
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b6:  // k = 0x3fff
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b7:  // k = 0x0000
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[56];
			m2 = xv;
			prod = xv;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.m_flag = (y > 0) - (y < 0);
			break;
		case 0x1b8:  // k = 0x0000
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b9:  // k = 0x3fff
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ba:  // k = 0x0000
			if (c.m_flag <= 0) c.m_skip = 0x1bd;
			break;
		case 0x1bb:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = c.src;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bc:  // k = 0x0000
			c.m_skip = 0x1c4;
			break;
		case 0x1bd:  // k = 0x0000
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[56];
			m2 = xv;
			prod = xv;
			a = c.src;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.m_flag = (y > 0) - (y < 0);
			break;
		case 0x1be:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1bf:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c0:  // k = 0x0000
			if (c.m_flag >= 0) c.m_skip = 0x1c4;
			break;
		case 0x1c1:  // k = 0x8000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * c.src;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c2:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c3:  // k = 0x0000
			c.queue_d(4, c.in[0][14]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c4:  // k = 0x0000
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c5:  // k = 0x0000
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c6:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c7:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c8:  // k = 0x005a
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c9:  // k = 0x0000
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ca:  // k = 0x0000
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cb:  // k = 0x7fff
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cc:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cd:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ce:  // k = 0x1000
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1cf:  // k = 0x3f8c
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d0:  // k = 0xcd58
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d1:  // k = 0x32a8
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d2:  // k = 0x0451
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d3:  // k = 0xdb7f
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d4:  // k = 0x2beb
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(74, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d5:  // k = 0x2481
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d6:  // k = 0xefc4
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d7:  // k = 0xe201
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d8:  // k = 0x1e3a
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d9:  // k = 0x203b
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1da:  // k = 0x0000
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1db:  // k = 0x0000
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dc:  // k = 0x2000
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * xv;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1dd:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1de:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1df:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e0:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e1:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e2:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e3:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e4:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e5:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e6:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e7:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e8:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e9:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ea:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1eb:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ec:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ed:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ee:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ef:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f0:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f1:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f2:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f3:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f4:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f5:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f6:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f7:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f8:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f9:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fa:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fb:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fc:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fd:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fe:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ff:  // k = 0x0000
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		}
	}
}

}  // namespace

void ins_m10(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

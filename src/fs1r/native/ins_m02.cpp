// fs1r/native/ins_m02.cpp - VOP3-2, the insertion window's program for type msb 02: its 96 steps.
//
// rom::INSERTION_PROGRAMS[2], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 2: ins-05 Phaser1 and ins-06 Phaser2, programmed by
// EffectHost::phaser1() and phaser2() in fx_modulation.cpp. Like ins-m01 it is one of the fifteen
// programs the window shares, and the same words run under both types: what the two differ by is
// which coefficients they write, phaser1() its ten stage words and one feedback word, phaser2() the
// same ten, two feedback words and the LFO phase. Nothing here knows which one is running.
//
// Every constant the program reads is read from c.coef[st] on the pass, and the two swept allpass
// coefficients out of t[10] and t[11] on the pass too, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic
// below in the order yss236_core::step() runs its own.
//
// This program touches no delay RAM. Every w0 of its 96 steps is zero, so there is no line, no tap
// and no index anywhere in it: the whole of the phaser is two cascades of allpass sections, which is
// what separates it from ins-m00's and ins-m01's. It fills five cells, m[3] and m[7] from the serial
// banks at page 13, m[4] from page 15, m[13] from page 13's LFO and m[42] from the running value, and
// reads only m[13] and m[42] back.
//
// What the 96 steps compute, in the groups the comments open each of. The names are the host's:
// phaser1() and phaser2() name every step they program, through the tables in rom_tables.cpp.
//   0x1a0-0x1b2  the two signals in and the two shelves. r[7] and r[8] are the two the base program
//                leaves the window; r[67] and r[70] take them, r[68] and r[71] are the low shelves'
//                outputs (ins::LPF_PH[0], [1], [2] and ins::LPF_PH_C[0], [1], [2]) and r[69] and
//                r[72] the high shelves' (ins::HPF_PH[0], [1], [2] and ins::HPF_PH_C[0], [1], [2]).
//                0x1aa and 0x1ab take the window's own LFO cell, m[13], into r[122] and r[123], the
//                two sweep registers.
//   0x1b3-0x1c2  the swept coefficient of each cascade. ins::PH_DEPTH[0] and [1], 0x1b3 and 0x1b5,
//                rectify and scale the LFO the register holds, ins::PH_FB[0] and [1], 0x1b4 and
//                0x1b6, add the phase shift and register it, and 0x1c1 and 0x1c2 multiply the register
//                by itself through t[10] and t[11]: the sweep is squared. 0x1b4, 0x1b6, 0x1c1 and
//                0x1c2 each queue t[10] or t[11] three steps ahead, which is how the sections from
//                0x1d0 on come to read them.
//   0x1b7-0x1c0  the ten stage words, ins::PH_STAGES: five taps of each cascade, r[75] to r[79] and
//                r[82] to r[86], summed a half at a time into r[120] and r[121]. phaser1() and
//                phaser2() open one word of each cascade and leave the other four at zero, so each
//                cascade's output is the one stage the Stage parameter names, the 2 and the half
//                cancelling, and walking that parameter walks the output along the cascade.
//   0x1c3-0x1c6  the diffuse taps, ins::PH_DIR: r[81] and r[85] of the second cascade and its
//                selected stage r[121] into r[119], the signal the left output carries. phaser1()'s
//                Diffuse parameter opens one of the three; phaser2() has none, so under it the ROM's
//                words stand and r[119] is r[81], the second cascade's first section.
//   0x1c7-0x1ca  the tail of the sweep. 0x1c7 and 0x1c9 load the offset, and 0x1c8 and 0x1ca take it
//                off the square of 0x1c1 and queue t[10] and t[11] again, so the coefficient the
//                sections run is the fourth power of the phase shift plus depth term less that offset.
//   0x1cb-0x1ce  the cascade on r[74] to r[79]'s input: r[69] and r[72], the shelves' outputs, come
//                in at a quarter and a half, and the other cascade's selected stage r[121] through
//                ins::PH_WIDTH1[0], the one word phaser1() writes. The matching word for the cascade
//                on r[81] to r[86] is 0x1e2, ins::PH_WIDTH2[1], which adds r[121] to the 0x1e3 that
//                takes r[120], and phaser2() writes ins::PH_WIDTH2[0] at 0x1cd as well, so under it
//                each cascade takes the other's output back: that is the FB Level, and the two
//                cascades feed each other by it.
//   0x1cf-0x1e0  the first cascade, six allpass sections r[74] to r[79] on t[10], three steps
//                each: y = t[10] * x - y a step back, r[74] taking r[73] and r[79] taking r[78]. The
//                class-3 step of each group hands the section's output on as the running value.
//   0x1e1-0x1f3  the second cascade, the same six sections r[81] to r[86] on t[11].
//   0x1f6-0x1f9  the dry and wet mix, ins::PH_WET: r[69] and r[72], the shelves' outputs, at
//                ins::PH_WET[0] and [2] against the wet at [1] and [3], into r[17] and r[18], the two
//                outputs, which the base program reads back.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] and r[70]
// the shelves' inputs, r[68], r[69], r[71] and r[72] the two shelves of each channel; r[122] and
// r[123] the two sweep registers, the LFO at 0x1aa and 0x1ab, the phase shift plus depth term at
// 0x1b4 and 0x1b6 and its square at 0x1c1 and 0x1c2, which 0x1c8 and 0x1ca raise to the fourth power;
// r[73] and r[80] the cascades' inputs and r[74] to r[79] and r[81] to r[86] their twelve sections;
// r[120], r[121] and r[119] each cascade's selected stage and the diffuse tap; r[17] and r[18] the
// two outputs.
//
// The old model in vop3_effects.h has a case for each of these two types, and it is a loop of
// `stages` allpasses whose frequency the LFO sweeps through a tan formula: no delay RAM either, but
// no shelves worth the name, no stage crossfade, no cross-cascade feedback and no diffuse taps. It
// is no reference for any number here, and where the two differ, the program above is what runs.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m02: identical, 2 types, 19200 native passes.

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
	double* const t = c.t;                 // the 16 t registers, the two swept coefficients
	double xv, k, m2, prod, a, y, v;       // the step's operands and its result

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x1a0 to 0x1b2, the two signals in, the two channels' shelves and the LFO into the sweeps -----
		case 0x1a0:  // m[3] takes the left serial input of page 13, for the windows outside this one; acc = 128 * k + 0
			c.queue_d(3, c.in[0][13]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1a1:  // m[7] takes the right serial input of page 13; acc = k * m[42] - acc / 32768, the running value into a cell
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.x = 0.0; c.hasx = false;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a2:  // m[42] takes the value 0x1a1 left in the running value, negated; acc = 128 * k - acc, the same value wrapped
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) - a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			c.queue_d(42, yss236_core::clip(c.acc, yss236_core::DSAT));
			break;
		case 0x1a3:  // r[67] = saturate(2 * (0.5 * r[7] + 0)): the left channel's shelf input
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a4:  // ins::LPF_PH[1], the left low shelf's b1 on its own input
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a5:  // ins::LPF_PH[2], the same shelf's a1 on its own output
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:  // ins::LPF_PH[0]: r[68] = saturate(2 * (0.5 * r[67] + acc)), the left low shelf's output
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a7:  // m[13] takes page 13's LFO, this window's own; ins::HPF_PH[1], the left high shelf's b1
			c.queue_d(13, c.lfo[13]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a8:  // ins::HPF_PH[2], the same shelf's a1 on its own output
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a9:  // ins::HPF_PH[0]: r[69] = saturate(4 * (0.25 * r[68] + acc)), the left high shelf's output
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1aa:  // r[122] = wrap(128 * k + m[13]): the first sweep register, off the window's own LFO cell
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[13];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1ab:  // ins::PH_OFFSET, the word phaser2()'s LFO Phase writes: r[123] = wrap(128 * k + m[13]), the second sweep register
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[13];
			a = c.src;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1ac:  // r[70] = saturate(2 * (0.5 * r[8] + 0)): the right channel's shelf input
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ad:  // ins::LPF_PH_C[1], the right low shelf's b1 on its own input
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ae:  // ins::LPF_PH_C[2], the right low shelf's a1 on its own output
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1af:  // ins::LPF_PH_C[0]: r[71] = saturate(2 * (0.5 * r[70] + acc)), the right low shelf's output
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b0:  // ins::HPF_PH_C[1], the right high shelf's b1
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b1:  // ins::HPF_PH_C[2], the same shelf's a1 on its own output
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b2:  // ins::HPF_PH_C[0]: r[72] = saturate(4 * (0.25 * r[71] + acc)), the right high shelf's output
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1b3 to 0x1c6, the two swept coefficients, the ten stage taps and the diffuse taps ----------
		case 0x1b3:  // ins::PH_DEPTH[0]: acc = |k * r[122]|, the first sweep rectified and scaled by the depth
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1b4:  // ins::PH_FB[0]: r[122] = saturate(128 * k + acc), the phase shift added, and t[10] the same value over 128
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x1b5:  // ins::PH_DEPTH[1]: acc = |k * r[123]|, the second sweep rectified and scaled
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 3);
			break;
		case 0x1b6:  // ins::PH_FB[1]: r[123] = saturate(128 * k + acc), and t[11] the same value over 128
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x1b7:  // ins::PH_STAGES[0]: acc = k * r[75] + 0, the first cascade's second section
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b8:  // ins::PH_STAGES[1]: acc = k * r[76] + acc, its third
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b9:  // ins::PH_STAGES[2]: acc = k * r[77] + acc, its fourth
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ba:  // ins::PH_STAGES[3]: acc = k * r[78] + acc, its fifth
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bb:  // ins::PH_STAGES[4]: r[120] = saturate(2 * (k * r[79] + acc)), its sixth, the first cascade's selected stage
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1bc:  // ins::PH_STAGES[5]: acc = k * r[82] + 0, the second cascade's second section
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bd:  // ins::PH_STAGES[6]: acc = k * r[83] + acc, its third
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1be:  // ins::PH_STAGES[7]: acc = k * r[84] + acc, its fourth
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bf:  // ins::PH_STAGES[8]: acc = k * r[85] + acc, its fifth
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c0:  // ins::PH_STAGES[9]: r[121] = saturate(2 * (k * r[86] + acc)), its sixth, the second cascade's selected stage
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1c1:  // r[122] = saturate(t[10] * r[122]), the first sweep squared, and t[10] that value over 128
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1c2:  // r[123] = saturate(t[11] * r[123]), the second sweep squared, and t[11] that value over 128
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1c3:  // ins::PH_DIR[0]... acc = k * r[120] + 0, off the first cascade's selected stage
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c4:  // ins::PH_DIR[0]: acc = k * r[121] + acc, off the second cascade's selected stage
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c5:  // ins::PH_DIR[1]: acc = k * r[81] + acc, off the second cascade's first section
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c6:  // ins::PH_DIR[2]: r[119] = saturate(2 * (k * r[85] + acc)), off the second cascade's fifth section: the wet signal the left output carries
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1c7 to 0x1ca, the sweep's tail: the cube of the squared sweep less the offset ---------------
		case 0x1c7:  // acc = 128 * k + 0, the offset the sweep is taken off
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1c8:  // acc = t[10] * r[122] - acc: the offset off the square of the square, and t[10] that value over 128
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		case 0x1c9:  // acc = 128 * k + 0, the second sweep's offset
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1ca:  // acc = t[11] * r[123] - acc, and t[11] that value over 128
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;

		// ---- 0x1cb to 0x1ce, the two cascades' inputs: the dry in, and each cascade's own output back ------
		case 0x1cb:  // acc = k * r[69] + 0, a quarter of the left channel's shelves
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cc:  // acc = 2 * (k * r[72] + acc), a half of the right channel's shelves
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1cd:  // ins::PH_WIDTH2[0]: acc = k * r[120] + acc, the cascade on r[74] to r[79]'s selected stage on toward r[73], the other cascade's input
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ce:  // ins::PH_WIDTH1[0]: r[73] = saturate(k * r[121] + acc): the cascade on r[74] to r[79]'s input, off the other cascade's selected stage and the two dry shares
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1cf to 0x1e0, the first cascade: six allpass sections on t[10], three steps each ----------
		case 0x1cf:  // acc = r[73]: r[73], this cascade's input, into the running value
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d0:  // acc = t[10] * r[74] - acc, the first section's output a step back
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d1:  // r[74] = saturate(t[10] * r[73] - acc): the first section, y = t[10] * x - y a step back
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(74, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d2:  // acc = r[74]: the section's output on into the next one
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d3:  // acc = t[10] * r[75] - acc
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d4:  // r[75] = saturate(t[10] * r[74] - acc): the second section
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d5:  // acc = r[75]
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d6:  // acc = t[10] * r[76] - acc
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d7:  // r[76] = saturate(t[10] * r[75] - acc): the third section
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1d8:  // acc = r[76]
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d9:  // acc = t[10] * r[77] - acc
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1da:  // r[77] = saturate(t[10] * r[76] - acc): the fourth section
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1db:  // acc = r[77]
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dc:  // acc = t[10] * r[78] - acc
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dd:  // r[78] = saturate(t[10] * r[77] - acc): the fifth section
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1de:  // acc = r[78]
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1df:  // acc = t[10] * r[79] - acc
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e0:  // r[79] = saturate(t[10] * r[78] - acc): the sixth section, the end of the cascade
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[10] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e1 to 0x1f5, the second cascade: the same six sections on t[11] ---------------------------
		case 0x1e1:  // acc = 2 * (k * r[72] + 0): the running value cleared off the right channel's shelves
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e2:  // ins::PH_WIDTH2[1]: acc = k * r[121] + acc, the cascade on r[81] to r[86]'s own selected stage on toward r[80], its own input
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e3:  // r[80] = saturate(k * r[120] + acc): this cascade's input, the first cascade's selected stage and the second one's own
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e4:  // acc = r[80]: the input into the running value
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e5:  // acc = t[11] * r[81] - acc
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e6:  // r[81] = saturate(t[11] * r[80] - acc): the first section
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1e7:  // acc = r[81]
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e8:  // acc = t[11] * r[82] - acc
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e9:  // r[82] = saturate(t[11] * r[81] - acc): the second section
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(82, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ea:  // acc = r[82]
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1eb:  // acc = t[11] * r[83] - acc
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ec:  // r[83] = saturate(t[11] * r[82] - acc): the third section
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(83, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ed:  // acc = r[83]
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ee:  // acc = t[11] * r[84] - acc
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ef:  // r[84] = saturate(t[11] * r[83] - acc): the fourth section
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(84, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f0:  // acc = r[84]
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f1:  // acc = t[11] * r[85] - acc
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f2:  // r[85] = saturate(t[11] * r[84] - acc): the fifth section
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(85, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f3:  // acc = r[85]: the sixth section's output on into the last one
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f4:  // acc = t[11] * r[86] - acc
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f5:  // r[86] = saturate(t[11] * r[85] - acc): the sixth section, the end of the second cascade
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = t[11] * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(86, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f6 to 0x1f9, the dry and wet mix, the window's two outputs --------------------------------
		case 0x1f6:  // ins::PH_WET[0]: acc = k * r[69] + 0, the dry share of the left output
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f7:  // ins::PH_WET[1]: r[17] = saturate(4 * (k * r[119] + acc)), the wet share added: the window's left output
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f8:  // ins::PH_WET[2]: acc = k * r[72] + 0, the dry share of the right output
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f9:  // ins::PH_WET[3]: r[18] = saturate(4 * (k * r[121] + acc)), the right output
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			k = yss236_core::s16(c.coef[st]);
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1fa to 0x1ff, the last cell, and six steps that only count ---------------------------------
		case 0x1fa:  // m[4] takes the left serial input of page 15, which no step of this program reads
			c.queue_d(4, c.in[0][15]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fb: case 0x1fc: case 0x1fd: case 0x1fe: case 0x1ff:  // class 0, no memory op and no ALU: the steps count and nothing else
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m02(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
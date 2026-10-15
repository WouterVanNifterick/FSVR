// fs1r/native/rev.cpp - VOP3-2, the reverb: its 94 steps of the effect chip's base program.
//
// The reverb block owns no program of its own. Its steps are 94 of the base program's 512, in ten runs
// of the schedule, which rom::STEPS_REVERB lists and the base program leaves to the reverb window: the
// whole block is here, so the rest of the pass belongs to the `base` row. Under it sit the halls and
// rooms the Effect Type List calls types 1 to 12 (Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate,
// White Room, Tunnel, Basement, Canyon), which the firmware tells apart by the constants and the delay
// offsets it writes and not by the program, so one native covers all twelve. Types 13 to 16 of the
// reverb block run their programs in the reverb delay window instead, under the keys `dly-mNN`, so no
// step of theirs is here.
//
// What the 94 steps compute is named from the host that feeds them, in fx_reverb.cpp and the rev: tables
// of rom_tables.cpp, which between them say which constant belongs to which part of the reverb:
//
//   six comb units     rev::HALL_COMB_STEPS, four words each: the gain, two input taps and the feedback
//                      coefficient, the four words EffectHost::combUnit() writes per unit
//   six allpass units  rev::ROW_STEPS, four words each: s28, s24, s26 and s22, the four words
//                      EffectHost::diffusers() writes, with rev::ROW_SLOTS giving each unit's offset
//   the input filters  rev::HPF_STEPS and rev::LPF_STEPS, three words each, copied into three more
//                      steps each by rev::HPF_COPY and rev::LPF_COPY
//   22 early reflections rev::ER_WORDS: the first ten scaled by the ER/Rev balance in
//                      EffectHost::erBalance(), the last twelve set to the complement of it
//   the surround       rev::MIX_STEPS, rev::LEVEL_STEPS, rev::WIDTH_STEPS and the type's shelf words in
//                      rev::TABLE_STEPS
//
// The cells carry the values between the units. A delay RAM read is made at one step and taken up by a
// cell 5 to 11 steps later, which is what yss236_core::captured() returns and what an msrc of 4 queues;
// m[34] to m[40] are a chain of the previous step's own operand (msrc 7), one cell per step, which gives
// each comb unit its second input tap a step after its first. The registers are r[47] to r[54] for the
// input filters, r[55] to r[60] for the six comb units' states, r[119] and r[120] for the two sums the
// surround reads, r[125] for an allpass state, and r[9] and r[10] for the level.
//
// Only the fields the program fixes are written out below. Everything the host rewrites is read where it
// lives, as step() reads it: every constant from c.coef[st] on the pass, every delay offset through
// c.addr(), which reads the c.slot[] entry the host last wrote. No number here is one the firmware
// chose, and the code runs on all twelve reverb types unchanged.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every block below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x01a to 0x039. holds the host words rev::ER_WORDS[0] to [9], rev::MIX_STEPS, rev::WIDTH_STEPS, rev::HPF_COPY, rev::HPF_STEPS, rev::LPF_COPY, rev::LPF_STEPS, rev::TABLE_STEPS; the rest of its steps read the delay RAM and move values between the cells
void block_0x01a(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x01a to 0x039, 32 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x01a  mix word 1 (rev::MIX_STEPS)
		c.begin_step();
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01a]) * m2;
		a = 0.0;
		y = (prod + a) * 2.0;
		c.queue_r(50, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x01b  HPF word 1, the copy (rev::HPF_COPY)
		c.begin_step();
		c.push_hist(0x01b, c.ram[c.addr(6, 0, false)]);
		xv = r[51];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01b]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x01c  HPF word 2, the copy (rev::HPF_COPY)
		c.begin_step();
		c.queue_d(62, c.captured(c.m[62], 0x01c));
		xv = r[50];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01c]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x01d  HPF word 3, the copy (rev::HPF_COPY)
		c.begin_step();
		xv = r[50];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01d]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(51, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x01e  LPF word 1, the copy (rev::LPF_COPY)
		c.begin_step();
		xv = r[52];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01e]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x01f  LPF word 2, the copy (rev::LPF_COPY)
		c.begin_step();
		c.push_hist(0x01f, c.ram[c.addr(7, 0, false)]);
		xv = r[51];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x01f]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x020  LPF word 3, the copy (rev::LPF_COPY)
		c.begin_step();
		c.queue_d(22, c.captured(c.m[22], 0x020));
		xv = r[51];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x020]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(52, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x021  mix word 2 (rev::MIX_STEPS)
		c.begin_step();
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x021]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x022  mix word 0 (rev::MIX_STEPS)
		c.begin_step();
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x022]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(47, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x023  HPF word 1 (rev::HPF_STEPS)
		c.begin_step();
		c.push_hist(0x023, c.ram[c.addr(8, 0, false)]);
		xv = r[48];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x023]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x024  HPF word 2 (rev::HPF_STEPS)
		c.begin_step();
		c.queue_d(61, c.captured(c.m[61], 0x024));
		xv = r[47];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x024]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x025  HPF word 3 (rev::HPF_STEPS)
		c.begin_step();
		xv = r[47];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x025]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(48, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x026  LPF word 1 (rev::LPF_STEPS)
		c.begin_step();
		c.queue_d(9, c.lfo[1]);
		xv = r[49];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x026]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x027  LPF word 2 (rev::LPF_STEPS)
		c.begin_step();
		c.push_hist(0x027, c.ram[c.addr(9, 0, false)]);
		xv = r[48];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x027]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x028  LPF word 3 (rev::LPF_STEPS)
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x028));
		xv = r[48];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x028]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(49, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x029  the width, word 1 (rev::WIDTH_STEPS)
		c.begin_step();
		xv = r[52];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x029]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.wlatch = outv; c.haswl = true;   // the data of the next delay RAM write
		// 0x02a  early reflection word 0 (rev::ER_WORDS[0])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02a]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x02b  early reflection word 1 (rev::ER_WORDS[1])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02b]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl) { outv = c.wlatch; c.haswl = false; }
		c.ram[c.addr(10, 0, false)] = outv;
		// 0x02c  early reflection word 2 (rev::ER_WORDS[2])
		c.begin_step();
		c.queue_d(57, c.captured(c.m[57], 0x02c));
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02c]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x02d  early reflection word 3 (rev::ER_WORDS[3])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[59];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02d]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x02e  early reflection word 4 (rev::ER_WORDS[4])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[60];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02e]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(119, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x02f  mix word 3 (rev::MIX_STEPS)
		c.begin_step();
		c.push_hist(0x02f, c.ram[c.addr(11, 0, false)]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[60];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x02f]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x030  mix word 4 (rev::MIX_STEPS)
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x030]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(53, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x031  the type shelf, word 1 (rev::TABLE_STEPS)
		c.begin_step();
		xv = r[54];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x031]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x032  the type shelf, word 2 (rev::TABLE_STEPS)
		c.begin_step();
		xv = r[53];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x032]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x033  the type shelf, word 3 (rev::TABLE_STEPS)
		c.begin_step();
		c.push_hist(0x033, c.ram[c.addr(12, 0, false)]);
		xv = r[53];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x033]) * m2;
		a = c.acc;
		y = (prod + a) * 4.0;
		c.queue_r(54, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x034  the width, word 0 (rev::WIDTH_STEPS)
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x034));
		xv = r[49];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[60];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x034]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.wlatch = outv; c.haswl = true;   // the data of the next delay RAM write
		// 0x035  early reflection word 5 (rev::ER_WORDS[5])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[61];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x035]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x036  early reflection word 6 (rev::ER_WORDS[6])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x036]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x037  early reflection word 7 (rev::ER_WORDS[7])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x037]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl) { outv = c.wlatch; c.haswl = false; }
		c.ram[c.addr(13, 0, false)] = outv;
		// 0x038  early reflection word 8 (rev::ER_WORDS[8])
		c.begin_step();
		c.queue_d(59, c.captured(c.m[59], 0x038));
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x038]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x039  early reflection word 9 (rev::ER_WORDS[9])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x039]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(120, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x03e to 0x041. holds the host words rev::ROW_STEPS[0] to [3]; the rest of its steps read the delay RAM and move values between the cells
void block_0x03e(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x03e to 0x041, 4 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x03e  allpass 0 s26 (rev::ROW_STEPS[2])
		c.begin_step();
		xv = r[54];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x03e]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x03f  allpass 0 s22 (rev::ROW_STEPS[3])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[22];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x03f]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(15, 0, false)] = outv;
		// 0x040  allpass 0 s28 (rev::ROW_STEPS[0])
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x040));
		xv = r[54];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x040]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x041  allpass 0 s24 (rev::ROW_STEPS[1])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[22];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x041]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(125, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x04c to 0x04f. holds the host words rev::ROW_STEPS[4] to [7]; the rest of its steps read the delay RAM and move values between the cells
void block_0x04c(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x04c to 0x04f, 4 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x04c  allpass 1 s26 (rev::ROW_STEPS[6])
		c.begin_step();
		c.queue_d(58, c.captured(c.m[58], 0x04c));
		xv = r[125];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x04c]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x04d  allpass 1 s22 (rev::ROW_STEPS[7])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[59];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x04d]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.wlatch = outv; c.haswl = true;   // the data of the next delay RAM write
		// 0x04e  allpass 1 s28 (rev::ROW_STEPS[4])
		c.begin_step();
		xv = r[125];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x04e]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x04f  allpass 1 s24 (rev::ROW_STEPS[5])
		c.begin_step();
		c.push_hist(0x04f, c.ram[c.addr(19, 0, false)]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[59];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x04f]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		y = c.acc = yss236_core::accmode(y, 1);
		c.queue_d(62, yss236_core::clip(y, yss236_core::DSAT));


		
}
// The run of steps 0x059 to 0x05e. holds the host words rev::ER_WORDS[10] to [15]; the rest of its steps read the delay RAM and move values between the cells
void block_0x059(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x059 to 0x05e, 6 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x059  early reflection word 10 (rev::ER_WORDS[10])
		c.begin_step();
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x059]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x05a  early reflection word 11 (rev::ER_WORDS[11])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x05a]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x05b  early reflection word 12 (rev::ER_WORDS[12])
		c.begin_step();
		c.push_hist(0x05b, c.ram[c.addr(22, 0, false)]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x05b]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x05c  early reflection word 13 (rev::ER_WORDS[13])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[59];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x05c]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x05d  early reflection word 14 (rev::ER_WORDS[14])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x05d]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x05e  early reflection word 15 (rev::ER_WORDS[15])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[63];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x05e]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(119, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x062 to 0x065. holds the host words rev::ROW_STEPS[8] to [11]; the rest of its steps read the delay RAM and move values between the cells
void block_0x062(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x062 to 0x065, 4 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x062  allpass 2 s26 (rev::ROW_STEPS[10])
		c.begin_step();
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x062]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x063  allpass 2 s22 (rev::ROW_STEPS[11])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x063]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(24, 0, false)] = outv;
		// 0x064  allpass 2 s28 (rev::ROW_STEPS[8])
		c.begin_step();
		c.queue_d(61, c.captured(c.m[61], 0x064));
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x064]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x065  allpass 2 s24 (rev::ROW_STEPS[9])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x065]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(119, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x07f to 0x088. holds the host words rev::ER_WORDS[16] to [21], rev::ROW_STEPS[12] to [15]; the rest of its steps read the delay RAM and move values between the cells
void block_0x07f(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x07f to 0x088, 10 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x07f  allpass 3 s26 (rev::ROW_STEPS[14])
		c.begin_step();
		c.push_hist(0x07f, c.ram[c.addr(31, 0, false)]);
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x07f]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x080  allpass 3 s22 (rev::ROW_STEPS[15])
		c.begin_step();
		c.queue_d(61, c.captured(c.m[61], 0x080));
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[61];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x080]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.wlatch = outv; c.haswl = true;   // the data of the next delay RAM write
		// 0x081  allpass 3 s28 (rev::ROW_STEPS[12])
		c.begin_step();
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x081]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x082  allpass 3 s24 (rev::ROW_STEPS[13])
		c.begin_step();
		c.queue_d(40, c.in[0][4]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[61];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x082]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(119, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x083  early reflection word 16 (rev::ER_WORDS[16])
		c.begin_step();
		c.queue_d(58, c.in[1][4]);
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x083]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl) { outv = c.wlatch; c.haswl = false; }
		c.ram[c.addr(32, 0, false)] = outv;
		// 0x084  early reflection word 17 (rev::ER_WORDS[17])
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x084));
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x084]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x085  early reflection word 18 (rev::ER_WORDS[18])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x085]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x086  early reflection word 19 (rev::ER_WORDS[19])
		c.begin_step();
		c.queue_d(12, c.lfo[4]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[59];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x086]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x087  early reflection word 20 (rev::ER_WORDS[20])
		c.begin_step();
		c.push_hist(0x087, c.ram[c.addr(33, 0, false)]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[60];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x087]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x088  early reflection word 21 (rev::ER_WORDS[21])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[61];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x088]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		c.queue_r(120, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x08e to 0x091. holds the host words rev::ROW_STEPS[16] to [19]; the rest of its steps read the delay RAM and move values between the cells
void block_0x08e(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x08e to 0x091, 4 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x08e  allpass 4 s26 (rev::ROW_STEPS[18])
		c.begin_step();
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x08e]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x08f  allpass 4 s22 (rev::ROW_STEPS[19])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x08f]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(35, 0, false)] = outv;
		// 0x090  allpass 4 s28 (rev::ROW_STEPS[16])
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x090));
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x090]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x091  allpass 4 s24 (rev::ROW_STEPS[17])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x091]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(120, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x097 to 0x09a. holds the host words rev::ROW_STEPS[20] to [23]; the rest of its steps read the delay RAM and move values between the cells
void block_0x097(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x097 to 0x09a, 4 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x097  allpass 5 s26 (rev::ROW_STEPS[22])
		c.begin_step();
		c.push_hist(0x097, c.ram[c.addr(37, 0, false)]);
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x097]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x098  allpass 5 s22 (rev::ROW_STEPS[23])
		c.begin_step();
		c.queue_d(57, c.captured(c.m[57], 0x098));
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x098]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.wlatch = outv; c.haswl = true;   // the data of the next delay RAM write
		// 0x099  allpass 5 s28 (rev::ROW_STEPS[20])
		c.begin_step();
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x099]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x09a  allpass 5 s24 (rev::ROW_STEPS[21])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x09a]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(120, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);


		
}
// The run of steps 0x0a3 to 0x0b5. holds the host words rev::HALL_COMB_STEPS[0] to [15] [17] to [19]; the rest of its steps read the delay RAM and move values between the cells
void block_0x0a3(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x0a3 to 0x0b5, 19 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x0a3  comb 0 x1 (rev::HALL_COMB_STEPS[1])
		c.begin_step();
		c.push_hist(0x0a3, c.ram[c.addr(40, 0, false)]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0a3]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0a4  comb 0 x2 (rev::HALL_COMB_STEPS[2])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[34];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0a4]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(34, c.psrc);
		// 0x0a5  comb 0 a (rev::HALL_COMB_STEPS[3])
		c.begin_step();
		c.queue_d(56, c.captured(c.m[56], 0x0a5));
		xv = r[55];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0a5]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(55, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0a6  comb 1 x1 (rev::HALL_COMB_STEPS[5])
		c.begin_step();
		c.queue_d(13, c.lfo[5]);
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0a6]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0a7  comb 1 x2 (rev::HALL_COMB_STEPS[6])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[35];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0a7]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(35, c.psrc);
		// 0x0a8  comb 1 a (rev::HALL_COMB_STEPS[7])
		c.begin_step();
		c.queue_d(57, c.captured(c.m[57], 0x0a8));
		xv = r[56];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0a8]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(56, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0a9  comb 0 gain (rev::HALL_COMB_STEPS[0])
		c.begin_step();
		xv = r[55];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0a9]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(42, 0, false)] = outv;
		// 0x0aa  comb 2 x1 (rev::HALL_COMB_STEPS[9])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[58];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0aa]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0ab  comb 2 x2 (rev::HALL_COMB_STEPS[10])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[36];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0ab]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(36, c.psrc);
		// 0x0ac  comb 2 a (rev::HALL_COMB_STEPS[11])
		c.begin_step();
		xv = r[57];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0ac]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(57, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0ad  comb 1 gain (rev::HALL_COMB_STEPS[4])
		c.begin_step();
		xv = r[56];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0ad]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(43, 0, false)] = outv;
		// 0x0ae  comb 3 x1 (rev::HALL_COMB_STEPS[13])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0ae]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0af  comb 3 x2 (rev::HALL_COMB_STEPS[14])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[37];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0af]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(37, c.psrc);
		// 0x0b0  comb 3 a (rev::HALL_COMB_STEPS[15])
		c.begin_step();
		xv = r[58];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0b0]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(58, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0b1  comb 2 gain (rev::HALL_COMB_STEPS[8])
		c.begin_step();
		xv = r[57];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0b1]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(44, 0, false)] = outv;
		// 0x0b2  comb 4 x1 (rev::HALL_COMB_STEPS[17])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[57];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0b2]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0b3  comb 4 x2 (rev::HALL_COMB_STEPS[18])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[38];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0b3]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(38, c.psrc);
		// 0x0b4  comb 4 a (rev::HALL_COMB_STEPS[19])
		c.begin_step();
		xv = r[59];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0b4]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(59, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0b5  comb 3 gain (rev::HALL_COMB_STEPS[12])
		c.begin_step();
		xv = r[58];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0b5]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(45, 0, false)] = outv;


		
}
// The run of steps 0x0d1 to 0x0d7. holds the host words rev::HALL_COMB_STEPS[16] [20] to [23], rev::LEVEL_STEPS; the rest of its steps read the delay RAM and move values between the cells
void block_0x0d1(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers, named as step() names them
	double* const m = c.m;                 // the 64 cells, which carry the values between the units
	double xv, a, m2, prod, y, outv;      // the step's operands and its result

// ---- 0x0d1 to 0x0d7, 7 steps ----
		xv = a = m2 = prod = y = outv = 0.0;
		// 0x0d1  comb 4 gain (rev::HALL_COMB_STEPS[16])
		c.begin_step();
		xv = r[59];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0d1]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(52, 0, false)] = outv;
		// 0x0d2  comb 5 x1 (rev::HALL_COMB_STEPS[21])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[56];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0d2]) * m2;
		a = 0.0;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		// 0x0d3  comb 5 x2 (rev::HALL_COMB_STEPS[22])
		c.begin_step();
		xv = 0.0;
		c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		c.psrc = c.src; c.src = m[39];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0d3]) * m2;
		a = c.acc;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 0);
		c.queue_d(39, c.psrc);
		// 0x0d4  comb 5 a (rev::HALL_COMB_STEPS[23])
		c.begin_step();
		xv = r[60];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0d4]) * m2;
		a = c.acc;
		y = (prod + a) * 2.0;
		c.queue_r(60, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0d5  the level gain, word 1 (rev::LEVEL_STEPS)
		c.begin_step();
		xv = r[120];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0d5]) * m2;
		a = 0.0;
		y = (prod + a) * 4.0;
		c.queue_r(10, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0d6  the level gain, word 0 (rev::LEVEL_STEPS)
		c.begin_step();
		xv = r[119];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[0];
		m2 = xv;
		prod = yss236_core::s16(c.coef[0x0d6]) * m2;
		a = 0.0;
		y = (prod + a) * 4.0;
		c.queue_r(9, yss236_core::mode(y, 1));
		y = c.acc = yss236_core::accmode(y, 1);
		// 0x0d7  comb 5 gain (rev::HALL_COMB_STEPS[20])
		c.begin_step();
		xv = r[60];
		c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
		c.psrc = c.src; c.src = m[62];
		m2 = c.src;
		prod = yss236_core::s16(c.coef[0x0d7]) * m2;
		a = c.x;
		y = (prod + a) * 1.0;
		y = c.acc = yss236_core::accmode(y, 1);
		outv = yss236_core::clip(y, yss236_core::SAT);
		c.haswl = false;              // a writer carrying the flag writes its own data
		c.ram[c.addr(53, 0, false)] = outv;
}

// The reverb window's ten runs of steps, and the block that is each one.
struct StepRun { int from, to; void (*block)(yss236_core&); };
const StepRun RUNS[] = {
    {0x01a, 0x03a, block_0x01a},
    {0x03e, 0x042, block_0x03e},
    {0x04c, 0x050, block_0x04c},
    {0x059, 0x05f, block_0x059},
    {0x062, 0x066, block_0x062},
    {0x07f, 0x089, block_0x07f},
    {0x08e, 0x092, block_0x08e},
    {0x097, 0x09b, block_0x097},
    {0x0a3, 0x0b6, block_0x0a3},
    {0x0d1, 0x0d8, block_0x0d1},
};
}  // namespace

// One run of the reverb window's steps, exactly as yss236_core::step() would run them. The check hands
// this the ten runs above, one call each per pass; a range the window never hands us, which no host
// builds, falls back to the interpreter.
void rev(yss236_core& c, int from, int to)
{
    for (StepRun const& run : RUNS)
        if (run.from == from && run.to == to) { run.block(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

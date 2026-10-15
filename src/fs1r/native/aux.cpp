// fs1r/native/aux.cpp - VOP3-2, the aux window's program: its 40 steps.
//
// rom::AUX_PROGRAM is the only program the window has (WINDOWS[AUX].programs is one entry, 40 steps,
// rom::STEPS_AUX 0x0e8 to 0x10f) and it runs under every performance: the window has no parameters at
// all, since EffectHost::applyParam's AUX case breaks without writing anything
// (fx_dispatch.cpp), so nothing in it moves when a parameter does. It is therefore one native for one
// program, and no type is excluded.
//
// What the 40 steps compute is two things, and the comments below open each:
//
//   0x0e8-0x107  eight units of four steps, each a first-order smoother of one cell. The unit's state
//                is a register, its cell is the multiplier input, and the word on its second step is one
//                of rom::STEPS_OUTPUT_GAIN (0xe9, 0xed, 0xf1, 0xf5, 0xf9, 0xfd, 0x101, 0x105, the eight
//                steps EffectHost::setOutputGain writes 21 into for 0 dB), so the host's output gain
//                word is the unit's pole. The third step leaves the pass's difference between the cell
//                and the smoother doubled into a second register, and the base program's output mixer
//                reads those eight (0x0d8, 0x0e0, 0x050, 0x056, 0x05f, 0x089, 0x092, 0x0e0).
//   0x108-0x10f  the window's tail: two empty steps, then two pairs that clear the running value and
//                write it into the cells m[40] and m[41], which are the two cells units 5 and 7
//                smooth, and the window's serial output, the cell m[17] to out[8].
//
// The registers: r[61] to r[66], r[101] and r[102] the eight units' states, r[31] to r[34] and r[43] to
// r[46] the eight differences, r[7] and r[8] what the tail's two clear steps read. The cells: m[1] to
// m[6], m[40] and m[41] the eight units' cells, m[17] the output. A scan of the whole program shows the
// eight states and differences written here and read nowhere outside the window, and each of the eight
// cells written by a step of another window, except m[40] and m[41] which the tail also writes.
//
// Every step is one case of the switch, in the order yss236_core::step() runs its own arithmetic, with
// the step's fixed fields folded in. The one constant the program reads is the gain word of the eight
// units' second steps, and it is read from c.coef[st] on the pass as step() reads it: the aux window's
// preset puts 0x18 there (rom::AUX_PRESETS, through EffectHost::resetWindow) and setOutputGain puts 21
// there at boot, and which word is in the chip when these steps run is the host's business, not this
// program's.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x0e8 to 0x110, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's.
void block_0x0e8(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double xv, a, m2, prod, y, k;          // the step's operands and its result

	for (int st = 0x0e8; st < 0x110; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x0e8 to 0x107, the eight units: a smoother of one cell each --------------------------------
		case 0x0e8:  // unit 0: the running value takes the state r[61]
			k = yss236_core::s16(c.coef[st]);
			xv = r[61];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;                   // the multiplier input is the register itself (class 3 is unity)
			prod = m2;
			a = 0.0;                   // the adder is off, so this step only loads the running value
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);    // dr is 0, so no register takes it
		break;
		case 0x0e9:  // unit 0: the state moves toward the cell by the gain word times the difference
			k = yss236_core::s16(c.coef[st]);
			xv = r[31];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;             // the gain word, class 2 with no t input
			a = c.acc;                 // the adder is on the running value the step before set
			y = (prod + a) * 1.0;
			c.queue_r(61, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0ea:  // unit 0: the difference between the cell m[1] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[61];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[1];
			m2 = c.src;                // the multiplier input is the cell, not the register
			prod = m2;
			a = c.x;                   // the adder is on the register fetched this step, and negates
			y = (prod - a) * 2.0;      // (sub), scaled by RSCALE[1]
			c.queue_r(31, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0eb:  // unit 0: the empty step between the units
			xv = 0.0;                  // a class-0 step with no register reads still takes the latch
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x0ec:  // unit 1: the running value takes the state r[62]
			k = yss236_core::s16(c.coef[st]);
			xv = r[62];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x0ed:  // unit 1: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[43];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(62, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0ee:  // unit 1: the difference between the cell m[6] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[62];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[6];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(43, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0ef:  // unit 1: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x0f0:  // unit 2: the running value takes the state r[63]
			k = yss236_core::s16(c.coef[st]);
			xv = r[63];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x0f1:  // unit 2: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[32];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(63, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0f2:  // unit 2: the difference between the cell m[2] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[63];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[2];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(32, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0f3:  // unit 2: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x0f4:  // unit 3: the running value takes the state r[64]
			k = yss236_core::s16(c.coef[st]);
			xv = r[64];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x0f5:  // unit 3: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[44];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(64, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0f6:  // unit 3: the difference between the cell m[5] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[64];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[5];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(44, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0f7:  // unit 3: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x0f8:  // unit 4: the running value takes the state r[65]
			k = yss236_core::s16(c.coef[st]);
			xv = r[65];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x0f9:  // unit 4: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[33];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(65, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0fa:  // unit 4: the difference between the cell m[3] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[65];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[3];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(33, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0fb:  // unit 4: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x0fc:  // unit 5: the running value takes the state r[66]
			k = yss236_core::s16(c.coef[st]);
			xv = r[66];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x0fd:  // unit 5: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[45];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(66, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0fe:  // unit 5: the difference between the cell m[40] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[66];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[40];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(45, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x0ff:  // unit 5: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x100:  // unit 6: the running value takes the state r[101]
			k = yss236_core::s16(c.coef[st]);
			xv = r[101];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x101:  // unit 6: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[34];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(101, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x102:  // unit 6: the difference between the cell m[4] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[101];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[4];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(34, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x103:  // unit 6: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x104:  // unit 7: the running value takes the state r[102]
			k = yss236_core::s16(c.coef[st]);
			xv = r[102];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x105:  // unit 7: the state moves by the gain word
			k = yss236_core::s16(c.coef[st]);
			xv = r[46];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(102, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x106:  // unit 7: the difference between the cell m[41] and the state, doubled
			k = yss236_core::s16(c.coef[st]);
			xv = r[102];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[41];
			m2 = c.src;
			prod = m2;
			a = c.x;
			y = (prod - a) * 2.0;
			c.queue_r(46, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
		break;
		case 0x107:  // unit 7: the empty step
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		// ---- 0x108 to 0x10f, the tail: the two clear pairs and the window's output -----------------------
		case 0x108:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x109:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x10a:  // clear the running value, reading r[7] on the way
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;                   // the adder is off, so this step writes the running value only
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x10b:  // and write it, scaled by RSCALE[3], into the cell m[40]
			k = yss236_core::s16(c.coef[st]);
			xv = r[17];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;                 // the adder is on the running value 0x10a cleared
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(40, yss236_core::clip(y, yss236_core::DSAT));   // msrc 6: the step's own result
		break;
		case 0x10c:  // clear the running value again, reading r[8]
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
		break;
		case 0x10d:  // and write it into the cell m[41]
			k = yss236_core::s16(c.coef[st]);
			xv = r[18];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(41, yss236_core::clip(y, yss236_core::DSAT));
		break;
		case 0x10e:  // the window's output: the cell m[17] to the serial out at page 8
			c.out[8] = m[17];
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		case 0x10f:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = xv;
		break;
		}
	}
}

}  // namespace

// One run of the aux window's steps, exactly as yss236_core::step() would run them. The check hands this
// the one run above, one call each per pass; a range the window never hands us, which no host builds,
// falls back to the interpreter.
void aux(yss236_core& c, int from, int to)
{
	if (from == 0x0e8 && to == 0x110) { block_0x0e8(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
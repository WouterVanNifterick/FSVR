// fs1r/native/dly_m00.cpp - VOP3-2, the reverb delay window's first program: its 72 steps.
//
// rom::REVERB_DELAY_PROGRAMS[0], the program the window holds for reverb types 0 to 12 (No Effect,
// Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate, White Room, Tunnel, Basement, Canyon). Types 13 to
// 16 of the reverb block (Delay L,C,R, Delay L,R, Echo, Cross Delay) run their programs here instead,
// under the keys dly-m08 to dly-m11, and fx_delay.cpp is the host that feeds those. Under types 0 to
// 12 no DELAY_ROUTES entry matches, so nothing rewrites the window's constants or delay offsets when a
// parameter moves: they are the reverb type's preset (rom::REVERB_DELAY_0_BLOCKS) and they are the
// same thirteen times over, which a read of c.coef[] and c.slot[] under each type confirms. The code
// reads them where they live all the same, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in three groups the comments below open each of:
//
//   0x110-0x126  the send into the block and the host's two tone filters, rev::DELAY_LPF and
//                rev::DELAY_HPF and their copies, each group three steps, each reading one of the
//                block's own three lines (the slots 0x44, 0x45 and 0x46, in the reverb's area) and
//                leaving a coefficient in t[12] or t[13]. The reads go out in m[22], m[23] and m[24].
//   0x127-0x14d  the window's own line, read as four pairs with the index added, each read taken up
//                by a cell five steps later and each pair differenced and scaled by t[9], t[11], t[11]
//                or t[10] onto the running value. Those three coefficients and the three index loads
//                are all zero under every type that loads this program, so the four products are zero
//                and what the group adds is the running value on its own.
//   0x14e-0x157  three more reads of the block's own area, published in m[25], m[26] and m[27], the
//                two writes of the window's line (the value 0x146 latched, and 0x157's own result),
//                and the block's output, the cell m[41] to the serial out at 0x156.
//
// The registers: r[123] and r[2] the two inputs of the filter groups, r[124], r[126] and r[1] their
// three states, r[127] the first group's output, r[119], r[121], r[122] and r[120] the four tap sums,
// r[13] and r[14] the running value scaled out. The cells: m[12] the send, written by a window outside
// this one, m[56] to m[63] the eight tap captures, m[22] to m[27] what this window publishes, m[8] to
// m[10] the three LFO values it hands on, m[1] and m[5] the two serial inputs. The t registers:
// t[12] and t[13] the two filter coefficients, t[9], t[10] and t[11] the three tap coefficients.
//
// A delay RAM read is made at one step and taken up by a cell 5 to 11 steps later, which is what
// yss236_core::captured() returns and what an msrc of 4 queues. A multiply by a constant of zero with
// the adder on the running value is the chip's way of writing one register to another, and a step whose
// only job is to clear the running value does the same with the adder off; both appear below and both
// are what step() computes.
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
	double xv, a, m2, prod, y, outv, v, k; // the step's operands and its result

	for (int st = 0x110; st < 0x158; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x110 to 0x126, the send and the host's tone filters -------------------------------------------
		//
		// rev::DELAY_LPF, the three words the host's designFilter writes for the low pass, and rev::DELAY_HPF
		// for the high pass, then their copies (rev::DELAY_LPF_COPY, rev::DELAY_HPF_COPY). A group's three
		// steps read one of the block's own delay lines (the slots 0x44, 0x45 and 0x46, which sit in the
		// reverb's area) and leave a coefficient in t[12] or t[13] and a value in a register. The idiom at
		// 0x114, 0x119 and 0x121 is the chip's: 2 * |x| - 128, which is a register's value centred on zero,
		// read back as a t multiplier a third of the way up (t_write divides by 128).
		//
		// The three cells m[22], m[23] and m[24] hold the three reads, for windows outside this one to take
		// up: the delay window's output goes to the reverb block, which is where the sound is.
		case 0x110:  // the send in: m[1] from serial bank 0 page 8, r[124] from the cell m[12] another window writes
				c.queue_d(1, c.in[0][8]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[12];
				a = c.src;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(124, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x111:  // rev::DELAY_LPF[2], the group's feed gain
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
		case 0x112:  // rev::DELAY_LPF[1]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(125, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x113:  // rev::DELAY_LPF[0]: the first read, of the slot 0x44
				c.push_hist(st, c.ram[c.addr(0x44, 0, false)]);
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
		case 0x114:  // rev::DELAY_HPF[0], its coefficient into t[12]
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
		case 0x117:  // the second read, of the slot 0x45; m[8] is the LFO 8 for another window, r[127] = t[12] * r[124]
				c.push_hist(st, c.ram[c.addr(0x45, 0, false)]);
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
		case 0x118:  // rev::DELAY_LPF_COPY[2]: m[22] takes up the read of 0x113
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
		case 0x119:  // rev::DELAY_LPF_COPY[1], its coefficient into t[13]
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
		case 0x11b:  // rev::DELAY_HPF_COPY[2]: the third read, of the slot 0x46
				c.push_hist(st, c.ram[c.addr(0x46, 0, false)]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[12];
				a = c.src;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(126, yss236_core::mode(v, 0));
				c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x11c:  // rev::DELAY_HPF_COPY[1]: m[23] takes up the read of 0x117
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
		case 0x11e:  // r[124] = 128 + 2 * acc, the first group's state
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(124, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x11f:  // r[119] = t[13] * r[1]
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
		case 0x120:  // m[24] takes up the read of 0x11b
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
		case 0x121:  // the second group's state into r[126] and t[12]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 2.0;
				c.queue_r(126, yss236_core::mode(v, 1));
				c.acc = yss236_core::accmode(v, 1);
				c.t_write(c.ops[st], st, c.acc);
			break;
		case 0x122:  // the running value takes t[13] * r[119]
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
		case 0x123:  // the running value to zero
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
		case 0x124:  // t[9], the first tap's coefficient, and the first index load
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
		case 0x125:  // 0.5 * r[1] onto the running value
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
		case 0x126:  // the running value on, the send the taps work from
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

		// ---- 0x127 to 0x14d, the four delay taps ----------------------------------------------------------
		//
		// The window's own line, read as four pairs. Each pair is a read of one slot and a read of the slot
		// after it (memop 4, one word older), both with the index the IDXW steps loaded added, and each read
		// is taken up by a cell five steps later: m[56] and m[57] from the pair at 0x127 and 0x12b, m[60] and
		// m[61] from 0x12f and 0x133, m[62] and m[63] from 0x137 and 0x13b, m[58] and m[59] from 0x13f and
		// 0x143. The two steps that follow each pair difference the two and scale the difference, t[9] for
		// the first, t[11] for the second and third and t[10] for the fourth, adding it to the running value
		// with the sign step() gives the adder: y = t * cell - acc.
		//
		// The coefficients are t[9] at 0x124, t[11] at 0x12b (rev::DELAY_CURVE[0]) and t[10] at 0x13a, and
		// the index is loaded by those same three steps, from the running value there. Under the thirteen
		// reverb types that load this program those three running values are zero, so all three coefficients
		// and all three indices are zero and the four products are zero. The reads still happen, the cells
		// still move, and the sums r[119], r[121], r[122] and r[120] are the running value on its own.
		case 0x127:  // the first tap's read of the slot 0x49, with the index; m[9] is the LFO 9 for another window
				c.push_hist(st, c.ram[c.addr(0x49, c.cur_idx(), false)]);
				c.queue_d(9, c.lfo[9]);
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(1, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x128:  // m[24] takes up the read of 0x11b again, the second group
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
		case 0x129:  // the second group's first sum
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
		case 0x12b:  // rev::DELAY_CURVE[0]: the first tap's second read, the slot 0x4a one word older, into t[11] and the index
				c.push_hist(st, c.ram[c.addr(0x4a, (c.cur_idx() + 1), false)]);
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
		case 0x12c:  // m[56] takes up the read of 0x127
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
		case 0x12d:  // the second tap's scale
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
		case 0x12f:  // rev::DELAY_CURVE_COPY[0]: the second tap's first read, the slot 0x4b, into the running value
				c.push_hist(st, c.ram[c.addr(0x4b, c.cur_idx(), false)]);
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
		case 0x130:  // m[57] takes up the read of 0x12b
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
		case 0x131:  // the second tap's running value
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
		case 0x132:  // the running value on
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
		case 0x133:  // the second tap's second read, the slot 0x4c one word older
				c.push_hist(st, c.ram[c.addr(0x4c, (c.cur_idx() + 1), false)]);
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
		case 0x134:  // m[60] takes up the read of 0x12f, with the second group's half gain
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
		case 0x135:  // the running value on
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
		case 0x136:  // the second tap's state into r[126]
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				c.psrc = c.src; c.src = m[0];
				a = c.acc;
				v = 128.0 * yss236_core::s16(c.coef[st]) + a;
				v *= 1.0;
				c.queue_r(126, yss236_core::mode(v, 2));
				c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x137:  // the first tap: t[9] * m[56] - acc, and its own read of the slot 0x4d
				c.push_hist(st, c.ram[c.addr(0x4d, c.cur_idx(), false)]);
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
		case 0x138:  // the first tap's second half, t[9] * m[57] - acc, into r[119]
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
		case 0x139:  // the running value to zero
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
		case 0x13a:  // t[10] and the third index load
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
		case 0x13b:  // the second tap's difference, t[11] * m[60] - acc, and its read of the slot 0x4e
				c.push_hist(st, c.ram[c.addr(0x4e, (c.cur_idx() + 1), false)]);
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
		case 0x13c:  // the second tap's second half, t[11] * m[61] - acc, into r[121]
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
		case 0x13d:  // the third tap's scale
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
		case 0x13e:  // the third tap's running value
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
		case 0x13f:  // the third tap's first read, the slot 0x4f
				c.push_hist(st, c.ram[c.addr(0x4f, c.cur_idx(), false)]);
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
		case 0x140:  // m[63] takes up the read of 0x13b, the third tap's sum into r[91]
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
		case 0x141:  // the running value to zero
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
		case 0x142:  // the running value on
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
		case 0x143:  // the third tap's second read, the slot 0x50 one word older, into r[92]
				c.push_hist(st, c.ram[c.addr(0x50, (c.cur_idx() + 1), false)]);
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
		case 0x144:  // m[58] takes up the read of 0x13f
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
		case 0x145:  // the running value to zero
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
		case 0x146:  // the running value into the write latch (w1[13]), the value 0x153 writes
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
		case 0x147:  // the third tap: t[11] * m[62] - acc, and a read of the slot 0x51; m[10] is the LFO 10
				c.push_hist(st, c.ram[c.addr(0x51, 0, false)]);
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
		case 0x148:  // the third tap's second half, t[11] * m[63] - acc, into r[122]
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
		case 0x149:  // the running value to zero
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
		case 0x14a:  // the running value on
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
		case 0x14b:  // a read of the slot 0x52 and the running value into r[124]
				c.push_hist(st, c.ram[c.addr(0x52, 0, false)]);
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
		case 0x14c:  // the fourth tap, t[10] * m[58] - acc; m[25] takes up the read of 0x147
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
		case 0x14d:  // the fourth tap's second half, t[10] * m[59] - acc, into r[120]
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

		// ---- 0x14e to 0x157, the three more reads, the two writes and the output -----------------------------
		//
		// Three further reads, of the slots 0x51, 0x52 and 0x53, which are the block's own area again; their
		// values go to m[25], m[26] and m[27] for other windows, and the running value scales into r[13] and
		// r[14] on the way. The two writes are the window's line: 0x153 a class-0 step writes the value
		// 0x146 latched, and 0x157 carries w1[13] itself, which is why it writes its own result and not the
		// latch. The block's output is the cell m[41] to the serial out at 0x156.
		case 0x14e:  // the running value with a quarter of r[89]
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
		case 0x14f:  // a read of the slot 0x53, the running value into r[13]
				c.push_hist(st, c.ram[c.addr(0x53, 0, false)]);
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
		case 0x150:  // m[26] takes up the read of 0x14b
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
		case 0x151:  // the running value on
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
		case 0x152:  // the running value into r[125]
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
		case 0x153:  // the write of the latched value, into the slot 0x54
				c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
				if (c.haswl) { c.ram[c.addr(0x54, 0, false)] = c.wlatch; c.haswl = false; }
			break;
		case 0x154:  // m[27] takes up the read of 0x14f
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
		case 0x155:  // the running value into r[14]
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
		case 0x156:  // the block's output: m[41] to the serial out
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
		case 0x157:  // the write of this step's own result, into the slot 0x55
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
				c.ram[c.addr(0x55, 0, false)] = outv;
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

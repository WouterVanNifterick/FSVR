// fs1r/native/ins_m03.cpp - VOP3-2, the insertion window's program for type msb 03: its 96 steps.
//
// rom::INSERTION_PROGRAMS[3], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 3: ins-37 ER 1, ins-38 ER 2, ins-39 Gate Rev and
// ins-40 Revrs Gate, all four programmed by EffectHost::earlyReflections() in fx_reverb.cpp. The same
// 96 words run under every one of them; what the type changes is which words the host writes, the
// pattern row erPattern() reads (six entries further on for Gate Rev, eight for Reverse Gate), and
// how many of the twenty gains it leaves at zero. Nothing here knows which one is running.
//
// Every constant the program reads is read from c.coef[st] on the pass and every delay offset from
// the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here: the
// program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own. The program writes no t register and reads none, so the t
// file is left alone.
//
// Unlike ins-m02, which touches no delay RAM at all, this program is most of a delay line: eleven
// reads of ten tap slots plus one allpass point, and nine writes. What it computes, in the groups the
// comments open each of. The names are the host's, from rom_tables.cpp and fx_reverb.cpp.
//   0x1a0-0x1b3  the filters. 0x1a0 and 0x1a1 mix r[7] and r[8] into r[67] at the ROM word 0x2000 each
//                and twice that, so the high pass and the low pass run on one mono signal and the two
//                channels are told apart by their dry signals and their tap lines further on.
//                ins::ER_HPF's five words are the biquad over r[67] to r[70] (0x1a2 to 0x1a6, b0 at
//                0x1a6, b1 at 0x1a3, b2 at 0x1a2, -a1 at 0x1a4, -a2 at 0x1a5), ins::ER_LPF_2's the biquad
//                over r[71] to r[74] (0x1ac to 0x1b0), and ins::ER_LPF's three words the one-pole High
//                Damp filter (0x1b1 to 0x1b3) on the twentieth tap. Between them the steps read the
//                first five right-channel taps, each read landing in a cell five steps later.
//   0x1b4-0x1bb  the first two of the four diffusion rows, ins::ER_ROW_STEPS' first eight words: r[73],
//                the low pass's output, and the cells m[18] and m[19] into the slots 0x6d and 0x6e
//                (ins::ER_ROW_SLOTS[0] and [1]). Those two cells are the rows' own allpass points,
//                ins::ER_ROW_SLOTS[2] and [3], the slots 0x2e and 0x2f, which the base program's steps
//                0x0be and 0x0c2 capture: each row is fed its own output delayed, which is the pair of
//                allpasses the FB Level then feeds the delay line with. Both rows end at r[119], so
//                the second overwrites the first three steps later.
//   0x1bc-0x1c5  the ten left-channel gains, ins::ER_STEPS[0] to [9], on the cells m[22] to m[31].
//                Those cells are not this window's: the base program's steps 0x118 to 0x194 capture
//                them from ins::ER_SLOTS' even entries, the slots 0x44 to 0x63 and 0x64. The sum lands
//                in m[43] and is written into the slot 0x71 by 0x1c7.
//   0x1c6-0x1d1  that write, and the reads of the eighth, ninth and tenth right-channel taps. Seven
//                of these steps are class 0 with no memory op and no ALU: they count and nothing else.
//   0x1d2-0x1db  the ten right-channel gains, ins::ER_STEPS[10] to [19], on the cells m[56] to m[63]
//                and back on m[56] and m[57], which this program fills itself from the slots 0x68 to
//                0x75 (ins::ER_SLOTS' odd entries). The sum lands in m[44] and 0x1db writes it into
//                the slot 0x76. erPattern() leaves ins::ER_STEPS[19] at zero, so the tenth right tap
//                is never heard.
//   0x1dc-0x1df  the two sums with their allpass points, ins::ER_DIFF_SLOTS. earlyReflections()'
//                DIFFUSION case puts the slot 0x65 at delay[0x76] + d + 1 and the slot 0x75 at
//                delay[0x71] + d + 1, so each sum picks the other one's up d + 1 samples later: r[121]
//                is the left sum and r[120] the right.
//   0x1e0-0x1ed  the last two diffusion rows, ins::ER_ROW_STEPS' other eight words: an allpass and a
//                line for each sum, the slots 0x77 and 0x79 for r[120] and 0x78 and 0x7a for r[121]
//                (ins::ER_ROW_SLOTS[4] to [7]), which is what the two registers read back at 0x1e6
//                and 0x1ea.
//   0x1ee-0x1ef  the delay line's input. r[119], the diffusion rows' output, goes in at unity, and
//                ins::ER_FEEDBACK[0], the FB Level word, scales r[75], the High Damp filter's output on
//                the twentieth tap. The write is to the slot 0x7b, ins::ER_SLOTS[21], which is where
//                earlyReflections()'s erPattern() measures every tap's position from: the ten right
//                taps sit the initial delay plus the room size past it, so the line is the tail the
//                taps are read from.
//   0x1f0-0x1f7  the dry and wet mix, ins::ER_WET's four words: r[7] and r[8] at [0] and [2] against
//                r[121] and r[120] at [1] and [3], into r[17] and r[18], the two outputs the base
//                program reads back. 0x1f3 and 0x1f7 write the slots 0x7c and 0x7d with their
//                coefficients, which no parameter writes and which are zero, so both slots take zero
//                every pass; nothing reads them.
//   0x1f8-0x1ff  eight steps that count and nothing else.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] to r[70] the
// high pass, r[71] to r[74] the low pass, r[73] its output; r[75] the High Damp filter's output, the
// delay line's input; r[119] the two first diffusion rows' output; r[120] the right sum, r[121] the
// left; r[17] and r[18] the two outputs.
//
// The old model in vop3_effects.h has a case for these four types and is a loop of twelve taps off one
// line with a hand-picked gain envelope per tap, rising for Reverse Gate and decaying for Gate Reverb.
// It has no delay line's worth of slots, no diffusion rows, no feedback word and no High Damp filter,
// and where the two differ the program above is what runs. It is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m03: identical, 4 types, 19200 native passes.
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
	double xv, k, m2, prod, a, y, outv;     // the step's operands and its result

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x1a0 to 0x1b3, the two signals in, the high pass, the low pass and the first five right-channel taps ----
		case 0x1a0:  // acc = k * r[7]: the left signal's share of the mono mix, the ROM word 0x2000
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a1:  // r[67] = saturate(2 * (k * r[8] + acc)): both signals at the ROM word 0x2000 each, doubled, the filters' input
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a2:  // ins::ER_HPF[2], b2 on r[68], the mix as 0x1a4 leaves it
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
		case 0x1a3:  // the read of the slot 0x68, ins::ER_SLOTS[1], the first right-channel tap, which m[56] takes up at 0x1a8; ins::ER_HPF[1], b1 on r[67]
			c.push_hist(st, c.ram[c.addr(0x68, 0, false)]);
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
		case 0x1a4:  // m[3] takes the left serial input of page 13, which no step of this program reads; ins::ER_HPF[3], -a1 on r[69]; r[68] takes the mix r[67] as it stood at 0x1a3
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(68, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a5:  // m[7] takes the right serial input of page 13, read by no step here either; ins::ER_HPF[4], -a2 on r[70]
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(70, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a6:  // ins::ER_HPF[0], b0 on r[67]: r[69] = saturate(2 * (k * r[67] + acc)), the high pass's output
			k = yss236_core::s16(c.coef[st]);
			xv = r[67];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1a7:  // the read of the slot 0x69, ins::ER_SLOTS[3], the second right tap; acc = 0
			c.push_hist(st, c.ram[c.addr(0x69, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a8:  // m[56] takes that tap; the word here is zero, so the accumulator keeps the zero 0x1a7 left
			c.queue_d(56, c.captured(m[56], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1a9:  // acc = 0 * r[71]: a second of the three zero words this block carries
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
		case 0x1aa:  // r[72] takes r[71] as it stood at 0x1a9; acc = 0 * r[72]
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(72, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ab:  // the read of the slot 0x6a, ins::ER_SLOTS[5], the third right tap; r[71] = saturate(2 * (0.5 * r[69] + acc)), the high pass's output into the low pass at unity
			c.push_hist(st, c.ram[c.addr(0x6a, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ac:  // m[57] takes the third tap; ins::ER_LPF_2[2], b2 on r[72]
			c.queue_d(57, c.captured(m[57], st));
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
		case 0x1ad:  // ins::ER_LPF_2[1], b1 on r[71]
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
		case 0x1ae:  // ins::ER_LPF_2[3], -a1 on r[73]
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
		case 0x1af:  // the read of the slot 0x6b, ins::ER_SLOTS[7], the fourth right tap; ins::ER_LPF_2[4], -a2 on r[74]; r[74] takes r[73] as it stood at 0x1ae
			c.push_hist(st, c.ram[c.addr(0x6b, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(74, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b0:  // m[58] takes the fourth tap; ins::ER_LPF_2[0], b0 on r[71]: r[73] = saturate(4 * (k * r[71] + acc)), the low pass's output
			c.queue_d(58, c.captured(m[58], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b1:  // ins::ER_LPF[0], b0 on m[32], the twentieth tap, which the base program's step 0x198 captured from the slot 0x64
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[32];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1b2:  // ins::ER_LPF[1], b1 on m[42], which this step's own cell write leaves as the m[32] of a pass earlier; m[42] takes the m[32] of this pass
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(42, c.psrc);
			break;
		case 0x1b3:  // the read of the slot 0x6c, ins::ER_SLOTS[9], the fifth right tap; ins::ER_LPF[2], -a1 on r[75]: r[75] = saturate(k * r[75] + acc), the High Damp filter's output
			c.push_hist(st, c.ram[c.addr(0x6c, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1b4 to 0x1bb, the first two diffusion rows: r[73] and the cells m[18] and m[19] into the slots 0x6d and 0x6e ----
		case 0x1b4:  // m[59] takes the fourth tap, read at 0x1af; ins::ER_ROW_STEPS[0]: acc = k * r[73], the filtered signal into the first row
			c.queue_d(59, c.captured(m[59], st));
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
		case 0x1b5:  // ins::ER_ROW_STEPS[1]: r[119] = saturate(2 * (k * m[18] + acc)), the row's output with its own allpass point
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[18];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1b6:  // ins::ER_ROW_STEPS[2]: acc = k * r[73], the second row's input
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
		case 0x1b7:  // ins::ER_ROW_STEPS[3]: the write of the slot 0x6d, ins::ER_ROW_SLOTS[0], k * m[18] + acc
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[18];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x6d, 0, false)] = outv;
			break;
		case 0x1b8:  // m[60] takes the fifth tap, read at 0x1b3; ins::ER_ROW_STEPS[4]: acc = k * r[119], the first row's output into the second
			c.queue_d(60, c.captured(m[60], st));
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
		case 0x1b9:  // ins::ER_ROW_STEPS[5]: r[119] = saturate(2 * (k * m[19] + acc)) over the first row's output, so the two rows are cascaded
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[19];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1ba:  // ins::ER_ROW_STEPS[6]: acc = 0 * r[119]
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
		case 0x1bb:  // ins::ER_ROW_STEPS[7]: the write of the slot 0x6e, ins::ER_ROW_SLOTS[1], k * m[19] + acc
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[19];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x6e, 0, false)] = outv;
			break;

		// ---- 0x1bc to 0x1c5, the ten left-channel gains, ins::ER_STEPS[0] to [9], and their sum into the cell m[43] ----
		case 0x1bc:  // ins::ER_STEPS[0] on m[22], the first left tap, which the base program's step 0x118 captured from the slot 0x44; the read of the slot 0x6f, ins::ER_SLOTS[11], the sixth right tap
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[22];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bd:  // ins::ER_STEPS[2] on m[23], the second left tap
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[23];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1be:  // ins::ER_STEPS[4] on m[24], the third
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[24];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1bf:  // ins::ER_STEPS[6] on m[25], the fourth; the read of the slot 0x70, ins::ER_SLOTS[13], the seventh right tap
			c.push_hist(st, c.ram[c.addr(0x6f, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[25];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c0:  // ins::ER_STEPS[8] on m[26], the fifth
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[26];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c1:  // ins::ER_STEPS[10] on m[27], the sixth
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[27];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c2:  // m[4] takes the left serial input of page 14, which no step of this program reads; ins::ER_STEPS[12] on m[28], the seventh
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[28];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c3:  // ins::ER_STEPS[14] on m[29], the eighth; the read of the slot 0x72, ins::ER_SLOTS[15], the eighth right tap
			c.push_hist(st, c.ram[c.addr(0x70, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[29];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c4:  // m[61] takes the seventh right tap, read at 0x1bf; ins::ER_STEPS[16] on m[30], the ninth
			c.queue_d(61, c.captured(m[61], st));
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[30];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c5:  // ins::ER_STEPS[18] on m[31], the tenth left tap, the slot 0x64 by then; m[43] takes the whole sum, and the step latches it for 0x1c7
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[31];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.queue_d(43, yss236_core::clip(y, yss236_core::DSAT));
			c.wlatch = outv; c.haswl = true;
			break;

		// ---- 0x1c6 to 0x1d1, the write of that sum into the slot 0x71, and the eighth, ninth and tenth right taps ----
		case 0x1c6:  // class 0 with no memory op and no ALU: the step counts and nothing else
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c7:  // the write of the latched left sum into the slot 0x71, ins::ER_DIFF_SLOTS[1], which the DIFFUSION case allots its allpass point 0x75 beside
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			if (c.haswl) { c.ram[c.addr(0x71, 0, false)] = c.wlatch; c.haswl = false; }
			break;
		case 0x1c8:  // m[62] takes the eighth right tap, read at 0x1c3
			c.queue_d(62, c.captured(m[62], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c9:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ca:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cb:  // the read of the slot 0x73, ins::ER_SLOTS[17], the ninth right tap
			c.push_hist(st, c.ram[c.addr(0x72, 0, false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cc:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cd:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ce:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cf:  // the read of the slot 0x74, ins::ER_SLOTS[19], the tenth right tap
			c.push_hist(st, c.ram[c.addr(0x73, 0, false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1d0:  // m[63] takes the ninth right tap, read at 0x1cb
			c.queue_d(63, c.captured(m[63], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1d1:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;

		// ---- 0x1d2 to 0x1db, the ten right-channel gains, ins::ER_STEPS[10] to [19], and their sum into the cell m[44] ----
		case 0x1d2:  // ins::ER_STEPS[10] on m[56], the first right tap, which 0x1a8 captured from the slot 0x68
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d3:  // ins::ER_STEPS[11] on m[57], the second; the read of the slot 0x75, ins::ER_DIFF_SLOTS[3], the allpass point of the slot 0x71, which holds the left sum
			c.push_hist(st, c.ram[c.addr(0x74, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d4:  // m[56] takes the ninth right tap, read at 0x1cf; ins::ER_STEPS[12] on m[58], the third
			c.queue_d(56, c.captured(m[56], st));
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d5:  // ins::ER_STEPS[13] on m[59], the fourth
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d6:  // ins::ER_STEPS[14] on m[60], the fifth
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d7:  // ins::ER_STEPS[15] on m[61], the sixth; the read of the slot 0x65, ins::ER_DIFF_SLOTS[2], the allpass point of the slot 0x76
			c.push_hist(st, c.ram[c.addr(0x75, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[61];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d8:  // m[57] takes the tenth right tap, read at 0x1d3; ins::ER_STEPS[16] on m[62], the seventh
			c.queue_d(57, c.captured(m[57], st));
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[62];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1d9:  // ins::ER_STEPS[17] on m[63], the eighth
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[63];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1da:  // ins::ER_STEPS[18] on m[56], the ninth by now: 0x1dc has not replaced it yet
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1db:  // ins::ER_STEPS[19] on m[57], the tenth, which erPattern() leaves at zero; m[44] takes the sum and this step writes it into the slot 0x76, ins::ER_DIFF_SLOTS[0]
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.queue_d(44, yss236_core::clip(y, yss236_core::DSAT));
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x76, 0, false)] = outv;
			break;

		// ---- 0x1dc to 0x1df, the two sums through their allpass points, ins::ER_DIFF_SLOTS, into r[120] and r[121] ----
		case 0x1dc:  // m[56] takes the slot 0x75's value, the left sum a Diffusion delay back; the ROM word 0x7fff on m[43]: acc = 0.999969 * the left sum
			c.queue_d(56, c.captured(m[56], st));
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1dd:  // r[121] = saturate(0.999969 * m[33] + acc): the left sum with m[33], which the base program's step 0x19c captured from the slot 0x65, added
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[33];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1de:  // the ROM word 0x7fff on m[44]: acc = 0.999969 * the right sum
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[44];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1df:  // the read of the slot 0x77, ins::ER_ROW_SLOTS[7]; r[120] = saturate(0.999969 * m[56] + acc): the right sum with itself a Diffusion delay back
			c.push_hist(st, c.ram[c.addr(0x77, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e0 to 0x1e5, the reads of the two allpass points of the last two rows ----
		case 0x1e0:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e1:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e2:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e3:  // the read of the slot 0x78, ins::ER_ROW_SLOTS[5], the allpass point of the slot 0x7a
			c.push_hist(st, c.ram[c.addr(0x78, 0, false)]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e4:  // m[57] takes that read
			c.queue_d(57, c.captured(m[57], st));
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e5:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;

		// ---- 0x1e6 to 0x1e9, the right sum's allpass and line: the slots 0x77 and 0x79 ----
		case 0x1e6:  // ins::ER_ROW_STEPS[14]: acc = k * r[120], the right sum's input
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e7:  // ins::ER_ROW_STEPS[15]: the write of the slot 0x79, ins::ER_ROW_SLOTS[6], k * m[57] + acc
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x79, 0, false)] = outv;
			break;
		case 0x1e8:  // m[58] takes the slot 0x78's value, which no earlier step has read into a cell; ins::ER_ROW_STEPS[12]: acc = k * r[120], the allpass's feedback
			c.queue_d(58, c.captured(m[58], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1e9:  // ins::ER_ROW_STEPS[13]: r[120] = saturate(2 * (k * m[57] + acc)), the right sum through the allpass
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ea to 0x1ed, the left sum's allpass and line: the slots 0x78 and 0x7a ----
		case 0x1ea:  // ins::ER_ROW_STEPS[10]: acc = k * r[121], the left sum's input
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1eb:  // ins::ER_ROW_STEPS[11]: the write of the slot 0x7a, ins::ER_ROW_SLOTS[4], k * m[58] + acc
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7a, 0, false)] = outv;
			break;
		case 0x1ec:  // ins::ER_ROW_STEPS[8]: acc = k * r[121], the allpass's feedback
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ed:  // ins::ER_ROW_STEPS[9]: r[121] = saturate(2 * (k * m[58] + acc)), the left sum through the allpass
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ee to 0x1ef, the delay line's input, r[119] and the FB Level word on r[75] ----
		case 0x1ee:  // the ROM word 0x7fff: acc = 0.999969 * r[119], the two diffusion rows' output
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
		case 0x1ef:  // ins::ER_FEEDBACK[0], the FB Level word: the write of the slot 0x7b, ins::ER_SLOTS[21], k * r[75] + acc, which is the delay line the ten right taps are read from
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
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
			c.ram[c.addr(0x7b, 0, false)] = outv;
			break;

		// ---- 0x1f0 to 0x1f7, the dry and wet mix, ins::ER_WET, and two writes of zero ----
		case 0x1f0:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f1:  // ins::ER_WET[0]: acc = k * r[7], the left dry share
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f2:  // ins::ER_WET[1]: r[17] = saturate(4 * (k * r[121] + acc)), the window's left output
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f3:  // the write of the slot 0x7c, ins::ER_DIFF_SLOTS' area's neighbour: k * m[20], and the word here is zero, so the slot takes zero every pass
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[20];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7c, 0, false)] = outv;
			break;
		case 0x1f4:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f5:  // ins::ER_WET[2]: acc = k * r[8], the right dry share
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
		case 0x1f6:  // ins::ER_WET[3]: r[18] = saturate(4 * (k * r[120] + acc)), the right output
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		case 0x1f7:  // the write of the slot 0x7d: k * m[21], zero like 0x1f3 and read by no step of this program
			k = yss236_core::s16(c.coef[st]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[21];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			outv = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = false;
			if (c.haswl) { outv = c.wlatch; c.haswl = false; }
			c.ram[c.addr(0x7d, 0, false)] = outv;
			break;

		// ---- 0x1f8 to 0x1ff, eight steps that count and nothing else ----
		case 0x1f8:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f9:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fa:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fb:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fc:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fd:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fe:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ff:  // class 0, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;

		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m03(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

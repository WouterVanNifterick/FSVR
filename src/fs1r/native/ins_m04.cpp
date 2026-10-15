// fs1r/native/ins_m04.cpp - VOP3-2, the insertion window's program for Auto Wah + Distortion, Auto Wah +
// Overdrive, Touch Wah + Distortion and Touch Wah + Overdrive: its 96 steps.
//
// rom::INSERTION_PROGRAMS[4], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 4: ins-14 A-Wah+Dist and ins-15 A-Wah+Odrv on
// EffectHost::autoWah, ins-16 T-Wah+Dist and ins-17 T-Wah+Odrv on EffectHost::touchWah. The same 96
// words run under every one of them; what the type changes is which words fx_drive.cpp's two handlers
// write, so nothing here knows which one is running. Nothing in this program touches the delay RAM.
//
// Every constant the program reads is read from c.coef[st] on the pass and every delay offset from
// the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here: the
// program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
//   0x1a0-0x1ad  the two channels' EQ, two order-1 shelves each, the low and the high shelf of
//                fx_common.cpp's shelves() over ins::LPF_WAH, ins::HPF_WAH and their copies
//                ins::LPF_WAH_C and ins::HPF_WAH_C. 0x1a0 and 0x1a7 take the two inputs the base
//                program leaves the window into r[67] and r[69], and 0x1a5, 0x1a6 and 0x1a7 take the
//                serial inputs of page 13 and that page's LFO into m[3], m[7] and m[13].
//   0x1ae-0x1b5  the wah itself. ins::WAH_MODE's two words make r[76], the mode pair, and publish
//                the dry signals' copies into m[50] and m[51]; ins::WAH_HPF_2 is the highpass at
//                0x1b0 to 0x1b2 over r[76] into r[77] and ins::WAH_LPF_2 the lowpass at 0x1b3 to
//                0x1b5 over r[77] into r[78].
//   0x1b6-0x1bb  ins::WAH_MODE2's pair, which only touchWah() writes, and ins::WAH_HPF, the input
//                highpass it designs, over r[71] into r[72]. Under Auto Wah all six words are zero
//                and this path is silent.
//   0x1b8-0x1c0  the drive. ins::WAH_DRIVE's first word at 0x1b8 carries the signal into r[122], and
//                the two drive words fx_common.cpp's storeDrive() writes, 0x1bc and 0x1c0, square it.
//                0x1bd and 0x1be are ins::WAH_SENSE's pair on the left, 0x1c1 and 0x1c2 the same two
//                steps on the right, and 0x1bf ins::WAH_ATTACK's word times the highpass's output.
//   0x1c3-0x1cc  ins::WAH_MIRROR's three words: the rectifier round the sensitivity level m[56] and
//                the pair m[46], m[47] that carries it.
//   0x1cd-0x1d9  the smoother and the band shaper's first three words. 0x1ce is ins::WAH_CUTOFF's
//                class-1 load and 0x1cf ins::WAH_DEPTH's word times the LFO in m[13]; 0x1d0 to 0x1d7
//                are the one-pole smoother that turns the cutoff into t[9]; 0x1cd, 0x1d1, 0x1d4 and
//                0x1d8 are four of ins::WAH_BAND's twelve words.
//   0x1da-0x1e2  the resonator and the rest of ins::WAH_BAND. 0x1da and 0x1db are t[9] times m[58]
//                and m[59] plus the states r[74] and r[75], and 0x1dc to 0x1e2 are the remaining
//                eight words, which put the shaper's output into r[122] again.
//   0x1e3-0x1e5  the two feedback accumulators m[44] and m[45], and ins::WAH_DRIVE's fourth word,
//                the gain fx_common.cpp's storeDrive() derives from the drive.
//   0x1e6-0x1ea  ins::WAH_LPF2, the lowpass fx_drive.cpp's wahDistortionTone() designs: b0 at 0x1ea,
//                b1 at 0x1e7, b2 at 0x1e6, -a1 at 0x1e9 and -a2 at 0x1e8, over r[79] into r[81].
//   0x1eb-0x1ef  ins::WAH_PEAK, the same five steps over r[81] into r[83].
//   0x1f0-0x1f4  ins::WAH_LOW_SHELF2, the same five steps over r[83] into r[85].
//   0x1f5-0x1fc  ins::WAH_WET's eight words, the dry and the wet into the window's outputs r[17] and
//                r[18].
//   0x1fd-0x1ff  three class-0 steps with no ALU, so only the x latch runs.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] to r[70] the
// two channels' EQ, input, low shelf and high shelf; r[71], r[72] the input highpass; r[74], r[75] the
// resonator's states; r[76] to r[78] the wah's mode pair, highpass and lowpass; r[79], r[81], r[83],
// r[85] the three tone filters; r[73] the rectifier's output; r[121] the smoother's state; r[122] the
// drive and then the shaper's output; r[123] to r[125] the shaper's accumulator chain; r[80], r[82],
// r[84], r[86] the tone filters' delays; r[17] and r[18] the two outputs. The cells: m[3], m[4] the
// serial inputs this block takes for the base program, m[7] the second bank's, m[13] the page's LFO,
// m[42] and m[43] the two low shelves' outputs, m[44] and m[45] the resonator's feedback
// accumulators, m[46] and m[47] the pair the rectifier's carrier passes through, m[48] and m[49] the
// two channels' wet signals, m[50] and m[51] their dry copies, m[56] to m[59] the sensitivity level
// and the two states that carry it, and m[57] the shaper's fifth word.
//
// The one t register the program uses is t[9], written at 0x1d0, 0x1d3 and 0x1d7 and read at 0x1d3,
// 0x1d6, 0x1da, 0x1db, 0x1e3 and 0x1e4: the cutoff the host wrote at 0x1ce, then the smoother's gain.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

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
	double xv, k, g, m2, prod, a, y, v;    // the step's operands and its result

	// The step's x operand: the register r[sr] into the latch every step but a class-2/3 one moves, and
	// xv alongside it for the multiplier.
	auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };
	// The t file write of a step whose result is y; every write here takes the register form.
	auto twr = [&](double yy, int tn) {
		double const w = yy * 256.0;
		c.queue_t(tn, (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0);
	};

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		// ---- 0x1a0 to 0x1ad, the two channels' EQ: r[67] to r[70], four order-1 shelves ----
		// r[67] = saturate(2 * k * r[7]), the left input through the ROM's own half word
		case 0x1a0:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::LPF_WAH[1], b1 on the left low shelf: acc = k * r[67]
		case 0x1a1:
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::LPF_WAH[2], -a1: acc = k * m[42] + acc
		case 0x1a2:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[42];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::LPF_WAH[0], b0: m[42] = clip(saturate(2 * (k * r[67] + acc)), +-128), the shelf's output
		case 0x1a3:
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(42, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::HPF_WAH[1], b1 on the left high shelf: acc = k * m[42]
		case 0x1a4:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[42];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[3] takes the left serial input of page 13; ins::HPF_WAH[2], -a1: acc = k * r[68] + acc
		case 0x1a5:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[7] takes the right serial input of page 13; ins::HPF_WAH[0], b0: r[68] = saturate(4 * (k * m[42] + acc))
		case 0x1a6:
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[42];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[13] takes the page's LFO; r[69] = saturate(2 * k * r[8]), the right input
		case 0x1a7:
			c.queue_d(13, c.lfo[13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::LPF_WAH_C[1], b1 on the right low shelf: acc = k * r[69]
		case 0x1a8:
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::LPF_WAH_C[2], -a1: acc = k * m[43] + acc
		case 0x1a9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[43];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::LPF_WAH_C[0], b0: m[43] takes the right shelf's output
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(43, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::HPF_WAH_C[1], b1: acc = k * m[43]
		case 0x1ab:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[43];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::HPF_WAH_C[2], -a1: acc = k * r[70] + acc
		case 0x1ac:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::HPF_WAH_C[0], b0: r[70] = saturate(4 * (k * m[43] + acc)), the right channel's EQ output
		case 0x1ad:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[43];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ae to 0x1b5, the wah: the mode pair r[76], the highpass r[77], the lowpass r[78] ----
		// ins::WAH_MODE[0]: acc = k * m[48]
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[48];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_MODE[1]: r[76] = saturate(k * m[49] + acc), and m[50] takes the step before's operand m[48]
		case 0x1af:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[49];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(50, c.psrc);
			break;
		// ins::WAH_HPF_2[1], b1: acc = k * r[76], and m[51] takes the step before's operand m[49]
		case 0x1b0:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(51, c.psrc);
			break;
		// ins::WAH_HPF_2[2], -a1: acc = k * r[77] + acc
		case 0x1b1:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_HPF_2[0], b0: r[77] = saturate(2 * (k * r[76] + acc)), the highpass's output
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_LPF_2[1], b1: acc = saturate(k * r[77])
		case 0x1b3:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_LPF_2[2], -a1: acc = k * r[78] + acc
		case 0x1b4:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LPF_2[0], b0: r[78] = saturate(2 * (k * r[77] + acc)), the wah's output
		case 0x1b5:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1b6 to 0x1bb, ins::WAH_MODE2's pair and ins::WAH_HPF, touchWah()'s input highpass ----
		// ins::WAH_MODE2[0]: acc = k * r[68]; Auto Wah leaves the word at zero
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_MODE2[1]: r[71] = saturate(k * r[70] + acc)
		case 0x1b7:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_DRIVE[0]: r[122] = saturate(16 * k * r[78]), the signal into the drive
		case 0x1b8:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_HPF[1], b1: acc = k * r[71]
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_HPF[2], -a1: acc = k * r[72] + acc
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			fetch(72);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_HPF[0], b0: r[72] = saturate(2 * (k * r[71] + acc)), the input highpass's output
		case 0x1bb:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1bc to 0x1c2, the drive's two words and ins::WAH_SENSE's pair on both channels ----
		// ins::WAH_DRIVE[1]: r[122] = saturate(16 * k * r[122]), the drive's first square
		case 0x1bc:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// acc = k * m[44] - r[68], the left sense step's first half
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[44];
			m2 = c.src; prod = k * m2;
			a = xv;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_SENSE[0]: acc = saturate(k * r[74] - acc), and m[58] takes it
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			fetch(74);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(58, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::WAH_ATTACK: m[56] = |16 * k * r[72]|, the Touch Wah's sensitivity level
		case 0x1bf:
			k = yss236_core::s16(c.coef[st]);
			fetch(72);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 3);
			c.queue_d(56, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::WAH_DRIVE[2]: r[122] = saturate(16 * k * r[122]), the drive's second square
		case 0x1c0:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// acc = k * m[45] - r[70], the right sense step's first half
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[45];
			m2 = c.src; prod = k * m2;
			a = xv;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_SENSE[1]: acc = saturate(k * r[75] - acc), and m[59] takes it
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(59, yss236_core::clip(y, yss236_core::DSAT));
			break;

		// ---- 0x1c3 to 0x1cc, ins::WAH_MIRROR's three words and the pair m[46], m[47] ----
		// ins::WAH_MIRROR[0]: acc = -(m[56]) with the negatives clamped away
		case 0x1c3:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[56];
			m2 = xv; prod = k * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// m[4] takes the left serial input of page 14; r[73] = saturate(k * m[56] + acc)
		case 0x1c4:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// acc = k * m[46], the word here is zero
		case 0x1c5:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// acc = k * m[46] + acc / 32768, the adder reads r[0] and there is none
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// acc = k * m[56] - acc
		case 0x1c7:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_MIRROR[2]: acc = -(k * r[73] + acc) with the negatives clamped away
		case 0x1c8:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// m[46] = saturate(k * m[56] + acc)
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[56];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(46, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// acc = k * m[46], the word here is zero
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// acc = k * m[47] + acc
		case 0x1cb:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[47];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[47] = saturate(k * m[46] + acc)
		case 0x1cc:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(47, yss236_core::clip(y, yss236_core::DSAT));
			break;

		// ---- 0x1cd to 0x1d9, the cutoff, the smoother, and four of ins::WAH_BAND's twelve words ----
		// ins::WAH_BAND[1]: r[123] = saturate(16 * k * r[122]), the shaper's gain word
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_CUTOFF: the class-1 load of the cutoff into the running value
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * k + a;
			v *= 1.0;
			y = c.acc = yss236_core::accmode(v, 0);
			break;
		// ins::WAH_DEPTH: acc = k * m[13] + acc, the LFO's depth on the cutoff
		case 0x1cf:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[13];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// r[121] = saturate(k * m[47] + acc) and t[9] takes it: the smoother's state
		case 0x1d0:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[47];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 9);
			break;
		// ins::WAH_BAND[2]: r[124] = saturate(16 * k * r[123]), the shaper's first edge word
		case 0x1d1:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// class 0 with no ALU: only the x latch
		case 0x1d2:
			fetch(0);
			break;
		// r[121] = t[9] * r[121] and t[9] takes it again: the smoother's gain
		case 0x1d3:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = g * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			twr(y, 9);
			break;
		// ins::WAH_BAND[3]: r[125] = saturate(16 * k * r[124]), the shaper's second edge word
		case 0x1d4:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(125, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// class 0 with no ALU: only the x latch
		case 0x1d5:
			fetch(0);
			break;
		// acc = t[9] * r[121], the smoother's other half
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(121);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = g * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the class-1 load that adds half: t[9] takes saturate(0.5 + acc)
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			v *= 1.0;
			y = c.acc = yss236_core::accmode(v, 1);
			twr(y, 9);
			break;
		// ins::WAH_BAND[4]: m[57] takes saturate(16 * k * r[125]), the shaper's third edge word
		case 0x1d8:
			k = yss236_core::s16(c.coef[st]);
			fetch(125);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(57, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// class 0 with no ALU: only the x latch
		case 0x1d9:
			fetch(0);
			break;

		// ---- 0x1da to 0x1e2, the resonator and ins::WAH_BAND's remaining eight words ----
		// r[74] = saturate(t[9] * m[58] + r[74]), the left state the cutoff's gain drives
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(74);
			c.psrc = c.src; c.src = m[58];
			m2 = c.src; prod = g * m2;
			a = xv;
			y = (prod + a) * 1.0;
			c.queue_r(74, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// r[75] = saturate(t[9] * m[59] + r[75]), the right state
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(75);
			c.psrc = c.src; c.src = m[59];
			m2 = c.src; prod = g * m2;
			a = xv;
			y = (prod + a) * 1.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_BAND[5]: r[122] = saturate(16 * k * m[57]), the shaper's fourth edge word
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[57];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// acc = k * r[122], the word here is zero
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_BAND[7]: acc = k * r[123]
		case 0x1de:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_BAND[8]: acc = k * r[124]
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_BAND[9]: acc = k * r[125]
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			fetch(125);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// acc = k * m[57] + acc
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[57];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_BAND[10]: r[122] = saturate(k * r[122] + acc), the shaper's output
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e3 to 0x1e5, the two feedback accumulators and ins::WAH_DRIVE's fourth word ----
		// m[44] = saturate(t[9] * r[74] + m[44]), the left feedback accumulator
		case 0x1e3:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(74);
			c.psrc = c.src; c.src = m[44];
			m2 = xv; prod = g * m2;
			a = c.src;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(44, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// m[45] = saturate(t[9] * r[75] + m[45]), the right one
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			g = c.t[9];
			fetch(75);
			c.psrc = c.src; c.src = m[45];
			m2 = xv; prod = g * m2;
			a = c.src;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(45, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::WAH_DRIVE[3]: r[79] = saturate(k * r[122]), the drive's gain into the tone filters
		case 0x1e5:
			k = yss236_core::s16(c.coef[st]);
			fetch(122);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1e6 to 0x1ea, ins::WAH_LPF2, the lowpass fx_drive.cpp's wahDistortionTone() designs ----
		// ins::WAH_LPF2[2], b2: acc = k * r[80]
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			fetch(80);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LPF2[1], b1: acc = k * r[79] + acc
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LPF2[4], -a2: acc = k * r[82] + acc, and r[80] takes the step before's x
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			fetch(82);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(80, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LPF2[3], -a1: acc = k * r[81] + acc
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LPF2[0], b0: r[81] = saturate(2 * (k * r[79] + acc)), the lowpass's output
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1eb to 0x1ef, ins::WAH_PEAK, the same five steps over r[81] into r[83] ----
		// ins::WAH_PEAK[1], b1: acc = k * r[81]
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_PEAK[2], b2: acc = k * r[82] + acc, and r[82] takes the step before's x
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			fetch(82);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(82, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_PEAK[4], -a2: acc = k * r[84] + acc
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			fetch(84);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_PEAK[3], -a1: acc = k * r[83] + acc
		case 0x1ee:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_PEAK[0], b0: r[83] = saturate(2 * (k * r[81] + acc)), the peak's output
		case 0x1ef:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(83, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f0 to 0x1f4, ins::WAH_LOW_SHELF2, the same five steps over r[83] into r[85] ----
		// ins::WAH_LOW_SHELF2[1], b1: acc = k * r[83]
		case 0x1f0:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LOW_SHELF2[2], b2: acc = k * r[84] + acc, and r[84] takes the step before's x
		case 0x1f1:
			k = yss236_core::s16(c.coef[st]);
			fetch(84);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(84, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LOW_SHELF2[3], -a1: acc = k * r[85] + acc
		case 0x1f2:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LOW_SHELF2[4], -a2: acc = k * r[86] + acc, and r[86] takes the step before's x
		case 0x1f3:
			k = yss236_core::s16(c.coef[st]);
			fetch(86);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(86, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_LOW_SHELF2[0], b0: r[85] = saturate(16 * (k * r[83] + acc)), the low shelf's output
		case 0x1f4:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(85, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1f5 to 0x1fc, ins::WAH_WET's eight words: the dry, the wet, and the two outputs ----
		// ins::WAH_WET[0]: acc = k * r[68], the left dry word
		case 0x1f5:
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_WET[1]: m[48] = saturate(4 * (k * r[74] + acc)), the left wet signal
		case 0x1f6:
			k = yss236_core::s16(c.coef[st]);
			fetch(74);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(48, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::WAH_WET[4]: acc = k * r[70], the right dry word
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_WET[5]: m[49] = saturate(4 * (k * r[75] + acc)), the right wet signal
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(49, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::WAH_WET[2]: acc = k * m[50], the left dry signal again
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[50];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_WET[3]: r[17] = saturate(4 * (k * r[85] + acc)), the left output
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::WAH_WET[6]: acc = k * m[51], the right dry signal
		case 0x1fb:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[51];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::WAH_WET[7]: r[18] = saturate(4 * (k * r[85] + acc)), the right output
		case 0x1fc:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1fd to 0x1ff, three class-0 steps with no ALU, so only the x latch runs ----
		case 0x1fd:
			fetch(0);
			break;
		case 0x1fe:
			fetch(0);
			break;
		case 0x1ff:
			fetch(0);
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m04(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

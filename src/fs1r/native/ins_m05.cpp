// fs1r/native/ins_m05.cpp - VOP3-2, the insertion window's program for Distortion, Overdrive, Amp Sim,
// Compressor, Comp+Dist, HM Enhancer and Noise Gate: its 96 steps.
//
// rom::INSERTION_PROGRAMS[5], the program the insertion window holds under the seven types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 5: ins-28 Distortion and ins-30 Overdrive on
// EffectHost::distortion, ins-32 Amp Sim on EffectHost::ampSimulator, ins-24 Compressor on
// EffectHost::compressor, ins-25 Comp+Dist on EffectHost::compressorDistortion, ins-22 HM Enhancer on
// EffectHost::enhancer and ins-23 Noise Gate on EffectHost::noiseGate. One program, seven types: the
// handlers differ only in which words they write, so nothing here knows which of them is running.
// Nothing in this program touches the delay RAM, and no step of it is a control step.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware
// chose is folded in here: the program words are fixed per row and each step's fields are folded into
// the arithmetic below in the order yss236_core::step() runs its own.
//
//   0x1a0-0x1af  the inputs and the two channels' input filters. 0x1a0, 0x1a7 and 0x1aa are
//                ins::DIST_DRIVE's first three words, the signal into the shaper multiplied by the
//                drive three times. 0x1a1 takes the right input the base program leaves the window
//                into r[71] and 0x1a8 and 0x1a9 the left one into r[67], both through the ROM's own
//                fixed words. 0x1a2 to 0x1a6 are ins::DIST_INPUT_C, the right channel's order-2 input
//                filter over r[71] into r[73], and 0x1ab to 0x1af ins::DIST_INPUT, the left one's
//                over r[67] into r[69]. 0x1a5, 0x1a6 and 0x1a7 also take the two serial inputs of page
//                13 into m[3] and m[7] and that page's LFO into m[13], which nothing here reads: the
//                base program does.
//   0x1b0-0x1bb  the shaper and the detector. 0x1b0 is ins::DIST_BAND[1], the gain word of the shape
//                storeEdge() derives, and 0x1b1 and 0x1b2 are ins::HM_DRIVE, which scale the two
//                channels' filtered signals into r[120] and r[119] for the detector. 0x1b4 and 0x1b5
//                are ins::DIST_LEVEL, the two words that add the two channels and rectify them into
//                m[49], the level. 0x1b8 to 0x1ba are ins::DYN_ATTACK, the smoother storeCompAttack()
//                or storeGateAttack() writes, and 0x1bb takes m[42] one pass old into m[43].
//   0x1bc-0x1c1  the detector's two states and the release. 0x1bc and 0x1c1 are class-3 steps, which
//                take m[49] at unity gain, and 0x1bf the third: they add the level to what the
//                release words left. 0x1b6, 0x1be and 0x1c0 are ins::DYN_RELEASE.
//   0x1c2-0x1cb  the shaper's sum and the drive's gain. 0x1c2 to 0x1c8 are ins::DIST_BAND[5] to
//                [11], the polynomial the shaper adds to its chain: 0x1c2 finishes the monomial and
//                0x1c3 to 0x1c8 weigh the chain's registers and add them into r[1]. 0x1c9 and 0x1ca
//                multiply the level by t[9] and pass each on in t[10] and t[11], and 0x1cb is
//                ins::DIST_DRIVE[3], the gain storeDrive() derives from the drive and the output level.
//   0x1cc-0x1db  the threshold and the three tone filters. 0x1cc is ins::DYN_THRESHOLD, over r[76]
//                into r[123]. 0x1cd to 0x1d1 are ins::DIST_PRE and 0x1d2 to 0x1d6 ins::DIST_PRE_C, the
//                same five-word filter twice over r[79], and 0x1d7 to 0x1db ins::DIST_EQ over r[83]
//                into r[85]. designFilter() writes b0, b1, b2, -a1, -a2 in that order, and the two
//                z^-1 words of each section are the steps that carry rcopy.
//   0x1dc-0x1ec  the output level and the output mix. 0x1dc and 0x1e6 are the ROM's own pair: they
//                multiply r[119] and r[120] by the gains t[10] and t[11] into r[126] and r[127]. 0x1dd
//                and 0x1de are ins::DIST_MODE, and 0x1df to 0x1e2 ins::DIST_OUTPUT, the filter over
//                r[126] whose state is m[45]; 0x1e3 to 0x1e5 are ins::DIST_WET, the left mix into r[17]
//                and the right one into r[18]. 0x1e7 to 0x1ec are the same four steps on the right:
//                ins::DIST_MODE is the ROM's own word here, ins::DIST_OUTPUT_C over r[127] into m[46],
//                and r[18] takes that.
//   0x1ed-0x1ff  the gain curve. 0x1ed to 0x1f4 are eight class-1 steps off r[123], each clamping
//                negatives to zero, and they are the curve's breakpoints: r[123] less a quarter, a
//                half, one, two, four, eight, sixteen and sixty-four. 0x1f5 to 0x1fc are ins::DYN_RATIO,
//                the eight weights of the segments between them, summed into the running value, and
//                0x1fd takes that biased and clipped into m[42]. 0x1fe and 0x1ff are ins::DIST_WET's
//                dry words, a quarter of the two inputs into m[47] and m[48].
//
// So the seven types are one chain: a fixed two-channel input filter, a peak detector on the sum of
// the two channels, an eight-segment gain curve off the detector, a smoother, a five-multiply shape,
// three tone filters and a dry and wet mix. The gate and the compressor use the whole of it; the
// distortion and the enhancer reach it through ins::DIST_MODE. The old model in vop3_effects.h runs
// tanh and a power law for the shape, one envelope and a hard ratio for the dynamics, and a shelf
// plus a peak; it has none of this program's words, so it is no reference for any number here.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[1] to r[6] the
// shape's chain and then the curve's breakpoints; r[17] and r[18] the two outputs; r[67] to r[74] the
// two input filters and their states; r[75], r[76] and r[121] the detector's states; r[79] the
// shaper's output; r[80], r[82], r[84], r[86] the three tone filters' delays; r[81], r[83] the first
// two tone filters' outputs and r[85] the third's; r[119] to r[121] the detector's input scale; r[123]
// the thresholded level; r[124], r[125] the level times the smoother, and then the curve's last two
// breakpoints; r[126], r[127] the output's input scale. The cells: m[3], m[7] the serial inputs this
// block takes for the base program and m[13] the page's LFO; m[42], m[43] the curve's output and that
// one pass old; m[44] the smoother's state; m[45] the left output filter's state; m[46] the right
// one's; m[47], m[48] the two channels' dry signals; m[49] the level.
//
// The three t registers the program uses are t[9], written at 0x1ba, read at 0x1c9 and 0x1ca, the
// smoother's gain; and t[10] and t[11], written at 0x1c9 and 0x1ca and read at 0x1dc and 0x1e6, that
// same gain squared onto the two channels.
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

		// ---- 0x1a0 to 0x1af, the inputs, the two input filters and the drive's first words ----
		// ins::DIST_DRIVE[0]: r[1] = saturate(16 * k * m[45]), the signal into the shaper
		case 0x1a0:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[45];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the ROM's own word: r[71] = saturate(2 * k * r[8]), the right input
		case 0x1a1:
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(71, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_INPUT_C[1], b1: acc = k * r[71]
		case 0x1a2:
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT_C[2], b2: acc = k * r[72] + acc, and r[72] takes the step before's x
		case 0x1a3:
			k = yss236_core::s16(c.coef[st]);
			fetch(72);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(72, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT_C[3], -a1: acc = k * r[73] + acc
		case 0x1a4:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[3] takes the left serial input of page 13; ins::DIST_INPUT_C[4], -a2: acc = k * r[74] + acc,
		// and r[74] takes the step before's x
		case 0x1a5:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(74);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(74, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[7] takes the right serial input of page 13; ins::DIST_INPUT_C[0], b0:
		// r[73] = saturate(2 * (k * r[71] + acc)), the right channel's filtered input
		case 0x1a6:
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			fetch(71);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[13] takes the page's LFO; ins::DIST_DRIVE[1]: r[1] = saturate(16 * k * r[1])
		case 0x1a7:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the ROM's own word: acc = k * r[7]
		case 0x1a8:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: r[67] = saturate(2 * (k * r[8] + acc)), the left input
		case 0x1a9:
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(67, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_DRIVE[2], which is also ins::DIST_BAND[0]: r[1] = saturate(16 * k * r[1])
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_INPUT[1], b1: acc = k * r[67]
		case 0x1ab:
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT[2], b2: acc = k * r[68] + acc, and r[68] takes the step before's x
		case 0x1ac:
			k = yss236_core::s16(c.coef[st]);
			fetch(68);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(68, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT[3], -a1: acc = k * r[69] + acc
		case 0x1ad:
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT[4], -a2: acc = k * r[70] + acc, and r[70] takes the step before's x
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			fetch(70);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(70, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_INPUT[0], b0: r[69] = saturate(2 * (k * r[67] + acc)), the left channel's filtered input
		case 0x1af:
			k = yss236_core::s16(c.coef[st]);
			fetch(67);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(69, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1b0 to 0x1bb, the shape's chain and the level detector ----
		// ins::DIST_BAND[1], the gain word storeEdge() derives: r[2] = saturate(16 * k * r[1])
		case 0x1b0:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(2, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::HM_DRIVE[0]: r[120] = saturate(2 * k * r[73])
		case 0x1b1:
			k = yss236_core::s16(c.coef[st]);
			fetch(73);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::HM_DRIVE[1]: r[119] = saturate(2 * k * r[69])
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			fetch(69);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_BAND[2], the first shape word: r[3] = saturate(16 * k * r[2])
		case 0x1b3:
			k = yss236_core::s16(c.coef[st]);
			fetch(2);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(3, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_LEVEL[1], b1: acc = k * r[120]
		case 0x1b4:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_LEVEL[0], b0: m[49] = clip(16 * (k * r[119] + acc), +-128),
		// the level: the two channels summed and rectified
		case 0x1b5:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 3);
			c.queue_d(49, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::DYN_RELEASE[0]: r[121] = max(0, k * r[75] - acc)
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 2));
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// ins::DIST_BAND[3], the second shape word: r[4] = saturate(16 * k * r[3])
		case 0x1b7:
			k = yss236_core::s16(c.coef[st]);
			fetch(3);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(4, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DYN_ATTACK[1]: acc = k * m[43]
		case 0x1b8:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[43];
			m2 = c.src; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_ATTACK[2]: acc = k * m[44] + acc
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[44];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_ATTACK[0]: m[44] = clip(k * m[42] + acc, +-128) and t[9] takes it, the smoother's state
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[42];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(44, yss236_core::clip(y, yss236_core::DSAT));
			twr(y, 9);
			break;
		// ins::DIST_BAND[4], the third shape word: r[5] = saturate(16 * k * r[4]),
		// and m[43] takes the step before's operand m[42]
		case 0x1bb:
			k = yss236_core::s16(c.coef[st]);
			fetch(4);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(5, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(43, c.psrc);
			break;

		// ---- 0x1bc to 0x1c1, the detector's two states and the release ----
		// class 3, which multiplies by one: r[75] = saturate(m[49] + r[121]), the detector's first state
		case 0x1bc:
			k = yss236_core::s16(c.coef[st]);
			fetch(121);
			c.psrc = c.src; c.src = m[49];
			m2 = c.src; prod = m2;
			a = xv;
			y = (prod + a) * 1.0;
			c.queue_r(75, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the ROM's own word: acc = k * r[76]
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RELEASE[1]: acc = k * r[76] + acc / 32768
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// class 3, unity gain: acc = m[49] - acc
		case 0x1bf:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[49];
			m2 = c.src; prod = m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RELEASE[2]: acc = max(0, k * r[75] - acc)
		case 0x1c0:
			k = yss236_core::s16(c.coef[st]);
			fetch(75);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// class 3, unity gain: r[76] = saturate(m[49] + acc), the detector's second state
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[49];
			m2 = c.src; prod = m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1c2 to 0x1cb, the shape's sum, the level times the smoother and the drive's gain ----
		// ins::DIST_BAND[5], the fourth shape word: r[1] = saturate(16 * k * r[5])
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			fetch(5);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_BAND[6]: acc = k * r[1]
		case 0x1c3:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_BAND[7]: acc = k * r[2] + acc
		case 0x1c4:
			k = yss236_core::s16(c.coef[st]);
			fetch(2);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_BAND[8]: acc = k * r[3] + acc
		case 0x1c5:
			k = yss236_core::s16(c.coef[st]);
			fetch(3);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_BAND[9]: acc = k * r[4] + acc
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			fetch(4);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_BAND[10]: acc = k * r[5] + acc
		case 0x1c7:
			k = yss236_core::s16(c.coef[st]);
			fetch(5);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_BAND[11]: r[1] = saturate(k * r[1] + acc), the shaper's output
		case 0x1c8:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(1, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// r[124] = saturate(t[9] * r[119]) and t[10] takes it
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 10);
			break;
		// r[125] = saturate(t[9] * r[120]) and t[11] takes it
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(125, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			twr(y, 11);
			break;
		// ins::DIST_DRIVE[3]: r[79] = saturate(k * r[1]), the shaper's output at the drive's gain
		case 0x1cb:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(79, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1cc to 0x1db, the threshold and the three tone filters ----
		// ins::DYN_THRESHOLD: r[123] = saturate(k * r[76]), the level past the threshold
		case 0x1cc:
			k = yss236_core::s16(c.coef[st]);
			fetch(76);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_PRE[1], b1: acc = k * r[79]
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE[2], b2: acc = k * r[80] + acc, and r[80] takes the step before's x
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			fetch(80);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(80, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE[3], -a1: acc = k * r[81] + acc
		case 0x1cf:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE[4], -a2: acc = k * r[82] + acc
		case 0x1d0:
			k = yss236_core::s16(c.coef[st]);
			fetch(82);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE[0], b0: r[81] = saturate(2 * (k * r[79] + acc))
		case 0x1d1:
			k = yss236_core::s16(c.coef[st]);
			fetch(79);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(81, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_PRE_C[1], b1: acc = k * r[81]
		case 0x1d2:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE_C[2], b2: acc = k * r[82] + acc, and r[82] takes the step before's x
		case 0x1d3:
			k = yss236_core::s16(c.coef[st]);
			fetch(82);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(82, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE_C[3], -a1: acc = k * r[83] + acc
		case 0x1d4:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE_C[4], -a2: acc = k * r[84] + acc
		case 0x1d5:
			k = yss236_core::s16(c.coef[st]);
			fetch(84);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_PRE_C[0], b0: r[83] = saturate(2 * (k * r[81] + acc))
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			fetch(81);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(83, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_EQ[1], b1: acc = k * r[83]
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_EQ[2], b2: acc = k * r[84] + acc, and r[84] takes the step before's x
		case 0x1d8:
			k = yss236_core::s16(c.coef[st]);
			fetch(84);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(84, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_EQ[3], -a1: acc = k * r[85] + acc
		case 0x1d9:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_EQ[4], -a2: acc = k * r[86] + acc, and r[86] takes the step before's x
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			fetch(86);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(86, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_EQ[0], b0: r[85] = saturate(16 * (k * r[83] + acc)), the third filter's output
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			fetch(83);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(85, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1dc to 0x1ec, the output level, the output filters and the dry and wet mix ----
		// the ROM's own word: r[126] = saturate(t[10] * r[119])
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[10] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(126, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_MODE[0]: acc = k * r[124]
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_MODE[1]: acc = k * r[119] + acc
		case 0x1de:
			k = yss236_core::s16(c.coef[st]);
			fetch(119);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: r[77] = saturate(16 * (k * r[126] + acc))
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			fetch(126);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_OUTPUT[1], b1: acc = k * r[77]
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_OUTPUT[2], -a1: acc = k * m[45] + acc
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[45];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_OUTPUT[0], b0: m[45] = clip(2 * (k * r[77] + acc), +-128), the left output filter's state
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			fetch(77);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(45, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::DIST_WET[1]: r[17] = saturate(4 * (k * r[85] + m[47])), the left output
		case 0x1e3:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[47];
			m2 = xv; prod = k * m2;
			a = c.src;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_WET[4]: acc = k * r[85] + m[48]
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			fetch(85);
			c.psrc = c.src; c.src = m[48];
			m2 = xv; prod = k * m2;
			a = c.src;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_WET[3]: r[18] = saturate(4 * (k * m[46] + acc)), the right output
		case 0x1e5:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the ROM's own word: r[127] = saturate(t[11] * r[120])
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = c.t[11] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(127, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the ROM's own word: acc = k * r[125]
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			fetch(125);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: acc = k * r[120] + acc
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			fetch(120);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: r[78] = saturate(16 * (k * r[127] + acc))
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			fetch(127);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// ins::DIST_OUTPUT_C[1], b1: acc = k * r[78]
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_OUTPUT_C[2], -a1: acc = k * m[46] + acc
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[46];
			m2 = c.src; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DIST_OUTPUT_C[0], b0: m[46] = clip(2 * (k * r[78] + acc), +-128), the right output filter's state
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			fetch(78);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(46, yss236_core::clip(y, yss236_core::DSAT));
			break;

		// ---- 0x1ed to 0x1ff, the gain curve's breakpoints, its weights and the dry signals ----
		// the ROM's own word: r[1] = max(0, 128 * k + r[123]), the curve's first breakpoint
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(1, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[2] = max(0, 128 * k + r[123])
		case 0x1ee:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(2, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[3] = max(0, 128 * k + r[123])
		case 0x1ef:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(3, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[4] = max(0, 128 * k + r[123])
		case 0x1f0:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(4, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[5] = max(0, 128 * k + r[123])
		case 0x1f1:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(5, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[6] = max(0, 128 * k + r[123])
		case 0x1f2:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(6, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[124] = max(0, 128 * k + r[123])
		case 0x1f3:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(124, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// the ROM's own word: r[125] = max(0, 128 * k + r[123])
		case 0x1f4:
			k = yss236_core::s16(c.coef[st]);
			fetch(123);
			c.psrc = c.src; c.src = m[0];
			a = xv;
			v = 128.0 * k + a;
			c.queue_r(125, yss236_core::mode(v, 2));
			y = c.acc = yss236_core::accmode(v, 2);
			break;
		// ins::DYN_RATIO[0]: acc = 2 * k * r[1]
		case 0x1f5:
			k = yss236_core::s16(c.coef[st]);
			fetch(1);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[2]: acc = 2 * (k * r[3] + acc)
		case 0x1f6:
			k = yss236_core::s16(c.coef[st]);
			fetch(3);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[3]: acc = 2 * (k * r[4] + acc)
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			fetch(4);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[1]: acc = 2 * (k * r[2] + acc)
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			fetch(2);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[4]: acc = 2 * (k * r[5] + acc)
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			fetch(5);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[5]: acc = 4 * (k * r[6] + acc)
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			fetch(6);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[6]: acc = k * r[124] + acc
		case 0x1fb:
			k = yss236_core::s16(c.coef[st]);
			fetch(124);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// ins::DYN_RATIO[7]: acc = k * r[125] + acc
		case 0x1fc:
			k = yss236_core::s16(c.coef[st]);
			fetch(125);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the ROM's own word: m[42] = clip(128 * k + acc, +-128), the curve's output, biased and
		// clamped at zero
		case 0x1fd:
			k = yss236_core::s16(c.coef[st]);
			fetch(0);
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * k + a;
			y = c.acc = yss236_core::accmode(v, 2);
			c.queue_d(42, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::DIST_WET[0]: m[47] = clip(k * r[7], +-128), the left dry signal
		case 0x1fe:
			k = yss236_core::s16(c.coef[st]);
			fetch(7);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(47, yss236_core::clip(y, yss236_core::DSAT));
			break;
		// ins::DIST_WET[2]: m[48] = clip(k * r[8], +-128), the right dry signal
		case 0x1ff:
			k = yss236_core::s16(c.coef[st]);
			fetch(8);
			c.psrc = c.src; c.src = m[0];
			m2 = xv; prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(48, yss236_core::clip(y, yss236_core::DSAT));
			break;
}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m05(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
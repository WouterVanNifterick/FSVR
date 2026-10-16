// fs1r/native/ins_m12.cpp - VOP3-2, the insertion window's program for Wah+DS+Dly and Wah+OD+Dly:
// its 96 steps.
//
// rom::INSERTION_PROGRAMS[12], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 12, lsb 1 and 2: ins-18 Wah+DS+Dly and ins-19
// Wah+OD+Dly, both on EffectHost::wahDistortionDelay. Steps 0x1a0 to 0x1ff are the whole program.
// None of the 96 is a control step, a gate or a cskip, so the only thing the loop below carries out of
// step() is the m_skip guard a gate in the base program leaves behind.
//
// It is a wah, a distortion and a delay, in that order. What the 96 steps compute:
//
//   0x1a0-0x1ad  the input EQ. Each channel copies its signal into r[67] or r[70] and runs the low
//                shelf ins::LPF_WAH into r[68] or r[71] and the high shelf ins::HPF_WAH into r[69] or
//                r[72]. shelves() designs each shelf once and copies its three words to the other
//                channel's steps, so both channels run the same pair of sections.
//   0x1ae-0x1af  ins::WAH_MODE sums m[43] and m[44] into r[79], the signal the level follower uses.
//                Those two cells are the wah's own output, written at 0x1f9 and 0x1fb.
//   0x1b0-0x1b5  the level follower: three steps take the difference of r[79] and leave it in r[80],
//                three more smooth r[80] into r[81]. INFERRED: a difference of the signal is not an
//                envelope, so whether this is meant to read as a level or as an edge detector is not
//                settled by the arithmetic.
//   0x1b6-0x1bb  ins::WAH_MODE2 sums the two high shelves into r[73], the signal that drives the
//                resonator. ins::WAH_DRIVE[0] carries r[81] into r[119]. 0x1b9 and 0x1ba are one word
//                and its opposite, so they clear the running value and leave r[74] as a state with no
//                input of its own, ins::WAH_HPF_2[0].
//   0x1bc-0x1c4  ins::WAH_DRIVE[1] and [2] scale r[119] twice more. m[56] is the resonator's feedback:
//                ins::WAH_SENSE[0], the word wahSense() builds from the resonance, times the
//                resonator's state less the signal driving it. m[58] and m[59] are the same sum with
//                the resonance word and with the word at 0x1c4 in its place, and both reach the delay
//                line's input at 0x1dc.
//   0x1c6-0x1d8  the delay line. ins::WD_SL names its five slots: 0x77 is written at 0x1dc, and 0x71
//                to 0x74 are read one word apart at 0x1c7, 0x1cb, 0x1cf and 0x1d3, each taken up by a
//                cell five steps later, which is as far as step()'s capture reaches. The four taps are
//                summed into m[58] and m[59] rather than used on their own.
//   0x1cd-0x1d1  ins::WAH_BAND, the type's fixed edge: a gain word into r[121] and the geometric
//                series' one word into r[122], r[123] and two steps that are not a chain.
//   0x1d2-0x1de  the resonator's gain. r[124] is the resonator's drive and hands t[9], a 1.15 word,
//                to the t file; 0x1d9 scales r[124] by t[9] and 0x1de builds the next t[9] from the
//                running value. The gain is therefore the loop's own signal, and the loop saturates on
//                its own: that is what sweeps the wah's resonance with the level.
//   0x1e6-0x1ea  the resonator itself, r[78] = r[78] + t[9] * m[56], and ins::WAH_LPF2's four other
//                words, a section that ends in r[120] and r[121].
//   0x1eb-0x1f2  m[42], the integrator 0x1bd differences, and ins::WAH_PEAK, the distortion's mid peak.
//   0x1f3-0x1ff  the follower m[47], the wah's output as m[43] and m[44], and ins::WD_LEVEL into
//                r[17] and r[18], the two outputs.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals
// in; r[17] and r[18] the two outputs; r[67] to r[72] the input EQ, a copy, a low shelf and a high
// shelf per channel; r[73] the resonator's input; r[74] the state 0x1bb leaves; r[75] and r[76] the
// delay line's head; r[77] the accumulator the third tap feeds; r[79] the two channels summed, r[80]
// and r[81] the level follower; r[82] to r[84] ins::WAH_PEAK; r[85] the two-sample average of the
// first tap; r[119] the signal that leaves dry; r[120] and r[121] ins::WAH_LPF2's output and the left
// dry signal; r[122] and r[123] ins::WAH_BAND's series; r[124] the resonator's drive. Of the cells,
// m[42] is the resonator's integrator, m[43] and m[44] the wah's output, m[45] to m[47] the follower,
// m[48] the two-sample tap, m[56] and m[57] the feedback and the absolute value, m[58] and m[59] the
// two sums the delay line is fed, m[60] to m[63] the four taps. m[3], m[4] and m[7] are written here
// from the serial inputs of pages 13 and 14 and read nowhere in this window.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose
// is folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in the order yss236_core::step() runs its own. Which table names a step is said on
// the step; two of them name the same step, ins::WAH_BAND[5] and ins::WD_FB[0] both being 0x1dc, and
// at 0x1e6 the word ins::WAH_LPF2 lists is not the one the step multiplies by. The old model in
// vop3_effects.h runs the two types as a floating point wah, a tanh drive and a delay with feedback
// and mix; it has none of these words, so it is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m12: identical, 2 types, 19200 native passes.
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
		// ---- 0x1a0 to 0x1ad: the input EQ, the two shelves each channel runs in series ----
		//
		// r[7] and r[8] are the base program's left and right signals. Each channel copies its own
		// into r[67] or r[70], runs the low shelf ins::LPF_WAH into r[68] or r[71] and then the high
		// shelf ins::HPF_WAH into r[69] or r[72]. shelves() designs the low shelf into the first group
		// and copies its three words to the second channel's, and the same for the high shelf, so both
		// channels run the same pair: the EQ's two corners, one after the other.
		//
		// The three words of each shelf arrive in the order designFilter() wrote them, (b0, b1, -a1),
		// while the program runs them in another: the section's own output first, the input as it stood
		// a pass ago second, and b0 last, where the input copy has just landed. The low shelf's route
		// is x2 and the high shelf's x4, and Shelves::highScale is 2, which is the scale the high
		// shelf's words were designed at and the route puts back.
		case 0x1a0:
			// k = 0x4000, half, route x2: r[67] = r[7], the left channel's copy of the input
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
			// ins::LPF_WAH[1]: acc = LPF_WAH[1] * r[68], the shelf's own output
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
			// ins::LPF_WAH[2]: acc = LPF_WAH[2] * r[67] + acc, the input copy as it stood a pass ago
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
			// ins::LPF_WAH[0]: m[3] = the left serial input, and r[68] = 2 * (LPF_WAH[0] * r[67] + acc)
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
			// ins::HPF_WAH[1]: m[7] = the right serial input, and acc = HPF_WAH[1] * r[69]
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
			// ins::HPF_WAH[2]: acc = HPF_WAH[2] * r[68] + acc, r[68] as it stood a pass ago
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
			// ins::HPF_WAH[0]: r[69] = 4 * (HPF_WAH[0] * r[68] + acc), the high shelf's output
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
			// k = 0x4000, half, route x2: r[70] = r[8], the right channel's copy of the input
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
			// ins::LPF_WAH_C[1]: acc = LPF_WAH_C[1] * r[71]
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
			// ins::LPF_WAH_C[2]: acc = LPF_WAH_C[2] * r[70] + acc
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
			// ins::LPF_WAH_C[0]: r[71] = 2 * (LPF_WAH_C[0] * r[70] + acc)
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
			// ins::HPF_WAH_C[1]: acc = HPF_WAH_C[1] * r[72]
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
			// ins::HPF_WAH_C[2]: acc = HPF_WAH_C[2] * r[71] + acc
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
			// ins::HPF_WAH_C[0]: r[72] = 4 * (HPF_WAH_C[0] * r[71] + acc), the right channel's high shelf
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
		// ---- 0x1ae to 0x1af: ins::WAH_MODE, the two channels as one signal ----
		//
		// wahDistortionWet() puts SQRT_HALF in both words, so r[79] is the sum of m[43] and m[44] with
		// that factor, the same two signals again and the pair the level follower below works from.
		case 0x1ae:
			// ins::WAH_MODE[0], SQRT_HALF: acc = WAH_MODE[0] * m[43]
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
			// ins::WAH_MODE[1]: r[79] = WAH_MODE[1] * m[44] + acc, the two channels as one signal
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
		// ---- 0x1b0 to 0x1b5: the level follower ----
		//
		// Two blocks of the same three-step shape. The first takes the difference r[79] - r[79] a pass
		// ago, weights it by half a percent under and the state's own word over, and puts the result in
		// r[80]; the second smooths r[80] into r[81], whose word is ins::WAH_LPF_2's. The pair is what
		// the program has instead of an envelope generator: it follows the difference of the signal and
		// then smooths it, which is why the wah's filter is tuned from its own level.
		case 0x1b0:
			// acc = k * r[80], the follower's own state
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
			// acc = k * r[79] + acc, r[79] as it stood a pass ago
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
			// r[80] = 2 * (k * r[79] + acc): the state under the word times the difference, which is how
			// the program takes a level out of a signal
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
			// ins::WAH_LPF_2[1]: acc = WAH_LPF_2[1] * r[81], the smoother's own state
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
			// ins::WAH_LPF_2[2]: acc = WAH_LPF_2[2] * r[80] + acc, r[80] as it stood a pass ago
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
			// ins::WAH_LPF_2[0]: r[81] = 2 * (WAH_LPF_2[0] * r[80] + acc), the level the wah is tuned from
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
		// ---- 0x1b6 to 0x1bb: the mode pair, and the level into the drive ----
		//
		// ins::WAH_MODE2 sums the two high shelves' outputs into r[73], the mono signal that drives the
		// resonator below. 0x1b8 is ins::WAH_DRIVE[0] and carries r[81] into r[119]. 0x1b9 and 0x1ba are
		// one word and its opposite on the same register, so between them they clear the running value
		// and leave nothing for 0x1bb to add, which writes r[74] as a state with no input of its own:
		// ins::WAH_HPF_2[0] times the value already there.
		case 0x1b6:
			// ins::WAH_MODE2[0], the host's 0x5a81: acc = WAH_MODE2[0] * r[69]
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
			// ins::WAH_MODE2[1]: r[73] = WAH_MODE2[1] * r[72] + acc, the two high shelves as one signal
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
			// ins::WAH_DRIVE[0]: r[119] = 16 * (WAH_DRIVE[0] * r[81]), the level into the drive
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
			// acc = k * r[73]
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
			// acc = k * r[73] + acc: the word here is the one at 0x1b9 with the other sign, so between
			// them the two steps leave the running value at zero
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
			// ins::WAH_HPF_2[0]: r[74] = 2 * (WAH_HPF_2[0] * r[74] + acc), a state with no input
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
		// ---- 0x1bc to 0x1c4: the drive chain, and the resonator's first half ----
		//
		// ins::WAH_DRIVE[1] and ins::WAH_DRIVE[2], the two steps storeDrive() writes with the same word,
		// scale r[119] twice more, and what the wah's filter and its level follower put in it leaves the
		// window with the dry signal. m[56] is the resonator's feedback: ins::WAH_SENSE[0], the word
		// wahSense() builds from the resonance, times the resonator's state less the signal driving it.
		// m[57] is the absolute value of that state scaled by ins::WAH_ATTACK.
		case 0x1bc:
			// ins::WAH_DRIVE[1]: r[119] = 16 * (WAH_DRIVE[1] * r[119])
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
			// acc = k * m[42] - r[73]: the resonator's state less the signal driving it
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
			// ins::WAH_SENSE[0], the word wahSense() builds from the resonance: m[56] = that * acc
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
			// ins::WAH_ATTACK[0]: m[57] = clip(16 * (WAH_ATTACK[0] * r[74])), and the clamp takes |y|
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
			// ins::WAH_DRIVE[2]: r[119] = 16 * (WAH_DRIVE[2] * r[119])
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
		// ---- 0x1c1 to 0x1c5: the two sums the delay line is fed, and r[120] ----
		//
		// m[58] and m[59] are the same pair written twice: a word on m[43], which is ins::WAH_SENSE[1]
		// for m[58] and the word at 0x1c4 for m[59], added to m[47] scaled by the word at 0x1c1. Both
		// cells reach the delay line's input at 0x1dc, half each.
		case 0x1c1:
			// acc = k * m[47], the level the follower below keeps
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
			// ins::WAH_SENSE[1]: m[58] = WAH_SENSE[1] * m[43] + acc
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
			// ins::WAH_MIRROR[0]: acc = k * m[47]
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
			// m[59] = k * m[43] + acc: the sum m[58] has, with the word at this step in place of the
			// resonance word. Nothing writes that word; it is the program's own
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
			// m[4] = the serial input of page 14, and r[120] = 16 * (k * r[119])
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
		// ---- 0x1c6 to 0x1cc: the delay line's head and its first tap ----
		//
		// The window's line is the five slots ins::WD_SL names: 0x77 is written, at 0x1dc, and 0x71 to
		// 0x74 are read, one word apart, at 0x1c7, 0x1cb, 0x1cf and 0x1d3. A read is taken up by a cell
		// five steps later, which is how far step()'s capture reaches, so m[60] takes 0x1c7's read,
		// m[61] takes 0x1cf's, m[62] takes 0x1d3's and m[63] takes 0x1cb's. r[75] and r[76] hold the
		// line's head, written one sample apart around the read that refills it.
		case 0x1c6:
			// ins::WAH_MIRROR[1]: acc = WAH_MIRROR[1] * r[75] + m[57]
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
			// the first tap, ins::WD_SL[1] = slot 0x71: r[75] = k * m[57] + acc
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
			// ins::WAH_MIRROR[2]: acc = 0, the running value, and src takes m[76]
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
			// acc = k * r[76] + src, src being the m[76] 0x1c8 latched
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
			// acc = k * m[57] - acc
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
			// the second tap, ins::WD_SL[2] = slot 0x72: acc = k * r[75] - acc
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
			// m[60] takes the read 0x1c7 made, and r[76] = k * m[57] + acc
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
		// ---- 0x1cd to 0x1d1: the fixed edge, ins::WAH_BAND ----
		//
		// storeEdge() writes this table's steps 1 to 5 and no others: a gain word into 0x1cd and the
		// geometric series' one word into 0x1d1, 0x1d4, 0x1d8 and 0x1dc. r[121] and r[122] are the
		// first two of the series, read back and summed at 0x1e1 to 0x1e5.
		case 0x1cd:
			// ins::WAH_BAND[1], the edge's gain word: r[121] = 16 * (WAH_BAND[1] * r[120])
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
			// ins::WAH_CUTOFF[0]: acc = k * r[76], the distortion's lowpass cutoff on the running value
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
			// the third tap, ins::WD_SL[3] = slot 0x73: acc = k * r[76] + acc
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
			// m[61] takes the read 0x1cf made, and r[77] = k * r[77] + acc
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
			// ins::WAH_BAND[2]: r[122] = 16 * (WAH_BAND[2] * r[121])
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
		// ---- 0x1d2 to 0x1d5: the resonator's gain, and the last two taps ----
		//
		// 0x1d2 is a class-1 load whose word goes straight into t[9], and the steps after it start
		// from nothing. m[61] takes the tap 0x1cf read. r[124] is the resonator's drive, and this step
		// also hands the result to t[9], the gain the resonator's feedback at 0x1d9, 0x1dd, 0x1e6 and
		// 0x1eb multiplies by.
		case 0x1d2:
			// a class-1 load: acc = 128 * k and t[9] takes it, so this step's whole work is to put a
			// word in the t file, and the word is the program's own
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1d3:
			// the fourth tap, ins::WD_SL[4] = slot 0x74: the word here is zero, so acc keeps what it had
			// and the step's work is the read and the m operand latch, m[12] among them
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
			// m[62] takes the read 0x1d3 made, r[124] = k * r[77] + acc, and t[9] takes the result
			// as a 1.15 word
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
			// r[123] = 16 * (k * r[122])
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
		// ---- 0x1d6 to 0x1dc: the delay line's input ----
		//
		// 0x1d6 and 0x1d7 average the first tap over two samples, m[48] being the step before's m
		// operand, and 0x1d8 puts that through r[85]. m[63] takes the tap 0x1d3 read. 0x1d9 scales the
		// resonator's drive by t[9] and writes t[9] again from the result. 0x1dc is the line's write:
		// its word is the delay's feedback, ins::WD_FB, which this program also lists under
		// ins::WAH_BAND, and the RAM takes the clipped result.
		case 0x1d6:
			// acc = k * m[60], the first tap as it stands
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
			// m[48] takes the step before's m operand, and acc = k * m[48] + acc, the first tap over
			// two samples
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
			// m[63] takes the read 0x1cb made, and r[85] = 2 * (k * r[85] + acc)
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
			// r[124] = t[9] * r[124]: the resonator's drive under the gain t[9] took at 0x1d4, and
			// t[9] takes the result
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
			// ins::WAH_WET[3]: acc = WAH_WET[3] * m[58]
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
			// acc = 2 * (k * m[59] + acc), the two cells half each
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
			// the line's write, ins::WD_SL[0] = slot 0x77: the RAM takes the clipped result, and the word
			// at this step is the delay's feedback, ins::WD_FB, which ins::WAH_BAND[5] also lists
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
		// ---- 0x1dd to 0x1de: t[9], the resonator's loop gain ----
		//
		// The gain is the running value times t[9] from 0x1d4, put through the class-1 step's word, so
		// the loop's gain grows with what is in it and the resonator saturates on its own. The word at
		// 0x1dd is ins::WAH_BAND[6], which storeEdge() sets in the host's coef[] but does not write
		// to the chip: it writes that table's steps 1 to 5 and no others.
		case 0x1dd:
			// acc = t[9] * r[124], the running value the class-1 step below builds the gain from
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
			// a class-1 step: acc = 128 * k + acc, and t[9] takes it
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 1);
			c.t_write(c.ops[st], st, c.acc);
			break;
		// ---- 0x1df to 0x1e5: the edge's sum, and the dry signal ----
		//
		// r[123] is the fourth of ins::WAH_BAND's series steps. The four states r[120] to r[123] are
		// then added with a fifth of each, the word storeEdge() writes into steps 7 to 11, into
		// r[119] under ins::WAH_DRIVE[3], which is the signal that leaves the window dry.
		case 0x1df:
			// r[119] = 16 * (k * r[123])
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
			// acc = 0, the running value, with the word at this step
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
			// acc = k * r[120] + acc
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
			// acc = k * r[121] + acc
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
			// acc = k * r[122] + acc
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
			// acc = k * r[123] + acc
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
			// ins::WAH_DRIVE[3]: r[119] = k * r[119] + acc, the edge's sum, the dry signal
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
		// ---- 0x1e6 to 0x1ea: the resonator, and the distortion's lowpass ----
		//
		// r[78] is the resonator: its own value plus t[9] times m[56], the feedback 0x1be took, so
		// r[78] = r[78] * (1 + t[9] * WAH_SENSE[0]). This step is ins::WAH_LPF2[2], but its multiplier
		// input is t[9] rather than the word at the step, so the word is not read here. 0x1e7 to 0x1ea
		// are ins::WAH_LPF2's other four words, and the section they form ends in r[120] and r[121],
		// which are the delay line's input and the left dry signal.
		case 0x1e6:
			// ins::WAH_LPF2[2]: r[78] = t[9] * m[56] + r[78], the resonator's feedback. The multiplier input
			// is t[9] and not the word at this step, so that word is not read here
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
			// ins::WAH_LPF2[1]: acc = WAH_LPF2[1] * m[63]
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
			// ins::WAH_LPF2[4]: r[120] = k * m[62] + acc
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
			// ins::WAH_LPF2[3]: acc = WAH_LPF2[3] * m[61]
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
			// ins::WAH_LPF2[0]: r[121] = k * m[62] + acc
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
		// ---- 0x1eb to 0x1ec: the resonator's state, and the level into the peak ----
		//
		// m[42] takes t[9] times r[78] added to what it held, the integrator 0x1bd differences.
		case 0x1eb:
			// m[42] = t[9] * r[78] + src, src being the m[42] this step latched: the integrator 0x1bd
			// differences
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
			// r[82] = k * r[119], the peak's input, the dry signal
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
		// ---- 0x1ed to 0x1f2: ins::WAH_PEAK, the distortion's mid peak ----
		//
		// designFilter() writes its five words here in the order (b0, b1, b2, -a1, -a2), and the
		// program runs the section through r[82], r[83] and r[84]. 0x1f2 carries rcopy, so r[84] is
		// the peak's x[-1], and the section is in direct form I.
		case 0x1ed:
			// ins::WAH_PEAK[1]: acc = WAH_PEAK[1] * r[83]
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
			// ins::WAH_PEAK[3]: acc = WAH_PEAK[3] * r[82] + acc
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
			// ins::WAH_PEAK[2]: r[83] = 2 * (WAH_PEAK[2] * r[82] + acc)
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
			// ins::WAH_PEAK[4]: acc = WAH_PEAK[4] * r[84]
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
			// acc = k * r[83] + acc
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
			// ins::WAH_PEAK[0]: r[84] = WAH_PEAK[0] * r[83] + acc, and rcopy makes it the x[-1] of the
			// step before's operand, r[83]
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
		// ---- 0x1f3 to 0x1f7: the follower the two sums above take m[47] from ----
		//
		// m[45] is a saturating integrator of the running value, m[46] is m[45] a pass ago, and m[47]
		// smooths m[45] against m[46]. It is the level the release word and the resonance word at
		// 0x1c2 and 0x1c4 are added to.
		case 0x1f3:
			// acc = k * m[46] + acc
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
			// m[45] = clip(2 * (k * m[45] + acc)), a saturating integrator of the running value
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
			// m[46] takes the step before's m operand, and acc = k * m[47] + off
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
			// acc = k * m[45] + acc
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
			// m[47] = clip(4 * (k * m[45] + acc)), the level the sums at 0x1c2 and 0x1c4 take
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
		// ---- 0x1f8 to 0x1fb: the wah's own output, as two cells ----
		//
		// m[43] and m[44] are both r[78] a pass on, and they are the two signals the program's first
		// steps summed into r[79]: the level follower and the resonator's sums both work from them.
		case 0x1f8:
			// acc = 0, the running value
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
			// m[43] = 4 * (k * r[78] + acc), the wah's output a pass on
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
			// acc = 0, the running value
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
			// m[44] = 4 * (k * r[78] + acc), the same signal as m[43]
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
		// ---- 0x1fc to 0x1ff: the outputs, ins::WD_LEVEL ----
		//
		// The dry signal r[119] through 0x1df and 0x1cd reaches r[120] and r[121], and the wet path
		// reaches m[58] and m[59]. ins::WD_LEVEL writes the mix word into 0x1fd and 0x1ff, and the
		// word at 0x1fe is the program's own, a quarter of m[59].
		case 0x1fc:
			// ins::WAH_WET[7]: acc = WAH_WET[7] * m[58]
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
			// ins::WD_LEVEL[0], the mix word: r[17] = 4 * (WD_LEVEL[0] * r[120] + acc), the left output
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
			// acc = k * m[59], a quarter of the wet cell, the program's own word
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
			// ins::WD_LEVEL[2]: r[18] = 4 * (WD_LEVEL[2] * r[121] + acc), the right output
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

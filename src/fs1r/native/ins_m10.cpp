// fs1r/native/ins_m10.cpp - VOP3-2, the insertion window's program for Lo-Fi: its 96 steps.
//
// rom::INSERTION_PROGRAMS[10], the program the insertion window holds under the one type
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 10: ins-20 Lo-Fi on EffectHost::loFi. Steps 0x1dd to
// 0x1ff are zero class-0 steps, so the program proper is the 61 steps 0x1a0 to 0x1dc and the last 35 do
// nothing but advance the step counter and move the x latch. Nothing in it touches the delay RAM, no step
// reads or writes a t register, and the three cells the base program leaves behind (m[3] and m[7] from
// the serial inputs of page 13, m[4] from page 14) are written here and read nowhere in this window.
//
// It is a bit crusher: a sample and hold, a quantizer with two error accumulators around it, then a tone
// chain over the result. The 96 steps in the order they run it:
//
//   0x1a0-0x1a3  the sample and hold's counter. r[81] climbs by one word, 1/256 of full scale, a pass and
//                saturates rather than wraps, and 0x1a2 puts ins::LOFI_RATE into the running value for
//                0x1a3 to compare the counter against. That compare is the flag the block at 0x1b0 is
//                gated on, so ins::LOFI_RATE sets how often the signal is latched. 0x1a1 loads
//                ins::LOFI_BITS into m[56], the quantizer's step, and that word is the word length: the
//                step is the word length's own value in units of 2^-13, so a short word length crushes to
//                few levels and UNITY's 32767 is a step of 4.0, a sixth of full scale.
//
//   0x1a4  r[119] = 2 * r[78], the tone chain's output from the pass before. The mix at 0x1b6 and 0x1b9
//          read it, so the wet signal reaches the outputs one sample behind the pass that made it.
//
//   0x1a5-0x1ae  the input chain. r[67] is ins::LOFI_GAIN times the sum of r[7] and r[8], the two signals
//          the base program leaves the window, and the gain is the bit assign's: (32767 >> assign) >> 1.
//          0x1a7 to 0x1a9 are ins::LOFI_HIGH_SHELF, an order-1 section over r[67] into r[68], and 0x1aa to
//          0x1ae ins::LOFI_LPF2, the order-2 section into r[70]. Both are written by designFilter(), which
//          emits b0, b1, b2, -a1, -a2 in that order for an order-2 section and b0, b1, -a1 for an order-1
//          one, and each section's closing step carries the route that restores the scale the designer's
//          (16 >> scale) took out. The two z^-1 words are the steps that carry rcopy: 0x1ab latches r[70]
//          into r[71] for the -a2 term and 0x1ac latches r[68] into r[69] for the b2 term, and each is read
//          a pass later than it is written, so the section is in direct form I.
//
//   0x1b0-0x1b3  the latch: m[42] = r[70] and r[81] = 0, run only when the compare at 0x1a3 fired.
//
//   0x1b4-0x1b9  the quantizer's input and the output mix. 0x1b4 takes m[42] - r[79], the held signal
//          less the second error accumulator. 0x1b5 and 0x1b6 are ins::LOFI_WET[0] and ins::LOFI_WET2[0],
//          the dry and wet gains, into r[17], and 0x1b8 and 0x1b9 the same two words for r[18]. The dry
//          signal is the input and the wet is r[119], so the two channels share one crushed signal and
//          differ only in their dry side.
//
//   0x1b7-0x1c3  the quantizer, which is one subtraction and two conditional skips. 0x1b7 subtracts m[56]
//          from r[121] and its sign is the flag; if it is positive the value was above the step, so 0x1ba
//          lets 0x1bb write +step and 0x1bc jumps out to 0x1c4. If it was not, 0x1ba skips to 0x1bd, which
//          puts the step back and compares against zero: negative goes to -step through 0x1c0 and 0x1c1,
//          and anything from zero to the step is left as it was. So m[56] is a dead zone and the bit
//          crush is the clamping to +-step around it.
//
//   0x1c4-0x1cb  the error accumulators. r[80] takes the quantized value, ins::LOFI_LEVEL (a word of the
//          output gain law) scales it into r[121], and r[79] takes that: two integrators around the
//          quantizer, which is what makes the crush noise-shaped rather than flat, and 0x1b4 is where r[79]
//          comes back into the loop.
//
//   0x1ce  r[72] = ins::LOFI_OUT[0] * r[121] * 16, the bit assign's output level: 2048 up to UNITY, or the
//          wet gain when the assign is above six.
//
//   0x1cf-0x1dc  the tone chain over the quantized signal: ins::LOFI_FILTER, an order-1 section over r[72]
//          into r[73], ins::LOFI_FILTER_2, the order-2 section into r[75] (its z^-1 word is 0x1d4, which
//          carries r[73] one pass older into r[74], and r[76] is its second output delay), ins::LOFI_FILTER_3,
//          an order-1 section into r[77], and ins::LOFI_HIGH_SHELF_2, an order-1 section into r[78], which
//          is the wet signal 0x1a4 reads. fx_tone.cpp's loFi() takes the three filters' shapes, frequencies
//          and gains from rom::LOFI_FILTERS, one row per filter type, and the emphasis parameter chooses
//          the last section's words: a high shelf with it on and a plain gain with it off, which is what the
//          two zero words at 0x1da and 0x1db are in that case.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals in;
// r[17] and r[18] the two outputs; r[67] the input sum; r[68], r[69], r[70] and r[71] the input chain's
// filter, its x[-1] and y[-2] delays, its output and its y[-1] delay; r[72] to r[78] the tone chain, one
// pair per section; r[79] and r[80] the two error accumulators; r[81] the sample rate counter; r[119] the
// wet signal; r[121] the scratch every stage hands to the next; m[42] the held signal; m[56] the
// quantizer's step.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose is
// folded in here: the program words are fixed per row and each step's fields are folded into the arithmetic
// below in the order yss236_core::step() runs its own. The old model in vop3_effects.h runs Lo-Fi as a
// sample-and-hold decimation, a quantizer and a two-pole lowpass in floating point; it has none of these
// words, so it is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m10: identical, 1 type, 9600 native passes.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. There is no gate in this program, but the three cskip steps below do
// use the same guard, and the m_skip they set is what selects the quantizer's branch.
void block_0x1a0(yss236_core& c)
{
	double* const r = c.r;                 // the 128 registers
	double* const m = c.m;                 // the 64 cells, which carry the values between the steps
	double xv, k, m2, prod, a, y, v;       // the step's operands and its result

	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{		case 0x1a0:  // k = 0x0001, the rate counter: r[81] += 1/256, the ROM's own word, saturating
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(81, yss236_core::mode(v, 1));
			c.acc = yss236_core::accmode(v, 1);
			break;
		case 0x1a1:  // k = 0x0010, ins::LOFI_BITS[0] into m[56]: the quantizer's step
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			c.queue_d(56, yss236_core::clip(c.acc, yss236_core::DSAT));
			break;
		case 0x1a2:  // k = 0x0002, ins::LOFI_RATE[0] into the running value: the threshold the next step compares against
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.acc = yss236_core::accmode(v, 0);
			break;
		// the compare: acc = r[81] - acc, so the flag is positive once the counter passes the rate
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
		case 0x1a4:  // k = 0x2000, r[119] = 2 * r[78], the tone chain's output from the pass before: the wet signal
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
		case 0x1a5:  // k = 0x1fff, acc = ins::LOFI_GAIN[0] * r[7], the left signal at the bit assign's gain
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
		case 0x1a6:  // k = 0x1fff, r[67] = ins::LOFI_GAIN2[0] * r[8] + acc: the two signals summed into the input chain
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
		// ---- 0x1a7 to 0x1a9, ins::LOFI_HIGH_SHELF: the order-1 section over r[67] into r[68] ----
		// m[7] takes the right serial input of page 13 on the way, which this program reads nowhere
		case 0x1a7:  // k = 0x0000, acc = -a1 * r[68], the section's own delay
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
		// the b1 term on r[67], which the three-step register latency leaves one pass old here
		case 0x1a8:  // k = 0x0000, acc = b1 * r[67] + acc
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
		// r[68] = (b0 * r[67] + acc) * 4, the shelf's output; the route restores the designer's scale
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
		// ---- 0x1aa to 0x1ae, ins::LOFI_LPF2: the order-2 section over r[68] into r[70] ----
		case 0x1aa:  // k = 0x8b38, acc = -a1 * r[70], the section's own delay
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
		// the two z^-1 words are the steps that carry rcopy: 0x1ab latches r[70] into r[71] for the -a2
		// term and 0x1ac latches r[68] into r[69] for the b2 term, each one pass behind where it is read
		case 0x1ab:  // k = 0x775d, r[71] = the previous step's x; acc = b1 * r[68] + acc
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
		case 0x1ac:  // k = 0x3baf, r[69] = the previous step's x; acc = b0 * r[68] + acc
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
		case 0x1ad:  // k = 0xc60e, acc = -a2 * r[71], a pass older than the r[71] this step writes
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
		case 0x1ae:  // k = 0x3bae, r[70] = (b2 * r[69] + acc) * 2, the biquad's output
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
		case 0x1af:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		// ---- 0x1b0 to 0x1b3, the sample and hold, run only when the compare at 0x1a3 fired ----
		case 0x1b0:  // k = 0x0000, cskip 6: 0x1b1 to 0x1b3 run only while the flag is positive
			if (c.m_flag <= 0) c.m_skip = 0x1b4;
			break;
		case 0x1b1:  // k = 0x0000, m[42] = r[70]: the filtered signal held for the quantizer
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
		case 0x1b2:  // k = 0x0000, r[81] = 0, the counter starts again
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[0];
			a = 0.0;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(81, yss236_core::mode(v, 0));
			c.acc = yss236_core::accmode(v, 0);
			break;
		case 0x1b3:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		// ---- 0x1b4 to 0x1b9, the quantizer's input and the output mix ----
		case 0x1b4:  // k = 0x8000, r[121] = -r[79] + m[42]: the held signal less the second error accumulator
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
		// the left mix: the ROM's own words ins::LOFI_WET[0] (dry) and ins::LOFI_WET2[0] (wet)
		case 0x1b5:  // k = 0x0000, acc = dry * r[7]
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
		case 0x1b6:  // k = 0x3fff, r[17] = (wet * r[119] + acc) * 4, the left output
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
		// ---- 0x1b7 to 0x1c3, the dead-zone quantizer ----
		// It is three compares and four conditional skips around one subtraction. r[121] - m[56] lands in
		// r[121] and its sign is the flag; above the step 0x1bb replaces the value with +step and 0x1bc
		// jumps out, below zero 0x1bd puts the value back and 0x1c0 lets 0x1c1 write -step, and between
		// the two the value is left as it was. So the step is the dead zone and the bit crush is the
		// clamping to +-step around it.
		case 0x1b7:  // k = 0x0000, acc = r[121] - m[56], the compare on the way down
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
		// the right mix, the same two words for the other channel
		case 0x1b8:  // k = 0x0000, acc = dry2 * r[8]
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
		case 0x1b9:  // k = 0x3fff, r[18] = (wet2 * r[119] + acc) * 4, the right output
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
		case 0x1ba:  // k = 0x0000, cskip 6: 0x1bb runs only while the flag is positive
			if (c.m_flag <= 0) c.m_skip = 0x1bd;
			break;
		case 0x1bb:  // k = 0x0000, r[121] = m[56]: the value above the step
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = c.src;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(121, yss236_core::mode(y, 0));
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// cskip 1: an unconditional jump, so the negative branch below only runs when 0x1ba skipped
		case 0x1bc:  // k = 0x0000
			c.m_skip = 0x1c4;
			break;
		// ---- the negative branch, reached only when 0x1ba skipped 0x1bb and 0x1bc ----
		case 0x1bd:  // k = 0x0000, acc = r[121] + m[56], which puts the value back; the compare tests it against zero
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
		case 0x1be:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1bf:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c0:  // k = 0x0000, cskip 4: 0x1c1 to 0x1c3 run only while the flag is negative
			if (c.m_flag >= 0) c.m_skip = 0x1c4;
			break;
		case 0x1c1:  // k = 0x8000, r[121] = -m[56]: the value below zero
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
		case 0x1c2:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		// m[4] takes the left serial input of page 14 on the way, which this program reads nowhere
		case 0x1c3:  // k = 0x0000, nothing but the count
			c.queue_d(4, c.in[0][14]);
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		// ---- 0x1c4 to 0x1cb, the two error accumulators that shape the quantizer's error ----
		case 0x1c4:  // k = 0x0000, acc = r[121], the quantized value
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c5:  // k = 0x0000, r[80] = r[80] + acc, the first accumulator
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
		case 0x1c6:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c7:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1c8:  // k = 0x005a, r[121] = ins::LOFI_LEVEL[0] * r[80] * 16, the level law over the first accumulator
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
		case 0x1c9:  // k = 0x0000, acc = r[121]
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.hasx = true; c.x = xv;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = xv;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1ca:  // k = 0x0000, r[79] = r[79] + acc, the second accumulator, which 0x1b4 subtracts again
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
		case 0x1cb:  // k = 0x7fff, r[121] = r[121] * 16: the ROM's own word, the second of the two x16 steps
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
		case 0x1cc:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1cd:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		// ---- 0x1ce to 0x1dc, the tone chain over the quantized signal ----
		case 0x1ce:  // k = 0x1000, r[72] = ins::LOFI_OUT[0] * r[121] * 16, the bit assign's output level
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
		// ---- ins::LOFI_FILTER, an order-1 section over r[72] into r[73] ----
		case 0x1cf:  // k = 0x3f8c, acc = -a1 * r[73]
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
		case 0x1d0:  // k = 0xcd58, acc = b1 * r[72] + acc, a pass older than the r[72] 0x1d1 writes
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
		case 0x1d1:  // k = 0x32a8, r[73] = (b0 * r[72] + acc) * 2
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
		// ---- ins::LOFI_FILTER_2, the order-2 section over r[73] into r[75] ----
		case 0x1d2:  // k = 0x0451, acc = b2 * r[74]
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
		case 0x1d3:  // k = 0xdb7f, acc = b1 * r[73] + acc
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
		// the z^-1 word: r[74] holds r[73] one pass older, which is the b2 term's delay
		case 0x1d4:  // k = 0x2beb, r[74] = the previous step's x; acc = b0 * r[73] + acc
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
		case 0x1d5:  // k = 0x2481, acc = -a1 * r[75]
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
		case 0x1d6:  // k = 0xefc4, r[75] = (-a2 * r[76] + acc) * 4: r[76] is the section's second output delay
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
		// ---- ins::LOFI_FILTER_3, an order-1 section over r[75] into r[77] ----
		case 0x1d7:  // k = 0xe201, acc = b1 * r[75]
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
		// the z^-1 word, which r[76] then carries as this section's second output delay
		case 0x1d8:  // k = 0x1e3a, r[76] = the previous step's x; acc = b2 * r[77] + acc
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
		case 0x1d9:  // k = 0x203b, r[77] = (b0 * r[75] + acc) * 4
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
		// ---- ins::LOFI_HIGH_SHELF_2, an order-1 section over r[77] into r[78] ----
		// The emphasis parameter chooses this section's words: a high shelf with it on, and a plain gain
		// with it off, which is what the two zero words here are in that case.
		case 0x1da:  // k = 0x0000, acc = -a1 * r[78]
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
		case 0x1db:  // k = 0x0000, acc = b1 * r[77] + acc
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
		case 0x1dc:  // k = 0x2000, r[78] = (b0 * r[77] + acc) * 4, and 0x1a4 reads it as the wet signal next pass
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
		// ---- 0x1dd to 0x1ff, 35 zero class-0 steps: nothing but the count and the x latch ----
		case 0x1dd:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1de:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1df:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e0:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e1:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e2:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e3:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e4:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e5:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e6:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e7:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e8:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1e9:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ea:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1eb:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ec:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ed:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ee:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ef:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f0:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f1:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f2:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f3:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f4:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f5:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f6:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f7:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f8:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1f9:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fa:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fb:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fc:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fd:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1fe:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		case 0x1ff:  // k = 0x0000, nothing but the count
			c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m10(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

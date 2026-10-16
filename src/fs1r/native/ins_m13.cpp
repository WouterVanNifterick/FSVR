// fs1r/native/ins_m13.cpp - VOP3-2, the insertion window's program for Cmp+DS+Dly, Cmp+OD+Dly,
// Dist+Delay and Odrv+Delay: its 96 steps.
//
// rom::INSERTION_PROGRAMS[13], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 13: ins-26 Cmp+DS+Dly (lsb 1) and ins-27 Cmp+OD+Dly
// (lsb 2) on EffectHost::compressorDistortionDelay, ins-29 Dist+Delay (lsb 3) and ins-31 Odrv+Delay
// (lsb 4) on EffectHost::distortionDelay. Steps 0x1a0 to 0x1ff are the whole program. None of the 96 is
// a control step, a gate or a cskip, so the only thing the loop below carries out of step() is the
// m_skip guard a gate in the base program leaves behind.
//
// One signal, not two. r[7] and r[8] are the base program's left and right signals, and 0x1a1 and 0x1a2
// reduce them to r[68] under the program's own words; every step after that works on the one signal.
// What the 96 compute, in the order the steps run:
//
//   0x1a0-0x1a2  the stereo pair as one signal, and the drive state m[42] into r[119].
//   0x1a3-0x1ab  ins::DD_HPF, the input highpass, whose state alone is used; and the first three of the
//                delay line's four tap reads, slots 0x69, 0x6a and 0x6b.
//   0x1ac-0x1ae  ins::DD_ATTACK, a one-pole smoother of r[74] into r[76] with unity gain, and t[9].
//   0x1af-0x1bd  the fourth tap, slot 0x6c, ins::DD_BAND's first series words, the release pair, and
//                ins::DD_LPF into m[42].
//   0x1be-0x1c9  the release pair's other two words and ins::DD_BAND's sum of the four series states.
//   0x1ca-0x1cc  ins::DD_THRESHOLD, an empty step, and ins::DD_DRIVE's gain word.
//   0x1cd-0x1db  ins::DD_LPF2, ins::DD_PEAK and ins::DD_LOW_SHELF in series, of which only DD_LOW_SHELF's
//                state is read.
//   0x1dc-0x1de  ins::DD_LPF_2 over the first two taps: r[86], the delay line's feedback.
//   0x1df-0x1ed  the dry and wet mix, the four taps, the line's write at slot 0x7a, and the two outputs.
//   0x1ee-0x1ff  the level ladder and ins::DD_RATIO, which build r[74], the envelope 0x1ac smooths.
//
// Five shapes repeat, and they are worth naming because the program's meaning is mostly in them:
//
//   - Six filter sections, ins::DD_HPF, DD_LPF, DD_LPF2, DD_PEAK, DD_LOW_SHELF and DD_LPF_2, each a
//     host table of the designFilter() kind (b0, b1, b2, -a1, -a2). A section runs its words in the
//     order b2, b1, b0, -a2, -a1 rather than the order designFilter() wrote them, and its b0 step
//     carries rcopy, so the register it writes holds the input a sample ago and the b2 step reads that
//     one step further back. The two feedback terms both read the same state, because the rcopy at the
//     head of the next section puts r[70]'s own value into r[71]; the state is therefore a recursion on
//     (-a1 - a2) and not a two-state one. INFERRED: what the chip's second state register would have
//     been is not settled by this program, which never uses a second one.
//   - The delay line, ins::DD_SLOTS' five slots: 0x7a written at 0x1e9, and 0x69 to 0x6c read one word
//     apart at 0x1a4, 0x1a8, 0x1ac and 0x1b0, each read taken up by a cell five steps later (m[56],
//     m[57], m[58], m[59]), which is CAPMIN, the nearest step()'s capture reaches. Of the four taps
//     only the fourth, m[59], reaches an output; 0x1e4 and 0x1e6 multiply the third and the second by
//     zero words of the program's own, so r[124] is zero at 0x1ed. The first two reach the line's
//     feedback instead, through r[86].
//   - The level ladder, eight class-1 steps at 0x1ee to 0x1f5, each r[120] minus an offset of the
//     program's own (a quarter, a half, 1, 2, 4, 8, 16, 64) under a clamp that drops negatives, so each
//     is max(0, r[120] - offset). ins::DD_RATIO then weights the eight in the compiler's order, which
//     is not the order the steps run: 0, 2, 3, 1, 4, 5, 6, 7. That is a piecewise-linear curve over
//     eight bands, and 0x1fe adds it to full scale to make r[74].
//   - The two gains, ins::DD_LEVEL and ins::DD_MODE, are the two halves of that same curve: DD_LEVEL
//     scales onto the taps at the outputs, DD_MODE onto r[124] and r[125] in the drive. So the four
//     types differ mostly in which steps the host writes, and not in the program's shape.
//   - ins::DD_BAND's geometric series, from storeEdge(t.edge): five words into 0x1af, 0x1b2, 0x1b5,
//     0x1ba and 0x1c3, summed at 0x1c4 to 0x1c9 with a fifth of each.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals in;
// r[17] and r[18] the two outputs; r[68] the pair as one signal; r[69], r[70] and r[71] the DD_HPF's
// two taps and its state; r[72] and r[73] the release pair's two registers; r[74] the envelope and r[76]
// its smoothed form, with r[75] a copy of r[74]; r[77] the DD_LPF's input; r[78] the drive word's gain;
// r[80] and r[82] the DD_LPF2's and DD_PEAK's states; r[84] the DD_LOW_SHELF's state and the wet signal;
// r[86] the delay line's feedback. Of the cells, m[3] and m[7] are the serial inputs of page 13 and m[4]
// the left serial input of page 14, read nowhere here; m[42] the drive state; m[43] the second tap a pass
// on; m[56] to m[59] the four taps; m[60] the DD_MODE word on the DD_HPF's state.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose
// is folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in the order yss236_core::step() runs its own. Which table names a step is said on the
// step. Four tables name steps this program does not read in the way the name suggests: ins::DD_BAND's
// steps 6 to 11 (0x1c4 to 0x1c9) carry words storeEdge sets in the host's coef[] but never writes to
// the chip, so 0x1c5 to 0x1c9 read 6553 and 0x1c4 reads zero; and ins::DD_MODE[1] is written by
// distortionDelay alone. The old model in vop3_effects.h runs the four types as a floating point
// compressor, a tanh drive and two delay lines with independent taps; this program has one line, four
// taps and no envelope follower of its own, so that model is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check ins-m13: identical, 4 types, 38400 native passes.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's. There is no gate or cskip step in this program, so the
// guard is the only control the loop needs.
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

		// ---- 0x1a0 to 0x1a2: the stereo pair as one signal, and the drive state ----
		//
		// r[7] and r[8] are the base program's left and right signals. 0x1a1 scales the left one by a
		// quarter and 0x1a2 adds the right one under the program's own word, so r[68] is the pair as
		// one signal and every step after this point works on one signal. 0x1a0 is the other end of
		// the same trade: it puts m[42], the state 0x1bd keeps, into r[119], and nothing between
		// them reads r[119] from anything else.
		// DD_DRIVE[0] is the one entry of the drive table storeDrive never writes, so the word here is
		// the program's own: a quarter with the route x16, which puts m[42], the state 0x1bd keeps, into
		// r[119]. m[42] is the only cell the drive word reaches on its own.
		case 0x1a0:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the program's own word, a quarter, on r[7], the left signal the base program leaves
		case 0x1a1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[3] takes the left serial input of page 13, and the program's own word mixes the right signal
		// r[8] under what 0x1a1 left in the running value: r[68] is the pair as one signal, and it is the
		// DD_HPF's input. Nothing else in this window reads r[7] or r[8].
		case 0x1a2:
			c.queue_d(3, c.in[0][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(68, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1a3 to 0x1ab: ins::DD_HPF, the input highpass, and the first three delay taps ----
		//
		// The section is the usual five steps: b2, b1 and b0 build the feed-forward sum, then -a2 and
		// -a1 carry the state. The b0 step has rcopy, which is what lets the same section use one
		// register for two taps: r[69] takes the input a sample ago, and 0x1a3 reads it one step
		// back again for b2. The two feedback registers are chained the same way, by the rcopy at
		// 0x1a8, and both of them then hold the same value, so each section's state is a recursion on
		// (-a1 - a2) rather than a two-state one.
		//
		// Only the state is used: the feed-forward sum goes into the running value at 0x1a5 and 0x1a6
		// overwrites it, and 0x1aa picks the state up into r[124], which is what the rest of the window
		// reads the section for. The host designs this table with kind HPF1, which leaves b2 and -a2
		// at zero, so the feed-forward part is b0 and b1 and the state recursion is on -a1 alone.
		//
		// The three delay RAM reads are in here too, one word apart: slot 0x69 at 0x1a4, slot 0x6a at
		// 0x1a8 and slot 0x6b at 0x1ac, the second, third and fourth of ins::DD_SLOTS' taps.
		// DD_HPF[2], the b2 word, on r[69], which 0x1a5's rcopy fills with the input as it stood a sample
		// ago. Nothing adds r[69] to the section's output, so with b2 = 0 the feed-forward part is the
		// b0 and b1 terms alone; the host's HPF1 design leaves b2 and -a2 at zero.
		case 0x1a3:
			c.queue_d(7, c.in[1][13]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[69];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_HPF[1], the b1 word, on r[68] as it stood before 0x1a2's write lands, and the step reads the
		// delay line's first tap, slot 0x69 of ins::DD_SLOTS
		case 0x1a4:
			// the delay RAM read for slot 0x69
			c.push_hist(st, c.ram[c.addr(0x69, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_HPF[0], the b0 word: the section's feed-forward sum. rcopy makes r[69] the input a sample
		// ago, which is the x[-1] 0x1a3 wants one step further back.
		case 0x1a5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[68];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(69, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_HPF[4], the -a2 word, on r[71], which 0x1a8's rcopy fills with r[70] as it stood a sample ago
		case 0x1a6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[71];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_HPF[3], the -a1 word: the section's state into r[70], route x2. Both feedback terms read the
		// same state, because rcopy puts r[70]'s own value into r[71], so this is a one-state recursion
		// on (-a1 - a2) rather than a two.
		case 0x1a7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(70, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_DRIVE[1], the drive word, on r[119]; the step reads the second tap, slot 0x6a, and rcopy makes
		// r[71] the section's state a sample ago. The running value this leaves is not read again:
		// 0x1a9 adds nothing to it.
		case 0x1a8:
			// the delay RAM read for slot 0x6a
			c.push_hist(st, c.ram[c.addr(0x6a, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(71, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(60, yss236_core::clip(y, 128.0));
			break;
		// m[56] takes the read 0x1a4 made. Five steps is CAPMIN, the nearest step()'s capture reaches.
		case 0x1a9:
			c.queue_d(56, c.captured(m[56], st));
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		// the program's own word, a half with the route x2: r[124] is the DD_HPF's output, and it is the
		// only thing this window reads the section for.
		case 0x1aa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[70];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_DRIVE[2], the drive word, on m[60], the cell 0x1b0 fills
		case 0x1ab:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ac to 0x1ae: ins::DD_ATTACK, the envelope follower ----
		//
		// r[74] is the envelope the ratio row built at 0x1fe and r[75] is a plain copy of it. The host
		// writes the same word into 0x1ac and 0x1ad and t into 0x1ae, and storeCompAttack's words are
		// (0x8000 - t) / 2 and t, so the two applications together are 1 - t and the step is
		// r[76] = t * r[76] + (1 - t) * r[74]: a one-pole smoother with unity gain. t[9] takes the
		// result, and 0x1b1 and 0x1b6 are the only two steps that read t[9], both as the multiplier
		// input rather than as the word at the step.
		// DD_ATTACK[0] on r[74], the envelope 0x1fe keeps; the step reads the third tap, slot 0x6b, and
		// rcopy makes r[75] a plain copy of r[74], so the same word can be applied twice
		case 0x1ac:
			// the delay RAM read for slot 0x6b
			c.push_hist(st, c.ram[c.addr(0x6b, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[74];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(75, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[57] takes the read 0x1a8 made; DD_ATTACK[1] carries the same word as DD_ATTACK[0], which
		// storeCompAttack and storeGateAttack both write, so the attack word lands twice
		case 0x1ad:
			c.queue_d(57, c.captured(m[57], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[75];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_ATTACK[2], the word t, on r[76], and t[9] takes the result as a 1.15 word. The host writes
		// t into that step and (0x8000 - t) / 2 into 0x1ac and 0x1ad, so the two applications
		// together are 1 - t: r[76] is a one-pole smoother of r[74] with unity gain, and t[9] is
		// where 0x1b1 and 0x1b6 read it.
		case 0x1ae:
			k = yss236_core::s16(c.coef[st]);
			xv = r[76];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(76, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;

		// ---- 0x1af to 0x1bd: ins::DD_BAND's series, the fourth tap, and ins::DD_LPF ----
		//
		// The fourth tap is slot 0x6c at 0x1b0, the last of ins::DD_SLOTS' four, and m[60] takes that
		// step's result rather than the read: it is DD_MODE[2] on the highpass section's state, with
		// the clamp taking |y|. m[60] is what 0x1ab scales by the drive word a pass later.
		//
		// 0x1b3 to 0x1b9 are the release word on m[60], DD_MODE[0] on r[125], and the two steps that
		// build r[77] from it. r[72] and r[73] are the release pair's registers, and both are their
		// own rcopy's value, so neither changes after reset: r[120] at 0x1ca is the threshold word
		// times zero and the ladder at 0x1ee is a staircase of constants. INFERRED: that is what the
		// rcopy at 0x1b4 and 0x1c2 does on their own, and no register other than those two writes
		// them, but the checker only proves the arithmetic, not that the compressor is inert.
		//
		// ins::DD_LPF runs on r[77] at 0x1bb to 0x1bd, and m[42] takes the result: the state 0x1a0
		// scales into the drive.
		// DD_BAND[1], the edge's gain word, on r[119]
		case 0x1af:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_MODE[2], the mode word, on r[124], and the clamp takes |y|; the step reads the fourth tap,
		// slot 0x6c. m[60] takes the result, so the section's output reaches the drive a pass later.
		case 0x1b0:
			// the delay RAM read for slot 0x6c
			c.push_hist(st, c.ram[c.addr(0x6c, 0, false)]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			y = c.acc = yss236_core::accmode(y, 3);
			c.queue_d(60, yss236_core::clip(y, 128.0));
			break;
		// m[58] takes the read 0x1ac made. The multiplier input is t[9] and not the word here, so that
		// word is unread: r[125] = t[9] * r[124], and t[9] takes the result again, 0x1b6 reading the
		// new one.
		case 0x1b1:
			c.queue_d(58, c.captured(m[58], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(125, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			c.t_write(c.ops[st], st, y);
			break;
		// DD_BAND[2], the edge's first series word, on r[120]
		case 0x1b2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_RELEASE[0], the release word, on r[72], less m[60]
		case 0x1b3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[60];
			m2 = xv;
			prod = k * m2;
			a = c.src;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// the same release word on m[60], added to what 0x1b3 left, and rcopy makes r[72] the value it
		// held when 0x1b3 read it, which is r[72]'s own value a pass ago
		case 0x1b4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(72, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// m[59] takes the read 0x1b0 made; DD_BAND[3], the edge's second series word, on r[121]
		case 0x1b5:
			c.queue_d(59, c.captured(m[59], st));
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the multiplier input is t[9] again: r[126] = t[9] * r[124], and the word at this step is unread
		case 0x1b6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = c.t[9] * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(126, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_MODE[1], which only the two distortion-delay types write: compressorDistortionDelay leaves
		// it alone and distortionDelay sets it to zero
		case 0x1b7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_MODE[0] on r[125]
		case 0x1b8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the program's own word, zero: r[77] is DD_MODE[0] * r[125] under the route x16 and nothing else
		case 0x1b9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[126];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(77, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_BAND[4], the edge's third series word, on r[122]
		case 0x1ba:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_LPF[1], the b1 word, on r[77] as it stood a sample ago
		case 0x1bb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF[0], the b0 word: the section's feed-forward sum
		case 0x1bc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[77];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF[2], the -a1 word, and m[42] takes the result. m[42] is the leaky integrator of this
		// section's output that 0x1a0 scales into r[119]: the drive is the lowpassed sum of
		// DD_MODE[0] * t[9] * r[124], held by its own leak.
		case 0x1bd:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[42];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.queue_d(42, yss236_core::clip(y, 128.0));
			break;

		// ---- 0x1be to 0x1c9: ins::DD_RELEASE's other two words, and the edge's sum ----
		//
		// The release pair's second and third words sit here, on the same two registers as above, and
		// 0x1c3 starts ins::DD_BAND's series: the fourth word into r[123], then 0x1c4 to 0x1c9 add a
		// fifth of each of r[120] to r[123] back into r[119]. The five words are the geometric series
		// storeEdge builds from the tone's edge byte, so this is the edge's contribution summed with a
		// fifth of each stage rather than used on its own.
		// the program's own word, zero, on r[73]: it clears the running value the release pair adds to
		case 0x1be:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_RELEASE[1], the release word, on r[73]
		case 0x1bf:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc / 32768.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the program's own word, zero, on m[60], less what 0x1bf left
		case 0x1c0:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_RELEASE[2], the complement of the release word, on r[72], less what 0x1c0 left
		case 0x1c1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[72];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod - a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 2);
			break;
		// the program's own word, zero, on m[60]; rcopy makes r[73] the r[72] that 0x1c1 read
		case 0x1c2:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[60];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(73, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_BAND[5], the edge's fourth series word, on r[123]; m[4] takes the left serial input of page 14
		case 0x1c3:
			c.queue_d(4, c.in[0][14]);
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 16.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_BAND[6]. storeEdge sets this step's word to zero in the host's coef[] and writes steps 1 to 5
		// to the chip and no others, so the word the chip reads here is the program's own, zero.
		case 0x1c4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_BAND[7] to DD_BAND[11], a fifth of each of the four series states, summed in order
		case 0x1c5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1c8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_BAND[11]: r[119] is the edge's sum, and 0x1cc scales it into r[78]
		case 0x1c9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(119, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ca to 0x1cc: ins::DD_THRESHOLD, an empty step, and ins::DD_DRIVE's gain word ----
		case 0x1ca:
			k = yss236_core::s16(c.coef[st]);
			xv = r[73];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(120, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// an empty step, like 0x1ff: it counts, lands the writes issued three steps ago, and nothing else
		case 0x1cb:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		// DD_DRIVE[3], the gain word storeDrive derives from the drive and the output level, on r[119]
		case 0x1cc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(78, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1cd to 0x1db: ins::DD_LPF2, ins::DD_PEAK and ins::DD_LOW_SHELF, in that order ----
		//
		// Three sections of the same shape as the DD_HPF, each fed by the register the one before it
		// wrote: DD_LPF2 from r[78], DD_PEAK from r[80] and DD_LOW_SHELF from r[82]. Their
		// feed-forward sums are all overwritten by the step that follows them, so what each leaves is
		// its state: r[80] for DD_LPF2, r[82] for DD_PEAK and r[84] for DD_LOW_SHELF. r[84] is the
		// one the dry and wet mix below reads, and it is the only one of the three read anywhere.
		// DD_LPF2[2], the b2 word, on r[79], which 0x1cf's rcopy fills with the input a sample ago
		case 0x1cd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[79];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF2[1], the b1 word, on r[78] as it stood before 0x1cc's write lands
		case 0x1ce:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF2[0], the b0 word: the section's feed-forward sum, and rcopy makes r[79] the x[-1] 0x1cd wants
		case 0x1cf:
			k = yss236_core::s16(c.coef[st]);
			xv = r[78];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(79, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF2[3], the -a1 word, on r[80]
		case 0x1d0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LPF2[4], the -a2 word: the section's state into r[80], route x2, on the same one-state
		// recursion as 0x1a7
		case 0x1d1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(80, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_PEAK[2], the b2 word, on r[81], which 0x1d4's rcopy fills with the input a sample ago
		case 0x1d2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[81];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_PEAK[1], the b1 word, on r[80] as it stood before 0x1d1's write lands
		case 0x1d3:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_PEAK[0], the b0 word: the section's feed-forward sum
		case 0x1d4:
			k = yss236_core::s16(c.coef[st]);
			xv = r[80];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(81, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_PEAK[3], the -a1 word, on r[82]
		case 0x1d5:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_PEAK[4], the -a2 word: the section's state into r[82], route x2
		case 0x1d6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(82, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_LOW_SHELF[2], the b2 word, on r[83], which 0x1d9's rcopy fills with the input a sample ago
		case 0x1d7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[83];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LOW_SHELF[1], the b1 word, on r[82] as it stood before 0x1d6's write lands
		case 0x1d8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LOW_SHELF[0], the b0 word: the section's feed-forward sum
		case 0x1d9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[82];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(83, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LOW_SHELF[4], the -a2 word, on r[85], the r[84] 0x1db read, which 0x1dc's rcopy put there
		case 0x1da:
			k = yss236_core::s16(c.coef[st]);
			xv = r[85];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LOW_SHELF[3], the -a1 word: the section's state into r[84], route x16. r[84] is the wet
		// signal 0x1e0 and 0x1e2 mix, so this state is what the dry and wet mix works on.
		case 0x1db:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 16.0;
			c.queue_r(84, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1dc to 0x1de: ins::DD_LPF_2, the delay line's feedback ----
		//
		// A first-order section, so three steps: b0 on m[56] (the first tap), b1 on m[43] (which
		// 0x1dd fills with the m operand 0x1dc overwrote, m[56] a pass on), and -a1 into r[86].
		// DD_LPF_2[0], the b0 word, on m[56], the first tap; rcopy makes r[85] the r[84] 0x1db read
		case 0x1dc:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[56];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			c.queue_r(85, c.haspx ? c.px : xv);
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// m[43] takes the m operand this step overwrote, which is m[56] a pass on, and DD_LPF_2[1],
		// the b1 word, scales m[43]
		case 0x1dd:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[43];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			c.queue_d(43, c.psrc);
			break;
		// DD_LPF_2[2], the -a1 word: r[86], the delay line's feedback, sums the first two taps and leaks
		case 0x1de:
			k = yss236_core::s16(c.coef[st]);
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			c.queue_r(86, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1df to 0x1ed: the dry and wet mix, the taps, the line's write, and the two outputs ----
		//
		// dryWet() interleaves, so 0x1df to 0x1e2 alternate dry and wet: each channel's dry word
		// scales r[7] or r[8] and each channel's wet word scales the same r[84]. The channels differ
		// only in the dry signal they take.
		//
		// 0x1e3 to 0x1e6 take the four delay taps. Only the fourth, m[59], survives: 0x1e4 and 0x1e6
		// multiply the third and the second by the program's own zero words, so r[124] is zero at 0x1ed
		// and the second and third taps reach neither output. The first two taps are not dropped,
		// though: they went into r[86] at 0x1de, and r[86] is what the line's feedback scales.
		//
		// 0x1e7 to 0x1ea add a quarter of the mixed signals to the running value, and 0x1e9 is the
		// line's write. 0x1eb and 0x1ed are the outputs: each is DD_LEVEL scaled onto its tap plus a
		// quarter of its mixed signal under the route x4.
		// DD_WET[0], the dry word, on r[7]: the left dry signal
		case 0x1df:
			k = yss236_core::s16(c.coef[st]);
			xv = r[7];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_WET[1], the wet word, on r[84]: the left signal mixed. dryWet() interleaves, so the steps
		// alternate dry, wet, dry, wet and the two channels differ only in the dry word they take.
		case 0x1e0:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(121, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// DD_WET[2], the dry word, on r[8]: the right dry signal
		case 0x1e1:
			k = yss236_core::s16(c.coef[st]);
			xv = r[8];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_WET[3], the wet word, on r[84]: the right signal mixed
		case 0x1e2:
			k = yss236_core::s16(c.coef[st]);
			xv = r[84];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(122, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the program's own word, all but full scale, on m[59], the fourth tap
		case 0x1e3:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[59];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the program's own word, zero, on m[58], so r[123] is m[59] alone: the third tap is read and
		// then dropped by the zero word
		case 0x1e4:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(123, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the program's own word, zero, on m[58]
		case 0x1e5:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[58];
			m2 = c.src;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// the program's own word, zero, on m[57]: r[124] is left at zero, so the second tap is dropped
		// the same way and 0x1ed's r[124] term is zero
		case 0x1e6:
			k = yss236_core::s16(c.coef[st]);
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[57];
			m2 = c.src;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			c.queue_r(124, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// the program's own word, a quarter, on r[121]
		case 0x1e7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// a quarter on r[122], added to what 0x1e7 left under the route x2
		case 0x1e8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_FB[0], the feedback word, on r[86]: this is the delay line's write, and the RAM at slot 0x7a
		// takes the clipped result. wdata is set and memop is 1, so step() clears the write latch
		// rather than setting it, and the RAM gets this step's own result.
		case 0x1e9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[86];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 1);
			c.haswl = false;
			outv = yss236_core::clip(y, 32.0);
			c.ram[c.addr(0x7a, 0, false)] = outv;
			break;
		// a quarter on r[121] again
		case 0x1ea:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LEVEL[0], the mix word, on r[123], added to a quarter of r[121]: the left output, r[17]
		case 0x1eb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(17, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;
		// a quarter on r[122]
		case 0x1ec:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_LEVEL[2], the mix word, on r[124], added to a quarter of r[122]: the right output, r[18]
		case 0x1ed:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			c.queue_r(18, yss236_core::mode(y, 1));
			y = c.acc = yss236_core::accmode(y, 1);
			break;

		// ---- 0x1ee to 0x1ff: the level ladder and ins::DD_RATIO, the envelope this window's gain comes from ----
		//
		// The ladder is eight class-1 steps and ins::DD_RATIO is the compiler's order of them, which is
		// not the order the program runs: 0, 2, 3, 1, 4, 5, 6, 7. The eight offsets in the class-1
		// words are the program's own, and 0x1fe adds the weighted ladder to full scale under a clamp
		// that drops negatives, so r[74] is the curve the attack word smooths. DD_LEVEL and DD_MODE
		// take the same curve's two halves, so the four types differ mostly in which of these steps
		// the host writes: compressorDistortionDelay writes DD_MODE[0] and DD_MODE[2] and leaves
		// DD_MODE[1] alone, distortionDelay writes DD_MODE[0] and DD_MODE[1].
		// the level ladder. These eight are class 1, so each is 128 * the word here plus r[120], and the
		// offsets are the program's own: a quarter, a half, 1, 2, 4, 8, 16 and 64. The clamp drops
		// negatives, so each rung is max(0, r[120] - offset) and the eight are a staircase under
		// r[120] rather than a set of levels.
		case 0x1ee:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(119, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1ef:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(121, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f0:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(122, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f1:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(123, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f2:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(124, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f3:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(125, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f4:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(126, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		case 0x1f5:
			xv = r[120];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			a = c.x;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(127, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// the ratio row. DD_RATIO is the compiler's order and the program's order differs: the steps run
		// 0, 2, 3, 1, 4, 5, 6, 7. Each rung is weighted by its word and the running value keeps
		// the sum, so the eight words are a piecewise-linear curve over the ladder's eight bands.
		case 0x1f6:
			k = yss236_core::s16(c.coef[st]);
			xv = r[119];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = 0.0;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f7:
			k = yss236_core::s16(c.coef[st]);
			xv = r[121];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f8:
			k = yss236_core::s16(c.coef[st]);
			xv = r[122];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1f9:
			k = yss236_core::s16(c.coef[st]);
			xv = r[123];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fa:
			k = yss236_core::s16(c.coef[st]);
			xv = r[124];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 2.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// DD_RATIO[5], the last of the six steps with the route x2
		case 0x1fb:
			k = yss236_core::s16(c.coef[st]);
			xv = r[125];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 4.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fc:
			k = yss236_core::s16(c.coef[st]);
			xv = r[126];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		case 0x1fd:
			k = yss236_core::s16(c.coef[st]);
			xv = r[127];
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = true;
			c.psrc = c.src; c.src = m[0];
			m2 = xv;
			prod = k * m2;
			a = c.acc;
			y = (prod + a) * 1.0;
			y = c.acc = yss236_core::accmode(y, 0);
			break;
		// class 1: r[74] = full scale + the weighted ladder, clamped at zero and at full scale. This is
		// the envelope 0x1ac smooths, and it is the only reader of the ladder there is.
		case 0x1fe:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			c.psrc = c.src; c.src = m[0];
			a = c.acc;
			v = 128.0 * yss236_core::s16(c.coef[st]) + a;
			v *= 1.0;
			c.queue_r(74, yss236_core::mode(v, 2));
			c.acc = yss236_core::accmode(v, 2);
			break;
		// the program's last step, empty like 0x1cb
		case 0x1ff:
			xv = 0.0;
			c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = false;
			break;
		}
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m13(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

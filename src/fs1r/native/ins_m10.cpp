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
//          a pass later than it is written, so the section is in direct form I. SECTIONS below is those
//          two and the four of the tone chain.
//
//   0x1b0-0x1b3  the latch: m[42] = r[70] and r[81] = 0, run only when the compare at 0x1a3 fired.
//
//   0x1b4-0x1b9  the quantizer's input and the output mix. 0x1b4 takes m[42] - r[79], the held signal
//          less the second error accumulator. 0x1b5 and 0x1b6 are ins::LOFI_WET[0] and ins::LOFI_WET2[0],
//          the dry and wet gains, into r[17], and 0x1b8 and 0x1b9 the same two words for r[18]. The dry
//          signal is the input and the wet is r[119], so the two channels share one crushed signal and
//          differ only in their dry side. MIXES below is those two pairs of steps.
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
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check ins-m10: identical, 1 type, 9600 native passes.
#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The registers and cells the header names, so the arithmetic below can say what it reads
// ---------------------------------------------------------------------------------------------

// The two signals the base program leaves this window and the two outputs it reads back, the input
// chain's sum, the bit assign's output level, the sample and hold's rate counter, the two error
// accumulators, the wet signal the tone chain hands over, and the scratch register every stage passes
// through. The tone chain's own registers, r[72] to r[78], are named by the sections below, which are
// what read and write them, and so are the input chain's r[67] to r[71].
enum
{
	SIG_L = 7,       // the two signals in, r[7] and r[8]: the panned sends the base program leaves here
	SIG_R = 8,
	OUT_L = 17,      // the two outputs the base program reads back
	OUT_R = 18,
	IN_SUM = 67,     // the input chain's sum, ins::LOFI_GAIN over both signals
	LEVEL = 72,      // the bit assign's output level, which the tone chain's first section reads
	RATE = 81,       // the sample and hold's rate counter
	ERR_A = 80,      // the two error accumulators around the quantizer
	ERR_B = 79,      // and the one the quantizer's input subtracts from the held signal
	WET = 119,       // the tone chain's output from the pass before: the wet side of both mixes
	SCRATCH = 121,   // the register every stage hands to the next
	HELD = 42,       // m[42], the held signal
	QSTEP = 56,      // and m[56], the quantizer's step, the two cells this program writes and reads
};

// ---------------------------------------------------------------------------------------------
// The six filter sections: two over the input, four over the quantized signal
// ---------------------------------------------------------------------------------------------

// What one step of a section computes, named after the coefficient designFilter() wrote for it: b0, b1
// and b2 are the numerator's and -a1 and -a2 the denominator's, in the order that function emits them.
// The step that opens a section's sum is not in this list, because it is a word of its own and no two
// sections open theirs on the same register; the three that end in X1 or Y2 are the steps that carry the
// section's z^-1, which is how it holds x[-1] or y[-2] in a register of its own, and the three that end
// in CLOSE are the three shapes a section's last step has, one per coefficient that can close it.
enum part_t
{
	B0_CLOSE,   // b0 on the input, closing the section into its output with the section's route
	B0_X1,      // b0 on the input, and the z^-1 that keeps the input a pass back
	B1,         // b1 on the input, added to the sum
	B1_Y2,      // b1 on the input, and the z^-1 that keeps the output two passes back
	B2_CLOSE,   // b2 on the input a pass back, closing the section
	A1_SUM,     // -a1 on the output a pass back, added to the sum
	A1_Y2,      // -a1 on the output a pass back, and the z^-1 that keeps the output two passes back
	A2,         // -a2 on the output two passes back
	A2_CLOSE,   // -a2 on the output two passes back, closing the section
};

// One filter section, in direct form I. `st` is its first step and `nsteps` how many steps it runs,
// three for the four order-1 sections and five for the two order-2 ones. `open_on` is the register the
// section opens its sum on, which is its own output for the sections that open on -a1, the input for the
// one that opens on b1, and x[-1] for the one that opens on b2. `in` is the register the section's input
// arrives in, `out` the one the closing step writes, which is the next section's input, `x1` the one that
// holds the input a pass back and `y2` the one that holds the output two passes back.
struct Section
{
	int st, nsteps;
	int open_on, in, out, x1, y2;
	step::route_t route;
	part_t part[4];
};

// The two sections over the input chain, then the four over the quantized signal. The four steps that
// carry a z^-1 are the only ones that are not one of a section's own words: two hold the input a pass
// back for the b2 term and two hold the output two passes back for the -a2 term.
const Section SECTIONS[6] = {
    //   st   n  open   in  out  x1  y2  route      the rest of the section's steps
    {  0x1a7, 3,   68,  67,  68,   0,   0, ROUTE_4, { B1,               B0_CLOSE    } },
    {  0x1aa, 5,   70,  68,  70,  69,  71, ROUTE_2, { B1_Y2, B0_X1, A2, B2_CLOSE  } },
    {  0x1cf, 3,   73,  72,  73,   0,   0, ROUTE_2, { B1,               B0_CLOSE    } },
    {  0x1d2, 5,   74,  73,  75,  74,  76, ROUTE_4, { B1, B0_X1, A1_SUM, A2_CLOSE  } },
    {  0x1d7, 3,   75,  75,  77,   0,  76, ROUTE_4, { A1_Y2,            B0_CLOSE    } },
    {  0x1da, 3,   78,  77,  78,   0,   0, ROUTE_4, { B1,               B0_CLOSE    } },
};

// The section a step belongs to, or -1 when it belongs to none: 0x1a7 to 0x1ae for the input chain and
// 0x1cf to 0x1dc for the tone chain. The sections never overlap and the index is the same on every
// pass, so the step reaches its section rather than looking through the six for it.
constexpr auto SECTIONS_AT = index_by_span(SECTIONS);

// One section's step k. The first opens the sum with its own word and the adder off, and the rest are the
// section's other words in its own order, which is why they come out of the part list rather than from a
// fixed shape: the schedule interleaves a section's z^-1 copies among the five words designFilter wrote.
void section_step(yss236_core& c, int st, Section const& s, int k)
{
	if (k == 0)                                       // the section's opening word, the adder off
	{
		term(c, st, s.open_on);
		return;
	}
	switch (s.part[k - 1])
	{
	case B0_CLOSE: close(c, st, s.in, s.out, s.route); break;
	case B0_X1:    copy_z(c, st, s.in, s.x1, ADD_ACC, CLAMP_WRAP); break;
	case B1:       sum(c, st, s.in); break;
	case B1_Y2:    copy_z(c, st, s.in, s.y2, ADD_ACC, CLAMP_WRAP); break;
	case B2_CLOSE: close(c, st, s.x1, s.out, s.route); break;
	case A1_SUM:   sum(c, st, s.out); break;
	case A1_Y2:    copy_z(c, st, s.out, s.y2, ADD_ACC, CLAMP_WRAP); break;
	case A2:       sum(c, st, s.y2); break;
	case A2_CLOSE: close(c, st, s.y2, s.out, s.route); break;
	}
}

// ---------------------------------------------------------------------------------------------
// The two output mixes, one pair of steps each
// ---------------------------------------------------------------------------------------------

// One channel's mix of the dry signal and the wet one. The first step applies the dry word to the signal
// the base program leaves this window with the adder off, and the second closes the sum with the wet word
// on r[119], the tone chain's output from the pass before, into that channel's output register.
struct Mix { int st, nsteps, dry, out; };

const Mix MIXES[2] = {
    { 0x1b5, 2, SIG_L, OUT_L },   // ins::LOFI_WET[0] and ins::LOFI_WET2[0], into r[17]
    { 0x1b8, 2, SIG_R, OUT_R },   // the same two words again, into r[18]
};

// One mix's two steps. The closing step's route is the x4 both channels' outputs carry.
void output_mix(yss236_core& c, int st, Mix const& m)
{
	if (st == m.st) term(c, st, m.dry);
	else             close(c, st, WET, m.out, ROUTE_4);
}

// The mix a step belongs to, or -1 when it belongs to none. Each mix owns two steps and the two do not
// meet, so the index is the same on every pass and the step reaches its mix rather than looking for it.
constexpr auto MIXES_AT = index_by_span(MIXES);

// ---------------------------------------------------------------------------------------------
// What a step moves into a cell before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step of this program usually also takes a serial input, and these three are the only cells the window
// writes that nothing in it reads: m[3] and m[7] from the two banks of page 13, and m[4] from page 14.
// SERIAL_L takes its page's left serial input and SERIAL_R its right, and the take comes before the
// arithmetic of the same step, because step() moves a cell before it does anything else, so the step loop
// reads this table at the top of every step.
enum decor_t { SERIAL_L, SERIAL_R };

// One decor, and the step it rides on.
struct Decor { int st; decor_t kind; int cell; };

const Decor DECORS[3] = {
    //  the step  the decor                 the cell
    {  0x1a6, SERIAL_L,  3 },     // on the step that closes the input chain's sum into r[67]
    {  0x1a7, SERIAL_R,  7 },     // on the first step of the input chain's first section
    {  0x1c3, SERIAL_L,  4 },     // in the quantizer's padding, after the negative arm
};

// The decor of a step, or -1 when it carries none. Each of the three is on a step of its own, so the
// index is the same on every pass and the step loop reaches its decor rather than looking through the
// three for it.
constexpr auto DECORS_AT = index_by_step(DECORS);

// Apply the decor, which comes before the step's term.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case SERIAL_L: take_serial(c, d.cell, st, 0); break;
		case SERIAL_R: take_serial(c, d.cell, st, 1); break;
	}
}

// ---------------------------------------------------------------------------------------------
// The blocks of steps that are not a table of anything
// ---------------------------------------------------------------------------------------------

// The sample and hold's rate counter, 0x1a0 to 0x1a3: the counter, the quantizer's step, the rate the
// counter is compared against, and the compare. r[RATE] climbs by one word of 1/256 of full scale a pass
// and saturates rather than wraps, ins::LOFI_BITS lands in m[56] as the word the quantizer clamps to, and
// ins::LOFI_RATE lands in the running value so the last step has a threshold to test the counter against.
// That compare's sign is the flag the sample and hold at 0x1b0 is gated on.
void rate_counter(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1a0:
		// r[81] = k / 256 + r[81], the ROM's own word of one, and the register saturates as it goes
		load(c, st, RATE, 0, ADD_REG, false, ROUTE_1, CLAMP_SAT, RATE);
		break;
	case 0x1a1:
		// m[56] = ins::LOFI_BITS[0] / 256, the quantizer's step
		load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, QSTEP);
		break;
	case 0x1a2:
		load(c, st, 0, 0);                 // acc = ins::LOFI_RATE[0] / 256, the threshold
		break;
	default:
		// the compare: acc = r[81] - acc, and its sign is the flag the latch at 0x1b0 waits on
		run(c, {st, RATE, 0, M2_REG, true, 0, ADD_ACC, true, ROUTE_1, CLAMP_WRAP, 0, false, 0, false, true});
		break;
	}
}

// The input chain's sum, 0x1a5 and 0x1a6: the two signals the base program leaves the window, each scaled
// by its own word of ins::LOFI_GAIN and added into r[67], which is what the first of the two sections
// reads. The gain is the bit assign's, (32767 >> assign) >> 1.
void input_sum(yss236_core& c, int st)
{
	if (st == 0x1a5)
	{
		term(c, st, SIG_L);              // acc = ins::LOFI_GAIN[0] * r[7]
		return;
	}
	close(c, st, SIG_R, IN_SUM);         // r[67] = ins::LOFI_GAIN2[0] * r[8] + acc
}

// The sample and hold, 0x1b0 to 0x1b3: the gate on the counter compare at 0x1a3, the hold itself into
// m[42], and the counter cleared so it climbs again to the rate. None of the block runs when the gate
// skips it, which is what makes the quantizer's input a sample and hold at ins::LOFI_RATE.
void sample_and_hold(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b0:
		// cskip 6: the three steps to 0x1b3 run only while the flag from the compare at 0x1a3 is positive
		skip_positive(c, st, 0x1b4);
		break;
	case 0x1b1:
		// m[42] = r[70], the sectioned signal held for the quantizer. A class-3 step's word is 1.0 and
		// it passes the value through (s19), so nothing is applied to the register here.
		run(c, {st, 70, 0, M2_REG, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, false, HELD});
		break;
	case 0x1b2:
		load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, RATE);      // r[81] = 0, the counter again
		break;
	default:
		idle(c);                           // 0x1b3
		break;
	}
}

// One of the quantizer's two compares: r[121] less the step, or r[121] plus it, which puts the value
// back, and the sign of either is the flag the two skips read. Both of these steps saturate, which is
// what their clamp bits say, so this is the general form and not steps.h's compare(), which wraps.
void quantizer_compare(yss236_core& c, int st, bool sub)
{
	run(c, {st, SCRATCH, QSTEP, M2_REG, true, 0, ADD_CELL, sub, ROUTE_1, CLAMP_SAT, 0, false, 0, false, true});
}

// One arm of the quantizer's dead zone: r[121] takes the quantizer's own step m[56], scaled by the word at
// this step. The positive arm is a class-3 step, whose word is 1.0 and which passes the cell through
// (s19), and the negative arm scales that same cell by the ROM's own -1 word at 0x1c1.
void quantizer_arm(yss236_core& c, int st, bool unity)
{
	run(c, {st, 0, QSTEP, M2_CELL, unity, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, SCRATCH});
}

// The quantizer, 0x1b7 to 0x1c3: two compares, two conditional skips and a jump around one subtraction.
// r[121] - m[56] lands in r[121] and its sign is the flag, so a value above the step goes to +step and
// jumps out of the block, a value below zero goes to -step, and anything between the two is left as it
// was. The right channel's mix sits in the middle of this block and is not here: MIXES_AT takes its two
// steps before this does, and the decor for m[4] is read at the top of the step loop.
void quantizer(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b7:
		quantizer_compare(c, st, true);              // acc = r[121] - m[56]: was the value above the step?
		break;
	case 0x1ba:
		skip_positive(c, st, 0x1bd);                 // 0x1bb runs only while that flag is positive
		break;
	case 0x1bb:
		quantizer_arm(c, st, true);                  // r[121] = +step
		break;
	case 0x1bc:
		jump(c, st, 0x1c4);                          // cskip 1: out to the error accumulators
		break;
	case 0x1bd:
		quantizer_compare(c, st, false);             // acc = r[121] + m[56], which puts the value back,
		break;                                      // and its sign is the test against zero
	case 0x1c0:
		skip_negative(c, st, 0x1c4);                 // 0x1c1 to 0x1c3 run only while that flag is negative
		break;
	case 0x1c1:
		quantizer_arm(c, st, false);                 // r[121] = -step
		break;
	case 0x1c3:
		idle(c);                                     // 0x1c3: the x latch, and m[4] from the decor above
		break;
	default:
		idle(c);                                     // 0x1be, 0x1bf and 0x1c2
		break;
	}
}

// The two error accumulators, 0x1c4 to 0x1cb: the quantized value into r[80] and back out of it through
// the level law, then into r[79], which 0x1b4 takes back out of the quantizer's input. Two integrators
// around the quantizer are what make the crush noise-shaped rather than flat.
void accumulators(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1c4:
		run(c, {st, SCRATCH, 0, M2_REG, true, 0, ADD_OFF});             // acc = r[121], the quantized value
		break;
	case 0x1c5:
		// r[80] = r[80] + acc, the first accumulator, which ins::LOFI_LEVEL scales at 0x1c8
		run(c, {st, ERR_A, 0, M2_REG, true, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, ERR_A});
		break;
	case 0x1c8:
		gain(c, st, ERR_A, SCRATCH, ROUTE_16);                          // r[121] = ins::LOFI_LEVEL * r[80] * 16
		break;
	case 0x1c9:
		run(c, {st, SCRATCH, 0, M2_REG, true, 0, ADD_OFF});             // acc = r[121]
		break;
	case 0x1ca:
		// r[79] = r[79] + acc, the second accumulator, which 0x1b4 subtracts from the held signal
		run(c, {st, ERR_B, 0, M2_REG, true, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, ERR_B});
		break;
	case 0x1cb:
		gain(c, st, SCRATCH, SCRATCH, ROUTE_16);                        // r[121] = r[121] * 16
		break;
	default:
		idle(c);                                                         // 0x1c6 and 0x1c7
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. There is no gate in this program, but the three cskip steps below do
// use the same guard, and the m_skip they set is what selects the quantizer's branch.
void block_0x1a0(yss236_core& c)
{
	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		// the three serial inputs this window publishes, before the step's own arithmetic
		if (int const i = DECORS_AT[st]; i >= 0) decor(c, st, DECORS[i]);

		// ---- 0x1a0 to 0x1a3: the rate counter, the quantizer's step and the compare on the two ----
		if (st < 0x1a4) { rate_counter(c, st); continue; }

		// ---- 0x1a4: the wet signal, r[119] = 2 * r[78] ----
		if (st == 0x1a4) { gain(c, st, 78, WET, ROUTE_4); continue; }

		// ---- 0x1a5 and 0x1a6: the two signals in, summed at ins::LOFI_GAIN ----
		if (st < 0x1a7) { input_sum(c, st); continue; }

		// ---- the six sections: two over the input sum, then the four of the tone chain ----
		if (int const i = SECTIONS_AT[st]; i >= 0)
		{
			Section const& s = SECTIONS[i];
			section_step(c, st, s, st - s.st);
			continue;
		}

		// ---- 0x1af: the one step of padding between the input chain and the sample and hold ----
		if (st < 0x1b0) { idle(c); continue; }

		// ---- 0x1b0 to 0x1b3: the sample and hold, gated on the counter compare ----
		if (st < 0x1b4) { sample_and_hold(c, st); continue; }

		// ---- 0x1b4: the held signal less the second error accumulator, into the scratch register ----
		// The word at this step is the ROM's own -1, so this is m[42] - r[79], the quantizer's input.
		if (st == 0x1b4)
		{
			run(c, {st, ERR_B, HELD, M2_REG, false, 0, ADD_CELL, false, ROUTE_1, CLAMP_SAT, SCRATCH});
			continue;
		}

		// ---- the two output mixes, the left's at 0x1b5 and the right's at 0x1b8, two steps each ----
		if (int const i = MIXES_AT[st]; i >= 0) { output_mix(c, st, MIXES[i]); continue; }

		// ---- 0x1b7 to 0x1c3: the quantizer's dead zone ----
		if (st < 0x1c4) { quantizer(c, st); continue; }

		// ---- 0x1c4 to 0x1cb: the two error accumulators ----
		if (st < 0x1cc) { accumulators(c, st); continue; }

		// ---- 0x1cc and 0x1cd: the padding before the output level ----
		if (st < 0x1ce) { idle(c); continue; }

		// ---- 0x1ce: the bit assign's output level, which is the tone chain's input in r[72] ----
		if (st == 0x1ce) { gain(c, st, SCRATCH, LEVEL, ROUTE_16); continue; }

		// ---- 0x1dd to 0x1ff: 35 zero class-0 steps, nothing but the count and the x latch ----
		idle(c);
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
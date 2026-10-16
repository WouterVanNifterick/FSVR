// fs1r/native/dly_m08.cpp - VOP3-2, the reverb delay window's program for the four delay types: its 72 steps.
//
// rom::REVERB_DELAY_PROGRAMS[8], the program the window holds for reverb types 13 to 16 (Delay L,C,R,
// Delay L,R, Echo, Cross Delay). Types 0 to 12 run dly-m00's program in this window instead, under the
// same keys' first entry, and fx_delay.cpp is the host that feeds both. Under these four types the
// DELAY_ROUTES entry (fx_dispatch.cpp) matches and one of EffectHost::delayLCR, delayLR, echo or
// crossDelay runs, so every one of them rewrites this window's constants and delay offsets when a
// parameter moves. Which of the four is running is not a code difference here: the program words are
// the same under all of them and only the constants (rom::rev::DELAY_WORDS and the damp curve) and the
// six delay offsets (rom::rev::DELAY_SLOTS) differ. The code reads them where they live all the same,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in the five runs the tables below name, one for each of the groups this
// comment opens:
//
//   0x110-0x11d  the two serial inputs and the host's four tone filters, rev::DELAY_LPF, rev::DELAY_HPF
//                and their copies, one three-step one-pole each. r[88] and r[91] are the two first
//                poles, r[89] and r[92] the two second ones, and the second pole of a side is fed from
//                the first. The three reads that ride on the first step of each group (the slots 0x44,
//                0x45 and 0x46, in the reverb's area) are published in m[22], m[23] and m[24].
//   0x11e-0x129  three of the four delay taps, read here and taken up by cells five steps later, and the
//                three captures the windows outside this one read (m[24], m[56] and m[57]). Only one
//                step of the twelve computes anything: 0x128 copies m[56] into r[93].
//   0x12a-0x12f  the fourth tap's read at 0x12b and the two damped feedbacks. rev::DELAY_CURVE and
//                rev::DELAY_CURVE_COPY are a pair of words each on the same operand, r[93] and r[94],
//                into r[119] and r[120]; 0x12c copies m[57] into r[94] between them.
//   0x130-0x140  the two line writes, the two wet sums and the dry/wet mix. 0x133 carries w1[13] and
//                is itself a writer, so it writes its own result into the slot 0x4c (rev::DELAY_SLOTS[0],
//                the left line); 0x139 only latches, and the write at 0x13b into the slot 0x4e
//                (rev::DELAY_SLOTS[3]) takes that latch rather than its own result. r[124] and r[125]
//                are the two wet sums, and r[13] and r[14] the block's outputs after the dry/wet mix.
//   0x141-0x157  six steps of nothing (they only move the z^-1 latch), then three more reads of the
//                block's own area (the slots 0x51, 0x52 and 0x53) published in m[25], m[26] and m[27],
//                the LFO values m[10], and the block's output, the cell m[41] to the serial out at
//                0x156.
//
// The registers: r[3] and r[4] the two inputs of the block, r[87] to r[92] the tone filters' inputs
// and states, r[93] and r[94] the first two taps, r[119] and r[120] those two taps damped, r[124] and
// r[125] the two wet sums, r[13] and r[14] the outputs. The cells: m[1] and m[5] the two serial inputs
// (which no step of this program reads; the filters work on r[3] and r[4] instead), m[8], m[9] and m[10]
// the LFO values it hands on, m[22] to m[27] the six reads it publishes, m[41] the block's output, m[56]
// to m[59] the four taps. There is no t register anywhere in this program: no step has tw or tfrac set,
// and no step takes a multiplier from the t file.
//
// A delay RAM read is made at one step and taken up by a cell 5 to 11 steps later, which is what
// yss236_core::captured() returns and what an msrc of 4 queues. Every read here is five steps ahead of
// its cell, so each capture takes the read its window names. The coefficients are spread one to a step
// rather than one to a group, which is how the host gets a word of its own for each of them: the two
// halves of a damp pair multiply the same operand, the pole of a tone filter is one step and its gain
// the next, and the dry/wet mix is the same three-step shape with a multiply by four instead of two.
//
// The steps below are the shapes of fs1r/native/steps.h with each run's repeated units in a table, and
// a step's own fields folded into the one shape that fits it. Constants come from c.coef[st] on the
// pass and delay offsets through c.addr() off the c.slot[] entry the host last wrote, so no number the
// firmware chose is folded in here. A step's cell move and its delay RAM read happen before its
// arithmetic, which is step()'s order and the order every decor below is applied in.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, which is why the arithmetic below is
// spelled out by name rather than a step at a time.
//
// vop3_native_check dly-m08: identical, 74 types, 710,400 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, idle, run and the rest

// ---------------------------------------------------------------------------------------------
// What a step of the block carries besides its term
// ---------------------------------------------------------------------------------------------

// The decors of one step, in the order step() does them: the delay RAM read comes before the cell
// move, so a step that carries both reads first and publishes into the history it has just pushed.
struct Decors { Decor first, second; };

void decors(yss236_core& c, int st, Decors const& d)
{
	decor(c, st, d.first);
	decor(c, st, d.second);
}

// What a step does with the delay RAM at the end of it. The left line's writer carries the wdata flag
// itself, so it clears any latched data and writes its own result; the right line's value is latched
// two steps before the step that writes the line finds the latch waiting.
enum delay_t { NO_WRITE, WRITE_LATCHED, WRITE_OWN, SET_LATCH };

// The two program words that decide which of those a step does: w1[13] makes a step's result the data
// of the next write, and a step that is that write takes the latched data if there is one.
void delay_flags(step::alu& a, delay_t d)
{
	a.wdata = d == WRITE_OWN || d == SET_LATCH;
	a.write = d == WRITE_OWN || d == WRITE_LATCHED;
}

// ---------------------------------------------------------------------------------------------
// 0x110 to 0x11d: the two inputs and the four tone filters
// ---------------------------------------------------------------------------------------------

// One side's input step: the host's gain (0.5 under every type) on the serial input register the base
// program wrote, scaled by the route's two. The left step takes up the serial value itself into m[1] on
// the way past, and the right one reads the block's own second line and takes up the LFO 8 into m[8];
// neither of those two cells is read by a step of this program.
struct Input { int st, in, out; Decors decor; };

// r[3] and r[4] the block's two inputs, r[87] and r[90] what the four poles below start from.
constexpr Input INPUTS[2] = {
    //   st    in  out   decors
    { 0x110, 3, 87, { {DEC_SERIAL_L, 1}, {}           } },  // the left input, and m[1] the serial value
    { 0x117, 4, 90, { {DEC_READ, 0},     {DEC_LFO, 8} } },  // the right input, and m[8] the LFO 8
};

// The input step of st, if it is one. The index is the same on every pass, so the step reaches its row
// rather than looking through the two for it.
constexpr auto INPUTS_AT = index_by_step(INPUTS);

void input_step(yss236_core& c, int st, Input const& i)
{
	decors(c, st, i.decor);
	gain(c, st, i.in, i.out, ROUTE_2);
}

// One tone filter pole, three steps: -a1 on the pole's own state opens the sum, b1 comes on the pole's
// input, and b0 comes on it again with the sum closed back into the pole's state. So the pole reads
// y = b0 * x + b1 * x + (-a1) * y' with the route its own, and the three words are scaled by
// 2048 * (16 >> scale), which is why a first pole scales by two and a second by four: the route puts
// half the register's span back. `in` is the register the pole's input arrives in, read by words 2 and
// 3, and `out` the pole's state, which word 1 reads as y[-1] and word 3 writes.
struct Pole { int st, nsteps, in, out; route_t route; Decors decor[3]; };

// The host's four word lists, one per pole, three words each, and the program takes them the other way
// round: word 0 of rev::DELAY_LPF is at 0x113 and its word 2 at 0x111. The second pole of a side is fed
// from the first, which is why its `in` is the pole before's `out`.
constexpr Pole POLES[4] = {
    //   st  n  in  out  route   word 1, -a1 * y'    word 2, b1 * x     word 3, b0 * x
    { 0x111, 3, 87, 88, ROUTE_2, { {DEC_SERIAL_R, 5}, {},                {DEC_READ, 0} } },  // DELAY_LPF
    { 0x114, 3, 88, 89, ROUTE_4, { {},                {},                {}            } },  // DELAY_HPF
    { 0x118, 3, 90, 91, ROUTE_2, { {DEC_CAPTURE, 22}, {},                {}            } },  // DELAY_LPF_COPY
    { 0x11b, 3, 91, 92, ROUTE_4, { {DEC_READ, 0},     {DEC_CAPTURE, 23}, {}            } },  // DELAY_HPF_COPY
};

// The pole of step st, if it is one: a pole's three steps are consecutive, so each of them indexes to the
// same row with a different k. The index is the same on every pass.
constexpr auto POLES_AT = index_by_span(POLES);

// One pole's step k of three, at p.st + k.
void pole_step(yss236_core& c, int st, Pole const& p, int k)
{
	decors(c, st, p.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, p.out); break;                   // -a1 on the pole's own state, which is y[-1]
	case 1:  sum(c, st, p.in); break;                     // b1 on the pole's input
	default: close(c, st, p.in, p.out, p.route); break;   // b0 on it, and the pole's new state
	}
}

// The run of 14 steps at 0x110: the left side's input step, its two poles, then the right side's input
// step and its two, which is the order the four one-poles are written in.
void block_0x110(yss236_core& c)
{
	for_steps<0x110, 0x11e>(c, [&](yss236_core& c, auto st)
	{
		if (int i = INPUTS_AT[st]; i >= 0) input_step(c, st, INPUTS[i]);
		if (int p = POLES_AT[st]; p >= 0) pole_step(c, st, POLES[p], st - POLES[p].st);
		});
}

// ---------------------------------------------------------------------------------------------
// 0x11e to 0x129: the four delay taps and the captures for the windows outside this one
// ---------------------------------------------------------------------------------------------

// The twelve steps of the stretch, one decor pair each.
struct TapRun { int base; Decors decor[12]; };

// The six slots rom::rev::DELAY_SLOTS names are {0x4c, 0x47, 0x49, 0x4e, 0x48, 0x4a}, and this program
// reads four of the six and writes the other two. A read is made on one step and the cell that takes it
// up is five steps later, which is the shape of the stretch: a read, the padding and the captures the
// other windows want, then the next read.
//
//   0x11f  the slot 0x47  ->  m[56] at 0x124
//   0x123  the slot 0x48  ->  m[57] at 0x128
//   0x127  the slot 0x49  ->  m[58] at 0x12c
//   0x12b  the slot 0x4a  ->  m[59] at 0x130
//
// Under Delay L,C,R the four are the feedback, right, left and centre taps of the one line; under
// Echo they are the two taps of each of the two lines; under Cross Delay they are the two feeds across.
// Which is which is the host's offsets into DELAY_SLOTS, not this program's.
//
// Every step of the twelve but 0x128 is class 0 with no adder, shift, clamp or sub and no memory cycle
// of its own, so step() only moves the z^-1 latch for it, and what is left of a step is its decor.
constexpr TapRun TAPS = { 0x11e, {
    { {},                {}           },  // 0x11e
    { {DEC_READ, 0},     {}           },  // 0x11f  the slot 0x47, the first tap
    { {DEC_CAPTURE, 24}, {}           },  // 0x120  m[24] takes up the read of 0x11b
    { {},                {}           },  // 0x121
    { {},                {}           },  // 0x122
    { {DEC_READ, 0},     {}           },  // 0x123  the slot 0x48, the second tap
    { {DEC_CAPTURE, 56}, {}           },  // 0x124  m[56] takes up the first tap
    { {},                {}           },  // 0x125
    { {},                {}           },  // 0x126
    { {DEC_READ, 0},     {DEC_LFO, 9} },  // 0x127  the slot 0x49, and m[9] the LFO 9
    { {},                {}           },  // 0x128  the earlier copy step, which brings its own decor
    { {},                {}           },  // 0x129
} };

// One copy step, one per side. Each publishes the read of the next tap in the chain into `capture`,
// the cell that tap is later read out of, and then copies the tap it took up five steps earlier, `tap`,
// into the register that side's damp pair works on, which is what makes the damp pair a multiply of a
// register rather than of a cell. A class-3 step's word is 1.0 and the multiplier passes the cell
// through, so the copy is step()'s plain copy with the adder off.
struct TapCopy { int st, tap, capture, tap_reg; };

// The two copies are four steps apart, which is the distance between the two damp pairs they belong
// to, and each one is written on the next tap's read.
constexpr TapCopy COPIES[2] = {
	{ 0x128, 56, 57, 93 },
	{ 0x12c, 57, 58, 94 },
};

// The copy step of st, if it is one. Both runs that own a stretch of steps below look this up, so the
// index is what keeps a step off the walk over the two.
constexpr auto COPIES_AT = index_by_step(COPIES);

// Run the copy step if this step is one of the two. 0x128 sits in the tap stretch and 0x12c between the
// two damp pairs, so both runs below look this table up and neither owns a copy of its own. True when
// the step was one.
bool tap_copy(yss236_core& c, int st)
{
	if (int i = COPIES_AT[st]; i >= 0)
	{
		take_capture(c, COPIES[i].capture, st);
		run(c, {.st = st, .sm = COPIES[i].tap, .m2 = M2_CELL, .unity = true, .dr = COPIES[i].tap_reg});
		return true;
	}
	return false;
}

// The run of 12 steps at 0x11e, which reads three of the four taps and takes two of them up into cells.
// Only the copy step computes anything; every other step of it is a decor and the latch.
void block_0x11e(yss236_core& c)
{
	for_steps<TAPS.base, 0x12a>(c, [&](yss236_core& c, auto st)
	{
		if (tap_copy(c, st)) return;
		decors(c, st, TAPS.decor[st - TAPS.base]);
		idle(c);
		});
}

// ---------------------------------------------------------------------------------------------
// 0x12a to 0x12f: the two damped feedbacks
// ---------------------------------------------------------------------------------------------

// One damp pair, two steps: the high half of the host's damp curve on the tap register opens the sum
// and the low half closes it into the tap's damped register. `tap_reg` is the register the pair reads,
// which is the one the copy step above copied the tap cell into, and `damped_reg` the one it writes.
// The first pair's closing step also reads the fourth tap's slot.
struct Damp { int st, nsteps, tap_reg, damped_reg; Decors close; };

// EffectHost::storeHighDamp writes c0 = k * DB[12] and c1 = (1 - k) * DB[12] to the pair's two steps, so
// r[119] = (c0 + c1) * r[93] and r[120] likewise of r[94]. Under Delay L,C,R rev::DELAY_CURVE_COPY is
// not written and stays at the ROM's zero.
constexpr Damp DAMPS[2] = {
	//   st  n  tap_reg   damped  the closing step
	{ 0x12a, 2, 93, 119, { {DEC_READ, 0}, {} } },
	{ 0x12e, 2, 94, 120, { {},            {} } },
};

// The damp pair of step st, if it is one: a pair's two steps are consecutive, so each of them indexes to
// the same row with a different k.
constexpr auto DAMPS_AT = index_by_span(DAMPS);

// One damp pair's step k, at d.st + k.
void damp_step(yss236_core& c, int st, Damp const& d, int k)
{
	if (!k) { term(c, st, d.tap_reg); return; }     // the high half of the damp curve, opening the sum
	decors(c, st, d.close);                         // the fourth tap's read rides on the closing step
	close(c, st, d.tap_reg, d.damped_reg, ROUTE_1); // the low half, and the tap's damped state
}

// The run of 6 steps at 0x12a: the first pair, the second pair's copy step, the step between the pairs
// and the second pair itself.
void block_0x12a(yss236_core& c)
{
	for_steps<0x12a, 0x130>(c, [&](yss236_core& c, auto st)
	{
		if (tap_copy(c, st)) return;
		if (int d = DAMPS_AT[st]; d >= 0) damp_step(c, st, DAMPS[d], st - DAMPS[d].st);
		if (st == 0x12d) idle(c);     // the step between the two pairs, which computes nothing
		});
}

// ---------------------------------------------------------------------------------------------
// 0x130 to 0x140: the two line writes, the two wet sums and the dry/wet mix
// ---------------------------------------------------------------------------------------------

// One term of a line write, on the register the host's level word multiplies.
struct LineTerm { int reg; add_t add; delay_t delay; };

// One line's value, built by `nsteps` terms of which the last is the one the delay RAM takes. Every term
// but that one only moves the running value and wraps; the last saturates, which is what both writers
// carry in w3[15:14]. `decor` is the first term's: the cell that takes up the fourth tap's read, which
// the wet sum below then works from.
struct LineBuild { int st, nsteps; LineTerm term[4]; Decors decor; };

// The left line is the slot 0x4c (rom::rev::DELAY_SLOTS[0]) and is written by the step that closes the
// sum; the right line is the slot 0x4e (rom::rev::DELAY_SLOTS[3]) and its closing step only latches,
// so the write two steps on takes that latch rather than its own result.
constexpr LineBuild LINES[2] = {
    { 0x130, 4, { { 89, ADD_OFF, NO_WRITE  },     // rev::DELAY_WORDS[11], the left filtered input
                  { 92,  ADD_ACC, NO_WRITE    },  // rev::DELAY_WORDS[12], the right one
                  { 119, ADD_ACC, NO_WRITE    },  // rev::DELAY_WORDS[7],  the first damped feedback
                  { 120, ADD_ACC, WRITE_OWN } },  // rev::DELAY_WORDS[9],  the second, and the write
      { {DEC_CAPTURE, 59}, {} } },
    { 0x137, 3, { { 92, ADD_OFF, NO_WRITE  },     // rev::DELAY_WORDS[13], the right filtered input
                  { 120, ADD_ACC, NO_WRITE    },  // rev::DELAY_WORDS[8],   the second damped tap
                  { 119, ADD_ACC, SET_LATCH } },  // rev::DELAY_WORDS[10],  the first, latched
      { {}, {} } },
};

// The line of step st, if it is one: a line's terms are consecutive, so each of them indexes to the same
// row with a different k.
constexpr auto LINES_AT = index_by_span(LINES);

void line_build_step(yss236_core& c, int st, LineBuild const& l, int k)
{
	if (!k) decors(c, st, l.decor);
	LineTerm const& t = l.term[k];
	step::alu a;
	a.st = st;
	a.sr = t.reg;
	a.add = t.add;
	a.clamp = k + 1 == l.nsteps ? CLAMP_SAT : CLAMP_WRAP;
	delay_flags(a, t.delay);
	run(c, a);
}

// One wet sum, three steps: the side's own tap at full level opens the sum, a second tap comes on at the
// level the host wrote, and a third closes it into the channel's wet register. The right channel's
// middle step is also the writer of the right line, so the value that line takes is the latch left
// behind and not that step's own sum.
struct WetSum { int st, nsteps, first_tap, mid_tap, out_tap, out_reg; delay_t mid_write; };

// The left channel works off m[56], m[58] and m[59] and the right off m[57], m[59] and m[58], so which
// numbered tap lands in which of the three positions is the host's own: rev::DELAY_WORDS[4] and [6] for
// Delay L,C,R's centre level and Echo's second level, and [5] for the same level on the other side.
constexpr WetSum WET[2] = {
    //   st  n  first   mid   out  out_reg  what the middle step does with the line
    { 0x134, 3, 56, 58, 59, 124, NO_WRITE      },  // m[56], m[58] and m[59]
    { 0x13a, 3, 57, 59, 58, 125, WRITE_LATCHED },  // m[57], m[59] and m[58], and the write
};

// The wet sum of step st, if it is one: a sum's three steps are consecutive, so each of them indexes to
// the same row with a different k.
constexpr auto WET_AT = index_by_span(WET);

void wet_step(yss236_core& c, int st, WetSum const& w, int k)
{
	step::alu a;
	a.st = st;
	a.sm = k == 0 ? w.first_tap : k == 1 ? w.mid_tap : w.out_tap;
	a.m2 = M2_CELL;
	a.add = k ? ADD_ACC : ADD_OFF;
	a.clamp = k == 2 ? CLAMP_SAT : CLAMP_WRAP;
	a.dr = k == 2 ? w.out_reg : 0;
	if (k == 1) delay_flags(a, w.mid_write);
	run(c, a);
}

// One side's dry/wet mix, two steps: the side's filtered input at the dry level, then its wet sum at the
// wet level and four times over, which is what leaves the channel's output in r[13] or r[14]. The
// route is four where the tone filters use two, and the pair wraps where they saturate.
struct Mix { int base, dry, wet, out; };

// EffectHost::dryWet(w, d.word(0), ...) writes the two dry levels into rev::DELAY_WORDS[0] and [1],
// these steps' first words, and the two wet levels into [2] and [3], their second words.
constexpr Mix MIX[2] = {
    { 0x13d, 89, 124, 13 },  // the left output
    { 0x13f, 92, 125, 14 },  // the right output
};

void mix_step(yss236_core& c, int st, Mix const& m)
{
	if (st == m.base) term(c, st, m.dry);
	else run(c, {.st = st, .sr = m.wet, .add = ADD_ACC, .route = ROUTE_4, .dr = m.out});
}

// The run of 17 steps at 0x130. The schedule interleaves the two lines and the two wet sums: a line's
// terms, then the sum that reads the taps, then the other line's terms, then that one, then the two
// mixes. LINES and WET between them take every step from 0x130 to 0x13c, and their ranges do not meet.
void block_0x130(yss236_core& c)
{
	for_steps<0x130, 0x141>(c, [&](yss236_core& c, auto st)
	{
		if (st < 0x13d)
		{
			if (int l = LINES_AT[st]; l >= 0) line_build_step(c, st, LINES[l], st - LINES[l].st);
			if (int w = WET_AT[st]; w >= 0) wet_step(c, st, WET[w], st - WET[w].st);
		}
		else mix_step(c, st, MIX[st < 0x13f ? 0 : 1]);
		});
}

// ---------------------------------------------------------------------------------------------
// 0x141 to 0x157: the empty stretch, three more reads and the block's output
// ---------------------------------------------------------------------------------------------

// The twenty-three steps at the end of the block, one decor pair each. The first six are nothing at
// all, and what is left are three reads of the block's own area (the slots 0x51, 0x52 and 0x53), the
// cells the windows outside this one take those reads up in, and the block's own output to the serial
// out.
struct TailRun { int base; Decors decor[23]; };

constexpr TailRun TAIL = { 0x141, {
    { {},                {}            },  // 0x141
    { {},                {}            },  // 0x142
    { {},                {}            },  // 0x143
    { {},                {}            },  // 0x144
    { {},                {}            },  // 0x145
    { {},                {}            },  // 0x146
    { {DEC_READ, 0},     {DEC_LFO, 10} },  // 0x147  the slot 0x51, and m[10] the LFO 10
    { {},                {}            },  // 0x148
    { {},                {}            },  // 0x149
    { {},                {}            },  // 0x14a
    { {DEC_READ, 0},     {}            },  // 0x14b  the slot 0x52
    { {DEC_CAPTURE, 25}, {}            },  // 0x14c  m[25] takes up the read of 0x147
    { {},                {}            },  // 0x14d
    { {},                {}            },  // 0x14e
    { {DEC_READ, 0},     {}            },  // 0x14f  the slot 0x53
    { {DEC_CAPTURE, 26}, {}            },  // 0x150  m[26] takes up the read of 0x14b
    { {},                {}            },  // 0x151
    { {},                {}            },  // 0x152
    { {},                {}            },  // 0x153
    { {DEC_CAPTURE, 27}, {}            },  // 0x154  m[27] takes up the read of 0x14f
    { {},                {}            },  // 0x155
    { {DEC_OUT, 41},     {}            },  // 0x156  the block's output, m[41] to the serial out
    { {},                {}            },  // 0x157
} };

// The run of 23 steps at 0x141, none of which computes anything but its decor and the z^-1 latch.
void block_0x141(yss236_core& c)
{
	for_steps<TAIL.base, 0x158>(c, [&](yss236_core& c, auto st)
	{
		decors(c, st, TAIL.decor[st - TAIL.base]);
		idle(c);
		});
}

// The five runs of steps the window's 72 steps fall into, in step order. The window hands the whole
// range over in one call, 0x110 to 0x158, and that calls these five in this order, so the program is
// still one step() per step and a skip set in one of them carries into the next as it would across one
// loop.
struct StepRun { int from, to; void (*block)(yss236_core&); };

constexpr StepRun RUNS[] = {
    { 0x110, 0x11e, block_0x110 },  // the two inputs and the four tone filters
    { 0x11e, 0x12a, block_0x11e },  // the four delay taps and the captures for other windows
    { 0x12a, 0x130, block_0x12a },  // the two damped feedbacks
    { 0x130, 0x141, block_0x130 },  // the two line writes, the two wet sums and the dry/wet mix
    { 0x141, 0x158, block_0x141 },  // the empty stretch, three more reads and the output
};

}  // namespace

// One run of the reverb delay window's steps, exactly as yss236_core::step() would run them. The
// program is one run; a range no host builds falls back to the interpreter.
void dly_m08(yss236_core& c, int from, int to)
{
	if (from == 0x110 && to == 0x158)
	{
		for (StepRun const& r : RUNS) r.block(c);
		return;
	}
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
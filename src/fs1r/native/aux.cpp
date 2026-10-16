// fs1r/native/aux.cpp - VOP3-2, the aux window's program: its 40 steps.
//
// rom::AUX_PROGRAM is the only program the window has (WINDOWS[AUX].programs is one entry of 40 steps,
// rom::STEPS_AUX 0x0e8 to 0x10f) and it runs under every performance: the window has no parameters at
// all, since EffectHost::applyParam's AUX case breaks without writing anything (fx_dispatch.cpp), so
// nothing in it moves when a parameter moves. One program, one native, and no type is excluded.
//
// What the 40 steps compute is eight units of four steps and a tail, and the two groups below are the
// whole of it:
//
//   0x0e8-0x107  eight units, each a first-order smoother of one cell. The unit's state is a register,
//                its cell is the multiplier input, and the word on the unit's second step is the pole:
//                those eight steps are rom::STEPS_OUTPUT_GAIN (0xe9, 0xed, 0xf1, 0xf5, 0xf9, 0xfd,
//                0x101, 0x105), which EffectHost::setOutputGain writes 21 into for 0 dB. So one pass
//                moves the state toward the cell by the gain word times the difference the unit's third
//                step left, and the third step leaves the cell minus the state, doubled, in a second
//                register. The base program reads all eight of those differences once each, at 0x046,
//                0x050, 0x056, 0x05f, 0x089, 0x092, 0x0d8 and 0x0e0, and adds them into the running
//                value on the way to the outputs.
//   0x108-0x10f  the tail: two empty steps, then two pairs that clear the running value and write it
//                into the cells m[40] and m[41], which are two of the eight units' cells, and the
//                window's serial output, the cell m[17] to out[8].
//
// The registers, as the tables above name them: r[61] to r[66], r[101] and r[102] the eight states,
// r[31] to r[34] and r[43] to r[46] the eight differences, r[7] and r[8] what the tail's two clear steps
// read, r[17] and r[18] what its two write steps multiply in. Of the eight states, only r[101] and
// r[102] are read outside this window, by the reverb delay window's programs; the other six are read
// here and nowhere else in the program set. Of the eight cells, m[1] to m[6] are written by a step of
// another window, while m[40] and m[41] are written by the base program at 0x082 and 0x0c0 from the
// serial inputs and then overwritten with zero by the tail, three steps before the next pass reads
// them.
//
// One constant the program reads is the gain word of the eight units' second steps, and it is read
// from c.coef[st] on the pass as step() reads it: the window's own preset puts 0x18 there
// (rom::AUX_PRESETS, through EffectHost::resetWindow) and setOutputGain puts 21 there at boot, and
// which word is in the chip when these steps run is the host's business, not this program's. Nothing
// else in the program is a number the firmware chose: the program words are fixed per row, so each
// step's fields are folded into the arithmetic below in step()'s order.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, run and idle

// One unit: the register its state is in, the register its difference is in and the cell it smooths.
// In step order, four steps each, the second step of each being one of rom::STEPS_OUTPUT_GAIN.
struct Unit { int state, diff, cell; };
const Unit UNITS[8] = {
    { 61, 31,  1}, { 62, 43,  6}, { 63, 32,  2}, { 64, 44,  5},
    { 65, 33,  3}, { 66, 45, 40}, {101, 34,  4}, {102, 46, 41},
};

// The unit's first step: the running value takes the state, and nothing else. The adder is off, so the
// step only loads the accumulator, and dr is 0, so no register takes the result.
void take_state(yss236_core& c, Unit const& u, int st)
{
	// class 3 is unity: the word is ignored and the product is the register itself
	run(c, {st, u.state, 0, M2_REG, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP});
}

// The unit's second step: the state moves toward the cell by the gain word times the difference, and
// the running value takes the result, which is what the next step's accumulator holds.
void smooth_state(yss236_core& c, Unit const& u, int st)
{
	close(c, st, u.diff, u.state);
}

// The unit's third step: the cell minus the state, doubled, into the difference register. The step
// before wrote the state, but a register write lands three steps later, so the state it reads here is
// the one this pass's second step will replace, and the difference is this pass's move.
void cell_difference(yss236_core& c, Unit const& u, int st)
{
	// class 3 is unity again, and this time the multiplier's second input is the cell while the adder
	// reads the register the step just fetched, negated
	run(c, {st, u.state, u.cell, M2_CELL, true, 0, ADD_REG, true, ROUTE_2, CLAMP_SAT, u.diff});
}

// The unit's fourth step, and the two empty steps and the last step of the tail: a class-0 step with no
// register read still takes the latch, so the running x goes to zero and hasx drops.
void empty_step(yss236_core& c)
{
	idle(c);
}

// The tail's clear step: the running value takes the gain word times a register, with the adder off, so
// what the following write step puts in its cell is the running value this step leaves.
void clear_acc(yss236_core& c, int reg, int st)
{
	term(c, st, reg);
}

// The tail's write step: the running value, scaled by RSCALE[3], into the cell (msrc 6, the step's own
// result). dr is 0, so the register it reads only feeds the multiplier.
void write_cell(yss236_core& c, int reg, int cell, int st)
{
	run(c, {st, reg, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_16, CLAMP_SAT, 0, false, cell});
}

// The run of steps 0x0e8 to 0x110, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's.
void block_0x0e8(yss236_core& c)
{
	for (int st = 0x0e8; st < 0x110; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		int const i = st - 0x0e8;
		if (i < 32)
		{
			// ---- the eight units, four steps each ----
			Unit const& u = UNITS[i / 4];
			switch (i % 4)
			{
			case 0:  take_state(c, u, st); break;
			case 1:  smooth_state(c, u, st); break;
			case 2:  cell_difference(c, u, st); break;
			default: empty_step(c); break;
			}
			continue;
		}
		// ---- the tail ----
		switch (st)
		{
		case 0x108: case 0x109: case 0x10f: empty_step(c); break;
		case 0x10a: clear_acc(c, 7, st); break;         // clears the running value, reading r[7]
		case 0x10b: write_cell(c, 17, 40, st); break;  // and writes it into m[40], unit 5's cell
		case 0x10c: clear_acc(c, 8, st); break;
		case 0x10d: write_cell(c, 18, 41, st); break;  // and into m[41], unit 7's cell
		case 0x10e: serial_out(c, st, 17); empty_step(c); break;   // the window's serial output
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
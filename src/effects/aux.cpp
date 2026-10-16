// effects/aux.cpp - the aux window's program.
//
// Eight units of four steps and a tail:
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
// The registers, as UNITS names them: r[61] to r[66], r[101] and r[102] the eight states,
// r[31] to r[34] and r[43] to r[46] the eight differences, r[7] and r[8] what the tail's two clear steps
// read, r[17] and r[18] what its two write steps multiply in. Of the eight states, only r[101] and
// r[102] are read outside this window, by the reverb delay window's programs; the other six are read
// here and nowhere else in the program set. Of the eight cells, m[1] to m[6] are written by a step of
// another window, while m[40] and m[41] are written by the base program at 0x082 and 0x0c0 from the
// serial inputs and then overwritten with zero by the tail, three steps before the next pass reads
// them.

#include "effects/programs.h"

namespace fs1r::effects {

namespace {

using word::cell;
using word::sat;
using word::sum;

// One smoother: its state register, its difference register, the cell it follows, and the step of its gain
// word (rom::STEPS_OUTPUT_GAIN).
struct Unit { int state, diff, cell, gain; };
constexpr Unit UNITS[8] = {
	{ 61, 31,  1, 0x0e9}, { 62, 43,  6, 0x0ed}, { 63, 32,  2, 0x0f1}, { 64, 44,  5, 0x0f5},
	{ 65, 33,  3, 0x0f9}, { 66, 45, 40, 0x0fd}, {101, 34,  4, 0x101}, {102, 46, 41, 0x105},
};

}  // namespace

void aux(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;

	s.out[8] = m[17];   // the window's serial output

	// Each unit moves its state toward its cell by the gain times last sample's difference, which is what
	// makes it a one-pole smoother a sample behind.
	for (Unit const& u : UNITS) {
		double const step = r[u.diff];
		r[u.diff] = sat((m[u.cell] - r[u.state]) * 2.0);
		r[u.state] = sat(k[u.gain] * step + sum(r[u.state]));
	}

	// The tail: two of the units' cells refilled from the sends, after the units have read them
	m[40] = cell(sat((k[0x10b] * r[17] + sum(k[0x10a] * r[7])) * 16.0));
	m[41] = cell(sat((k[0x10d] * r[18] + sum(k[0x10c] * r[8])) * 16.0));
}

}  // namespace fs1r::effects

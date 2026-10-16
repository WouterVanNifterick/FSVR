// effects/aux_window.cpp - the aux window: eight one-pole smoothers on the send levels, which every performance
// runs. The base program reads their differences back.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

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

}  // namespace fsvr::effects

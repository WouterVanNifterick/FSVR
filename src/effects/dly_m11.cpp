// effects/dly_m11.cpp - the reverb delay window's clear program, which the host loads to silence the window
// (EffectHost::clearWindow): it moves the window's inputs into their cells and computes nothing.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void dly_m11(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x44)];
	double const d1 = s.ram[s.at(0x45)];
	double const d2 = s.ram[s.at(0x46)];
	double const d3 = s.ram[s.at(0x51)];
	double const d4 = s.ram[s.at(0x52)];
	double const d5 = s.ram[s.at(0x53)];
	r[87] = wrap(0.0);
	r[88] = wrap(0.0);
	r[89] = wrap(0.0);
	r[90] = wrap(0.0);
	r[91] = wrap(0.0);
	r[92] = wrap(0.0);
	r[93] = wrap(0.0);
	r[94] = wrap(0.0);
	r[95] = wrap(0.0);
	r[96] = wrap(0.0);
	r[97] = wrap(0.0);
	r[98] = wrap(0.0);
	r[99] = wrap(0.0);
	r[100] = wrap(0.0);
	r[101] = wrap(0.0);
	r[102] = wrap(0.0);
	m[1] = in[0][8];
	m[5] = in[1][8];
	m[8] = lfo[8];
	m[9] = lfo[9];
	m[10] = lfo[10];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d3;
	m[26] = d4;
	m[27] = d5;
}

}  // namespace fs1r::effects

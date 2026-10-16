// effects/var_m11.cpp - the variation window's clear program, which the host loads to silence the window
// (EffectHost::clearWindow): it passes the window's inputs on and computes nothing.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m11(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const d3 = s.ram[s.at(0x63)];
	double const d4 = s.ram[s.at(0x64)];
	double const d5 = s.ram[s.at(0x65)];
	r[103] = wrap(0.0);
	r[104] = wrap(0.0);
	r[105] = wrap(0.0);
	r[106] = wrap(0.0);
	r[107] = wrap(0.0);
	r[108] = wrap(0.0);
	r[109] = wrap(0.0);
	r[110] = wrap(0.0);
	r[111] = wrap(0.0);
	r[112] = wrap(0.0);
	r[113] = wrap(0.0);
	r[114] = wrap(0.0);
	r[115] = wrap(0.0);
	r[116] = wrap(0.0);
	r[117] = wrap(0.0);
	r[118] = wrap(0.0);
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d3;
	m[32] = d4;
	m[33] = d5;
}

}  // namespace fs1r::effects

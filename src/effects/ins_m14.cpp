// effects/ins_m14.cpp - the insertion window's clear program, which the host loads to silence the window
// (EffectHost::clearWindow): it passes the window's inputs on and computes nothing.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m14(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	r[67] = wrap(0.0);
	r[68] = wrap(0.0);
	r[69] = wrap(0.0);
	r[70] = wrap(0.0);
	r[71] = wrap(0.0);
	r[72] = wrap(0.0);
	r[73] = wrap(0.0);
	r[74] = wrap(0.0);
	r[75] = wrap(0.0);
	r[76] = wrap(0.0);
	r[77] = wrap(0.0);
	r[78] = wrap(0.0);
	r[79] = wrap(0.0);
	r[80] = wrap(0.0);
	r[81] = wrap(0.0);
	r[82] = wrap(0.0);
	r[83] = wrap(0.0);
	r[84] = wrap(0.0);
	r[85] = wrap(0.0);
	r[86] = wrap(0.0);
	m[3] = in[0][13];
	m[4] = in[0][15];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[14] = lfo[14];
	m[15] = lfo[15];
}

}  // namespace fsvr::effects

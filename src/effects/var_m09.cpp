// effects/var_m09.cpp - the variation window under Karaoke.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m09(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const d3 = s.ram[s.at(0x59)];
	double const d4 = s.ram[s.at(0x5a)];
	double const d5 = s.ram[s.at(0x5b)];
	double const v0 = sat(((k[0x159] * r[6]) + sum(k[0x158] * r[5])) * 2.0);
	double const v1 = sat(((k[0x15c] * v0) + sum(k[0x15a] * r[104], k[0x15b] * r[103])) * 2.0);
	double const v2 = sat(((k[0x15f] * v1) + sum(k[0x15d] * r[105], k[0x15e] * r[104])) * 2.0);
	s.ram[s.at(0x5c)] = clip32(sat((k[0x171] * d3) + sum(k[0x170] * v2)));
	double const v3 = sat(((k[0x16f] * d3) + sum(k[0x16e] * v2)) * 2.0);
	s.ram[s.at(0x5d)] = clip32(sat((k[0x177] * d4) + sum(k[0x176] * v3)));
	double const d6 = s.ram[s.at(0x5e)];
	double const d7 = s.ram[s.at(0x5f)];
	double const v4 = cell(sat((k[0x17a] * m[55]) + sum(k[0x178] * m[54], k[0x179] * d5)));
	s.ram[s.at(0x60)] = clip32(sat((k[0x17d] * sat(((k[0x175] * d4) + sum(k[0x174] * v3)) * 2.0)) + v4));
	double const v5 = sat(((k[0x17b] * d5) + 0.0) * 2.0);
	s.ram[s.at(0x61)] = clip32(sat((k[0x187] * d6) + sum(k[0x186] * v5)));
	double const v6 = sat(((k[0x17c] * d5) + 0.0) * 2.0);
	s.ram[s.at(0x62)] = clip32(sat((k[0x18b] * d6) + sum(k[0x18a] * v6)));
	double const d8 = s.ram[s.at(0x63)];
	double const d9 = s.ram[s.at(0x64)];
	double const d10 = s.ram[s.at(0x65)];
	double const v7 = sat(((k[0x185] * d6) + sum(k[0x184] * v5)) * 2.0);
	double const v8 = sat(((k[0x189] * d7) + sum(k[0x188] * v6)) * 2.0);
	r[15] = sat(((k[0x18d] * v7) + sum(k[0x18c] * r[5])) * 4.0);
	r[16] = sat(((k[0x18f] * v8) + sum(k[0x18e] * r[6])) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[120] = v7;
	r[121] = v8;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d8;
	m[32] = d9;
	m[33] = d10;
	m[54] = d5;
	m[55] = v4;
	m[60] = d6;
	m[61] = d7;
}

}  // namespace fs1r::effects

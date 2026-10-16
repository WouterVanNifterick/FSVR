// effects/var_m08.cpp - the variation window under Delay LCR, Delay L,R, Echo and CrossDelay.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void var_m08(State& s)
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
	double const d6 = s.ram[s.at(0x5c)];
	double const v0 = sat(((k[0x158] * r[5]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x15b] * v0) + sum(k[0x159] * r[104], k[0x15a] * r[103])) * 2.0);
	double const v2 = sat(((k[0x15e] * v1) + sum(k[0x15c] * r[105], k[0x15d] * r[104])) * 4.0);
	double const v3 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v4 = sat(((k[0x162] * v3) + sum(k[0x160] * r[107], k[0x161] * r[106])) * 2.0);
	double const v5 = sat(((k[0x165] * v4) + sum(k[0x163] * r[108], k[0x164] * r[107])) * 4.0);
	double const v6 = wrap(d3 + 0.0);
	double const v7 = sat((k[0x173] * v6) + sum(k[0x172] * r[109]));
	double const v8 = wrap(d4 + 0.0);
	double const v9 = sat((k[0x177] * v8) + sum(k[0x176] * r[110]));
	s.ram[s.at(0x5e)] = clip32(sat((k[0x17b] * v9) + sum(k[0x178] * v2, k[0x179] * v5, k[0x17a] * v7)));
	s.ram[s.at(0x60)] = clip32(sat((k[0x181] * v7) + sum(k[0x17f] * v5, k[0x180] * v9)));
	double const d7 = s.ram[s.at(0x63)];
	double const d8 = s.ram[s.at(0x64)];
	double const d9 = s.ram[s.at(0x65)];
	double const v10 = sat((k[0x17e] * d6) + sum(k[0x17c] * d3, k[0x17d] * d5));
	double const v11 = sat((k[0x184] * d5) + sum(k[0x182] * d4, k[0x183] * d6));
	r[15] = wrap(((k[0x186] * v10) + sum(k[0x185] * v2)) * 4.0);
	r[16] = wrap(((k[0x188] * v11) + sum(k[0x187] * v5)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v3;
	r[107] = v4;
	r[108] = v5;
	r[109] = v6;
	r[110] = v8;
	r[119] = v7;
	r[120] = v9;
	r[124] = v10;
	r[125] = v11;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[10];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d7;
	m[32] = d8;
	m[33] = d9;
	m[56] = d3;
	m[57] = d4;
	m[58] = d5;
	m[59] = d6;
}

}  // namespace fsvr::effects

// effects/dly_m08.cpp - the reverb delay window under Delay LCR, Delay L,R, Echo and CrossDelay.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void dly_m08(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& out = s.out;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x44)];
	double const d1 = s.ram[s.at(0x45)];
	double const d2 = s.ram[s.at(0x46)];
	double const d3 = s.ram[s.at(0x47)];
	double const d4 = s.ram[s.at(0x48)];
	double const d5 = s.ram[s.at(0x49)];
	double const d6 = s.ram[s.at(0x4a)];
	double const v0 = sat(((k[0x110] * r[3]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x113] * v0) + sum(k[0x111] * r[88], k[0x112] * r[87])) * 2.0);
	double const v2 = sat(((k[0x116] * v1) + sum(k[0x114] * r[89], k[0x115] * r[88])) * 4.0);
	double const v3 = sat(((k[0x117] * r[4]) + 0.0) * 2.0);
	double const v4 = sat(((k[0x11a] * v3) + sum(k[0x118] * r[91], k[0x119] * r[90])) * 2.0);
	double const v5 = sat(((k[0x11d] * v4) + sum(k[0x11b] * r[92], k[0x11c] * r[91])) * 4.0);
	double const v6 = wrap(d3 + 0.0);
	double const v7 = sat((k[0x12b] * v6) + sum(k[0x12a] * r[93]));
	double const v8 = wrap(d4 + 0.0);
	double const v9 = sat((k[0x12f] * v8) + sum(k[0x12e] * r[94]));
	s.ram[s.at(0x4c)] = clip32(sat((k[0x133] * v9) + sum(k[0x130] * v2, k[0x131] * v5, k[0x132] * v7)));
	s.ram[s.at(0x4e)] = clip32(sat((k[0x139] * v7) + sum(k[0x137] * v5, k[0x138] * v9)));
	double const d7 = s.ram[s.at(0x51)];
	double const d8 = s.ram[s.at(0x52)];
	double const d9 = s.ram[s.at(0x53)];
	double const v10 = sat((k[0x136] * d6) + sum(k[0x134] * d3, k[0x135] * d5));
	double const v11 = sat((k[0x13c] * d5) + sum(k[0x13a] * d4, k[0x13b] * d6));
	r[13] = wrap(((k[0x13e] * v10) + sum(k[0x13d] * v2)) * 4.0);
	r[14] = wrap(((k[0x140] * v11) + sum(k[0x13f] * v5)) * 4.0);
	r[87] = v0;
	r[88] = v1;
	r[89] = v2;
	r[90] = v3;
	r[91] = v4;
	r[92] = v5;
	r[93] = v6;
	r[94] = v8;
	r[119] = v7;
	r[120] = v9;
	r[124] = v10;
	r[125] = v11;
	m[1] = in[0][8];
	m[5] = in[1][8];
	m[8] = lfo[8];
	m[9] = lfo[9];
	m[10] = lfo[10];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d7;
	m[26] = d8;
	m[27] = d9;
	m[56] = d3;
	m[57] = d4;
	m[58] = d5;
	m[59] = d6;
	out[10] = m[41];
}

}  // namespace fs1r::effects

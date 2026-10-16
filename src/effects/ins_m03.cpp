// effects/ins_m03.cpp - the insertion window under ER 1, ER 2, Gate Rev and Revrs Gate.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m03(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;
	auto const& in = s.in;
	double const d0 = s.ram[s.at(0x68)];
	double const d1 = s.ram[s.at(0x69)];
	double const d2 = s.ram[s.at(0x6a)];
	double const d3 = s.ram[s.at(0x6b)];
	double const d4 = s.ram[s.at(0x6c)];
	double const v0 = sat(((k[0x1a1] * r[8]) + sum(k[0x1a0] * r[7])) * 2.0);
	double const v1 = sat(((k[0x1a6] * v0) + sum(k[0x1a2] * r[68], k[0x1a3] * r[67], k[0x1a4] * r[69], k[0x1a5] * r[70])) * 2.0);
	double const v2 = sat(((k[0x1ab] * v1) + sum(k[0x1a7] * r[70], k[0x1a8] * r[69], k[0x1a9] * r[71], k[0x1aa] * r[72])) * 2.0);
	double const v3 = sat(((k[0x1b0] * v2) + sum(k[0x1ac] * r[72], k[0x1ad] * r[71], k[0x1ae] * r[73], k[0x1af] * r[74])) * 4.0);
	s.ram[s.at(0x6d)] = clip32(sat((k[0x1b7] * m[18]) + sum(k[0x1b6] * v3)));
	double const v4 = sat(((k[0x1b5] * m[18]) + sum(k[0x1b4] * v3)) * 2.0);
	s.ram[s.at(0x6e)] = clip32(sat((k[0x1bb] * m[19]) + sum(k[0x1ba] * v4)));
	double const d5 = s.ram[s.at(0x6f)];
	double const d6 = s.ram[s.at(0x70)];
	double const v5 = sat((k[0x1c5] * m[31]) + sum(k[0x1bc] * m[22], k[0x1bd] * m[23], k[0x1be] * m[24], k[0x1bf] * m[25], k[0x1c0] * m[26], k[0x1c1] * m[27], k[0x1c2] * m[28], k[0x1c3] * m[29], k[0x1c4] * m[30]));
	s.ram[s.at(0x71)] = clip32(v5);
	double const d7 = s.ram[s.at(0x72)];
	double const d8 = s.ram[s.at(0x73)];
	double const d9 = s.ram[s.at(0x74)];
	double const d10 = s.ram[s.at(0x75)];
	double const v6 = sat((k[0x1db] * d9) + sum(k[0x1d2] * d0, k[0x1d3] * d1, k[0x1d4] * d2, k[0x1d5] * d3, k[0x1d6] * d4, k[0x1d7] * d5, k[0x1d8] * d6, k[0x1d9] * d7, k[0x1da] * d8));
	s.ram[s.at(0x76)] = clip32(v6);
	double const d11 = s.ram[s.at(0x77)];
	double const d12 = s.ram[s.at(0x78)];
	double const v7 = cell(v6);
	double const v8 = sat((k[0x1df] * d10) + sum(k[0x1de] * v7));
	s.ram[s.at(0x79)] = clip32(sat((k[0x1e7] * d11) + sum(k[0x1e6] * v8)));
	double const v9 = cell(v5);
	double const v10 = sat((k[0x1dd] * m[33]) + sum(k[0x1dc] * v9));
	s.ram[s.at(0x7a)] = clip32(sat((k[0x1eb] * d12) + sum(k[0x1ea] * v10)));
	double const v11 = sat(((k[0x1b9] * m[19]) + sum(k[0x1b8] * v4)) * 2.0);
	double const v12 = sat((k[0x1b3] * r[75]) + sum(k[0x1b1] * m[32], k[0x1b2] * m[42]));
	s.ram[s.at(0x7b)] = clip32(sat((k[0x1ef] * v12) + sum(k[0x1ee] * v11)));
	s.ram[s.at(0x7c)] = clip32(sat((k[0x1f3] * m[20]) + 0.0));
	s.ram[s.at(0x7d)] = clip32(sat((k[0x1f7] * m[21]) + 0.0));
	double const v13 = sat(((k[0x1ed] * d12) + sum(k[0x1ec] * v10)) * 2.0);
	double const v14 = sat(((k[0x1e9] * d11) + sum(k[0x1e8] * v8)) * 2.0);
	r[17] = sat(((k[0x1f2] * v13) + sum(k[0x1f1] * r[7])) * 4.0);
	r[18] = sat(((k[0x1f6] * v14) + sum(k[0x1f5] * r[8])) * 4.0);
	r[68] = r[67];
	r[70] = r[69];
	r[72] = r[71];
	r[74] = r[73];
	r[75] = v12;
	r[119] = v11;
	r[120] = v14;
	r[121] = v13;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[42] = m[32];
	m[43] = v9;
	m[44] = v7;
	m[56] = d10;
	m[57] = d11;
	m[58] = d12;
	m[59] = d3;
	m[60] = d4;
	m[61] = d5;
	m[62] = d6;
	m[63] = d7;
	r[67] = v0;
	r[69] = v1;
	r[71] = v2;
	r[73] = v3;
}

}  // namespace fsvr::effects

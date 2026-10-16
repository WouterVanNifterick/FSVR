// effects/var_m04.cpp - the variation window under Auto Wah and Touch Wah.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void var_m04(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const d3 = s.ram[s.at(0x63)];
	double const d4 = s.ram[s.at(0x64)];
	double const d5 = s.ram[s.at(0x65)];
	double const v0 = sat(((k[0x158] * r[5]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x15b] * v0) + sum(k[0x159] * r[103], k[0x15a] * r[104])) * 2.0);
	double const v2 = sat(((k[0x15e] * v1) + sum(k[0x15c] * r[104], k[0x15d] * r[105])) * 4.0);
	double const v3 = cell(sat((k[0x170] * r[114]) - wrap_sum((k[0x16f] * m[54]) - v2)));
	double const v4 = sat((k[0x168] * r[113]) + sum(128.0 * k[0x166], k[0x167] * m[8]));
	double const v5 = (q15(v4) * v4) + 0.0;
	double const v6 = wrap(v5);
	double const v7 = q15(sat((128.0 * k[0x173]) + sum(q15(sum(q15(v4) * v4)) * v6)));
	double const v8 = sat((v7 * v3) + r[114]);
	double const v9 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v10 = sat(((k[0x162] * v9) + sum(k[0x160] * r[106], k[0x161] * r[107])) * 2.0);
	double const v11 = sat(((k[0x165] * v10) + sum(k[0x163] * r[107], k[0x164] * r[108])) * 4.0);
	double const v12 = cell(sat((k[0x175] * r[115]) - wrap_sum((k[0x174] * m[55]) - v11)));
	double const v13 = sat((v7 * v12) + r[115]);
	double const v14 = sat((k[0x16a] * v11) + sum(k[0x169] * v2));
	double const v15 = sat(((k[0x16d] * v14) + sum(k[0x16b] * r[109], k[0x16c] * r[110])) * 2.0);
	double const v16 = cell(mag(((k[0x171] * v15) + 0.0) * 16.0));
	double const v17 = sat((k[0x177] * v16) + pos((k[0x176] * r[111]) - v16));
	double const v18 = sat((k[0x17e] * v16) + pos((k[0x17d] * v17) - wrap_sum((k[0x17c] * v16) - sum_on(sum(k[0x17a] * r[112]) / 32768.0, k[0x17b] * r[112]))));
	r[15] = sat(((k[0x185] * v2) + sum(k[0x184] * v8)) * 4.0);
	r[16] = sat(((k[0x187] * v11) + sum(k[0x186] * v13)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v9;
	r[107] = v10;
	r[108] = v11;
	r[110] = sat(((k[0x16d] * v14) + sum(k[0x16b] * r[109], k[0x16c] * r[110])) * 2.0);
	r[111] = sat((k[0x177] * v16) + pos((k[0x176] * r[111]) - v16));
	r[113] = sat((k[0x181] * v18) + sum(k[0x17f] * r[112], k[0x180] * r[113]));
	r[114] = v8;
	r[115] = v13;
	r[119] = v6;
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
	m[54] = cell(sat((v7 * v8) + m[54]));
	m[55] = cell(sat((v7 * v13) + m[55]));
	m[56] = v3;
	m[57] = v12;
	m[58] = v16;
	t[9] = v7;
	r[109] = sat((k[0x16a] * v11) + sum(k[0x169] * v2));
	r[112] = sat((k[0x17e] * v16) + pos((k[0x17d] * v17) - wrap_sum((k[0x17c] * v16) - sum_on(sum(k[0x17a] * r[112]) / 32768.0, k[0x17b] * r[112]))));
}

}  // namespace fsvr::effects

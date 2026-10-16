// effects/var_m05.cpp - the variation window under Phaser1 and Phaser2.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m05(State& s)
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
	double const v3 = sat(((k[0x16a] * r[112]) + sum(k[0x167] * m[55], k[0x168] * r[110], k[0x169] * r[111])) * 2.0);
	double const v4 = sat(((k[0x16e] * r[118]) + sum(k[0x16b] * r[115], k[0x16c] * r[116], k[0x16d] * r[117])) * 2.0);
	double const v5 = sat(((k[0x173] * r[117]) + sum(k[0x170] * v3, k[0x171] * v4, k[0x172] * r[114])) * 2.0);
	double const v6 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v7 = sat(((k[0x162] * v6) + sum(k[0x160] * r[106], k[0x161] * r[107])) * 2.0);
	double const v8 = sat(((k[0x165] * v7) + sum(k[0x163] * r[107], k[0x164] * r[108])) * 4.0);
	double const v9 = sat((128.0 * k[0x166]) + m[8]);
	double const v10 = sat((q15(v9) * v9) + 0.0);
	double const v11 = q15(sat((q15(v10) * v10) - sum(128.0 * k[0x174])));
	double const v12 = cell(sat((k[0x179] * v4) + sum_on(wrap_sum(((k[0x177] * v8) + sum(k[0x176] * v2)) * 2.0), k[0x178] * v3)));
	double const v13 = sat((v11 * v12) - wrap_sum((v11 * r[109]) - m[54]));
	double const v14 = cell(sat((v11 * v13) - wrap_sum((v11 * m[55]) - r[109])));
	double const v15 = sat((v11 * v14) - wrap_sum((v11 * r[110]) - m[55]));
	double const v16 = sat((v11 * v15) - wrap_sum((v11 * r[111]) - sum(r[110])));
	double const v17 = cell(sat((128.0 * k[0x180]) + 0.0));
	double const v18 = sat((128.0 * k[0x17a]) + m[9]);
	double const v19 = sat((q15(v18) * v18) + 0.0);
	double const v20 = q15(sat((q15(v19) * v19) - v17));
	double const v21 = sat((k[0x18c] * v3) + sum_on(wrap_sum(((k[0x18a] * v8) + 0.0) * 2.0), k[0x18b] * v4));
	double const v22 = sat((v20 * v21) - wrap_sum((v20 * r[114]) - sum(r[113])));
	double const v23 = sat((v20 * v22) - wrap_sum((v20 * r[115]) - sum(r[114])));
	double const v24 = sat((v20 * v23) - wrap_sum((v20 * r[116]) - sum(r[115])));
	double const v25 = sat((v20 * v24) - wrap_sum((v20 * r[117]) - sum(r[116])));
	r[15] = sat(((k[0x19d] * v5) + sum(k[0x19c] * v2)) * 4.0);
	r[16] = sat(((k[0x19f] * v4) + sum(k[0x19e] * v8)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v6;
	r[107] = v7;
	r[108] = v8;
	r[109] = sat((v11 * v12) - wrap_sum((v11 * r[109]) - m[54]));
	r[112] = sat((v11 * v16) - wrap_sum((v11 * r[112]) - sum(r[111])));
	r[118] = sat((v20 * v25) - wrap_sum((v20 * r[118]) - sum(r[117])));
	r[119] = v5;
	r[120] = v3;
	r[121] = v4;
	r[122] = v19;
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
	m[54] = v12;
	m[56] = v17;
	t[10] = v11;
	t[11] = v20;
	r[111] = sat((v11 * v15) - wrap_sum((v11 * r[111]) - sum(r[110])));
	r[117] = sat((v20 * v24) - wrap_sum((v20 * r[117]) - sum(r[116])));
	r[110] = sat((v11 * v14) - wrap_sum((v11 * r[110]) - m[55]));
	r[116] = sat((v20 * v23) - wrap_sum((v20 * r[116]) - sum(r[115])));
	m[55] = v14;
	r[115] = sat((v20 * v22) - wrap_sum((v20 * r[115]) - sum(r[114])));
	r[114] = sat((v20 * v21) - wrap_sum((v20 * r[114]) - sum(r[113])));
	r[113] = sat((k[0x18c] * v3) + sum_on(wrap_sum(((k[0x18a] * v8) + 0.0) * 2.0), k[0x18b] * v4));
}

}  // namespace fs1r::effects

// effects/var_m03.cpp - the variation window under HM Enhncer, Noise Gate and Compressor.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void var_m03(State& s)
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
	double const v0 = sat(((k[0x159] * r[6]) + sum(k[0x158] * r[5])) * 2.0);
	double const v1 = sat(((k[0x15e] * v0) + sum(k[0x15a] * r[103], k[0x15b] * r[104], k[0x15c] * r[105], k[0x15d] * r[106])) * 2.0);
	double const v2 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v3 = sat(((k[0x164] * v2) + sum(k[0x160] * r[107], k[0x161] * r[108], k[0x162] * r[109], k[0x163] * r[110])) * 2.0);
	double const v4 = sat(((k[0x166] * v1) + 0.0) * 2.0);
	double const v5 = sat((k[0x165] * m[55]) + 0.0);
	double const v6 = pos((128.0 * k[0x16e]) + v5);
	double const v7 = pos((128.0 * k[0x16f]) + v5);
	double const v8 = pos((128.0 * k[0x16d]) + v5);
	double const v9 = pos((128.0 * k[0x178]) + sum_on(wrap_sum(((k[0x175] * v8) + wrap_sum(((k[0x174] * pos((128.0 * k[0x16c]) + v5)) + wrap_sum(((k[0x173] * pos((128.0 * k[0x169]) + v5)) + wrap_sum(((k[0x172] * pos((128.0 * k[0x16b]) + v5)) + wrap_sum(((k[0x171] * pos((128.0 * k[0x16a]) + v5)) + wrap_sum(((k[0x170] * pos((128.0 * k[0x168]) + v5)) + 0.0) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 4.0), k[0x176] * v6, k[0x177] * v7));
	double const v10 = sat((k[0x17b] * v9) + sum(k[0x179] * r[115], k[0x17a] * r[116]));
	double const v11 = q15(v10);
	double const v12 = sat((v11 * v4) + 0.0);
	double const v13 = q15(v12);
	double const v14 = sat((v13 * v4) + 0.0);
	double const v15 = sat(((k[0x18b] * v14) + sum(k[0x189] * v4, k[0x18a] * v12)) * 16.0);
	double const v16 = sat(((k[0x18e] * v15) + sum(k[0x18c] * r[111], k[0x18d] * r[112])) * 2.0);
	double const v17 = sat(((k[0x167] * v3) + 0.0) * 2.0);
	double const v18 = sat((v11 * v17) + 0.0);
	double const v19 = q15(v18);
	double const v20 = sat((v19 * v17) + 0.0);
	double const v21 = sat(((k[0x191] * v20) + sum(k[0x18f] * v17, k[0x190] * v18)) * 16.0);
	double const v22 = sat(((k[0x194] * v21) + sum(k[0x192] * r[113], k[0x193] * r[114])) * 2.0);
	double const v23 = mag(((k[0x17d] * v17) + sum(k[0x17c] * v4)) * 16.0);
	double const v24 = cell(sat(v23 + cell(pos((k[0x17e] * m[54]) - v23))));
	r[1] = sat(((k[0x166] * v1) + 0.0) * 2.0);
	r[2] = sat(((k[0x167] * v3) + 0.0) * 2.0);
	r[15] = sat(((k[0x196] * v16) + sum(k[0x195] * r[5])) * 4.0);
	r[16] = sat(((k[0x199] * v22) + sum(k[0x197] * r[6], k[0x198] * v16)) * 4.0);
	r[104] = r[103];
	r[106] = r[105];
	r[108] = r[107];
	r[110] = r[109];
	r[111] = v15;
	r[112] = v16;
	r[113] = v21;
	r[114] = v22;
	r[115] = v9;
	r[116] = v10;
	r[119] = mag(((k[0x17d] * v17) + sum(k[0x17c] * v4)) * 16.0);
	r[120] = v12;
	r[121] = v18;
	r[122] = v14;
	r[123] = v20;
	r[124] = v8;
	r[125] = v6;
	r[126] = v7;
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
	m[54] = cell(sat(v23 + cell(pos((k[0x17e] * m[54]) - v23))));
	m[55] = cell(sat(v23 + pos((k[0x185] * v24) - wrap_sum(v23 - sum_on(sum(k[0x182] * m[55]) / 32768.0, k[0x183] * m[55])))));
	t[9] = v11;
	t[10] = v13;
	t[11] = v19;
	r[103] = v0;
	r[105] = v1;
	r[107] = v2;
	r[109] = v3;
}

}  // namespace fsvr::effects

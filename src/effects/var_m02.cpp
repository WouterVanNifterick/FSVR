// effects/var_m02.cpp - the variation window under Distortion, Overdrive and Amp Sim.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void var_m02(State& s)
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
	double const v0 = sat(((k[0x15f] * sat(((k[0x15b] * sat(((k[0x158] * r[108]) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v1 = sat(((k[0x162] * v0) + 0.0) * 16.0);
	double const v2 = sat(((k[0x166] * v1) + 0.0) * 16.0);
	double const v3 = sat(((k[0x16c] * v2) + 0.0) * 16.0);
	double const v4 = sat(((k[0x170] * v3) + 0.0) * 16.0);
	double const v5 = sat((k[0x179] * sat(((k[0x173] * v4) + 0.0) * 16.0)) + sum(k[0x174] * v0, k[0x175] * v1, k[0x176] * v2, k[0x177] * v3, k[0x178] * v4));
	double const v6 = sat((k[0x180] * v5) + 0.0);
	double const v7 = sat(((k[0x185] * v6) + sum(k[0x181] * r[110], k[0x182] * r[109], k[0x183] * r[112], k[0x184] * r[111])) * 2.0);
	double const v8 = sat(((k[0x18a] * v7) + sum(k[0x186] * r[112], k[0x187] * r[111], k[0x188] * r[114], k[0x189] * r[113])) * 2.0);
	double const v9 = sat(((k[0x18f] * v8) + sum(k[0x18b] * r[114], k[0x18c] * r[113], k[0x18d] * r[115], k[0x18e] * r[116])) * 16.0);
	double const v10 = sat(((k[0x15a] * r[6]) + sum(k[0x159] * r[5])) * 2.0);
	double const v11 = sat(((k[0x15e] * v10) + sum(k[0x15c] * r[103], k[0x15d] * r[104])) * 2.0);
	double const v12 = cell(mag(((k[0x161] * v11) + 0.0) * 16.0));
	double const v13 = sat((k[0x165] * v12) + pos((k[0x164] * r[105]) - v12));
	double const v14 = sat((k[0x16b] * v12) + pos((k[0x16a] * v13) - wrap_sum((k[0x169] * v12) - sum_on(sum(k[0x167] * r[106]) / 32768.0, k[0x168] * r[106]))));
	double const v15 = sat((k[0x16e] * v14) + 0.0);
	double const v16 = sat((128.0 * k[0x16f]) + v15);
	double const v17 = sat((128.0 * k[0x171]) + v15);
	double const v18 = pos(((128.0 * k[0x17c]) + sat(((k[0x17b] * v17) + sum(k[0x17a] * v16)) * 16.0)) * 16.0);
	double const v19 = sat((k[0x17f] * v18) + sum(k[0x17d] * r[117], k[0x17e] * r[118]));
	double const v20 = q15(sat(((k[0x190] * v19) + 0.0) * 2.0));
	double const v21 = sat((v20 * v11) + 0.0);
	r[15] = sat(((k[0x192] * v9) + sum(k[0x191] * m[54])) * 4.0);
	r[16] = sat(((k[0x194] * v9) + sum(k[0x193] * m[55])) * 4.0);
	r[104] = sat(((k[0x15e] * v10) + sum(k[0x15c] * r[103], k[0x15d] * r[104])) * 2.0);
	r[105] = sat((k[0x165] * v12) + pos((k[0x164] * r[105]) - v12));
	r[106] = sat((k[0x16b] * v12) + pos((k[0x16a] * v13) - wrap_sum((k[0x169] * v12) - sum_on(sum(k[0x167] * r[106]) / 32768.0, k[0x168] * r[106]))));
	r[108] = sat(((k[0x198] * v21) + sum(k[0x196] * r[107], k[0x197] * r[108])) * 2.0);
	r[110] = r[109];
	r[112] = r[111];
	r[114] = r[113];
	r[116] = r[115];
	r[117] = v18;
	r[118] = v19;
	r[119] = v5;
	r[120] = v1;
	r[121] = v2;
	r[122] = v3;
	r[123] = v4;
	r[124] = v17;
	r[125] = v16;
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
	m[54] = cell(sum(r[5]));
	m[55] = cell(sum(r[6]));
	m[56] = v12;
	t[9] = v20;
	r[103] = sat(((k[0x15a] * r[6]) + sum(k[0x159] * r[5])) * 2.0);
	r[107] = sat((v20 * v11) + 0.0);
	r[109] = v6;
	r[111] = v7;
	r[113] = v8;
	r[115] = v9;
}

}  // namespace fsvr::effects

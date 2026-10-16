// effects/var_m01.cpp - the variation window under Rotary SP, Tremolo and Auto Pan.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m01(State& s)
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
	double const v0 = sat((k[0x168] * m[9]) + 0.0);
	double const v1 = index(v0);
	double const d3 = s.ram[s.at(0x5a, int(v1))];
	double const d4 = s.ram[s.at(0x5b, 1 + int(v1))];
	double const v2 = sat(((128.0 * k[0x15c]) + mag(wrap(((128.0 * k[0x158]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v3 = q15(v2);
	double const v4 = sat((v3 * sat((v3 * v2) + 0.0)) + 0.0);
	double const v5 = pos((128.0 * k[0x16b]) + sum_on(sat(((k[0x169] * v2) + 0.0) * 16.0), k[0x16a] * v4));
	double const v6 = sat((k[0x170] * v5) + 0.0);
	double const v7 = index(v6);
	double const d5 = s.ram[s.at(0x5c, int(v7))];
	double const d6 = s.ram[s.at(0x5d, 1 + int(v7))];
	double const v8 = sat(((128.0 * k[0x160]) + mag(wrap(((128.0 * k[0x15a]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v9 = q15(v8);
	double const v10 = sat((v9 * sat((v9 * v8) + 0.0)) + 0.0);
	double const v11 = pos((128.0 * k[0x16e]) + sum_on(sat(((k[0x16c] * v8) + 0.0) * 16.0), k[0x16d] * v10));
	double const v12 = sat((k[0x178] * v11) + 0.0);
	double const v13 = index(v12);
	double const d7 = s.ram[s.at(0x5e, int(v13))];
	double const d8 = s.ram[s.at(0x5f, 1 + int(v13))];
	double const v14 = sat(((128.0 * k[0x15e]) + mag(wrap(((128.0 * k[0x159]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v15 = q15(v14);
	double const v16 = sat((v15 * sat((v15 * v14) + 0.0)) + 0.0);
	double const v17 = pos((128.0 * k[0x17b]) + sum_on(sat(((k[0x179] * v14) + 0.0) * 16.0), k[0x17a] * v16));
	double const v18 = sat((k[0x17e] * v17) + 0.0);
	double const v19 = index(v18);
	double const d9 = s.ram[s.at(0x60, int(v19))];
	double const d10 = s.ram[s.at(0x61, 1 + int(v19))];
	double const d11 = s.ram[s.at(0x63)];
	double const d12 = s.ram[s.at(0x64)];
	double const d13 = s.ram[s.at(0x65)];
	double const v20 = sat(((k[0x171] * r[5]) + 0.0) * 2.0);
	double const v21 = sat(((k[0x174] * v20) + sum(k[0x172] * r[104], k[0x173] * r[103])) * 2.0);
	double const v22 = sat(((k[0x177] * v21) + sum(k[0x175] * r[105], k[0x176] * r[104])) * 4.0);
	double const v23 = sat(((k[0x17f] * r[6]) + 0.0) * 2.0);
	double const v24 = sat(((k[0x182] * v23) + sum(k[0x180] * r[107], k[0x181] * r[106])) * 2.0);
	double const v25 = sat(((k[0x185] * v24) + sum(k[0x183] * r[108], k[0x184] * r[107])) * 4.0);
	s.ram[s.at(0x66)] = clip32(sat((k[0x19b] * v25) + sum(k[0x19a] * v22)));
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v25) + 0.0));
	double const v26 = frac(v6);
	double const v27 = sat((v26 * d6) - wrap_sum((v26 * d5) - d5));
	double const v28 = q15(sat((k[0x16f] * v5) + 0.0));
	double const v29 = frac(v12);
	double const v30 = sat((v29 * d8) - wrap_sum((v29 * d7) - d7));
	double const v31 = q15(sat((k[0x17c] * v11) + 0.0));
	double const v32 = sat((v31 * v30) + v30);
	double const v33 = frac(v0);
	double const v34 = sat((v33 * d4) - wrap_sum((v33 * d3) - d3));
	double const v35 = q15(sat((k[0x167] * m[9]) + 0.0));
	double const v36 = sat((v35 * v34) + v34);
	double const v37 = sat((k[0x193] * v36) + sum(k[0x191] * sat((v28 * v27) + v27), k[0x192] * v32));
	double const v38 = frac(v18);
	double const v39 = sat((v38 * d10) - wrap_sum((v38 * d9) - d9));
	double const v40 = q15(sat((k[0x17d] * v14) + 0.0));
	double const v41 = sat((k[0x197] * sat((v40 * v39) + v39)) + sum(k[0x195] * v32, k[0x196] * v36));
	r[1] = v11;
	r[2] = v10;
	r[15] = sat(((k[0x199] * v37) + sum(k[0x198] * v22)) * 4.0);
	r[16] = sat(((k[0x19d] * v41) + sum(k[0x19c] * v25)) * 4.0);
	r[103] = v20;
	r[104] = v21;
	r[105] = v22;
	r[106] = v23;
	r[107] = v24;
	r[108] = v25;
	r[119] = v37;
	r[120] = v41;
	r[121] = v32;
	r[122] = v36;
	r[123] = v5;
	r[124] = v4;
	r[125] = v17;
	r[126] = v16;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d11;
	m[32] = d12;
	m[33] = d13;
	m[56] = d5;
	m[57] = d6;
	m[58] = d9;
	m[59] = d10;
	m[60] = d7;
	m[61] = d8;
	m[62] = d3;
	m[63] = d4;
	t[7] = v33;
	t[9] = v28;
	t[10] = v26;
	t[11] = v40;
	t[12] = v38;
	t[13] = v31;
	t[14] = v29;
	t[15] = v35;
}

}  // namespace fs1r::effects

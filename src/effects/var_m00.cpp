// effects/var_m00.cpp - the variation window under Chorus, Celeste, Flanger and Symphonic.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void var_m00(State& s)
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
	double const v0 = ((k[0x159] * m[8]) + 0.0) * 16.0;
	double const v1 = sat(((128.0 * k[0x15c]) + mag(wrap(((128.0 * k[0x158]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v2 = q15(v1);
	double const v3 = pos((k[0x16c] * pos((128.0 * k[0x166]) + sum_on(sat(((k[0x164] * v1) + 0.0) * 16.0), k[0x165] * sat((v2 * sat((v2 * v1) + 0.0)) + 0.0)))) + mag((k[0x16b] * wrap(((128.0 * k[0x15a]) + wrap_sum(v0)) * 2.0)) + 0.0));
	double const v4 = index(v3);
	double const d3 = s.ram[s.at(0x5b, int(v4))];
	double const d4 = s.ram[s.at(0x5c, 1 + int(v4))];
	double const v5 = wrap(v0);
	double const v6 = wrap(((128.0 * k[0x15e]) + v5) * 2.0);
	double const v7 = sat(((128.0 * k[0x161]) + mag(wrap(((128.0 * k[0x15d]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v8 = q15(v7);
	double const v9 = pos((128.0 * k[0x16f]) + sum_on(sat(((k[0x16d] * v7) + 0.0) * 16.0), k[0x16e] * sat((v8 * sat((v8 * v7) + 0.0)) + 0.0)));
	double const v10 = pos((k[0x173] * v9) + mag((k[0x172] * v6) + 0.0));
	double const v11 = index(v10);
	double const d5 = s.ram[s.at(0x5d, int(v11))];
	double const d6 = s.ram[s.at(0x5e, 1 + int(v11))];
	double const d7 = s.ram[s.at(0x5f, int(v11))];
	double const d8 = s.ram[s.at(0x60, 1 + int(v11))];
	double const v12 = wrap(((128.0 * k[0x171]) + v5) * 2.0);
	double const v13 = sat(((128.0 * k[0x169]) + mag(wrap(((128.0 * k[0x163]) + m[8]) * 2.0) + 0.0)) * 2.0);
	double const v14 = q15(v13);
	double const v15 = pos((128.0 * k[0x17e]) + sum_on(sat(((k[0x17c] * v13) + 0.0) * 16.0), k[0x17d] * sat((v14 * sat((v14 * v13) + 0.0)) + 0.0)));
	double const v16 = pos((k[0x182] * v15) + mag((k[0x181] * v12) + 0.0));
	double const v17 = index(v16);
	double const d9 = s.ram[s.at(0x61, int(v17))];
	double const d10 = s.ram[s.at(0x62, 1 + int(v17))];
	double const d11 = s.ram[s.at(0x63)];
	double const d12 = s.ram[s.at(0x64)];
	double const d13 = s.ram[s.at(0x65)];
	double const v18 = frac(v3);
	double const v19 = sat((v18 * d4) - wrap_sum((v18 * d3) - d3));
	double const v20 = sat(((k[0x175] * r[5]) + 0.0) * 2.0);
	double const v21 = sat(((k[0x178] * v20) + sum(k[0x176] * r[104], k[0x177] * r[103])) * 2.0);
	double const v22 = sat(((k[0x17b] * v21) + sum(k[0x179] * r[105], k[0x17a] * r[104])) * 4.0);
	double const v23 = sat(((k[0x185] * r[6]) + 0.0) * 2.0);
	double const v24 = sat(((k[0x188] * v23) + sum(k[0x186] * r[107], k[0x187] * r[106])) * 2.0);
	double const v25 = sat(((k[0x18b] * v24) + sum(k[0x189] * r[108], k[0x18a] * r[107])) * 4.0);
	s.ram[s.at(0x66)] = clip32(sat((k[0x18e] * v25) + sum(k[0x18c] * v19, k[0x18d] * v22)));
	double const v26 = frac(v16);
	double const v27 = sat((v26 * d10) - wrap_sum((v26 * d9) - d9));
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v27) + sum(k[0x19e] * v25)));
	double const v28 = frac(v10);
	double const v29 = sat((v28 * d6) - wrap_sum((v28 * d5) - d5));
	double const v30 = sat((v28 * d8) - wrap_sum((v28 * d7) - d7));
	double const v31 = sat((k[0x193] * v30) + sum(k[0x191] * v19, k[0x192] * v29));
	double const v32 = sat((k[0x19a] * v27) + sum(k[0x198] * v29, k[0x199] * v30));
	r[1] = v9;
	r[2] = v6;
	r[15] = sat(((k[0x197] * v31) + sum(k[0x196] * v22)) * 4.0);
	r[16] = sat(((k[0x19d] * v32) + sum(k[0x19c] * v25)) * 4.0);
	r[103] = v20;
	r[104] = v21;
	r[105] = v22;
	r[106] = v23;
	r[107] = v24;
	r[108] = v25;
	r[119] = v19;
	r[120] = v27;
	r[121] = v29;
	r[122] = v30;
	r[123] = v5;
	r[124] = v31;
	r[125] = v32;
	r[126] = v15;
	r[127] = v12;
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
	m[56] = d3;
	m[57] = d4;
	m[58] = d9;
	m[59] = d10;
	m[60] = d5;
	m[61] = d6;
	m[62] = d7;
	m[63] = d8;
	t[9] = v18;
	t[10] = v26;
	t[11] = v28;
	t[12] = v14;
	t[13] = v8;
}

}  // namespace fsvr::effects

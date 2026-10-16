// effects/var_m06.cpp - the variation window under Ens Detune.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m06(State& s)
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
	double const v0 = pos((k[0x164] * m[8]) + 0.0);
	double const v1 = index(v0);
	double const d3 = s.ram[s.at(0x59, int(v1))];
	double const d4 = s.ram[s.at(0x5a, 1 + int(v1))];
	double const v2 = wrap((m[8] + 0.0) * 2.0);
	double const v3 = pos((128.0 * k[0x15c]) + sum(k[0x15b] * v2));
	double const v4 = pos((k[0x16c] * v3) + 0.0);
	double const v5 = index(v4);
	double const d5 = s.ram[s.at(0x5b, int(v5))];
	double const d6 = s.ram[s.at(0x5c, 1 + int(v5))];
	double const v6 = pos((k[0x173] * m[9]) + 0.0);
	double const v7 = index(v6);
	double const d7 = s.ram[s.at(0x5d, int(v7))];
	double const d8 = s.ram[s.at(0x5e, 1 + int(v7))];
	double const v8 = wrap((m[9] + 0.0) * 2.0);
	double const v9 = pos((128.0 * k[0x160]) + sum(k[0x15f] * v8));
	double const v10 = pos((k[0x17b] * v9) + 0.0);
	double const v11 = index(v10);
	double const d9 = s.ram[s.at(0x5f, int(v11))];
	double const d10 = s.ram[s.at(0x60, 1 + int(v11))];
	double const d11 = s.ram[s.at(0x63)];
	double const d12 = s.ram[s.at(0x64)];
	double const d13 = s.ram[s.at(0x65)];
	double const v12 = sat(((k[0x165] * r[5]) + 0.0) * 2.0);
	double const v13 = sat(((k[0x168] * v12) + sum(k[0x166] * r[104], k[0x167] * r[103])) * 2.0);
	double const v14 = sat(((k[0x16b] * v13) + sum(k[0x169] * r[105], k[0x16a] * r[104])) * 4.0);
	double const v15 = sat(((k[0x174] * r[6]) + 0.0) * 2.0);
	double const v16 = sat(((k[0x177] * v15) + sum(k[0x175] * r[107], k[0x176] * r[106])) * 2.0);
	double const v17 = sat(((k[0x17a] * v16) + sum(k[0x178] * r[108], k[0x179] * r[107])) * 4.0);
	double const v18 = frac(v10);
	double const v19 = sat((v18 * d10) - wrap_sum((v18 * d9) - d9));
	double const v20 = mag(v8 + 0.0);
	double const v21 = pos((128.0 * k[0x16d]) + v20);
	double const v22 = q15(sat(((k[0x172] * v21) + 0.0) * 16.0));
	double const v23 = frac(v6);
	double const v24 = pos((128.0 * k[0x163]) + v20);
	double const v25 = q15(sat(((k[0x171] * v24) + 0.0) * 16.0));
	double const v26 = sat((v25 * sat((v23 * d8) - wrap_sum((v23 * d7) - d7))) - wrap_sum((v22 * v19) - v19));
	double const v27 = frac(v4);
	double const v28 = sat((v27 * d6) - wrap_sum((v27 * d5) - d5));
	double const v29 = mag(v2 + 0.0);
	double const v30 = pos((128.0 * k[0x161]) + v29);
	double const v31 = q15(sat(((k[0x170] * v30) + 0.0) * 16.0));
	double const v32 = frac(v0);
	double const v33 = pos((128.0 * k[0x15e]) + v29);
	double const v34 = q15(sat(((k[0x16f] * v33) + 0.0) * 16.0));
	double const v35 = sat((v34 * sat((v32 * d4) - wrap_sum((v32 * d3) - d3))) - wrap_sum((v31 * v28) - v28));
	s.ram[s.at(0x66)] = clip32(sat((k[0x19a] * v35) + sum(k[0x197] * v14, k[0x198] * v17, k[0x199] * v26)));
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v26) + sum(k[0x19d] * v17, k[0x19e] * v35)));
	double const v36 = sat((k[0x192] * v26) + sum(k[0x191] * v35));
	double const v37 = sat((k[0x194] * v26) + sum(k[0x193] * v35));
	r[1] = v21;
	r[2] = v9;
	r[15] = sat(((k[0x196] * v36) + sum(k[0x195] * v14)) * 4.0);
	r[16] = sat(((k[0x19c] * v37) + sum(k[0x19b] * v17)) * 4.0);
	r[103] = v12;
	r[104] = v13;
	r[105] = v14;
	r[106] = v15;
	r[107] = v16;
	r[108] = v17;
	r[119] = v35;
	r[120] = v26;
	r[121] = v36;
	r[122] = v37;
	r[123] = v33;
	r[124] = v30;
	r[125] = v3;
	r[126] = v24;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[9] = lfo[11];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d11;
	m[26] = d12;
	m[27] = d13;
	m[56] = d3;
	m[57] = d4;
	m[58] = d7;
	m[59] = d8;
	m[60] = d5;
	m[61] = d6;
	m[62] = d9;
	m[63] = d10;
	t[7] = v18;
	t[9] = v34;
	t[10] = v32;
	t[11] = v31;
	t[12] = v27;
	t[13] = v25;
	t[14] = v23;
	t[15] = v22;
}

}  // namespace fs1r::effects

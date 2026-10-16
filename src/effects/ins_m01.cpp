// effects/ins_m01.cpp - the insertion window under Rotary SP, 2WayRotary, Tremolo, Auto Pan and Ambience.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m01(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat(((k[0x1a4] * sat(((128.0 * k[0x1a1]) + mag((m[12] + 0.0) * 2.0)) * 2.0)) + 0.0) * 2.0);
	double const v1 = q15(v0);
	double const v2 = sat((v1 * sat((v1 * v0) + 0.0)) + 0.0);
	double const v3 = pos((128.0 * k[0x1af]) + sum_on(sat(((k[0x1ad] * v0) + 0.0) * 16.0), k[0x1ae] * v2));
	double const v4 = sat((k[0x1b4] * v3) + 0.0);
	double const v5 = index(v4);
	double const d0 = s.ram[s.at(0x6d, int(v5))];
	double const d1 = s.ram[s.at(0x6e, 1 + int(v5))];
	double const v6 = sat(((k[0x1ac] * sat(((128.0 * k[0x1a9]) + mag(wrap(((128.0 * k[0x1a3]) + wrap_sum(((k[0x1a2] * m[12]) + 0.0) * 16.0)) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v7 = q15(v6);
	double const v8 = sat((v7 * sat((v7 * v6) + 0.0)) + 0.0);
	double const v9 = pos((128.0 * k[0x1b8]) + sum_on(sat(((k[0x1b6] * v6) + 0.0) * 16.0), k[0x1b7] * v8));
	double const v10 = sat((k[0x1bc] * v9) + 0.0);
	double const v11 = index(v10);
	double const d2 = s.ram[s.at(0x6f, int(v11))];
	double const d3 = s.ram[s.at(0x70, 1 + int(v11))];
	double const v12 = sat(((k[0x1b5] * sat(((128.0 * k[0x1b2]) + mag(wrap(((128.0 * k[0x1ab]) + m[12]) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v13 = q15(v12);
	double const v14 = sat((v13 * sat((v13 * v12) + 0.0)) + 0.0);
	double const v15 = pos((128.0 * k[0x1c1]) + sum_on(sat(((k[0x1bf] * v12) + 0.0) * 16.0), k[0x1c0] * v14));
	double const v16 = sat((k[0x1c4] * v15) + 0.0);
	double const v17 = index(v16);
	double const d4 = s.ram[s.at(0x71, int(v17))];
	double const d5 = s.ram[s.at(0x72, 1 + int(v17))];
	double const v18 = sat(((k[0x1be] * sat(((128.0 * k[0x1bb]) + mag(wrap(((128.0 * k[0x1a6]) + wrap_sum(((k[0x1a5] * m[12]) + 0.0) * 16.0)) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v19 = q15(v18);
	double const v20 = sat((v19 * sat((v19 * v18) + 0.0)) + 0.0);
	double const v21 = pos((128.0 * k[0x1c9]) + sum_on(sat(((k[0x1c7] * v18) + 0.0) * 16.0), k[0x1c8] * v20));
	double const v22 = sat((k[0x1cc] * v21) + 0.0);
	double const v23 = index(v22);
	double const d6 = s.ram[s.at(0x73, int(v23))];
	double const d7 = s.ram[s.at(0x74, 1 + int(v23))];
	double const v24 = sat(((k[0x1d0] * r[8]) + sum(k[0x1cf] * r[7])) * 2.0);
	double const v25 = sat(((k[0x1d3] * v24) + sum(k[0x1d1] * r[67], k[0x1d2] * r[68])) * 2.0);
	double const v26 = sat(((k[0x1d8] * v25) + sum(k[0x1d4] * r[68], k[0x1d5] * r[69], k[0x1d6] * r[70], k[0x1d7] * r[71])) * 2.0);
	double const v27 = sat(((k[0x1db] * v26) + sum(k[0x1d9] * r[70], k[0x1da] * r[72])) * 4.0);
	double const v28 = sat(((k[0x1dd] * r[7]) + sum(k[0x1dc] * r[8])) * 2.0);
	double const v29 = sat(((k[0x1e0] * v28) + sum(k[0x1de] * r[73], k[0x1df] * r[74])) * 2.0);
	double const v30 = sat(((k[0x1e5] * v29) + sum(k[0x1e1] * r[74], k[0x1e2] * r[75], k[0x1e3] * r[76], k[0x1e4] * r[77])) * 2.0);
	double const v31 = sat(((k[0x1e8] * v30) + sum(k[0x1e6] * r[76], k[0x1e7] * r[78])) * 4.0);
	s.ram[s.at(0x7e)] = clip32(sat((k[0x1fa] * v31) + sum(k[0x1f9] * v27)));
	s.ram[s.at(0x7f)] = clip32(sat((k[0x1ff] * v31) + 0.0));
	double const v32 = frac(v4);
	double const v33 = sat((v32 * d1) - wrap_sum((v32 * d0) - d0));
	double const v34 = q15(sat((k[0x1c3] * v3) + 0.0));
	double const v35 = frac(v16);
	double const v36 = sat((v35 * d5) - wrap_sum((v35 * d4) - d4));
	double const v37 = q15(sat((k[0x1c6] * v15) + 0.0));
	double const v38 = sat((v37 * v36) + v36);
	double const v39 = frac(v10);
	double const v40 = sat((v39 * d3) - wrap_sum((v39 * d2) - d2));
	double const v41 = q15(sat((k[0x1ce] * v9) + 0.0));
	double const v42 = sat((v41 * v40) + v40);
	double const v43 = sat((k[0x1f5] * v42) + sum(k[0x1f3] * sat((v34 * v33) + v33), k[0x1f4] * v38));
	double const v44 = frac(v22);
	double const v45 = sat((v44 * d7) - wrap_sum((v44 * d6) - d6));
	double const v46 = q15(sat((k[0x1cd] * v21) + 0.0));
	double const v47 = sat((k[0x1f8] * v38) + sum(k[0x1f6] * sat((v46 * v45) + v45), k[0x1f7] * v42));
	r[1] = v2;
	r[2] = v20;
	r[3] = v14;
	r[4] = v8;
	r[17] = sat(((k[0x1fc] * v43) + sum(k[0x1fb] * v27)) * 4.0);
	r[18] = sat(((k[0x1fe] * v47) + sum(k[0x1fd] * v31)) * 4.0);
	r[67] = v24;
	r[69] = r[68];
	r[71] = r[70];
	r[72] = v27;
	r[73] = v28;
	r[75] = r[74];
	r[77] = r[76];
	r[78] = v31;
	r[119] = v43;
	r[120] = v47;
	r[121] = v38;
	r[122] = v42;
	r[123] = v15;
	r[124] = v9;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[14] = lfo[14];
	m[15] = lfo[15];
	m[56] = d0;
	m[57] = d1;
	m[58] = d6;
	m[59] = d7;
	m[60] = d4;
	m[61] = d5;
	m[62] = d2;
	m[63] = d3;
	t[3] = v39;
	t[9] = v34;
	t[10] = v32;
	t[11] = v46;
	t[12] = v44;
	t[13] = v37;
	t[14] = v35;
	t[15] = v41;
	r[68] = v25;
	r[70] = v26;
	r[74] = v29;
	r[76] = v30;
}

}  // namespace fs1r::effects

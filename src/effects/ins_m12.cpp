// effects/ins_m12.cpp - the insertion window under Wah+DS+Dly and Wah+OD+Dly.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m12(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	double const d0 = s.ram[s.at(0x71)];
	double const d1 = s.ram[s.at(0x72)];
	double const d2 = s.ram[s.at(0x73)];
	double const d3 = s.ram[s.at(0x74)];
	double const v0 = cell(sat(((k[0x1c2] * m[43]) + sum(k[0x1c1] * m[47])) * 4.0));
	double const v1 = cell(sat(((k[0x1c4] * m[44]) + sum(k[0x1c3] * m[47])) * 4.0));
	double const v2 = sat(((k[0x1d8] * r[85]) + sum(k[0x1d6] * d0, k[0x1d7] * m[48])) * 2.0);
	s.ram[s.at(0x77)] = clip32(sat((k[0x1dc] * v2) + wrap_sum(((k[0x1db] * v1) + sum(k[0x1da] * v0)) * 2.0)));
	double const v3 = sat((k[0x1e8] * d2) + sum(k[0x1e7] * d3));
	double const v4 = sat((k[0x1ea] * d2) + sum(k[0x1e9] * d1));
	double const v5 = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	double const v6 = sat(((k[0x1a3] * v5) + sum(k[0x1a1] * r[68], k[0x1a2] * r[67])) * 2.0);
	double const v7 = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	double const v8 = sat(((k[0x1aa] * v7) + sum(k[0x1a8] * r[71], k[0x1a9] * r[70])) * 2.0);
	double const v9 = sat(((k[0x1a6] * v6) + sum(k[0x1a4] * r[69], k[0x1a5] * r[68])) * 4.0);
	double const v10 = sat(((k[0x1ad] * v8) + sum(k[0x1ab] * r[72], k[0x1ac] * r[71])) * 4.0);
	double const v11 = sat((k[0x1b7] * v10) + sum(k[0x1b6] * v9));
	double const v12 = sat(((k[0x1bb] * r[74]) + sum(k[0x1b9] * r[73], k[0x1ba] * v11)) * 2.0);
	double const v13 = cell(mag(((k[0x1bf] * v12) + 0.0) * 16.0));
	double const v14 = sat((k[0x1c7] * v13) + pos((k[0x1c6] * r[75]) - v13));
	double const v15 = sat((k[0x1cc] * v13) + pos((k[0x1cb] * v14) - wrap_sum((k[0x1ca] * v13) - sum_on(sum(k[0x1c8] * r[76]) / 32768.0, k[0x1c9] * r[76]))));
	double const v16 = cell(sat((k[0x1be] * r[78]) - wrap_sum((k[0x1bd] * m[42]) - v11)));
	double const v17 = sat((k[0x1d0] * r[77]) + sum(k[0x1ce] * r[76], k[0x1cf] * v15));
	double const v18 = sat((k[0x1d4] * v17) + sum(128.0 * k[0x1d2], k[0x1d3] * m[12]));
	double const v19 = sat((q15(v18) * v18) + 0.0);
	double const v20 = q15(sat((128.0 * k[0x1de]) + sum(q15(v19) * v19)));
	double const v21 = sat((k[0x1af] * m[44]) + sum(k[0x1ae] * m[43]));
	double const v22 = sat(((k[0x1b2] * v21) + sum(k[0x1b0] * r[80], k[0x1b1] * r[79])) * 2.0);
	double const v23 = sat(((k[0x1b5] * v22) + sum(k[0x1b3] * r[81], k[0x1b4] * r[80])) * 2.0);
	double const v24 = sat(((k[0x1c0] * sat(((k[0x1bc] * sat(((k[0x1b8] * v23) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v25 = sat(((k[0x1c5] * v24) + 0.0) * 16.0);
	double const v26 = sat(((k[0x1cd] * v25) + 0.0) * 16.0);
	double const v27 = sat(((k[0x1d1] * v26) + 0.0) * 16.0);
	double const v28 = sat(((k[0x1d5] * v27) + 0.0) * 16.0);
	double const v29 = sat((k[0x1e5] * sat(((k[0x1df] * v28) + 0.0) * 16.0)) + sum(k[0x1e0] * v24, k[0x1e1] * v25, k[0x1e2] * v26, k[0x1e3] * v27, k[0x1e4] * v28));
	double const v30 = sat((k[0x1ec] * v29) + 0.0);
	double const v31 = sat((v20 * v16) + r[78]);
	double const v32 = sat(((k[0x1ef] * v30) + sum(k[0x1ed] * r[83], k[0x1ee] * r[82])) * 2.0);
	double const v33 = cell(sat(((k[0x1f4] * m[45]) + sum(k[0x1f0] * r[84], k[0x1f1] * r[83], k[0x1f2] * v32, k[0x1f3] * m[46])) * 2.0));
	r[17] = sat(((k[0x1fd] * v3) + sum(k[0x1fc] * v0)) * 4.0);
	r[18] = sat(((k[0x1ff] * v4) + sum(k[0x1fe] * v1)) * 4.0);
	r[69] = sat(((k[0x1a6] * v6) + sum(k[0x1a4] * r[69], k[0x1a5] * r[68])) * 4.0);
	r[72] = sat(((k[0x1ad] * v8) + sum(k[0x1ab] * r[72], k[0x1ac] * r[71])) * 4.0);
	r[74] = sat(((k[0x1bb] * r[74]) + sum(k[0x1b9] * r[73], k[0x1ba] * v11)) * 2.0);
	r[75] = sat((k[0x1c7] * v13) + pos((k[0x1c6] * r[75]) - v13));
	r[77] = sat((k[0x1d0] * r[77]) + sum(k[0x1ce] * r[76], k[0x1cf] * v15));
	r[78] = sat((v20 * v16) + r[78]);
	r[81] = sat(((k[0x1b5] * v22) + sum(k[0x1b3] * r[81], k[0x1b4] * r[80])) * 2.0);
	r[85] = v2;
	r[119] = v29;
	r[120] = v3;
	r[121] = v4;
	r[122] = v27;
	r[123] = v28;
	r[124] = v19;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[42] = cell(sat((v20 * v31) + m[42]));
	m[47] = cell(sat(((k[0x1f7] * v33) + sum(k[0x1f5] * m[47], k[0x1f6] * m[45])) * 4.0));
	m[48] = d0;
	m[56] = v16;
	m[57] = v13;
	m[58] = v0;
	m[59] = v1;
	m[60] = d0;
	m[61] = d1;
	m[62] = d2;
	m[63] = d3;
	t[9] = v20;
	r[68] = sat(((k[0x1a3] * v5) + sum(k[0x1a1] * r[68], k[0x1a2] * r[67])) * 2.0);
	r[71] = sat(((k[0x1aa] * v7) + sum(k[0x1a8] * r[71], k[0x1a9] * r[70])) * 2.0);
	r[73] = sat((k[0x1b7] * v10) + sum(k[0x1b6] * v9));
	r[76] = sat((k[0x1cc] * v13) + pos((k[0x1cb] * v14) - wrap_sum((k[0x1ca] * v13) - sum_on(sum(k[0x1c8] * r[76]) / 32768.0, k[0x1c9] * r[76]))));
	r[80] = sat(((k[0x1b2] * v21) + sum(k[0x1b0] * r[80], k[0x1b1] * r[79])) * 2.0);
	r[67] = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	r[70] = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	r[79] = sat((k[0x1af] * m[44]) + sum(k[0x1ae] * m[43]));
	m[43] = cell(sat(((k[0x1f9] * v31) + sum(k[0x1f8] * v9)) * 4.0));
	m[44] = cell(sat(((k[0x1fb] * v31) + sum(k[0x1fa] * v10)) * 4.0));
	double const v34 = r[82];
	r[82] = sat((k[0x1ec] * v29) + 0.0);
	double const v35 = r[83];
	r[83] = sat(((k[0x1ef] * v30) + sum(k[0x1ed] * r[83], k[0x1ee] * v34)) * 2.0);
	double const v36 = r[84];
	r[84] = v35;
	double const v37 = m[45];
	m[45] = cell(sat(((k[0x1f4] * m[45]) + sum(k[0x1f0] * v36, k[0x1f1] * v35, k[0x1f2] * v32, k[0x1f3] * m[46])) * 2.0));
	m[46] = v37;
}

}  // namespace fsvr::effects

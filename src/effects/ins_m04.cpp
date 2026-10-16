// effects/ins_m04.cpp - the insertion window under A-Wah+Dist, A-Wah+Odrv, T-Wah+Dist and T-Wah+Odrv.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m04(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat((k[0x1af] * m[49]) + sum(k[0x1ae] * m[48]));
	double const v1 = sat(((k[0x1b2] * v0) + sum(k[0x1b0] * r[76], k[0x1b1] * r[77])) * 2.0);
	double const v2 = sat(((k[0x1b5] * v1) + sum_on(sat((k[0x1b3] * r[77]) + 0.0), k[0x1b4] * r[78])) * 2.0);
	double const v3 = sat(((k[0x1c0] * sat(((k[0x1bc] * sat(((k[0x1b8] * v2) + 0.0) * 16.0)) + 0.0) * 16.0)) + 0.0) * 16.0);
	double const v4 = sat(((k[0x1cd] * v3) + 0.0) * 16.0);
	double const v5 = sat(((k[0x1d1] * v4) + 0.0) * 16.0);
	double const v6 = sat(((k[0x1d4] * v5) + 0.0) * 16.0);
	double const v7 = cell(sat(((k[0x1d8] * v6) + 0.0) * 16.0));
	double const v8 = sat((k[0x1e2] * sat(((k[0x1dc] * v7) + 0.0) * 16.0)) + sum(k[0x1dd] * v3, k[0x1de] * v4, k[0x1df] * v5, k[0x1e0] * v6, k[0x1e1] * v7));
	double const v9 = sat((k[0x1e5] * v8) + 0.0);
	double const v10 = sat(((k[0x1ea] * v9) + sum(k[0x1e6] * r[80], k[0x1e7] * r[79], k[0x1e8] * r[82], k[0x1e9] * r[81])) * 2.0);
	double const v11 = sat(((k[0x1ef] * v10) + sum(k[0x1eb] * r[81], k[0x1ec] * r[82], k[0x1ed] * r[84], k[0x1ee] * r[83])) * 2.0);
	double const v12 = sat(((k[0x1f4] * v11) + sum(k[0x1f0] * r[83], k[0x1f1] * r[84], k[0x1f2] * r[85], k[0x1f3] * r[86])) * 16.0);
	double const v13 = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	double const v14 = cell(sat(((k[0x1a3] * v13) + sum(k[0x1a1] * r[67], k[0x1a2] * m[42])) * 2.0));
	double const v15 = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	double const v16 = cell(sat(((k[0x1aa] * v15) + sum(k[0x1a8] * r[69], k[0x1a9] * m[43])) * 2.0));
	double const v17 = sat(((k[0x1a6] * v14) + sum(k[0x1a4] * m[42], k[0x1a5] * r[68])) * 4.0);
	double const v18 = sat(((k[0x1ad] * v16) + sum(k[0x1ab] * m[43], k[0x1ac] * r[70])) * 4.0);
	double const v19 = sat((k[0x1b7] * v18) + sum(k[0x1b6] * v17));
	double const v20 = sat(((k[0x1bb] * v19) + sum(k[0x1b9] * r[71], k[0x1ba] * r[72])) * 2.0);
	double const v21 = cell(mag(((k[0x1bf] * v20) + 0.0) * 16.0));
	double const v22 = cell(sat((k[0x1be] * r[74]) - wrap_sum((k[0x1bd] * m[44]) - v17)));
	double const v23 = sat((k[0x1c4] * v21) + pos((k[0x1c3] * r[73]) - v21));
	double const v24 = cell(sat((k[0x1c9] * v21) + pos((k[0x1c8] * v23) - wrap_sum((k[0x1c7] * v21) - sum_on(sum(k[0x1c5] * m[46]) / 32768.0, k[0x1c6] * m[46])))));
	double const v25 = cell(sat((k[0x1cc] * v24) + sum(k[0x1ca] * m[46], k[0x1cb] * m[47])));
	double const v26 = sat((k[0x1d0] * v25) + sum(128.0 * k[0x1ce], k[0x1cf] * lfo[13]));
	double const v27 = (q15(v26) * v26) + 0.0;
	double const v28 = wrap(v27);
	double const v29 = q15(sat((128.0 * k[0x1d7]) + sum(q15(sum(q15(v26) * v26)) * v28)));
	double const v30 = cell(sat((k[0x1c2] * r[75]) - wrap_sum((k[0x1c1] * m[45]) - v18)));
	double const v31 = sat((v29 * v22) + r[74]);
	double const v32 = sat((v29 * v30) + r[75]);
	r[17] = sat(((k[0x1fa] * v12) + sum(k[0x1f9] * m[48])) * 4.0);
	r[18] = sat(((k[0x1fc] * v12) + sum(k[0x1fb] * m[49])) * 4.0);
	r[67] = sat(((k[0x1a0] * r[7]) + 0.0) * 2.0);
	r[68] = sat(((k[0x1a6] * v14) + sum(k[0x1a4] * m[42], k[0x1a5] * r[68])) * 4.0);
	r[69] = sat(((k[0x1a7] * r[8]) + 0.0) * 2.0);
	r[70] = sat(((k[0x1ad] * v16) + sum(k[0x1ab] * m[43], k[0x1ac] * r[70])) * 4.0);
	r[72] = sat(((k[0x1bb] * v19) + sum(k[0x1b9] * r[71], k[0x1ba] * r[72])) * 2.0);
	r[73] = sat((k[0x1c4] * v21) + pos((k[0x1c3] * r[73]) - v21));
	r[74] = sat((v29 * v22) + r[74]);
	r[75] = sat((v29 * v30) + r[75]);
	r[76] = v0;
	r[77] = v1;
	r[78] = v2;
	r[80] = r[79];
	r[82] = r[81];
	r[84] = r[83];
	r[86] = r[85];
	r[121] = v28;
	r[122] = v8;
	r[123] = v4;
	r[124] = v5;
	r[125] = v6;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[42] = v14;
	m[43] = v16;
	m[44] = cell(sat((v29 * v31) + m[44]));
	m[45] = cell(sat((v29 * v32) + m[45]));
	m[46] = v24;
	m[47] = v25;
	m[50] = m[48];
	m[51] = m[49];
	m[56] = v21;
	m[57] = v7;
	m[58] = v22;
	m[59] = v30;
	t[9] = v29;
	r[71] = sat((k[0x1b7] * v18) + sum(k[0x1b6] * v17));
	r[79] = v9;
	r[81] = v10;
	r[83] = v11;
	r[85] = v12;
	m[48] = cell(sat(((k[0x1f6] * v31) + sum(k[0x1f5] * v17)) * 4.0));
	m[49] = cell(sat(((k[0x1f8] * v32) + sum(k[0x1f7] * v18)) * 4.0));
}

}  // namespace fsvr::effects

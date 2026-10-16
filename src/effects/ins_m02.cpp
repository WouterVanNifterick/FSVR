// effects/ins_m02.cpp - the insertion window under Phaser1 and Phaser2.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m02(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const v0 = sat(((k[0x1a3] * r[7]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x1a6] * v0) + sum(k[0x1a4] * r[67], k[0x1a5] * r[68])) * 2.0);
	double const v2 = sat(((k[0x1a9] * v1) + sum(k[0x1a7] * r[68], k[0x1a8] * r[69])) * 4.0);
	double const v3 = sat(((k[0x1bb] * r[79]) + sum(k[0x1b7] * r[75], k[0x1b8] * r[76], k[0x1b9] * r[77], k[0x1ba] * r[78])) * 2.0);
	double const v4 = sat(((k[0x1c0] * r[86]) + sum(k[0x1bc] * r[82], k[0x1bd] * r[83], k[0x1be] * r[84], k[0x1bf] * r[85])) * 2.0);
	double const v5 = sat(((k[0x1c6] * r[85]) + sum(k[0x1c3] * v3, k[0x1c4] * v4, k[0x1c5] * r[81])) * 2.0);
	double const v6 = sat(((k[0x1ac] * r[8]) + 0.0) * 2.0);
	double const v7 = sat(((k[0x1af] * v6) + sum(k[0x1ad] * r[70], k[0x1ae] * r[71])) * 2.0);
	double const v8 = sat(((k[0x1b2] * v7) + sum(k[0x1b0] * r[71], k[0x1b1] * r[72])) * 4.0);
	double const v9 = sat((128.0 * k[0x1b4]) + mag((k[0x1b3] * wrap((128.0 * k[0x1aa]) + lfo[13])) + 0.0));
	double const v10 = sat((q15(v9) * v9) + 0.0);
	double const v11 = q15(sat((q15(v10) * v10) - sum(128.0 * k[0x1c7])));
	double const v12 = sat((k[0x1ce] * v4) + sum_on(wrap_sum(((k[0x1cc] * v8) + sum(k[0x1cb] * v2)) * 2.0), k[0x1cd] * v3));
	double const v13 = sat((v11 * v12) - wrap_sum((v11 * r[74]) - sum(r[73])));
	double const v14 = sat((v11 * v13) - wrap_sum((v11 * r[75]) - sum(r[74])));
	double const v15 = sat((v11 * v14) - wrap_sum((v11 * r[76]) - sum(r[75])));
	double const v16 = sat((v11 * v15) - wrap_sum((v11 * r[77]) - sum(r[76])));
	double const v17 = sat((v11 * v16) - wrap_sum((v11 * r[78]) - sum(r[77])));
	double const v18 = sat((128.0 * k[0x1b6]) + mag((k[0x1b5] * wrap((128.0 * k[0x1ab]) + lfo[13])) + 0.0));
	double const v19 = sat((q15(v18) * v18) + 0.0);
	double const v20 = q15(sat((q15(v19) * v19) - sum(128.0 * k[0x1c9])));
	double const v21 = sat((k[0x1e3] * v3) + sum_on(wrap_sum(((k[0x1e1] * v8) + 0.0) * 2.0), k[0x1e2] * v4));
	double const v22 = sat((v20 * v21) - wrap_sum((v20 * r[81]) - sum(r[80])));
	double const v23 = sat((v20 * v22) - wrap_sum((v20 * r[82]) - sum(r[81])));
	double const v24 = sat((v20 * v23) - wrap_sum((v20 * r[83]) - sum(r[82])));
	double const v25 = sat((v20 * v24) - wrap_sum((v20 * r[84]) - sum(r[83])));
	double const v26 = sat((v20 * v25) - wrap_sum((v20 * r[85]) - sum(r[84])));
	r[17] = sat(((k[0x1f7] * v5) + sum(k[0x1f6] * v2)) * 4.0);
	r[18] = sat(((k[0x1f9] * v4) + sum(k[0x1f8] * v8)) * 4.0);
	r[67] = v0;
	r[68] = v1;
	r[69] = v2;
	r[70] = v6;
	r[71] = v7;
	r[72] = v8;
	r[79] = sat((v11 * v17) - wrap_sum((v11 * r[79]) - sum(r[78])));
	r[86] = sat((v20 * v26) - wrap_sum((v20 * r[86]) - sum(r[85])));
	r[119] = v5;
	r[120] = v3;
	r[121] = v4;
	r[122] = v10;
	r[123] = v19;
	m[3] = in[0][13];
	m[4] = in[0][15];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[42] = cell(sum_on(-wrap_sum((k[0x1a1] * m[42]) - (sum(128.0 * k[0x1a0]) / 32768.0)), 128.0 * k[0x1a2]));
	t[10] = v11;
	t[11] = v20;
	r[78] = sat((v11 * v16) - wrap_sum((v11 * r[78]) - sum(r[77])));
	r[85] = sat((v20 * v25) - wrap_sum((v20 * r[85]) - sum(r[84])));
	r[77] = sat((v11 * v15) - wrap_sum((v11 * r[77]) - sum(r[76])));
	r[84] = sat((v20 * v24) - wrap_sum((v20 * r[84]) - sum(r[83])));
	r[76] = sat((v11 * v14) - wrap_sum((v11 * r[76]) - sum(r[75])));
	r[83] = sat((v20 * v23) - wrap_sum((v20 * r[83]) - sum(r[82])));
	r[75] = sat((v11 * v13) - wrap_sum((v11 * r[75]) - sum(r[74])));
	r[82] = sat((v20 * v22) - wrap_sum((v20 * r[82]) - sum(r[81])));
	r[74] = sat((v11 * v12) - wrap_sum((v11 * r[74]) - sum(r[73])));
	r[81] = sat((v20 * v21) - wrap_sum((v20 * r[81]) - sum(r[80])));
	r[73] = sat((k[0x1ce] * v4) + sum_on(wrap_sum(((k[0x1cc] * v8) + sum(k[0x1cb] * v2)) * 2.0), k[0x1cd] * v3));
	r[80] = sat((k[0x1e3] * v3) + sum_on(wrap_sum(((k[0x1e1] * v8) + 0.0) * 2.0), k[0x1e2] * v4));
}

}  // namespace fsvr::effects

// effects/ins_m13.cpp - the insertion window under Cmp+DS+Dly, Cmp+OD+Dly, Dist+Delay and Odrv+Delay.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m13(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	double const d0 = s.ram[s.at(0x69)];
	double const d1 = s.ram[s.at(0x6a)];
	double const d2 = s.ram[s.at(0x6b)];
	double const d3 = s.ram[s.at(0x6c)];
	double const v0 = sat(((k[0x1ab] * cell(sat(((k[0x1a8] * sat(((k[0x1a0] * m[42]) + 0.0) * 16.0)) + 0.0) * 16.0))) + 0.0) * 16.0);
	double const v1 = sat(((k[0x1af] * v0) + 0.0) * 16.0);
	double const v2 = sat(((k[0x1b2] * v1) + 0.0) * 16.0);
	double const v3 = sat(((k[0x1b5] * v2) + 0.0) * 16.0);
	double const v4 = sat(((k[0x1ba] * v3) + 0.0) * 16.0);
	double const v5 = sat((k[0x1cc] * sat((k[0x1c9] * sat(((k[0x1c3] * v4) + 0.0) * 16.0)) + sum(k[0x1c4] * v0, k[0x1c5] * v1, k[0x1c6] * v2, k[0x1c7] * v3, k[0x1c8] * v4))) + 0.0);
	double const v6 = sat(((k[0x1d1] * r[81]) + sum(k[0x1cd] * r[79], k[0x1ce] * r[78], k[0x1cf] * v5, k[0x1d0] * r[80])) * 2.0);
	double const v7 = sat(((k[0x1d6] * r[83]) + sum(k[0x1d2] * r[81], k[0x1d3] * r[80], k[0x1d4] * v6, k[0x1d5] * r[82])) * 2.0);
	double const v8 = sat(((k[0x1db] * r[84]) + sum(k[0x1d7] * r[83], k[0x1d8] * r[82], k[0x1d9] * v7, k[0x1da] * r[85])) * 16.0);
	double const v9 = sat(((k[0x1e0] * v8) + sum(k[0x1df] * r[7])) * 4.0);
	double const v10 = sat(((k[0x1e2] * v8) + sum(k[0x1e1] * r[8])) * 4.0);
	double const v11 = sat(((k[0x1de] * r[86]) + sum(k[0x1dc] * d0, k[0x1dd] * m[43])) * 2.0);
	s.ram[s.at(0x7a)] = clip32(sat((k[0x1e9] * v11) + wrap_sum(((k[0x1e8] * v10) + sum(k[0x1e7] * v9)) * 2.0)));
	double const v12 = sat(((k[0x1a2] * r[8]) + sum(k[0x1a1] * r[7])) * 2.0);
	double const v13 = sat(((k[0x1a7] * r[70]) + sum(k[0x1a3] * r[69], k[0x1a4] * r[68], k[0x1a5] * v12, k[0x1a6] * r[71])) * 2.0);
	double const v14 = sat(((k[0x1aa] * v13) + 0.0) * 2.0);
	double const v15 = cell(mag(((k[0x1b0] * v14) + 0.0) * 16.0));
	double const v16 = sat((k[0x1b4] * v15) + pos((k[0x1b3] * r[72]) - v15));
	double const v17 = sat((k[0x1c2] * v15) + pos((k[0x1c1] * v16) - wrap_sum((k[0x1c0] * v15) - sum_on(sum(k[0x1be] * r[73]) / 32768.0, k[0x1bf] * r[73]))));
	double const v18 = sat((k[0x1ca] * v17) + 0.0);
	double const v19 = pos((128.0 * k[0x1f4]) + v18);
	double const v20 = pos((128.0 * k[0x1f5]) + v18);
	double const v21 = pos((128.0 * k[0x1ee]) + v18);
	double const v22 = pos((128.0 * k[0x1ef]) + v18);
	double const v23 = pos((128.0 * k[0x1f0]) + v18);
	double const v24 = pos((128.0 * k[0x1f1]) + v18);
	double const v25 = pos((128.0 * k[0x1f2]) + v18);
	double const v26 = pos((128.0 * k[0x1f3]) + v18);
	double const v27 = sat((k[0x1ae] * r[76]) + sum(k[0x1ac] * r[74], k[0x1ad] * r[75]));
	double const v28 = sat((q15(v27) * v14) + 0.0);
	double const v29 = q15(v28);
	double const v30 = sat(((k[0x1b9] * sat((v29 * v14) + 0.0)) + sum(k[0x1b7] * v14, k[0x1b8] * v28)) * 16.0);
	r[17] = sat(((k[0x1eb] * sat((k[0x1e4] * d2) + sum(k[0x1e3] * d3))) + sum(k[0x1ea] * v9)) * 4.0);
	r[18] = sat(((k[0x1ed] * sat((k[0x1e6] * d1) + sum(k[0x1e5] * d2))) + sum(k[0x1ec] * v10)) * 4.0);
	r[72] = sat((k[0x1b4] * v15) + pos((k[0x1b3] * r[72]) - v15));
	r[73] = sat((k[0x1c2] * v15) + pos((k[0x1c1] * v16) - wrap_sum((k[0x1c0] * v15) - sum_on(sum(k[0x1be] * r[73]) / 32768.0, k[0x1bf] * r[73]))));
	r[76] = sat((k[0x1ae] * r[76]) + sum(k[0x1ac] * r[74], k[0x1ad] * r[75]));
	r[79] = r[78];
	r[81] = r[80];
	r[83] = r[82];
	r[85] = r[84];
	r[86] = v11;
	r[119] = v21;
	r[120] = v18;
	r[121] = v22;
	r[122] = v23;
	r[123] = v24;
	r[124] = v25;
	r[125] = v26;
	r[126] = v19;
	r[127] = v20;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[42] = cell(sat(((k[0x1bd] * m[42]) + sum(k[0x1bb] * r[77], k[0x1bc] * v30)) * 2.0));
	m[43] = d0;
	m[56] = d0;
	m[57] = d1;
	m[58] = d2;
	m[59] = d3;
	m[60] = v15;
	t[9] = v29;
	r[75] = r[74];
	r[77] = sat(((k[0x1b9] * sat((v29 * v14) + 0.0)) + sum(k[0x1b7] * v14, k[0x1b8] * v28)) * 16.0);
	r[78] = v5;
	r[80] = v6;
	r[82] = v7;
	r[84] = v8;
	r[74] = pos((128.0 * k[0x1fe]) + sum_on(wrap_sum(((k[0x1fb] * v26) + wrap_sum(((k[0x1fa] * v25) + wrap_sum(((k[0x1f9] * v24) + wrap_sum(((k[0x1f8] * v23) + wrap_sum(((k[0x1f7] * v22) + wrap_sum(((k[0x1f6] * v21) + 0.0) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 2.0)) * 4.0), k[0x1fc] * v19, k[0x1fd] * v20));
	double const v31 = r[68];
	r[68] = sat(((k[0x1a2] * r[8]) + sum(k[0x1a1] * r[7])) * 2.0);
	double const v32 = r[69];
	r[69] = v31;
	double const v33 = r[70];
	r[70] = sat(((k[0x1a7] * r[70]) + sum(k[0x1a3] * v32, k[0x1a4] * v31, k[0x1a5] * v12, k[0x1a6] * r[71])) * 2.0);
	r[71] = v33;
}

}  // namespace fsvr::effects

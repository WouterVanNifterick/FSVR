// effects/ins_m00.cpp - the insertion window under Chorus, Celeste, Flanger, Symphonic, Delay LCR, Delay L,R,
// Echo and CrossDelay.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m00(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x68)];
	double const d1 = s.ram[s.at(0x69)];
	double const v0 = ((k[0x1a1] * m[12]) + 0.0) * 16.0;
	double const v1 = wrap(((128.0 * k[0x1a2]) + wrap_sum(v0)) * 2.0);
	double const v2 = sat(((k[0x1a7] * sat(((128.0 * k[0x1a4]) + mag(wrap(((128.0 * k[0x1a0]) + m[12]) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v3 = q15(v2);
	double const v4 = sat((v3 * sat((v3 * v2) + 0.0)) + 0.0);
	double const v5 = pos((128.0 * k[0x1b1]) + sum_on(sat(((k[0x1af] * v2) + 0.0) * 16.0), k[0x1b0] * v4));
	double const v6 = pos((k[0x1b4] * v5) + mag((k[0x1b3] * v1) + 0.0));
	double const v7 = index(v6);
	double const d2 = s.ram[s.at(0x6e, int(v7))];
	double const d3 = s.ram[s.at(0x6f, 1 + int(v7))];
	double const v8 = wrap(v0);
	double const v9 = wrap(((128.0 * k[0x1a6]) + v8) * 2.0);
	double const v10 = sat(((k[0x1ac] * sat(((128.0 * k[0x1a9]) + mag(wrap(((128.0 * k[0x1a5]) + m[12]) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v11 = q15(v10);
	double const v12 = sat((v11 * sat((v11 * v10) + 0.0)) + 0.0);
	double const v13 = pos((128.0 * k[0x1ba]) + sum_on(sat(((k[0x1b8] * v10) + 0.0) * 16.0), k[0x1b9] * v12));
	double const v14 = pos((k[0x1bd] * v13) + mag((k[0x1bc] * v9) + 0.0));
	double const v15 = index(v14);
	double const d4 = s.ram[s.at(0x70, int(v15))];
	double const d5 = s.ram[s.at(0x71, 1 + int(v15))];
	double const d6 = s.ram[s.at(0x72, int(v15))];
	double const d7 = s.ram[s.at(0x73, 1 + int(v15))];
	s.ram[s.at(0x74)] = clip32(sum(d0));
	double const v16 = wrap(((128.0 * k[0x1ae]) + v8) * 2.0);
	double const v17 = sat(((k[0x1bb] * sat(((128.0 * k[0x1b7]) + mag(wrap(((128.0 * k[0x1ab]) + m[12]) * 2.0) + 0.0)) * 2.0)) + 0.0) * 2.0);
	double const v18 = q15(v17);
	double const v19 = sat((v18 * sat((v18 * v17) + 0.0)) + 0.0);
	double const v20 = pos((128.0 * k[0x1cf]) + sum_on(sat(((k[0x1cd] * v17) + 0.0) * 16.0), k[0x1ce] * v19));
	double const v21 = pos((k[0x1d3] * v20) + mag((k[0x1d2] * v16) + 0.0));
	double const v22 = index(v21);
	double const d8 = s.ram[s.at(0x75, int(v22))];
	double const d9 = s.ram[s.at(0x76, 1 + int(v22))];
	s.ram[s.at(0x7a)] = clip32(sum(d1));
	double const v23 = sat(((k[0x1d9] * r[8]) + 0.0) * 2.0);
	double const v24 = sat(((k[0x1dc] * v23) + sum(k[0x1da] * r[73], k[0x1db] * r[74])) * 2.0);
	double const v25 = sat(((k[0x1e1] * v24) + sum(k[0x1dd] * r[74], k[0x1de] * r[75], k[0x1df] * r[76], k[0x1e0] * r[77])) * 2.0);
	double const v26 = sat(((k[0x1e4] * v25) + sum(k[0x1e2] * r[76], k[0x1e3] * r[78])) * 4.0);
	double const v27 = frac(v21);
	double const v28 = sat((v27 * d9) - wrap_sum((v27 * d8) - d8));
	double const v29 = sat(((k[0x1e9] * v28) + sum(k[0x1e7] * r[81], k[0x1e8] * r[82])) * 2.0);
	double const v30 = frac(v6);
	double const v31 = sat((v30 * d3) - wrap_sum((v30 * d2) - d2));
	double const v32 = sat(((k[0x1d8] * v31) + sum(k[0x1d6] * r[79], k[0x1d7] * r[80])) * 2.0);
	s.ram[s.at(0x7e)] = clip32(sat((k[0x1fb] * v32) + sum(k[0x1f9] * v26, k[0x1fa] * v29)));
	double const v33 = sat(((k[0x1bf] * r[7]) + 0.0) * 2.0);
	double const v34 = sat(((k[0x1c2] * v33) + sum(k[0x1c0] * r[68], k[0x1c1] * r[67])) * 2.0);
	double const v35 = sat(((k[0x1c7] * v34) + sum(k[0x1c3] * r[68], k[0x1c4] * r[69], k[0x1c5] * r[70], k[0x1c6] * r[71])) * 2.0);
	double const v36 = sat(((k[0x1ca] * v35) + sum(k[0x1c8] * r[70], k[0x1c9] * r[72])) * 4.0);
	s.ram[s.at(0x7f)] = clip32(sat((k[0x1ff] * v29) + sum(k[0x1fc] * v36, k[0x1fd] * v26, k[0x1fe] * v32)));
	double const v37 = frac(v14);
	double const v38 = sat((v37 * d5) - wrap_sum((v37 * d4) - d4));
	double const v39 = sat((v37 * d7) - wrap_sum((v37 * d6) - d6));
	double const v40 = sat((k[0x1ef] * v39) + sum(k[0x1ed] * v31, k[0x1ee] * v38));
	double const v41 = sat((k[0x1f2] * v38) + sum(k[0x1f0] * v28, k[0x1f1] * v39));
	r[1] = v4;
	r[2] = v19;
	r[3] = v12;
	r[4] = sat((k[0x1ef] * v39) + sum(k[0x1ed] * v31, k[0x1ee] * v38));
	r[5] = sat((k[0x1f2] * v38) + sum(k[0x1f0] * v28, k[0x1f1] * v39));
	r[17] = sat(((k[0x1f5] * v40) + sum(k[0x1f3] * v36, k[0x1f4] * v26)) * 4.0);
	r[18] = sat(((k[0x1f8] * v41) + sum(k[0x1f6] * v26, k[0x1f7] * v36)) * 4.0);
	r[67] = v33;
	r[69] = r[68];
	r[71] = r[70];
	r[72] = v36;
	r[73] = v23;
	r[75] = r[74];
	r[77] = r[76];
	r[78] = v26;
	r[79] = v31;
	r[80] = v32;
	r[81] = v28;
	r[82] = v29;
	r[119] = v38;
	r[120] = v39;
	r[121] = v5;
	r[122] = v1;
	r[123] = v20;
	r[124] = v16;
	r[125] = v13;
	r[126] = v9;
	r[127] = v8;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[13] = lfo[13];
	m[14] = lfo[14];
	m[15] = lfo[15];
	m[42] = d0;
	m[43] = d1;
	m[56] = d2;
	m[57] = d3;
	m[58] = d8;
	m[59] = d9;
	m[60] = d4;
	m[61] = d5;
	m[62] = d6;
	m[63] = d7;
	t[9] = v30;
	t[10] = v27;
	t[11] = v37;
	r[68] = v34;
	r[70] = v35;
	r[74] = v24;
	r[76] = v25;
}

}  // namespace fsvr::effects

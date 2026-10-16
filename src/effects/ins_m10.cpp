// effects/ins_m10.cpp - the insertion window under Lo-Fi.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

using namespace word;

void ins_m10(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;
	auto const& in = s.in;
	double const v0 = sat((k[0x1a6] * r[8]) + sum(k[0x1a5] * r[7]));
	double const v1 = sat(((k[0x1a9] * v0) + sum(k[0x1a7] * r[68], k[0x1a8] * r[67])) * 4.0);
	double const v2 = sat(((k[0x1ae] * r[69]) + sum(k[0x1aa] * r[70], k[0x1ab] * r[68], k[0x1ac] * v1, k[0x1ad] * r[71])) * 2.0);
	double const v3 = sum(v2);
	double const v4 = sat((128.0 * k[0x1a0]) + r[81]);
	double const v5 = wrap_sum(v4 - sum(128.0 * k[0x1a2]));
	bool const v6 = v5 > 0;
	double const v7 = v6 ? cell(v3) : m[42];
	double const v8 = sat((k[0x1b4] * r[79]) + v7);
	bool const v9 = !v6;
	bool const v10 = v6 || v9;
	double const v11 = v10 ? sum(k[0x1b5] * r[7]) : (v10 ? v8 : (v6 ? sum(128.0 * k[0x1b2]) : (v6 ? v3 : v2)));
	double const v12 = sat(((k[0x1a4] * r[78]) + 0.0) * 4.0);
	double const v13 = sat(((k[0x1b6] * v12) + v11) * 4.0);
	double const v14 = cell(sum(128.0 * k[0x1a1]));
	double const v15 = v10 ? v8 : r[121];
	double const v16 = sat(v15 - v14);
	double const v17 = v10 ? sum(k[0x1b8] * r[8]) : (v10 ? v16 : (v10 ? v13 : v11));
	double const v18 = sat(((k[0x1b9] * v12) + v17) * 4.0);
	double const v19 = v14 + 0.0;
	double const v20 = v10 ? v16 : v5;
	bool const v21 = v20 > 0;
	bool const v22 = v9 && v21;
	bool const v23 = v6 && v21;
	bool const v24 = v23 || v22;
	double const v25 = (k[0x1c1] * v14) + 0.0;
	double const v26 = sat(v15 + v14);
	bool const v27 = !v21;
	bool const v28 = v9 && v27;
	bool const v29 = v6 && v27;
	bool const v30 = v29 || v28;
	bool const v31 = (v30 ? v26 : v20) < 0;
	bool const v32 = v28 && v31;
	bool const v33 = v29 && v31;
	bool const v34 = v33 || v32;
	double const v35 = v34 ? wrap(v25) : (v24 ? wrap(v19) : v15);
	double const v36 = sum(v35);
	bool const v37 = !v31;
	bool const v38 = ((((v23 || v33) || (v29 && v37)) || v22) || v32) || (v28 && v37);
	double const v39 = v38 ? v36 : (v34 ? sum(k[0x1c1] * v14) : (v30 ? v26 : (v24 ? sum(v14) : (v10 ? v18 : v17))));
	double const v40 = sat(r[80] + v39);
	double const v41 = v38 ? v40 : r[80];
	double const v42 = sat(((k[0x1c8] * v41) + 0.0) * 16.0);
	double const v43 = v38 ? v42 : v35;
	double const v44 = sat(((k[0x1cb] * v43) + 0.0) * 16.0);
	double const v45 = v38 ? v44 : v43;
	double const v46 = sat(((k[0x1ce] * v45) + 0.0) * 16.0);
	double const v47 = v38 ? v36 : (v38 ? v42 : (v38 ? v40 : v39));
	double const v48 = sat(r[79] + v47);
	double const v49 = v38 ? sum(k[0x1cf] * r[73]) : (v38 ? v46 : (v38 ? v44 : (v38 ? v48 : v47)));
	double const v50 = v38 ? sum_on(v49, k[0x1d0] * r[72]) : v49;
	double const v51 = v38 ? v46 : r[72];
	double const v52 = sat(((k[0x1d1] * v51) + v50) * 2.0);
	double const v53 = v38 ? v52 : r[73];
	double const v54 = v38 ? r[73] : (v38 ? r[74] : (v38 ? v51 : (v38 ? r[72] : (v38 ? r[73] : (v38 ? v45 : (v38 ? 0.0 : (v38 ? 0.0 : (v38 ? v43 : (v38 ? r[79] : (v38 ? v35 : (v38 ? v41 : (v38 ? 0.0 : (v38 ? 0.0 : (v38 ? r[80] : (v38 ? v35 : (v34 ? 0.0 : (v34 ? 0.0 : (v34 ? 0.0 : (v30 ? 0.0 : (v30 ? 0.0 : (v30 ? v15 : (v24 ? 0.0 : (v10 ? v12 : (v10 ? r[8] : (v10 ? v15 : (v10 ? v12 : (v10 ? r[7] : (v10 ? r[79] : (v6 ? 0.0 : (v6 ? 0.0 : (v6 ? v2 : 0.0)))))))))))))))))))))))))))))));
	double const v55 = v38 ? true : (v38 ? true : (v38 ? true : (v38 ? true : (v38 ? true : (v38 ? true : (v38 ? false : (v38 ? false : (v38 ? true : (v38 ? true : (v38 ? true : (v38 ? true : (v38 ? false : (v38 ? false : (v38 ? true : (v38 ? true : (v34 ? false : (v34 ? false : (v34 ? false : (v30 ? false : (v30 ? false : (v30 ? true : (v24 ? false : (v10 ? true : (v10 ? true : (v10 ? true : (v10 ? true : (v10 ? true : (v10 ? true : (v6 ? false : (v6 ? false : (v6 ? true : false)))))))))))))))))))))))))))))));
	double const v56 = v38 ? sum(k[0x1d2] * r[74]) : (v38 ? v52 : v50);
	double const v57 = v38 ? sum_on(v56, k[0x1d3] * r[73]) : v56;
	double const v58 = v38 ? sum_on(v57, k[0x1d4] * v53) : v57;
	double const v59 = v38 ? sum_on(v58, k[0x1d5] * r[75]) : v58;
	double const v60 = sat(((k[0x1d6] * r[76]) + v59) * 4.0);
	double const v61 = v38 ? sum(k[0x1d7] * r[75]) : (v38 ? v60 : v59);
	double const v62 = v38 ? sum_on(v61, k[0x1d8] * r[77]) : v61;
	double const v63 = v38 ? v60 : r[75];
	double const v64 = sat(((k[0x1d9] * v63) + v62) * 4.0);
	double const v65 = v38 ? sum(k[0x1da] * r[78]) : (v38 ? v64 : v62);
	double const v66 = v38 ? v64 : r[77];
	double const v67 = (128.0 * k[0x1b2]) + 0.0;
	r[17] = v10 ? v13 : r[17];
	r[18] = v10 ? v18 : r[18];
	r[67] = v0;
	r[69] = r[68];
	r[71] = r[70];
	r[72] = v38 ? v46 : r[72];
	r[73] = v38 ? v52 : r[73];
	r[74] = v38 ? (v55 ? v54 : v53) : r[74];
	r[76] = v38 ? ((v38 ? true : (v38 ? true : (v38 ? true : (v38 ? true : v55)))) ? (v38 ? r[75] : (v38 ? r[76] : (v38 ? r[75] : (v38 ? v53 : v54)))) : r[77]) : r[76];
	r[78] = v38 ? sat(((k[0x1dc] * v66) + (v38 ? sum_on(v65, k[0x1db] * r[77]) : v65)) * 4.0) : r[78];
	r[79] = v38 ? v48 : r[79];
	r[80] = v41;
	r[81] = v6 ? wrap(v67) : v4;
	r[119] = v12;
	r[121] = v45;
	m[3] = in[0][13];
	m[4] = v34 ? in[0][14] : m[4];
	m[7] = in[1][13];
	m[42] = v7;
	m[56] = v14;
	r[68] = v1;
	r[70] = v2;
	r[75] = v38 ? v60 : r[75];
	r[77] = v38 ? v64 : r[77];
}

}  // namespace fsvr::effects

// effects/ins_m10.cpp - the insertion window's program for Lo-Fi.
//
// rom::INSERTION_PROGRAMS[10], the program the insertion window holds under the one type
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 10: ins-20 Lo-Fi on EffectHost::loFi. Steps 0x1dd to
// 0x1ff are zero class-0 steps, so the program proper is the 61 steps 0x1a0 to 0x1dc and the last 35 do
// nothing but advance the step counter and move the x latch. Nothing in it touches the delay RAM, no step
// reads or writes a t register, and the three cells the base program leaves behind (m[3] and m[7] from
// the serial inputs of page 13, m[4] from page 14) are written here and read nowhere in this window.
//
// It is a bit crusher: a sample and hold, a quantizer with two error accumulators around it, then a tone
// chain over the result. The 96 steps in the order they run it:
//
//   0x1a0-0x1a3  the sample and hold's counter. r[81] climbs by one word, 1/256 of full scale, a pass and
//                saturates rather than wraps, and 0x1a2 puts ins::LOFI_RATE into the running value for
//                0x1a3 to compare the counter against. That compare is the flag the block at 0x1b0 is
//                gated on, so ins::LOFI_RATE sets how often the signal is latched. 0x1a1 loads
//                ins::LOFI_BITS into m[56], the quantizer's step, and that word is the word length: the
//                step is the word length's own value in units of 2^-13, so a short word length crushes to
//                few levels and UNITY's 32767 is a step of 4.0, a sixth of full scale.
//
//   0x1a4  r[119] = 2 * r[78], the tone chain's output from the pass before. The mix at 0x1b6 and 0x1b9
//          read it, so the wet signal reaches the outputs one sample behind the pass that made it.
//
//   0x1b0-0x1b3  the latch: m[42] = r[70] and r[81] = 0, run only when the compare at 0x1a3 fired.
//
//   0x1b7-0x1c3  the quantizer, which is one subtraction and two conditional skips. 0x1b7 subtracts m[56]
//          from r[121] and its sign is the flag; if it is positive the value was above the step, so 0x1ba
//          lets 0x1bb write +step and 0x1bc jumps out to 0x1c4. If it was not, 0x1ba skips to 0x1bd, which
//          puts the step back and compares against zero: negative goes to -step through 0x1c0 and 0x1c1,
//          and anything from zero to the step is left as it was. So m[56] is a dead zone and the bit
//          crush is the clamping to +-step around it.
//
//   0x1ce  r[72] = ins::LOFI_OUT[0] * r[121] * 16, the bit assign's output level: 2048 up to UNITY, or the
//          wet gain when the assign is above six.
//
//   0x1cf-0x1dc  the tone chain over the quantized signal: ins::LOFI_FILTER, an order-1 section over r[72]
//          into r[73], ins::LOFI_FILTER_2, the order-2 section into r[75] (its z^-1 word is 0x1d4, which
//          carries r[73] one pass older into r[74], and r[76] is its second output delay), ins::LOFI_FILTER_3,
//          an order-1 section into r[77], and ins::LOFI_HIGH_SHELF_2, an order-1 section into r[78], which
//          is the wet signal 0x1a4 reads. fx_tone.cpp's loFi() takes the three filters' shapes, frequencies
//          and gains from rom::LOFI_FILTERS, one row per filter type, and the emphasis parameter chooses
//          the last section's words: a high shelf with it on and a plain gain with it off, which is what the
//          two zero words at 0x1da and 0x1db are in that case.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals in;
// r[17] and r[18] the two outputs; r[67] the input sum; r[68], r[69], r[70] and r[71] the input chain's
// filter, its x[-1] and y[-2] delays, its output and its y[-1] delay; r[72] to r[78] the tone chain, one
// pair per section; r[79] and r[80] the two error accumulators; r[81] the sample rate counter; r[119] the
// wet signal; r[121] the scratch every stage hands to the next; m[42] the held signal; m[56] the
// quantizer's step.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

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

}  // namespace fs1r::effects

// effects/ins_m13.cpp - the insertion window's program for Cmp+DS+Dly, Cmp+OD+Dly,
// Dist+Delay and Odrv+Delay: its 96 steps.
//
// One signal, not two. r[7] and r[8] are the base program's left and right signals, and 0x1a1 and 0x1a2
// reduce them to r[68] under the program's own words; every step after that works on the one signal.
// What the 96 compute, in the order the steps run:
//
//   0x1a0-0x1a2  the stereo pair as one signal, and the drive state m[42] into r[119].
//   0x1a3-0x1ab  ins::DD_HPF, the input highpass, whose state alone is used; and the first three of the
//                delay line's four tap reads, slots 0x69, 0x6a and 0x6b.
//   0x1ac-0x1ae  ins::DD_ATTACK, a one-pole smoother of r[74] into r[76] with unity gain, and t[9].
//   0x1af-0x1bd  the fourth tap, slot 0x6c, ins::DD_BAND's first series words, the release pair, and
//                ins::DD_LPF into m[42].
//   0x1be-0x1c9  the release pair's other two words and ins::DD_BAND's sum of the four series states.
//   0x1ca-0x1cc  ins::DD_THRESHOLD, an empty step, and ins::DD_DRIVE's gain word.
//   0x1cd-0x1db  ins::DD_LPF2, ins::DD_PEAK and ins::DD_LOW_SHELF in series, of which only DD_LOW_SHELF's
//                state is read.
//   0x1dc-0x1de  ins::DD_LPF_2 over the first two taps: r[86], the delay line's feedback.
//   0x1df-0x1ed  the dry and wet mix, the four taps at the outputs, the line's write at slot 0x7a, and
//                the two outputs.
//   0x1ee-0x1ff  the level ladder and ins::DD_RATIO, which build r[74], the envelope 0x1ac smooths.
//
// Five shapes repeat, and they are worth naming because the program's meaning is mostly in them:
//
//   - Six filter sections, ins::DD_HPF, DD_LPF, DD_LPF2, DD_PEAK, DD_LOW_SHELF and DD_LPF_2, each a
//     host table of the designFilter() kind (b0, b1, b2, -a1, -a2). A section runs its words in the
//     order b2, b1, b0, -a2, -a1 rather than the order designFilter() wrote them, and its b0 step
//     carries rcopy, so the register it writes holds the input a sample ago and the b2 step reads that
//     one step further back. The two feedback terms both read the same state, because the rcopy at the
//     head of the next section puts r[70]'s own value into r[71]; the state is therefore a recursion on
//     (-a1 - a2) and not a two-state one. INFERRED: what the chip's second state register would have
//     been is not settled by this program, which never uses a second one.
//   - The delay line, ins::DD_SLOTS' five slots: 0x7a written at 0x1e9, and 0x69 to 0x6c read one word
//     apart at 0x1a4, 0x1a8, 0x1ac and 0x1b0, each read taken up by a cell five steps later (m[56],
//     m[57], m[58], m[59]), which is CAPMIN, the nearest step()'s capture reaches. The fourth, m[59],
//     reaches the left output on its own, because 0x1e4 multiplies the third, m[58], by a zero word of
//     the program's own and adds what 0x1e3 left. The third is dropped: 0x1e5 reads it again and
//     clears the running value. The second, m[57], reaches the right output at 0x1e6, whose word is the
//     preset's own full scale, and the first two reach the line's feedback instead, through r[86].
//   - The level ladder, eight class-1 steps at 0x1ee to 0x1f5, each r[120] minus an offset of the
//     program's own (a quarter, a half, 1, 2, 4, 8, 16, 64) under a clamp that drops negatives, so each
//     is max(0, r[120] - offset). ins::DD_RATIO then weights the eight in the compiler's order, which
//     is not the order the steps run: 0, 2, 3, 1, 4, 5, 6, 7. That is a piecewise-linear curve over
//     eight bands, and 0x1fe adds it to full scale to make r[74].
//   - The two gains, ins::DD_LEVEL and ins::DD_MODE, are the two halves of that same curve: DD_LEVEL
//     scales onto the taps at the outputs, DD_MODE onto r[124] and r[125] in the drive. So the four
//     types differ mostly in which steps the host writes, and not in the program's shape.
//   - ins::DD_BAND's geometric series, from storeEdge(t.edge): five words into 0x1af, 0x1b2, 0x1b5,
//     0x1ba and 0x1c3, summed at 0x1c4 to 0x1c9 with a fifth of each.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals in;
// r[17] and r[18] the two outputs; r[68] the pair as one signal; r[69], r[70] and r[71] the DD_HPF's
// two taps and its state; r[72] and r[73] the release pair's two registers; r[74] the envelope and r[76]
// its smoothed form, with r[75] a copy of r[74]; r[77] the DD_LPF's input; r[78] the drive word's gain;
// r[80] and r[82] the DD_LPF2's and DD_PEAK's states; r[84] the DD_LOW_SHELF's state and the wet signal;
// r[86] the delay line's feedback. Of the cells, m[3] and m[7] are the serial inputs of page 13 and m[4]
// the left serial input of page 14, read nowhere here; m[42] the drive state; m[43] the second tap a pass
// on; m[56] to m[59] the four taps; m[60] the DD_MODE word on the DD_HPF's state.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

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

}  // namespace fs1r::effects

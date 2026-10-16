// effects/ins_m03.cpp - the insertion window's program for type msb 03.
//
// rom::INSERTION_PROGRAMS[3], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 3: ins-37 ER 1, ins-38 ER 2, ins-39 Gate Rev and
// ins-40 Revrs Gate, all four programmed by EffectHost::earlyReflections() in fx_reverb.cpp. The same
// 96 words run under every one of them; what the type changes is which words the host writes, the
// pattern row erPattern() reads (six entries further on for Gate Rev, eight for Reverse Gate), and
// how many of the twenty gains it leaves at zero. Nothing here knows which one is running.
//
// Unlike ins-m02, which touches no delay RAM at all, this program is most of a delay line: eleven
// reads of ten tap slots plus one allpass point, and nine writes. What it computes, in the groups the
// comments open each of. The names are the host's, from rom_tables.cpp and fx_reverb.cpp.
//   0x1a0-0x1b3  the filters. 0x1a0 and 0x1a1 mix r[7] and r[8] into r[67] at the ROM word 0x2000 each
//                and twice that, so the high pass and the low pass run on one mono signal and the two
//                channels are told apart by their dry signals and their tap lines further on.
//                ins::ER_HPF's five words are the biquad over r[67] to r[70] (0x1a2 to 0x1a6, b0 at
//                0x1a6, b1 at 0x1a3, b2 at 0x1a2, -a1 at 0x1a4, -a2 at 0x1a5), ins::ER_LPF_2's the biquad
//                over r[71] to r[74] (0x1ac to 0x1b0), and ins::ER_LPF's three words the one-pole High
//                Damp filter (0x1b1 to 0x1b3) on the twentieth tap. Between them the steps read the
//                first five right-channel taps, each read landing in a cell five steps later.
//   0x1b4-0x1bb  the first two of the four diffusion rows, ins::ER_ROW_STEPS' first eight words: r[73],
//                the low pass's output, and the cells m[18] and m[19] into the slots 0x6d and 0x6e
//                (ins::ER_ROW_SLOTS[0] and [1]). Those two cells are the rows' own allpass points,
//                ins::ER_ROW_SLOTS[2] and [3], the slots 0x2e and 0x2f, which the base program's steps
//                0x0be and 0x0c2 capture: each row is fed its own output delayed, which is the pair of
//                allpasses the FB Level then feeds the delay line with. Both rows end at r[119], so
//                the second overwrites the first three steps later.
//   0x1bc-0x1c5  the ten left-channel gains, ins::ER_STEPS[0] to [9], on the cells m[22] to m[31].
//                Those cells are not this window's: the base program's steps 0x118 to 0x194 capture
//                them from ins::ER_SLOTS' even entries, the slots 0x44 to 0x63 and 0x64. The sum lands
//                in m[43] and is written into the slot 0x71 by 0x1c7.
//   0x1c6-0x1d1  that write, and the reads of the eighth, ninth and tenth right-channel taps. Seven
//                of these steps are class 0 with no memory op and no ALU: they count and nothing else.
//   0x1d2-0x1db  the ten right-channel gains, ins::ER_STEPS[10] to [19], on the cells m[56] to m[63]
//                and back on m[56] and m[57], which this program fills itself from the slots 0x68 to
//                0x75 (ins::ER_SLOTS' odd entries). The sum lands in m[44] and 0x1db writes it into
//                the slot 0x76. erPattern() leaves ins::ER_STEPS[19] at zero, so the tenth right tap
//                is never heard.
//   0x1dc-0x1df  the two sums with their allpass points, ins::ER_DIFF_SLOTS. earlyReflections()'
//                DIFFUSION case puts the slot 0x65 at delay[0x76] + d + 1 and the slot 0x75 at
//                delay[0x71] + d + 1, so each sum picks the other one's up d + 1 samples later: r[121]
//                is the left sum and r[120] the right.
//   0x1e0-0x1ed  the last two diffusion rows, ins::ER_ROW_STEPS' other eight words: an allpass and a
//                line for each sum, the slots 0x77 and 0x79 for r[120] and 0x78 and 0x7a for r[121]
//                (ins::ER_ROW_SLOTS[4] to [7]), which is what the two registers read back at 0x1e6
//                and 0x1ea.
//   0x1ee-0x1ef  the delay line's input. r[119], the diffusion rows' output, goes in at unity, and
//                ins::ER_FEEDBACK[0], the FB Level word, scales r[75], the High Damp filter's output on
//                the twentieth tap. The write is to the slot 0x7b, ins::ER_SLOTS[21], which is where
//                earlyReflections()'s erPattern() measures every tap's position from: the ten right
//                taps sit the initial delay plus the room size past it, so the line is the tail the
//                taps are read from.
//   0x1f0-0x1f7  the dry and wet mix, ins::ER_WET's four words: r[7] and r[8] at [0] and [2] against
//                r[121] and r[120] at [1] and [3], into r[17] and r[18], the two outputs the base
//                program reads back. 0x1f3 and 0x1f7 write the slots 0x7c and 0x7d with their
//                coefficients, which no parameter writes and which are zero, so both slots take zero
//                every pass; nothing reads them.
//   0x1f8-0x1ff  eight steps that count and nothing else.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] to r[70] the
// high pass, r[71] to r[74] the low pass, r[73] its output; r[75] the High Damp filter's output, the
// delay line's input; r[119] the two first diffusion rows' output; r[120] the right sum, r[121] the
// left; r[17] and r[18] the two outputs.
//
// The old model in vop3_effects.h has a case for these four types and is a loop of twelve taps off one
// line with a hand-picked gain envelope per tap, rising for Reverse Gate and decaying for Gate Reverb.
// It has no delay line's worth of slots, no diffusion rows, no feedback word and no High Damp filter,
// and where the two differ the program above is what runs. It is no reference for any number here.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void ins_m03(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;
	auto const& in = s.in;
	double const d0 = s.ram[s.at(0x68)];
	double const d1 = s.ram[s.at(0x69)];
	double const d2 = s.ram[s.at(0x6a)];
	double const d3 = s.ram[s.at(0x6b)];
	double const d4 = s.ram[s.at(0x6c)];
	double const v0 = sat(((k[0x1a1] * r[8]) + sum(k[0x1a0] * r[7])) * 2.0);
	double const v1 = sat(((k[0x1a6] * v0) + sum(k[0x1a2] * r[68], k[0x1a3] * r[67], k[0x1a4] * r[69], k[0x1a5] * r[70])) * 2.0);
	double const v2 = sat(((k[0x1ab] * v1) + sum(k[0x1a7] * r[70], k[0x1a8] * r[69], k[0x1a9] * r[71], k[0x1aa] * r[72])) * 2.0);
	double const v3 = sat(((k[0x1b0] * v2) + sum(k[0x1ac] * r[72], k[0x1ad] * r[71], k[0x1ae] * r[73], k[0x1af] * r[74])) * 4.0);
	s.ram[s.at(0x6d)] = clip32(sat((k[0x1b7] * m[18]) + sum(k[0x1b6] * v3)));
	double const v4 = sat(((k[0x1b5] * m[18]) + sum(k[0x1b4] * v3)) * 2.0);
	s.ram[s.at(0x6e)] = clip32(sat((k[0x1bb] * m[19]) + sum(k[0x1ba] * v4)));
	double const d5 = s.ram[s.at(0x6f)];
	double const d6 = s.ram[s.at(0x70)];
	double const v5 = sat((k[0x1c5] * m[31]) + sum(k[0x1bc] * m[22], k[0x1bd] * m[23], k[0x1be] * m[24], k[0x1bf] * m[25], k[0x1c0] * m[26], k[0x1c1] * m[27], k[0x1c2] * m[28], k[0x1c3] * m[29], k[0x1c4] * m[30]));
	s.ram[s.at(0x71)] = clip32(v5);
	double const d7 = s.ram[s.at(0x72)];
	double const d8 = s.ram[s.at(0x73)];
	double const d9 = s.ram[s.at(0x74)];
	double const d10 = s.ram[s.at(0x75)];
	double const v6 = sat((k[0x1db] * d9) + sum(k[0x1d2] * d0, k[0x1d3] * d1, k[0x1d4] * d2, k[0x1d5] * d3, k[0x1d6] * d4, k[0x1d7] * d5, k[0x1d8] * d6, k[0x1d9] * d7, k[0x1da] * d8));
	s.ram[s.at(0x76)] = clip32(v6);
	double const d11 = s.ram[s.at(0x77)];
	double const d12 = s.ram[s.at(0x78)];
	double const v7 = cell(v6);
	double const v8 = sat((k[0x1df] * d10) + sum(k[0x1de] * v7));
	s.ram[s.at(0x79)] = clip32(sat((k[0x1e7] * d11) + sum(k[0x1e6] * v8)));
	double const v9 = cell(v5);
	double const v10 = sat((k[0x1dd] * m[33]) + sum(k[0x1dc] * v9));
	s.ram[s.at(0x7a)] = clip32(sat((k[0x1eb] * d12) + sum(k[0x1ea] * v10)));
	double const v11 = sat(((k[0x1b9] * m[19]) + sum(k[0x1b8] * v4)) * 2.0);
	double const v12 = sat((k[0x1b3] * r[75]) + sum(k[0x1b1] * m[32], k[0x1b2] * m[42]));
	s.ram[s.at(0x7b)] = clip32(sat((k[0x1ef] * v12) + sum(k[0x1ee] * v11)));
	s.ram[s.at(0x7c)] = clip32(sat((k[0x1f3] * m[20]) + 0.0));
	s.ram[s.at(0x7d)] = clip32(sat((k[0x1f7] * m[21]) + 0.0));
	double const v13 = sat(((k[0x1ed] * d12) + sum(k[0x1ec] * v10)) * 2.0);
	double const v14 = sat(((k[0x1e9] * d11) + sum(k[0x1e8] * v8)) * 2.0);
	r[17] = sat(((k[0x1f2] * v13) + sum(k[0x1f1] * r[7])) * 4.0);
	r[18] = sat(((k[0x1f6] * v14) + sum(k[0x1f5] * r[8])) * 4.0);
	r[68] = r[67];
	r[70] = r[69];
	r[72] = r[71];
	r[74] = r[73];
	r[75] = v12;
	r[119] = v11;
	r[120] = v14;
	r[121] = v13;
	m[3] = in[0][13];
	m[4] = in[0][14];
	m[7] = in[1][13];
	m[42] = m[32];
	m[43] = v9;
	m[44] = v7;
	m[56] = d10;
	m[57] = d11;
	m[58] = d12;
	m[59] = d3;
	m[60] = d4;
	m[61] = d5;
	m[62] = d6;
	m[63] = d7;
	r[67] = v0;
	r[69] = v1;
	r[71] = v2;
	r[73] = v3;
}

}  // namespace fs1r::effects

// effects/var_m06.cpp - the variation window's program for Ens Detune.
//
// rom::VARIATION_PROGRAMS[6], the program the window holds for the one variation type 7, and one of the
// twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 6 to
// Ens Detune and to nothing else.
//
//   DETUNE_DET          {0x164, 0x16C, 0x173, 0x17B} the four detune words, each one a delay index
//   DETUNE_LOW_SHELF    {0x168, 0x167, 0x166}         the left channel's low shelf: b0, b1, -a1
//   DETUNE_HIGH_SHELF   {0x16B, 0x16A, 0x169}         the left channel's high shelf, the same three
//   DETUNE_LOW_SHELF_COPY  {0x177, 0x176, 0x175}      the right channel's low shelf
//   DETUNE_HIGH_SHELF_COPY {0x17A, 0x179, 0x178}      the right channel's high shelf
//   DETUNE_PAN          {0x195, 0x19B, 0x196, 0x19C}  the dry and wet pairs, left then right
//   DETUNE_SLOTS        {0x66, 0x59, 0x5A, 0x5B, 0x5C, 0x67, 0x5D, 0x5E, 0x5F, 0x60}
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, each b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and one
// of the output, the low shelf on a step that doubles its result and the high shelf on one that
// quadruples it. What comes out of them is what goes into the two delay lines.
//
// fx_modulation.cpp's ensembleDetune() writes DETUNE_DET[0] and DETUNE_DET[1] with one word and
// DETUNE_DET[2] and DETUNE_DET[3] with another, both off rom::DETUNE[], so each channel's two taps share
// one word; and DETUNE_SLOTS[0] and [5] are where each channel writes, [1] to [4] and [6] to [9] the four
// slots it reads, one delay value each for all four, so within a tap pair the two words are one sample
// apart and the 'older' bit on the second read is what makes it so. INIT_DELAY_L and INIT_DELAY_R set
// those four offsets to the base plus one plus the parameter, which puts the whole line that many
// samples behind the write.
//
//   0x158-0x163  the two LFO cells into six registers, and the three oldest delay words published.
//                m[2] and m[6] take the serial inputs for the base program; the class-3 steps 0x158 and
//                0x159 put m[8] and m[9] doubled into r[123] and r[126], 0x15d and 0x162 take their
//                absolute values into r[124] and r[1], and the four class-1 loads leave the two base
//                delays r[125] and r[2] at max(0, 64 + the LFO) and the four crossfade weights
//                max(0, |2 * the LFO| - 24) and max(0, |2 * the LFO| - 40) in r[123], r[124] on the
//                left and r[126], r[1] on the right, the same two on both sides. 0x15b, 0x15f and 0x163
//                read the slots 0x56, 0x57 and 0x58 at the ring pointer, and 0x160, 0x164 and 0x168
//                publish them into m[22], m[23] and m[24].
//   0x164        DETUNE_DET[0]: the left tap pair's index. The result is the word times m[8], and
//                t[10] takes its fraction and the delay index its floor, so the index is in samples.
//   0x165-0x16b  the left channel's input r[103] and its two order-1 shelves into r[104] and r[105]:
//                the low shelf is -a1, b1 and b0 and the high shelf the same three, on the words
//                DETUNE_LOW_SHELF and DETUNE_HIGH_SHELF. 0x167 and 0x16b also read the slots 0x59 and
//                0x5a, the first tap pair, at the index 0x164 loaded, the second one word older.
//   0x16c        DETUNE_DET[1]: the second left tap pair's index, the same word times r[125], which is
//                the first one's plus 64 of the LFO's units, so the two taps are 256 * w * 64 samples
//                apart, w the word both share. With the ROM's own 0.034943 that is 572 samples, which
//                is what the index holds with the LFO cell at zero.
//   0x16d        the right channel's second crossfade weight, max(0, |2 * m[9]| - 40), into r[1],
//                which 0x162 left at |2 * m[9]|; r[126] keeps the -24 form 0x163 left for the first.
//   0x16f-0x172  the four crossfade weights, each the register times one eighth and sixteen, into
//                t[9], t[11], t[13] and t[15]. 0x172 also publishes the LFO of its page into m[11].
//   0x173        DETUNE_DET[2]: the first right tap pair's index, the word times m[9], and the read of
//                the slot 0x5c on the same step is the second word of the first pair.
//   0x174-0x17a  the right channel's input r[106] and its two shelves into r[107] and r[108], the
//                three steps each on DETUNE_LOW_SHELF_COPY and DETUNE_HIGH_SHELF_COPY. 0x175 publishes
//                the LFO of its page into m[9]. 0x177 and 0x17b read the slots 0x5d and 0x5e, the first
//                right tap pair, at the index 0x173 loaded.
//   0x17b        DETUNE_DET[3]: the second right tap pair's index, the same word times r[2].
//   0x17c-0x17d  the first left tap: m[56] and m[57] are the slot 0x59 and 0x5a words one sample apart,
//                and t[10] is the fraction between them, so r[119] is that pair interpolated.
//   0x17f-0x180  the second left tap, the slots 0x5b and 0x5c on t[12], into r[121].
//   0x183-0x184  the left crossfade: r[119] takes t[9] of its own value and one less t[11] of r[121],
//                so the two taps are summed with the weights the LFO cells give.
//   0x186-0x187  the first right tap, the slots 0x5d and 0x5e on t[14], into r[120].
//   0x18a-0x18b  the second right tap, the slots 0x5f and 0x60 on t[7], into r[122].
//   0x18e-0x18f  the right crossfade: r[120] takes t[13] of its own value and one less t[15] of r[122].
//                0x18f also reads the slot 0x63 at the ring pointer, publishes the LFO of its page into
//                m[12], and 0x188 publishes that read into m[63].
//   0x191-0x194  the two copies. The ROM's own words are a near-unity and a zero, so r[121] ends up
//                r[119] less one part in 32768 and r[122] r[120] the same. They are copies because
//                0x19a writes r[119] again with what goes into the left line, so the left blend has to
//                be somewhere else by the time 0x196 reads it.
//   0x195-0x19f  the outputs and the two delay writes. DETUNE_PAN's four words: r[15] takes the dry word
//                on r[105] and the wet word on r[121], both quadrupled, and r[16] the same on r[108]
//                and r[122]. dryWet() gives the variation windows no dry and this type a wet word of
//                8186, so each output is 0.9993 of its tap. 0x19a latches that left output and 0x19b
//                writes it into the slot 0x66, the left line; 0x19d and 0x19e build the right one, the
//                ROM's zero words on r[119] and r[120] adding nothing to it, and 0x19f writes it into
//                the slot 0x67 itself, latching nothing.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d write
// them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005). r[103] the left input,
// r[104] and r[105] its two shelves; r[106] the right input, r[107] and r[108] its two. r[123], r[124]
// and r[125] the left LFO's three, r[126], r[1] and r[2] the right's; r[119] and r[120] the two
// interpolated and crossfaded taps, r[121] and r[122] the copies the outputs read. The cells: m[2] and
// m[6] the serial inputs this block takes for the base program, m[8] and m[9] the window's two LFOs and
// the ones 0x172 and 0x175 publish as m[11] and m[12], m[22] to m[27] the six published delay reads,
// m[56] to m[63] the eight tap words, in pair order: left, left, right, right, left, left, right, right.
// The delay slots: 0x59 to 0x5c the left line's four words and 0x5d to 0x60 the right one's, all at the
// base the host set and the delay index added to the ring pointer; 0x66 and 0x67 are where each line is
// written. Each capture takes the read five steps before it (captured()'s lower bound), so m[56] is the
// slot 0x59 word, m[57] the slot 0x5a word, m[58] the slot 0x5d word, m[59] the slot 0x5e word, m[60] the
// slot 0x5b word, m[61] the slot 0x5c word, m[62] the slot 0x5f word and m[63] the slot 0x60 word, and
// m[22] to m[27] the slots 0x56, 0x57, 0x58, 0x64, 0x65 and 0x65.
//
// The eight t registers: t[10] and t[12] the two left tap positions, t[14] and t[7] the two right, each
// the fraction below one sample of the index its step queued; t[9] and t[11] the left crossfade weights
// and t[13] and t[15] the right ones, each a register divided by 16384. A t write divides by 128, so a
// fraction is the accumulator's 256th and a weight is twice its register over 32768.
//
// How far the taps sweep is the LFO cells' range: chips/vop3.cpp's lfo() scales the wave by the depth
// and multiplies by RANGE, so a full-depth bipolar saw fills m[8] and m[9] to +-128, and the check's
// run has m[8] at 127.96. A tap's index is then 256 * w * m[8] samples, the base delay r[125] one more
// 64 of the same, and the four weights 232/16384 and 216/16384 at their largest, so the second tap
// carries almost all of each channel's output.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m06(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const v0 = pos((k[0x164] * m[8]) + 0.0);
	double const v1 = index(v0);
	double const d3 = s.ram[s.at(0x59, int(v1))];
	double const d4 = s.ram[s.at(0x5a, 1 + int(v1))];
	double const v2 = wrap((m[8] + 0.0) * 2.0);
	double const v3 = pos((128.0 * k[0x15c]) + sum(k[0x15b] * v2));
	double const v4 = pos((k[0x16c] * v3) + 0.0);
	double const v5 = index(v4);
	double const d5 = s.ram[s.at(0x5b, int(v5))];
	double const d6 = s.ram[s.at(0x5c, 1 + int(v5))];
	double const v6 = pos((k[0x173] * m[9]) + 0.0);
	double const v7 = index(v6);
	double const d7 = s.ram[s.at(0x5d, int(v7))];
	double const d8 = s.ram[s.at(0x5e, 1 + int(v7))];
	double const v8 = wrap((m[9] + 0.0) * 2.0);
	double const v9 = pos((128.0 * k[0x160]) + sum(k[0x15f] * v8));
	double const v10 = pos((k[0x17b] * v9) + 0.0);
	double const v11 = index(v10);
	double const d9 = s.ram[s.at(0x5f, int(v11))];
	double const d10 = s.ram[s.at(0x60, 1 + int(v11))];
	double const d11 = s.ram[s.at(0x63)];
	double const d12 = s.ram[s.at(0x64)];
	double const d13 = s.ram[s.at(0x65)];
	double const v12 = sat(((k[0x165] * r[5]) + 0.0) * 2.0);
	double const v13 = sat(((k[0x168] * v12) + sum(k[0x166] * r[104], k[0x167] * r[103])) * 2.0);
	double const v14 = sat(((k[0x16b] * v13) + sum(k[0x169] * r[105], k[0x16a] * r[104])) * 4.0);
	double const v15 = sat(((k[0x174] * r[6]) + 0.0) * 2.0);
	double const v16 = sat(((k[0x177] * v15) + sum(k[0x175] * r[107], k[0x176] * r[106])) * 2.0);
	double const v17 = sat(((k[0x17a] * v16) + sum(k[0x178] * r[108], k[0x179] * r[107])) * 4.0);
	double const v18 = frac(v10);
	double const v19 = sat((v18 * d10) - wrap_sum((v18 * d9) - d9));
	double const v20 = mag(v8 + 0.0);
	double const v21 = pos((128.0 * k[0x16d]) + v20);
	double const v22 = q15(sat(((k[0x172] * v21) + 0.0) * 16.0));
	double const v23 = frac(v6);
	double const v24 = pos((128.0 * k[0x163]) + v20);
	double const v25 = q15(sat(((k[0x171] * v24) + 0.0) * 16.0));
	double const v26 = sat((v25 * sat((v23 * d8) - wrap_sum((v23 * d7) - d7))) - wrap_sum((v22 * v19) - v19));
	double const v27 = frac(v4);
	double const v28 = sat((v27 * d6) - wrap_sum((v27 * d5) - d5));
	double const v29 = mag(v2 + 0.0);
	double const v30 = pos((128.0 * k[0x161]) + v29);
	double const v31 = q15(sat(((k[0x170] * v30) + 0.0) * 16.0));
	double const v32 = frac(v0);
	double const v33 = pos((128.0 * k[0x15e]) + v29);
	double const v34 = q15(sat(((k[0x16f] * v33) + 0.0) * 16.0));
	double const v35 = sat((v34 * sat((v32 * d4) - wrap_sum((v32 * d3) - d3))) - wrap_sum((v31 * v28) - v28));
	s.ram[s.at(0x66)] = clip32(sat((k[0x19a] * v35) + sum(k[0x197] * v14, k[0x198] * v17, k[0x199] * v26)));
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v26) + sum(k[0x19d] * v17, k[0x19e] * v35)));
	double const v36 = sat((k[0x192] * v26) + sum(k[0x191] * v35));
	double const v37 = sat((k[0x194] * v26) + sum(k[0x193] * v35));
	r[1] = v21;
	r[2] = v9;
	r[15] = sat(((k[0x196] * v36) + sum(k[0x195] * v14)) * 4.0);
	r[16] = sat(((k[0x19c] * v37) + sum(k[0x19b] * v17)) * 4.0);
	r[103] = v12;
	r[104] = v13;
	r[105] = v14;
	r[106] = v15;
	r[107] = v16;
	r[108] = v17;
	r[119] = v35;
	r[120] = v26;
	r[121] = v36;
	r[122] = v37;
	r[123] = v33;
	r[124] = v30;
	r[125] = v3;
	r[126] = v24;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[9] = lfo[11];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d11;
	m[26] = d12;
	m[27] = d13;
	m[56] = d3;
	m[57] = d4;
	m[58] = d7;
	m[59] = d8;
	m[60] = d5;
	m[61] = d6;
	m[62] = d9;
	m[63] = d10;
	t[7] = v18;
	t[9] = v34;
	t[10] = v32;
	t[11] = v31;
	t[12] = v27;
	t[13] = v25;
	t[14] = v23;
	t[15] = v22;
}

}  // namespace fs1r::effects

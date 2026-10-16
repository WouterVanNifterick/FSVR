// effects/var_m09.cpp - the variation window's program for Karaoke.
//
// rom::VARIATION_PROGRAMS[9], the program the variation window holds under variation type 24, which
// fx_dispatch.cpp routes to EffectHost::karaoke(). It is one of the twenty-nine programs the window
// shares: nothing here knows which type is running, because the program words are fixed and only the
// constants and the delay offsets move, which fx_delay.cpp writes whenever a parameter does. The code
// reads them where they live on the pass, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in these groups:
//
//   0x158-0x15f  the two serial inputs and the host's two tone filters, var::KARAOKE_HPF (0x15a, 0x15b,
//                0x15c) and var::KARAOKE_LPF (0x15d, 0x15e, 0x15f), one pole each, three words apiece.
//                The step that sums the two inputs is 0x159: r[103] = 2 * (k * r[6] + k * r[5]), which
//                at the shipped constants is half the sum, so the whole chain runs on the mono. The
//                high-pass's state is r[104] and the low-pass's r[105], the low-pass fed from the
//                high-pass. The two reads that ride on the filter steps (the slots 0x56 and 0x57) are
//                published in m[28] and m[29].
//   0x160-0x16d  the published reads and the first two row taps: the reads of the slots 0x58, 0x59 and
//                0x5A, published in m[30], m[60] and m[61]. 0x167 also takes the page's LFO into m[11].
//   0x16e-0x173  the first of the four row units, the pair var::KARAOKE_ROW_STEPS[0..3] over the slot
//                0x59: 0x16f reads the line (m[60], the read of 0x167) and 0x171 writes it through the
//                latch, which 0x173 spends on the slot 0x5C.
//   0x174-0x177  the second row unit, var::KARAOKE_ROW_STEPS[4..7] over the slot 0x5A: 0x175 reads the
//                line (m[61], the read of 0x16b) and 0x177 writes the slot 0x5D itself.
//   0x178-0x183  the main line. 0x176 takes up the read of 0x16f, the slot 0x5B, which is where
//                EffectHost::karaoke() puts the delay time: var::KARAOKE_SLOTS[1] is set to
//                var::KARAOKE_SLOTS[0]'s plus PITCH_DELAY. 0x178 to 0x17a fold that tap into m[55]
//                through var::KARAOKE_LEVEL, the feedback level, and 0x17d adds it to r[120] and latches
//                the sum, which 0x183 spends on the slot 0x60, var::KARAOKE_SLOTS[0], the line's own
//                write. The two taps at 0x17b and 0x17c are the line's output into the two row units
//                below, half each.
//   0x184-0x18b  the third and fourth row units, var::KARAOKE_ROW_STEPS[8..15] over the slots 0x5E and
//                0x5F: 0x185 reads the third line's tap (m[60], the read of 0x17b) and 0x187 writes the
//                slot 0x61, and 0x189 reads the fourth line's tap (m[61], the read of 0x17f) and 0x18b
//                writes the slot 0x62. 0x187 also takes the page's LFO into m[12].
//   0x18c-0x18f  the two outputs, the dry/wet pan var::KARAOKE_PAN: r[15] = 4 * (k * r[120] + k * r[5])
//                and r[16] = 4 * (k * r[121] + k * r[6]), the row chain's two sums and the two inputs.
//                0x18f reads the slot 0x63 on the way past.
//   0x190-0x19f  the tail: the reads of the slots 0x64 and 0x65 published in m[32] and m[33], and the
//                read of the slot 0x63 published in m[31] at 0x194, five steps after the read each time,
//                which is captured()'s lower bound. The rest of the steps are idle but for the z^-1
//                latch.
//
// Which units the row is made of, and what each word of a unit carries, is EffectHost::diffusers()'s
// business: it writes four words per unit (s28, s24, s26, s22) at var::KARAOKE_ROW_STEPS and the delay
// offsets at var::KARAOKE_ROW_SLOTS, which is the table {0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62, 0x5F},
// a write slot and a read slot for each of the four units. What each step adds up to, with k the word
// that step carries:
//
//   r[103]        0x159       = 2 * (k159 * r[6] + k158 * r[5])              the mono sum
//   r[104]        0x15c       = 2 * (k15c * r[103] + k15b * r[103] + k15a * r[104])
//   r[105]        0x15f       = 2 * (k15f * r[104] + k15e * r[104] + k15d * r[105])
//   line 0x5C     0x171       = k171 * line(0x59) + k170 * r[105]             row 1
//   r[120]        0x16f       = 2 * (k16f * line(0x59) + k16e * r[105])
//   r[120]        0x175       = 2 * (k175 * line(0x5A) + k174 * r[120])      row 2
//   line 0x5D     0x177       = k177 * line(0x5A) + k176 * r[120]
//   m[55]         0x17a       = k17a * m[55] + k179 * line(0x5B) + k178 * line(0x5B)
//   line 0x60     0x17d/0x183 = k17d * r[120] + m[55]                        the main line's write
//   r[120]        0x17b       = 2 * k17b * line(0x5B)   and r[121] at 0x17c = 2 * k17c * line(0x5B)
//   r[120]        0x185       = 2 * (k185 * line(0x5E) + k184 * r[120])      row 3
//   line 0x61     0x187       = k187 * line(0x5E) + k186 * r[120]
//   r[121]        0x189       = 2 * (k189 * line(0x5F) + k188 * r[121])      row 4
//   line 0x62     0x18b       = k18b * line(0x5B) + k18a * r[121]
//   r[15]         0x18d       = 4 * (k18d * r[120] + k18c * r[5])            the pan
//   r[16]         0x18f       = 4 * (k18f * r[121] + k18e * r[6])
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three
// steps allows, which is why the sums above read r[120] where the reader stands before the writer's
// write has landed. The old model in vop3_effects.h's run_delay() has the Karaoke case as one line, a
// feedback off its own tap and a second tap one delay further on, and is no reference for any number
// here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input, r[104] and r[105] the two
// tone-filter poles; r[120] and r[121] the two sums the pan reads. The cells: m[2] and m[6] the two
// serial inputs this block takes for the base program, m[11] and m[12] the two LFO values the window
// publishes, m[28], m[29] and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads,
// m[54] the main line's tap, m[55] the feedback off it, and m[60] and m[61] the row taps, which each
// cell takes twice a pass, once for the row that feeds the line and once for the row that comes off it.
// The delay slots: 0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62 and 0x5F the four row lines and their
// offsets, 0x5B and 0x60 the main line's tap and its write, 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the
// reads the base program and the other windows share. No step here reads at a delay index and none
// writes the t file, so no step of this program is fractional and none takes a multiplier from a t
// register.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m09(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x56)];
	double const d1 = s.ram[s.at(0x57)];
	double const d2 = s.ram[s.at(0x58)];
	double const d3 = s.ram[s.at(0x59)];
	double const d4 = s.ram[s.at(0x5a)];
	double const d5 = s.ram[s.at(0x5b)];
	double const v0 = sat(((k[0x159] * r[6]) + sum(k[0x158] * r[5])) * 2.0);
	double const v1 = sat(((k[0x15c] * v0) + sum(k[0x15a] * r[104], k[0x15b] * r[103])) * 2.0);
	double const v2 = sat(((k[0x15f] * v1) + sum(k[0x15d] * r[105], k[0x15e] * r[104])) * 2.0);
	s.ram[s.at(0x5c)] = clip32(sat((k[0x171] * d3) + sum(k[0x170] * v2)));
	double const v3 = sat(((k[0x16f] * d3) + sum(k[0x16e] * v2)) * 2.0);
	s.ram[s.at(0x5d)] = clip32(sat((k[0x177] * d4) + sum(k[0x176] * v3)));
	double const d6 = s.ram[s.at(0x5e)];
	double const d7 = s.ram[s.at(0x5f)];
	double const v4 = cell(sat((k[0x17a] * m[55]) + sum(k[0x178] * m[54], k[0x179] * d5)));
	s.ram[s.at(0x60)] = clip32(sat((k[0x17d] * sat(((k[0x175] * d4) + sum(k[0x174] * v3)) * 2.0)) + v4));
	double const v5 = sat(((k[0x17b] * d5) + 0.0) * 2.0);
	s.ram[s.at(0x61)] = clip32(sat((k[0x187] * d6) + sum(k[0x186] * v5)));
	double const v6 = sat(((k[0x17c] * d5) + 0.0) * 2.0);
	s.ram[s.at(0x62)] = clip32(sat((k[0x18b] * d6) + sum(k[0x18a] * v6)));
	double const d8 = s.ram[s.at(0x63)];
	double const d9 = s.ram[s.at(0x64)];
	double const d10 = s.ram[s.at(0x65)];
	double const v7 = sat(((k[0x185] * d6) + sum(k[0x184] * v5)) * 2.0);
	double const v8 = sat(((k[0x189] * d7) + sum(k[0x188] * v6)) * 2.0);
	r[15] = sat(((k[0x18d] * v7) + sum(k[0x18c] * r[5])) * 4.0);
	r[16] = sat(((k[0x18f] * v8) + sum(k[0x18e] * r[6])) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[120] = v7;
	r[121] = v8;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d8;
	m[32] = d9;
	m[33] = d10;
	m[54] = d5;
	m[55] = v4;
	m[60] = d6;
	m[61] = d7;
}

}  // namespace fs1r::effects

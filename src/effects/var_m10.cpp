// effects/var_m10.cpp - the variation window's program for the reverbs.
//
// rom::VARIATION_PROGRAMS[10], the program the variation window holds under the four variation types
// 25 to 28, which fx_dispatch.cpp routes to EffectHost::variationReverb(): Hall, Room, Stage and Plate.
// It is one of the twenty-nine programs the window shares, so nothing here knows which of the four is
// running. The program words are fixed and only the constants and the delay offsets move, which
// fx_reverb.cpp writes whenever a parameter does, so the code reads them where they live on the pass,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
//   0x158-0x15f  the two serial inputs, the mono sum and the host's two tone filters, the high-pass
//                var::REVERB_HPF and the low-pass var::REVERB_LPF, one pole each and three words apiece.
//                0x159 is the mono sum r[103] = 2 * (k * r[6] + k * r[5]), so the whole reverb runs on
//                the mono. The high-pass's state is r[104], the low-pass's r[105], the low-pass fed from
//                the high-pass. The reads of the slots 0x56 and 0x57 ride on the high-pass's second word
//                and the low-pass's third, and are published in m[28] and m[29].
//   0x160-0x164  m[28] takes up the read of 0x15b, the read of the slot 0x58 rides on 0x163, and m[29]
//                takes up the read of 0x15f. 0x164 is var::REVERB_W[5]: r[119] = k * r[118], the level
//                the diffusion parameter picks, on the base program's own return r[118].
//   0x165-0x166  r[109] = 2 * (k * m[55] + k * r[105]), the pre-delay tap published for the base
//                program, and 0x166 passes it on. m[55] is m[57] as 0x191 left it a pass back.
//   0x167-0x16e  nine rcopy steps that carry no arithmetic, only the z^-1 copies r[110] to r[117]. They
//                hold the reads of the slots 0x59 and 0x5A, publish m[30] (the read of 0x58) and m[56]
//                (the read of 0x59), and take the page's LFO into m[11].
//   0x16f-0x177  the second diffusion unit, var::REVERB_ROW_STEPS[4..7] over the slot 0x5C, and the
//                pre-delay. 0x16f reads the slot 0x5B and m[58] takes it up five steps later; 0x170
//                to 0x173 are the unit's four words, and 0x173 writes the slot 0x5C itself. 0x177 is
//                var::REVERB_WIDTH[0], the FB_LEVEL width: the slot 0x5D takes the unit's tap r[120]
//                mixed with the pre-delay tap m[57] behind it.
//   0x178-0x183  the first diffusion unit, var::REVERB_ROW_STEPS[0..3], driven by the pre-delay tap
//                m[57] rather than by the input. 0x179 publishes the unit's tap in m[63], 0x17b latches
//                the line's own write and 0x183 spends it on the slot 0x60, the unit's write. The read
//                of the slot 0x5E rides on 0x17b.
//   0x184-0x189  the two damping lowpasses, the (x, x, a) triples var::REVERB_COMBS[1..3] and
//                [5..7]: r[106] off r[107], and r[108] off the early reflection m[60]. 0x187 reads the
//                slot 0x61 and takes the page's LFO into m[12].
//   0x18a-0x18d  m[61] takes up the read of the slot 0x61, five steps after 0x187. 0x18a is a dead step:
//                its M_PREVIOUS write of m[54] is on a class-0 step, which step() returns before, so
//                nothing ever writes m[54] and 0x188 reads it as zero.
//   0x18e-0x193  the two output sums, var::REVERB_W[9] and [8] into r[120] and var::REVERB_W[6] and [7]
//                into r[121], each summing two early reflections and one level term. 0x191 is
//                var::REVERB_W[0] and copies m[57] into m[55], so m[55] is the pre-delay tap a pass
//                back; 0x190 publishes the read of the slot 0x62 in m[62].
//   0x194-0x198  m[31], m[32] and m[33] take up the reads of the slots 0x63, 0x64 and 0x65 for the base
//                program and the other windows, and the pan var::REVERB_PAN scales the two sums into
//                r[15] and r[16]: r[15] = 4 * (k * r[121] + k * r[5]) and r[16] = 4 * (k * r[120] + k * r[6]).
//                At the shipped constants k is a quarter and the two inputs carry nothing, so each
//                output is its own sum.
//   0x199-0x19f  the two comb gains, var::REVERB_COMBS[0] and [4]: the slot 0x66 takes the unit's tap
//                m[63] times its gain plus the first damper's state r[106], the slot 0x67 the same tap
//                plus the second damper's r[108]. m[33] takes up the read of the slot 0x65 on the way past.
//                The slots 0x66 and 0x67 are the bases the early reflections hang off, so combDelays2()
//                sets 0x5E and 0x61 from 0x66 and 0x5F and 0x62 from 0x67, each pair the same offset and
//                the second shorter by the diffusion.
//
//   tap        0x179 / 0x171   = 2 * (s24 * line(L) + s28 * x), saturating at +-128, into m[63] / r[120]
//   write      0x17b / 0x173   = clip(2 * (s22 * line(L) + s26 * x), +-32), into the unit's write slot
//   with x = m[57] and L = the slot 0x5B for the first unit, and x = r[105] and L = the slot 0x59 for
//   the second. The two damping lowpasses are one poles each, y = 2 * (x * in + a * y_prev) over three
//   words apiece, and the two comb gains are the RT60 gains the damping loss scales.
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three steps
// allows, which is why the sums above read r[120] where the reader stands before the writer's write has
// landed. The old model in vop3_effects.h's run_delay() has a single Schroeder tail with eight combs and
// four all-passes for every reverb, and is no reference for any number or for any shape here: this program
// runs two units, one driven by the other's tap through the initial delay, and four early reflections.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input; r[104] and r[105] the two
// tone-filter poles; r[106], r[107] and r[108] the two damping lowpasses and the first one's input;
// r[109] the pre-delay return this window hands the base program; r[118] the base program's own return
// and r[119] its level; r[120] the second unit's tap and the right output sum, r[121] the left one. The
// cells: m[2] and m[6] the two serial inputs this block takes, m[11] and m[12] the two LFO values the
// window publishes, m[28] to m[30] and m[31] to m[33] the six reads the base program and the other
// windows share, m[54] the cell nothing writes, m[55] the pre-delay tap a pass back, m[56] and m[58] the
// two units' lines, m[57] the pre-delay tap, m[59] the cell that stays at zero, m[60] to m[62] the three
// early reflections the outputs sum, and m[63] the first unit's tap. The delay slots: 0x5C and 0x60 the
// two units' write slots, 0x59 and 0x5B their read slots, 0x5D and 0x5A the pre-delay's write and read,
// 0x66 and 0x67 the early reflections' two base offsets with 0x5E, 0x61, 0x5F and 0x62 the four taps off
// them, and 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the reads the base program shares. No step here reads
// at a delay index, none writes the t file with its own result and none sets the compare flag, so no
// step of this program is fractional, no step takes a multiplier from a t register and no step branches.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m10(State& s)
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
	double const v1 = sat((k[0x15c] * v0) + sum(k[0x15a] * r[104], k[0x15b] * r[103]));
	double const v2 = sat(((k[0x15f] * v1) + sum(k[0x15d] * r[105], k[0x15e] * r[104])) * 2.0);
	s.ram[s.at(0x5c)] = clip32(sat((k[0x173] * d3) + sum(k[0x172] * v2)));
	s.ram[s.at(0x5d)] = clip32(sat((k[0x177] * d4) + sat(((k[0x171] * d3) + sum(k[0x170] * v2)) * 2.0)));
	double const d6 = s.ram[s.at(0x5e)];
	double const d7 = s.ram[s.at(0x5f)];
	s.ram[s.at(0x60)] = clip32(sat((k[0x17b] * d5) + sum(k[0x17a] * d4)));
	double const d8 = s.ram[s.at(0x61)];
	double const d9 = s.ram[s.at(0x62)];
	double const d10 = s.ram[s.at(0x63)];
	double const d11 = s.ram[s.at(0x64)];
	double const d12 = s.ram[s.at(0x65)];
	double const v3 = sat(((k[0x183] * d6) + 0.0) * 2.0);
	double const v4 = sat(((k[0x186] * v3) + sum(k[0x184] * r[106], k[0x185] * r[107])) * 2.0);
	double const v5 = cell(sat(((k[0x179] * d5) + sum(k[0x178] * d4)) * 2.0));
	s.ram[s.at(0x66)] = clip32(sat((k[0x19b] * v5) + v4));
	double const v6 = sat(((k[0x189] * d7) + sum(k[0x187] * r[108], k[0x188] * m[54])) * 2.0);
	s.ram[s.at(0x67)] = clip32(sat((k[0x19f] * v5) + v6));
	double const v7 = sat(((k[0x193] * d9) + sum(k[0x191] * m[55], k[0x192] * d6)) * 2.0);
	double const v8 = sat((k[0x164] * r[118]) + sum(k[0x160] * m[55], k[0x161] * r[110], k[0x162] * r[112], k[0x163] * r[115]));
	double const v9 = sat(((k[0x18f] * d8) + sum_on(v8, k[0x18e] * d7)) * 2.0);
	r[15] = sat(((k[0x196] * v7) + sum(k[0x195] * r[5])) * 4.0);
	r[16] = sat(((k[0x198] * v9) + sum(k[0x197] * r[6])) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v4;
	r[107] = v3;
	r[108] = v6;
	r[117] = r[116];
	r[119] = v8;
	r[120] = v9;
	r[121] = v7;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[11];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d10;
	m[32] = d11;
	m[33] = d12;
	m[56] = d3;
	m[57] = d4;
	m[58] = d5;
	m[59] = d6;
	m[60] = d7;
	m[61] = d8;
	m[62] = d9;
	m[63] = v5;
	r[116] = r[115];
	r[115] = r[114];
	r[114] = r[113];
	r[113] = r[112];
	r[112] = r[111];
	r[111] = r[110];
	r[110] = r[109];
	r[109] = sat(((k[0x165] * m[55]) + 0.0) * 2.0);
	m[55] = d4;
}

}  // namespace fs1r::effects

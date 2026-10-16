// effects/var_m08.cpp - the variation window's program for the four delay types.
//
// rom::VARIATION_PROGRAMS[8], the program the window holds for the four variation types 20 to 23, and
// one of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives
// msb 8 to EffectHost::delayLCR (lsb 1, type 20), delayLR (lsb 2, type 21), echo (lsb 3, type 22) and
// crossDelay (lsb 4, type 23). Which of the four is running is not a code difference here: the program
// words are the same under all of them and only the constants and the six delay offsets differ, which
// fx_delay.cpp writes when a parameter moves. The code reads them where they live all the same, as
// step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in these groups:
//
// Which of the four types is running is decided by the constants and the six offsets alone. What the
// tail of the program adds up to, each term as the dump's coefficients leave it, with k standing for
// the word that step carries and the letters for the taps the type's own offsets give:
//
//   r[109] r[110]  the first two taps, m[56] and m[57]
//   r[119] r[120]  the same two through the damp pair, which sums to one under every type here
//                  (0x31c7 + 0x4e39 and 0x238e + 0x5c72 are both 0x8000), so the pair is a copy and
//                  what it carries is the tap, not a fraction of it
//   0x17b  the left line   = k178 * r[105] + k179 * r[108]      the two damped inputs
//                           + k17a * r[119] + k17b * r[120]     the feedback off the first two taps
//   0x183  the right line  = k180 * r[120] + k181 * r[119] + k17f * r[108], latched at 0x181
//   r[124] = k17c * m[56] + k17d * m[58] + k17e * m[59]      the left wet sum, three taps
//   r[125] = k182 * m[57] + k183 * m[59] + k184 * m[58]      the right wet sum, the same three taps
//   r[15]  = 4 * 0.25 * r[124]  and r[16] = 4 * 0.25 * r[125], so the two outputs are the two sums
//
//   type 20 Delay L,C,R   k178 = k179 0.5, k17a 0.153, k17b 0   one feedback, the mono sum in one line
//                          r[124] = LCH + 0.62 * CCH, r[125] = RCH + 0.62 * CCH, and all four taps off
//                          that one line, so the right line carries nothing and is written zero
//   type 21 Delay L,R     k178 = k179 0.5, k17a = k17b 0.175     two feedbacks, each half the level
//                          r[124] = LCH, r[125] = RCH
//   type 22 Echo          k178 1.0, k179 0, k17a 0.336           each channel into its own line, each
//                          fed one feedback off its own first tap
//                          r[124] = L1 + 0.238 * L2, r[125] = R1 + 0.238 * R2
//   type 23 Cross Delay   k178 = k179 0.5, k17a 0, k17b 0.366   the mono sum into one line and one
//                          feedback off the other line's tap; the third and fourth taps sit one sample
//                          behind the right line's write, so they read that line a pass back
//                          r[124] = m[56] + 0.72 * m[59], r[125] = m[57]
//
// which tap each type means by a cell is its delay offsets and not this file: var::DELAY_SLOTS[1] is
// the first feedback (L,C,R), the second feedback (L,R), the first left tap (Echo) or the right-to-left
// delay (Cross Delay), and so on down the list. The old model in vop3_effects.h's run_delay() has the
// same shape, two lines and a handful of taps, and is no reference for any number here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] and r[106] the two inputs after the step
// that scales them, r[104] and r[105] the left channel's two damp poles and r[107] and r[108] the
// right channel's; r[109] and r[110] the first two taps, r[119] and r[120] those two through the damp
// pair; r[124] and r[125] the two wet sums. The cells: m[2] and m[6] the two serial inputs this block
// takes for the base program, m[11] and m[12] the two LFO values the window publishes, m[28], m[29]
// and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads, and m[56] to m[59] the
// four delay taps, in step order: the slots 0x59, 0x5A, 0x5B and 0x5C. The delay slots: 0x56, 0x57 and
// 0x58 read what the base program published, and 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x60, 0x63, 0x64 and
// 0x65 are the block's own area, which is where the base program's INIT_DELAY_* offsets put them. No
// step here reads at a delay index and none writes the t file, so no step of this program is
// fractional and none takes a multiplier from a t register.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void var_m08(State& s)
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
	double const d6 = s.ram[s.at(0x5c)];
	double const v0 = sat(((k[0x158] * r[5]) + 0.0) * 2.0);
	double const v1 = sat(((k[0x15b] * v0) + sum(k[0x159] * r[104], k[0x15a] * r[103])) * 2.0);
	double const v2 = sat(((k[0x15e] * v1) + sum(k[0x15c] * r[105], k[0x15d] * r[104])) * 4.0);
	double const v3 = sat(((k[0x15f] * r[6]) + 0.0) * 2.0);
	double const v4 = sat(((k[0x162] * v3) + sum(k[0x160] * r[107], k[0x161] * r[106])) * 2.0);
	double const v5 = sat(((k[0x165] * v4) + sum(k[0x163] * r[108], k[0x164] * r[107])) * 4.0);
	double const v6 = wrap(d3 + 0.0);
	double const v7 = sat((k[0x173] * v6) + sum(k[0x172] * r[109]));
	double const v8 = wrap(d4 + 0.0);
	double const v9 = sat((k[0x177] * v8) + sum(k[0x176] * r[110]));
	s.ram[s.at(0x5e)] = clip32(sat((k[0x17b] * v9) + sum(k[0x178] * v2, k[0x179] * v5, k[0x17a] * v7)));
	s.ram[s.at(0x60)] = clip32(sat((k[0x181] * v7) + sum(k[0x17f] * v5, k[0x180] * v9)));
	double const d7 = s.ram[s.at(0x63)];
	double const d8 = s.ram[s.at(0x64)];
	double const d9 = s.ram[s.at(0x65)];
	double const v10 = sat((k[0x17e] * d6) + sum(k[0x17c] * d3, k[0x17d] * d5));
	double const v11 = sat((k[0x184] * d5) + sum(k[0x182] * d4, k[0x183] * d6));
	r[15] = wrap(((k[0x186] * v10) + sum(k[0x185] * v2)) * 4.0);
	r[16] = wrap(((k[0x188] * v11) + sum(k[0x187] * v5)) * 4.0);
	r[103] = v0;
	r[104] = v1;
	r[105] = v2;
	r[106] = v3;
	r[107] = v4;
	r[108] = v5;
	r[109] = v6;
	r[110] = v8;
	r[119] = v7;
	r[120] = v9;
	r[124] = v10;
	r[125] = v11;
	m[2] = in[0][10];
	m[6] = in[1][10];
	m[11] = lfo[10];
	m[12] = lfo[12];
	m[28] = d0;
	m[29] = d1;
	m[30] = d2;
	m[31] = d7;
	m[32] = d8;
	m[33] = d9;
	m[56] = d3;
	m[57] = d4;
	m[58] = d5;
	m[59] = d6;
}

}  // namespace fs1r::effects

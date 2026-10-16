// effects/var_m07.cpp - the variation window's program for 3-Band EQ.
//
// rom::VARIATION_PROGRAMS[13], the program the window holds for the one variation type 13, and one of the
// twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 13 to
// 3-Band EQ and to nothing else.
//
//   EQ3_LOW_SHELF2         {0x15E,0x15A,0x15B,0x15C,0x15D}  the left channel's low band: b0, b1, b2, -a1, -a2
//   EQ3_PEAK               {0x163,0x15F,0x160,0x161,0x162}  the left channel's middle band, the same five
//   EQ3_HIGH_SHELF2        {0x168,0x164,0x165,0x166,0x167}  the left channel's high band, the same five
//   EQ3_LOW_SHELF2_COPY    {0x16E,0x16A,0x16B,0x16C,0x16D}  the right channel's low band
//   EQ3_PEAK_COPY          {0x173,0x16F,0x170,0x171,0x172}  the right channel's middle band
//   EQ3_HIGH_SHELF2_COPY   {0x178,0x174,0x175,0x176,0x177}  the right channel's high band
//   EQ3_A                  {0x158,0x159,0x169}               the two words that sum the left input, and the right one
//   EQ3_B                  {0x179,0x17A,0x17B}               the two outputs and the word between them
//
// fx_tone.cpp's threeBandEq() designs each band with designFilter(2, 1, ...) for the two lower bands and
// designFilter(2, 4, ...) for the high one, which writes the five words b0, b1, b2, -a1 and -a2 into
// the list above and copies them to the other channel's list. Each word is the designed coefficient
// times 2048 * (16 >> scale), which the chip's s16() halves, and the b0 step's own route multiplies the
// whole section it closes, so the five words stand for the coefficients as designed: a shift of one on
// 0x15E, 0x163, 0x16E and 0x173, and a shift of three on 0x168 and 0x178, both restore the factor the
// designer's scale took out. What the section computes is then the difference equation
//
//   y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] + (-a1) y[n-1] + (-a2) y[n-2]
//
// The type's own preset holds all three gains at 0 dB, and then each band's numerator word pair is the
// same polynomial as its denominator pair: the left low band 1.0000, -1.949341, +0.950012 against the
// same 1.949341 and -0.950012 on its own output, the middle 1.0000, -1.952026, +0.971985, and the high
// 1.0000, -0.437500, +0.047852 (all off the dump's k values with each band's route applied). Each
// section is therefore all-pass, the cascade is transparent, and the window passes the summed input
// through unchanged until a band is moved. INFERRED from those words, not measured.
//
// The MODE parameter (index 14) is the stereo switch, and it is the only one that touches EQ3_A and
// EQ3_B. In mono it puts a quarter on both of the summing words and leaves the right output's word at
// zero; in stereo it halves the first, zeroes the second and gives the right output its share:
//
//   stereo  coef[0x158] = HALF     coef[0x159] = 0        coef[0x169] = HALF
//           r[103] = 2 * (0 * R + HALF * L) = L, r[111] = 2 * HALF * R = R,
//           coef[0x179] = x  coef[0x17A] = 0     coef[0x17B] = x
//           r[15] = 4 x r[109] and r[16] = 4 (x r[117] + 0), one chain each
//   mono    coef[0x158] = QUARTER  coef[0x159] = QUARTER  coef[0x169] = HALF
//           r[103] = 2 * (0.25 R + 0.25 L) = (L + R) / 2, r[111] = 2 * 0.5 * R = R,
//           coef[0x179] = x  coef[0x17A] = x     coef[0x17B] = 0
//           r[15] = 4 x r[109] and r[16] = 4 (0 + x r[109]), so both outputs are the left chain's
//
// and x is typeGain(VARIATION, QUARTER), the type's own level, the same word on both output steps. So in
// mono the right chain still runs and its output register r[117] is multiplied by zero. The type's own
// preset holds the mono words (QUARTER, QUARTER, HALF at 0x158, 0x159, 0x169, and zero at 0x17B), which
// is what the checker runs it under.
//
//   0x158-0x159  EQ3_A. m[2] and m[6] take the serial inputs for the base program, and the two words
//                sum the left input into r[103]: the first into the running value, the second doubling
//                that plus a quarter of the right one. 0x158 has no adder input and no route, so it is
//                the left input alone into the accumulator.
//   0x15a-0x168  the left channel's three bands, one after the other, each on the last band's output.
//   0x169        EQ3_A[2]: the right input into r[111] doubled, with no adder input.
//   0x16a-0x178  the right channel's three bands, the same six steps on the copy.
//   0x179-0x17b  EQ3_B. 0x179 quadruples its word on r[109] into the left output r[15] with no adder
//                input, 0x17a leaves the same word on r[109] in the accumulator without a route, and
//                0x17b adds its own word on r[117] to that and quadruples the sum into r[16].
//   0x17c-0x19f  the tail, class 0 throughout and idle but for the three reads and the four published
//                cells: the slots 0x63, 0x64 and 0x65 read at 0x18f, 0x193 and 0x197, the captures at
//                0x194, 0x198 and 0x19c that take them into m[31], m[32] and m[33] five steps later,
//                which is captured()'s lower bound, and the LFO of page 12 at 0x186 into m[12].
//
//   b1  the word on the band's input, with no adder, which opens the sum
//   b2  the word on the z^-1 copy of that input, which the same step writes (w1[7]), added to the sum
//   -a1 the word on the band's own output a pass back
//   -a2 the word on its second output delay, the copy the next band writes
//   b0  the word on the band's input again, closing the sum into the output with the band's route
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d
// write them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005). r[103] the summed
// left input, r[105] the left low band, r[107] the left middle band, r[109] the left high band and the
// left output; r[111] the right input, r[113] the right low band, r[115] the right middle band, r[117]
// the right high band and the right output. Four registers per channel hold the section delays, each one
// written by an rcopy step that copies the value the step before it latched, and read by the b2 step of
// the same section on the next pass:
//
//   r[104] the summed input a pass older, the left low band's b2; r[112] the right input a pass older,
//   the right low band's b2. r[106] the left low band's output a pass older, the left middle band's b2,
//   and two passes older it is that band's -a2; r[114] the same two on the right. r[108] the left
//   middle band's output a pass older, the left high band's b2, and its -a2 two passes back; r[116]
//   the same on the right. r[110] and r[118] the two high bands' outputs a pass older, their own b2.
//
// The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[11] and m[12] the
// two LFOs the window publishes, m[28] to m[33] the six published delay reads, in step order: the slots
// 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65.
// The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring pointer, which is where
// the base program's INIT_DELAY_* offsets put them. Nothing here writes the delay RAM and nothing reads
// at a delay index, so no band is a delay line and no step in the program is a tap: the variation
// window's 3-Band EQ is three cascaded order-2 sections per channel and no memory at all.

#include "effects/programs.h"

namespace fs1r::effects {

namespace {

using word::sat;
using word::sum;

// One channel's three bands, cascaded, in direct form I. `w` is the channel's fifteen words, five a band in
// step order (b1, b2, -a1, -a2, b0); `h` its eight history registers: the input's last two samples, then
// each band's last two outputs, which are also the next band's input history. The high band's route (x16)
// restores the scale its designer took out (designFilter(2, 4, ...)); the lower two are designed at x2.
double eq3(double const* w, double* h, double x)
{
	constexpr double ROUTE[3] = {2.0, 2.0, 16.0};
	double y[3];
	double in = x;
	for (int b = 0; b < 3; b++) {
		double const* c = w + 5 * b;
		double const* z = h + 2 * b;
		in = y[b] = sat((c[4] * in + sum(c[0] * z[0], c[1] * z[1], c[2] * z[2], c[3] * z[3])) * ROUTE[b]);
	}
	// every history pair moves one sample on
	for (int b = 3; b-- > 0;) { h[2 * b + 3] = h[2 * b + 2]; h[2 * b + 2] = y[b]; }
	h[1] = h[0];
	h[0] = x;
	return in;
}

}  // namespace

void var_m07(State& s)
{
	auto& r = s.r;
	auto const& k = s.k;

	// The window moves cells for the other programs: two serial inputs, two LFOs and six delay taps.
	constexpr int TAPS[6] = {0x56, 0x57, 0x58, 0x63, 0x64, 0x65};
	double tap[6];
	for (int i = 0; i < 6; i++) tap[i] = s.ram[s.at(TAPS[i])];

	// The inputs: left is EQ3_A's mix of the two sends (mono: their average), right the right send
	double const left = sat((k[0x159] * r[6] + sum(k[0x158] * r[5])) * 2.0);
	double const right = sat((k[0x169] * r[6] + 0.0) * 2.0);

	double const l = eq3(&k[0x15a], &r[103], left);
	double const rr = eq3(&k[0x16a], &r[111], right);

	// EQ3_B: the two outputs at the type's level; in mono both carry the left chain
	r[15] = sat((k[0x179] * l + 0.0) * 4.0);
	r[16] = sat((k[0x17b] * rr + sum(k[0x17a] * l)) * 4.0);

	s.m[2] = s.in[0][10];
	s.m[6] = s.in[1][10];
	s.m[11] = s.lfo[11];
	s.m[12] = s.lfo[12];
	for (int i = 0; i < 6; i++) s.m[28 + i] = tap[i];
}

}  // namespace fs1r::effects

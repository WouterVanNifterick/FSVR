// fs1r/native/var_m07.cpp - VOP3-2, the variation window's program for 3-Band EQ: its 72 steps.
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
// one order-2 section per band, each on the previous band's output: three cascaded sections per channel,
// and nothing else in the program. BANDS below is those six sections, one row each.
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
// The 72 steps, in the five groups the loop below splits them into:
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
// Each band's five steps, which band_step() runs in this order:
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
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m07: identical, 1 type, 9600 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// One band, in direct form I. `in` is the register its input is in, which b1 and b0 read a pass later
// than the step before the band wrote it; `x1` holds that input a pass older, which is what b2 reads and
// what the same step writes; `y1` is the band's output a pass back, which -a1 reads and b0 overwrites;
// `y2` is its output two passes back, which -a2 reads.
struct Band
{
	int base;                  // the first of the five steps
	int in, x1, y1, y2;
	step::route_t route;       // the closing step's route, which restores the designer's scale
	bool own_y2;               // the -a2 step is also the z^-1 that keeps y[-2] itself
	Decor decor[5];
};

// The six bands: three per channel, each one on the last band's output. The left high band and the
// right high band are the two designFilter(2, 4, ...) sections, so their closing steps carry the x16
// route the other four do not, and those two are the bands whose own -a2 step keeps their y[-2]: the
// four lower bands read that delay out of the register the next band up writes.
constexpr Band BANDS[6] = {
    //  base   in  x1   y1   y2  route        b1               b2                 -a1            -a2             b0
    { 0x15a, 103, 104, 105, 106, ROUTE_2,  false, { {},                {DEC_READ, 0x56},  {},            {}, {}                } },
    { 0x15f, 105, 106, 107, 108, ROUTE_2,  false, { {DEC_READ, 0x57},  {DEC_CAPTURE, 28}, {},            {}, {DEC_READ, 0x58}  } },
    { 0x164, 107, 108, 109, 110, ROUTE_16, true,  { {DEC_CAPTURE, 29}, {},                {DEC_LFO, 11}, {}, {DEC_CAPTURE, 30} } },
    { 0x16a, 111, 112, 113, 114, ROUTE_2,  false, { {},                {},                {},            {}, {}                } },
    { 0x16f, 113, 114, 115, 116, ROUTE_2,  false, { {},                {},                {},            {}, {}                } },
    { 0x174, 115, 116, 117, 118, ROUTE_16, true,  { {},                {},                {},            {}, {}                } },
};

// One band's five steps. The decor the step's words carry comes first, because step() moves a cell or
// reads the delay RAM before it does any arithmetic, and then the band's term for this step.
void band(yss236_core& c, int st, Band const& b, int k)
{
	decor(c, st, b.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, b.in); break;                                   // b1 on x[n]
	case 1:  copy_z(c, st, b.x1, b.x1, ADD_ACC, CLAMP_WRAP); break;      // b2 on x[n-1], and x[n-1] copied
	case 2:  sum(c, st, b.y1); break;                                    // -a1 on y[n-1]
	case 3:
		// -a2 on the output two passes back. Where that delay is a register of the band's own, this step
		// keeps it as well, by its own z^-1; the four lower bands read the one the band above writes.
		if (b.own_y2) copy_z(c, st, b.y2, b.y2, ADD_ACC, CLAMP_WRAP);
		else          sum(c, st, b.y2);
		break;
	default: close(c, st, b.in, b.y1, b.route); break;                  // b0 on x[n], into y[n]
	}
}

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{

		if (st < 0x15a)
		{
			// ---- EQ3_A: the two serial inputs this window publishes for the base program ----
			if (st == 0x158)
			{
				take_serial(c, 2, st, 0);
				term(c, st, 5);                       // acc = k * r[5], the left input alone
			}
			else
			{
				take_serial(c, 6, st, 1);
				// r[103] = (k * r[6] + acc) * 2: the left and right inputs summed for the bands
				close(c, st, 6, 103, ROUTE_2);
			}
			return;
		}

		if (st < 0x169)
		{
			// ---- the left channel's three bands, cascaded ----
			Band const& b = BANDS[(st - 0x15a) / 5];
			band(c, st, b, st - b.base);
			return;
		}

		if (st == 0x169)
		{
			// EQ3_A[2]: the right input doubled on its own, with no adder input
			gain(c, st, 6, 111, ROUTE_2);
			return;
		}

		if (st < 0x179)
		{
			// ---- the right channel's three bands, the same six steps on the copy ----
			Band const& b = BANDS[3 + (st - 0x16a) / 5];
			band(c, st, b, st - b.base);
			return;
		}

		if (st < 0x17c)
		{
			// ---- EQ3_B: the two outputs, and the word between them that carries the channel ----
			switch (st)
			{
			case 0x179: gain(c, st, 109, 15, ROUTE_4); break;      // r[15] = 4 k r[109]: the left chain
			case 0x17a: term(c, st, 109); break;                    // the same word with no route and no register
			default:    close(c, st, 117, 16, ROUTE_4); break;      // r[16] = 4 (k r[117] + acc): the right chain
			}
			return;
		}

		// ---- the tail: three published reads, the captures that take them up, and the second LFO ----
		// The reads are at 0x18f, 0x193 and 0x197 and each capture follows five steps later, which is
		// captured()'s lower bound, so the tail is idle between them but for the x latch.
		int const k = st - 0x18f;
		if (k == 0 || k == 4 || k == 8)
			read_delay(c, st);
		else if (k == 5 || k == 9 || k == 13)
			take_capture(c, 31 + (k - 5) / 4, st);
		else if (st == 0x186)
			take_lfo(c, 12, st);
		idle(c);
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m07(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
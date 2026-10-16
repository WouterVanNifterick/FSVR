// effects/ins_m13.cpp - VOP3-2, the insertion window's program for Cmp+DS+Dly, Cmp+OD+Dly,
// Dist+Delay and Odrv+Delay: its 96 steps.
//
// rom::INSERTION_PROGRAMS[13], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 13: ins-26 Cmp+DS+Dly (lsb 1) and ins-27 Cmp+OD+Dly
// (lsb 2) on EffectHost::compressorDistortionDelay, ins-29 Dist+Delay (lsb 3) and ins-31 Odrv+Delay
// (lsb 4) on EffectHost::distortionDelay. Steps 0x1a0 to 0x1ff are the whole program. None of the 96 is
// a control step, a gate or a cskip, so the only thing the loop below carries out of step() is the
// m_skip guard a gate in the base program leaves behind.
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
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose
// is folded in here: the program words are fixed per row and each step's fields are spelled out below in
// the order yss236_core::step() runs its own. Which table names a step is said on the step. Four tables
// name steps this program does not read in the way the name suggests: ins::DD_BAND's
// steps 6 to 11 (0x1c4 to 0x1c9) carry words storeEdge sets in the host's coef[] but never writes to
// the chip, so 0x1c5 to 0x1c9 read 6553 and 0x1c4 reads zero; and ins::DD_MODE[1] is written by
// distortionDelay alone. The old model in vop3_effects.h runs the four types as a floating point
// compressor, a tanh drive and two delay lines with independent taps; this program has one line, four
// taps and no envelope follower of its own, so that model is no reference for any number here.
//
// The six sections, the four delay taps, the three serial takes, ins::DD_BAND's five series words, the
// two mixes, the four taps at the outputs and the level ladder and the ratio row are tables below, and
// the step loop runs them in step order; the rest of the 96 are the short blocks the loop dispatches on.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check ins-m13: identical, 4 types, 38400 native passes.
#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The registers and cells the header names, so the arithmetic below can say what it reads
// ---------------------------------------------------------------------------------------------

// The two signals in and the two outputs, then every register this window names except the five
// sections' own delay registers, r[79] to r[83], which the section that reads and writes them names,
// which is the Section table below. The DD_HPF's three are here because 0x1aa reads its state and
// 0x1a8's z^-1 fills the other two.
enum
{
	SIG_L = 7,        // r[7] and r[8]: the panned sends the base program leaves this window
	SIG_R = 8,
	OUT_L = 17,       // r[17] and r[18]: the two outputs the base program reads back
	OUT_R = 18,
	STEREO = 68,      // the pair as one signal, the DD_HPF's input
	HPF_X1 = 69,      // r[69] is the DD_HPF's input a pass back, r[70] its state and r[71] the same
	HPF_STATE = 70,   //   state a pass back, which the z^-1 at 0x1a8 puts there
	HPF_Y1 = 71,
	REL_A = 72,       // the release pair's two registers, each its own z^-1
	REL_B = 73,
	ENV = 74,         // the envelope the ladder builds and ins::DD_ATTACK smooths
	ENV_COPY = 75,    // the copy of r[74] that ins::DD_ATTACK's first two words share
	ENV_SMOOTH = 76,  // the smoothed envelope, and t[9] is 0x1ae's result
	TONE_IN = 77,     // ins::DD_MODE[0] * r[125] under the route x16, ins::DD_LPF's input
	LEVEL = 78,       // ins::DD_DRIVE[3]'s gain, ins::DD_LPF2's input
	WET_COPY = 85,    // r[85]: the wet signal a pass back, which ins::DD_LPF_2's z^-1 keeps and
	FEEDBACK = 86,    //   r[86] is the delay line's feedback, which the line's write scales
	WET = 84,         // r[84]: the wet signal, ins::DD_LOW_SHELF's state, which both mixes scale
	WORK = 119,       // the working register: the drive, then the edge's sum, then the ladder's first rung
	THRESHOLD = 120,  // the edge's first series word, then ins::DD_THRESHOLD's, and every rung's input
	MIX_L = 121,      // the two channels' mixed signals, a quarter of each onto the outputs
	MIX_R = 122,
	TAP_L = 123,      // the left output's tap, which is the fourth tap alone
	HPF_OUT = 124,    // the DD_HPF's output and ins::DD_MODE's operand, then the right output's tap
	MODE_A = 125,     // r[125] and r[126]: ins::DD_MODE's two operands under t[9]
	MODE_B = 126,
};

// The cells, which carry the values between the steps: the three serial inputs this window publishes
// and reads nowhere, the leaky integrator the drive word scales, the four delay taps and the mode word
// on the DD_HPF's state.
enum
{
	SERIAL_13_L = 3,     // m[3] and m[7]: page 13's two serial inputs, and m[4] page 14's left
	SERIAL_14_L = 4,     //   one. This window writes all three and reads none
	SERIAL_13_R = 7,
	DRIVE_CELL = 42,     // m[42]: the leaky integrator of ins::DD_LPF's output, the drive's input
	FEEDBACK_CELL = 43,  // m[43]: the first tap as it stood a pass on, which 0x1dd fills
	TAP0 = 56,           // m[56] to m[59]: the four taps, ins::DD_SLOTS' second to fifth slots
	TAP1 = 57,
	TAP2 = 58,
	TAP3 = 59,
	MODE_CELL = 60,      // m[60]: ins::DD_MODE[2] on the DD_HPF's state
};

// t[9], the smoothed envelope as a 1.15 word, which 0x1ae writes and the two steps that scale by it
// read. The word at each of those steps is unread, because the multiplier's other input is this.
constexpr int ATTACK_T = 9;

// ---------------------------------------------------------------------------------------------
// The delay line: ins::DD_SLOTS' five slots and the four taps
// ---------------------------------------------------------------------------------------------

// One tap of the line: the step that reads its slot out of the delay RAM and the step five steps
// later, CAPMIN and the nearest step()'s capture reaches, that takes the read into a cell. The slots
// are 0x69 to 0x6c and read_delay() takes each from its own step over four, so the slot is named by
// the step here rather than by a number. comboDelay() gives the four the same delay time, so the four
// cells hold one signal read at four different steps.
struct Tap { int read; int cell; int cap; };

constexpr Tap TAPS[4] = {
    //  the read  the cell  the capture    and what the two steps do besides
    {  0x1a4,  TAP0,  0x1a9 },      // slot 0x69: on ins::DD_HPF's b1 word, and the capture is a class-0 step
    {  0x1a8,  TAP1,  0x1ad },      // slot 0x6a: on ins::DD_DRIVE[1], and the capture rides DD_ATTACK[1]
    {  0x1ac,  TAP2,  0x1b1 },      // slot 0x6b: on DD_ATTACK[0], and the capture rides t[9] * r[124]
    {  0x1b0,  TAP3,  0x1b5 },      // slot 0x6c: on ins::DD_MODE[2], and the capture rides ins::DD_BAND[3]
};

// The three serial inputs this window publishes and reads nowhere in it: m[3] and m[7] from page 13's
// two banks, and m[4] from page 14's. They are decors, so they ride on the step whose slot they take
// and are applied before that step's arithmetic.
struct Serial { int st; int bank; int cell; };

constexpr Serial SERIALS[3] = {
    { 0x1a2, 0, SERIAL_13_L },      // on the step that closes the stereo pair into r[68]
    { 0x1a3, 1, SERIAL_13_R },      // on ins::DD_HPF's first step; Effects::run leaves the right bank at zero
    { 0x1c3, 0, SERIAL_14_L },      // on ins::DD_BAND's fifth word
};

// The serial take of a step, -1 where there is none. The three steps are fixed and none of them is
// another's, so the index is built once and a step reaches its own take rather than looking through the
// three for it.
constexpr auto SERIALS_AT = index_by_step(SERIALS);

// ---------------------------------------------------------------------------------------------
// The six filter sections
// ---------------------------------------------------------------------------------------------

// What one step of a section computes, named after the coefficient designFilter() wrote for it: b0, b1
// and b2 are the numerator's and -a1 and -a2 the denominator's. The three that open a section are the
// three shapes a section's first step has, one per coefficient that can come first, and the rest are
// the section's other words in the order the schedule runs them, which is not the order
// designFilter() emitted them in.
enum part_t
{
	B2_OPEN,        // b2 on the input a pass back, the adder off: the section's first word
	B1_OPEN,        // b1 on the input, the adder off: the section's first word
	B0_CELL_OPEN,   // b0 on a cell, the adder off, and the z^-1 that keeps the wet signal a pass back
	B1_SUM,         // b1 on the input, added to the sum
	B0_X1,          // b0 on the input, added to the sum, and the z^-1 that keeps the input a pass back
	B0_SUM,         // b0 on the input, added to the sum
	B1_CELL_PREV,   // b1 on m[43] added to the sum, and the cell takes the m operand before it
	A1_CELL,        // -a1 on m[42]: the section's state into the leaky integrator the drive scales
	A1_SUM,         // -a1 on the output a pass back, added to the sum
	A2_SUM,         // -a2 on the output a pass back, added to the sum
	A1_CLOSE,       // -a1 on the output a pass back, closing the section into its output
	A2_CLOSE,       // -a2 on the output a pass back, closing the section into its output
};

// One filter section, in direct form I with both feedback terms on the same state. `st` is its first
// step and `nsteps` how many it runs, five for the four order-2 sections and three for the two order-1
// ones. `in` is the register the section's input arrives in, `x1` the one that holds the input a pass
// back and so is what the b2 word scales, `y1` the one the -a2 word scales and the z^-1 at the head
// of the next section fills with this one's output, and `out` the one the -a1 word scales and the
// closing step writes, which is the next section's input. `route` is the closing step's route, the one
// that restores the scale designFilter()'s own (16 >> scale) took out of the coefficients.
struct Section
{
	int st, nsteps;
	int in, x1, y1, out;
	step::route_t route;
	part_t part[5];
};

// The six sections in step order, each running designFilter()'s own words in the schedule's order.
// ins::DD_HPF carries a decor of its own, the read of the line's first tap, which the TAPS table above
// holds; nothing else here does.
constexpr Section SECTIONS[6] = {
    //   st  nsteps  in  x1  y1  out  route     the section's steps in the order they run
    {  0x1a3, 5, STEREO, HPF_X1, HPF_Y1, HPF_STATE, ROUTE_2,
       { B2_OPEN, B1_SUM, B0_X1, A2_SUM, A1_CLOSE } },                 // ins::DD_HPF
    {  0x1bb, 3, TONE_IN,       0,     0,         0, ROUTE_2,
       { B1_OPEN, B0_SUM, A1_CELL } },                                 // ins::DD_LPF
    {  0x1cd, 5,    LEVEL,      79,     81,        80, ROUTE_2,
       { B2_OPEN, B1_SUM, B0_X1, A1_SUM, A2_CLOSE } },                // ins::DD_LPF2
    {  0x1d2, 5,       80,      81,     83,        82, ROUTE_2,
       { B2_OPEN, B1_SUM, B0_X1, A1_SUM, A2_CLOSE } },                // ins::DD_PEAK
    {  0x1d7, 5,       82,      83,     85,        84, ROUTE_16,
       { B2_OPEN, B1_SUM, B0_X1, A2_SUM, A1_CLOSE } },                // ins::DD_LOW_SHELF
    {  0x1dc, 3,        0,       0,     86,        86, ROUTE_2,
       { B0_CELL_OPEN, B1_CELL_PREV, A1_CLOSE } },                     // ins::DD_LPF_2
};

// The section a step belongs to, -1 where it belongs to none. The sections are not contiguous: the
// DD_HPF's is in three steps and ins::DD_LPF's is in the middle of ins::DD_BAND's series. None of the
// six runs into another, so the index is built rather than searched, and a step's place in its section
// is what its own first step tells it.
constexpr auto SECTION_AT = index_by_span(SECTIONS);

// That index as the section a step runs one of the six's words of, or null where it runs none.
Section const* section_for(int st)
{
	int const i = SECTION_AT[st];
	return i < 0 ? nullptr : &SECTIONS[i];
}

// One section's step k of n. Only ins::DD_LPF_2 runs on cells rather than registers, and only it
// publishes its result to one, so the two cell shapes are spelled out rather than tabulated.
void section_step(yss236_core& c, int st, Section const& s, int k)
{
	switch (s.part[k])
	{
		// ins::DD_LPF_2's b0 word on the first tap, and the z^-1 that keeps the wet signal a pass
		// back in r[85], which the next step's b1 word and ins::DD_LOW_SHELF's -a2 word read
	case B0_CELL_OPEN:
		run(c, {.st = st, .sm = TAP0, .m2 = M2_CELL, .dr = WET_COPY, .z1 = true});
		break;
		// ins::DD_LPF_2's b1 word on m[43], which takes the m operand this step's predecessor
		// overwrote, so the cell holds the first tap as it stood a pass on (s32)
	case B1_CELL_PREV:
		run(c, {.st = st, .sm = FEEDBACK_CELL, .m2 = M2_CELL, .add = ADD_ACC, .dm = FEEDBACK_CELL, .prev = true});
		break;
		// ins::DD_LPF's -a1 word on the leaky integrator, which is the state 0x1a0 scales into the
		// drive: the section's output under its own leak rather than in a register of its own
	case A1_CELL:
		run(c, {.st = st, .sm = DRIVE_CELL, .m2 = M2_CELL, .add = ADD_ACC, .route = s.route, .clamp = CLAMP_SAT, .dm = DRIVE_CELL});
		break;
	case B1_OPEN: term(c, st, s.in); break;                      // b1, the section's first word
	case B1_SUM:  sum(c, st, s.in); break;                       // b1
	case B0_X1:   copy_z(c, st, s.in, s.x1, ADD_ACC, CLAMP_WRAP); break;   // b0, and x[-1] kept
	case B0_SUM:  sum(c, st, s.in); break;                       // b0
	case A1_SUM:  sum(c, st, s.out); break;                      // -a1 on the output a pass back
	case A2_SUM:  sum(c, st, s.y1); break;                       // -a2 on the same value, one register on
	case A1_CLOSE: close(c, st, s.out, s.out, s.route); break;   // -a1, closing into the output
	case A2_CLOSE: close(c, st, s.y1, s.out, s.route); break;    // -a2, closing into the output
	case B2_OPEN: term(c, st, s.x1); break;                      // b2, the section's first word
	}
}

// ---------------------------------------------------------------------------------------------
// The tables of steps that are not a filter
// ---------------------------------------------------------------------------------------------

// One channel's mix of the dry signal and the wet one, two steps each. dryWet() interleaves, so the
// four steps run dry, wet, dry, wet and the two channels differ only in the dry signal they take: the
// wet word is the same on both and scales the same r[84], ins::DD_LOW_SHELF's state.
struct Mix { int base, dry, out; };

constexpr Mix MIXES[2] = {
    { 0x1df, SIG_L, MIX_L },     // ins::DD_WET[0] and ins::DD_WET[1], into r[121]
    { 0x1e1, SIG_R, MIX_R },     // ins::DD_WET[2] and ins::DD_WET[3], into r[122]
};

// ins::DD_BAND's five series words, the geometric series storeEdge builds from the tone's edge byte:
// the gain word once and then the same word four times down the chain, each stage under the route x16,
// and the fifth closing back on r[119]. Only the two middle steps carry anything else, a delay tap
// read and a serial take, which the two decor tables above hold.
struct BandWord { int st, src, dst; };

constexpr BandWord BAND[5] = {
    { 0x1af, WORK,      THRESHOLD },      // ins::DD_BAND[1], the edge's gain word
    { 0x1b2, THRESHOLD, MIX_L },          // ins::DD_BAND[2], the first series word
    { 0x1b5, MIX_L,     MIX_R },          // ins::DD_BAND[3], the second, with the fourth tap's cell
    { 0x1ba, MIX_R,     TAP_L },          // ins::DD_BAND[4], the third
    { 0x1c3, TAP_L,     WORK },           // ins::DD_BAND[5], the fourth, with m[4] taking page 14's input
};

// ins::DD_BAND's series word of a step, -1 where there is none. The five steps are fixed, no two of them
// are the same and every one of them lands outside the block the step loop dispatches on, so the index
// is built once and a step reaches its word rather than looking through the five for it.
constexpr auto BAND_AT = index_by_step(BAND);

// The four taps at the outputs, one step each, one cell each. The fourth reaches r[123], because the
// third tap's own word is the preset's own zero and the adder is the running value, which is what the
// fourth tap left. The second reaches r[124] under the preset's own full scale word, and the right
// output then scales that by its own word under the route x4, so both outputs do take a tap. The third
// is read twice and scaled by zero both times, and the second reading clears the running value.
struct TapOut { int cell; int dr; };

constexpr TapOut TAP_OUT[4] = {
    { TAP3, 0 },          // 0x1e3: the fourth tap into the running value, the adder off
    { TAP2, TAP_L },      // 0x1e4: the third tap under a zero word, added to what 0x1e3 left
    { TAP2, 0 },          // 0x1e5: the third tap again under a zero word, the adder off, which clears
    { TAP1, HPF_OUT },    // 0x1e6: the second tap into r[124], which the right output closes
};

// ins::DD_RATIO's eight words, the compiler's order of the ladder's eight bands rather than the order
// the steps run them in: DD_RATIO[0] is 0x1f6, [2] is 0x1f7 and [1] is 0x1f9, and the rest follow the
// step order. The first of the eight opens the sum and the rest add to it, and the weights are the
// curve ins::storeRatio copied out of rom::COMP_RATIO.
struct Ratio { int src; route_t route; add_t add; };

constexpr Ratio RATIO[8] = {
    { WORK,    ROUTE_2, ADD_OFF  },      // 0x1f6, ins::DD_RATIO[0]: rung 0, opening the sum
    { MIX_L,   ROUTE_2, ADD_ACC  },      // 0x1f7, ins::DD_RATIO[2]: rung 1
    { MIX_R,   ROUTE_2, ADD_ACC  },      // 0x1f8, ins::DD_RATIO[3]: rung 2
    { TAP_L,   ROUTE_2, ADD_ACC  },      // 0x1f9, ins::DD_RATIO[1]: rung 3
    { HPF_OUT, ROUTE_2, ADD_ACC  },      // 0x1fa, ins::DD_RATIO[4]: rung 4
    { MODE_A,  ROUTE_4, ADD_ACC  },      // 0x1fb, ins::DD_RATIO[5]: rung 5, under the route x4
    { MODE_B,  ROUTE_1, ADD_ACC  },      // 0x1fc, ins::DD_RATIO[6]: rung 6
    { 127,     ROUTE_1, ADD_ACC  },      // 0x1fd, ins::DD_RATIO[7]: rung 7
};

// ---------------------------------------------------------------------------------------------
// The blocks that are not a table
// ---------------------------------------------------------------------------------------------

// The stereo pair as one signal and the drive state, 0x1a0 to 0x1a2. r[7] and r[8] are the base
// program's left and right signals and nothing else in this window reads either of them except the
// two mixes below, so this is the one place they become one signal.
void stereo_pair(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1a0:
		// ins::DD_DRIVE[0], the one entry of the drive table storeDrive never writes, so the word is
		// the preset's own: a quarter under the route x16 on m[42], the state 0x1bd keeps
		run(c, {.st = st, .sm = DRIVE_CELL, .m2 = M2_CELL, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = WORK});
		break;
	case 0x1a1:
		term(c, st, SIG_L);       // the preset's own word on the left signal, the adder off
		break;
	default:
		// the same word on the right signal under the route x2, added to what 0x1a1 left, so r[68]
		// is the pair as one signal: the input the whole window works on
		close(c, st, SIG_R, STEREO, ROUTE_2);
		break;
	}
}

// The drive word's second entry, the first tap's capture, the DD_HPF's own output and the drive word's
// third entry: 0x1a8 to 0x1ab.
void drive_and_highpass(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1a8:
		// ins::DD_DRIVE[1] on r[119], the adder off, into m[60] under a clamp that takes |y|. The
		// z^-1 puts r[119]'s own value into r[71], which is what makes the DD_HPF's two feedback
		// words read the same state. The running value this leaves is not read again
		run(c, {.st = st, .sr = WORK, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = HPF_Y1, .z1 = true, .dm = MODE_CELL});
		break;
	case 0x1a9:
		idle(c);            // the class-0 step that takes up the first tap's read
		break;
	case 0x1aa:
		// the preset's own half on the DD_HPF's state under the route x2, and nothing else in this
		// window reads the section for: r[124] is the highpassed pair
		gain(c, st, HPF_STATE, HPF_OUT, ROUTE_2);
		break;
	default:
		// ins::DD_DRIVE[2], the same word storeDrive wrote into 0x1a8, on m[60]
		run(c, {.st = st, .sm = MODE_CELL, .m2 = M2_CELL, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = WORK});
		break;
	}
}

// ins::DD_ATTACK, the envelope follower, 0x1ac to 0x1ae. r[74] is the envelope the ratio row builds at
// 0x1fe and r[75] is a plain copy of it, so the host's two identical words land twice.
void attack(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1ac:
		// ins::DD_ATTACK[0] on r[74], the adder off, and the z^-1 makes r[75] a copy of it.
		// storeCompAttack writes (0x8000 - t) / 2 into this step and 0x1ad and t into 0x1ae, so the
		// two applications together are 1 - t; the two distortion types come through
		// distortionGate instead, whose storeGateAttack writes (-0x8000 - t) * 32 twice
		copy_z(c, st, ENV, ENV_COPY, ADD_OFF, CLAMP_WRAP);
		break;
	case 0x1ad:
		sum(c, st, ENV_COPY);     // ins::DD_ATTACK[1], the same word as the step before
		break;
	default:
		// ins::DD_ATTACK[2], the word t, on r[76] and t[9] takes the result as a 1.15 word. The two
		// applications are 1 - t and t, so r[76] = t * r[76] + (1 - t) * r[74]: a one-pole smoother
		// of unity gain, and t[9] is where the two steps that scale by it read it
		t_put(c, st, ATTACK_T, run(c, {.st = st, .sr = ENV_SMOOTH, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = ENV_SMOOTH}));
		break;
	}
}

// One step of the two that scale by t[9]. Both are ins::DD_MODE's operands scaled by the smoothed
// envelope rather than by the word at the step, which is why the coefficient here is unread. Only
// 0x1b1's t write field is set, so only that one of the two puts the result back into t[9].
double mode_scale(yss236_core& c, int st, int dr)
{
	return run(c, {.st = st, .sr = HPF_OUT, .treg = ATTACK_T, .clamp = CLAMP_SAT, .dr = dr});
}

// ins::DD_MODE's words and the release pair's first two steps, 0x1b0 to 0x1ba with ins::DD_BAND's three
// words among them, which the step loop takes first.
void mode_and_release(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b0:
		// ins::DD_MODE[2] on r[124], the DD_HPF's output, under the route x16 and a clamp that takes
		// |y| into m[60]: the mode word the drive scales a pass later at 0x1ab
		run(c, {.st = st, .sr = HPF_OUT, .route = ROUTE_16, .clamp = CLAMP_ABS, .dm = MODE_CELL});
		break;
	case 0x1b1:
		// t[9] on r[124] into r[125], and t[9] takes the result, which 0x1b6 then reads. The word at
		// this step is never read: the multiplier's other input is t[9], not the coefficient
		t_put(c, st, ATTACK_T, mode_scale(c, st, MODE_A));
		break;
	case 0x1b3:
		// ins::DD_RELEASE[0] on r[72] less m[60], the mode word, under a clamp that drops the
		// negatives, and nothing is written
		run(c, {.st = st, .sr = REL_A, .sm = MODE_CELL, .add = ADD_CELL, .sub = true, .clamp = CLAMP_ZERO});
		break;
	case 0x1b4:
		// the same release word on m[60], added to what 0x1b3 left, closing into r[72]
		run(c, {.st = st, .sm = MODE_CELL, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = REL_A});
		break;
	case 0x1b6:
		// t[9] on r[124] into r[126], the second of the two steps that scale by the smoothed envelope.
		// This one's t write field is zero, so it reads t[9] and does not put the result back
		mode_scale(c, st, MODE_B);
		break;
	case 0x1b7:
		term(c, st, HPF_OUT);        // ins::DD_MODE[1] on r[124], the adder off
		break;
	case 0x1b8:
		sum(c, st, MODE_A);          // ins::DD_MODE[0] on r[125], added to the running value
		break;
	default:
		// ins::DD_BAND[0], the entry storeEdge never writes, so the word is the preset's own zero:
		// r[77] is ins::DD_MODE[1] * r[124] + ins::DD_MODE[0] * r[125] under the route x16 with the
		// r[126] term dropped. compressorDistortionDelay leaves DD_MODE[1] alone and distortionDelay
		// sets it to zero, and the preset's own word there is zero too, so under all four types that
		// term is zero and r[77] is ins::DD_MODE[0] * r[125] alone. This is ins::DD_LPF's input
		close(c, st, MODE_B, TONE_IN, ROUTE_16);
		break;
	}
}

// The release pair's other two words, 0x1be to 0x1c2, on the same two registers as 0x1b3 and 0x1b4.
// Each of the two is its own z^-1, so neither changes after a reset, which is why r[120] at 0x1ca is
// the threshold word times zero and the ladder at 0x1ee is a staircase of constants whatever the
// release word is. INFERRED: the checker proves the arithmetic and not that the compressor is inert.
void release_tail(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1be:
		term(c, st, REL_B);     // the preset's own zero, which clears the running value
		break;
	case 0x1bf:
		// ins::DD_RELEASE[1] on r[73], added to the running value over 32768: the adder is the cell
		// and no cell is latched, so it takes the running value scaled down rather than a cell
		run(c, {.st = st, .sr = REL_B, .add = ADD_CELL});
		break;
	case 0x1c0:
		// the preset's own full scale on m[60] less the running value, which takes the mode word back
		// out of what 0x1bf left
		run(c, {.st = st, .sm = MODE_CELL, .m2 = M2_CELL, .add = ADD_ACC, .sub = true});
		break;
	case 0x1c1:
		// ins::DD_RELEASE[2] on r[72] less the running value, under a clamp that drops the
		// negatives. The host's word for it is -0x8000 - t, which is not a representable 1.15 word:
		// it wraps on the way to the chip, and the step reads the wrapped value, a small positive
		run(c, {.st = st, .sr = REL_A, .add = ADD_ACC, .sub = true, .clamp = CLAMP_ZERO});
		break;
	default:
		// the same word as 0x1c0 on m[60], added to what 0x1c1 left, closing into r[73]
		run(c, {.st = st, .sm = MODE_CELL, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = REL_B});
		break;
	}
}

// ins::DD_BAND's own last words, 0x1c4 to 0x1c9: the edge's series summed with a fifth of each of its
// four stages. storeEdge sets 0x1c4's word to zero in the host's shadow and writes steps 6 to 11 to
// neither, so the chip reads the preset's words here: zero, and then 6553, which is a fifth of full
// scale. The four states are r[120] to r[123], the series the five words above built.
void edge_sum(yss236_core& c, int st)
{
	if (st == 0x1c4) { term(c, st, WORK); return; }               // ins::DD_BAND[6], the preset's zero
	if (st == 0x1c9) { close(c, st, WORK, WORK); return; }         // ins::DD_BAND[11], the edge's sum
	sum(c, st, THRESHOLD + (st - 0x1c5));                         // ins::DD_BAND[7] to [10]
}

// ins::DD_THRESHOLD, the padding between it and the drive's gain word, and that word: 0x1ca to 0x1cc.
void threshold_and_drive(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1ca:
		// ins::DD_THRESHOLD on r[73], the release register, which never leaves zero, so this is the
		// threshold word times zero and the ladder below is a staircase of constants
		gain(c, st, REL_B, THRESHOLD, ROUTE_1);
		break;
	case 0x1cb:
		idle(c);                // the padding, like 0x1ff: it counts and moves the x latch
		break;
	default:
		// ins::DD_DRIVE[3], the gain word storeDrive derives from the drive and the output level,
		// and ins::DD_LPF2's input
		gain(c, st, WORK, LEVEL, ROUTE_1);
		break;
	}
}

// One channel's dry and wet mix, two steps each, 0x1df to 0x1e2. The two steps run four times over.
void output_mix(yss236_core& c, int st)
{
	int const k = st - MIXES[0].base;         // 0 to 3, two steps per channel
	Mix const& m = MIXES[k >> 1];
	// ins::DD_WET[1] or [3], the wet word on r[84], closing the channel's mixed signal into r[121] or
	// r[122] under the route x4, which is the scale dryWet()'s own shift took out
	if (k & 1) close(c, st, WET, m.out, ROUTE_4);
	else       term(c, st, m.dry);             // ins::DD_WET[0] or [2], the dry word, the adder off
}

// The four taps at the outputs, one step each, 0x1e3 to 0x1e6.
void tap_outputs(yss236_core& c, int st)
{
	TapOut const& t = TAP_OUT[st - 0x1e3];
	// the adder is the running value on the two steps that write a register and off on the two that
	// only move it, which is what makes 0x1e4 the fourth tap alone and 0x1e5 a clear
	if (t.dr) run(c, {.st = st, .sm = t.cell, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = t.dr});
	else      term_cell(c, st, t.cell);
}

// The tail of the program: the two channels' quarters, the delay line's write and the two outputs,
// 0x1e7 to 0x1ed.
void line_and_outputs(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1e7:
		term(c, st, MIX_L);      // the preset's own quarter of the left mixed signal
		break;
	case 0x1e8:
		// the same quarter of the right one under the route x2, added to the left one, so the right
		// channel reaches the line's write at twice the level it reaches its own output at 0x1ec
		run(c, {.st = st, .sr = MIX_R, .add = ADD_ACC, .route = ROUTE_2});
		break;
	case 0x1e9:
		// ins::DD_FB[0] on r[86], the line's feedback, added to the two quarters and written into
		// slot 0x7a: this is the delay line's write. wdata is set and memop is 1, so the step clears
		// the write latch rather than setting it and the RAM takes this step's own result
		run(c, {.st = st, .sr = FEEDBACK, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		break;
	case 0x1ea:
		// the left quarter again, the adder off: 0x1e7's copy went into the line's write and this
		// one is what the left output adds to
		term(c, st, MIX_L);
		break;
	case 0x1eb:
		close(c, st, TAP_L, OUT_L, ROUTE_4);      // ins::DD_LEVEL[0] on r[123] plus a quarter of r[121]
		break;
	case 0x1ec:
		term(c, st, MIX_R);                       // the right quarter, under no route this time
		break;
	default:
		close(c, st, HPF_OUT, OUT_R, ROUTE_4);    // ins::DD_LEVEL[2] on r[124] plus a quarter of r[122]
		break;
	}
}

// The level ladder, eight class-1 steps from 0x1ee to 0x1f5. Each is its own word loaded as a value in
// LOADK units and added to r[120] rather than scaled by anything, under a clamp that drops the
// negatives, so each rung is max(0, r[120] + 128 * word) and the eight are a staircase under the
// threshold rather than a set of levels. The words are the preset's own, a negative run from a quarter
// down to 64 for the two compressor types and the distortion types' own, and every rung reads its word
// on the pass, so nothing about the curve is folded in here.
void level_ladder(yss236_core& c, int st)
{
	int const k = st - 0x1ee;
	// the rungs go to r[119] and r[121] to r[127]: r[120] is left as the input all eight of them read
	load(c, {.st = st, .sr = THRESHOLD, .add = ADD_REG, .clamp = CLAMP_ZERO, .dr = WORK + k + (k ? 1 : 0)});
}

// ins::DD_RATIO, the curve over the ladder's eight bands, 0x1f6 to 0x1fd.
void ratio_row(yss236_core& c, int st)
{
	Ratio const& q = RATIO[st - 0x1f6];
	run(c, {.st = st, .sr = q.src, .add = q.add, .route = q.route});
}

// The envelope, 0x1fe: full scale plus the weighted ladder under a clamp that drops the negatives,
// which is the value 0x1ac smooths and the only reader of the ladder there is.
void envelope(yss236_core& c, int st)
{
	load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_ZERO, .dr = ENV});
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's. There is no gate or cskip step in this program, so the
// guard is the only control the loop needs.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// ---- the three serial inputs this window publishes, before any step's arithmetic ----
		if (int i = SERIALS_AT[st]; i >= 0) take_serial(c, SERIALS[i].cell, st, SERIALS[i].bank);

		// ---- the delay line's four taps: the read, then the capture five steps later ----
		// Left a scan: a step of this table is looked for in two of its four columns at once, the read
		// and the capture, and index_by_step keys on one column.
		for (Tap const& tp : TAPS)
		{
			if (st == tp.read) read_delay(c, st);
			if (st == tp.cap)  take_capture(c, tp.cell, st);
		}

		// ---- 0x1a0 to 0x1a2: the stereo pair as one signal, and the drive state ----
		if (st < 0x1a3) { stereo_pair(c, st); return; }

		// ---- the six filter sections, where they fall ----
		if (Section const* s = section_for(st)) { section_step(c, st, *s, st - s->st); return; }

		// ---- 0x1a8 to 0x1ab: the drive again, the first tap, the DD_HPF's output, the drive again ----
		if (st < 0x1ac) { drive_and_highpass(c, st); return; }

		// ---- 0x1ac to 0x1ae: ins::DD_ATTACK, the envelope follower ----
		if (st < 0x1af) { attack(c, st); return; }

		// ---- ins::DD_BAND's five series words, where they fall ----
		if (int i = BAND_AT[st]; i >= 0) { gain(c, st, BAND[i].src, BAND[i].dst, ROUTE_16); return; }

		// ---- 0x1b0 to 0x1ba: ins::DD_MODE, the release pair, and ins::DD_MODE's own chain ----
		if (st < 0x1bb) { mode_and_release(c, st); return; }

		// ---- 0x1be to 0x1c2: the release pair's other two words ----
		if (st < 0x1c3) { release_tail(c, st); return; }

		// ---- 0x1c4 to 0x1c9: ins::DD_BAND's own last words, the edge's sum ----
		if (st < 0x1ca) { edge_sum(c, st); return; }

		// ---- 0x1ca to 0x1cc: the threshold, the padding and ins::DD_DRIVE's gain word ----
		if (st < 0x1cd) { threshold_and_drive(c, st); return; }

		// ---- 0x1df to 0x1e2: the two dry and wet mixes ----
		if (st < 0x1e3) { output_mix(c, st); return; }

		// ---- 0x1e3 to 0x1e6: the four taps at the outputs ----
		if (st < 0x1e7) { tap_outputs(c, st); return; }

		// ---- 0x1e7 to 0x1ed: the two channels' quarters, the line's write and the outputs ----
		if (st < 0x1ee) { line_and_outputs(c, st); return; }

		// ---- 0x1ee to 0x1f5: the level ladder ----
		if (st < 0x1f6) { level_ladder(c, st); return; }

		// ---- 0x1f6 to 0x1fd: ins::DD_RATIO, the curve over the ladder's bands ----
		if (st < 0x1fe) { ratio_row(c, st); return; }

		// ---- 0x1fe: the envelope, and 0x1ff: the program's last step, empty ----
		if (st == 0x1fe) envelope(c, st);
		else             idle(c);
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m13(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
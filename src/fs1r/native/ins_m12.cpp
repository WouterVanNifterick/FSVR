// fs1r/native/ins_m12.cpp - VOP3-2, the insertion window's program for Wah+DS+Dly and Wah+OD+Dly:
// its 96 steps.
//
// rom::INSERTION_PROGRAMS[12], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 12, lsb 1 and 2: ins-18 Wah+DS+Dly and ins-19
// Wah+OD+Dly, both on EffectHost::wahDistortionDelay. Steps 0x1a0 to 0x1ff are the whole program.
// None of the 96 is a control step, a gate or a cskip, so the only thing the loop below carries out of
// step() is the m_skip guard a gate in the base program leaves behind.
//
// It is a wah, a distortion and a delay, in that order. What the 96 steps compute:
//
//   0x1a0-0x1ad  the input EQ. Each channel copies its signal into r[67] or r[70] and runs the low
//                shelf ins::LPF_WAH into r[68] or r[71] and the high shelf ins::HPF_WAH into r[69] or
//                r[72]. shelves() designs each shelf once and copies its three words to the other
//                channel's steps, so both channels run the same pair of sections.
//   0x1ae-0x1af  ins::WAH_MODE sums m[43] and m[44] into r[79], the signal the level follower uses.
//                Those two cells are the wah's own output, written at 0x1f9 and 0x1fb.
//   0x1b0-0x1b5  the level follower: three steps take the difference of r[79] and leave it in r[80],
//                three more smooth r[80] into r[81]. INFERRED: a difference of the signal is not an
//                envelope, so whether this is meant to read as a level or as an edge detector is not
//                settled by the arithmetic.
//   0x1b6-0x1bb  ins::WAH_MODE2 sums the two high shelves into r[73], the signal that drives the
//                resonator. ins::WAH_DRIVE[0] carries r[81] into r[119]. 0x1b9 and 0x1ba are one word
//                and its opposite, so they clear the running value and leave r[74] as a state with no
//                input of its own, ins::WAH_HPF_2[0].
//   0x1bc-0x1c4  ins::WAH_DRIVE[1] and [2] scale r[119] twice more. m[56] is the resonator's feedback:
//                ins::WAH_SENSE[0], the word wahSense() builds from the resonance, times the
//                resonator's state less the signal driving it. m[58] and m[59] are the same sum with
//                the resonance word and with the word at 0x1c4 in its place, and both reach the delay
//                line's input at 0x1dc.
//   0x1c6-0x1d8  the delay line. ins::WD_SL names its five slots: 0x77 is written at 0x1dc, and 0x71
//                to 0x74 are read one word apart at 0x1c7, 0x1cb, 0x1cf and 0x1d3. A capture takes the
//                latest read of the pass made CAPMIN to 11 steps earlier, so m[60] takes the slot 0x71
//                at 0x1cc, m[61] the slot 0x72 at 0x1d0, m[62] the slot 0x73 at 0x1d4 and m[63] the
//                slot 0x74 at 0x1d8. The first is averaged over two samples and the average is what
//                the line's feedback sums at 0x1dc; the other three are added to m[42] by
//                ins::WAH_LPF2 at 0x1e7 to 0x1ea.
//   0x1cd-0x1d1  ins::WAH_BAND, the type's fixed edge: a gain word into r[121] and the geometric
//                series' one word into r[122], r[123] and two steps that are not a chain.
//   0x1d2-0x1de  the resonator's gain. r[124] is the resonator's drive and hands t[9], a 1.15 word,
//                to the t file; 0x1d9 scales r[124] by t[9] and 0x1de builds the next t[9] from the
//                running value. The gain is therefore the loop's own signal, and the loop saturates on
//                its own: that is what sweeps the wah's resonance with the level.
//   0x1e6-0x1ea  the resonator itself, r[78] = r[78] + t[9] * m[56], and ins::WAH_LPF2's four other
//                words, a section that ends in r[120] and r[121].
//   0x1eb-0x1f2  m[42], the integrator 0x1bd differences, and ins::WAH_PEAK, the distortion's mid peak.
//   0x1f3-0x1ff  the follower m[47], the wah's output as m[43] and m[44], and ins::WD_LEVEL into
//                r[17] and r[18], the two outputs.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] the two signals
// in; r[17] and r[18] the two outputs; r[67] to r[72] the input EQ, a copy, a low shelf and a high
// shelf per channel; r[73] the resonator's input; r[74] the state 0x1bb leaves; r[75] and r[76] the
// delay line's head; r[77] the accumulator the third tap feeds; r[79] the two channels summed, r[80]
// and r[81] the level follower; r[82] to r[84] ins::WAH_PEAK; r[85] the two-sample average of the
// first tap; r[119] the signal that leaves dry; r[120] and r[121] ins::WAH_LPF2's output and the left
// dry signal; r[122] and r[123] ins::WAH_BAND's series; r[124] the resonator's drive. Of the cells,
// m[42] is the resonator's integrator, m[43] and m[44] the wah's output, m[45] to m[47] the follower,
// m[48] the two-sample tap, m[56] and m[57] the feedback and the absolute value, m[58] and m[59] the
// two sums the delay line is fed, m[60] to m[63] the four taps. m[3], m[4] and m[7] are written here
// from the serial inputs of pages 13 and 14 and read nowhere in this window, and m[12] is latched at
// 0x1d3 and multiplied by the word at that step, which the ROM leaves at zero.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose
// is folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in the order yss236_core::step() runs its own. Which table names a step is said on
// the step; two of them name the same step, ins::WAH_BAND[5] and ins::WD_FB[0] both being 0x1dc, and
// at 0x1e6 the word ins::WAH_LPF2 lists is not the one the step multiplies by. The old model in
// vop3_effects.h runs the two types as a floating point wah, a tanh drive and a delay with feedback
// and mix; it has none of these words, so it is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time. Inside the groups the loop opens, each repeated shape is a
// table and a small function that runs it: the two channels' copy and shelves, the follower's two
// three-step blocks, the two sums the delay line is fed, ins::WAH_DRIVE's three steps, ins::WAH_BAND's
// five, the four taps, the resonator's two output steps, the edge's sum, the wah's two output cells
// and the two mixes into r[17] and r[18]. The steps with a shape of their own are a named function
// each, and the serial inputs, the delay RAM reads and the captures are one decor table the loop
// reads at the top of every step.
//
// vop3_native_check ins-m12: identical, 2 types, 19200 native passes.
#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, load and the rest

// ---------------------------------------------------------------------------------------------
// The registers and the cells, so the tables below can say what they read and write
// ---------------------------------------------------------------------------------------------

enum
{
	SIG_L = 7,        // the two signals the base program leaves this window
	SIG_R = 8,
	OUT_L = 17,       // and the two it reads back
	OUT_R = 18,
	EQ_L_COPY = 67,   // the input EQ: a copy of the signal, a low shelf and a high shelf per channel
	EQ_L_LOW = 68,
	EQ_L_HIGH = 69,
	EQ_R_COPY = 70,
	EQ_R_LOW = 71,
	EQ_R_HIGH = 72,
	RES_IN = 73,      // the two high shelves as one signal, which is what drives the resonator
	RES_STATE = 74,   // ins::WAH_HPF_2[0]'s state, which the program gives no input of its own
	LINE_A = 75,      // the delay line's two head registers
	LINE_B = 76,
	LINE_ACC = 77,    // and the accumulator the third tap feeds
	RES = 78,         // the resonator
	LEVEL = 79,       // ins::WAH_MODE: the two channels summed, which the follower works from
	LVL_1 = 80,       // the level follower's two states
	LVL_2 = 81,
	PEAK_IN = 82,     // ins::WAH_PEAK's input, then its own output delay and the input a pass back
	PEAK_OUT = 83,
	PEAK_X1 = 84,
	TAP_AVG = 85,     // the two-sample average of the first tap
	DRY = 119,        // the signal that leaves the window dry
	LINE_IN = 120,    // the delay line's input
	DRY_L = 121,      // and the left dry signal
	BAND_2 = 122,     // the rest of ins::WAH_BAND's series
	BAND_3 = 123,
	RES_DRIVE = 124,  // the resonator's drive
};

enum
{
	LFO_CELL = 12,    // m[12], this window's first LFO, latched at 0x1d3 under a word of zero
	INTEG = 42,       // m[42], the resonator's integrator, which 0x1bd differences
	WAH_L = 43,       // m[43] and m[44], the wah's two outputs, written at 0x1f9 and 0x1fb
	WAH_R = 44,
	FOL_A = 45,       // m[45] to m[47], the follower the two sums at 0x1c2 and 0x1c4 work from
	FOL_B = 46,
	FOL_C = 47,
	TAP_PREV = 48,    // m[48], the first tap as it stood a pass ago
	FB = 56,          // m[56], the resonator's feedback
	ATTACK = 57,      // m[57], the resonator's absolute value under ins::WAH_ATTACK
	WET_L = 58,       // m[58] and m[59], the two sums the delay line is fed
	WET_R = 59,
	TAP_1 = 60,       // m[60] to m[63], the four taps
	TAP_2 = 61,
	TAP_3 = 62,
	TAP_4 = 63,
};

// t[9] is the resonator's loop gain, and the multiplier's other input wherever the program says t[9].
enum { RES_GAIN = 9 };

// ---------------------------------------------------------------------------------------------
// What a step moves into a cell, or reads out of the delay RAM, before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step of this program may do three things before it computes: it takes a serial input into one of
// the three cells the base program expects this window to publish, it takes up a delay RAM read made
// CAPMIN to 11 steps earlier, or it reads the delay RAM itself. DEC_SERIAL_L and DEC_SERIAL_R take
// the step's own page's left and right serial input, DEC_CAPTURE takes the read into `cell`, and
// DEC_READ pushes the RAM's word into the capture history, where a capture further down takes it up.
// No step here does two of them on the same word, so the order within a step is not a question.
enum decor_t { DEC_SERIAL_L, DEC_SERIAL_R, DEC_CAPTURE, DEC_READ };

struct Decor { int st; decor_t kind; int cell; };

// The three serial inputs, the four reads and the four captures that go with them. Each read's slot is
// its own step over four, which is where step() takes it from, so no decor here names a slot.
const Decor DECORS[11] = {
    //  the step  the decor                the cell
    {  0x1a3, DEC_SERIAL_L,  3 },     // the left serial input of page 13, m[3]
    {  0x1a4, DEC_SERIAL_R,  7 },     // and the right one, m[7]
    {  0x1c5, DEC_SERIAL_L,  4 },     // the left serial input of page 14, this window's own, m[4]
    {  0x1c7, DEC_READ,      0 },     // ins::WD_SL[1], the slot 0x71, which m[60] takes up at 0x1cc
    {  0x1cb, DEC_READ,      0 },     // ins::WD_SL[2], the slot 0x72, which m[61] takes up at 0x1d0
    {  0x1cc, DEC_CAPTURE,  60 },
    {  0x1cf, DEC_READ,      0 },     // ins::WD_SL[3], the slot 0x73, which m[62] takes up at 0x1d4
    {  0x1d0, DEC_CAPTURE,  61 },
    {  0x1d3, DEC_READ,      0 },     // ins::WD_SL[4], the slot 0x74, which m[63] takes up at 0x1d8
    {  0x1d4, DEC_CAPTURE,  62 },
    {  0x1d8, DEC_CAPTURE,  63 },
};

// The cell move and the delay RAM read of one step, if it carries any. It comes before the step's own
// arithmetic, because step() moves a cell or reads the delay RAM before it does anything else.
void publish(yss236_core& c, int st)
{
	for (Decor const& d : DECORS)
	{
		if (d.st != st) continue;
		switch (d.kind)
		{
			case DEC_SERIAL_L: take_serial(c, d.cell, st, 0); break;
			case DEC_SERIAL_R: take_serial(c, d.cell, st, 1); break;
			case DEC_CAPTURE:  take_capture(c, d.cell, st); break;
			case DEC_READ:     read_delay(c, st); break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// A sum closed on a cell rather than on a register
// ---------------------------------------------------------------------------------------------

// p = k * m[sm] + p, routed, taken into a register (`dr`) or a cell (`dm`), and with `prev` the cell
// takes the step before's m operand instead of the result (w2[15:13] = 7). steps.h has no name for
// this, because every section in the other programs closes on a register, so it is named here: ten
// of the 96 steps are this one shape, and five of those ten publish a cell.
void cell_sum(yss236_core& c, int st, int sm, route_t route, clamp_t clamp, int dr, int dm,
              bool prev = false)
{
	run(c, {st, 0, sm, M2_CELL, false, 0, ADD_ACC, false, route, clamp, dr, false, dm, prev});
}

// ---------------------------------------------------------------------------------------------
// 0x1a0 to 0x1ad: the input EQ
// ---------------------------------------------------------------------------------------------

// One channel's seven steps: a copy of the signal, then the low shelf's three and the high shelf's
// three. `copy` is where the copy lands and both shelves read it, `low` is the low shelf's output and
// the high shelf's input, and `high` is the high shelf's output, which ins::WAH_MODE and ins::WAH_MODE2
// sum into the level the level follower and the resonator work from.
struct EqChannel { int base; int sig, copy, low, high; };

const EqChannel EQ[2] = {
    { 0x1a0, SIG_L, EQ_L_COPY, EQ_L_LOW, EQ_L_HIGH },     // ins::LPF_WAH and ins::HPF_WAH
    { 0x1a7, SIG_R, EQ_R_COPY, EQ_R_LOW, EQ_R_HIGH },     // and ins::LPF_WAH_C and ins::HPF_WAH_C
};

// One channel's step k of seven. The three words of a shelf arrive in the order designFilter() wrote
// them, (b0, b1, -a1), while the program runs them in another: the section's own output first, the
// input as it stood a pass ago second, and b0 last, where the input copy has just landed. So the
// shelf's first step is -a1 on its output delay and the low shelf closes by x2, which is the half the
// designer took out, while the high shelf closes by x4 because Shelves::highScale is 2 and its route
// puts the other half back.
void input_eq(yss236_core& c, int st)
{
	EqChannel const& e = EQ[(st - 0x1a0) / 7];
	int const k = (st - e.base) % 7;
	if (k == 0)
	{
		// k = 0x4000 on the x2 route, which is unity: the channel's own copy of the signal the base
		// program leaves the window
		gain(c, st, e.sig, e.copy, ROUTE_2);
		return;
	}
	bool const low = k < 4;                    // which of the two shelves this step belongs to
	int const in = low ? e.copy : e.low;       // the shelf's input, which b1 and b0 read
	int const out = low ? e.low : e.high;      // and its output delay, which -a1 reads a pass back
	switch ((k - 1) % 3)
	{
	case 0:  term(c, st, out); break;                                // the -a1 word
	case 1:  sum(c, st, in); break;                                  // b1 on the input a pass ago
	default: close(c, st, in, out, low ? ROUTE_2 : ROUTE_4); break;  // b0 closing the shelf
	}
}

// ---------------------------------------------------------------------------------------------
// The pairs of cells: the wah's own output, and the two sums the delay line is fed
// ---------------------------------------------------------------------------------------------

// 0x1ae and 0x1af: ins::WAH_MODE, the two channels as one signal. wahDistortionWet() puts SQRT_HALF in
// both words, so r[79] is m[43] and m[44] under that factor, which is the pair the level follower
// below differences and the two sums at 0x1c2 and 0x1c4 add their own word to.
void wah_mode(yss236_core& c, int st)
{
	if (st == 0x1ae) term_cell(c, st, WAH_L);                          // ins::WAH_MODE[0] on m[43]
	else cell_sum(c, st, WAH_R, ROUTE_1, CLAMP_SAT, LEVEL, 0);          // ins::WAH_MODE[1] on m[44]
}

// One of the level follower's two three-step blocks. `in` is the signal it smooths and `state` the
// register it smooths it into, which the first step reads as the state's own delay: the pair is the
// program has instead of an envelope generator, and it is what makes the wah's filter follow its own
// level. Each block weights the state at about half a percent under and the new value over, then
// doubles the difference.
struct Smoother { int base; int in, state; };

const Smoother SMOOTHERS[2] = {
    { 0x1b0, LEVEL, LVL_1 },     // the difference of the signal, into r[80]
    { 0x1b3, LVL_1, LVL_2 },     // and the same again one state on, into r[81]
};

void smoother(yss236_core& c, int st, Smoother const& s)
{
	switch (st - s.base)
	{
	case 0:  term(c, st, s.state); break;                    // the state under the first word
	case 1:  sum(c, st, s.in); break;                        // the signal a pass ago on top of it
	default: close(c, st, s.in, s.state, ROUTE_2); break;   // and the difference, doubled
	}
}

// ---------------------------------------------------------------------------------------------
// The three steps of ins::WAH_DRIVE, the four taps and ins::WAH_BAND's five
// ---------------------------------------------------------------------------------------------

// ins::WAH_DRIVE[0], [1] and [2] are the three steps storeDrive() writes one word to. Each is the x16
// gain on r[119], and it saturates: the drive accumulates on itself, once from the level the follower
// leaves in r[81] and twice from what the step before wrote. They sit four steps apart, on either side
// of the state 0x1bb leaves and the feedback 0x1be takes, which is why this is a table the step loop
// looks in rather than a block.
struct Drive { int st; int src; };

const Drive DRIVE[3] = {
    { 0x1b8, LVL_2 },      // the level into the drive
    { 0x1bc, DRY },
    { 0x1c0, DRY },
};

Drive const* drive_for(int st)
{
	for (Drive const& d : DRIVE)
		if (d.st == st) return &d;
	return nullptr;
}

// The four taps of the delay line, ins::WD_SL[1] to [4]. The read each one rides on is in DECORS above,
// which is what puts the slot's word into the capture history five steps before its cell takes it up;
// what is here is what the step then does with the running value. `in` is the register the multiplier
// scales and `cell` the cell it latches, `m2` says which of the two, and the first of the four closes
// into the line's head while the other three only move the running value.
struct Tap { int st; int in, cell; m2_t m2; bool sub; clamp_t clamp; int dr; };

const Tap TAPS[4] = {
    { 0x1c7,  0,      ATTACK, M2_CELL, false, CLAMP_SAT,  LINE_A },   // the first tap into the head
    { 0x1cb,  LINE_A, 0,      M2_REG,  true,  CLAMP_ZERO, 0 },        // the second, less the head
    { 0x1cf,  LINE_B, 0,      M2_REG,  false, CLAMP_WRAP, 0 },        // and the third, plus the head
    { 0x1d3,  0,      LFO_CELL, M2_CELL, false, CLAMP_WRAP, 0 },      // the fourth, under a word of zero
};

Tap const* tap_for(int st)
{
	for (Tap const& t : TAPS)
		if (t.st == st) return &t;
	return nullptr;
}

void tap(yss236_core& c, int st, Tap const& t)
{
	run(c, {st, t.in, t.cell, t.m2, false, 0, ADD_ACC, t.sub, ROUTE_1, t.clamp, t.dr});
}

// ins::WAH_BAND's series, the five steps storeEdge() writes and no other step of the table. r[120] to
// r[123] are the states it runs through, each handed to the next on its own word by the x16 route, and
// the fifth brings the series back to r[119], which is the signal that leaves the window dry. The edge's
// gain word is the first of them, and the three after it are ins::WAH_BAND[1], [2] and [3].
struct Band { int st; int in, out; };

const Band BAND[5] = {
    { 0x1c5, DRY,     LINE_IN },     // the edge's gain word, into the delay line's input
    { 0x1cd, LINE_IN, DRY_L },       // ins::WAH_BAND[1]
    { 0x1d1, DRY_L,   BAND_2 },      // ins::WAH_BAND[2]
    { 0x1d5, BAND_2,  BAND_3 },      // ins::WAH_BAND[3]
    { 0x1df, BAND_3,  DRY },         // ins::WAH_BAND[4]
};

Band const* band_for(int st)
{
	for (Band const& b : BAND)
		if (b.st == st) return &b;
	return nullptr;
}

// ---------------------------------------------------------------------------------------------
// The blocks of steps that are a table of their own
// ---------------------------------------------------------------------------------------------

// 0x1b6 to 0x1bb: the two high shelves as one signal, and a state with no input of its own.
// ins::WAH_MODE2 sums the shelves into r[73], the mono signal the resonator below runs on. 0x1b9 and
// 0x1ba are one word and its opposite on the same register, so between them they clear the running
// value and leave nothing for 0x1bb to add, which writes r[74] as a state with no input: the value
// already there under ins::WAH_HPF_2[0].
void mode_pair(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b6: term(c, st, EQ_L_HIGH); break;                       // the host's 0x5a81, on the left
	case 0x1b7: close(c, st, EQ_R_HIGH, RES_IN, ROUTE_1); break;     // and the right shelf on the same word
	case 0x1b9: term(c, st, RES_IN); break;                          // ins::WAH_HPF_2[1]
	case 0x1ba: sum(c, st, RES_IN); break;                           // ins::WAH_HPF_2[2], the other sign
	default:   close(c, st, RES_STATE, RES_STATE, ROUTE_2); break;   // ins::WAH_HPF_2[0], and no input
	}
}

// 0x1bd to 0x1bf: the resonator's integrator, its feedback and the attack. 0x1bd takes the integrator
// cell less the signal driving the resonator, which is the difference 0x1eb accumulates into m[42]
// again. 0x1be is ins::WAH_SENSE[0], the word wahSense() builds from the resonance, on r[78] less
// that difference, and m[56] takes the result, so the state grows by its own feedback each pass.
// 0x1bf is ins::WAH_ATTACK[0] on the state 0x1bb leaves, and its clamp takes the absolute value.
void resonator_feedback(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1bd:
		// the multiplier reads the integrator cell and the adder the register the step has just latched,
		// negated, which is why the two latches come before the arithmetic and not after it
		run(c, {st, RES_IN, INTEG, M2_CELL, false, 0, ADD_REG, true, ROUTE_1, CLAMP_WRAP});
		break;
	case 0x1be:
		// ins::WAH_SENSE[0] on r[78] less the difference, and m[56] takes the result
		run(c, {st, RES, 0, M2_REG, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_SAT, 0, false, FB});
		break;
	default:
		// ins::WAH_ATTACK[0] on r[74] by the x16 route, and the clamp takes the absolute value of it
		run(c, {st, RES_STATE, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_ABS, 0, false, ATTACK});
		break;
	}
}

// 0x1c1 to 0x1c4: the two sums the delay line is fed, the same pair written twice. Each opens on the
// follower cell m[47] with the adder off and then adds a word on one of the wah's own output cells,
// four times and saturated, into a cell of its own: ins::WAH_SENSE[1] into m[58], and the same sum
// with the word at 0x1c4 in place of the resonance word into m[59]. Nothing writes that second word.
struct WetSum { int base; int out; int cell; };

const WetSum WET_SUMS[2] = {
    { 0x1c1, WAH_L, WET_L },
    { 0x1c3, WAH_R, WET_R },
};

void wet_sums(yss236_core& c, int st)
{
	WetSum const& s = WET_SUMS[(st - 0x1c1) / 2];
	if (st == s.base) term_cell(c, st, FOL_C);                       // the level the follower keeps
	else cell_sum(c, st, s.out, ROUTE_4, CLAMP_SAT, 0, s.cell);      // and the wah's output on top of it
}

// 0x1c6 to 0x1d0, less the taps: the delay line's head and the accumulator the third tap feeds. r[75]
// and r[76] hold the line's signal one delay word back and are written one sample apart around the
// reads that refill them, which is why 0x1c6 opens on r[75] less m[57] and 0x1c8 to 0x1ca build the
// other register around r[76]. The three taps in that range and ins::WAH_BAND's two steps are taken
// by the tables above before this runs, so what is left is the head itself and the accumulator's pair.
void line_head(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1c6:
		// ins::WAH_MIRROR[1]: the word on r[75] less m[57], and the clamp holds the negatives at zero
		run(c, {st, LINE_A, ATTACK, M2_REG, false, 0, ADD_CELL, true, ROUTE_1, CLAMP_ZERO});
		break;
	case 0x1c8:
		term(c, st, LINE_B); break;
	case 0x1c9:
		// r[76] on its own word added to the running value over 32768: the step latches no cell, so the
		// adder's m input is the running value scaled down
		run(c, {st, LINE_B, 0, M2_REG, false, 0, ADD_CELL, false, ROUTE_1, CLAMP_WRAP});
		break;
	case 0x1ca:
		// m[57] on its own word less the running value, which leaves the line's input for 0x1dc
		run(c, {st, 0, ATTACK, M2_CELL, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_WRAP});
		break;
	case 0x1cc:
		// m[60] takes the read 0x1c7 made, and r[76] is m[57] on its word added to the running value
		cell_sum(c, st, ATTACK, ROUTE_1, CLAMP_SAT, LINE_B, 0);
		break;
	case 0x1ce:
		term(c, st, LINE_B); break;
	default:
		// 0x1d0: m[61] takes the read 0x1cf made, and r[77] is its own value on the accumulator's word
		close(c, st, LINE_ACC, LINE_ACC, ROUTE_1);
		break;
	}
}

// 0x1d2 and 0x1d4: the resonator's drive. 0x1d2 is a class-1 load of the ROM's own word into the
// running value and nothing else: no register, no cell and no t register takes it, so all it does is
// seed the sum the tap at 0x1d3 adds to. 0x1d4 closes that sum into r[124] and hands the result to
// t[9] as a 1.15 word, and 0x1d9, 0x1dd, 0x1e6 and 0x1eb are the four steps that multiply by it.
void resonator_drive(yss236_core& c, int st)
{
	if (st == 0x1d2)
	{
		load(c, st, 0, 0);
		return;
	}
	// m[62] takes the read 0x1d3 made, and the drive is the accumulator's word on r[77] added to the
	// running value; t[9] takes the result
	t_put(c, RES_GAIN, run(c, {st, LINE_ACC, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, RES_DRIVE}));
}

// 0x1d6 to 0x1dc: the delay line's input. 0x1d6 and 0x1d7 average the first tap over two samples,
// m[48] being the step before's m operand, and 0x1d8 puts the sum through r[85]. 0x1d9 scales the
// resonator's drive by the gain t[9] took at 0x1d4 and writes t[9] again from the result, which is the
// loop's own signal. 0x1da and 0x1db are the two wet cells half each, and 0x1dc is the line's write:
// ins::WD_FB on r[85] added to that, and the RAM takes the clipped result.
void line_input(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1d6:
		term_cell(c, st, TAP_1); break;
	case 0x1d7:
		// m[48] takes the step before's m operand, and the tap is that cell on its own word added to the
		// running value, which is the first tap averaged over two samples
		cell_sum(c, st, TAP_PREV, ROUTE_1, CLAMP_WRAP, 0, TAP_PREV, true);
		break;
	case 0x1d8:
		// m[63] takes the read 0x1cb made, and r[85] is its own value on the running value, doubled
		close(c, st, TAP_AVG, TAP_AVG, ROUTE_2);
		break;
	case 0x1d9:
		// r[124] = t[9] * r[124]: the drive under the gain 0x1d4 took, and t[9] takes the result
		t_put(c, RES_GAIN,
		      run(c, {st, RES_DRIVE, 0, M2_REG, false, RES_GAIN, ADD_OFF, false, ROUTE_1, CLAMP_SAT, RES_DRIVE}));
		break;
	case 0x1da:
		term_cell(c, st, WET_L); break;
	case 0x1db:
		cell_sum(c, st, WET_R, ROUTE_2, CLAMP_WRAP, 0, 0);     // m[59] half: the two cells, one each
		break;
	default:
		// ins::WD_FB on r[85] added to the running value, and the slot 0x77 takes the clipped result.
		// The wdata bit is set, so the step drops any latch an earlier step left and writes its own.
		run(c, {st, TAP_AVG, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT,
		        0, false, 0, false, false, true, true});
		break;
	}
}

// 0x1dd and 0x1de: t[9] again, the loop's own gain. 0x1dd multiplies r[124] by the gain the file
// holds and 0x1de, a class-1 step, adds the ROM's own word to that and hands the result to t[9]. The
// word at 0x1dd is ins::WAH_BAND[6], which storeEdge() sets in the host's coef[] but never writes to
// the chip, so the step's own constant is unused and the multiplier's word is t[9].
void loop_gain(yss236_core& c, int st)
{
	if (st == 0x1dd)
	{
		run(c, {st, RES_DRIVE, 0, M2_REG, false, RES_GAIN, ADD_OFF, false, ROUTE_1, CLAMP_WRAP});
		return;
	}
	load(c, st, 0, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT);
	t_put(c, RES_GAIN, c.acc);
}

// 0x1e0 to 0x1e5: the edge's sum. The four states ins::WAH_BAND's series left are added to r[119]
// with a fifth of each, the word storeEdge() writes into its steps 7 to 11, and ins::WAH_DRIVE[3]
// closes the sum back into r[119], which is the signal that leaves the window dry.
struct EdgeSum { int base; int in; int state[4]; int out; };

const EdgeSum EDGE_SUM = { 0x1e0, DRY, { LINE_IN, DRY_L, BAND_2, BAND_3 }, DRY };

void edge_sum(yss236_core& c, int st, EdgeSum const& e)
{
	switch (st - e.base)
	{
	case 0:  term(c, st, e.in); break;                     // the running value cleared
	case 5:  close(c, st, e.in, e.out, ROUTE_1); break;    // ins::WAH_DRIVE[3]
	default: sum(c, st, e.state[st - e.base - 1]); break;  // a fifth of each state
	}
}

// 0x1e6 to 0x1ea: the resonator and ins::WAH_LPF2's other four words. 0x1e6 is the resonator itself,
// r[78] = r[78] + t[9] * m[56], so the state grows by its own feedback; it is ins::WAH_LPF2[2], but
// its multiplier input is t[9] rather than the word at the step, so that word is not read here. The
// four steps after it are the section's other words: the third and fourth taps, each added to m[62] on
// its own word, closing into the two registers the section ends in, which are the delay line's input
// and the left dry signal.
struct Lpf2 { int base; int cell; int out; };

const Lpf2 LPF2_TAPS[2] = {
    { 0x1e7, TAP_4, LINE_IN },     // ins::WAH_LPF2[1] and [4]
    { 0x1e9, TAP_3, DRY_L },       // and ins::WAH_LPF2[3] and [0]
};

void resonator(yss236_core& c, int st)
{
	if (st == 0x1e6)
	{
		// the multiplier reads t[9] and m[56] and the adder the register the step has just latched, so
		// the state is r[78] * (1 + t[9] * WAH_SENSE[0])
		run(c, {st, RES, FB, M2_CELL, false, RES_GAIN, ADD_REG, false, ROUTE_1, CLAMP_SAT, RES});
		return;
	}
	Lpf2 const& l = LPF2_TAPS[(st - 0x1e7) / 2];
	if (st == l.base) term_cell(c, st, l.cell);
	else cell_sum(c, st, TAP_3, ROUTE_1, CLAMP_SAT, l.out, 0);
}

// 0x1eb and 0x1ec: the integrator again, and the peak's input. m[42] takes t[9] times r[78] added to
// what it held, which is what 0x1bd differences, and r[82] takes the dry signal on the first of
// ins::WAH_PEAK's own words.
void integrator(yss236_core& c, int st)
{
	if (st == 0x1eb)
	{
		// the multiplier reads t[9] and r[78], the adder the cell the step has just latched, and m[42]
		// takes the result
		run(c, {st, RES, INTEG, M2_REG, false, RES_GAIN, ADD_CELL, false, ROUTE_1, CLAMP_SAT, 0, false, INTEG});
		return;
	}
	gain(c, st, DRY, PEAK_IN, ROUTE_1);         // r[82] = k * r[119], the peak's input
}

// 0x1ed to 0x1f2: ins::WAH_PEAK, the distortion's mid peak. designFilter() writes its five words here
// in the order (b0, b1, b2, -a1, -a2) and the program runs the section through r[82], r[83] and r[84]
// in another: the section's own output first, then the input as it stood a pass ago, then b0 closing
// it, then the second output delay and its -a2 term. The last step carries rcopy, so r[84] is the
// peak's x[-1] and the section is in direct form I.
void wah_peak(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1ed: term(c, st, PEAK_OUT); break;                             // ins::WAH_PEAK[1]
	case 0x1ee: sum(c, st, PEAK_IN); break;                              // ins::WAH_PEAK[3]
	case 0x1ef: close(c, st, PEAK_IN, PEAK_OUT, ROUTE_2); break;         // ins::WAH_PEAK[2]
	case 0x1f0: term(c, st, PEAK_X1); break;                             // ins::WAH_PEAK[4]
	case 0x1f1: sum(c, st, PEAK_OUT); break;                             // b2 on the output a pass ago
	default:   copy_z(c, st, PEAK_OUT, PEAK_X1, ADD_ACC, CLAMP_WRAP); break;   // ins::WAH_PEAK[0]
	}
}

// 0x1f3 to 0x1f7: the follower m[47], which the two sums at 0x1c2 and 0x1c4 add their own word to.
// m[45] is a saturating integrator of the running value, m[46] is m[45] a pass ago, and m[47] smooths
// m[45] against m[46]. 0x1f5 is the step that publishes the integrator into m[46]: its own result is
// not a cell write, so what m[46] takes is the m operand of the step before.
void follower(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1f3:
		sum_cell(c, st, FOL_B); break;
	case 0x1f4:
		cell_sum(c, st, FOL_A, ROUTE_2, CLAMP_SAT, 0, FOL_A); break;    // m[45], the integrator
	case 0x1f5:
		// m[46] takes the step before's m operand, and the running value is m[47] on its own word
		run(c, {st, 0, FOL_C, M2_CELL, false, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, false, FOL_B, true});
		break;
	case 0x1f6:
		sum_cell(c, st, FOL_A); break;
	default:
		cell_sum(c, st, FOL_A, ROUTE_4, CLAMP_SAT, 0, FOL_C); break;    // m[47], the level the sums take
	}
}

// 0x1f8 to 0x1fb: the wah's own output as two cells. Both pairs open on the other channel's high shelf
// under the word the ROM left at that step, which is zero, so the opening step is there to clear the
// running value and the closing step does the work: r[78] a pass on, four times, into a cell. These two
// are the signals the program's first steps summed into r[79].
struct WahOut { int base; int clear; int cell; };

const WahOut WAH_OUT[2] = {
    { 0x1f8, EQ_L_HIGH, WAH_L },
    { 0x1fa, EQ_R_HIGH, WAH_R },
};

void wah_output(yss236_core& c, int st)
{
	WahOut const& w = WAH_OUT[(st - 0x1f8) / 2];
	if (st == w.base) term(c, st, w.clear);
	// r[78] a pass on, four times, into the cell: the multiplier reads the register here, not a cell,
	// so this is the register's own word rather than cell_sum()'s
	else run(c, {st, RES, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_4, CLAMP_SAT, 0, false, w.cell});
}

// 0x1fc to 0x1ff: ins::WD_LEVEL, the two outputs. The dry signal reaches r[120] and r[121] through
// ins::WAH_BAND and 0x1cd, and the wet path reaches m[58] and m[59], so each output is one cell's word
// on its own with the adder off and then the other word added to it, four times and saturated, into
// the register the base program reads back.
struct Output { int base; int wet; int dry; int reg; };

const Output OUTPUTS[2] = {
    { 0x1fc, WET_L, LINE_IN, OUT_L },     // ins::WD_LEVEL[0], the mix word, into r[17]
    { 0x1fe, WET_R, DRY_L, OUT_R },       // ins::WD_LEVEL[2], into r[18]
};

void output_mix(yss236_core& c, int st, Output const& o)
{
	if (st == o.base) term_cell(c, st, o.wet);
	else close(c, st, o.dry, o.reg, ROUTE_4);
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's. There is no gate or cskip step in this program, so
// the guard is the only control the loop needs.
//
// Four tables are read before the ranges below, because the steps they name are spread through the
// window rather than sitting in one place: ins::WAH_DRIVE's three, ins::WAH_BAND's five and the four
// taps, and the decor table takes each step's cell move and delay RAM read before any of them.
void block_0x1a0(yss236_core& c)
{
	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		// the three serial inputs this window publishes, the four delay RAM reads and the four captures
		// that go with them, all before the step's own arithmetic
		publish(c, st);

		// ---- ins::WAH_DRIVE[0], [1] and [2]: the same x16 gain on r[119], three steps apart ----
		if (Drive const* d = drive_for(st)) { gain(c, st, d->src, DRY, ROUTE_16); continue; }

		// ---- ins::WAH_BAND's series, r[120] to r[123] and back to r[119] ----
		if (Band const* b = band_for(st)) { gain(c, st, b->in, b->out, ROUTE_16); continue; }

		// ---- the four taps of the delay line, whose reads are in the decor table above ----
		if (Tap const* t = tap_for(st)) { tap(c, st, *t); continue; }

		// ---- 0x1a0 to 0x1ad: the input EQ, a copy and two shelves per channel ----
		if (st < 0x1ae) { input_eq(c, st); continue; }

		// ---- 0x1ae and 0x1af: ins::WAH_MODE, the two channels as one signal ----
		if (st < 0x1b0) { wah_mode(c, st); continue; }

		// ---- 0x1b0 to 0x1b5: the level follower, two blocks of the same three steps ----
		if (st < 0x1b6) { smoother(c, st, SMOOTHERS[(st - 0x1b0) / 3]); continue; }

		// ---- 0x1b6 to 0x1bb: ins::WAH_MODE2 and ins::WAH_HPF_2[0], around the first drive step ----
		if (st < 0x1bc) { mode_pair(c, st); continue; }

		// ---- 0x1bd to 0x1bf: the resonator's integrator, its feedback and the attack ----
		if (st < 0x1c1) { resonator_feedback(c, st); continue; }

		// ---- 0x1c1 to 0x1c4: the two sums the delay line is fed ----
		if (st < 0x1c6) { wet_sums(c, st); continue; }

		// ---- 0x1c6 to 0x1d0, less the taps: the delay line's head and its accumulator ----
		if (st < 0x1d2) { line_head(c, st); continue; }

		// ---- 0x1d2 and 0x1d4, with the fourth tap between them: the resonator's drive ----
		if (st < 0x1d6) { resonator_drive(c, st); continue; }

		// ---- 0x1d6 to 0x1dc: the delay line's input and its write ----
		if (st < 0x1dd) { line_input(c, st); continue; }

		// ---- 0x1dd and 0x1de: t[9], the loop's own gain ----
		if (st < 0x1e0) { loop_gain(c, st); continue; }

		// ---- 0x1e0 to 0x1e5: the four states ins::WAH_BAND left, summed into the dry signal ----
		if (st < 0x1e6) { edge_sum(c, st, EDGE_SUM); continue; }

		// ---- 0x1e6 to 0x1ea: the resonator, and ins::WAH_LPF2's other four words ----
		if (st < 0x1eb) { resonator(c, st); continue; }

		// ---- 0x1eb and 0x1ec: the integrator again, and the peak's input ----
		if (st < 0x1ed) { integrator(c, st); continue; }

		// ---- 0x1ed to 0x1f2: ins::WAH_PEAK, the distortion's mid peak ----
		if (st < 0x1f3) { wah_peak(c, st); continue; }

		// ---- 0x1f3 to 0x1f7: the follower m[47] ----
		if (st < 0x1f8) { follower(c, st); continue; }

		// ---- 0x1f8 to 0x1fb: the wah's own output, as m[43] and m[44] ----
		if (st < 0x1fc) { wah_output(c, st); continue; }

		// ---- 0x1fc to 0x1ff: ins::WD_LEVEL, the two outputs ----
		output_mix(c, st, OUTPUTS[(st - 0x1fc) / 2]);
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m12(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
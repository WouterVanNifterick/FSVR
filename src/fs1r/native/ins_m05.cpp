// fs1r/native/ins_m05.cpp - VOP3-2, the insertion window's program for Distortion, Overdrive, Amp Sim,
// Compressor, Comp+Dist, HM Enhancer and Noise Gate: its 96 steps.
//
// rom::INSERTION_PROGRAMS[5], the program the insertion window holds under the seven types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 5: ins-28 Distortion and ins-30 Overdrive on
// EffectHost::distortion, ins-32 Amp Sim on EffectHost::ampSimulator, ins-24 Compressor on
// EffectHost::compressor, ins-25 Comp+Dist on EffectHost::compressorDistortion, ins-22 HM Enhancer on
// EffectHost::enhancer and ins-23 Noise Gate on EffectHost::noiseGate. One program, seven types: the
// handlers differ only in which words they write, so nothing here knows which of them is running.
// Nothing in this program touches the delay RAM, and no step of it is a control step.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware
// chose is folded in here: the program words are fixed per row and each step's fields are folded into
// the arithmetic below in the order yss236_core::step() runs its own.
//
//   0x1a0-0x1af  the inputs and the two channels' input filters. 0x1a0, 0x1a7 and 0x1aa are
//                ins::DIST_DRIVE's first three words, the signal into the shaper multiplied by the
//                drive three times. 0x1a1 takes the right input the base program leaves the window
//                into r[71] and 0x1a8 and 0x1a9 the left one into r[67], both through the ROM's own
//                fixed words. 0x1a2 to 0x1a6 are ins::DIST_INPUT_C, the right channel's order-2 input
//                filter over r[71] into r[73], and 0x1ab to 0x1af ins::DIST_INPUT, the left one's
//                over r[67] into r[69]. 0x1a5, 0x1a6 and 0x1a7 also take the two serial inputs of page
//                13 into m[3] and m[7] and that page's LFO into m[13], which nothing here reads: the
//                base program does.
//   0x1b0-0x1bb  the shaper and the detector. 0x1b0 is ins::DIST_BAND[1], the gain word of the shape
//                storeEdge() derives, and 0x1b1 and 0x1b2 are ins::HM_DRIVE, which scale the two
//                channels' filtered signals into r[120] and r[119] for the detector. 0x1b4 and 0x1b5
//                are ins::DIST_LEVEL, the two words that add the two channels and rectify them into
//                m[49], the level. 0x1b8 to 0x1ba are ins::DYN_ATTACK, the smoother storeCompAttack()
//                or storeGateAttack() writes, and 0x1bb takes m[42] one pass old into m[43].
//   0x1bc-0x1c1  the detector's two states and the release. 0x1bc and 0x1c1 are class-3 steps, which
//                take m[49] at unity gain, and 0x1bf the third: they add the level to what the
//                release words left. 0x1b6, 0x1be and 0x1c0 are ins::DYN_RELEASE.
//   0x1c2-0x1cb  the shaper's sum and the drive's gain. 0x1c2 to 0x1c8 are ins::DIST_BAND[5] to
//                [11], the polynomial the shaper adds to its chain: 0x1c2 finishes the monomial and
//                0x1c3 to 0x1c8 weigh the chain's registers and add them into r[1]. Only 0x1c3 has the
//                adder off; the other five add to the running value, and 0x1c8 closes it into r[1].
//                0x1c9 and 0x1ca multiply the level by t[9] and pass each on in t[10] and t[11], and
//                0x1cb is ins::DIST_DRIVE[3], the gain storeDrive() derives from the drive and the
//                output level.
//   0x1cc-0x1db  the threshold and the three tone filters. 0x1cc is ins::DYN_THRESHOLD, over r[76]
//                into r[123]. 0x1cd to 0x1d1 are ins::DIST_PRE and 0x1d2 to 0x1d6 ins::DIST_PRE_C, the
//                same five-word filter twice over r[79], and 0x1d7 to 0x1db ins::DIST_EQ over r[83]
//                into r[85]. designFilter() writes b0, b1, b2, -a1, -a2 in that order, and the schedule
//                runs them the other way round with the z^-1 copies on the b2 steps and, in three of
//                the five sections, on the -a2 step as well: 0x1d0 and 0x1d5 read r[82] and r[84]
//                without writing them back, so those two carry only one copy.
//   0x1dc-0x1ec  the output level and the output mix. 0x1dc and 0x1e6 are the ROM's own pair: they
//                multiply r[119] and r[120] by the gains t[10] and t[11] into r[126] and r[127]. 0x1dd
//                and 0x1de are ins::DIST_MODE, and 0x1df to 0x1e2 ins::DIST_OUTPUT, the filter over
//                r[126] whose state is m[45]; 0x1e3 to 0x1e5 are ins::DIST_WET, the left mix into r[17]
//                and the right one into r[18]. 0x1e7 to 0x1ec are the same four steps on the right:
//                ins::DIST_MODE is the ROM's own word here, ins::DIST_OUTPUT_C over r[127] into m[46],
//                and r[18] takes that.
//   0x1ed-0x1ff  the gain curve. 0x1ed to 0x1f4 are eight class-1 steps off r[123], each clamping
//                negatives to zero, and they are the curve's breakpoints: r[123] less a quarter, a
//                half, one, two, four, eight, sixteen and sixty-four. 0x1f5 to 0x1fc are ins::DYN_RATIO,
//                the eight weights of the segments between them, summed into the running value, and
//                0x1fd takes that biased and clipped into m[42]. 0x1fe and 0x1ff are ins::DIST_WET's
//                dry words, a quarter of the two inputs into m[47] and m[48].
//
// So the seven types are one chain: a fixed two-channel input filter, a peak detector on the sum of
// the two channels, an eight-segment gain curve off the detector, a smoother, a five-multiply shape,
// three tone filters and a dry and wet mix. The gate and the compressor use the whole of it; the
// distortion and the enhancer reach it through ins::DIST_MODE. The old model in vop3_effects.h runs
// tanh and a power law for the shape, one envelope and a hard ratio for the dynamics, and a shelf
// plus a peak; it has none of this program's words, so it is no reference for any number here.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[1] to r[6] the
// shape's chain and then the curve's breakpoints; r[17] and r[18] the two outputs; r[67] to r[74] the
// two input filters and their states; r[75], r[76] and r[121] the detector's states; r[79] the
// shaper's output; r[80], r[82], r[84], r[86] the three tone filters' delays; r[81], r[83] the first
// two tone filters' outputs and r[85] the third's; r[119] to r[121] the detector's input scale; r[123]
// the thresholded level; r[124], r[125] the level times the smoother, and then the curve's last two
// breakpoints; r[126], r[127] the output's input scale. The cells: m[3], m[7] the serial inputs this
// block takes for the base program, m[4] from page 14 and m[13] the page 13's LFO; m[42], m[43] the
// curve's output and that one pass old; m[44] the smoother's state; m[45] the left output filter's
// state; m[46] the right one's; m[47], m[48] the two channels' dry signals; m[49] the level.
//
// The three t registers the program uses are t[9], written at 0x1ba, read at 0x1c9 and 0x1ca, the
// smoother's gain; and t[10] and t[11], written at 0x1c9 and 0x1ca and read at 0x1dc and 0x1e6, that
// same gain squared onto the two channels.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled
// out by name rather than a step at a time.
//
// vop3_native_check ins-m05: identical, 7 types, 67200 native passes.
#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, load and the rest

// ---------------------------------------------------------------------------------------------
// The registers and cells the header names, so the tables below can say what they read
// ---------------------------------------------------------------------------------------------

// The two signals the base program leaves this window and the two outputs it reads back; the head of
// the shaper's chain, whose five links the BIQUADS table names; the two input filters and the three
// tone filters; the detector's two states and the levels off them; and the four registers each output
// channel's half works through. The cells are the four this window takes for the base program and the
// seven it keeps for itself: the level, the smoother, the two output filter states, the two dry
// signals, and the gain curve with its one-pass-old copy.
enum
{
	CHAIN = 1,        // r[1] to r[6]: the shaper's chain, then the gain curve's breakpoints
	SIG_L = 7,        // r[7] and r[8], the panned sends the base program leaves the window
	SIG_R = 8,
	OUT_L = 17,       // r[17] and r[18], the two outputs the base program reads back
	OUT_R = 18,
	IN_L = 67,        // r[67] to r[70]: the left channel's input filter, its input, x[-1], output and
	FILT_L = 69,      // y[-2], the second of them being the filtered signal the detector works from
	IN_R = 71,        // r[71] to r[74]: the right channel's, the same four registers
	FILT_R = 73,
	PEAK = 75,        // r[75] and r[76], the detector's two states: the level plus a decaying term each
	PEAK2 = 76,
	OUT_L_IN = 77,    // r[77] and r[78], the two output filters' inputs
	OUT_R_IN = 78,
	SHAPED = 79,      // r[79], the polynomial at the drive's gain, which the tone filters work from
	PRE = 81,         // r[81] and r[83], the first two tone filters' outputs and so the next filters'
	PRE_C = 83,       // inputs; r[85] is the third's and the signal both output mixes weight
	EQ = 85,
	DET_L = 119,      // r[119] and r[120], the two channels at ins::HM_DRIVE's gain
	DET_R = 120,
	REL = 121,        // r[121], the release ramp 0x1b6 leaves for the detector's first state
	THRESH = 123,     // r[123], the second state past the threshold, which the gain curve hangs off
	LEV_L = 124,      // r[124] and r[125], the level times the smoother, and then the curve's last
	LEV_R = 125,      // two breakpoints, reused after the output stage has read them
	SCALE_L = 126,    // r[126] and r[127], the same level at the output stage's gain
	SCALE_R = 127,
	SERIAL_L = 3,     // m[3] and m[7], the serial inputs of page 13, which the base program reads
	SERIAL_R = 7,
	SERIAL_A = 4,     // m[4], page 14's left input, which nothing in this window reads back
	LFO_CELL = 13,    // m[13], page 13's LFO
	CURVE_OUT = 42,   // m[42], the gain curve's output, and m[43] that one pass old
	CURVE_OLD = 43,
	SMOOTH = 44,      // m[44], the smoother's state
	STATE_L = 45,     // m[45] and m[46], the two output filters' states, which the shaper reads back
	STATE_R = 46,
	DRY_L = 47,       // m[47] and m[48], the two channels' dry signals, for the output mixes
	DRY_R = 48,
	LEVEL = 49,       // m[49], the level: the two channels summed and rectified
};

// ---------------------------------------------------------------------------------------------
// What a step moves into a cell before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// Four of the 96 steps take a value from the hardware side, and three of those publish a cell for the
// base program rather than for this window: the two serial inputs of page 13 and that page's LFO. The
// fourth takes page 14's left input into m[4], which nothing here reads. All four happen before the
// step's own arithmetic, because step() moves a cell before it multiplies or adds anything, so the
// step loop reads this table before it hands the step to a block.
enum decor_t { DEC_NONE, DEC_SERIAL_L, DEC_SERIAL_R, DEC_LFO };

struct Decor { int st; decor_t kind; int cell; };

constexpr Decor DECORS[4] = {
    //  the step  the decor                    the cell
    {   0x1a5,   DEC_SERIAL_L, SERIAL_L   },   // on the right channel's -a2 word
    {   0x1a6,   DEC_SERIAL_R, SERIAL_R   },   // on that section's b0
    {   0x1a7,   DEC_LFO,      LFO_CELL   },   // on ins::DIST_DRIVE[1]
    {   0x1c3,   DEC_SERIAL_L, SERIAL_A   },   // on ins::DIST_BAND[6], the polynomial's first word
};

// The decor a step carries, or null for the 92 that carry none. Every step of every pass asks this
// table first, and the four rows never move, so the step indexes to its row rather than reading the four.
constexpr auto DECORS_AT = index_by_step(DECORS);

Decor const* decor_for(int st) { return DECORS_AT[st] < 0 ? nullptr : &DECORS[DECORS_AT[st]]; }

void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_SERIAL_L: take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R: take_serial(c, d.cell, st, 1); break;
		case DEC_LFO:      take_lfo(c, d.cell, st); break;
		case DEC_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// The shaper's chain, r[1] to r[6]
// ---------------------------------------------------------------------------------------------

// One word of the chain: a register scaled by its word and by a route of 16, saturating into `out`.
// `cell` says the word scales a cell instead of a register, which only the first of them does: its
// source is m[45], the left output filter's state from the pass before, so the whole chain carries one
// pass of that filter in it, and that is what the seven types' shapes are cut against.
void shape_word(yss236_core& c, int st, int in, int out, int cell = 0)
{
	run(c, {.st = st, .sr = in, .sm = cell, .m2 = cell ? M2_CELL : M2_REG, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = out});
}

// The one word of the chain that publishes on its way out: m[43] takes the step before's own cell,
// which is m[42], the gain curve's output. The smoother at 0x1b8 reads it, so the curve reaches the
// shaper one pass late, which is what makes the dynamics a feedback path round the whole block.
void shape_word_published(yss236_core& c, int st, int in, int out)
{
	run(c, {.st = st, .sr = in, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = out, .dm = CURVE_OLD, .prev = true});
}

// ---------------------------------------------------------------------------------------------
// The five order-2 sections: the two input filters and the three tone filters
// ---------------------------------------------------------------------------------------------

// One section in direct form I, five steps: b1 on the input, b2 on x[-1], -a1 on y[-1], -a2 on y[-2],
// and b0 closing the sum into the output. designFilter() writes b0, b1, b2, -a1, -a2 in that order,
// and the schedule runs them the other way round with the z^-1 copies riding on the b2 and -a2 words,
// so `x1` holds the input a pass back and `y2` the output two passes back. All five sections carry the
// b2 copy; only three carry the -a2 one, `y2_copy` says which, and the two that do not read y[-2] out
// of a register nothing then advances. The closing step's route is the designer's own (16 >> scale)
// put back, which is why it is the only one of the five that has a route or saturates.
struct Biquad
{
	int st, nsteps;    // the section's five steps, and how many that is
	int in, x1, out, y2;
	route_t route;
	bool y2_copy;
};

constexpr Biquad BIQUADS[5] = {
    //   st  n  in  x1  out  y2  route   the -a2 copy
    { 0x1a2, 5, IN_R, IN_R + 1, FILT_R, FILT_R + 1, ROUTE_2,  true  },   // ins::DIST_INPUT_C, right channel
    { 0x1ab, 5, IN_L, IN_L + 1, FILT_L, FILT_L + 1, ROUTE_2,  true  },   // ins::DIST_INPUT, left channel
    { 0x1cd, 5, SHAPED, SHAPED + 1, PRE, PRE + 1, ROUTE_2,  false },   // ins::DIST_PRE, the first tone filter
    { 0x1d2, 5, PRE, PRE + 1, PRE_C, PRE_C + 1, ROUTE_2,  false },   // ins::DIST_PRE_C, over the first's
    { 0x1d7, 5, PRE_C, PRE_C + 1, EQ, EQ + 1, ROUTE_16,     true  },   // ins::DIST_EQ, over the second's
};

// The section a step belongs to, or null for the steps between the sections. The five steps of a
// section are consecutive, so a step indexes to its section and reads its place in it as st - row.st.
constexpr auto BIQUADS_AT = index_by_span(BIQUADS);

Biquad const* biquad_for(int st) { return BIQUADS_AT[st] < 0 ? nullptr : &BIQUADS[BIQUADS_AT[st]]; }

// One section's step k of five. Only the closing step saturates and routes: the other four are terms
// of a sum, which is what a direct form I section is.
void biquad_step(yss236_core& c, int st, Biquad const& b, int k)
{
	switch (k)
	{
	case 0: term(c, st, b.in); break;                                        // b1 on the input
	case 1: copy_z(c, st, b.x1, b.x1, ADD_ACC, CLAMP_WRAP); break;          // b2 on x[-1], and x[-1] kept
	case 2: sum(c, st, b.out); break;                                        // -a1 on y[-1]
	case 3:                                                             // -a2 on y[-2], where it is kept
		if (b.y2_copy) copy_z(c, st, b.y2, b.y2, ADD_ACC, CLAMP_WRAP);
		else          sum(c, st, b.y2);
		break;
	default: close(c, st, b.in, b.out, b.route); break;                      // b0, closing the section
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1a0 to 0x1af: the shaper's input and the two channels' input filters
// ---------------------------------------------------------------------------------------------

// The three drive words and the two channels' inputs. The schedule interleaves them rather than
// finishing one before the next, because the shaper's input is three words deep by the time the left
// filter has run: a word of the drive chain, then a channel in and its section, then the next word.
void inputs(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1a0:
		// ins::DIST_DRIVE[0]: the signal into the shaper, which is the left output filter's state
		shape_word(c, st, 0, CHAIN, STATE_L);
		break;
	case 0x1a1:
		// the ROM's own word: the right channel's input into r[71] at a half
		gain(c, st, SIG_R, IN_R, ROUTE_2);
		break;
	case 0x1a7:
		// ins::DIST_DRIVE[1]
		shape_word(c, st, CHAIN, CHAIN);
		break;
	case 0x1a8:
		// the ROM's own word on r[7], which the step after adds r[8] to
		term(c, st, SIG_L);
		break;
	case 0x1a9:
		// and the left channel's input, which is the two signals the base program leaves summed
		close(c, st, SIG_R, IN_L, ROUTE_2);
		break;
	case 0x1aa:
		// ins::DIST_DRIVE[2]
		shape_word(c, st, CHAIN, CHAIN);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1b0 to 0x1c1: the rest of the chain, the level, the smoother and the detector
// ---------------------------------------------------------------------------------------------

// One of the two release words, ins::DYN_RELEASE[0] and [2]: the detector's first state decayed by its
// word, less whatever the running value is, with the negatives held at zero. `dr` is the register
// that takes it, and only 0x1b6 has one: 0x1c0's result is the ramp itself, which 0x1c1 adds to the
// level to make the detector's second state.
void release(yss236_core& c, int st, int dr)
{
	run(c, {.st = st, .sr = PEAK, .add = ADD_ACC, .sub = true, .clamp = CLAMP_ZERO, .dr = dr});
}

// 0x1b0 to 0x1bb: four more words down the chain, with the level and the smoother between them. The
// level is what the detector and so the whole gain curve work from, so it is rectified and put in a
// cell, and the smoother is the one recurrence in the program: three words that average the curve's
// output against the state the pass before left, at ins::DYN_ATTACK's attack rate.
void shaper_chain(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b0:
		shape_word(c, st, CHAIN, CHAIN + 1); break;          // ins::DIST_BAND[1]
	case 0x1b3:
		shape_word(c, st, CHAIN + 1, CHAIN + 2); break;      // ins::DIST_BAND[2]
	case 0x1b7:
		shape_word(c, st, CHAIN + 2, CHAIN + 3); break;      // ins::DIST_BAND[3]
	case 0x1bb:
		shape_word_published(c, st, CHAIN + 3, CHAIN + 4); break;   // ins::DIST_BAND[4]
	case 0x1b1:
		// ins::HM_DRIVE[0]: the right channel at the detector's gain
		gain(c, st, FILT_R, DET_R, ROUTE_2);
		break;
	case 0x1b2:
		// ins::HM_DRIVE[1]: and the left channel's
		gain(c, st, FILT_L, DET_L, ROUTE_2);
		break;
	case 0x1b4:
		// ins::DIST_LEVEL[1]: the level's own word on the right channel
		term(c, st, DET_R);
		break;
	case 0x1b5:
		// ins::DIST_LEVEL[0]: the left channel added on top and the sum taken at its absolute value,
		// so m[49] is the peak of the two channels and not their total
		run(c, {.st = st, .sr = DET_L, .add = ADD_ACC, .route = ROUTE_16, .clamp = CLAMP_ABS, .dm = LEVEL});
		break;
	case 0x1b6:
		release(c, st, REL);                                 // ins::DYN_RELEASE[0]
		break;
	case 0x1b8:
		term_cell(c, st, CURVE_OLD);                         // ins::DYN_ATTACK[1], on the curve a pass old
		break;
	case 0x1b9:
		sum_cell(c, st, SMOOTH);                             // ins::DYN_ATTACK[2], plus the state
		break;
	default:
		// ins::DYN_ATTACK[0]: the curve's own word on top, into the state cell and out to t[9], which
		// is the gain the two channel scales at 0x1c9 and 0x1ca multiply by
		t_put(c, st, 9, run(c, {.st = st, .sm = CURVE_OUT, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dm = SMOOTH}));
		break;
	}
}

// 0x1bc to 0x1c1: the detector's two states and the release between them. Both states are the level
// plus a term that decays, so the pair is the peak the gain curve measures the level against: the level
// reaches both at once through the class-3 steps and falls only as fast as the release words allow.
// The three class-3 steps take m[49] at unity gain, their word being 1.0 rather than a coefficient.
void detector(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1bc:
		// r[75] = m[49] + r[121], the level plus the ramp 0x1b6 left
		run(c, {.st = st, .sr = REL, .sm = LEVEL, .m2 = M2_CELL, .unity = true, .add = ADD_REG, .clamp = CLAMP_SAT, .dr = PEAK});
		break;
	case 0x1bd:
		// the ROM's own word on the second state, ins::DYN_RELEASE[1]'s first half
		term(c, st, PEAK2);
		break;
	case 0x1be:
		// and the same word again with the running value added at 1/32768, which is the decay term in
		// register units: the second state is its own decay plus a little of what it was
		run(c, {.st = st, .sr = PEAK2, .add = ADD_CELL});
		break;
	case 0x1bf:
		// class 3: acc = m[49] - acc, how far the level has fallen below its own decay
		run(c, {.st = st, .sm = LEVEL, .m2 = M2_CELL, .unity = true, .add = ADD_ACC, .sub = true});
		break;
	case 0x1c0:
		// ins::DYN_RELEASE[2]: the ramp, the first state's decay less that, and never negative
		release(c, st, 0);
		break;
	default:
		// class 3: r[76] = m[49] + acc, the second state
		run(c, {.st = st, .sm = LEVEL, .m2 = M2_CELL, .unity = true, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = PEAK2});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1c2 to 0x1cb: the shape's polynomial, and the two channel scales out of the t file
// ---------------------------------------------------------------------------------------------

// One word of the shaper's polynomial, ins::DIST_BAND[6] to [11]: the chain's registers weighed and
// added into r[1]. Only the first word stands alone, with the adder off; the four in between add to
// the running value and the last closes the sum, which is also the only one that saturates. So the
// polynomial is the shape the drive's five multiplies reach the shaper with.
struct PolyTerm { int reg; bool closes; };

constexpr PolyTerm POLY[6] = {
	{ CHAIN,     false },     // ins::DIST_BAND[6], the only word with the adder off
	{ CHAIN + 1, false },     // ins::DIST_BAND[7]
	{ CHAIN + 2, false },     // ins::DIST_BAND[8]
	{ CHAIN + 3, false },     // ins::DIST_BAND[9]
	{ CHAIN + 4, false },     // ins::DIST_BAND[10]
	{ CHAIN,     true  },     // ins::DIST_BAND[11], closing the sum back into r[1]
};

void polynomial(yss236_core& c, int st, int k)
{
	PolyTerm const& p = POLY[k];
	run(c, {.st = st, .sr = p.reg, .add = k ? ADD_ACC : ADD_OFF, .clamp = p.closes ? CLAMP_SAT : CLAMP_WRAP, .dr = p.closes ? CHAIN : 0});
}

// The four steps that multiply by a t register, which is how the smoother's gain becomes a pole both
// the detector and the output stage read. The first pair scales the two channels by t[9] and passes
// each on squared in t[10] and t[11]; the second pair, on the output side of the block, applies those
// two to the same two channels. `tn` is the t register the result also lands in, zero for the two
// steps that write none.
struct TGain { int st; int src, dst; int treg, tn; };

constexpr TGain T_GAIN[4] = {
    //   st  src  dst  treg  tn
    { 0x1c9, DET_L, LEV_L,   9, 10 },     // the smoothed level of each channel, squared into its t register
    { 0x1ca, DET_R, LEV_R,   9, 11 },
    { 0x1dc, DET_L, SCALE_L, 10,  0 },     // and the output scale, which reads what those two left
    { 0x1e6, DET_R, SCALE_R, 11,  0 },
};

// The t step a step is, or null for the steps that are not one. Each row is a single step, so the step
// indexes straight to it.
constexpr auto T_GAIN_AT = index_by_step(T_GAIN);

TGain const* t_gain_for(int st) { return T_GAIN_AT[st] < 0 ? nullptr : &T_GAIN[T_GAIN_AT[st]]; }

void t_gain(yss236_core& c, int st, TGain const& g)
{
	double const y = run(c, {.st = st, .sr = g.src, .treg = g.treg, .clamp = CLAMP_SAT, .dr = g.dst});
	if (g.tn) t_put(c, st, g.tn, y);
}

// ---------------------------------------------------------------------------------------------
// 0x1dc to 0x1ec: the output stage
// ---------------------------------------------------------------------------------------------

// One channel's six steps from the mode mix to the end of its output filter. ins::DIST_MODE's two
// words add the smoothed level to the channel's own scale, the ROM's own word closes that sum at a
// sixteen, and the three steps after are the output filter ins::DIST_OUTPUT or ins::DIST_OUTPUT_C:
// b1 on its input, -a1 on the state, and b0 closing into that state. The state is a cell rather than
// a register because 0x1a0 reads it back into the shaper's chain, one pass behind.
struct ChannelOut { int st, nsteps; int mode_reg, scale_src, scale_reg, in, state; };

constexpr ChannelOut CHANNELS[2] = {
    //   st  n  mode  src  scale  in  state
    { 0x1dd, 6, LEV_L, DET_L, SCALE_L, OUT_L_IN, STATE_L },   // the left, off DIST_MODE and DIST_OUTPUT
    { 0x1e7, 6, LEV_R, DET_R, SCALE_R, OUT_R_IN, STATE_R },   // and the right, off DIST_OUTPUT_C
};

// The channel a step belongs to, or null for the steps between the two. The six steps of a channel are
// consecutive, so a step indexes to its channel and reads its place in it as st - row.st.
constexpr auto CHANNELS_AT = index_by_span(CHANNELS);

ChannelOut const* channel_for(int st) { return CHANNELS_AT[st] < 0 ? nullptr : &CHANNELS[CHANNELS_AT[st]]; }

void channel_step(yss236_core& c, int st, ChannelOut const& p, int k)
{
	switch (k)
	{
	case 0:
		term(c, st, p.mode_reg); break;                  // ins::DIST_MODE[0], on the smoothed level
	case 1:
		sum(c, st, p.scale_src); break;                  // ins::DIST_MODE[1], plus the channel's own scale
	case 2:
		close(c, st, p.scale_reg, p.in, ROUTE_16); break;   // the two together, the ROM's word at a sixteen
	case 3:
		term(c, st, p.in); break;                        // ins::DIST_OUTPUT[1], b1
	case 4:
		sum_cell(c, st, p.state); break;                 // ins::DIST_OUTPUT[2], -a1 on y[-1]
	default:
		// ins::DIST_OUTPUT[0], b0: the filter's output into the cell its own -a1 reads, at a half
		run(c, {.st = st, .sr = p.in, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dm = p.state});
		break;
	}
}

// The two output mixes, ins::DIST_WET, three steps between the two channels' halves. The left one
// takes the wet share and its dry share in the same step and closes into r[17]; the right one's dry
// share is the running value a step before it multiplies its own wet share by, so the pair sums the
// two channels' dry signals into both outputs with one step of the pipeline in common.
struct MixStep { int st; m2_t m2; int src; int cell; add_t add; route_t route; int dr; };

constexpr MixStep MIXES[3] = {
    //  the step  what the word scales  its register  the adder's cell  the adder  route  out
    {   0x1e3,  M2_REG,                 EQ,      DRY_L,     ADD_CELL, ROUTE_4, OUT_L  },
    {   0x1e4,  M2_REG,                 EQ,      DRY_R,     ADD_CELL, ROUTE_1, 0      },
    {   0x1e5,  M2_CELL,                0, STATE_R,     ADD_ACC,  ROUTE_4, OUT_R  },
};

void mix_step(yss236_core& c, MixStep const& p)
{
	run(c, {.st = p.st, .sr = p.src, .sm = p.cell, .m2 = p.m2, .add = p.add, .route = p.route, .clamp = p.dr ? CLAMP_SAT : CLAMP_WRAP, .dr = p.dr});
}

// ---------------------------------------------------------------------------------------------
// 0x1ed to 0x1ff: the gain curve and the two dry signals
// ---------------------------------------------------------------------------------------------

// One segment of the gain curve. The eight breakpoints are eight class-1 steps off the thresholded
// level, each its own ROM word added to the level and held at zero, so r[123] is measured against a
// staircase of thresholds: a quarter, a half, one, two, four, eight, sixteen and sixty-four. The
// weights are ins::DYN_RATIO, one per segment between two breakpoints, and the eight are summed into
// m[42], which is the curve the smoother reads. `bp` is the register a breakpoint lands in and `w`
// the one its own segment's weight reads, so the table holds both halves of one segment: the two
// channels' breakpoints are the smoothed level's own registers, reused after the output stage has
// read them.
struct Segment { int bp, w; route_t route; };

constexpr Segment CURVE[8] = {
    //   bp   w  route      the breakpoint lands in r[bp] and its own weight reads r[w]
    { CHAIN,     CHAIN,     ROUTE_2 },      // ins::DYN_RATIO[0]
    { CHAIN + 1, CHAIN + 2, ROUTE_2 },      // ins::DYN_RATIO[2]
    { CHAIN + 2, CHAIN + 3, ROUTE_2 },      // ins::DYN_RATIO[3]
    { CHAIN + 3, CHAIN + 1, ROUTE_2 },      // ins::DYN_RATIO[1]
    { CHAIN + 4, CHAIN + 4, ROUTE_2 },      // ins::DYN_RATIO[4]
    { CHAIN + 5, CHAIN + 5, ROUTE_4 },      // ins::DYN_RATIO[5]
    { LEV_L,     LEV_L,     ROUTE_1 },      // ins::DYN_RATIO[6]
    { LEV_R,     LEV_R,     ROUTE_1 },      // ins::DYN_RATIO[7]
};

void gain_curve(yss236_core& c, int st)
{
	if (st < 0x1f5)
	{
		// one of the eight breakpoints: r[n] = max(0, 128 * k + r[123]), the level less that step
		load(c, {.st = st, .sr = THRESH, .add = ADD_REG, .clamp = CLAMP_ZERO, .dr = CURVE[st - 0x1ed].bp});
		return;
	}
	if (st < 0x1fd)
	{
		// and the weight of one segment, the first on its own and the rest added to the running value.
		// The routes differ step by step, which is the ratio law's own shape and not a table's.
		Segment const& g = CURVE[st - 0x1f5];
		run(c, {.st = st, .sr = g.w, .add = st == 0x1f5 ? ADD_OFF : ADD_ACC, .route = g.route});
		return;
	}
	switch (st)
	{
	case 0x1fd:
		// m[42] = max(0, 128 * k + acc): the eight weights summed, biased by the ROM's own word and
		// held at zero, which is the curve the smoother and so the shaper will read next pass
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_ZERO, .dm = CURVE_OUT});
		break;
	case 0x1fe:
		// ins::DIST_WET[0]: m[47] = k * r[7], the left channel's dry signal
		run(c, {.st = st, .sr = SIG_L, .clamp = CLAMP_SAT, .dm = DRY_L});
		break;
	default:
		// ins::DIST_WET[2]: m[48] = k * r[8], the right channel's
		run(c, {.st = st, .sr = SIG_R, .clamp = CLAMP_SAT, .dm = DRY_R});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. There is no gate in this program and none of its steps is a control
// step, so no cskip of this block's own either: the guard is the only one a skip can open here.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// the four cells this window takes from the hardware side, before the step's own arithmetic
		if (Decor const* d = decor_for(st)) decor(c, st, *d);

		// ---- 0x1a0 to 0x1af: the drive chain, the two channels' inputs and their filters ----
		if (st < 0x1b0)
		{
			if (Biquad const* b = biquad_for(st)) { biquad_step(c, st, *b, st - b->st); return; }
			inputs(c, st);
			return;
		}

		// ---- 0x1b0 to 0x1bb: the rest of the chain, the level and the smoother ----
		if (st < 0x1bc) { shaper_chain(c, st); return; }

		// ---- 0x1bc to 0x1c1: the detector's two states and the release between them ----
		if (st < 0x1c2) { detector(c, st); return; }

		// ---- 0x1c2 to 0x1cb: the polynomial, the two smoothed levels and the drive's last word ----
		if (st < 0x1cc)
		{
			if (st == 0x1c2) { shape_word(c, st, CHAIN + 4, CHAIN); return; }   // ins::DIST_BAND[5]
			if (st < 0x1c9) { polynomial(c, st, st - 0x1c3); return; }
			if (st < 0x1cb) { t_gain(c, st, *t_gain_for(st)); return; }
			// ins::DIST_DRIVE[3]: the polynomial at the drive's own gain, into the tone filters' input
			gain(c, st, CHAIN, SHAPED, ROUTE_1);
			return;
		}

		// ---- 0x1cc to 0x1db: the threshold and the three tone filters ----
		if (st < 0x1dc)
		{
			if (Biquad const* b = biquad_for(st)) { biquad_step(c, st, *b, st - b->st); return; }
			// ins::DYN_THRESHOLD: the second state past the threshold, which the curve measures against
			gain(c, st, PEAK2, THRESH, ROUTE_1);
			return;
		}

		// ---- 0x1dc to 0x1ec: the two output scales, the two channels' halves and the two mixes ----
		if (st < 0x1ed)
		{
			if (TGain const* g = t_gain_for(st)) { t_gain(c, st, *g); return; }
			if (ChannelOut const* p = channel_for(st)) { channel_step(c, st, *p, st - p->st); return; }
			mix_step(c, MIXES[st - 0x1e3]);
			return;
		}

		// ---- 0x1ed to 0x1ff: the eight breakpoints, the eight weights and the two dry signals ----
		gain_curve(c, st);
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m05(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
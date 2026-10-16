// effects/ins_m01.cpp - VOP3-2, the insertion window's program for type msb 01: its 96 steps.
//
// rom::INSERTION_PROGRAMS[1], the program the insertion window holds under the five types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 1: ins-09 Rotary SP, ins-10 2-Way Rotary,
// ins-11 Tremolo, ins-12 Auto Pan and ins-13 Ambience, programmed by EffectHost::rotarySpeaker(),
// twoWayRotary(), tremolo(), autoPan() and ambience(). Like ins-m00 it is one of the fifteen
// programs the window shares: nothing here knows which type is running, because the program words
// are fixed and only the constants and the delay offsets move, which fx_modulation.cpp and
// fx_delay.cpp write whenever a parameter does. The code reads them where they live on the pass, as
// step() reads them, because that is the contract and not because the numbers are known here.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset
// through c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is
// folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in the order yss236_core::step() runs its own.
//
// What the 96 steps compute, in the groups the comments open each of. The host names every group it
// programs, in EffectHost::rotarySpeaker(), twoWayRotary(), tremolo(), autoPan() and ambience(), and in
// the tables they call:
//   0x1a0-0x1b3  the window's LFO cell through a rectifier into four accumulators, r[121], r[124],
//                r[122] and r[123], each of which names a delay length in samples: a register's value
//                times 256 is the delay, its integer part is the index the taps read at (queued at
//                0x1b4, 0x1bc, 0x1c4 and 0x1cc, in effect three steps later) and its fraction the
//                multiplier that interpolates between a line's two taps. Four envelopes, r[1], r[4],
//                r[3] and r[2], smooth each accumulator, and the two rectifiers at 0x1a8 and 0x1b1 feed
//                two of them back. ins::BAL_STEP_A, BAL_STEP_B, BAL_STEP_C, ins::MODE_STEPS_A and
//                MODE_STEPS_B, the tremolo's phase, the auto pan's direction and the 2-way rotary's mic
//                angle, are steps of this block. ins::LFO_DIR_STEPS, ins::PC_FB and ins::PC_MIX, which
//                name 0x1a0, 0x1a8, 0x1b0, 0x1b7, 0x1a7, 0x1aa, 0x1af, 0x1b1 and 0x1ba, are the
//                ensemble detune's and the pitch change's, so no handler that loads this program writes
//                them and under these five types they hold the words the preset left.
//   0x1b4-0x1ce  the four delay lengths. ins::ROT_LEVEL[1], [7], [5] and [3] are the depth words that
//                scale the accumulator into samples and queue its index; ins::ROT_LEVEL[0], [4], [2] and
//                [6] are the negative ones, which reach t[9], t[13], t[11] and t[15] as multipliers.
//                Eight reads of the delay RAM happen on the way past, the tap pairs at one word apart
//                and at the same index, one pair per delay length.
//   0x1ca-0x1ee  the four interpolations, each two cells taken five steps after the read that filled
//                them: m[56] and m[57] to r[119] on t[10], m[60] and m[61] to r[121] on t[14], m[62] and
//                m[63] to r[122] on t[3], m[58] and m[59] to r[120] on t[12]. The last two registers
//                reuse the accumulators' own, which have finished with them by then.
//   0x1cf-0x1e8  the two channels' tone shaping, three bands each: r[67] to r[68] through
//                ins::LPF_ROT, then to r[70] through ins::EQ_ROT, then to r[72] through ins::HPF_ROT,
//                and ins::LPF_ROT_C, ins::EQ_ROT_C and ins::HPF_ROT_C for r[73], r[74], r[76] and r[78].
//                Each shelf's own state a step back, in r[69], r[71], r[75] and r[77, is what the a-terms
//                ride on.
//   0x1ef-0x1f8  the four amplitude gains, and then the two wet sums: ins::ROT_PAN's four words weigh
//                the taps, which is the 2-way rotary's low/high balance and the ambience's phase.
//   0x1f9-0x1ff  the dry mix, ins::TREM_WORDS and ins::ROT_WORDS, the two outputs r[17] and r[18],
//                which the base program reads back, and the two delay lines' writes: the slot 0x7e takes
//                the tremolo mode's mix at 0x1fb and the slot 0x7f its own value at 0x1ff.
//
// The cells the program fills are its two serial inputs and its four LFOs, m[3] and m[7] from the banks
// at page 13 and m[4] from page 14, and m[13], m[14] and m[15] from the LFOs of pages 13, 14 and 15,
// which no step of this program reads; and the eight reads it takes up for itself, m[56] to m[63] from
// the eight swept taps, each five steps after the read that fills it, which is captured()'s window. The
// LFO cell this program sweeps, m[12], it only reads: the step that fills it is the variation window's
// at 0x18f, which takes lfo[12], this window's first LFO, out of the same hardware.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67], r[68], r[70]
// and r[72] the left channel's three bands and r[73], r[74], r[76] and r[78] the right's, with r[69],
// r[71], r[75] and r[77] their a-term copies a step back; r[1], r[4], r[3] and r[2] the four envelopes;
// r[121], r[124], r[122] and r[123] the four accumulators, each of which names one delay length and one
// tap pair; by 0x1ec and 0x1ee r[121] and r[122] have been reused for the third and the second tap, with
// r[119] holding the first and r[120] the fourth; r[17] and r[18] the two outputs. The slots: 0x6d and 0x6e, 0x6f and 0x70, 0x71 and
// 0x72 and 0x73 and 0x74 the four tap pairs, of which ins::TREM_SLOTS holds the first as its second and
// third, the third as its fourth and fifth and the fourth as its seventh and eighth, and the second
// pair not at all; and 0x7e and 0x7f the two lines' own writes, its zeroth and sixth. Of the eight
// offsets ins::TREM_SLOTS names, these five types are the only ones that move any: ambience()'s
// DELAY_TIME and tremolo()'s MODE, every other handler here writing coefficients and LFO words only.
//
// The old model in vop3_effects.h has a case for each of these five types, and each is a handful of
// lines over an FxLfo, a delay line and a pair of gains: no filter of three bands, no four taps and no
// accumulator. It is no reference for any number here, and where the two differ, the program above is
// what runs.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time. Inside the groups the loop opens, each repeated shape is a table
// and a small function that runs it: the four accumulators' sweep words, depth words, envelopes and
// amplitude clamps, the two channels' three bands each, the four interpolated taps, the two wet sums and
// the two outputs.
//
// vop3_native_check ins-m01: identical, 5 types, 48000 native passes.

#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: load, run, term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step moves into a cell, or reads out of the delay RAM, before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step of this program usually also takes a serial input, an LFO, a capture of an earlier delay RAM
// read, or reads the delay RAM itself. DEC_SERIAL_L and DEC_SERIAL_R take the step's own page's left and
// right serial input into `cell`, DEC_LFO takes that page's LFO, DEC_CAPTURE takes the read made CAPMIN
// to 11 steps earlier into `cell`, and the two reads push the delay RAM's word at the index a delay
// length queued into the capture history, a capture further down the program taking it up.
enum decor_t { DEC_SERIAL_L, DEC_SERIAL_R, DEC_LFO, DEC_CAPTURE, DEC_READ, DEC_READ_OLD };

// One decor, and the step it rides on. 0x1c7 is the only step with two, and DECORS below keeps them in
// the order step() moves them: the read first, then the page's LFO.
struct Decor { int st; decor_t kind; int cell; };

// The window publishes its two serial inputs for the windows outside it (m[3] and m[7]), the left serial
// input of page 14 for itself (m[4]), the LFOs of the three pages it has steps on (m[13], m[14] and
// m[15], which no step of this program reads: the variation window's step at 0x18f has already published
// page 12's into m[12], which this program only reads), and the eight swept taps m[56] to m[63. Each tap
// is five steps after the read that fills its cell, which is captured()'s lower bound, and the read's
// slot is its own step over four, so a decor never names a slot.
constexpr Decor DECORS[22] = {
    //  the step  the decor
    {  0x1a1, DEC_SERIAL_L,  3 },    // the left serial input of page 13, for the windows outside this one
    {  0x1a2, DEC_SERIAL_R,  7 },    // and the right one
    {  0x1a7, DEC_LFO,      13 },    // this window's second LFO, the page's own
    {  0x1b7, DEC_READ,      0 },    // the slot 0x6d at the first index, which m[56] takes up at 0x1bc
    {  0x1bb, DEC_READ_OLD,  0 },    // the slot 0x6e one word further on, into m[57] at 0x1c0
    {  0x1bc, DEC_CAPTURE,  56 },
    {  0x1bf, DEC_READ,      0 },    // the slot 0x6f at the second index, into m[62] at 0x1c4
    {  0x1c0, DEC_CAPTURE,  57 },
    {  0x1c3, DEC_READ_OLD,  0 },    // the slot 0x70 one word further on, into m[63] at 0x1c8
    {  0x1c4, DEC_CAPTURE,  62 },
    {  0x1c7, DEC_READ,      0 },    // the slot 0x71 at the third index, into m[60] at 0x1cc
    {  0x1c7, DEC_LFO,      14 },    // and this window's third LFO
    {  0x1c8, DEC_CAPTURE,  63 },
    {  0x1cb, DEC_READ_OLD,  0 },    // the slot 0x72 one word further on, into m[61] at 0x1d0
    {  0x1cc, DEC_CAPTURE,  60 },
    {  0x1cf, DEC_READ,      0 },    // the slot 0x73 at the fourth index, into m[58] at 0x1d4
    {  0x1d0, DEC_CAPTURE,  61 },
    {  0x1d1, DEC_SERIAL_L,  4 },    // the left serial input of page 14, which is this window's own
    {  0x1d3, DEC_READ_OLD,  0 },    // the slot 0x74 one word further on, into m[59] at 0x1d8
    {  0x1d4, DEC_CAPTURE,  58 },
    {  0x1d8, DEC_CAPTURE,  59 },
    {  0x1e7, DEC_LFO,      15 },    // and the fourth
};

// The first decor of each step, -1 where a step carries none. 0x1c7 carries two, in the order DECORS
// writes them, so an index of one row per step cannot name both: this lands on the first of a step's run
// and publish walks the run from there, which is the walk over all twenty-two rows this replaces.
constexpr auto DECOR_AT = []() {
	std::array<int16_t, yss236_core::STEPS> ix{};
	for (int16_t& i : ix) i = -1;
	// backwards, so the first row of a step's run wins over the rows after it
	for (int k = int(sizeof DECORS / sizeof DECORS[0]) - 1; k >= 0; k--)
		ix[DECORS[k].st] = int16_t(k);
	return ix;
}();

// A delay RAM read at the index a delay length queued rather than at the ring pointer, which is what
// makes a tap pair two words a sample apart: the earlier word at the index itself, the later one word
// older, and the t register the pair interpolates on the fraction between them. steps.h has the ring
// pointer read in read_delay(); these two are that read with the index added to the offset.
void read_delay_at(yss236_core& c, int st, bool older)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, (older ? 1 : 0) + c.cur_idx(), false)]);
}

// The decor a step carries. It comes before the step's own arithmetic, because step() moves a cell or
// reads the delay RAM before it does any arithmetic.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_SERIAL_L: take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R: take_serial(c, d.cell, st, 1); break;
		case DEC_LFO:      take_lfo(c, d.cell, st); break;
		case DEC_CAPTURE:  take_capture(c, d.cell, st); break;
		case DEC_READ:     read_delay_at(c, st, false); break;
		case DEC_READ_OLD: read_delay_at(c, st, true); break;
	}
}

// The cell moves and the delay RAM reads of one step, if it carries any. The loop runs this for every
// step before the group's own step, because step() does all of them first. DECORS is in step order, so a
// step's rows are the run that starts at DECOR_AT[st] and the walk stops on the first row of the next
// step; the bound is what keeps it off the end of the table.
void publish(yss236_core& c, int st)
{
	int const n = int(sizeof DECORS / sizeof DECORS[0]);
	for (int i = DECOR_AT[st]; i >= 0 && i < n && DECORS[i].st == st; i++)
		decor(c, st, DECORS[i]);
}

// ---------------------------------------------------------------------------------------------
// 0x1a0 to 0x1ce: the LFO cell, the four accumulators, and their depth words and envelopes
// ---------------------------------------------------------------------------------------------

// The three steps that read this window's LFO cell m[12]: its own rectifier, which is a class-3 step and
// so multiplies nothing, and the two depth words that scale the cell by the x16 route and leave it in the
// running value for the sweep word two steps later to add to. The variation window's step at 0x18f is
// the one that fills m[12], out of the same hardware as this window's first LFO.
struct Lfo { int st; bool unity; step::route_t route; step::clamp_t clamp; };

constexpr Lfo LFOS[3] = {
    //  the step  unity  route    clamp
    {  0x1a0, true,  ROUTE_2,  CLAMP_ABS },    // |2 * m[12]|, the step ins::LFO_DIR_STEPS[0] names
    {  0x1a2, false, ROUTE_16, CLAMP_WRAP },   // 16 k m[12]: the offset the next accumulator takes
    {  0x1a5, false, ROUTE_16, CLAMP_WRAP },   // and again, the third accumulator's
};

// The LFO word of step st, if it is one. The index is the same on every pass, so the step reaches its row
// rather than looking through the three for it.
constexpr auto LFOS_AT = index_by_step(LFOS);

void lfo_step(yss236_core& c, int st, Lfo const& x)
{
	run(c, {.st = st, .sm = 12, .m2 = M2_CELL, .unity = x.unity, .route = x.route, .clamp = x.clamp});
}

// The three rectifiers that feed an accumulator back: the second accumulator at 0x1a8, the fourth at
// 0x1b1 and the third at 0x1ba, each a class-3 step, so the register is the product whole and what the
// running value takes is its absolute value.
struct Rect { int st, reg; };

constexpr Rect RECTS[3] = {
    {  0x1a8, 124 },     // ins::LFO_DIR_STEPS[1], and the rectifier that feeds the second accumulator back
    {  0x1b1, 123 },     // ins::PC_FB[5], on the fourth
    {  0x1ba, 122 },     // ins::PC_MIX[0], on the third
};

// The rectifier of step st, if it is one.
constexpr auto RECTS_AT = index_by_step(RECTS);

void rectifier(yss236_core& c, int st, Rect const& r)
{
	run(c, {.st = st, .sr = r.reg, .unity = true, .clamp = CLAMP_ABS});
}

// A sweep accumulator's word. This is a class-1 step, the one shape in this program that loads a value
// rather than applying a coefficient: the word arrives as 128 * k, the running value is added to it, and
// the route and the clamp of the row put the result in the accumulator's register. `cell` is the cell
// the adder reads instead of the running value, which only the fourth accumulator's step does, off the
// LFO cell itself.
struct Sweep { int st, reg, cell; add_t add; step::route_t route; step::clamp_t clamp; };

// The seven sweep words of the head. Four saturate and three wrap: the two that load an accumulator for
// the first time wrap, and so does 0x1ab, which is the fourth accumulator's first load and the only one
// that reads the LFO cell instead of the running value.
constexpr Sweep SWEEPS[7] = {
    //  the step  reg  cell  the adder    route    clamp
    {  0x1a1, 121,   0,  ADD_ACC,  ROUTE_2,  CLAMP_SAT },     // the first accumulator
    {  0x1a3, 124,   0,  ADD_ACC,  ROUTE_2,  CLAMP_WRAP },    // the second, wrapped rather than saturated
    {  0x1a6, 122,   0,  ADD_ACC,  ROUTE_2,  CLAMP_WRAP },    // ins::BAL_STEP_A, the third
    {  0x1a9, 124,   0,  ADD_ACC,  ROUTE_2,  CLAMP_SAT },     // and the second again, saturated
    {  0x1ab, 123,  12,  ADD_CELL, ROUTE_2,  CLAMP_WRAP },    // ins::BAL_STEP_B and [C], the fourth, off m[12]
    {  0x1b2, 123,   0,  ADD_ACC,  ROUTE_2,  CLAMP_SAT },     // and the fourth again, saturated
    {  0x1bb, 122,   0,  ADD_ACC,  ROUTE_2,  CLAMP_SAT },     // the third again, with the read of 0x6e
};

// The sweep word of step st, if it is one.
constexpr auto SWEEPS_AT = index_by_step(SWEEPS);

void sweep(yss236_core& c, int st, Sweep const& s)
{
	load(c, {.st = st, .sm = s.cell, .add = s.add, .route = s.route, .clamp = s.clamp, .dr = s.reg});
}

// One of the four envelopes, in two steps: the accumulator's own value on the t register the feedback
// word wrote, into the envelope's register, and then the envelope on itself. r[1] smooths r[121] on
// t[9], r[4] r[124] on t[15], r[3] r[123] on t[13] and r[2] r[122] on t[11]; all eight saturate.
struct Envelope { int st, reg, src, treg; };

constexpr Envelope ENVELOPES[8] = {
    //  the step  the envelope  the accumulator  its weight
    {  0x1a7,     1,         121,           9 },
    {  0x1aa,     1,           1,           9 },
    {  0x1b0,     4,         124,          15 },
    {  0x1b3,     4,           4,          15 },
    {  0x1b9,     3,         123,          13 },
    {  0x1bd,     3,           3,          13 },
    {  0x1c2,     2,         122,          11 },
    {  0x1c5,     2,           2,          11 },
};

// The envelope word of step st, if it is one.
constexpr auto ENVELOPES_AT = index_by_step(ENVELOPES);

void envelope(yss236_core& c, int st, Envelope const& e)
{
	run(c, {.st = st, .sr = e.src, .treg = e.treg, .clamp = CLAMP_SAT, .dr = e.reg});
}

// One accumulator's amplitude envelope, the three steps that turn a sweep into an amplitude: the
// accumulator's own value at sixteen times its depth, the envelope's word added on top of it, and the
// sweep word that closes the sum into the accumulator with the negatives held at zero. The four
// accumulators take these three steps in step order, each on its own envelope: r[1] on r[121], r[4] on
// r[124], r[3] on r[123] and r[2] on r[122], and ins::PC_FB[4], ins::MODE_STEPS_B[0] and
// ins::MODE_STEPS_A[0] are three of the nine words these steps carry.
struct AmpEnv { int st, nsteps, acc, env; };

constexpr AmpEnv AMP_ENV[4] = {
    //    st  n  acc  env
    {  0x1ad, 3, 121,  1 },
    {  0x1b6, 3, 124,  4 },
    {  0x1bf, 3, 123,  3 },
    {  0x1c7, 3, 122,  2 },
};

// The amplitude envelope of step st, if it is one: an envelope's three steps are consecutive, so each of
// them indexes to the same row with a different k.
constexpr auto AMP_ENV_AT = index_by_span(AMP_ENV);

void amp_env(yss236_core& c, int st, AmpEnv const& a)
{
	switch (st - a.st)
	{
	case 0:
		// the accumulator's own value on the x16 route, saturating: what the envelope is built on
		run(c, {.st = st, .sr = a.acc, .route = ROUTE_16, .clamp = CLAMP_SAT});
		break;
	case 1:
		// the envelope's word on top of it, added to the running value
		sum(c, st, a.env);
		break;
	default:
		// the sweep word with the negatives held at zero, which is what makes the accumulator an
		// amplitude: the offset it carries is ins::PC_FB[4] at 0x1af, ins::MOD_LEVEL[0] at 0x1b8 and the
		// two of the third and fourth accumulators at 0x1c1 and 0x1c9
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_ZERO, .dr = a.acc});
		break;
	}
}

// What a depth word leaves in the t file. D_FEEDBACK is the accumulator's own value doubled and back into
// its own register, which the envelope two steps later scales. D_POSITION is the accumulator scaled into
// the delay length in samples: its fraction below one sample goes into the t register the pair
// interpolates on, and its whole samples into the delay index the pair's two reads take three steps
// later. D_AMPLITUDE is the same accumulator on the x1 route, and its word is the tap gain's weight.
enum depth_t { D_FEEDBACK, D_POSITION, D_AMPLITUDE };

// The twelve depth words, in step order. Each accumulator takes one of each, on the steps below.
struct Depth { int st, reg, treg; depth_t kind; };

constexpr Depth DEPTHS[12] = {
    //  the step  reg  treg  the shape
    {  0x1a4, 121,   9, D_FEEDBACK },     // the first accumulator's feedback
    {  0x1ac, 124,  15, D_FEEDBACK },     // the second's
    {  0x1b4, 121,  10, D_POSITION },     // ins::ROT_LEVEL[1] and ins::MOD_LEVEL[0]: the first delay length
    {  0x1b5, 123,  13, D_FEEDBACK },     // the fourth's
    {  0x1bc, 124,   3, D_POSITION },     // ins::ROT_LEVEL[7] and ins::MOD_LEVEL[5]: the second length
    {  0x1be, 122,  11, D_FEEDBACK },     // the third's
    {  0x1c3, 121,   9, D_AMPLITUDE },    // ins::ROT_LEVEL[0], and the read of the slot 0x70 rides on it
    {  0x1c4, 123,  14, D_POSITION },     // ins::ROT_LEVEL[5]: the third length
    {  0x1c6, 123,  13, D_AMPLITUDE },    // ins::ROT_LEVEL[4]
    {  0x1cc, 122,  12, D_POSITION },     // ins::ROT_LEVEL[3]: the fourth length
    {  0x1cd, 122,  11, D_AMPLITUDE },    // ins::ROT_LEVEL[2]
    {  0x1ce, 124,  15, D_AMPLITUDE },    // ins::ROT_LEVEL[6]
};

// The depth word of step st, if it is one.
constexpr auto DEPTHS_AT = index_by_step(DEPTHS);

void depth(yss236_core& c, int st, Depth const& d)
{
	double const y = d.kind == D_FEEDBACK
		? run(c, {.st = st, .sr = d.reg, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = d.reg})
		: run(c, {.st = st, .sr = d.reg, .clamp = CLAMP_SAT});
	if (d.kind == D_POSITION)
	{
		t_put_fraction(c, st, d.treg, y);   // the fraction below one sample, which the tap pair interpolates on
		load_index(c, y);               // and the whole of it, which the pair's two reads take three steps on
		return;
	}
	t_put(c, st, d.treg, y);               // the other two leave a 1.15 multiplier: the envelope's or the gain's
}

// One step of the head, 0x1a0 to 0x1ce. The LFO cell, the four accumulators, their depth words and their
// envelopes interleave, so each step is looked up in the six indexes above and the first that names it
// runs it, which is every step of the head and nothing else. No two of the six own the same step, which
// is what lets the six tests stand in that order without a step reaching a table further down.
void accumulator(yss236_core& c, int st)
{
	if (int i = LFOS_AT[st];      i >= 0) { lfo_step(c, st, LFOS[i]); return; }
	if (int i = RECTS_AT[st];     i >= 0) { rectifier(c, st, RECTS[i]); return; }
	if (int i = SWEEPS_AT[st];    i >= 0) { sweep(c, st, SWEEPS[i]); return; }
	if (int i = ENVELOPES_AT[st]; i >= 0) { envelope(c, st, ENVELOPES[i]); return; }
	if (int i = AMP_ENV_AT[st];   i >= 0) { amp_env(c, st, AMP_ENV[i]); return; }
	if (int i = DEPTHS_AT[st];    i >= 0) { depth(c, st, DEPTHS[i]); return; }
}

// ---------------------------------------------------------------------------------------------
// The four interpolated taps, two steps each
// ---------------------------------------------------------------------------------------------

// One tap pair: the two words a sample apart at the index a delay length queued, with the position's
// fraction taken off the earlier word and applied to the later one, so the register the pair's second step
// writes holds the delay line interpolated at that position. `treg` is the t register holding the
// fraction, `early` the cell of the word at the index and `late` the cell of the word one older. The two
// cells are taken up five steps after the read that filled them, which is captured()'s lower bound.
struct Interp { int base; int treg, early, late, out; };

constexpr Interp INTERPOLATIONS[4] = {
    {  0x1ca, 10, 56, 57, 119 },     // the slots 0x6d and 0x6e: the left channel's first wet tap
    {  0x1e9, 12, 58, 59, 120 },     // the slots 0x73 and 0x74: the right channel's first wet tap
    {  0x1eb, 14, 60, 61, 121 },     // the slots 0x71 and 0x72, into the register the first accumulator held
    {  0x1ed,  3, 62, 63, 122 },     // the slots 0x6f and 0x70, into the one the third held
};

void interp(yss236_core& c, int st, Interp const& p)
{
	if (st == p.base)
	{
		// the pair's weight: the fraction times the earlier word, with the word itself taken off, which
		// is the pair interpolated at the index less one word
		run(c, {.st = st, .sm = p.early, .m2 = M2_CELL, .treg = p.treg, .add = ADD_CELL, .sub = true});
		return;
	}
	// the later word on that weight less the running value, into the register the pan words scale. The
	// read of the pair's second slot rides on this step, and ins::EQ3_LEVEL[0] and ins::MOD_WORDS[4] are
	// the two of the four closing words a table names; the other two are the ROM's own.
	run(c, {.st = st, .sm = p.late, .m2 = M2_CELL, .treg = p.treg, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = p.out});
}

// ---------------------------------------------------------------------------------------------
// 0x1cf to 0x1e8: the two channels, three bands each
// ---------------------------------------------------------------------------------------------

// Which of a band's steps carries the z^-1 copy that shifts its delay register along. The mid peak's b2
// step keeps the band's input a pass back in x1, which is what it reads and what it writes (w1[7]). The
// high shelf's -a1 step puts its own input a pass back in the same register the peak's -a2 read, so one
// copy serves both. The low shelf carries none: the register its -a1 reads is the one its own b0
// overwrites, which is the output delay a pass back and needs no copy.
enum copy_t { NO_COPY, COPY_ON_B2, COPY_ON_A1 };

// One band of a channel's three. `in` is the register the previous band's output is in, which b1 and b0
// read, `out` the register b0 writes and the -a1 step reads a pass back, `x1` the copy of the input a
// pass back and `y2` the peak's second output delay. The b0 step's route is what restores the scale the
// designer's own (16 >> scale) took out of the host's words.
struct Band { int st, nsteps; copy_t copy; int in, x1, y2, out; step::route_t route; };

void band(yss236_core& c, int st, Band const& b)
{
	int const k = st - b.st;
	if (k == 0)
	{
		term(c, st, b.in);                       // the band's b1 word on its own input, opening the sum
		return;
	}
	if (b.copy == COPY_ON_B2)
	{
		// the mid peak: b2 on the input a pass back, which the same step keeps there, then -a1 on the
		// band's output a pass back and -a2 on the one two passes back, and b0 on the input again
		switch (k)
		{
		case 1: copy_z(c, st, b.x1, b.x1, ADD_ACC, CLAMP_WRAP); break;
		case 2: sum(c, st, b.out); break;
		case 3: sum(c, st, b.y2); break;
		default: close(c, st, b.in, b.out, b.route); break;
		}
		return;
	}
	// the two shelves: -a1 on the band's own output a pass back, and b0 on its input, closing the sum
	switch (k)
	{
	case 1:
		if (b.copy == COPY_ON_A1) copy_z(c, st, b.out, b.x1, ADD_ACC, CLAMP_WRAP);
		else                      sum(c, st, b.out);
		break;
	default: close(c, st, b.in, b.out, b.route); break;
	}
}

// One channel's thirteen steps. The first two are the two signals the base program leaves this window,
// r[7] and r[8]: the channel's own share opens the running value and the other one's word is added to it
// and the sum doubled into `mixed`, which is the first band's input. The three bands then run in cascade,
// the last of which leaves the signal the dry mix and the tremolo take.
struct Channel { int base; int in, other, mixed; Band band[3]; };

// The left channel first, whose steps also publish three of the eight delay reads the taps take up, then
// the right one, which publishes the fourth LFO. The bands sit in the channel's own row, so each channel
// has its own index of them and channel() is told which one to read.
constexpr Channel CHANNELS[2] = {
    //  base   in  other  mixed   the three bands
    {  0x1cf,   7,     8,     67,
        { { 0x1d1, 3, NO_COPY,    67,  0,  0, 68, ROUTE_2 },     // the low shelf, ins::LPF_ROT
          { 0x1d4, 5, COPY_ON_B2, 68, 69, 71, 70, ROUTE_2 },     // the mid peak, ins::EQ_ROT
          { 0x1d9, 3, COPY_ON_A1, 70, 71,  0, 72, ROUTE_4 } } }, // the high shelf, ins::HPF_ROT
    {  0x1dc,   8,     7,     73,
        { { 0x1de, 3, NO_COPY,    73,  0,  0, 74, ROUTE_2 },     // and ins::LPF_ROT_C
          { 0x1e1, 5, COPY_ON_B2, 74, 75, 77, 76, ROUTE_2 },     // ins::EQ_ROT_C
          { 0x1e6, 3, COPY_ON_A1, 76, 77,  0, 78, ROUTE_4 } } }, // and ins::HPF_ROT_C
};

// A band's steps are consecutive, so each of a band's steps indexes to the same row with a different k,
// and the three bands of a channel cover 0x1d1 to 0x1df and 0x1de to 0x1ec without meeting.
constexpr auto LEFT_BANDS_AT = index_by_span(CHANNELS[0].band);
constexpr auto RIGHT_BANDS_AT = index_by_span(CHANNELS[1].band);

void channel(yss236_core& c, int st, Channel const& h, std::array<int16_t, yss236_core::STEPS> const& band_at)
{
	int const k = st - h.base;
	if (k == 0)
	{
		term(c, st, h.in);       // the channel's own share of the two signals, the adder off
		return;
	}
	if (k == 1)
	{
		// the other signal's word on top of it, doubled: r[67] and r[73], the first band's input
		close(c, st, h.other, h.mixed, ROUTE_2);
		return;
	}
	if (int i = band_at[st]; i >= 0)
		band(c, st, h.band[i]);
}

// ---------------------------------------------------------------------------------------------
// 0x1ef to 0x1f8: the four tap gains and the two wet sums
// ---------------------------------------------------------------------------------------------

// One tap's amplitude gain: r[reg] = saturate(t[treg] * r[reg] + r[reg]), which is unity less the depth
// the word names. The adder reads the register the step has just latched rather than the running value,
// so the sum is the tap and its complement, and the register the gain writes is the one it reads.
struct Gain { int st, reg, treg; };

// The four gains in step order, one per tap: ins::MOD_WORDS[7] at 0x1ef, ins::PC_MIX[5] at 0x1f0,
// ins::MOD_WORDS[6] at 0x1f1, and the ROM's own word at 0x1f2.
constexpr Gain GAINS[4] = {
    {  0x1ef, 119,  9 },
    {  0x1f0, 120, 11 },
    {  0x1f1, 121, 13 },
    {  0x1f2, 122, 15 },
};

void tap_gain(yss236_core& c, int st, Gain const& g)
{
	run(c, {.st = st, .sr = g.reg, .treg = g.treg, .add = ADD_REG, .clamp = CLAMP_SAT, .dr = g.reg});
}

// One channel's wet sum: three taps, each on its own word, and the sum closes back into the channel's
// first tap. The two the later steps scale are the two the interpolation steps of the head left in the
// accumulators' own registers, and the channels take them the other way round: by 0x1ec and 0x1ee r[121]
// holds the third tap and r[122] the second, so the left sums r[119] + r[121] + r[122] and the right
// r[120] + r[122] + r[121]. ins::ROT_PAN's four words weigh the first and the third step of each sum and
// ins::MOD_WORDS[0] to [3] the same two; the middle step carries no table's word.
struct Wet { int base; int first, second, third; };

constexpr Wet WETS[2] = {
    {  0x1f3, 119, 121, 122 },     // the left channel, on ins::ROT_PAN[0] and [2]
    {  0x1f6, 120, 122, 121 },     // and the right, on ins::ROT_PAN[1] and [3]
};

void wet(yss236_core& c, int st, Wet const& w)
{
	switch (st - w.base)
	{
	case 0: term(c, st, w.first); break;             // the channel's first tap, on ins::ROT_PAN[0] or [1]
	case 1: sum(c, st, w.second); break;             // and the third tap, which no table word weighs
	default: close(c, st, w.third, w.first); break; // ins::ROT_PAN[2] or [3]: the three taps summed
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1f9 to 0x1ff: the dry mix, the two outputs and the two delay lines' writes
// ---------------------------------------------------------------------------------------------

// What a step does with its line's slot. SET_LATCH latches its result as the data of the next write,
// WRITE_LATCHED is that writer and spends the latch rather than its own result, and WRITE_OWN is a writer
// that carries w1[13] itself, so it drops any latched data and writes its own.
enum delay_t { NO_WRITE, SET_LATCH, WRITE_LATCHED, WRITE_OWN };

// The two fields run() takes for a step that hands its result to a delay slot.
void delay_write(step::alu& a, delay_t w)
{
	a.wdata = w == SET_LATCH || w == WRITE_OWN;
	a.write = w == WRITE_LATCHED || w == WRITE_OWN;
}

// One channel's output, four steps on the left and three on the right. The dry word scales the signal
// the channel's three bands left it, and the output is the wet sum added to that, four times and
// saturated, into the register the base program reads back. The two channels differ in two places: only
// the left one has the tremolo's pair, which mixes the other channel's signal in and latches the result
// for its line's write, and the two writers fall on opposite sides of their outputs, the left's a step
// before its own and the right's a step after.
struct Out
{
	int base;                     // the first step
	int latch, writer;            // the step that latches for the line, 0 where there is none, and the
	                              // step that writes the slot
	int tone, other;              // this channel's tone-shaped signal, and the other's for the tremolo
	int wet, reg, slot;           // the wet sum the output closes, the register it writes, the line's slot
	delay_t write;                // what the writer leaves for the slot
	step::clamp_t writer_clamp;   // and the clamp it takes
};

constexpr Out OUTS[2] = {
    //  base  latch  writer  tone  other  wet  reg  the slot  what the writer leaves  its clamp
    {  0x1f9,   1,      2,   72,    78, 119,  17,     0x7e,  WRITE_LATCHED,           CLAMP_WRAP },
    {  0x1fd,   0,      2,   78,     0, 120,  18,     0x7f,  WRITE_OWN,               CLAMP_SAT  },
};

void out_steps(yss236_core& c, int st, Out const& o)
{
	int const k = st - o.base;
	if (k == 0)
	{
		term(c, st, o.tone);       // the dry word on the channel's tone-shaped signal
		return;
	}
	if (k == o.latch)
	{
		// ins::TREM_WORDS[1]: the other channel's signal on its word added to this one, saturating, and
		// latched as the data of the write the next step makes
		run(c, {.st = st, .sr = o.other, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true});
		return;
	}
	if (k == o.writer)
	{
		// the line's write into its own slot, which on the left takes the latch above and on the right
		// drops it and writes the dry word's value instead
		step::alu a;
		a.st = st;
		a.sr = o.tone;
		a.clamp = o.writer_clamp;
		delay_write(a, o.write);
		run(c, a);
		return;
	}
	// ins::ROT_WORDS[2] or [3]: the wet sum on its word added to the dry mix, four times, into the
	// register the base program reads back
	close(c, st, o.wet, o.reg, ROUTE_4);
}

// ---------------------------------------------------------------------------------------------
// The run of steps 0x1a0 to 0x1ff, the whole of the window's program
// ---------------------------------------------------------------------------------------------

// The loop is one iteration per step and carries the m_skip guard step() opens with, so a gate in the base
// program's 0x1xx half skips these steps as it would the interpreter's. Each test below is one of the
// groups the comments at the head of this file open.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// the serial inputs, the LFOs, the captures and the delay RAM reads, before any arithmetic
		publish(c, st);

		if (st < 0x1b4)
		{
			// ---- 0x1a0 to 0x1b3, the LFO cell, four accumulators, their envelopes and their rectifiers
			accumulator(c, st);
			return;
		}
		if (st < 0x1ca)
		{
			// ---- 0x1b4 to 0x1c9, the four delay lengths and the eight delay RAM reads on the way past
			accumulator(c, st);
			return;
		}
		if (st < 0x1cc)
		{
			// ---- 0x1ca and 0x1cb, the first tap pair interpolated
			interp(c, st, INTERPOLATIONS[0]);
			return;
		}
		if (st < 0x1cf)
		{
			// ---- 0x1cc to 0x1ce, the fourth delay length and the two amplitude words off r[122] and r[124]
			accumulator(c, st);
			return;
		}
		if (st < 0x1dc)
		{
			// ---- 0x1cf to 0x1db, the left channel: the two signals summed, then the three bands
			channel(c, st, CHANNELS[0], LEFT_BANDS_AT);
			return;
		}
		if (st < 0x1e9)
		{
			// ---- 0x1dc to 0x1e8, the right channel, the same thirteen steps on the other registers
			channel(c, st, CHANNELS[1], RIGHT_BANDS_AT);
			return;
		}
		if (st < 0x1ef)
		{
			// ---- 0x1e9 to 0x1ee, the other three interpolations
			interp(c, st, INTERPOLATIONS[(st - 0x1e9) / 2 + 1]);
			return;
		}
		if (st < 0x1f3)
		{
			// ---- 0x1ef to 0x1f2, the four amplitude gains
			tap_gain(c, st, GAINS[st - 0x1ef]);
			return;
		}
		if (st < 0x1f9)
		{
			// ---- 0x1f3 to 0x1f8, the two wet sums, which the pan law words weight
			wet(c, st, WETS[(st - 0x1f3) / 3]);
			return;
		}
		// ---- 0x1f9 to 0x1ff, the dry mix, the two outputs and the two delay lines' writes
		out_steps(c, st, OUTS[st < 0x1fd ? 0 : 1]);
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m01(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
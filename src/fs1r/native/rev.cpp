// fs1r/native/rev.cpp - VOP3-2, the reverb: its 94 steps of the effect chip's base program.
//
// The reverb block owns no program of its own. Its steps are 94 of the base program's 512, in ten runs
// of the schedule, which rom::STEPS_REVERB lists and the base program leaves to the reverb window: the
// whole block is here, so the rest of the pass belongs to the `base` row. Under it sit the halls and
// rooms the Effect Type List calls types 1 to 12 (Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate,
// White Room, Tunnel, Basement, Canyon), which the firmware tells apart by the constants and the delay
// offsets it writes and not by the program, so one native covers all twelve. Types 13 to 16 of the
// reverb block run their programs in the reverb delay window instead, under the keys `dly-mNN`, so no
// step of theirs is here.
//
// What the 94 steps compute is named from the host that feeds them, in fx_reverb.cpp and the rev: tables
// of rom_tables.cpp, which between them say which constant belongs to which part of the reverb:
//
//   six comb units     rev::HALL_COMB_STEPS, four words each: the gain, two input taps and the feedback
//                      coefficient, the four words EffectHost::combUnit() writes per unit
//   six allpass units  rev::ROW_STEPS, four words each: s28, s24, s26 and s22, the four words
//                      EffectHost::diffusers() writes, with rev::ROW_SLOTS giving each unit's offset
//   the input filters  rev::HPF_STEPS and rev::LPF_STEPS, three words each, copied into three more
//                      steps each by rev::HPF_COPY and rev::LPF_COPY
//   22 early reflections rev::ER_WORDS: the first ten scaled by the ER/Rev balance in
//                      EffectHost::erBalance(), the last twelve set to the complement of it
//   the surround       rev::MIX_STEPS, rev::LEVEL_STEPS, rev::WIDTH_STEPS and the type's shelf words in
//                      rev::TABLE_STEPS
//
// The program runs the units in a pipeline three deep, which is what the schedule below says: three
// units' state steps, then the first unit's write into the delay line, then the next three state steps,
// then the next write, and so on. A comb unit's gain step writes its output into the line, and the unit
// three places along reads that slot back, so a unit's output is the next unit's input within one pass
// and the six comb filters are a six-deep loop rather than six separate delays.
//
// The cells carry the values between the units. A delay RAM read is made at one step and taken up by a
// cell 5 to 11 steps later, which is what yss236_core::captured() returns and what an msrc of 4 queues;
// m[34] to m[40] are a chain of the previous step's own operand (msrc 7), one cell per step, which gives
// each comb unit its second input tap a step after its first. The registers are r[47] to r[54] for the
// input filters, r[55] to r[60] for the six comb units' states, r[119] and r[120] for the two sums the
// surround reads, r[125] for an allpass state, and r[9] and r[10] for the level.
//
// Only the fields the program fixes are written out below. Everything the host rewrites is read where it
// lives, as step() reads it: every constant from c.coef[st] on the pass, every delay offset through
// c.addr(), which reads the c.slot[] entry the host last wrote. No number here is one the firmware
// chose, and the code runs on all twelve reverb types unchanged.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check rev: identical, 13 types, 9600 native passes each.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step of a unit carries besides its term
// ---------------------------------------------------------------------------------------------

// What a step does with the delay line at the end of it.
enum delay_t
{
	NO_WRITE,        // nothing
	WRITE_LATCHED,   // writes the data an earlier wdata step latched: the reverb's paired taps
	WRITE_OWN,       // writes its own result, and clears any latched data: a unit's own output
	SET_LATCH,       // latches its result as the data of the next write
};

// ---------------------------------------------------------------------------------------------
// The six allpass units, rev::ROW_STEPS
// ---------------------------------------------------------------------------------------------

// One allpass unit, four words and four steps in the order s26, s22, s28, s24. The first and third
// words are the unit's gain on its input, with the adder off, and the second and fourth are the two
// taps on its delay line added to the running value; the fourth closes the pair into the unit's output
// with the x2 route. `in` is the register the unit reads (the previous unit's output), `dly` the cell
// its taps come off, and `out_reg` or `out_cell` where s24 puts the result.
struct Allpass
{
	int st, nsteps;            // the unit's four steps, and how many that is (always four)
	int in, dly;
	int out_reg, out_cell;     // s24's result: a register, or a cell when out_reg is 0
	Decor decor[4];
	delay_t s22_write;         // what the s22 tap does with the delay line
};

// rev::ROW_STEPS[0] to [23] are the four words of the six units in this order, and rev::ROW_SLOTS the
// offsets. Three of the six units write their own tap into the line (NO_WRITE is never used: s22
// either latches its result for the next writer or writes the latched one).
constexpr Allpass ALLPASS[6] = {
    //  base   n  in   dly  out_reg out_cell  s26                s22                  s28                s24
    { 0x03e, 4, 54,  22, 125, 0,  { {},                {},                {DEC_CAPTURE, 56}, {}                 }, WRITE_OWN },
    { 0x04c, 4, 125, 59, 0,   62, { {DEC_CAPTURE, 58}, {},                {},                {DEC_READ, 0}      }, SET_LATCH },
    { 0x062, 4, 119, 56, 119, 0,  { {},                {},                {DEC_CAPTURE, 61}, {}                 }, WRITE_OWN },
    { 0x07f, 4, 119, 61, 119, 0,  { {DEC_READ, 0},     {DEC_CAPTURE, 61}, {},                {DEC_SERIAL_L, 40} }, SET_LATCH },
    { 0x08e, 4, 120, 56, 120, 0,  { {},                {},                {DEC_CAPTURE, 56}, {}                 }, WRITE_OWN },
    { 0x097, 4, 120, 57, 120, 0,  { {DEC_READ, 0},     {DEC_CAPTURE, 57}, {},                {}                 }, SET_LATCH },
};

// One allpass unit's four steps, s26, s22, s28 and s24, each at st + k.
void allpass_step(yss236_core& c, int st, Allpass const& u, int k)
{
	decor(c, st, u.decor[k]);
	step::alu a;
	a.st = st;
	a.m2 = M2_CELL;
	switch (k)
	{
	case 0:                                                              // s26: the gain on the input
	case 2:                                                              // s28: the gain again
		a.sr = u.in;
		a.m2 = M2_REG;
		run(c, a);
		break;
	default:                                                             // s22 and s24: the two taps
		a.sm = u.dly;
		a.add = ADD_ACC;
		a.clamp = CLAMP_SAT;
		if (k == 3)                                                      // s24 closes the pair, x2
		{
			a.route = ROUTE_2;
			a.dr = u.out_reg;
			a.dm = u.out_cell;
		}
		if (k == 1)
		{
			// The s22 tap either latches its own result for the writer a few steps along, or is that
			// writer: a writer carrying the flag clears any latched data and writes its own result.
			a.wdata = true;
			a.write = u.s22_write != SET_LATCH;
		}
		run(c, a);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The six comb units, rev::HALL_COMB_STEPS
// ---------------------------------------------------------------------------------------------

// One comb unit, four words and four steps: the gain, the two input taps and the feedback coefficient,
// in the order x1, x2, a and gain. The taps are cells rather than registers, m[in] and m[state], and x2
// hands the step before's own operand on to the next unit's m[state], which is how the six units chain
// without a register each. The feedback step adds the word on the unit's state register to the taps
// and closes the sum into that same register with the x2 route, and the gain step then adds the unit's
// own state again on top of the surround's m[62] and writes the result into the delay line.
struct Comb
{
	int x1, x2, a, gain;     // the unit's four steps
	int in, state, reg;      // its input cell, its state cell, its state register
	Decor x1_decor, a_decor;
};

// EffectHost::combUnit() writes these twenty-four words; the states are r[55] to r[60] and the states'
// cells m[34] to m[39], one each.
constexpr Comb COMB[6] = {
    //   x1     x2     a   gain   in  state  reg  x1 decor           a decor
    { 0x0a3, 0x0a4, 0x0a5, 0x0a9, 56, 34, 55, {DEC_READ, 0}, {DEC_CAPTURE, 56} },
    { 0x0a6, 0x0a7, 0x0a8, 0x0ad, 57, 35, 56, {DEC_LFO, 13}, {DEC_CAPTURE, 57} },
    { 0x0aa, 0x0ab, 0x0ac, 0x0b1, 58, 36, 57, {},            {}                },
    { 0x0ae, 0x0af, 0x0b0, 0x0b5, 56, 37, 58, {},            {}                },
    { 0x0b2, 0x0b3, 0x0b4, 0x0d1, 57, 38, 59, {},            {}                },
    { 0x0d2, 0x0d3, 0x0d4, 0x0d7, 56, 39, 60, {},            {}                },
};

// Which of a comb unit's four steps a step of the block is. The gain steps come several places behind
// their own unit's state steps, which is the pipeline the six units run in.
enum comb_role_t { COMB_X1, COMB_X2, COMB_A, COMB_GAIN };

struct CombStep { int unit; comb_role_t role; };

// The schedule of the block at 0x0a3: three units' state steps, then the first unit's gain, then the
// next three, then the next gain, and so on, so that by the time a unit's gain runs, the unit three
// along has already read the slot it wrote. Combs 4 and 5 finish in the block at 0x0d1.
constexpr CombStep COMB_SCHED[19] = {
    {0, COMB_X1}, {0, COMB_X2}, {0, COMB_A}, {1, COMB_X1}, {1, COMB_X2}, {1, COMB_A}, {0, COMB_GAIN},
    { 2, COMB_X1}, {2, COMB_X2}, {2, COMB_A}, {1, COMB_GAIN },
    { 3, COMB_X1}, {3, COMB_X2}, {3, COMB_A}, {2, COMB_GAIN },
    { 4, COMB_X1}, {4, COMB_X2}, {4, COMB_A}, {3, COMB_GAIN },
};

// One comb unit's step at the given role: the input tap, the state's tap, the feedback word or the gain
// word that writes the unit's output into its delay slot.
void comb_step(yss236_core& c, int st, Comb const& k, comb_role_t role)
{
	switch (role)
	{
	case COMB_X1:
		decor(c, st, k.x1_decor);
		run(c, {.st = st, .sm = k.in, .m2 = M2_CELL});                    // the input tap: the adder is off
		break;
	case COMB_X2:
		// the state's tap, and the cell that hands the step before's operand to the next unit
		run(c, {.st = st, .sm = k.state, .m2 = M2_CELL, .add = ADD_ACC, .dm = k.state, .prev = true});
		break;
	case COMB_A:
		decor(c, st, k.a_decor);
		run(c, {.st = st, .sr = k.reg, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = k.reg});
		break;
	default:
		// the gain: the surround's m[62] scaled, added to the unit's own state, written into its slot.
		// It carries the flag and is the writer, so it clears any latched data and writes its own.
		run(c, {.st = st, .sr = k.reg, .sm = 62, .m2 = M2_CELL, .add = ADD_REG, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The early reflections, rev::ER_WORDS
// ---------------------------------------------------------------------------------------------

// One early reflection word: its own coefficient on one cell, added to the running value, and for the
// last of a group the register that closes the group. The groups of five and six are the two halves of
// the surround the reverb sends to the left and the right, r[119] and r[120].
struct ErWord
{
	int st;                  // the step this word is at, which is what index_by_step keys on
	int cell, out_reg;       // out_reg 0 unless this word closes its group
	add_t add;
	int add_reg;             // the register the adder reads, when `add` is ADD_REG
	Decor decor;
	delay_t write;
};

// rev::ER_WORDS[0] to [21], in step order. The first word of the last two groups adds to the surround's
// own register instead of the running value, which is what carries the reverb's input into them.
constexpr ErWord ER[22] = {
    //   st   cell  out  add        reg   decor                write
    { 0x02a, 56, 0,   ADD_OFF, 0,   {},                 NO_WRITE      },
    { 0x02b, 57, 0,   ADD_ACC, 0,   {},                 WRITE_LATCHED },
    { 0x02c, 58, 0,   ADD_ACC, 0,   {DEC_CAPTURE, 57},  NO_WRITE      },
    { 0x02d, 59, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x02e, 60, 119, ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x035, 61, 0,   ADD_OFF, 0,   {},                 NO_WRITE      },
    { 0x036, 56, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x037, 57, 0,   ADD_ACC, 0,   {},                 WRITE_LATCHED },
    { 0x038, 56, 0,   ADD_ACC, 0,   {DEC_CAPTURE, 59},  NO_WRITE      },
    { 0x039, 62, 120, ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x059, 56, 0,   ADD_REG, 119, {},                 NO_WRITE      },
    { 0x05a, 57, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x05b, 58, 0,   ADD_ACC, 0,   {DEC_READ, 0},      NO_WRITE      },
    { 0x05c, 59, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x05d, 56, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x05e, 63, 119, ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x083, 56, 0,   ADD_REG, 120, {DEC_SERIAL_R, 58}, WRITE_LATCHED },
    { 0x084, 57, 0,   ADD_ACC, 0,   {DEC_CAPTURE, 56},  NO_WRITE      },
    { 0x085, 58, 0,   ADD_ACC, 0,   {},                 NO_WRITE      },
    { 0x086, 59, 0,   ADD_ACC, 0,   {DEC_LFO, 12},      NO_WRITE      },
    { 0x087, 60, 0,   ADD_ACC, 0,   {DEC_READ, 0},      NO_WRITE      },
    { 0x088, 61, 120, ADD_ACC, 0,   {},                 NO_WRITE      },
};

// One early reflection word. The words that close a group write a register and saturate; the ones in
// between only move the running value and wrap.
void er_word(yss236_core& c, ErWord const& e)
{
	decor(c, e.st, e.decor);
	step::alu a;
	a.st = e.st;
	a.sm = e.cell;
	a.m2 = M2_CELL;
	a.sr = e.add_reg;
	a.add = e.add;
	a.clamp = e.out_reg ? CLAMP_SAT : CLAMP_WRAP;
	a.dr = e.out_reg;
	a.wdata = e.write == SET_LATCH;
	a.write = e.write != SET_LATCH && e.write != NO_WRITE;
	run(c, a);
}

// The early reflection word of step st, if it is one. The index is the same for every pass and every
// type, so the step reaches its word rather than looking through the twenty-two for it.
constexpr auto ER_AT = index_by_step(ER);

// The allpass unit of step st, if it is one. The four steps of a unit are consecutive, so each of them
// indexes to the same row with a different k.
constexpr auto ALLPASS_AT = index_by_span(ALLPASS);

// The early reflection word of a step, or null. `assert` is not wanted on the sample path, so a step
// no word owns is simply one the caller does not hand here.
ErWord const* er_at(int st) { return ER_AT[st] < 0 ? nullptr : &ER[ER_AT[st]]; }

// ---------------------------------------------------------------------------------------------
// The input filters, rev::HPF_STEPS, rev::LPF_STEPS and their two copies
// ---------------------------------------------------------------------------------------------

// One of the four three-word sections of the block at 0x01a. Word 1 applies the section's first
// coefficient to the stage before it, word 2 adds the second to that sum, and word 3 closes it into
// the section's own register with the route its design gave, which is what the highpass sections' x1
// and the lowpass sections' x2 are. `w1` is the register word 1 reads, which is also where word 3
// writes, and `w2` the register word 2 reads, one stage back.
struct Section3 { int st, nsteps, w1, w2, out; step::route_t route; Decor decor[3]; };

constexpr Section3 SECTIONS[4] = {
    //  base   n  w1  w2  out  route    word 1            word 2              word 3
    { 0x01b, 3, 51, 50, 51, ROUTE_1, { {DEC_READ, 0}, {DEC_CAPTURE, 62}, {}                } },  // HPF_COPY
    { 0x01e, 3, 52, 51, 52, ROUTE_2, { {},            {DEC_READ, 0},     {DEC_CAPTURE, 22} } },  // LPF_COPY
    { 0x023, 3, 48, 47, 48, ROUTE_1, { {DEC_READ, 0}, {DEC_CAPTURE, 61}, {}                } },  // HPF_STEPS
    { 0x026, 3, 49, 48, 49, ROUTE_2, { {DEC_LFO, 9},  {DEC_READ, 0},     {DEC_CAPTURE, 56} } },  // LPF_STEPS
};

// A section's three steps, and so which section and which word of it a step is. The index is the same on
// every pass, so the step reaches its section rather than looking through the four for it.
constexpr auto SECTION3_AT = index_by_span(SECTIONS);

// ---------------------------------------------------------------------------------------------
// The runs of steps, which rom::STEPS_REVERB fixes and RUNS below names
// ---------------------------------------------------------------------------------------------

// One run of the window's steps and the function that runs it. Every block here walks its own run from
// `from` to `to`, so a block shared by several runs needs nothing but the range.
struct StepRun
{
	int from, to;
	void (*block)(yss236_core&);
};

// ---- 0x01a to 0x039: the mix and width words, the input filters and the first ten reflections ----
template <int from, int to>
void block_0x01a(yss236_core& c)
{
	for_steps<from, to>(c, [&](yss236_core& c, auto st)
	{
		// ---- the surround's mix words, the stereo width and the type's shelf ----
		// rev::MIX_STEPS, rev::WIDTH_STEPS and rev::TABLE_STEPS. The two width words scale a cell and
		// add the surround's register to them, and latch the result as the data of the delay write an
		// early reflection pair makes a few steps later.
		switch (st)
		{
		case 0x01a: gain(c, st, 120, 50, ROUTE_2); return;       // mix word 1: the right surround into the filter chain
		case 0x021: term(c, st, 120); return;                    // mix word 2
		case 0x022: close(c, st, 119, 47, ROUTE_2); return;       // mix word 0: the left surround into r[47]
		case 0x02f: read_delay(c, st); run(c, {.st = st, .sm = 60, .m2 = M2_CELL}); return;      // mix word 3
		case 0x030: run(c, {.st = st, .sm = 62, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = 53}); return;  // mix word 4
		case 0x029:                                                                        // the width, word 1
			run(c, {.st = st, .sr = 52, .sm = 62, .m2 = M2_CELL, .add = ADD_REG, .clamp = CLAMP_SAT, .wdata = true});
			return;
		case 0x034:                                                                        // the width, word 0
			take_capture(c, 56, st);
			run(c, {.st = st, .sr = 49, .sm = 60, .m2 = M2_CELL, .add = ADD_REG, .clamp = CLAMP_SAT, .wdata = true});
			return;
		case 0x031: term(c, st, 54); return;                     // the type's shelf, word 1
		case 0x032: sum(c, st, 53); return;                      // word 2
		case 0x033: read_delay(c, st); close(c, st, 53, 54, ROUTE_4); return;          // word 3, out of the shelf line
		default: break;
		}

		// ---- the two input filters and the two copies the type's shelf works from ----
		if (int const k = SECTION3_AT[st]; k >= 0)
		{
			Section3 const& f = SECTIONS[k];
			int const w = st - f.st;
			decor(c, st, f.decor[w]);
			switch (w)
			{
			case 0:  term(c, st, f.w1); break;                       // word 1: the coefficient on the stage before
			case 1:  sum(c, st, f.w2); break;                        // word 2: plus this section's own delay
			default: close(c, st, f.w2, f.out, f.route); break;      // word 3: closed and scaled into this section
			}
		}

		// ---- the first ten early reflections, rev::ER_WORDS[0] to [9] ----
		if (ErWord const* e = er_at(st)) er_word(c, *e);
		});
}

// ---- one run of the reverb's steps that is allpass units and early reflections, nothing else ----
template <int from, int to>
void unit_block(yss236_core& c)
{
	for_steps<from, to>(c, [&](yss236_core& c, auto st)
	{
		if (int u = ALLPASS_AT[st]; u >= 0)
			allpass_step(c, st, ALLPASS[u], st - ALLPASS[u].st);
		else if (ErWord const* e = er_at(st))
			er_word(c, *e);
		});
}

// ---- 0x0a3 to 0x0b5: the six comb units' state steps and the first four gains ----
template <int from, int to>
void comb_block(yss236_core& c)
{
	for_steps<from, to>(c, [&](yss236_core& c, auto st)
	{
		CombStep const& s = COMB_SCHED[st - 0x0a3];
		comb_step(c, st, COMB[s.unit], s.role);
		});
}

// ---- 0x0d1 to 0x0d7: comb 4's gain, comb 5's state steps, the level, comb 5's gain ----
// The level's two words are the surround's own registers scaled by the type's level words, the one step
// that is not a comb unit's, so the run has its own switch rather than the schedule's.
template <int from, int to>
void level_block(yss236_core& c)
{
	for_steps<from, to>(c, [&](yss236_core& c, auto st)
	{
		switch (st)
		{
		case 0x0d1: comb_step(c, st, COMB[4], COMB_GAIN); break;
		case 0x0d2: comb_step(c, st, COMB[5], COMB_X1); break;
		case 0x0d3: comb_step(c, st, COMB[5], COMB_X2); break;
		case 0x0d4: comb_step(c, st, COMB[5], COMB_A); break;
		case 0x0d5: gain(c, st, 120, 10, ROUTE_4); break;     // the level, word 1: the right return
		case 0x0d6: gain(c, st, 119, 9, ROUTE_4); break;      // the level, word 0: the left return
		default:   comb_step(c, st, COMB[5], COMB_GAIN); break;
		}
		});
}

// The ten runs of the window's block, in step order. Five of them are one allpass unit each and all six
// run the same function, so the reverb is six units and twenty-two reflections laid out in step order
// rather than ten separate blocks.
constexpr StepRun RUNS[] = {
    { 0x01a, 0x03a, block_0x01a<0x01a, 0x03a> },  // the mix and width words, the input filters, reflections 0 to 9
    { 0x03e, 0x042, unit_block<0x03e,  0x042> },  // allpass unit 0
    { 0x04c, 0x050, unit_block<0x04c,  0x050> },  // allpass unit 1
    { 0x059, 0x05f, unit_block<0x059,  0x05f> },  // reflections 10 to 15
    { 0x062, 0x066, unit_block<0x062,  0x066> },  // allpass unit 2
    { 0x07f, 0x089, unit_block<0x07f,  0x089> },  // allpass unit 3, then reflections 16 to 21
    { 0x08e, 0x092, unit_block<0x08e,  0x092> },  // allpass unit 4
    { 0x097, 0x09b, unit_block<0x097,  0x09b> },  // allpass unit 5
    { 0x0a3, 0x0b6, comb_block<0x0a3,  0x0b6> },  // the comb units' state steps and the first four gains
    { 0x0d1, 0x0d8, level_block<0x0d1, 0x0d8> },  // comb 4's gain, comb 5's steps, the level, comb 5's gain
};

}  // namespace

// One run of the reverb window's steps, exactly as yss236_core::step() would run them. The engine hands
// this one of the ten runs above per pass; a range none of them covers, which no host builds, falls
// back to the interpreter.
void rev(yss236_core& c, int from, int to)
{
    for (StepRun const& run : RUNS)
        if (run.from == from && run.to == to) { run.block(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
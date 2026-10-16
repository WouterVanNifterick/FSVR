// fs1r/native/ins_m07.cpp - VOP3-2, the insertion window's program for Pitch Chng, Ens Detune and 3-Band EQ:
// its 96 steps.
//
// rom::INSERTION_PROGRAMS[7], the program the insertion window holds under the three types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 7: ins-07 Pitch Chng on EffectHost::pitchChange,
// ins-08 Ens Detune on EffectHost::ensembleDetune and ins-21 3-Band EQ on EffectHost::threeBandEq. One
// program, three types: the handlers differ only in which words they write, so nothing here knows which of
// them is running. No step of it is a control step.
//
// The ROM names the steps, and the lists agree with the program word for word:
//
//   LFO_DIR_STEPS  {0x1A0,0x1A8,0x1B0,0x1B7}        the four read positions
//   PC_FB          {0x1F9,0x1FE,0x1A7,0x1AA,0x1AF,0x1B1}    the delay feedback
//   PC_MIX         {0x1BA,0x1CB,0x1CA,0x1E9,0x1EE,0x1EB,0x1F0}
//   PC_WET         {0x1E8,0x1ED}
//   PC_VOICE1_A    {0x1E4}   PC_VOICE1_B {0x1E7}   PC_VOICE2 {0x1E6,0x1E5}
//   EQ3_LEVEL      {0x1EA}
//   EQ7_A          {0x1BF,0x1BB,0x1BC,0x1BD,0x1BE}   the low band: b0, b1, b2, -a1, -a2
//   EQ7_B          {0x1C4,0x1C0,0x1C1,0x1C2,0x1C3}   the middle band, the same five
//   EQ7_C          {0x1C9,0x1C5,0x1C6,0x1C7,0x1C8}   the high band, the same five
//   EQ7_A_C        {0x1D0,0x1CC,0x1CD,0x1CE,0x1CF}   and each band's copy for the other channel
//   EQ7_B_C        {0x1D5,0x1D1,0x1D2,0x1D3,0x1D4}
//   EQ7_C_C        {0x1DA,0x1D6,0x1D7,0x1D8,0x1D9}
//   PC_SLOTS       {0x7E,0x68,0x69,0x6A,0x6B,0x7F,0x6C,0x6D,0x6E,0x6F}
//
// fx_tone.cpp's threeBandEq() designs each band with designFilter(2, 1, ...) for the two lower ones and
// designFilter(2, 4, ...) for the high one, which writes b0, b1, b2, -a1 and -a2 into the list in that
// order and copies them to the other channel's list. Each word is the designed coefficient times
// 2048 * (16 >> scale), which the chip's s16() halves, and the b0 step's own route multiplies the whole
// section it closes: a shift of one on 0x1BF, 0x1C4, 0x1D0 and 0x1D5, and a shift of three on 0x1C9 and
// 0x1DA, so RSCALE's 2 and 16 restore the factor the designer's scale took out. What each section computes
// is then
//
//   y[n] = b0 x[n-1] + b1 x[n-1] + b2 x[n-2] + (-a1) y[n-1] + (-a2) y[n-2]
//
// six order-2 sections, three bands per channel, each on the previous band's output. The gain path runs one
// sample behind the input because the b1 step reads the input register the step before the b0 step
// refilled, which is what r[68] and r[70] and r[72] are for: each is an rcopy step, so it takes the value
// the step before it latched.
//
// The mode parameter is the one input mix. ins::PC_MIX[0] (0x1BA) is the stereo switch on the left input,
// ins::PC_MIX[1] and [2] (0x1CB, 0x1CA) sum the two inputs into r[75], and ins::PC_MIX[3] (0x1E9) against
// ins::EQ3_LEVEL (0x1EA) choose which chain joins the dry signal, so in stereo the left chain does and in
// mono the summed one. The old model in vop3_effects.h runs this as two detuned fractional taps crossfaded
// in floating point over a shelf, a peak and a high shelf; it has none of these words, so it is no reference
// for any number here.
//
// The registers, so the names below can be checked against the words. r[7] and r[8] the two signals the
// base program leaves this window and r[17] and r[18] the two outputs it reads back; r[67] ins::PC_MIX[0]
// and r[75] ins::PC_MIX[1] and [2], the two channels' inputs; r[68] to r[82] the six sections' own
// registers, where a section's x[-1] is the next one's y[-2]; r[73] and r[81] the two chains' outputs;
// r[119] and r[120] the two voice levels; r[121] to r[126] the head's six, which are two rectified LFO
// amplitudes and four delay lengths. The head's registers change hands as the program runs: r[121] is one
// voice's rectified amplitude through 0x1aa and that voice's first tap from 0x1b9, and r[122] is the other
// voice's second delay length through 0x1a8 and the second crossfade's output from 0x1dc. Of the cells,
// m[3] and m[7] are the banks of page 13's serial input, m[4] page 14's left, m[13], m[14] and m[15] the
// LFOs of pages 13, 14 and 14, m[42] and m[43] the two delay lines' signals, and m[56] to m[63] the eight
// swept taps. Of the slots ins::PC_SLOTS names ten; the program reads two more, 0x70 and 0x71 at 0x1c3
// and 0x1c7, and writes two more, 0x7b and 0x7c at 0x1ef and 0x1f3, which no list in the ROM names.
//
// Every constant the program reads is read from c.coef[st] on the pass and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time. Inside the groups the comments below open, each repeated shape
// is a table and a small function that runs it: the head's four read positions, mode words, rectifiers
// and tap gains, the four interpolated taps, the six order-2 sections, the two voice crossfades, the
// two voice levels and the two delay lines' feeds.
//
// vop3_native_check ins-m07: identical, 3 types, 28800 native passes.
#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: load, run, term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The registers and the cells the code below names
// ---------------------------------------------------------------------------------------------

// The two signals the base program leaves this window and the two outputs it reads back, the register
// each band chain ends in, and the four registers the voice levels, the crossfades and the two delay
// lines pass between them. The six sections' own registers are named by the sections themselves, which
// are what read and write them, and so is r[124], which only the head uses.
enum
{
	SIG_L = 7,       // the two signals in, r[7] and r[8]: the panned sends the base program leaves here
	SIG_R = 8,
	OUT_L = 17,      // the two outputs the base program reads back, r[17] and r[18]
	OUT_R = 18,
	DRY_L = 67,      // r[67]: ins::PC_MIX[0], the stereo switch on the left input, and the low band's
	MONO = 75,       //   input; r[75]: ins::PC_MIX[1] and [2], the two signals summed, the other input
	CHAIN_L = 73,    // r[73]: the left chain's output, which ins::PC_MIX[3] and the first line read;
	CHAIN_C = 81,    //   r[81]: the summed chain's, which ins::EQ3_LEVEL[0] and the second line read
	VOICE_A = 119,   // r[119] and r[120]: ins::PC_VOICE2[1] and ins::PC_VOICE1_B, the two voice levels,
	VOICE_B = 120,   //   which the two crossfades weigh and the two outputs close on
	CROSS_A = 121,   // r[121] and r[122]: one tap and one crossfade output each. The head has r[121] as
	CROSS_B = 122,   //   a rectified amplitude and r[122] as a delay length before 0x1b9 and 0x1dc
	RECT_2 = 124,    // r[124]: the second voice's rectified amplitude, which nothing after 0x1b4 reads
	LINE_L = 42,     // m[42] and m[43]: the two cells the two delay lines take their signals through
	LINE_R = 43,
};

// ---------------------------------------------------------------------------------------------
// What a step moves into a cell, or reads out of the delay RAM, before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step of this program usually also takes a serial input, an LFO, a capture of an earlier delay RAM
// read, or reads the delay RAM itself. The read comes first on every step that carries both, because
// step() reads the delay RAM before it moves a cell. RD_RING reads at the ring pointer, RD_RING_AT the
// same read at the index a delay length queued, which is what makes a tap pair two words a sample
// apart, and RD_OLD_AT the word one further on.
enum read_t { RD_NONE, RD_RING, RD_RING_AT, RD_OLD_AT };

// MV_CAPTURE takes up the read made CAPMIN to 11 steps earlier, MV_LFO the step's own page's LFO, and
// MV_SERIAL_L and MV_SERIAL_R that page's left and right serial input.
enum move_t { MV_NONE, MV_CAPTURE, MV_LFO, MV_SERIAL_L, MV_SERIAL_R };

// What one step does before its arithmetic: its delay RAM read, and the value it moves into a cell.
struct Decor { read_t read; move_t move; int cell; };

// The decor of a step that carries neither, so a row names only the steps that have one.
constexpr Decor NONE = { RD_NONE, MV_NONE, 0 };

// Apply a step's decor. Both halves come before its arithmetic, in the order step() does them.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.read)
	{
		case RD_RING:    read_delay(c, st); break;
		case RD_RING_AT: read_delay(c, st, false, true); break;
		case RD_OLD_AT:  read_delay(c, st, true, true); break;
		case RD_NONE: break;
	}
	switch (d.move)
	{
		case MV_CAPTURE:  take_capture(c, d.cell, st); break;
		case MV_LFO:      take_lfo(c, d.cell, st); break;
		case MV_SERIAL_L: take_serial(c, d.cell, st, 0); break;
		case MV_SERIAL_R: take_serial(c, d.cell, st, 1); break;
		case MV_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1a0 to 0x1b4, the head: four read positions, six mode words, two unity words, two rectifiers,
// four tap gains, and four steps of its own
// ---------------------------------------------------------------------------------------------

// One of the four read positions, ins::LFO_DIR_STEPS. Its word on the cell it scales or on the register
// it reads, with the negatives held at zero, leaves the delay length in samples: the fraction below one
// goes into the t register the tap pair further down interpolates on, and the whole part becomes the
// index that pair's two reads take three steps later.
struct Sweep { int st, cell, src, treg; Decor move; };

const Sweep SWEEPS[4] = {
	//  the step  the cell  the register  the t register   and the decor it carries
	{  0x1a0,     12,      0,          10,               NONE                         },
	{  0x1a8,      0,    122,          12,               { RD_NONE, MV_CAPTURE, 56 } },
	{  0x1b0,     13,      0,          14,               { RD_NONE, MV_CAPTURE, 60 } },
	{  0x1b7,      0,    125,           3,               { RD_OLD_AT, MV_NONE, 0 }    },
};

void sweep(yss236_core& c, int st, Sweep const& s)
{
	decor(c, st, s.move);
	double const y = run(c, { st, s.src, s.cell, s.cell ? M2_CELL : M2_REG, false, 0, ADD_OFF,
	                          false, ROUTE_1, CLAMP_ZERO });
	t_put_fraction(c, s.treg, y);   // the fraction below one sample
	load_index(c, y);               // and the whole of it, which the pair's two reads take three steps on
}

// The read position of a step, -1 where no position is. The four rows never move, so a step reaches its
// row rather than looking through the four for it on every pass.
constexpr auto SWEEPS_AT = index_by_step(SWEEPS);

// One of the six mode words, the class-1 steps that load a register from its own word with the negatives
// held at zero. Four add the word to the running value and two to the register the step has just latched,
// which is what a word the host writes as a difference from the value already there wants rather than as
// a depth on top of it. All six are ins::PC_FB's words, and the two of them that read the delay RAM read
// it one word older than the tap pair they serve.
struct Mode { int st, reg; add_t add; Decor move; };

const Mode MODES[6] = {
	//  the step  the register  the adder reads   and the delay RAM read it carries
	{  0x1a5,     122,       ADD_ACC,           NONE                         },
	{  0x1a7,     123,       ADD_ACC,           { RD_OLD_AT, MV_NONE, 0 }   },
	{  0x1aa,     121,       ADD_REG,           NONE                         },
	{  0x1ad,     125,       ADD_ACC,           NONE                         },
	{  0x1af,     126,       ADD_ACC,           { RD_OLD_AT, MV_NONE, 0 }   },
	{  0x1b1,     124,       ADD_REG,           NONE                         },
};

void mode_word(yss236_core& c, int st, Mode const& m)
{
	decor(c, st, m.move);
	load(c, st, m.add == ADD_REG ? m.reg : 0, 0, m.add, false, ROUTE_1, CLAMP_ZERO, m.reg);
}

// The mode word of a step, -1 where there is none: six rows that never move, indexed once at compile time.
constexpr auto MODES_AT = index_by_step(MODES);

// The two rectifiers. Each is a class-3 step, so its word is 1.0 and the register it has just latched is
// the product whole, and what the running value takes is its absolute value: a delay length is never
// negative. The first one's step also takes this window's first LFO into m[13].
struct Rect { int st, reg; Decor move; };

const Rect RECTS[2] = {
	{  0x1a6, CROSS_A, { RD_NONE, MV_LFO, 13 } },
	{  0x1ae, RECT_2,  NONE },
};

void rectifier(yss236_core& c, int st, Rect const& r)
{
	decor(c, st, r.move);
	run(c, { st, r.reg, 0, M2_REG, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_ABS, r.reg });
}

// The rectifier of a step, -1 where there is none: two rows that never move, indexed once at compile time.
constexpr auto RECTS_AT = index_by_step(RECTS);

// The two unity words, which put a rectified amplitude on its own voice's first tap register before the
// mode word of ins::PC_FB[2] and [3] adds its depth to it. A class-3 step multiplies nothing, so the
// cell is the whole of it and the x2 is the route. The first one's step also takes the left serial input
// of page 13 into m[3].
struct Unity { int st, cell, reg; Decor move; };

const Unity UNITIES[2] = {
	{  0x1a1, 12, CROSS_A, { RD_NONE, MV_SERIAL_L, 3 } },
	{  0x1a9, 13, RECT_2,  NONE },
};

void unity(yss236_core& c, int st, Unity const& u)
{
	decor(c, st, u.move);
	run(c, { st, 0, u.cell, M2_CELL, true, 0, ADD_OFF, false, ROUTE_2, CLAMP_WRAP, u.reg });
}

// The unity word of a step, -1 where there is none: two rows that never move, indexed once at compile time.
constexpr auto UNITIES_AT = index_by_step(UNITIES);

// One of the four tap gains, the steps that scale a delay length by x16 and leave it in the t file as a
// 1.15 multiplier, which is what weights the tap pair further down against the voice's gain. The two
// steps that carry a delay RAM read are the pair's earlier word; the last one publishes the read of
// 0x1af into m[61], the later word of its pair.
struct TapGain { int st, src, treg; Decor move; };

const TapGain TAP_GAINS[4] = {
	//  the step  the length  the t register   and the decor it carries
	{  0x1ab,   123,        9,               { RD_RING_AT, MV_NONE, 0 }    },
	{  0x1b2,   CROSS_A,   11,               NONE                          },
	{  0x1b3,   126,       13,               { RD_RING_AT, MV_NONE, 0 }    },
	{  0x1b4,   RECT_2,    15,               { RD_NONE, MV_CAPTURE, 61 }   },
};

void tap_gain(yss236_core& c, int st, TapGain const& g)
{
	decor(c, st, g.move);
	t_put(c, g.treg, run(c, { st, g.src, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_SAT }));
}

// The tap gain of a step, -1 where there is none: four rows that never move, indexed once at compile time.
constexpr auto TAP_GAINS_AT = index_by_step(TAP_GAINS);

// The head. Its steps are irregular and interleave the five tables above, so each step is looked up in
// them and the first that names it runs it, which is every step from 0x1a0 to 0x1b4 and nothing else.
// Four steps fit none of the five and are here. The five indexes answer in the same order the scans ran,
// so a step two tables name still runs the first one's row.
void head(yss236_core& c, int st)
{
	int i;
	if ((i = SWEEPS_AT[st]) >= 0)   { sweep(c, st, SWEEPS[i]); return; }
	if ((i = RECTS_AT[st]) >= 0)     { rectifier(c, st, RECTS[i]); return; }
	if ((i = MODES_AT[st]) >= 0)     { mode_word(c, st, MODES[i]); return; }
	if ((i = UNITIES_AT[st]) >= 0)   { unity(c, st, UNITIES[i]); return; }
	if ((i = TAP_GAINS_AT[st]) >= 0) { tap_gain(c, st, TAP_GAINS[i]); return; }
	switch (st)
	{
	case 0x1a2:
		// m[7] takes the right serial input of page 13, which no step of this program reads
		take_serial(c, 7, st, 1);
		idle(c);
		break;
	case 0x1a3:
		read_delay(c, st, false, true);    // the slot 0x68 at the position 0x1a0 loaded, for m[56] at 0x1a8
		idle(c);
		break;
	case 0x1a4:
		term(c, st, CROSS_A);             // the ROM's own word on the first voice's amplitude, r[121]
		break;
	case 0x1ac:
		take_capture(c, 57, st);          // m[57] takes up the read of 0x1a7, the later word of the
		term(c, st, RECT_2);              //   pair the gain at 0x1ab weights
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The four interpolated taps, 0x1b5, 0x1b8, 0x1db and 0x1dd, and the two voice crossfades
// ---------------------------------------------------------------------------------------------

// One tap pair: two words a sample apart at the index a read position queued, with the position's
// fraction taken off the earlier word and applied to the later one, so the register the pair's second
// step writes holds the delay line interpolated at that position. `st` is the pair's first step and
// `nsteps` the two it runs. `treg` is the t register holding the fraction, `early` the cell the word at
// the index filled and `late` the cell the word one older filled, both taken up five steps after the
// read that filled them, and `out` the register the pair closes in. `move` is the decor the pair's first
// step carries, which is the only step of any of the four that has one: m[58] publishes the read of
// 0x1b3.
struct Tap { int st, nsteps; int treg, early, late, out; Decor move; };

// The four pairs interpolate on the t registers the read positions and the tap gains wrote between them:
// t[10] and t[9] for the first, t[12] and t[14] for the second, t[3] for the third and t[15] and t[13]
// for the fourth. The eight cells are the six slots ins::PC_SLOTS names at 0x68 to 0x6d, each read twice
// a sample apart, once at the index the read position queued and once one word older.
const Tap TAPS[4] = {
	//  the first step  the steps it runs  the fraction  the earlier cell  the later cell  the register   and its decor
	{  0x1b5,       2,             10,       56,          57,          VOICE_A,       NONE                        },
	{  0x1b8,       2,             12,       60,          61,          CROSS_A,       { RD_NONE, MV_CAPTURE, 58 } },
	{  0x1db,       2,              3,       62,          63,          CROSS_B,       NONE                        },
	{  0x1dd,       2,             14,       58,          59,          VOICE_B,       NONE                        },
};

// One pair's step k of the two, which is the word it applies: the earlier one takes the fraction off and
// the later one puts it on.
void tap_pair(yss236_core& c, int st, Tap const& p, int k)
{
	if (k == 0)
	{
		decor(c, st, p.move);
		// the fraction on the word at the index, with the word itself taken off, which is the pair
		// interpolated at the index less one word
		run(c, { st, 0, p.early, M2_CELL, false, p.treg, ADD_CELL, true, ROUTE_1, CLAMP_WRAP });
		return;
	}
	// and the word one older on the same weight less the running value, into the register the voice's own
	// gains scale. This is the tap the pair exists for.
	run(c, { st, 0, p.late, M2_CELL, false, p.treg, ADD_ACC, true, ROUTE_1, CLAMP_SAT, p.out });
}

// The tap pair of a step, -1 where there is none. The two steps of a pair are consecutive, so both index
// to the same row and say which of the two they are with st - TAPS[i].st.
constexpr auto TAPS_AT = index_by_span(TAPS);

// The first voice's two taps, 0x1b5 to 0x1b9, and ins::PC_MIX[0] at 0x1ba. ins::LFO_DIR_STEPS[3] sits
// between the two pairs and is one of the four read positions, so the sweep table is looked up here as
// well as in the head. A step the pair table names runs its pair, one the sweep table names runs its
// position, and the rest falls through to the stereo switch below.
void first_voice(yss236_core& c, int st)
{
	int i;
	if ((i = TAPS_AT[st]) >= 0) { tap_pair(c, st, TAPS[i], st - TAPS[i].st); return; }
	if ((i = SWEEPS_AT[st]) >= 0) { sweep(c, st, SWEEPS[i]); return; }
	// ins::PC_MIX[0]: r[67] = 2 * ins::PC_MIX[0] * r[7], the stereo switch on the left input, which is
	// the low band's input. The adder is off, so this is a gain on the input rather than a sum, and the
	// tap pair above leaves the running value alone.
	gain(c, st, SIG_L, DRY_L, ROUTE_2);
}

// The two voice crossfades, which weigh one voice's two taps against the other's. The same two steps as a
// tap pair, but on the registers two tap pairs closed into rather than on the cells the delay RAM
// filled, and the first step's adder reads the register it has just latched rather than the running
// value, so the two of them are a value and its complement rather than a sum. The four words are the
// ROM's own. `st` is the pair's first step and `nsteps` the two it runs.
struct Xfade { int st, nsteps; int treg0, in0, treg1, in1, out; };

const Xfade XFADES[2] = {
	//  the first step  its steps  its weight  the tap it weighs   the second's weight  its tap    the register
	{  0x1df,       2,        11,        CROSS_A,             9,                  VOICE_A,  CROSS_A      },
	{  0x1e1,       2,        15,        CROSS_B,             13,                 VOICE_B,  CROSS_B      },
};

// One crossfade's step k of the two: the first is the value and the second its complement.
void voice_xfade(yss236_core& c, int st, Xfade const& x, int k)
{
	if (k == 0)
	{
		run(c, { st, x.in0, 0, M2_REG, false, x.treg0, ADD_REG, true, ROUTE_1, CLAMP_WRAP });
		return;
	}
	run(c, { st, x.in1, 0, M2_REG, false, x.treg1, ADD_ACC, true, ROUTE_1, CLAMP_SAT, x.out });
}

// The crossfade of a step, -1 where there is none: the two steps of a crossfade are consecutive, so both
// index to the same row.
constexpr auto XFADES_AT = index_by_span(XFADES);

// The second voice's two taps and the two crossfades, 0x1db to 0x1e2. The first pair reads at the
// position ins::LFO_DIR_STEPS[3] loaded and the second at the one ins::LFO_DIR_STEPS[2] did, so the two
// voices' taps are crossfaded in the other order from the first voice's. A step a pair names runs the
// pair, one a crossfade names runs the crossfade, and none of the four leave the running value alone.
void second_voice(yss236_core& c, int st)
{
	int i;
	if ((i = TAPS_AT[st]) >= 0) { tap_pair(c, st, TAPS[i], st - TAPS[i].st); return; }
	if ((i = XFADES_AT[st]) >= 0) { voice_xfade(c, st, XFADES[i], st - XFADES[i].st); return; }
}

// ---------------------------------------------------------------------------------------------
// The six order-2 sections, 0x1bb to 0x1c9 and 0x1cc to 0x1da
// ---------------------------------------------------------------------------------------------

// Which of a section's five steps computes which of the coefficients designFilter() wrote. b1 opens the
// sum on the section's input, b2 adds the input a pass back and is the step that keeps it there, -a1
// adds the section's output a pass back, -a2 adds the output two passes back, and b0 closes the sum
// into the output with the route the designer's own (16 >> scale) asks for. The two high bands carry
// their z^-1 on the -a2 step instead, which is A2_Y2: the same term, and y[-2] kept.
enum band_part_t { B1, B2_X1, A1, A2, A2_Y2, B0 };

// One section, in direct form I. `st` is the section's first step and `nsteps` the five it runs. `in` is
// the register the previous section's output arrives in, or the stereo switch's for the first of each
// channel, `x1` the one that holds the input a pass back, `y2` the one that holds the output two passes
// back, and `out` the one b0 writes and -a1 reads a pass back. The register each pair of neighbours
// shares is the first one's y2 and the second's x1, which is how the six run as one chain three bands
// deep per channel rather than as six sections of their own. The rows below are ins::EQ7_A, EQ7_B and
// EQ7_C and then ins::EQ7_A_C, EQ7_B_C and EQ7_C_C, each naming its five words b0 first and the other
// four in designFilter's order.
struct Band
{
	int st, nsteps;
	int in, x1, y2, out;
	step::route_t route;
	band_part_t part[5];
	Decor move[5];
};

const Band BANDS[6] = {
	//  base  n  in  x1  y2  out  route      the five parts                     and each step's decor
	{  0x1bb, 5, 67, 68, 70, 69, ROUTE_2,   { B1, B2_X1, A1, A2, B0 },
	  { { RD_RING_AT, MV_NONE, 0 }, { RD_NONE, MV_CAPTURE, 59 }, NONE, NONE, { RD_OLD_AT, MV_NONE, 0 } } },
	{  0x1c0, 5, 69, 70, 72, 71, ROUTE_2,   { B1, B2_X1, A1, A2, B0 },
	  { { RD_NONE, MV_CAPTURE, 62 }, NONE, NONE, { RD_RING, MV_NONE, 0 }, { RD_NONE, MV_CAPTURE, 63 } } },
	{  0x1c5, 5, 71, 72, 74, 73, ROUTE_16,  { B1, B2_X1, A1, A2_Y2, B0 },
	  { NONE, NONE, { RD_RING, MV_LFO, 14 }, { RD_NONE, MV_CAPTURE, 42 }, NONE } },
	{  0x1cc, 5, 75, 76, 78, 77, ROUTE_2,   { B1, B2_X1, A1, A2, B0 },
	  { { RD_NONE, MV_CAPTURE, 43 }, NONE, NONE, NONE, NONE } },
	{  0x1d1, 5, 77, 78, 80, 79, ROUTE_2,   { B1, B2_X1, A1, A2, B0 },
	  { { RD_NONE, MV_SERIAL_L, 4 }, NONE, NONE, NONE, NONE } },
	{  0x1d6, 5, 79, 80, 82, 81, ROUTE_16,  { B1, B2_X1, A1, A2_Y2, B0 },
	  { NONE, NONE, NONE, NONE, NONE } },
};

// The section of a step, -1 where it belongs to none. The index says which section and its `nsteps`
// which of the section's five steps this is.
constexpr auto BANDS_AT = index_by_span(BANDS);

// The section a step belongs to, or null when it belongs to none. The two runs of three are 0x1bb to
// 0x1c9 and 0x1cc to 0x1da, the left channel's bands and the other one's.
Band const* band_for(int st)
{
	return BANDS_AT[st] < 0 ? nullptr : &BANDS[BANDS_AT[st]];
}

// One section's step k of five, named after the coefficient the step's word is.
void band_step(yss236_core& c, int st, Band const& b, int k)
{
	decor(c, st, b.move[k]);
	switch (b.part[k])
	{
	case B1:
		term(c, st, b.in);                                   // b1 on the input, opening the section
		break;
	case B2_X1:
		copy_z(c, st, b.x1, b.x1, ADD_ACC, CLAMP_WRAP);      // b2 on the input a pass back, and kept
		break;
	case A1:
		sum(c, st, b.out);                                   // -a1 on the output a pass back
		break;
	case A2:
		sum(c, st, b.y2);                                    // -a2 on the output two passes back
		break;
	case A2_Y2:
		copy_z(c, st, b.y2, b.y2, ADD_ACC, CLAMP_WRAP);      // the same, and y[-2] kept
		break;
	default:
		close(c, st, b.in, b.out, b.route);                  // b0 on the input, closing the section
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1ca and 0x1cb: the two signals summed into the other channel's input
// ---------------------------------------------------------------------------------------------

// ins::PC_MIX[2] and [1]: the right signal's word on its own, then the left one's on top of it, doubled
// and saturated into r[75], which is where the other channel's low band starts. In stereo the left
// chain joins the dry signal instead, which is what ins::PC_MIX[3] and ins::EQ3_LEVEL[0] are for.
void input_sum(yss236_core& c, int st)
{
	if (st == 0x1ca) { term(c, st, SIG_R); return; }
	close(c, st, SIG_L, MONO, ROUTE_2);
}

// ---------------------------------------------------------------------------------------------
// 0x1e3 to 0x1f0, the two voice levels and the dry and wet mix
// ---------------------------------------------------------------------------------------------

// One voice's level, two steps: its gain word scales the register its crossfade left, and that same word
// scales it again into the register the wet mix closes on. The left voice takes 0x1e4 and 0x1e7 and the
// right 0x1e6 and 0x1e5, so the schedule runs the two voices across each other, and the right voice's
// closing step is the one that also takes this window's last LFO into m[15]. The four words are
// ins::PC_VOICE1_A, ins::PC_VOICE2[0] and [1] and ins::PC_VOICE1_B.
struct Voice { int in_st, out_st, reg, out; Decor move; };

const Voice VOICES[2] = {
	//  the first step  its closing step  the register  the output  and the decor the closing step carries
	{  0x1e4,       0x1e7,            CROSS_A,     VOICE_B,    NONE                      },
	{  0x1e6,       0x1e5,            CROSS_B,     VOICE_A,    { RD_NONE, MV_LFO, 15 }   },
};

void voice_level(yss236_core& c, int st, Voice const& v)
{
	if (st == v.in_st) { term(c, st, v.reg); return; }
	decor(c, st, v.move);
	close(c, st, v.reg, v.out, ROUTE_1);
}

// The dry and wet mix. The two channels share the shape and take different chains: ins::PC_WET's two
// words scale the two signals the base program leaves this window, ins::PC_MIX[3] adds the left chain
// and ins::EQ3_LEVEL[0] the summed one, ins::PC_MIX[5] and [6] close the sums into the two outputs, and
// ins::PC_MIX[4] adds the summed chain to the left output before the right one closes on r[120]. Which
// chain is a word of its own is what the mode parameter moves: in stereo the left chain joins the dry
// signal and in mono the summed one, and the other chain's step leaves the running value as it was.
//
// VOICES is the one table left scanned, and it is two rows of two steps each. The four steps are not four
// consecutive ones: the two voices cross, so the left voice owns 0x1e4 and 0x1e7 and the right 0x1e6 and
// 0x1e5. index_by_span would have to be told that 0x1e4 runs three steps to claim 0x1e7, and then it
// would hand 0x1e5 and 0x1e6 to the left voice as well, which is the wrong row on two of the four steps.
// Two rows over four steps is the whole of the walk, so it stays a scan.
void output_mix(yss236_core& c, int st)
{
	for (Voice const& v : VOICES)
		if (st == v.in_st || st == v.out_st) { voice_level(c, st, v); return; }
	switch (st)
	{
	case 0x1e3:
		idle(c);                                   // the padding before the two voice levels
		break;
	case 0x1e8:
		term(c, st, SIG_L);                        // ins::PC_WET[0], the left dry signal
		break;
	case 0x1e9:
		sum(c, st, CHAIN_L);                       // ins::PC_MIX[3]: the left chain joins the dry
		break;
	case 0x1ea:
		sum(c, st, CHAIN_C);                       // ins::EQ3_LEVEL[0], the summed chain
		break;
	case 0x1eb:
		close(c, st, VOICE_A, OUT_L, ROUTE_4);     // ins::PC_MIX[5], the left output
		break;
	case 0x1ec:
		// the left line's signal, latched rather than written: m[42], the read of 0x1c3 published, which
		// the write at 0x1ef spends. A class-3 step's word is 1.0 and it passes the cell through.
		run(c, { st, 0, LINE_L, M2_CELL, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP,
		         0, false, 0, false, false, true, false });
		break;
	case 0x1ed:
		term(c, st, SIG_R);                        // ins::PC_WET[1], the right dry signal
		break;
	case 0x1ee:
		sum(c, st, CHAIN_C);                       // ins::PC_MIX[4]
		break;
	case 0x1ef:
		// the left line's write into the slot 0x7b. It spends the latch above rather than its own
		// result, and its own word still goes on the running value, which is the left chain again.
		run(c, { st, CHAIN_L, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_WRAP,
		         0, false, 0, false, false, false, true });
		break;
	case 0x1f0:
		close(c, st, VOICE_B, OUT_R, ROUTE_4);     // ins::PC_MIX[6], the right output
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x1f1 to 0x1ff, the two delay lines' feeds and the four writes that end them
// ---------------------------------------------------------------------------------------------

// The two delay lines' feeds. The first voice's line runs the left chain, the summed chain and both
// crossfades in that order and spends the latch 0x1fa leaves on the write at 0x1fb; the second line
// takes the summed chain and both crossfades and writes its own result. ins::PC_FB[0] and [1] are the
// two crossfade terms and the two chains' words are the ROM's own.
void line_feed(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1f1: case 0x1f2: case 0x1f4: case 0x1f5: case 0x1f6: case 0x1fc:
		idle(c);                                   // the padding between the writes
		break;
	case 0x1f3:
		// the second line's own signal, m[43] the read of 0x1c7 published, into the slot 0x7c. A writer
		// carrying w1[13] drops any latched data and writes its own result, and it is a class-3 step, so
		// its word is 1.0 and the cell passes through.
		run(c, { st, 0, LINE_R, M2_CELL, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP,
		         0, false, 0, false, false, true, true });
		break;
	case 0x1f7:
		term(c, st, CHAIN_L);                      // the left chain
		break;
	case 0x1f8:
		sum(c, st, CHAIN_C);                       // and the summed one
		break;
	case 0x1f9:
		sum(c, st, CROSS_A);                       // ins::PC_FB[0], the first voice's crossfade
		break;
	case 0x1fa:
		// the second voice's crossfade, and this one's result becomes the data of the write below rather
		// than going to a register
		run(c, { st, CROSS_B, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT,
		         0, false, 0, false, false, true, false });
		break;
	case 0x1fb:
		idle(c);
		write_delay_latched(c, st);                // the slot 0x7e, the first voice's line
		break;
	case 0x1fd:
		term(c, st, CHAIN_C);                      // the summed chain into the second line
		break;
	case 0x1fe:
		sum(c, st, CROSS_B);                       // the second voice's crossfade
		break;
	case 0x1ff:
		// ins::PC_FB[1] itself, into the slot 0x7f. A writer carrying w1[13], so it drops the latch
		// 0x1fa left and writes its own result.
		run(c, { st, CROSS_A, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT,
		         0, false, 0, false, false, true, true });
		break;
	}
}

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's.
void block_0x1a0(yss236_core& c)
{
	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		// ---- 0x1a0 to 0x1b4, the head: the four read positions, the six mode words, the two
		//      rectifiers, the two unity words and the four tap gains ----
		if (st < 0x1b5) { head(c, st); continue; }

		// ---- 0x1b5 to 0x1ba, the first voice's two interpolated taps and the stereo switch. The
		//      second voice's other read position, ins::LFO_DIR_STEPS[3], sits between the pairs,
		//      so the sweep table is looked up here as well as in the head ----
		if (st < 0x1bb) { first_voice(c, st); continue; }

		// ---- 0x1bb to 0x1c9 and 0x1cc to 0x1da, the two channels' three bands each ----
		if (Band const* b = band_for(st)) { band_step(c, st, *b, st - b->st); continue; }

		// ---- 0x1ca and 0x1cb, the two signals summed into the other channel's input ----
		if (st < 0x1cc) { input_sum(c, st); continue; }

		// ---- 0x1db to 0x1e2, the second voice's two interpolated taps and the two crossfades ----
		if (st < 0x1e3) { second_voice(c, st); continue; }

		// ---- 0x1e3 to 0x1f0, the two voice levels and the dry and wet mix ----
		if (st < 0x1f1) { output_mix(c, st); continue; }

		// ---- 0x1f1 to 0x1ff, the two delay lines' feeds and the four writes that end them ----
		line_feed(c, st);
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m07(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
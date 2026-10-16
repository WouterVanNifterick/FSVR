// fs1r/native/ins_m02.cpp - VOP3-2, the insertion window's program for type msb 02: its 96 steps.
//
// rom::INSERTION_PROGRAMS[2], the program the insertion window holds under the two types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 2: ins-05 Phaser1 and ins-06 Phaser2, programmed by
// EffectHost::phaser1() and phaser2() in fx_modulation.cpp. Like ins_m01 it is one of the fifteen
// programs the window shares, and the same words run under both types: what the two differ by is
// which coefficients they write, phaser1() its ten stage words and one feedback word, phaser2() the
// same ten, two feedback words and the LFO phase. Nothing here knows which one is running.
//
// Every constant the program reads is read from c.coef[st] on the pass, and the two swept allpass
// coefficients out of t[10] and t[11] on the pass too, so no number the firmware chose is folded in
// here: the program words are fixed per row, and fs1r/native/steps.h is the vocabulary the arithmetic
// below is written with, each step named for what it computes rather than for its own program words.
//
// This program touches no delay RAM. Every w0 of its 96 steps is zero, so there is no line, no tap
// and no index anywhere in it: the whole of the phaser is two cascades of allpass sections, which is
// what separates it from ins-m00's and ins-m01's. It fills five cells, m[3] and m[7] from the serial
// banks at page 13, m[4] from page 15, m[13] from page 13's LFO and m[42] from the running value, and
// reads only m[13] and m[42] back. Five of the 96 steps move a cell on the way past and nothing else
// does, and none of the 96 is a control step, so the m_skip guard below is the only one a gate in the
// base program can open.
//
// What the 96 steps compute, in the groups the comments open each of. The names are the host's:
// phaser1() and phaser2() name every step they program, through the tables in rom_tables.cpp.
//   0x1a0-0x1b2  the two signals in and the two shelves. r[7] and r[8] are the two the base program
//                leaves the window; r[67] and r[70] take them, r[68] and r[71] are the low shelves'
//                outputs (ins::LPF_PH[0], [1], [2] and ins::LPF_PH_C[0], [1], [2]) and r[69] and
//                r[72] the high shelves' (ins::HPF_PH[0], [1], [2] and ins::HPF_PH_C[0], [1], [2]).
//                0x1aa and 0x1ab take the window's own LFO cell, m[13], into r[122] and r[123], the
//                two sweep registers.
//   0x1b3-0x1c2  the swept coefficient of each cascade. ins::PH_DEPTH[0] and [1], 0x1b3 and 0x1b5,
//                rectify and scale the LFO the register holds, ins::PH_FB[0] and [1], 0x1b4 and
//                0x1b6, add the phase shift and register it, and 0x1c1 and 0x1c2 multiply the register
//                by itself through t[10] and t[11]: the sweep is squared. 0x1b4, 0x1b6, 0x1c1 and
//                0x1c2 each queue t[10] or t[11] three steps ahead, which is how the sections from
//                0x1d0 on come to read them.
//   0x1b7-0x1c0  the ten stage words, ins::PH_STAGES: five taps of each cascade, r[75] to r[79] and
//                r[82] to r[86], summed a half at a time into r[120] and r[121]. phaser1() and
//                phaser2() open one word of each cascade and leave the other four at zero, so each
//                cascade's output is the one stage the Stage parameter names, the 2 and the half
//                cancelling, and walking that parameter walks the output along the cascade.
//   0x1c3-0x1c6  the diffuse taps, ins::PH_DIR: r[81] and r[85] of the second cascade and its
//                selected stage r[121] into r[119], the signal the left output carries. phaser1()'s
//                Diffuse parameter opens one of the three; phaser2() has none, so under it the ROM's
//                words stand and r[119] is r[81], the second cascade's first section.
//   0x1c7-0x1ca  the tail of the sweep. 0x1c7 and 0x1c9 load the offset, and 0x1c8 and 0x1ca take it
//                off the square of 0x1c1 and queue t[10] and t[11] again, so the coefficient the
//                sections run is the fourth power of the phase shift plus depth term less that offset.
//   0x1cb-0x1ce  the cascade on r[74] to r[79]'s input: r[69] and r[72], the shelves' outputs, come
//                in at a quarter and a half, and the other cascade's selected stage r[121] through
//                ins::PH_WIDTH1[0], the one word phaser1() writes. The matching word for the cascade
//                on r[81] to r[86] is 0x1e2, ins::PH_WIDTH2[1], which adds r[121] to the 0x1e3 that
//                takes r[120], and phaser2() writes ins::PH_WIDTH2[0] at 0x1cd as well, so under it
//                each cascade takes the other's output back: that is the FB Level, and the two
//                cascades feed each other by it.
//   0x1cf-0x1e0  the first cascade, six allpass sections r[74] to r[79] on t[10], three steps
//                each: y = t[10] * x - y a step back, r[74] taking r[73] and r[79] taking r[78]. The
//                class-3 step of each group hands the section's output on as the running value.
//   0x1e1-0x1f5  the second cascade, the same six sections r[81] to r[86] on t[11], behind its own
//                input at 0x1e1 to 0x1e3.
//   0x1f6-0x1f9  the dry and wet mix, ins::PH_WET: r[69] and r[72], the shelves' outputs, at
//                ins::PH_WET[0] and [2] against the wet at [1] and [3], into r[17] and r[18], the two
//                outputs, which the base program reads back.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] and r[70]
// the shelves' inputs, r[68], r[69], r[71] and r[72] the two shelves of each channel; r[122] and
// r[123] the two sweep registers, the LFO at 0x1aa and 0x1ab, the phase shift plus depth term at
// 0x1b4 and 0x1b6 and its square at 0x1c1 and 0x1c2, which 0x1c8 and 0x1ca raise to the fourth power;
// r[73] and r[80] the cascades' inputs and r[74] to r[79] and r[81] to r[86] their twelve sections;
// r[120], r[121] and r[119] each cascade's selected stage and the diffuse tap; r[17] and r[18] the
// two outputs.
//
// The old model in vop3_effects.h has a case for each of these two types, and it is a loop of
// `stages` allpasses whose frequency the LFO sweeps through a tan formula: no delay RAM either, but
// no shelves worth the name, no stage crossfade, no cross-cascade feedback and no diffuse taps. It
// is no reference for any number here, and where the two differ, the program above is what runs.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled
// out by name rather than a step at a time.
//
// vop3_native_check ins-m02: identical, 2 types, 19200 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, load and the rest

// ---------------------------------------------------------------------------------------------
// What a step carries besides its arithmetic
// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// The two channels' four shelves, 0x1a3 to 0x1b2
// ---------------------------------------------------------------------------------------------

// One of the four shelves. A shelf is three steps: its b1 word on its own input, its a1 word on its own
// output, and the two closed a half at a time into `out`. The low shelf of a channel has a step in front
// of it that scales the signal the base program leaves the window into `in` with that same x2 route and
// saturates it; the high shelf has no such step, because the low shelf's output is already its input.
struct Shelf
{
	int st;                // the first of the shelf's steps, which is what index_by_span keys on
	int nsteps;            // how many that is: four for a low shelf, which has the step in front, else three
	int in, out;           // the registers its b1, a1 and closing steps read and write
	int feed;              // the register the step in front scales into `in`, zero where there is none
	route_t route;         // the closing step's route: x2 for the low shelves, x4 for the high
	Decor decor;           // what the b1 step carries past: only the left high shelf takes the LFO
};

// ins::LPF_PH and ins::LPF_PH_C are the low shelves' three words and ins::HPF_PH and ins::HPF_PH_C the
// high ones. The left high shelf's b1 step is the one that publishes the window's own LFO cell, and the
// two sweep registers at 0x1aa and 0x1ab read that cell.
constexpr Shelf SHELVES[4] = {
    //   st     n  in  out  feed  route      the b1 step's decor
    { 0x1a3, 4, 67, 68, 7, ROUTE_2, { DEC_NONE, 0  } },
    { 0x1a7, 3, 68, 69, 0, ROUTE_4, { DEC_LFO,  13 } },
    { 0x1ac, 4, 70, 71, 8, ROUTE_2, { DEC_NONE, 0  } },
    { 0x1b0, 3, 71, 72, 0, ROUTE_4, { DEC_NONE, 0  } },
};

// Which shelf a step belongs to, so the step reaches its shelf rather than looking through the four for it.
// The two shelves of a channel are not one run, so the index is by span and each row carries its own length.
// It is the same on every pass and under both types.
constexpr auto SHELVES_AT = index_by_span(SHELVES);

// One shelf's steps at 0x1a3 + k. The low shelves' k runs to 3 and the high ones' to 2, because only a
// low shelf has the step in front.
void shelf(yss236_core& c, int st, Shelf const& s, int k)
{
	if (s.feed)
	{
		if (k == 0)
		{
			gain(c, st, s.feed, s.in, ROUTE_2);    // the channel's signal into the chain, half at a time
			return;
		}
		k--;
	}
	if (k == 0)
	{
		decor(c, st, s.decor);
		term(c, st, s.in);                       // the b1 word on the shelf's own input
	}
	else if (k == 1)
		sum(c, st, s.out);                       // and the a1 word on its own output
	else
		close(c, st, s.in, s.out, s.route);      // the pair closed into the shelf's output
}

// The shelf a step belongs to, or null for the two sweep registers that sit between the channels' pairs.
// The two shelves of a channel are not one run: the low one has the step in front and the high one has
// not, so each covers its own nsteps from its own st.
Shelf const* find_shelf(int st)
{
	int const i = SHELVES_AT[st];
	return i < 0 ? nullptr : &SHELVES[i];
}

// ---------------------------------------------------------------------------------------------
// The two swept coefficients, 0x1b3 to 0x1b6, 0x1c1, 0x1c2 and 0x1c7 to 0x1ca
// ---------------------------------------------------------------------------------------------

// One cascade's swept allpass coefficient, two steps. The depth step rectifies and scales the LFO the
// sweep register holds, and the feedback step the one after it adds the phase shift and saturates the
// sum back into the register, leaving its running value over 128 in the cascade's t register three
// steps ahead of the sections that read it.
struct Sweep { int base, reg, treg; };

constexpr Sweep SWEEPS[2] = {
    //   base  reg  treg
    { 0x1b3, 122, 10 },  // ins::PH_DEPTH[0] then ins::PH_FB[0], for the cascade on t[10]
    { 0x1b5, 123, 11 },  // ins::PH_DEPTH[1] then ins::PH_FB[1], for the cascade on t[11]
};

void sweep(yss236_core& c, int st, Sweep const& s, int k)
{
	if (k == 0)
	{
		// ins::PH_DEPTH: acc = |k * r[reg]|, the LFO rectified and scaled by the depth
		run(c, {.st = st, .sr = s.reg, .clamp = CLAMP_ABS});
	}
	else
	{
		// ins::PH_FB: r[reg] = saturate(128 * k + acc), the phase shift added to the rectified depth
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = s.reg});
		t_put(c, st, s.treg, c.acc);     // and the t register the sections of this cascade read
	}
}

// The two places past 0x1b6 where a sweep's coefficient is squared again. At `square` the register
// multiplies the t register the feedback step left and its own result goes back into the t register,
// which is the square of the phase shift plus depth term. At `offset` the sweep's own offset word is
// loaded as a value of its own, and at the step after it the offset is taken off that square and the
// difference over 128 goes back into the t register, which is the fourth power less the offset.
struct SweepTail { int square, offset, reg, treg; };

constexpr SweepTail SWEEP_TAIL[2] = {
    //  square  offset  reg  treg
    { 0x1c1, 0x1c7, 122, 10 },  // the square at 0x1c1, and the offset at 0x1c7 and 0x1c8
    { 0x1c2, 0x1c9, 123, 11 },  // the square at 0x1c2, and the offset at 0x1c9 and 0x1ca
};

void sweep_square(yss236_core& c, int st, SweepTail const& s)
{
	// r[reg] = saturate(t[treg] * r[reg]), and the same value over 128 back into the t register
	t_put(c, st, s.treg, run(c, {.st = st, .sr = s.reg, .treg = s.treg, .clamp = CLAMP_SAT, .dr = s.reg}));
}

void sweep_offset(yss236_core& c, int st, SweepTail const& s, int k)
{
	if (k == 0) load(c, {.st = st});    // acc = 128 * k, the offset the square is taken down to
	else
	{
		// acc = t[treg] * r[reg] - acc, the offset off the square of the square, over 128 into the t register
		t_put(c, st, s.treg, run(c, {.st = st, .sr = s.reg, .treg = s.treg, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT}));
	}
}

// ---------------------------------------------------------------------------------------------
// The stage taps and the diffuse tap, 0x1b7 to 0x1c6
// ---------------------------------------------------------------------------------------------

// One cascade's five stage words, ins::PH_STAGES: the word on each of five section outputs, the first
// into the running value alone and the rest added to it, the last closing the sum a half at a time into
// `out`. The five registers are consecutive from `first`, which is what the ROM's own words assume.
struct StageTap { int base, first, out; };

constexpr StageTap STAGES[2] = {
    //  base  first  out
    { 0x1b7, 75, 120 },  // the first cascade, r[75] to r[79] into its selected stage r[120]
    { 0x1bc, 82, 121 },  // the second, r[82] to r[86] into r[121]
};

void stage_tap(yss236_core& c, int st, StageTap const& s, int k)
{
	if (k == 0)      term(c, st, s.first);                        // ins::PH_STAGES[0], on its own
	else if (k == 4) close(c, st, s.first + k, s.out, ROUTE_2);    // ins::PH_STAGES[4], closing the sum
	else             sum(c, st, s.first + k);                      // ins::PH_STAGES[1] to [3], on top
}

// The diffuse taps, ins::PH_DIR: the two cascades' selected stages and two of the second cascade's
// sections, summed a word at a time and the last closed a half at a time into r[119], the wet signal
// the left output carries.
struct Diffuse { int base; int reg[4]; int out; };

constexpr Diffuse DIFFUSE[1] = {
    //  base                the four registers        out
    { 0x1c3, { 120, 121, 81, 85 }, 119 },
};

void diffuse(yss236_core& c, int st, Diffuse const& d, int k)
{
	if (k == 0)      term(c, st, d.reg[k]);                       // ins::PH_DIR[0], on its own
	else if (k == 3) close(c, st, d.reg[k], d.out, ROUTE_2);       // ins::PH_DIR[2], closing the sum
	else             sum(c, st, d.reg[k]);                         // ins::PH_DIR[0] and [1], on top
}

// ---------------------------------------------------------------------------------------------
// The two cascades: their inputs at 0x1cb and 0x1e1, and their six sections each
// ---------------------------------------------------------------------------------------------

// One cascade's input: the dry shares off the two channels' shelves and the other cascade's selected
// stage, the last term closing into the register the sections read. The first cascade takes a quarter
// of the left channel's shelves and a half of the right's; the second opens on the right's alone, and
// the two then take each other's selected stage on the way in, which is the FB Level. So the group
// only differs in which shelves it opens on and in what order the two cascades' selected stages come.
struct CascadeIn
{
	int base, n;                 // the group's first step, and how many steps it runs
	int sr[4];                   // the register each of its steps reads
	add_t add[4];                // and what that step's adder takes
	route_t route[4];            // and the route its result takes
	int dr;                      // the register the last step writes, the cascade's input
};

constexpr CascadeIn CASCADE_IN[2] = {
    //  base   n                  the registers           the adders                       the routes            dr
    { 0x1cb, 4, { 69, 72, 120, 121 }, { ADD_OFF, ADD_ACC, ADD_ACC, ADD_ACC }, { ROUTE_1, ROUTE_2, ROUTE_1, ROUTE_1 }, 73 },
    { 0x1e1, 3, { 72, 121, 120 },     { ADD_OFF, ADD_ACC, ADD_ACC },          { ROUTE_2, ROUTE_1, ROUTE_1 },          80 },
};

void cascade_in(yss236_core& c, int st, CascadeIn const& p, int k)
{
	bool const last = k == p.n - 1;
	run(c, {.st = st, .sr = p.sr[k], .add = p.add[k], .route = p.route[k], .clamp = last ? CLAMP_SAT : CLAMP_WRAP, .dr = last ? p.dr : 0});
}

// One cascade of six allpass sections on a swept coefficient, y = t * x - y a step back. The class-3
// step at the front takes the cascade's input into the running value without a word on it (s19), and
// each section is then three steps: the section's own output a step back on the swept coefficient, less
// the running value; the same word on the section's input, which is what the section writes; and the
// section's output handed on as the running value the section behind it works from. The last section
// has no third step, because nothing runs behind it.
struct Cascade
{
	int base;       // the class-3 step that takes the cascade's input
	int input;      // the cascade's input register
	int first;      // the first of its six section registers
	int treg;       // the swept coefficient all six of them multiply by
};

constexpr Cascade CASCADES[2] = {
    //  base  input  first  treg
    { 0x1cf, 73, 74, 10 },  // r[74] takes r[73], and r[79] takes r[78]
    { 0x1e4, 80, 81, 11 },  // r[81] takes r[80], and r[86] takes r[85]
};

void cascade(yss236_core& c, int st, Cascade const& p, int k)
{
	if (k == 0)
	{
		run(c, {.st = st, .sr = p.input, .unity = true});
		return;
	}
	int const j = (k - 1) / 3;      // which of the six sections
	int const m = (k - 1) % 3;      // and which of its three steps
	if (m == 2)
	{
		// the section's output, handed on as the running value the next section works from
		run(c, {.st = st, .sr = p.first + j, .unity = true});
		return;
	}
	// The first of the pair reads the section's own output a step back and leaves the running value
	// wrapping; the second reads the register before it, which is the section's input, and saturates
	// its result into the section's own register.
	int const src = m ? (j ? p.first + j - 1 : p.input) : p.first + j;
	run(c, {.st = st, .sr = src, .treg = p.treg, .add = ADD_ACC, .sub = true, .clamp = m ? CLAMP_SAT : CLAMP_WRAP, .dr = m ? p.first + j : 0});
}

// ---------------------------------------------------------------------------------------------
// The dry and wet mix, 0x1f6 to 0x1f9
// ---------------------------------------------------------------------------------------------

// One channel's output, two steps. ins::PH_WET: the shelves' output as the dry share on its own, and
// the wet share the step after it added on top and quadrupled into the window's output register, which
// the base program reads back.
struct MixPair { int base, dry_reg, wet_reg, out; };

constexpr MixPair MIX[2] = {
    //   base  dry_reg  wet_reg  out
    { 0x1f6, 69, 119, 17 },  // the left output, the wet off the diffuse tap r[119]
    { 0x1f8, 72, 121, 18 },  // the right output, the wet off r[121]
};

void mix(yss236_core& c, int st, MixPair const& p, int k)
{
	if (k == 0) term(c, st, p.dry_reg);                   // ins::PH_WET[0] and [2], the dry share alone
	else        close(c, st, p.wet_reg, p.out, ROUTE_4);  // ins::PH_WET[1] and [3], the wet share on top
}

// ---------------------------------------------------------------------------------------------
// The run of steps 0x1a0 to 0x1ff, the whole of the window's program
// ---------------------------------------------------------------------------------------------

// The loop is one iteration per step and carries the m_skip guard step() opens with, so a gate in the
// base program skips these steps as it would the interpreter's. Each branch below names the group its
// step belongs to and hands it the step's own index into that group, which is all the arithmetic needs:
// every group's table holds the registers and the shape of its steps, and nothing here is a step at a
// time.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// ---- 0x1a0 to 0x1a2, the two serial inputs and the window's own running value ----
		// None of the three writes a register: 0x1a0 loads its own word as a value, 0x1a1 scales m[42]
		// by its word and takes the running value off that, and 0x1a2 loads its word less the running
		// value, negated, and publishes the result in m[42]. 0x1a1's adder names r[0], which fetches
		// nothing, so what the hardware offers it there is the running value over 32768.
		if (st < 0x1a3)
		{
			switch (st)
			{
			case 0x1a0: take_serial(c, 3, st, 0); load(c, {.st = st}); break;
			case 0x1a1: take_serial(c, 7, st, 1);
			            run(c, {.st = st, .sm = 42, .m2 = M2_CELL, .add = ADD_REG, .sub = true});
			            break;
			default:    load(c, {.st = st, .add = ADD_ACC, .sub = true, .dm = 42}); break;
			}
			return;
		}

		// ---- 0x1a3 to 0x1b2, the two channels' shelves, with the two sweep registers between the pairs ----
		if (st < 0x1b3)
		{
			if (Shelf const* s = find_shelf(st))
			{
				shelf(c, st, *s, st - s->st);
				return;
			}
			// 0x1aa and 0x1ab: ins::PH_OFFSET is the word phaser2()'s LFO Phase writes, at the second of
			// the two. A class-1 load wraps its result into the register, and its adder takes the LFO
			// cell m[13] that 0x1a7 queued.
			load(c, {.st = st, .sm = 13, .add = ADD_CELL, .dr = st == 0x1aa ? 122 : 123});
			return;
		}

		// ---- 0x1b3 to 0x1ca, the swept coefficients, the ten stage words, the diffuse taps and the offsets ----
		if (st < 0x1b7)
		{
			Sweep const& s = SWEEPS[(st - SWEEPS[0].base) / 2];
			sweep(c, st, s, st - s.base);
			return;
		}
		if (st < 0x1c1)
		{
			StageTap const& s = STAGES[(st - STAGES[0].base) / 5];
			stage_tap(c, st, s, st - s.base);
			return;
		}
		if (st < 0x1c3) { sweep_square(c, st, SWEEP_TAIL[st - SWEEP_TAIL[0].square]); return; }
		if (st < 0x1c7) { diffuse(c, st, DIFFUSE[0], st - DIFFUSE[0].base);            return; }
		if (st < 0x1cb)
		{
			SweepTail const& s = SWEEP_TAIL[(st - SWEEP_TAIL[0].offset) / 2];
			sweep_offset(c, st, s, st - s.offset);
			return;
		}

		// ---- 0x1cb to 0x1f5, the two cascades: each one's input, then its six allpass sections ----
		if (st < 0x1cf) { CascadeIn const& p = CASCADE_IN[0];         cascade_in(c, st, p, st - p.base); return; }
		if (st < 0x1e1) { cascade(c, st, CASCADES[0], st - CASCADES[0].base); return; }
		if (st < 0x1e4) { CascadeIn const& p = CASCADE_IN[1];         cascade_in(c, st, p, st - p.base); return; }
		if (st < 0x1f6) { cascade(c, st, CASCADES[1], st - CASCADES[1].base); return; }

		// ---- 0x1f6 to 0x1f9, the dry and wet mix into the window's two outputs ----
		if (st < 0x1fa)
		{
			MixPair const& p = MIX[(st - MIX[0].base) / 2];
			mix(c, st, p, st - p.base);
			return;
		}

		// ---- 0x1fa to 0x1ff, the last cell, and five steps that only count ----
		if (st == 0x1fa) take_serial(c, 4, st, 0);
		idle(c);
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m02(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
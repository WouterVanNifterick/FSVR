// fs1r/native/flt.cpp - VOP3-1, the per-voice filter chip: its program, all 512 steps.
//
// The chip takes sixteen channels in and gives the same sixteen back filtered, and the program is a
// round robin over them: the work for one channel is spread through the pass, and the steps that do
// it are interleaved with the other three channels it shares a page with.  What is fixed about the
// schedule is the page: 124 steps at 16 + 124 * g, the formula FilterHost::page() gives, hold the
// four channels 4g to 4g + 3, which are slots (4g + 12) to (4g + 15) modulo 16.  Each channel, or
// lane, of a page owns six registers, r[6*((s + 4) & 15) .. +5] and the ramp one past them, which
// for lane i of page g is r[24 * g + 6 * i .. +6]:
//
//   r[base + 1]  the coefficient the lane read a step ago, kept by the z^-1 copy steps
//   r[base + 2]  the lane's two pole states and the BPF tap
//   r[base + 3]  the LPF12 tap
//   r[base + 4]  the LPF18 tap
//   r[base + 5]  the LPF24 tap
//   r[base + 6]  the output ramp, which walks to 128 while the slot sounds and back down when muted
//
// and the eight registers r[98], r[99], r[101], r[102], r[104], r[105], r[107] and r[108] are the four
// lanes' pole states, one pair each.  r[97], r[98] and r[99] are the lanes' cutoff words, r[127] is
// the EG level's full scale, and the t file holds the pole gains.  The pages come round again in the
// next pass, so every one of these is a state the chip carries from sample to sample.
//
// The four pages are the same schedule.  Each is 124 steps in the same seventeen blocks, four lanes
// apiece wherever a block is a lane's own work, and a block's arithmetic is the same in all four
// pages: what differs is the registers and the cells the ROM's tables give each page, and where the
// hardware side's serial inputs, its LFO and its serial output land.  So the code below is one page,
// run four times, and each page's numbers are four rows of a table: the cells its lane's values
// travel through, and the decor of every step that takes a value from the hardware side on its way
// in.  The blocks are, in step order within a page:
//
//   rel 0x00-0x0b  the lane's EG level and its depth off r[127], the LFO2 depth on top of it by the
//                  route's two, and the cutoff word off the sum, four lanes in turn, three steps each
//   rel 0x0c-0x13  the pole coefficients: the cutoff words onto m[62] by the route's two, then the
//                  first pole gains onto m[61].  The same eight steps in all four pages.
//   rel 0x14-0x1b  the pole gains of the page's own four lanes, which is where the registers the
//                  lanes' pole states live in are updated and t[1] to t[4] follow.
//   rel 0x1c-0x23  the resonance stage: four pole states less m[63], and four more m[63] added.
//   rel 0x24-0x27  the pole states into the cells the LPF12 taps and the sweep read them off.
//   rel 0x28-0x2f  the LPF12 coefficients onto r[104] and r[105], and their gains into t[1] and t[2].
//   rel 0x30-0x33  the LPF12 taps: a pole state scaled by t[1] or t[2] onto its cutoff word.
//   rel 0x34-0x43  the EG to cutoff sweep, four lanes of four steps: the lane's second EG reading,
//                  its LFO2 depth by the route's four into the lane's cutoff register, and the two
//                  unity steps that load the ramp's coefficient into t[8] to t[11].
//   rel 0x44-0x53  the LPF12 stage, four lanes of four steps: the pole state the sweep left, the
//                  coefficient the lane read a step ago kept as its z^-1, the pole state itself, and
//                  the LPF18 section's input off the lane's LPF18 tap.
//   rel 0x54-0x5f  the LPF18 stage: the lane's LPF12 tap out and the LFO2 depth by four, four lanes
//                  of two steps, then the LPF18 taps, one a lane.
//   rel 0x60-0x63  the cutoff words onto the tap gains t[1] to t[4].
//   rel 0x64-0x73  the output ramp, eight pairs: the lane's LPF18 tap and its LPF24 tap each walked
//                  back up its own register by the lane's own gain, t[1] to t[4], of which the two
//                  slope words below leave t[1] and t[2] and the pole gains above t[3] and t[4].
//   rel 0x74-0x77  the ramp's slope words, one a lane, onto the lane's own ramp register.
//   rel 0x78-0x7b  the filter's own output, four steps at the end of a page, sixteen over the pass.
//
// The prologue at 0x000 to 0x00f is outside the round robin: it takes the serial inputs and the EG
// level into cells, sends m[25] to the serial output, opens the gate at 0x004 at 0x00b (the block at
// 0x005 to 0x00a is the host's to write, because core::trigger() writes those registers for it),
// loads r[127] with the EG level's full scale, and loads m[61], m[62] and m[63] as the constants the
// pole coefficients and the resonance stage are built on.
//
// The program words are the firmware's own and are folded into the arithmetic step by step.  Three
// things are not program words and are read where they live: every coefficient comes from
// c.coef[st] each pass, the sixteen tap steps at B + 120 + k read the register and take the route
// FilterHost::setType() writes into them, and the gated block at 0x005 to 0x00a never runs, because
// the gate above it only opens for the host's trigger, which writes those registers itself.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled
// out by name rather than a step at a time.  docs/filter.md is what the chip computes, and
// docs/findings.md the ladder and the state variable filter under it.
//
// vop3_native_check flt: identical, 6 types, 57600 native passes.
#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step carries besides its arithmetic
// ---------------------------------------------------------------------------------------------

// A step of a page usually also moves a cell in from the hardware side or sends one out to the serial
// output.  `cell` is the cell it moves into or out of and the step's own page decides the serial
// bank, the LFO and the output slot, exactly as step() takes them: in[0] is the voice's own serial
// input, in[1] the second bank, and in[3] the EG level.
enum decor_t { DEC_NONE, DEC_SERIAL_L, DEC_SERIAL_R, DEC_EG, DEC_LFO, DEC_OUT };

struct Decor { decor_t kind; int cell; };

// Apply the decor, which comes before the term, because step() moves a cell before it adds.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_SERIAL_L:  take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R:  take_serial(c, d.cell, st, 1); break;
		case DEC_EG:        take_eg(c, d.cell, st); break;
		case DEC_LFO:       take_lfo(c, d.cell, st); break;
		case DEC_OUT:       serial_out(c, st, d.cell); break;
		case DEC_NONE: break;
	}
}

// One step's program words, folded into the arithmetic, in step::alu's own field order, save for the
// step itself, so that a row reads as the expression it computes: the two operands, the multiplier's
// word, the adder, the route and the clamp, and where the result goes.  The step is not in the row,
// because a page runs in step order and the loop owns the number.  `tw` is yss236_core::t_write()'s
// own field, 0 for no t write, 1 for the step's own word and 2 for its result, and `loadk` the class-1
// step, which loads its word as a value instead of scaling one.
struct Row
{
	int sr = 0;                 // the register the step reads (w4[13:7])
	int sm = 0;                 // the cell the step latches (w4[5:0])
	int treg = 0;               // the multiplier's word is t[treg], 0 for the step's own c.coef[st]
	m2_t m2 = M2_REG;           // what the multiplier scales
	bool unity = false;         // class 3: the word is one and the multiplier passes m2 through
	add_t add = ADD_OFF;        // what the adder adds to the product
	bool sub = false;           // and whether it is negated
	route_t route = ROUTE_1;    // the result's route
	clamp_t clamp = CLAMP_WRAP; // and its clamp
	int dr = 0;                 // the register the result is written to
	bool z1 = false;            // and whether it takes the step before's x operand, the z^-1 copy
	int dm = 0;                 // the cell the result is written to
	int tn = 0;                 // the t register the result or the step's own word goes to
	int tw = 0;                 // which of the two, and 0 for no t write at all
	bool loadk = false;         // class 1: the step's word is loaded as a value rather than scaling one
};

// One row of a table, at step st, with the decor it carries on the way past.
void row_step(yss236_core& c, int st, Row const& r, Decor const& d)
{
	decor(c, st, d);
	if (r.loadk)
	{
		load(c, st, r.sr, r.sm, r.add, r.sub, r.route, r.clamp, r.dr, r.dm);
		// t_write() reads the loaded value, which is the running value after the clamp and not the
		// one load() hands back
		if (r.tw & 2) t_put(c, r.tn, c.acc);
		else if (r.tw & 1) t_put_word(c, st, r.tn);
		return;
	}
	double const y = run(c, {st, r.sr, r.sm, r.m2, r.unity, r.treg, r.add, r.sub, r.route, r.clamp,
	                         r.dr, r.z1, r.dm});
	if (r.tw & 2) t_put(c, r.tn, y);
	else if (r.tw & 1) t_put_word(c, st, r.tn);
}

// ---------------------------------------------------------------------------------------------
// The schedule, the same in all four pages
// ---------------------------------------------------------------------------------------------

// A page's first step, 16 + 124 * g, and where its four lanes sit: lane i owns r[24 * g + 6 * i .. +6].
const int PAGE_BASE[4] = { 0x010, 0x08c, 0x108, 0x184 };
inline int lane_base(int g, int i) { return 24 * g + 6 * i; }

// The registers the schedule reads and writes the same way in every page, and the t registers it
// scales by.  CUTOFF is where the four lanes' cutoff words land in the opening twelve steps;
// SWEEP_OUT what the EG to cutoff sweep leaves a lane in and what the LPF12 stage reads it back out
// of; SWEEP_IN what the sweep's third step reads instead, one register back for the last lane;
// LPF12_OUT and LPF12_OUT2 the pole states the LPF12 stage leaves; and the t arrays the gains each
// block scales by, LPF12_TREG the t[2 * i + 2] the sweep's last step leaves.
const int CUTOFF[4] = {  97,  98,  99,  97 };
const int SWEEP_OUT[4] = {  98,  99, 101, 104 };
const int SWEEP_IN[4] = {  98,  99, 101, 102 };
const int LPF12_OUT[4] = {  98, 101, 104, 107 };
const int LPF12_OUT2[4] = {  99, 102, 105, 108 };
const int LPF12_TREG[4] = {   2,   4,   6,   8 };
const int LPF18_TREG[4] = {   1,   3,   5,   7 };
const int RAMP_TREG[4] = {   1,   2,   3,   4 };
const int SLOPE_TREG[4] = {   1,   2,   1,   2 };
const int OUT_TREG[4] = {  1,  2,  1,  2 };

// ---- rel 0x0c to 0x13: the pole coefficients, the cutoff words onto m[62] by the route's two and
//      then the first pole gains onto m[61].  The same eight steps in all four pages. ----
const Row POLE_COEFS[8] = {
	{  97, 62,  0, M2_REG  , false, ADD_CELL, false, ROUTE_2, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  98, 62,  0, M2_REG  , false, ADD_CELL, false, ROUTE_2, CLAMP_ZERO,  97, false,  0,  2, 2, false },
	{  99, 62,  0, M2_REG  , false, ADD_CELL, false, ROUTE_2, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  97, 62,  0, M2_REG  , false, ADD_CELL, false, ROUTE_2, CLAMP_ZERO,  97, false,  0,  2, 2, false },
	{  98, 61,  1, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  97, 61,  2, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_ZERO,  97, false,  0,  2, 2, false },
	{  98, 61,  1, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  97, 61,  2, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_ZERO,  97, false,  0,  2, 2, false }
};

// ---- rel 0x1c to 0x23: the resonance stage, four pole states less m[63] and four more it added ----
const Row RESONANCE[8] = {
	{  97, 63,  0, M2_REG  , false, ADD_CELL, true , ROUTE_1, CLAMP_SAT ,  98, false,  0,  0, 0, false },
	{ 100, 63,  0, M2_REG  , false, ADD_CELL, true , ROUTE_1, CLAMP_SAT ,  99, false,  0,  0, 0, false },
	{ 103, 63,  0, M2_REG  , false, ADD_CELL, true , ROUTE_1, CLAMP_SAT ,  98, false,  0,  0, 0, false },
	{ 106, 63,  0, M2_REG  , false, ADD_CELL, true , ROUTE_1, CLAMP_SAT ,  99, false,  0,  0, 0, false },
	{  98, 63,  0, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_SAT ,  98, false,  0,  0, 0, false },
	{  99, 63,  0, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_SAT ,  99, false,  0,  0, 0, false },
	{  98, 63,  0, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_SAT , 101, false,  0,  0, 0, false },
	{  99, 63,  0, M2_REG  , false, ADD_CELL, false, ROUTE_1, CLAMP_SAT , 102, false,  0,  0, 0, false }
};

// ---- rel 0x28 to 0x2f: the LPF12 coefficients onto r[104] and r[105], then their gains into t[1]
//      and t[2].  The same eight steps in all four pages. ----
const Row LPF12_COEFS[8] = {
	{  98,  0,  0, M2_REG  , false, ADD_REG , true , ROUTE_2, CLAMP_SAT , 104, false,  0,  0, 0, true  },
	{  99,  0,  0, M2_REG  , false, ADD_REG , true , ROUTE_2, CLAMP_SAT , 105, false,  0,  0, 0, true  },
	{ 101,  0,  0, M2_REG  , false, ADD_REG , true , ROUTE_2, CLAMP_SAT , 104, false,  0,  0, 0, true  },
	{ 102,  0,  0, M2_REG  , false, ADD_REG , true , ROUTE_2, CLAMP_SAT , 105, false,  0,  0, 0, true  },
	{ 104,  0,  0, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  1, 2, false },
	{ 105,  0,  0, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  2, 2, false },
	{ 104,  0,  0, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  1, 2, false },
	{ 105,  0,  0, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  2, 2, false }
};

// ---- rel 0x14 to 0x1b: the pole gains of the page's own four lanes.  The arithmetic is the same in
//      all four pages and so is every register but one: only the cell a step latches is page's own,
//      which GAIN_CELL below holds. ----
const Row PAGE_GAINS[8] = {
	{  98,  0,  1, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  97,  0,  2, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO,  97, false,  0,  2, 2, false },
	{  98,  0,  1, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO,  98, false,  0,  1, 2, false },
	{  97,  0,  2, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO,  99, false,  0,  2, 2, false },
	{  98,  0,  1, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO,  97, false,  0,  1, 2, false },
	{  97,  0,  2, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO, 100, false,  0,  2, 2, false },
	{  98,  0,  1, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO, 103, false,  0,  3, 2, false },
	{  99,  0,  2, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_ZERO, 106, false,  0,  4, 2, false }
};

// ---- rel 0x24 to 0x27: the pole states into the cells the LPF12 taps and the sweep read them off ----
const Row POLE_STATES[4] = {
	{  97,  0,  1, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  0, 0, false },
	{ 100,  0,  2, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  0, 0, false },
	{ 103,  0,  3, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  0, 0, false },
	{ 106,  0,  4, M2_REG  , false, ADD_OFF , false, ROUTE_1, CLAMP_SAT ,   0, false,  0,  0, 0, false }
};

// ---- rel 0x30 to 0x33: the LPF12 taps, a pole state scaled by t[1] or t[2] onto its cutoff word ----
const Row LPF12_TAPS[4] = {
	{  97,  0,  1, M2_CELL , false, ADD_REG , false, ROUTE_1, CLAMP_ZERO,   0, false,  0,  1, 2, false },
	{ 100,  0,  2, M2_CELL , false, ADD_REG , false, ROUTE_1, CLAMP_ZERO,   0, false,  0,  3, 2, false },
	{ 103,  0,  1, M2_CELL , false, ADD_REG , false, ROUTE_1, CLAMP_ZERO,   0, false,  0,  5, 2, false },
	{ 106,  0,  2, M2_CELL , false, ADD_REG , false, ROUTE_1, CLAMP_ZERO,   0, false,  0,  7, 2, false }
};

// ---- rel 0x60 to 0x63: the cutoff words onto the tap gains t[1] to t[4], by the route's four ----
const Row CUTOFF_GAINS[4] = {
	{  97,  0,  0, M2_CELL , false, ADD_REG , false, ROUTE_4, CLAMP_ZERO,   0, false,  0,  1, 2, false },
	{ 100,  0,  0, M2_CELL , false, ADD_REG , false, ROUTE_4, CLAMP_ZERO,   0, false,  0,  2, 2, false },
	{ 103,  0,  0, M2_CELL , false, ADD_REG , false, ROUTE_4, CLAMP_ZERO,   0, false,  0,  3, 2, false },
	{ 106,  0,  0, M2_CELL , false, ADD_REG , false, ROUTE_4, CLAMP_ZERO,   0, false,  0,  4, 2, false }
};

// ---------------------------------------------------------------------------------------------
// What each page brings with it: the cells its lane's values travel through, and its decor
// ---------------------------------------------------------------------------------------------

// The cells the four lanes' EG levels and LFO2 depths come in on in the opening twelve steps.  These
// come from the ROM's tables and follow no formula: page 0's four lanes take the EG level from
// m[17], m[20], m[23] and m[26] and the LFO2 from m[1] to m[4], page 1's from m[19], m[21], m[25]
// and m[30] and m[5] to m[8], and page 3's from m[28], m[18], m[17] and m[36] and m[13] to m[16].
const int EG[4][4] = {
	{ 17, 20, 23, 26 },
	{ 19, 21, 25, 30 },
	{ 22, 24, 29, 34 },
	{ 28, 18, 17, 36 } };
const int LFO2[4][4] = {
	{  1,  2,  3,  4 },
	{  5,  6,  7,  8 },
	{  9, 10, 11, 12 },
	{ 13, 14, 15, 16 } };

// SWEEP_EG and SWEEP_EG2 are the two cells the EG to cutoff sweep reads per lane and SWEEP_TAP the
// cell its last two steps share.  LPF12_TAP is what the LPF12 stage's first step latches and
// LPF12_OUT_CELL what its last step writes.  LPF18_CELL is the LPF18 stage's input cell and
// LPF18_TAP_CELL the one its tap step reads and writes.  OUT_CELL is where the filter's own output
// goes, one cell a lane.
const int SWEEP_EG[4][4] = {
	{ 18, 21, 24, 17 },
	{ 23, 22, 28, 21 },
	{ 27, 25, 32, 24 },
	{ 31, 19, 20, 17 } };
const int SWEEP_EG2[4][4] = {
	{ 19, 22, 25, 20 },
	{ 27, 24, 29, 25 },
	{ 31, 28, 33, 18 },
	{ 35, 24, 18, 32 } };
const int SWEEP_TAP[4][4] = {
	{ 27, 41, 18, 42 },
	{ 31, 40, 23, 41 },
	{ 35, 40, 27, 41 },
	{ 21, 40, 31, 35 } };
const int LPF12_TAP[4][4] = {
	{ 28,  0,  0,  0 },
	{ 33,  0,  0,  0 },
	{ 19,  0,  0,  0 },
	{ 22,  0,  0,  0 } };
const int LPF12_OUT_CELL[4][4] = {
	{ 17, 18, 20, 17 },
	{ 21, 23, 24, 21 },
	{ 18, 19, 24, 18 },
	{ 20, 21, 22, 20 } };
const int LPF18_CELL[4][4] = {
	{ 17, 18, 20, 17 },
	{ 21, 23, 24, 21 },
	{ 18, 19, 24, 18 },
	{ 20, 21, 22, 20 } };
const int LPF18_TAP_CELL[4][4] = {
	{ 18, 20, 17, 18 },
	{ 23, 29, 21, 23 },
	{ 25, 27, 18, 25 },
	{ 21, 22, 20, 22 } };
const int OUT_CELL[4][4] = {
	{ 17, 18, 19, 20 },
	{ 17, 21, 22, 23 },
	{ 25, 27, 28, 29 },
	{ 31, 32, 33, 35 } };

// The cells the shared blocks above hold per page.  STATE_CELL is where a pole state lands, and both
// the LPF12 taps and the cutoff gains read it; TAP_CELL is where the LPF12 tap lands.  STATE_SRC is
// the cell the pole state's own step latches, GAIN_CELL the one the page's pole gains latch, LPF18_SRC
// the one the LPF18 stage's first step latches, SLOPE_SRC the one the ramp's slope word latches and
// OUT_SRC the one the output tap latches.  RAMP_EVEN_SRC and RAMP_ODD_SRC are the cells the ramp's
// sixteen steps latch, eight each.  Every one of them is m[0] but for the entries below, which are
// the cells the page's voice registers arrive in.
const int STATE_CELL[4][4] = {
	{ 37, 38, 39, 40 },
	{ 32, 37, 38, 39 },
	{ 29, 37, 38, 39 },
	{ 33, 37, 38, 39 } };
const int STATE_SRC[4][4] = {
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{ 21,  0,  0,  0 } };
const int TAP_CELL[4][4] = {
	{ 27, 41, 18, 42 },
	{ 31, 40, 23, 41 },
	{ 35, 40, 27, 41 },
	{ 21, 40, 31, 35 } };
const int OUT_SRC[4][4] = {
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{ 17,  0,  0,  0 },
	{  0,  0,  0,  0 } };
const int LPF18_SRC[4][4] = {
	{ 29,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 } };
const int SLOPE_SRC[4][4] = {
	{  0,  0,  0,  0 },
	{  0,  0,  0, 17 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 } };
const int RAMP_EVEN_SRC[4][8] = {
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 35,  0,  0,  0,  0,  0,  0,  0 },
	{ 20,  0,  0,  0,  0,  0,  0,  0 },
	{ 23,  0,  0,  0,  0,  0,  0,  0 } };
const int RAMP_ODD_SRC[4][8] = {
	{  0,  0,  0,  0,  0,  0,  0, 31 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0 } };
const int GAIN_CELL[4][8] = {
	{  0,  0,  0,  0, 27,  0,  0,  0 },
	{  0,  0,  0, 32,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0, 18 },
	{  0,  0,  0,  0,  0,  0,  0,  0 } };

// The decor of every step that takes a value from the hardware side on its way in or sends one out,
// one row per block and one page each.  Where the serial inputs land is the hardware side's schedule
// and no formula either: page 0 takes all four voices' serial inputs in the block that opens the
// page's own pole gains, page 3 in the block that opens the EG to cutoff sweep, and the others split
// them between the two.
const Decor OPEN_DEC[4][12] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 11}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 20}, {DEC_SERIAL_R, 18}, {DEC_EG, 17}, {DEC_LFO, 15},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor STATE_DEC[4][4] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_OUT, 21}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor GAIN_DEC[4][8] = {
	{ {DEC_SERIAL_L, 17}, {DEC_SERIAL_R, 20}, {DEC_EG, 26}, {DEC_LFO, 4},
		{DEC_OUT, 27}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 21}, {DEC_SERIAL_R, 25}, {DEC_EG, 30}, {DEC_OUT, 32},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 8}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 24}, {DEC_SERIAL_R, 18}, {DEC_EG, 34}, {DEC_OUT, 18} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor RES_DEC[4][8] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 12},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 17}, {DEC_SERIAL_R, 32}, {DEC_EG, 36}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 16}, {DEC_NONE, 0} }
};

const Decor SWEEP_DEC[4][16] = {
	{ {DEC_SERIAL_L, 23}, {DEC_SERIAL_R, 27}, {DEC_EG, 19}, {DEC_LFO, 5},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 27}, {DEC_SERIAL_R, 31}, {DEC_EG, 22}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 9}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 31}, {DEC_SERIAL_R, 35}, {DEC_EG, 28}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 13},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 18}, {DEC_SERIAL_R, 19}, {DEC_EG, 17}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 1}, {DEC_NONE, 0} }
};

const Decor LPF12_DEC[4][16] = {
	{ {DEC_OUT, 28}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 22}, {DEC_SERIAL_R, 24}, {DEC_EG, 21}, {DEC_NONE, 0} },
	{ {DEC_OUT, 33}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_OUT, 19}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_OUT, 22}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor LPF18_DEC[4][8] = {
	{ {DEC_OUT, 29}, {DEC_NONE, 0}, {DEC_LFO, 6}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 25}, {DEC_NONE, 0}, {DEC_SERIAL_R, 28}, {DEC_NONE, 0},
		{DEC_EG, 24}, {DEC_NONE, 0}, {DEC_LFO, 10}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 19}, {DEC_NONE, 0}, {DEC_SERIAL_R, 24}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor LPF18_TAP_DEC[4][4] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_EG, 18}, {DEC_NONE, 0}, {DEC_LFO, 14}, {DEC_NONE, 0} },
	{ {DEC_SERIAL_L, 21}, {DEC_SERIAL_R, 22}, {DEC_EG, 20}, {DEC_NONE, 0} }
};

const Decor CGAIN_DEC[4][4] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 2}, {DEC_NONE, 0} }
};

const Decor RAMP_DEC[4][16] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_SERIAL_L, 28}, {DEC_SERIAL_R, 29}, {DEC_EG, 25}, {DEC_OUT, 31} },
	{ {DEC_OUT, 35}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_OUT, 20}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_OUT, 23}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0},
		{DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

const Decor SLOPE_DEC[4][4] = {
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_LFO, 7} },
	{ {DEC_SERIAL_L, 32}, {DEC_SERIAL_R, 33}, {DEC_EG, 29}, {DEC_OUT, 17} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} },
	{ {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} }
};

// ---------------------------------------------------------------------------------------------
// The seventeen blocks of a page
// ---------------------------------------------------------------------------------------------

// ---- rel 0x00 to 0x0b: the lane's EG level and depth, its LFO2 depth on top of it, and its cutoff
//      word.  Three steps a lane and four lanes in turn, so the running value is a lane's own
//      throughout and each lane's cutoff word is complete before the next lane's is started. ----
void lane_open(yss236_core& c, int g, int st, int rel)
{
	int const i = rel / 3;
	Row r{};
	switch (rel % 3)
	{
	case 0:    // the EG level scaled by its depth onto r[127], the level's own full scale
		r = { 127, EG[g][i], 0, M2_CELL, false, ADD_REG };
		break;
	case 1:    // and the LFO2 depth on top of that, by the route's two
		r = { 0, LFO2[g][i], 0, M2_CELL, false, ADD_ACC, false, ROUTE_2 };
		break;
	default:   // and the cutoff word off the sum, into r[97], r[98], r[99] and r[97] again
		r = { 0, 0, 0, M2_REG, false, ADD_ACC, false, ROUTE_1, CLAMP_ZERO, CUTOFF[i], false, 0, 0, 0, true };
		break;
	}
	row_step(c, st, r, OPEN_DEC[g][rel]);
}

// ---- rel 0x0c to 0x13: the pole coefficients ----
void pole_coeffs(yss236_core& c, int g, int st, int rel)
{
	row_step(c, st, POLE_COEFS[rel - 0x0c], {DEC_NONE, 0});
}

// ---- rel 0x14 to 0x1b: the pole gains of the page's own four lanes ----
void page_pole_gains(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x14;
	Row r = PAGE_GAINS[k];
	r.sm = GAIN_CELL[g][k];     // the cell this step latches, m[0] in every page but one step
	row_step(c, st, r, GAIN_DEC[g][k]);
}

// ---- rel 0x1c to 0x23: the resonance stage ----
void resonance(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x1c;
	row_step(c, st, RESONANCE[k], RES_DEC[g][k]);
}

// ---- rel 0x24 to 0x27: the pole states into the cells the LPF12 taps and the sweep read ----
void pole_states(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x24;
	Row r = POLE_STATES[i];
	r.sm = STATE_SRC[g][i];
	r.dm = STATE_CELL[g][i];
	row_step(c, st, r, STATE_DEC[g][i]);
}

// ---- rel 0x28 to 0x2f: the LPF12 coefficients and their gains ----
void lpf12_coeffs(yss236_core& c, int g, int st, int rel)
{
	row_step(c, st, LPF12_COEFS[rel - 0x28], {DEC_NONE, 0});
}

// ---- rel 0x30 to 0x33: the LPF12 taps, a pole state scaled onto its cutoff word ----
void lpf12_taps(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x30;
	Row r = LPF12_TAPS[i];
	r.sm = STATE_CELL[g][i];
	r.dm = TAP_CELL[g][i];
	row_step(c, st, r, {DEC_NONE, 0});
}

// ---- rel 0x34 to 0x43: the EG to cutoff sweep, four steps a lane.  The first two read the lane's EG
//      again and leave the cutoff in SWEEP_OUT[i]; the last two are the unity steps, the multiplier
//      being one and the adder the cell, which is also how a step's own word rather than its result
//      gets into the t file. ----
void eg_sweep(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x34, i = k / 4;
	Row r{};
	switch (k % 4)
	{
	case 0:
		r = { 0, SWEEP_EG[g][i], 0, M2_CELL, false, ADD_OFF };
		break;
	case 1:
		r = { 0, SWEEP_EG2[g][i], 0, M2_CELL, false, ADD_ACC, false, ROUTE_4, CLAMP_SAT, SWEEP_OUT[i] };
		break;
	case 2:    // the pole state less the tap, which is what loads the ramp's coefficient into t[8 + i]
		r = { SWEEP_IN[i], SWEEP_TAP[g][i], 0, M2_REG, true, ADD_CELL, true, ROUTE_1, CLAMP_SAT,
		      0, false, 0, 8 + i, 1, false };
		break;
	default:   // and the same cell added to the running value, into t[2 * i + 2]
		r = { 0, SWEEP_TAP[g][i], 0, M2_CELL, true, ADD_ACC, false, ROUTE_1, CLAMP_SAT,
		      0, false, 0, 2 * i + 2, 2, false };
		break;
	}
	row_step(c, st, r, SWEEP_DEC[g][k]);
}

// ---- rel 0x44 to 0x53: the LPF12 stage, four steps a lane.  The second step is the z^-1 copy that
//      keeps r[base + 1], the coefficient the lane read a step ago, and the fourth closes the lane's
//      LPF18 input into its second pole state and the cell the tap step below reads. ----
void lpf12_stage(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x44, i = k / 4, b = lane_base(g, i);
	Row r{};
	switch (k % 4)
	{
	case 0:    // the pole state the sweep left, scaled by the gain the sweep put in t[8 + i]
		r = { SWEEP_OUT[i], LPF12_TAP[g][i], 8 + i, M2_REG, false, ADD_REG, true };
		break;
	case 1:    // and the same gain on the coefficient the lane read a step ago, kept as its z^-1
		r = { b + 1, 0, 8 + i, M2_REG, false, ADD_ACC, true, ROUTE_1, CLAMP_WRAP, b + 1, true };
		break;
	case 2:    // and the lane's own pole state, by the gain t[LPF12_TREG[i]]
		r = { b + 2, 0, LPF12_TREG[i], M2_REG, false, ADD_ACC, false, ROUTE_1, CLAMP_SAT, LPF12_OUT[i] };
		break;
	default:   // and the lane's LPF18 input
		r = { b + 3, 0, 0, M2_REG, false, ADD_ACC, false, ROUTE_1, CLAMP_SAT, LPF12_OUT2[i], false,
		      LPF12_OUT_CELL[g][i] };
		break;
	}
	row_step(c, st, r, LPF12_DEC[g][k]);
}

// ---- rel 0x54 to 0x5b: the LPF18 stage's input, two steps a lane.  The first is the lane's LPF12 tap
//      on its own and the second the LFO2 depth by the route's four on top of it, which is what
//      leaves the LPF18 section's input in r[base + 2]. ----
void lpf18_input(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x54, i = k / 2, b = lane_base(g, i);
	Row r{};
	if (k % 2)
		r = { 0, LPF18_CELL[g][i], LPF18_TREG[i], M2_CELL, false, ADD_ACC, false, ROUTE_4, CLAMP_SAT,
		      b + 2, false, LPF18_TAP_CELL[g][i] };
	else
		r = { b + 2, LPF18_SRC[g][i], 0, M2_REG, false, ADD_OFF };
	row_step(c, st, r, LPF18_DEC[g][k]);
}

// ---- rel 0x5c to 0x5f: the LPF18 taps, one a lane, the tap cell scaled by t[1], t[3], t[5] or t[7]
//      back onto the lane's own LPF18 register ----
void lpf18_taps(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x5c, b = lane_base(g, i);
	row_step(c, st,
	         { b + 3, LPF18_TAP_CELL[g][i], LPF18_TREG[i], M2_CELL, false, ADD_REG, false, ROUTE_1,
	           CLAMP_SAT, b + 3 },
	         LPF18_TAP_DEC[g][i]);
}

// ---- rel 0x60 to 0x63: the cutoff words onto the tap gains t[1] to t[4] ----
void cutoff_gains(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x60;
	Row r = CUTOFF_GAINS[i];
	r.sm = STATE_CELL[g][i];
	row_step(c, st, r, CGAIN_DEC[g][i]);
}

// ---- rel 0x64 to 0x73: the output ramp, eight pairs of two steps.  Pair q is lane q % 4 and walks
//      r[base + 4 + q / 4]: the first step of a pair scales that register by the lane's own gain less
//      itself, and the second scales r[base + 3 + q / 4] by the same gain and takes the running value
//      off it, which is the pair the header's ramp walks with. ----
void output_ramp(yss236_core& c, int g, int st, int rel)
{
	int const k = rel - 0x64, q = k / 2, i = q % 4, reg = lane_base(g, i) + 4 + q / 4;
	Row r{};
	if (k % 2)
		r = { reg - 1, RAMP_ODD_SRC[g][q], RAMP_TREG[i], M2_REG, false, ADD_ACC, true, ROUTE_1,
		      CLAMP_SAT, reg };
	else
		r = { reg, RAMP_EVEN_SRC[g][q], RAMP_TREG[i], M2_REG, false, ADD_REG, true };
	row_step(c, st, r, RAMP_DEC[g][k]);
}

// ---- rel 0x74 to 0x77: the ramp's slope words, one a lane, onto the lane's own ramp register ----
void ramp_slope(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x74, reg = lane_base(g, i) + 6;
	decor(c, st, SLOPE_DEC[g][i]);
	load(c, st, reg, SLOPE_SRC[g][i], ADD_REG, false, ROUTE_1, CLAMP_ZERO, reg, 0);
	t_put(c, SLOPE_TREG[i], c.acc);
}

// ---- rel 0x78 to 0x7b: the filter's own output, four steps a page.  The register the step reads
//      and the route its result takes are the type's own, FilterHost::setType() writes them into the
//      step's words, so both come out of c.ops[st] on the pass and this one block is spelled out
//      rather than run through step::run, whose operands a table row would have to fix.  OUT_SRC is
//      the cell the step also latches, and sends to the serial output when it is not m[0]. ----
void output_taps(yss236_core& c, int g, int st, int rel)
{
	int const i = rel - 0x78, sm = OUT_SRC[g][i];
	if (sm) serial_out(c, st, sm);
	int const sr = c.ops[st].sr;
	latch_reg(c, sr);
	latch_cell(c, sm);
	double const y = (c.t[OUT_TREG[i]] * c.r[sr]) * yss236_core::RSCALE[c.ops[st].shift];
	c.acc = yss236_core::accmode(y, CLAMP_SAT);
	c.queue_d(OUT_CELL[g][i], yss236_core::clip(y, yss236_core::DSAT));
}

// ---------------------------------------------------------------------------------------------
// The prologue and the four pages
// ---------------------------------------------------------------------------------------------

// The sixteen steps before the round robin: the serial inputs and the EG level into cells, m[25] out
// to the serial output, the gate, and the three constants the pole coefficients and the resonance
// stage are built on, m[61], m[62] and m[63], with r[127] the EG level's own full scale.  The six
// steps the gate holds, 0x005 to 0x00a, are the host's: core::trigger() writes their registers
// itself, so there is nothing there but begin_step() to do.
void prologue(yss236_core& c)
{
	for (int st = 0x000; st <= 0x00f; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;
		switch (st)
		{
		case 0x000: take_serial(c, 24, st, 0); idle(c); break;   // channel 0 into m[24]
		case 0x001: take_serial(c, 25, st, 1); idle(c); break;   // and the second bank into m[25]
		case 0x002: take_eg(c, 23, st); idle(c); break;          // and the EG level into m[23]
		case 0x003: serial_out(c, st, 25); idle(c); break;       // channel 0 out of m[25]
		case 0x004: gate(c, st, 0x00b); break;                   // and the gate onto the block below
		case 0x00b: load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, 63); break;
		case 0x00c: load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 127, 0); break;
		case 0x00d: load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, 62); break;
		case 0x00e: load(c, st, 0, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0, 61); break;
		case 0x00f: take_lfo(c, 3, st); idle(c); break;          // and the LFO into m[3]
		default: break;
		}
	}
}

// The 124 steps of page g, the seventeen blocks in order.  One iteration per step, carrying the
// m_skip guard step() opens with.  The only gate in this program is the prologue's and it is shut
// again by the time the first page runs, so nothing past 0x00f is ever skipped.
void page(yss236_core& c, int g)
{
	for (int rel = 0; rel < 124; rel++)
	{
		int const st = PAGE_BASE[g] + rel;
		c.begin_step();
		if (st < c.m_skip) continue;
		if      (rel < 0x0c) lane_open(c, g, st, rel);
		else if (rel < 0x14) pole_coeffs(c, g, st, rel);
		else if (rel < 0x1c) page_pole_gains(c, g, st, rel);
		else if (rel < 0x24) resonance(c, g, st, rel);
		else if (rel < 0x28) pole_states(c, g, st, rel);
		else if (rel < 0x30) lpf12_coeffs(c, g, st, rel);
		else if (rel < 0x34) lpf12_taps(c, g, st, rel);
		else if (rel < 0x44) eg_sweep(c, g, st, rel);
		else if (rel < 0x54) lpf12_stage(c, g, st, rel);
		else if (rel < 0x5c) lpf18_input(c, g, st, rel);
		else if (rel < 0x60) lpf18_taps(c, g, st, rel);
		else if (rel < 0x64) cutoff_gains(c, g, st, rel);
		else if (rel < 0x74) output_ramp(c, g, st, rel);
		else if (rel < 0x78) ramp_slope(c, g, st, rel);
		else                 output_taps(c, g, st, rel);
	}
}

}  // namespace

// The whole program.  The check hands this every step, and a part of one is the interpreter's.
void flt(yss236_core& c, int from, int to)
{
	if (from != 0 || to != yss236_core::STEPS)
	{
		for (int st = from; st < to; st++) c.step(st);
		return;
	}
	prologue(c);
	for (int g = 0; g < 4; g++) page(c, g);
}

}  // namespace fs1r::native
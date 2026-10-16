// effects/var_m01.cpp - VOP3-2, the variation window's program for Rotary SP, Tremolo and Auto
// Pan: its 72 steps.
//
// rom::VARIATION_PROGRAMS[1], the program the window holds for the three variation types 8 to 10, and the
// only program among the twelve that these three share. Under each of them the same program words run and
// only the constants (rom::var's word lists, the LFO depths and the feedback level) and the eight delay
// offsets (rom::var::MOD_SLOTS) move, so this native is one function for all three and no type is
// excluded. Which of the three is running is not a code difference here; the host's parameters are.
//
// What the 72 steps compute, in the groups the loop below splits them into:
//
//   0x158-0x15a  the two serial inputs this window publishes, m[2] and m[6], and the three registers the
//                rest of the block works from, r[123], r[125] and r[1], each taken off the shared cell
//                m[8] and doubled.
//   0x15b-0x166  the three decay chains those three feed. Each folds a published slot's read through
//                |r|, then a saturating doubling that writes t[9], t[11] or t[13], then two multiplies
//                into r[124], r[126] and r[2]. m[28], m[29] and m[30] carry the three reads on.
//   0x167-0x16e  the fractional delay positions: t[15] and t[7] off the cell m[9], the first with an
//                index load of its own, and the two modulated positions r[123] and r[1], each clamped at
//                zero, which the taps below read at.
//   0x16f-0x17e  the left channel. The first tap (the slot 0x5b, read past the ring pointer) and the LFO
//                publish m[11] with t[9]; the second tap's capture m[62] and t[10] with its index load;
//                the input path r[5] to r[103], r[104] and r[105] against the taps 0x5c and 0x5d, which
//                are the two- and four-sample combs the delay line is written through; the captures m[56]
//                and m[57] with t[14] and t[12]; and the right position r[125] clamped at zero.
//   0x17f-0x185  the right channel, the mirror of the left one: the input path r[6] to r[106], r[107] and
//                r[108], the taps 0x5f and 0x60, the captures m[60], m[61] and m[58], and the tap read
//                0x61.
//   0x186-0x18e  the four interpolations, each written as its fraction times the second tap less the
//                fraction times the first, so m[56] and m[57] on t[10] make r[119], m[60] and m[61] on
//                t[14] make r[121] and m[62] and m[63] on t[7] make r[122]; then the three of them scaled
//                by (1 + t[9]), (1 + t[13]) and (1 + t[15]), which is where this block differs from the
//                chorus program's and what makes the amplitude and pitch move together.
//   0x18f-0x197  the second interpolation off the pair m[58] and m[59] on t[12] into r[120], its scaling by
//                (1 + t[11]), and the two output sums that add the three scaled values to the running
//                one.
//   0x198-0x19f  the two outputs r[15] and r[16], the captures m[32] and m[33] of the reads at 0x64 and
//                0x65, the two delay line writes into the slots 0x66 and 0x67, and the idle step 0x19e
//                between them.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs, r[103] and r[106]
// the scaled inputs, r[104], r[105], r[107] and r[108] the input accumulators, r[15] and r[16] the two
// outputs, which the base program reads at 0x018 and 0x03b as the later stereo pair after r[13] and
// r[14], r[119] to r[122] the four interpolated tap sums, r[123], r[125] and r[1] the three followers
// that end as two delay positions, r[124], r[126] and r[2] the products between them. The cells: m[8]
// the shared cell, m[2] and m[6] the serial inputs this window publishes, m[9] the cell the t[15] and
// t[7] positions scale off, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six
// published reads and m[56] to m[63] the eight taps. The delay slots: 0x66 and 0x67 the two delay lines,
// 0x5a to 0x61 the eight taps (rom::var::MOD_SLOTS), 0x56 to 0x58 and 0x63 to 0x65 the six reads it hands
// on.
//
// The t registers: t[7], t[10], t[12] and t[14] the four fractional tap positions, t[9], t[11], t[13] and
// t[15] the decay and scaling coefficients. Every one of the eight is written from a running value the
// step itself produced, so they move with the block and not with a parameter directly.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// The tables below are the units the program repeats, one row each, and the functions after them are what
// a unit computes; the loop at the end of the file is a dispatch on the step's place in the block and
// nothing else. Where the firmware ran the same shape three times over, three steps apart, or four times
// over, two apart, it is three or four rows of one table and one function, and the registers and cells
// those rows name are the ones the groups above describe.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m01: identical, 3 types, 28800 native passes.

#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The two steps this program has that the vocabulary does not name
// ---------------------------------------------------------------------------------------------

// A delay RAM read at the delay index (w2[3]), which is what every tap in the two channel blocks is:
// the ring pointer plus the position the last index load set, one word older where the word says so.
// read_delay() in steps.h is the same read at the ring pointer on its own, and the eight taps of this
// program add the index to it.
void read_at_index(yss236_core& c, int st, bool older)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, (older ? 1 : 0) + c.cur_idx(), false)]);
}

// A delay line write: the step's own result into the slot its step number names. The step carries the
// write data word (w1[13]) and its own memory cycle, so it writes the line itself and clears the latch
// rather than filling it for a step after it to write.
void write_delay_line(yss236_core& c, int st, double y)
{
	c.haswl = false;
	write_delay(c, st, yss236_core::clip(y, yss236_core::SAT));
}

// ---------------------------------------------------------------------------------------------
// What a step carries before its arithmetic: the program words that are neither the multiply nor the
// add. Every step of this program carries at most one of them, except 0x16f and 0x18f, which read the
// delay RAM and publish the window's LFO in the same step.
// ---------------------------------------------------------------------------------------------

enum dec_t { DEC_NONE, DEC_READ, DEC_INDEX, DEC_INDEX_OLD, DEC_READ_LFO, DEC_INDEX_LFO, DEC_CAPTURE };

// The cell an LFO or a capture step publishes into; for a read, the slot it reads, which is its own step
// over four and so is only here to be read.
struct Decor { dec_t kind; int cell; };

// step() moves these before it computes anything, and they touch nothing the arithmetic does, so the
// order between them and the multiply does not matter.
void decor(yss236_core& c, Decor const& d, int st)
{
	switch (d.kind)
	{
	case DEC_READ:      read_delay(c, st); break;
	case DEC_INDEX:     read_at_index(c, st, false); break;
	case DEC_INDEX_OLD: read_at_index(c, st, true); break;
	case DEC_READ_LFO:  read_delay(c, st); take_lfo(c, d.cell, st); break;
	case DEC_INDEX_LFO: read_at_index(c, st, true); take_lfo(c, d.cell, st); break;
	case DEC_CAPTURE:   take_capture(c, d.cell, st); break;
	default: break;
	}
}

// ---------------------------------------------------------------------------------------------
// The arithmetic shapes this program leans on, each a run() with the fields a step of this program
// fixes and nothing left out.
// ---------------------------------------------------------------------------------------------

// p = k * r[sr] * route, the adder off and nothing written: a step that exists for the running value it
// leaves behind, which the next step either adds to or closes into a register.
double carry(yss236_core& c, int st, int sr, route_t route = ROUTE_1, clamp_t clamp = CLAMP_SAT)
{
	return run(c, {.st = st, .sr = sr, .route = route, .clamp = clamp});
}

// p = |r[sr]|: a class-3 step, so the word is 1.0 and the register itself is the multiplier's second
// input, with the absolute value the clamp takes.
void abs_reg(yss236_core& c, int st, int sr)
{
	run(c, {.st = st, .sr = sr, .unity = true, .clamp = CLAMP_ABS});
}

// r[dr] = saturate(t[tn] * r[sr]): one multiply of a decay chain, the word in a t register rather than
// in the step itself.
void t_gain(yss236_core& c, int st, int sr, int dr, int tn)
{
	run(c, {.st = st, .sr = sr, .treg = tn, .clamp = CLAMP_SAT, .dr = dr});
}

// p = m[cell] * (t[tn] - 1): the first half of an interpolation, the tap below the fraction scaled by
// the fraction less one, with the cell's own value taken back off the adder.
void interp_lo(yss236_core& c, int st, int cell, int tn)
{
	run(c, {.st = st, .sm = cell, .m2 = M2_CELL, .treg = tn, .add = ADD_CELL, .sub = true});
}

// r[dr] = saturate(t[tn] * m[cell] - p): the second half, the same fraction on the tap above added to
// the first half and the pair closed into a register.
void interp_hi(yss236_core& c, int st, int cell, int tn, int dr)
{
	run(c, {.st = st, .sm = cell, .m2 = M2_CELL, .treg = tn, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = dr});
}

// r[sr] = saturate(t[tn] * r[sr] + r[sr]): the interpolated pair scaled by its own decay gain plus
// one, which is where this block differs from the chorus program's and what makes the amplitude and
// the pitch move together.
void scale_by_gain(yss236_core& c, int st, int sr, int tn)
{
	run(c, {.st = st, .sr = sr, .treg = tn, .add = ADD_REG, .clamp = CLAMP_SAT, .dr = sr});
}

// ---------------------------------------------------------------------------------------------
// The units the program repeats, one table row each
// ---------------------------------------------------------------------------------------------

// r[108] is the right channel's four-sample comb output, and both delay lines are written from it. The
// left one adds the running value its step before left, which holds the left channel's comb on the same
// word, and the two lines are what feeds the ring back round.
int const COMB_OUT = 108;

// One of the three followers. Two of the steps publish a serial input for the base program before they
// run, and all three take the shared cell m[8], add the step's own word to it as a class-1 load and
// double the sum into a register of its own.
struct Follower
{
	int st;             // the step
	int dr;             // the register m[8] is doubled into
	int cell;           // the cell it publishes a serial input into, 0 for none
	int bank;           // and the serial bank that input is on
};
constexpr Follower FOLLOWERS[3] = {
	{ 0x158, 123, 2, 0 },
	{ 0x159, 125, 6, 1 },
	{ 0x15a,   1, 0, 0 },
};

// One of the three decay chains, four steps each. The chain takes the absolute value of its follower
// register, doubles that saturating and keeps the result in a t register, then multiplies the follower
// by that gain into a register of its own and multiplies that by the gain again: two poles of decay,
// which is what the window's feedback level feeds. `st` holds the four in the order they run, and the
// shape each one has is its place in that four.
struct Decay
{
	int st[4];          // |follower|, the doubling that writes t[gain], and the two multiplies
	int follower, product;
	int gain;           // the t register, which is one of the four the decay chains and the scalings share
};
constexpr Decay DECAYS[3] = {
	{ { 0x15b, 0x15c, 0x161, 0x164 }, 123, 124,  9 },
	{ { 0x15d, 0x15e, 0x162, 0x165 }, 125, 126, 11 },
	{ { 0x15f, 0x160, 0x163, 0x166 },   1,   2, 13 },
};

// A delay position written to the t file. The running value is one position, scaled and saturated, and
// the t register takes it either whole or as the fraction below one sample, which is also the delay
// index the taps read at. `cell` and `sr` name the multiplier's second input and exactly one is set:
// m[9] carries the two positions the window is given, and the rest are built from a channel's register.
struct TWrite
{
	int st;
	int tn;             // the t register the position lands in
	bool frac;          // and whether it is a whole position or the fraction below one sample
	int sr;             // the register it scales, or 0
	int cell;           // the cell it scales, or 0
	Decor decor;
};
constexpr TWrite T_WRITES[8] = {
	// t[15] and t[7] off m[9]: the positions the window is given, the second also the index
	{ 0x167, 15, false,   0, 9, { DEC_NONE,     0 } },
	{ 0x168,  7, true,    0, 9, { DEC_CAPTURE, 30 } },
	// the left channel's first two taps, off r[123]
	{ 0x16f,  9, false, 123, 0, { DEC_INDEX_LFO, 11 } },
	{ 0x170, 10, true,  123, 0, { DEC_CAPTURE, 62 } },
	// t[14] off the left channel's r[1], between its two combs and its second position
	{ 0x178, 14, true,    1, 0, { DEC_CAPTURE, 56 } },
	// t[13] off the same r[1], then t[11] and t[12] off the position r[125]
	{ 0x17c, 13, false,   1, 0, { DEC_CAPTURE, 57 } },
	{ 0x17d, 11, false, 125, 0, { DEC_NONE,     0 } },
	{ 0x17e, 12, true,  125, 0, { DEC_NONE,     0 } },
};

// One modulated delay position, three steps. The follower's register is scaled by its word, the product
// register is added to that, and the sum is closed back into the follower with the negatives clamped
// away, which is what turns a signed position into a distance into the delay line. The closing step
// reads the slot it hands on, at the index.
struct Position
{
	int base;           // the first of the three steps
	int follower;       // the register the position is built from and lands in
	int product;        // and the one added to it
	Decor decor;        // the read on the closing step, none on the right channel's position
};
constexpr Position POSITIONS[3] = {
	{ 0x169, 123, 124, { DEC_INDEX, 0x5a } },   // the left tap's position, read at 0x5a
	{ 0x16c,   1,   2, { DEC_NONE,   0    } },   // the third tap's position
	{ 0x179, 125, 126, { DEC_INDEX, 0x5e } },   // the right tap's position, read at 0x5e
};

// One channel's input path, seven steps: the input scaled, then the two-sample comb and the four-sample
// comb the delay line is written through. `in` is the serial input register, `scaled` where the input
// lands doubled, `y1` the two-sample comb's output and `y2` the four-sample comb's. The two channels
// run the same seven steps and differ only in the taps and the cells published on the way past.
struct Comb
{
	int base;           // the first of the seven steps
	int in, scaled, y1, y2;
	Decor decor[7];
};
constexpr Comb COMBS[2] = {
	//    base   in  scaled  y1   y2
	{ 0x171,   5,   103, 104, 105, {
		{ DEC_NONE,      0 }, { DEC_NONE,      0 }, { DEC_INDEX,   0x5c }, { DEC_NONE,      0 },
		{ DEC_CAPTURE,  63 }, { DEC_NONE,      0 }, { DEC_INDEX_OLD, 0x5d } } },
	{ 0x17f,   6,   106, 107, 108, {
		{ DEC_INDEX_OLD, 0x5f }, { DEC_CAPTURE,  60 }, { DEC_NONE,      0 }, { DEC_NONE,      0 },
		{ DEC_INDEX,   0x60 }, { DEC_CAPTURE,  61 }, { DEC_NONE,      0 } } },
};

// One interpolation between two taps, two steps. `tn` holds the fraction between them, `older` and
// `newer` the cells holding the two taps and `out` the register the pair lands in.
struct Interp
{
	int base;           // the first of the two steps
	int tn, older, newer, out;
	Decor decor[2];
};
constexpr Interp INTERPS[4] = {
	{ 0x186, 10, 56, 57, 119, { { DEC_NONE,       0 }, { DEC_INDEX_OLD, 0x61 } } },
	{ 0x188, 14, 60, 61, 121, { { DEC_CAPTURE,   58 }, { DEC_NONE,       0    } } },
	{ 0x18a,  7, 62, 63, 122, { { DEC_NONE,       0 }, { DEC_NONE,       0    } } },
	{ 0x18f, 12, 58, 59, 120, { { DEC_READ_LFO,  12 }, { DEC_NONE,       0    } } },
};

// One interpolated pair scaled by its own decay gain plus one. Three of them run back to back after the
// first three interpolations and the fourth runs between the two output sums.
struct Scaling
{
	int st;
	int sr;             // the interpolated register
	int tn;             // and the gain it is scaled by
	Decor decor;
};
constexpr Scaling SCALINGS[4] = {
	{ 0x18c, 119,  9, { DEC_CAPTURE, 59 } },
	{ 0x18d, 121, 13, { DEC_NONE,     0 } },
	{ 0x18e, 122, 15, { DEC_NONE,     0 } },
	{ 0x194, 120, 11, { DEC_CAPTURE, 31 } },
};

// One output sum: the three interpolated taps added together in the order the program takes them, the
// third of them closing the sum into a register, with the read it hands on on that closing step.
struct OutSum
{
	int base;           // the first of the three steps
	int first, second, third;
	int out;            // the register the sum lands in
	Decor decor;
};
constexpr OutSum OUT_SUMS[2] = {
	{ 0x191, 119, 121, 122, 119, { DEC_READ, 0x64 } },
	{ 0x195, 121, 122, 120, 120, { DEC_READ, 0x65 } },
};

// One channel's output and its delay line, four steps. The first publishes the last read this window
// hands on and opens the sum on the channel's four-sample comb register; the second closes the sum
// into the output register the base program reads, four times over; the third opens the delay line's
// own sum; and the fourth writes the line. `reopen` is the step that opens the line's sum, and it is 0
// on the right channel, whose third step is the idle at 0x19e instead. `adds` says whether the line's
// sum carries on into the write.
struct OutLine
{
	int open;           // the capture and the first term of the output sum
	int into;           // the closing step, which writes the output register
	int reopen;         // the term that opens the delay line's own sum, or 0 for the idle step
	int write;          // the step the line is written at
	int comb;           // the four-sample comb register the two sums open on
	int sum;            // and the interpolated register that closes the first of them
	int dr;             // the output register the base program reads
	bool adds;          // whether the delay line's write adds to the running value
	Decor decor;
};
constexpr OutLine OUT_LINES[2] = {
	{ 0x198, 0x199, 0x19a, 0x19b, 105, 119, 15, true,  { DEC_CAPTURE, 32 } },
	{ 0x19c, 0x19d,     0, 0x19f, 108, 120, 16, false, { DEC_CAPTURE, 33 } },
};

// ---------------------------------------------------------------------------------------------
// What each unit computes
// ---------------------------------------------------------------------------------------------

// One follower: the serial input the base program is waiting for, then the shared cell doubled into
// the register the block below runs on.
void follower(yss236_core& c, Follower const& f)
{
	if (f.cell) take_serial(c, f.cell, f.st, f.bank);
	// r[dr] = wrap(2 * (128 k + m[8])): the shared cell, the step's own word as a class-1 load, doubled
	load(c, {.st = f.st, .sm = 8, .add = ADD_CELL, .route = ROUTE_2, .dr = f.dr});
}

// One step of one decay chain, `k` its place in the chain's four.
void decay(yss236_core& c, Decay const& d, int k)
{
	int const st = d.st[k];
	switch (k)
	{
	case 0:  abs_reg(c, st, d.follower); break;
	case 1:
		// r[follower] = saturate(2 * (128 k + p)) and t[gain] the running value as a 1.15 word
		load(c, {.st = st, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = d.follower});
		t_put(c, st, d.gain, c.acc);
		break;
	case 2:  t_gain(c, st, d.follower, d.product, d.gain); break;
	default: t_gain(c, st, d.product, d.product, d.gain); break;
	}
}

// A delay position written to the t file, and to the index queue where the position is a fraction.
void t_write(yss236_core& c, TWrite const& p)
{
	decor(c, p.decor, p.st);
	// The multiplier's second input is the shared cell m[9] on the two positions the window is handed
	// and a channel's own register on the rest.
	double const y = p.cell
		? run(c, {.st = p.st, .sm = p.cell, .m2 = M2_CELL, .clamp = CLAMP_SAT})
		: carry(c, p.st, p.sr);
	if (p.frac) { t_put_fraction(c, p.st, p.tn, y); load_index(c, y); }
	else t_put(c, p.st, p.tn, y);
}

// One modulated delay position, `k` its place in the three steps.
void position(yss236_core& c, int st, Position const& p)
{
	switch (st - p.base)
	{
	case 0: carry(c, st, p.follower, ROUTE_16, CLAMP_SAT); break;    // the follower's value, scaled up
	case 1: sum(c, st, p.product); break;                             // the product added to it
	default:
		decor(c, p.decor, st);
		// r[follower] = clamp_negatives(128 k + p): the position the taps below read at
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_ZERO, .dr = p.follower});
		break;
	}
}

// One channel's input path, `k` the step's place in its seven. The two combs are three steps each: the
// running value opened on the comb before, the scaled input or the first comb added to it, and the sum
// closed into the comb's own register with the route that restores the scale.
void comb(yss236_core& c, int st, Comb const& p, int k)
{
	decor(c, p.decor[k], st);
	switch (k)
	{
	case 0: gain(c, st, p.in, p.scaled, ROUTE_2); break;   // r[scaled] = 2 k r[in]: the scaled channel input
	case 1: term(c, st, p.y1); break;                      // the running value opened afresh
	case 2: sum(c, st, p.scaled); break;                   // the scaled input added to it
	case 3: close(c, st, p.scaled, p.y1, ROUTE_2); break;   // r[y1] = 2 (k r[scaled] + p): the two-sample comb
	case 4: term(c, st, p.y2); break;                      // and again, for the comb after it
	case 5: sum(c, st, p.y1); break;                       // the two-sample comb added to it
	default: close(c, st, p.y1, p.y2, ROUTE_4); break;     // r[y2] = 4 (k r[y1] + p): the four-sample comb
	}
}

// One interpolation between two taps, `k` which half of the two steps this is.
void interp(yss236_core& c, int st, Interp const& i, int k)
{
	decor(c, i.decor[k], st);
	if (k == 0) interp_lo(c, st, i.older, i.tn);
	else interp_hi(c, st, i.newer, i.tn, i.out);
}

// One interpolated pair scaled by its decay gain plus one.
void scaling(yss236_core& c, Scaling const& s)
{
	decor(c, s.decor, s.st);
	scale_by_gain(c, s.st, s.sr, s.tn);
}

// One output sum, `k` the step's place in its three.
void out_sum(yss236_core& c, int st, OutSum const& o, int k)
{
	switch (k)
	{
	case 0:  term(c, st, o.first); break;                   // the sum opens on the first interpolated tap
	case 1:  sum(c, st, o.second); break;                   // the second is added to it
	default:
		decor(c, o.decor, st);
		close(c, st, o.third, o.out, ROUTE_1);               // and the third closes the sum into a register
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The 72 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's. Every step's place in the block is fixed by the firmware's
// own spacing, so the ranges below are the program's and not ours.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{

		// ---- 0x158 to 0x15a, the two published serial inputs and the three followers ----
		if (st < 0x15b)
		{
			follower(c, FOLLOWERS[st - 0x158]);
			return;
		}

		// ---- 0x15b to 0x166, the three decay chains ----
		if (st < 0x167)
		{
			// Three published reads, four steps apart, each of the slot that is its own step over four:
			// 0x56, 0x57 and 0x58. The two captures follow the first two of them five steps later,
			// which is captured()'s lower bound, so m[28] takes up the read of 0x15b and m[29] that of
			// 0x15f; m[30] picks the third up at 0x168, in the block below.
			if (st == 0x15b || st == 0x15f || st == 0x163) read_delay(c, st);
			else if (st == 0x160) take_capture(c, 28, st);
			else if (st == 0x164) take_capture(c, 29, st);

			// The three chains run two steps of each first and one of each second: 0x15b, 0x15c,
			// 0x15d, 0x15e, 0x15f and 0x160 open and double, then 0x161 to 0x166 multiply. So in the
			// first group the chain is every other step and the place in its four the other, and in
			// the second the chain is every third step and the place the other.
			bool const opens = st < 0x161;
			int const j = st - (opens ? 0x15b : 0x161);
			decay(c, DECAYS[opens ? j / 2 : j % 3], opens ? j % 2 : 2 + j / 3);
			return;
		}

		// ---- 0x167 to 0x16e, the fractional delay positions ----
		if (st < 0x169) { t_write(c, T_WRITES[st - 0x167]); return; }               // t[15] and t[7] off m[9]
		if (st < 0x16f) { position(c, st, POSITIONS[st < 0x16c ? 0 : 1]); return; } // r[123] and r[1]

		// ---- 0x16f to 0x17e, the left channel ----
		if (st < 0x171) { t_write(c, T_WRITES[st - 0x16f + 2]); return; }   // t[9] and t[10]: its two taps
		if (st < 0x178) { Comb const& p = COMBS[0]; comb(c, st, p, st - p.base); return; }
		if (st < 0x179) { t_write(c, T_WRITES[4]); return; }              // t[14]: the pair on r[1]
		if (st < 0x17c) { position(c, st, POSITIONS[2]); return; }        // the position r[125]
		if (st < 0x17f) { t_write(c, T_WRITES[st - 0x17c + 5]); return; }  // t[13], t[11] and t[12]

		// ---- 0x17f to 0x185, the right channel ----
		if (st < 0x186) { Comb const& p = COMBS[1]; comb(c, st, p, st - p.base); return; }

		// ---- 0x186 to 0x18e, three interpolations and the three scalings ----
		if (st < 0x18c)
		{
			Interp const& i = INTERPS[(st - 0x186) / 2];
			interp(c, st, i, (st - 0x186) % 2);
			return;
		}
		if (st < 0x18f) { scaling(c, SCALINGS[st - 0x18c]); return; }

		// ---- 0x18f to 0x197, the second interpolation, its scaling and the two output sums ----
		if (st < 0x191) { Interp const& i = INTERPS[3]; interp(c, st, i, st - i.base); return; }  // on t[12]
		if (st < 0x194) { OutSum const& o = OUT_SUMS[0]; out_sum(c, st, o, st - o.base); return; }
		if (st == 0x194) { scaling(c, SCALINGS[3]); return; }                                  // by t[11]
		if (st < 0x198) { OutSum const& o = OUT_SUMS[1]; out_sum(c, st, o, st - o.base); return; }

		// ---- 0x198 to 0x19f, the two outputs and the two delay line writes ----
		// The same four steps on each channel: publish the last read and open the output sum on the
		// comb, close it into the register the base program reads, open the delay line's own sum, and
		// write the line. The right channel's third step is the idle at 0x19e, which is why its row
		// has no step to open on.
		OutLine const& o = OUT_LINES[st < 0x19c ? 0 : 1];
		if (st == o.open)
		{
			decor(c, o.decor, st);
			term(c, st, o.comb);                                 // the output sum opens on the comb
		}
		else if (st == o.into)
			close(c, st, o.sum, o.dr, ROUTE_4);                   // r[15] or r[16]: the channel's output
		else if (st == o.reopen)
			term(c, st, o.comb);                                 // the delay line's own sum opens
		else if (st == o.write)
			// The line is written from r[108] either way: the four-sample comb on the step's own word,
			// and the running value the step before left where the channel has one.
			write_delay_line(c, st, run(c, {.st = st, .sr = COMB_OUT, .add = o.adds ? ADD_ACC : ADD_OFF, .clamp = CLAMP_SAT}));
		else
			idle(c);                                             // 0x19e, the idle between the two writes
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m01(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
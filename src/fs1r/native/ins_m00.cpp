// fs1r/native/ins_m00.cpp - VOP3-2, the insertion window's program for type msb 00: its 96 steps.
//
// rom::INSERTION_PROGRAMS[0], the program the insertion window holds under every type
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 0, and under Thru, which loads it with every
// coefficient at zero. The nine the tracker names for this row are Thru, Chorus, Celeste, Flanger,
// Symphonic, Delay L,C,R, Delay L,R, Echo and Cross Delay, programmed by EffectHost::chorus(),
// flanger(), symphonic(), delayLCR(), delayLR(), echo() and crossDelay(). It is one of the fifteen
// programs the window shares: nothing here knows which type is running, because the program words are
// fixed and only the constants and the delay offsets move, which fx_modulation.cpp and fx_delay.cpp
// write whenever a parameter does. The code reads them where they live on the pass, as step() reads
// them, because that is the contract and not because the numbers are known here.
//
// What the 96 steps compute, in the groups the comments below open each of. The host names every group
// it programs, in EffectHost::chorus(), flanger(), symphonic(), delayLCR(), delayLR(), echo(),
// crossDelay() and the tables they call:
//
//   0x1a0-0x1be  the LFO cell m[12] through a rectifier into three accumulators, r[121], r[125] and
//                r[123], each of which names a delay length in samples: a register's value times 256
//                is the delay, its integer part is the index the taps read at (queue_i at 0x1b4,
//                0x1bd and 0x1d3, in effect three steps later) and its fraction the multiplier that
//                interpolates between a line's two taps. The steps that seed the three accumulators,
//                0x1a7, 0x1aa, 0x1af and 0x1b1 and the same four around r[125] and r[123], take their
//                words from ins::PC_FB and ins::LFO_DIR_STEPS, which are the ensemble detune's, and
//                from ins::PH_DEPTH, the phaser's. None of the handlers that load this program writes
//                them, so under those types the three accumulators hold zero, the index is zero and
//                every tap reads at the offset the host wrote. That is what this program does with the
//                words it is given, and the check below is what judges it; the host's chorus and
//                flanger are a question of their own, which this row does not touch.
//   0x1bf-0x1ca  the left channel's three bands. r[7], which the base program leaves the window, goes
//                into r[67] and through ins::LPF_MOD into r[68], the low shelf's output; that into
//                r[70] through ins::EQ_MOD, the mid peak; and that into r[72] through ins::HPF_MOD,
//                the high shelf's half. The first two delay taps, the slots 0x6E and 0x6F and 0x70 and
//                0x71, are read on the way past, at the first accumulator's index.
//   0x1cb-0x1ea  the other two taps, the slots 0x72 and 0x73 and 0x75 and 0x76, each pair at one word
//                apart so that the pair interpolates a fractional delay, and the four
//                interpolations: m[56] and m[57] to r[79] on t[9], m[60] and m[61] to r[119] on t[11],
//                m[58] and m[59] to r[81] on t[10] and m[62] and m[63] to r[120] on t[11]. The fixed
//                tap of the slot 0x68 is latched at 0x1cc and spent by the write into the slot 0x74 at
//                0x1d3, and the one of the slot 0x69 is latched at 0x1ea and spent by the write into the
//                slot 0x7A at 0x1eb. ins::LPF_MOD_C brings the right channel's r[8] in, and
//                ins::DELAY_CURVE makes r[79] the left channel's damped feedback in r[80].
//   0x1d9-0x1e4  the right channel's mid band and high shelf, ins::EQ_MOD_C into r[76] and
//                ins::HPF_MOD_C into r[78].
//   0x1e5-0x1ef  ins::DELAY_CURVE_COPY makes r[81] the right channel's damped feedback in r[82], and
//                the two wet sums: r[4] of r[119], r[120] and r[79], and r[5] of r[119], r[120] and
//                r[81].
//   0x1f0-0x1ff  the dry and wet, ins::MOD_WORDS's four words, into the two outputs r[17] and r[18],
//                which the base program reads back, and the two delay lines' writes: the slot 0x7E
//                takes r[80] at 0x1fb and the slot 0x7F takes r[82] with r[78] and r[72] at 0x1ff.
//
// The shapes those groups are written in below. The three delay lengths are one table, LENGTHS, rather
// than three, because the program interleaves the three accumulators instead of running one out before
// the next, so the table is one row per step and `shape` says which step of a delay length's recipe
// that step is; the third accumulator's six steps after 0x1be are LENGTH_TAIL, in three short runs,
// because the left channel's bands and the first tap sit in between them. The two channels' three bands
// are BANDS, and each band's own steps say which term of its difference equation they are. The four
// fractional taps are TAPS, the two damped feedbacks DAMPS, the two fixed taps FIXED, the two wet sums
// WET, the two outputs OUTPUTS and the two delay lines' writes DELAYS. Every group is a table and a
// loop; nothing here computes alone, and every step of the 96 is in one group or the other.
//
// The cells the program fills are its two serial inputs and its LFO, m[3] and m[7] from the banks at
// page 13 and m[4] from page 14, and m[13], m[14] and m[15] the page's LFOs, for the windows outside
// this one; and the eight reads it takes up for itself, m[42] and m[43] from the fixed slots 0x68 and
// 0x69 and m[56] to m[63] from the six swept taps, each five to eleven steps after the read that fills
// it, which is captured()'s window. The registers: r[7] and r[8] the two signals the base program
// leaves the window; r[67], r[68], r[70] and r[72] the left channel's three bands and r[73], r[74],
// r[76] and r[78] the right's; r[79] and r[80] the left channel's tap and its damped feedback, r[81] and
// r[82] the right's; r[119] and r[120] the two fractional taps; r[4] and r[5] the two wet sums; r[17] and
// r[18] the two outputs; r[121], r[125] and r[123] the three accumulators, with r[122], r[124], r[126]
// and r[127] their companion registers, r[1], r[2] and r[3] the seeds' products and r[69], r[71], r[75]
// and r[77] the four z^-1 copies the two right-hand filter bands ride on. The slots: 0x68 and 0x69 the
// two fixed taps, 0x6E and 0x6F, 0x70 and 0x71, 0x72 and 0x73 and 0x75 and 0x76 the four tap pairs,
// 0x74 and 0x7A the two slots the fixed taps are written into, which no step of this program reads,
// and 0x7E and 0x7F the two delay lines' own writes, which ins::MOD_SLOTS calls slots 5 and 0.
//
// The old model in vop3_effects.h has no case for this program's types beyond its chorus, flanger and
// echo one-liners, each a single delay line with a feedback and a dry/wet, and is no reference for any
// number here.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic
// below in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time. The one shape steps.h has no word for is a delay RAM read at a
// swept position, which all eight of the swept taps here are, so read_delay_at() below spells out that
// one line, as ins_m01.cpp's does for its own.
//
// vop3_native_check ins-m00: identical, 55 types, 528000 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z, load and the rest

// ---------------------------------------------------------------------------------------------
// What a step of this program carries besides its term
// ---------------------------------------------------------------------------------------------

// A step of this program usually also moves a cell or reads the delay RAM on the way past. `cell` is
// the cell a DEC_CAPTURE, DEC_LFO or DEC_SERIAL step publishes into. DEC_READ is a read at the slot's
// own offset, which is what the two fixed taps are read by; DEC_SWEEP and DEC_SWEEP_OLDER are a read
// at the index a delay length loaded, which is what makes a tap pair two words a sample apart: the
// earlier word at the index itself and the later one word older, and the t register the pair
// interpolates on the fraction between them.
enum decor_t { DEC_NONE, DEC_READ, DEC_SWEEP, DEC_SWEEP_OLDER,
              DEC_CAPTURE, DEC_LFO, DEC_SERIAL_L, DEC_SERIAL_R };

struct Decor { decor_t kind; int cell; };

// A delay RAM read at the index a delay length queued rather than at the ring pointer. steps.h has
// the ring pointer read in read_delay(); this is that read with the index added to the offset, which
// is what step() does for a step whose w2[3] is set.
void read_delay_at(yss236_core& c, int st, bool older)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, (older ? 1 : 0) + c.cur_idx(), false)]);
}

// Apply one of a step's decors. It comes before the step's own arithmetic, because step() moves a cell
// or reads the delay RAM before it does any arithmetic.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_READ:         read_delay(c, st); break;
		case DEC_SWEEP:        read_delay_at(c, st, false); break;
		case DEC_SWEEP_OLDER:  read_delay_at(c, st, true); break;
		case DEC_CAPTURE:      take_capture(c, d.cell, st); break;
		case DEC_LFO:          take_lfo(c, d.cell, st); break;
		case DEC_SERIAL_L:     take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R:     take_serial(c, d.cell, st, 1); break;
		case DEC_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// The three delay lengths, 0x1a0 to 0x1be and the third accumulator's tail at 0x1cb to 0x1d3
// ---------------------------------------------------------------------------------------------

// Which step of a delay length's recipe a step is. The three accumulators run the same recipe, but the
// program interleaves them, so a step's row says what that step computes rather than whose it is: R is
// the accumulator's own sweep register, C its companion and P the register the delay's word is
// interpolated into.
enum length_t
{
	LEN_FEED,       // r[127] = 16 * k * m[12]: the LFO cell through the feed word the companions add
	LEN_SEED,       // R = 2 * (128 * k + m[12]): the accumulator's first value, off the LFO cell
	LEN_CARRY,      // C or R = 2 * (128 * k + acc): the running value latched into a register
	LEN_COMP,       // C = 2 * (128 * k + r[127]): the companion, which adds the feed word
	LEN_NEXT,       // R = 128 * k + acc: the accumulator's next value, negatives held at zero
	LEN_RECT,       // acc = |R|: the sweep register rectified, its own word unused
	LEN_MRECT,      // acc = |k * C|: the companion through its own word, and rectified
	LEN_SCALE,      // R = 2 * k * R, and t the fraction below one sample of the delay R names
	LEN_DEEP,       // acc = 16 * k * R: the sweep register scaled into what the depth step adds to
	LEN_PRODUCT,    // P = t * R, and the same word again on P: the delay word, interpolated twice
	LEN_ACC,        // acc = k * P + acc: the product on top of the scaled sweep register
	LEN_DEPTH,      // acc = k * R + acc, and t the fraction and the index the swept taps read at
};

// One step of a delay length. `reg` is the register the step works on, `cell` the LFO cell the seed and
// the feed read, `treg` the t register holding a fraction where the step multiplies by one, and `out`
// the register it writes. `clamp` is per step and not per recipe, and `write` says the depth step at
// 0x1d3 is also the delay RAM writer that spends the first fixed tap's latch.
struct Length
{
	length_t shape;
	int reg, cell, treg, out;
	clamp_t clamp;
	bool write;
	Decor decor[2];
};

#define L(shape, reg, cell, treg, out, clamp, write, dk0, dc0, dk1, dc1) \
	{ shape, reg, cell, treg, out, clamp, write, { { dk0, dc0 }, { dk1, dc1 } } }

// The steps 0x1a0 to 0x1be, in step order: all three accumulators' seeds, companions, rectifiers,
// scaled sweep registers, interpolated products and depths, with the two fixed taps' reads at 0x1a3
// and 0x1a7, the first swept tap's read at 0x1bb and the LFO and serial cells published along the way.
constexpr Length LENGTHS[31] = {
	L(LEN_SEED,    0, 12, 0, 121, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1a0
	L(LEN_FEED,    0, 12, 0, 127, CLAMP_WRAP, false, DEC_SERIAL_L,   3, DEC_NONE, 0),      // 0x1a1
	L(LEN_CARRY,   0,  0, 0, 122, CLAMP_WRAP, false, DEC_SERIAL_R,   7, DEC_NONE, 0),      // 0x1a2
	L(LEN_RECT,  121,  0, 0,   0, CLAMP_ABS,  false, DEC_READ,        0, DEC_NONE, 0),      // 0x1a3
	L(LEN_CARRY,   0,  0, 0, 121, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1a4
	L(LEN_SEED,    0, 12, 0, 125, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1a5
	L(LEN_COMP,  127,  0, 0, 126, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1a6
	L(LEN_SCALE, 121,  0, 0, 121, CLAMP_SAT,  false, DEC_READ,        0, DEC_LFO,  13),     // 0x1a7
	L(LEN_RECT,  125,  0, 0,   0, CLAMP_ABS,  false, DEC_CAPTURE,    42, DEC_NONE, 0),      // 0x1a8
	L(LEN_CARRY,   0,  0, 0, 125, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1a9
	L(LEN_PRODUCT, 121, 0, 9, 1, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1aa
	L(LEN_SEED,    0, 12, 0, 123, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1ab
	L(LEN_SCALE, 125,  0, 0, 125, CLAMP_SAT,  false, DEC_CAPTURE,   43, DEC_NONE, 0),      // 0x1ac
	L(LEN_PRODUCT,   1, 0, 9, 1, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1ad
	L(LEN_COMP,  127,  0, 0, 124, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1ae
	L(LEN_DEEP,  121,  0, 0,   0, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1af
	L(LEN_ACC,      1,  0, 0,   0, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b0
	L(LEN_NEXT,    0,  0, 0, 121, CLAMP_ZERO, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b1
	L(LEN_PRODUCT, 125, 0,11,   3, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b2
	L(LEN_MRECT, 122,  0, 0,   0, CLAMP_ABS,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b3
	L(LEN_DEPTH, 121,  0, 0,   0, CLAMP_ZERO, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b4
	L(LEN_PRODUCT,   3, 0,11,   3, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b5
	L(LEN_RECT,  123,  0, 0,   0, CLAMP_ABS,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b6
	L(LEN_CARRY,   0,  0, 0, 123, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b7
	L(LEN_DEEP,  125,  0, 0,   0, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b8
	L(LEN_ACC,      3,  0, 0,   0, CLAMP_WRAP, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1b9
	L(LEN_NEXT,    0,  0, 0, 125, CLAMP_ZERO, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1ba
	L(LEN_SCALE, 123,  0, 0, 123, CLAMP_SAT,  false, DEC_SWEEP,      0, DEC_NONE, 0),      // 0x1bb
	L(LEN_MRECT, 126,  0, 0,   0, CLAMP_ABS,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1bc
	L(LEN_DEPTH, 125,  0, 0,   0, CLAMP_ZERO, false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1bd
	L(LEN_PRODUCT, 123, 0,10,   2, CLAMP_SAT,  false, DEC_NONE,       0, DEC_NONE, 0),      // 0x1be
};

// The third accumulator's last six steps, which the left channel's bands, the first fractional tap and
// the first fixed tap sit in between, so they are three short runs rather than one. `st` is the run's
// first step and `nsteps` how many steps it runs.
struct LengthRun { int st, nsteps; Length len[3]; };

constexpr LengthRun LENGTH_TAIL[3] = {
	{ 0x1cb, 1, {
		L(LEN_PRODUCT,   2, 0, 10, 2, CLAMP_SAT,  false, DEC_SWEEP, 0, DEC_NONE, 0) } },
	{ 0x1cd, 3, {
		L(LEN_DEEP,   123, 0,  0,   0, CLAMP_SAT,  false, DEC_CAPTURE, 61, DEC_NONE, 0),
		L(LEN_ACC,     2, 0,  0,   0, CLAMP_WRAP, false, DEC_NONE, 0, DEC_NONE, 0),
		L(LEN_NEXT,    0, 0,  0, 123, CLAMP_ZERO, false, DEC_SWEEP_OLDER, 0, DEC_NONE, 0) } },
	{ 0x1d2, 2, {
		L(LEN_MRECT, 124, 0,  0,   0, CLAMP_ABS,  false, DEC_NONE, 0, DEC_NONE, 0),
		L(LEN_DEPTH, 123, 0,  0,   0, CLAMP_ZERO, true,  DEC_NONE, 0, DEC_NONE, 0) } },
};

// The tail step of a step, -1 where there is none. The three runs never move, so a step reaches its run
// rather than looking through the three for it.
constexpr auto LENGTH_TAIL_AT = index_by_span(LENGTH_TAIL);

#undef L

// One step of a delay length, after its decors have been applied.
void length(yss236_core& c, int st, Length const& s)
{
	for (Decor const& d : s.decor) decor(c, st, d);
	switch (s.shape)
	{
	case LEN_FEED:
		// r[127] = 16 * k * m[12]: the LFO cell through the feed word the two companions add
		run(c, {.st = st, .sm = s.cell, .m2 = M2_CELL, .route = ROUTE_16, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_SEED:
		// R = 2 * (128 * k + m[12]), the accumulator's first value off the LFO cell
		load(c, {.st = st, .sm = s.cell, .add = ADD_CELL, .route = ROUTE_2, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_CARRY:
		// 2 * (128 * k + acc): the running value latched into the sweep register or its companion
		load(c, {.st = st, .add = ADD_ACC, .route = ROUTE_2, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_COMP:
		// the companion, which adds r[127] on its own step rather than the running value
		load(c, {.st = st, .sr = s.reg, .add = ADD_REG, .route = ROUTE_2, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_NEXT:
		// the accumulator's next value, with no route and the negatives held at zero
		load(c, {.st = st, .add = ADD_ACC, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_RECT:
		// acc = |R|: the sweep register rectified, the step's own word unused
		run(c, {.st = st, .sr = s.reg, .unity = true, .clamp = s.clamp});
		break;
	case LEN_MRECT:
		// acc = |k * C|: the companion through its own word, and rectified
		run(c, {.st = st, .sr = s.reg, .clamp = s.clamp});
		break;
	case LEN_SCALE:
	{
		// R = 2 * k * R, and t the fraction below one sample of the delay R names in samples
		double const y = run(c, {.st = st, .sr = s.reg, .route = ROUTE_2, .clamp = s.clamp, .dr = s.out});
		c.t_write(c.ops[st], st, y);
		break;
	}
	case LEN_DEEP:
		// acc = 16 * k * R, the sweep register scaled into what the depth step adds to
		run(c, {.st = st, .sr = s.reg, .route = ROUTE_16, .clamp = s.clamp});
		break;
	case LEN_PRODUCT:
		// P = t * R, and the same word again on P itself, which is the delay word interpolated twice
		run(c, {.st = st, .sr = s.reg, .treg = s.treg, .clamp = s.clamp, .dr = s.out});
		break;
	case LEN_ACC:
		run(c, {.st = st, .sr = s.reg, .add = ADD_ACC, .clamp = s.clamp});
		break;
	default:
	{
		// The depth step: acc = k * R + acc with the negatives held at zero, then t the fraction and
		// the index the swept taps read at. run() writes the delay RAM when the step is a writer, which
		// is 0x1d3 alone; that write and the two publishes are on separate state, so the order they
		// run in inside the step does not change what the step leaves behind.
		double const y = run(c, {.st = st, .sr = s.reg, .add = ADD_ACC, .clamp = s.clamp, .write = s.write});
		c.t_write(c.ops[st], st, y);
		load_index(c, y);
		break;
	}
	}
}

// ---------------------------------------------------------------------------------------------
// The two channels' three tone bands, 0x1bf to 0x1ca and 0x1d9 to 0x1e4
// ---------------------------------------------------------------------------------------------

// Which term of a band's difference equation one of its steps computes, and whether the adder is on.
// The mid peaks and the high shelves share the same five roles in the same order, but the two low
// shelves do not: the left one puts -a1 first, where the right one puts b1 first, and the order is the
// program's own because the running value is scaled and clamped between one step's term and the next.
enum band_role_t
{
	B_PRE,        // the step before the band, which scales the channel's signal into it
	B_TERM_IN,    // b1 on x[n] with the adder off, which opens the sum
	B_TERM_Y1,    // -a1 on y[n-1] with the adder off, which opens it instead
	B_B2,         // b2 on x[n-1], and the z^-1 that keeps that delay in a register
	B_SUM_IN,     // b1 on x[n], added to the running value
	B_SUM_Y1,     // -a1 on y[n-1], added
	B_SUM_Y2,     // -a2 on y[n-2], added
	B_B0,         // b0 on x[n], closing the sum into the band's own output
};

// One band, in direct form I. `st` is the band's first step and `nsteps` how many steps it runs. `src` is
// the register the channel's signal arrives in and `in` the one the B_PRE step scales it into, which is
// also where b1 and b0 read it. `x1` is the register b2 reads, which the same step copies into `keep`,
// so that x[-1] exists without a delay RAM; `y1` is the band's own output, which -a1 reads and b0
// overwrites; and `y2` is the one -a2 reads, which the band above keeps.
struct Band
{
	int st, nsteps;                 // its first step and how many steps it runs
	int src, in;
	int x1, keep;
	int y1, y2;
	step::route_t route;         // the route both the B_PRE step and the closing B_B0 take
	band_role_t role[5];
	Decor decor[5][2];
};

// The six bands, three per channel, each on the last band's output. ins::LPF_MOD and ins::EQ_MOD and
// ins::HPF_MOD write the left channel's five words and ins::LPF_MOD_C, ins::EQ_MOD_C and ins::HPF_MOD_C
// the right's. The two low shelves have a step of their own before them, which scales the channel's
// signal into the band, and no x[-1] or y[-2]; the two mid peaks keep both; and the two high shelves
// keep only x[-1], in the very register the mid peak above them reads its y[-2] from. The two low
// shelves differ in one more thing, which is the order of their two terms: the left opens the sum with
// -a1 on its own output and the right opens it with b1 on its input.
constexpr Band BANDS[6] = {
	//  base  steps  src  in  x1 keep  y1  y2  route    the band's roles                 its two decors per step
	{  0x1bf, 4,   7, 67,  0,  0, 68,  0, ROUTE_2, { B_PRE, B_TERM_Y1, B_SUM_IN, B_B0 }, {
		{ {DEC_SWEEP_OLDER, 0}, {DEC_NONE, 0} },
		{ {DEC_CAPTURE,  56},  {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} } } },
	{  0x1c3, 5,   0, 68, 69, 69, 70, 71, ROUTE_2, { B_TERM_IN, B_B2, B_SUM_Y1, B_SUM_Y2, B_B0 }, {
		{ {DEC_SWEEP, 0},      {DEC_NONE, 0} },
		{ {DEC_CAPTURE,  57},  {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_SWEEP_OLDER, 0}, {DEC_LFO, 14} } } },
	{  0x1c8, 3,   0, 70, 72, 71, 72,  0, ROUTE_4, { B_TERM_IN, B_B2, B_B0 }, {
		{ {DEC_CAPTURE,  60},  {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} } } },
	{  0x1d9, 4,   8, 73,  0,  0, 74,  0, ROUTE_2, { B_PRE, B_TERM_IN, B_SUM_Y1, B_B0 }, {
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_SWEEP_OLDER, 0}, {DEC_NONE, 0} },
		{ {DEC_CAPTURE,  58},  {DEC_NONE, 0} } } },
	{  0x1dd, 5,   0, 74, 75, 75, 76, 77, ROUTE_2, { B_TERM_IN, B_B2, B_SUM_Y1, B_SUM_Y2, B_B0 }, {
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_CAPTURE,  59},  {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} } } },
	{  0x1e2, 3,   0, 76, 78, 77, 78,  0, ROUTE_4, { B_TERM_IN, B_B2, B_B0 }, {
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} },
		{ {DEC_NONE, 0},       {DEC_NONE, 0} } } },
};

// One band's step k, after its decors have been applied.
void band(yss236_core& c, int st, Band const& b, int k)
{
	decor(c, st, b.decor[k][0]);
	decor(c, st, b.decor[k][1]);
	switch (b.role[k])
	{
	case B_PRE:     gain(c, st, b.src, b.in, b.route); break;   // the channel's signal into the band
	case B_TERM_IN: term(c, st, b.in); break;                    // b1 on x[n], the adder off
	case B_TERM_Y1: term(c, st, b.y1); break;                    // -a1 on y[n-1], the adder off
	case B_SUM_IN:  sum(c, st, b.in); break;                     // b1 on x[n]
	case B_SUM_Y1:  sum(c, st, b.y1); break;                     // -a1 on y[n-1]
	case B_SUM_Y2:  sum(c, st, b.y2); break;                     // -a2 on y[n-2]
	case B_B2:
		// b2 on x[n-1], and the same step's own z^-1 is what keeps that delay in a register
		copy_z(c, st, b.x1, b.keep, ADD_ACC, CLAMP_WRAP);
		break;
	default:       close(c, st, b.in, b.y1, b.route); break;    // b0 on x[n], closing into y[n]
	}
}

// The band of a step, -1 where it belongs to none: six rows of three to five steps that never move, and
// the index says which of a band's steps this one is as st - BANDS[i].st.
constexpr auto BANDS_AT = index_by_span(BANDS);

// ---------------------------------------------------------------------------------------------
// The four fractional taps, the two damped feedbacks and the two fixed taps
// ---------------------------------------------------------------------------------------------

// One fractional tap: the pair of cells m[lo] and m[hi] a word apart, which the fraction in t[treg]
// chooses between. The first step scales the older of the pair and subtracts the pair's own value,
// which is the difference; the second adds that difference to the younger one, which puts the scale
// back and closes the interpolation into `out`. So the whole of a tap is m[lo] + t * (m[hi] - m[lo]),
// written as two steps because the chip has one adder and no register to put the difference in.
// `st` is the pair's first step and `nsteps` the two it runs. `lead_write` says the first step is also a
// delay RAM writer, which is the second fixed tap's slot: the latch 0x1ea leaves is spent there, before
// the interpolation it belongs to has closed.
struct Tap
{
	int st, nsteps;                // the first of the two steps, and the two it runs
	int lo, hi;                   // the pair of cells, the older first
	int treg;                     // the t register holding the fraction between them
	int out;                      // the register the interpolation closes into
	Decor lead, trail;            // what each of the two steps publishes on the way past
	bool lead_write;
};

constexpr Tap TAPS[4] = {
	//  base  steps  lo  hi  treg  out  the first step     the second step            writer
	{  0x1d0,  2,    60, 61,  11, 119, {DEC_CAPTURE, 62}, {DEC_SERIAL_L, 4},         false },
	{  0x1d4,  2,    56, 57,   9,  79, {DEC_CAPTURE, 63}, {DEC_NONE, 0},              false },
	{  0x1e5,  2,    58, 59,  10,  81, {DEC_NONE, 0},     {DEC_NONE, 0},              false },
	{  0x1eb,  2,    62, 63,  11, 120, {DEC_NONE, 0},     {DEC_NONE, 0},              true  },
};

void tap(yss236_core& c, int st, Tap const& p, int k)
{
	decor(c, st, k ? p.trail : p.lead);
	if (k)
	{
		// r[out] = t * m[hi] - acc, which is m[lo] + t * (m[hi] - m[lo])
		run(c, {.st = st, .sm = p.hi, .m2 = M2_CELL, .treg = p.treg, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = p.out});
		return;
	}
	// acc = t * m[lo] - m[lo]: the pair's own difference, which the second step adds back
	run(c, {.st = st, .sm = p.lo, .m2 = M2_CELL, .treg = p.treg, .add = ADD_CELL, .sub = true, .write = p.lead_write});
}

// The tap of a step, -1 where there is none: four rows of two steps that never move.
constexpr auto TAPS_AT = index_by_span(TAPS);

// One damped feedback: ins::DELAY_CURVE's first word on the tap, its second word on the state the tap a
// pass back was written into, and the two summed and doubled into that same state, which is what makes
// the tap darker each time round the line. `st` is the run's first step and `nsteps` the three it runs.
struct Damp { int st, nsteps; int tap, state; Decor decor[3]; };

constexpr Damp DAMPS[2] = {
	//  base  steps  tap  state  the first step      the second            the third
	{  0x1d6,  3,     79,    80, { {DEC_NONE, 0}, {DEC_SWEEP, 0}, {DEC_NONE, 0} } },
	{  0x1e7,  3,     81,    82, { {DEC_LFO, 15}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
};

void damp(yss236_core& c, int st, Damp const& d, int k)
{
	decor(c, st, d.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, d.tap); break;
	case 1:  sum(c, st, d.state); break;
	default: close(c, st, d.tap, d.state, ROUTE_2); break;
	}
}

// The damped feedback of a step, -1 where there is none: two rows of three steps that never move.
constexpr auto DAMPS_AT = index_by_span(DAMPS);

// The two fixed taps: the reads of the slots 0x68 and 0x69, taken up into m[42] and m[43] five steps
// later and latched here as the data of a delay RAM write. Neither slot is read by any step of this
// program, so the pair is the window's only use of them.
struct FixedTap { int st, cell; };

constexpr FixedTap FIXED[2] = {
	{ 0x1cc, 42 },        // the left channel's, spent on the slot 0x74 at 0x1d3
	{ 0x1ea, 43 },        // the right channel's, spent on the slot 0x7a at 0x1eb
};

void fixed_tap(yss236_core& c, FixedTap const& f)
{
	// acc = m[cell] with the step's own word unused, and the result latched as the next write's data
	run(c, {.st = f.st, .sm = f.cell, .m2 = M2_CELL, .unity = true, .wdata = true});
}

// The fixed tap of a step, -1 where there is none: one step per row, indexed once at compile time.
constexpr auto FIXED_AT = index_by_step(FIXED);

// ---------------------------------------------------------------------------------------------
// The two wet sums, the two outputs and the two delay lines' writes
// ---------------------------------------------------------------------------------------------

// One wet sum: the channel's own damped tap, the first fractional tap and the second one, added in that
// order, into the register the base program reads as that channel's return. `st` is the run's first step
// and `nsteps` the three it runs.
struct WetSum { int st, nsteps; int own, first, second; int out; };

constexpr WetSum WET[2] = {
	{ 0x1ed, 3, 79, 119, 120,  4 },       // the left output's
	{ 0x1f0, 3, 81, 120, 119,  5 },       // the right output's
};

void wet_sum(yss236_core& c, int st, WetSum const& w, int k)
{
	switch (k)
	{
	case 0:  term(c, st, w.own); break;
	case 1:  sum(c, st, w.first); break;
	default: close(c, st, w.second, w.out, ROUTE_1); break;
	}
}

// The wet sum of a step, -1 where there is none: two rows of three steps that never move.
constexpr auto WET_AT = index_by_span(WET);

// One output: ins::MOD_WORDS' dry word on the channel's tone-shaped signal, the other channel's on top
// of it, and ins::MOD_WORDS' wet word on that channel's wet sum, the three scaled by the route that puts
// ins::MOD_WORDS' own scale back into the register the base program reads back. `st` is the run's first
// step and `nsteps` the three it runs.
struct Output { int st, nsteps; int dry, cross, wet; int reg; };

constexpr Output OUTPUTS[2] = {
	{ 0x1f3, 3, 72, 78,  4, 17 },        // the left output, r[17]
	{ 0x1f6, 3, 78, 72,  5, 18 },        // the right output, r[18]
};

void output(yss236_core& c, int st, Output const& o, int k)
{
	switch (k)
	{
	case 0:  term(c, st, o.dry); break;
	case 1:  sum(c, st, o.cross); break;
	default: close(c, st, o.wet, o.reg, ROUTE_4); break;
	}
}

// The output of a step, -1 where there is none: two rows of three steps that never move.
constexpr auto OUTPUTS_AT = index_by_span(OUTPUTS);

// One delay line's own write: the program's own registers taken into the running value, the last of
// them saturated, and that sum is what lands in the line for the next pass's taps to read back. The
// last step of each run is that writer, and it carries the wdata flag, so it drops any latched data and
// writes its own result. `st` is the run's first step and `nsteps` how long the run is.
struct DelayWrite { int st, nsteps; int a, b, c, state; };

constexpr DelayWrite DELAYS[2] = {
	{ 0x1f9, 3, 78, 82, 80, 80 },     // into the slot 0x7e, the second delay line's own write
	{ 0x1fc, 4, 72, 78, 80, 82 },     // into the slot 0x7f, the first delay line's
};

void delay_write(yss236_core& c, int st, DelayWrite const& w, int k)
{
	// The last step of the run is the writer, whichever index that is: the first delay line's run is
	// three steps long and the second one's four. It carries the wdata flag, so it drops any latched
	// data and writes its own result rather than taking an earlier step's.
	if (k == w.nsteps - 1)
	{
		run(c, {.st = st, .sr = w.state, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		return;
	}
	switch (k)
	{
	case 0:  term(c, st, w.a); break;
	case 1:  sum(c, st, w.b); break;
	default: sum(c, st, w.c); break;
	}
}

// The delay line write of a step, -1 where there is none: two rows of three and four steps that never
// move, and `nsteps` says which step of the run is the writer.
constexpr auto DELAYS_AT = index_by_span(DELAYS);

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. Each group below is a table of its own repeated unit, looked up by
// the step, and every one of the 96 steps is in a group or the other: nothing here is a class-0 step.
// Each table's index is the row that group's steps run, or -1 where the step is not one of that group's,
// and the groups are asked in the same order the rows are listed in, so a step two groups name runs the
// first group's row exactly as the scans did.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// ---- 0x1a0 to 0x1be: the LFO, the three accumulators and the three delay lengths. This table is
		//      not indexed by a scan but by hand, one row per step from 0x1a0, so the row is st - 0x1a0 ----
		if (st < 0x1bf) { length(c, st, LENGTHS[st - 0x1a0]); return; }

		int i;

		// ---- 0x1bf to 0x1ca and 0x1d9 to 0x1e4: the two channels' three bands each ----
		if ((i = BANDS_AT[st]) >= 0) { band(c, st, BANDS[i], st - BANDS[i].st); return; }

		// ---- 0x1d0, 0x1d4, 0x1e5 and 0x1eb: the four fractional taps, two steps each ----
		if ((i = TAPS_AT[st]) >= 0) { tap(c, st, TAPS[i], st - TAPS[i].st); return; }

		// ---- 0x1d6 and 0x1e7: the two damped feedbacks ----
		if ((i = DAMPS_AT[st]) >= 0) { damp(c, st, DAMPS[i], st - DAMPS[i].st); return; }

		// ---- 0x1cc and 0x1ea: the two fixed taps ----
		if ((i = FIXED_AT[st]) >= 0) { fixed_tap(c, FIXED[i]); return; }

		// ---- the third accumulator's tail, interleaved with three of the groups above ----
		if ((i = LENGTH_TAIL_AT[st]) >= 0) { length(c, st, LENGTH_TAIL[i].len[st - LENGTH_TAIL[i].st]); return; }

		// ---- 0x1ed and 0x1f0: the two wet sums ----
		if ((i = WET_AT[st]) >= 0) { wet_sum(c, st, WET[i], st - WET[i].st); return; }

		// ---- 0x1f3 and 0x1f6: the two outputs ----
		if ((i = OUTPUTS_AT[st]) >= 0) { output(c, st, OUTPUTS[i], st - OUTPUTS[i].st); return; }

		// ---- 0x1f9 to 0x1ff: the two delay lines' own writes ----
		if ((i = DELAYS_AT[st]) >= 0) { delay_write(c, st, DELAYS[i], st - DELAYS[i].st); return; }
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m00(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
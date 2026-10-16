// fs1r/native/var_m06.cpp - VOP3-2, the variation window's program for Ens Detune: its 72 steps.
//
// rom::VARIATION_PROGRAMS[6], the program the window holds for the one variation type 7, and one of the
// twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 6 to
// Ens Detune and to nothing else.
//
//   DETUNE_DET          {0x164, 0x16C, 0x173, 0x17B} the four detune words, each one a delay index
//   DETUNE_LOW_SHELF    {0x168, 0x167, 0x166}         the left channel's low shelf: b0, b1, -a1
//   DETUNE_HIGH_SHELF   {0x16B, 0x16A, 0x169}         the left channel's high shelf, the same three
//   DETUNE_LOW_SHELF_COPY  {0x177, 0x176, 0x175}      the right channel's low shelf
//   DETUNE_HIGH_SHELF_COPY {0x17A, 0x179, 0x178}      the right channel's high shelf
//   DETUNE_PAN          {0x195, 0x19B, 0x196, 0x19C}  the dry and wet pairs, left then right
//   DETUNE_SLOTS        {0x66, 0x59, 0x5A, 0x5B, 0x5C, 0x67, 0x5D, 0x5E, 0x5F, 0x60}
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, each b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and one
// of the output, the low shelf on a step that doubles its result and the high shelf on one that
// quadruples it. What comes out of them is what goes into the two delay lines.
//
// fx_modulation.cpp's ensembleDetune() writes DETUNE_DET[0] and DETUNE_DET[1] with one word and
// DETUNE_DET[2] and DETUNE_DET[3] with another, both off rom::DETUNE[], so each channel's two taps share
// one word; and DETUNE_SLOTS[0] and [5] are where each channel writes, [1] to [4] and [6] to [9] the four
// slots it reads, one delay value each for all four, so within a tap pair the two words are one sample
// apart and the 'older' bit on the second read is what makes it so. INIT_DELAY_L and INIT_DELAY_R set
// those four offsets to the base plus one plus the parameter, which puts the whole line that many
// samples behind the write.
//
// The 72 steps, in the groups the step loop below splits them into:
//
//   0x158-0x163  the two LFO cells into six registers, and the three oldest delay words published.
//                m[2] and m[6] take the serial inputs for the base program; the class-3 steps 0x158 and
//                0x159 put m[8] and m[9] doubled into r[123] and r[126], 0x15d and 0x162 take their
//                absolute values into r[124] and r[1], and the four class-1 loads leave the two base
//                delays r[125] and r[2] at max(0, 64 + the LFO) and the four crossfade weights
//                max(0, |2 * the LFO| - 24) and max(0, |2 * the LFO| - 40) in r[123], r[124] on the
//                left and r[126], r[1] on the right, the same two on both sides. 0x15b, 0x15f and 0x163
//                read the slots 0x56, 0x57 and 0x58 at the ring pointer, and 0x160, 0x164 and 0x168
//                publish them into m[22], m[23] and m[24].
//   0x164        DETUNE_DET[0]: the left tap pair's index. The result is the word times m[8], and
//                t[10] takes its fraction and the delay index its floor, so the index is in samples.
//   0x165-0x16b  the left channel's input r[103] and its two order-1 shelves into r[104] and r[105]:
//                the low shelf is -a1, b1 and b0 and the high shelf the same three, on the words
//                DETUNE_LOW_SHELF and DETUNE_HIGH_SHELF. 0x167 and 0x16b also read the slots 0x59 and
//                0x5a, the first tap pair, at the index 0x164 loaded, the second one word older.
//   0x16c        DETUNE_DET[1]: the second left tap pair's index, the same word times r[125], which is
//                the first one's plus 64 of the LFO's units, so the two taps are 256 * w * 64 samples
//                apart, w the word both share. With the ROM's own 0.034943 that is 572 samples, which
//                is what the index holds with the LFO cell at zero.
//   0x16d        the right channel's second crossfade weight, max(0, |2 * m[9]| - 40), into r[1],
//                which 0x162 left at |2 * m[9]|; r[126] keeps the -24 form 0x163 left for the first.
//   0x16f-0x172  the four crossfade weights, each the register times one eighth and sixteen, into
//                t[9], t[11], t[13] and t[15]. 0x172 also publishes the LFO of its page into m[11].
//   0x173        DETUNE_DET[2]: the first right tap pair's index, the word times m[9], and the read of
//                the slot 0x5c on the same step is the second word of the first pair.
//   0x174-0x17a  the right channel's input r[106] and its two shelves into r[107] and r[108], the
//                three steps each on DETUNE_LOW_SHELF_COPY and DETUNE_HIGH_SHELF_COPY. 0x175 publishes
//                the LFO of its page into m[9]. 0x177 and 0x17b read the slots 0x5d and 0x5e, the first
//                right tap pair, at the index 0x173 loaded.
//   0x17b        DETUNE_DET[3]: the second right tap pair's index, the same word times r[2].
//   0x17c-0x17d  the first left tap: m[56] and m[57] are the slot 0x59 and 0x5a words one sample apart,
//                and t[10] is the fraction between them, so r[119] is that pair interpolated.
//   0x17f-0x180  the second left tap, the slots 0x5b and 0x5c on t[12], into r[121].
//   0x183-0x184  the left crossfade: r[119] takes t[9] of its own value and one less t[11] of r[121],
//                so the two taps are summed with the weights the LFO cells give.
//   0x186-0x187  the first right tap, the slots 0x5d and 0x5e on t[14], into r[120].
//   0x18a-0x18b  the second right tap, the slots 0x5f and 0x60 on t[7], into r[122].
//   0x18e-0x18f  the right crossfade: r[120] takes t[13] of its own value and one less t[15] of r[122].
//                0x18f also reads the slot 0x63 at the ring pointer, publishes the LFO of its page into
//                m[12], and 0x188 publishes that read into m[63].
//   0x191-0x194  the two copies. The ROM's own words are a near-unity and a zero, so r[121] ends up
//                r[119] less one part in 32768 and r[122] r[120] the same. They are copies because
//                0x19a writes r[119] again with what goes into the left line, so the left blend has to
//                be somewhere else by the time 0x196 reads it.
//   0x195-0x19f  the outputs and the two delay writes. DETUNE_PAN's four words: r[15] takes the dry word
//                on r[105] and the wet word on r[121], both quadrupled, and r[16] the same on r[108]
//                and r[122]. dryWet() gives the variation windows no dry and this type a wet word of
//                8186, so each output is 0.9993 of its tap. 0x19a latches that left output and 0x19b
//                writes it into the slot 0x66, the left line; 0x19d and 0x19e build the right one, the
//                ROM's zero words on r[119] and r[120] adding nothing to it, and 0x19f writes it into
//                the slot 0x67 itself, latching nothing.
//
// The units the loop below runs, one table each. HEADS is the twelve steps at the head, which take the
// two serial inputs and build the two base delays and the four crossfade weights off the LFO cells.
// EQS is the two channels' seven-step EQ. INDICES is the four tap positions, WEIGHTS the four weights
// those positions' crossfades scale by. UNITS is the four interpolated taps and the two crossfades, two
// steps each, with the class-0 steps between them idle. COPIES, OUTPUTS and LINES are the tail: each
// crossfade copied so the line step can overwrite it, the dry and wet into r[15] and r[16], and the
// two lines written.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d write
// them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005). r[103] the left input,
// r[104] and r[105] its two shelves; r[106] the right input, r[107] and r[108] its two. r[123], r[124]
// and r[125] the left LFO's three, r[126], r[1] and r[2] the right's; r[119] and r[120] the two
// interpolated and crossfaded taps, r[121] and r[122] the copies the outputs read. The cells: m[2] and
// m[6] the serial inputs this block takes for the base program, m[8] and m[9] the window's two LFOs and
// the ones 0x172 and 0x175 publish as m[11] and m[12], m[22] to m[27] the six published delay reads,
// m[56] to m[63] the eight tap words, in pair order: left, left, right, right, left, left, right, right.
// The delay slots: 0x59 to 0x5c the left line's four words and 0x5d to 0x60 the right one's, all at the
// base the host set and the delay index added to the ring pointer; 0x66 and 0x67 are where each line is
// written. Each capture takes the read five steps before it (captured()'s lower bound), so m[56] is the
// slot 0x59 word, m[57] the slot 0x5a word, m[58] the slot 0x5d word, m[59] the slot 0x5e word, m[60] the
// slot 0x5b word, m[61] the slot 0x5c word, m[62] the slot 0x5f word and m[63] the slot 0x60 word, and
// m[22] to m[27] the slots 0x56, 0x57, 0x58, 0x64, 0x65 and 0x65.
//
// The eight t registers: t[10] and t[12] the two left tap positions, t[14] and t[7] the two right, each
// the fraction below one sample of the index its step queued; t[9] and t[11] the left crossfade weights
// and t[13] and t[15] the right ones, each a register divided by 16384. A t write divides by 128, so a
// fraction is the accumulator's 256th and a weight is twice its register over 32768.
//
// How far the taps sweep is the LFO cells' range: chips/vop3.cpp's lfo() scales the wave by the depth
// and multiplies by RANGE, so a full-depth bipolar saw fills m[8] and m[9] to +-128, and the check's
// run has m[8] at 127.96. A tap's index is then 256 * w * m[8] samples, the base delay r[125] one more
// 64 of the same, and the four weights 232/16384 and 216/16384 at their largest, so the second tap
// carries almost all of each channel's output.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m06: identical, 1 type, 9600 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, load, run and the rest

// ---------------------------------------------------------------------------------------------
// What a step does with a cell or the delay RAM before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step that is more than a term: it reads the delay RAM, publishes a capture of an earlier read, or
// takes a serial input or an LFO. `cell` is the cell a DEC_CAPTURE, DEC_LFO or DEC_SERIAL_* step moves
// into, and `lfo` the cell the step also publishes its page's LFO into, which is 0x18f alone: it reads
// the slot 0x63 and then publishes the LFO, and the read comes first as step() runs it.
enum decor_t { DEC_NONE, DEC_READ, DEC_READ_INDEX, DEC_READ_INDEX_OLD,
               DEC_CAPTURE, DEC_LFO, DEC_SERIAL_L, DEC_SERIAL_R };

struct Decor { decor_t kind; int cell; int lfo; };

// A delay RAM read at the index a tap position loaded rather than at the ring pointer, which is what
// makes a tap pair two words a sample apart: the earlier word at the index itself, the later one word
// older, and the t register the pair interpolates on the fraction between them. steps.h has the ring
// pointer read in read_delay(); this is that read with the index added to the offset.
void read_delay_at(yss236_core& c, int st, bool older)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, (older ? 1 : 0) + c.cur_idx(), false)]);
}

// The decor a step carries. It comes before the step's own arithmetic, because step() moves a cell or
// reads the delay RAM before it multiplies anything.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_READ:           read_delay(c, st); break;
		case DEC_READ_INDEX:     read_delay_at(c, st, false); break;
		case DEC_READ_INDEX_OLD: read_delay_at(c, st, true); break;
		case DEC_CAPTURE:        take_capture(c, d.cell, st); break;
		case DEC_LFO:            take_lfo(c, d.cell, st); break;
		case DEC_SERIAL_L:       take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R:       take_serial(c, d.cell, st, 1); break;
		case DEC_NONE: break;
	}
	if (d.lfo)
		take_lfo(c, d.lfo, st);
}

// ---------------------------------------------------------------------------------------------
// 0x158 to 0x163, the two LFO cells and what the base delays and the weights come off them
// ---------------------------------------------------------------------------------------------

// What a step of the head computes. H_SERIAL is the class-3 step that puts 2 * an LFO cell into a
// register of its own, H_HALF the running value a base delay is built on, H_ABS the absolute value both
// weights come from, H_BASE the base delay itself, and H_FIRST and H_SECOND the -24 and -40 weights.
enum head_t { H_SERIAL, H_IDLE, H_HALF, H_BASE, H_ABS, H_FIRST, H_SECOND };

// One step of the head. `cell` is the window's LFO cell H_SERIAL reads, and `reg` and `reg2` the
// registers the other shapes work on: the doubled LFO for H_SERIAL, H_HALF and H_ABS, and for H_ABS
// the absolute value lands in `reg2`; the base delay for H_BASE; the first weight for H_FIRST; and
// for H_SECOND one register, the left channel's |2 * m[8]|, which is what 0x161 takes the -40 off. The
// two channels' steps interleave one apart and 0x15a is idle between the left channel's first two.
struct Head { head_t shape; int cell; int reg; int reg2; Decor decor; };

const Head HEADS[12] = {
	//  shape       cell  reg  reg2  the decor
	{  H_SERIAL,       8, 123,   0, { DEC_SERIAL_L, 2, 0 } },   // 0x158: the left cell, doubled, with the left serial input
	{  H_SERIAL,       9, 126,   0, { DEC_SERIAL_R, 6, 0 } },   // 0x159: the right one, and the right serial input
	{  H_IDLE,         0,   0,   0, { DEC_NONE,     0, 0 } },   // 0x15a
	{  H_HALF,         0, 123,   0, { DEC_READ,     0, 0 } },   // 0x15b: the slot 0x56, and half of r[123]
	{  H_BASE,         0, 125,   0, { DEC_NONE,     0, 0 } },   // 0x15c: the left base delay
	{  H_ABS,          0, 123, 124, { DEC_NONE,     0, 0 } },   // 0x15d: |r[123]| into r[124]
	{  H_FIRST,        0, 123,   0, { DEC_NONE,     0, 0 } },   // 0x15e: the -24 weight
	{  H_HALF,         0, 126,   0, { DEC_READ,     0, 0 } },   // 0x15f: the slot 0x57, and half of r[126]
	{  H_BASE,         0,   2,   0, { DEC_CAPTURE, 22, 0 } },   // 0x160: the right base delay, and the slot 0x56 read published
	{  H_SECOND,       0, 124,   0, { DEC_NONE,     0, 0 } },   // 0x161: the -40 weight, still the left channel's
	{  H_ABS,          0, 126,   1, { DEC_NONE,     0, 0 } },   // 0x162: |r[126]| into r[1]
	{  H_FIRST,        0, 126,   0, { DEC_READ,     0, 0 } },   // 0x163: the slot 0x58, and the -24 weight on it
};

// One step of the head. The base delay and the two weights are all class-1 loads off the running
// value, and all hold the negatives at the zero, so a weight below its offset reads as no weight.
void head(yss236_core& c, int st)
{
	Head const& h = HEADS[st - 0x158];
	decor(c, st, h.decor);
	switch (h.shape)
	{
	case H_SERIAL:  // r[123] and r[126]: the class-3 step, 2 * the LFO cell, wrapped at +-128
		run(c, { st, 0, h.cell, M2_CELL, true, 0, ADD_OFF, false, ROUTE_2, CLAMP_WRAP, h.reg });
		break;
	case H_HALF:    // the ROM's half on that register, into the running value
		term(c, st, h.reg);
		break;
	case H_BASE:    // r[125] and r[2]: max(0, 64 + that half), the line's base delay
		load(c, st, 0, 0, ADD_ACC, false, ROUTE_1, CLAMP_ZERO, h.reg);
		break;
	case H_ABS:     // r[124] and r[1]: |2 * the LFO cell|, which both weights come off
		run(c, { st, h.reg, 0, M2_REG, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_ABS, h.reg2 });
		break;
	case H_FIRST:   // max(0, that - 24), the first weight of the channel, before its factor
		load(c, st, 0, 0, ADD_ACC, false, ROUTE_1, CLAMP_ZERO, h.reg);
		break;
	case H_SECOND:  // max(0, the left |2 * m[8]| - 40), the second weight of the left channel
		load(c, st, h.reg, 0, ADD_REG, false, ROUTE_1, CLAMP_ZERO, h.reg);
		break;
	default:        // 0x15a: class 0, no RAM mark and no ALU, so only the x latch
		idle(c);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The two channels' EQ, two order-1 shelves each
// ---------------------------------------------------------------------------------------------

// One channel's seven steps. `src` is the input the base program leaves this window in, `in` this
// channel's own copy of it on the ROM's near-unity word doubled, `lo` the low shelf's output and `hi`
// the high shelf's, which is the channel's EQ output: what its line takes and what its dry word scales
// on the way to the output register.
//
// The three registers hold the same quantities a pass apart at different points of the run, because a
// step's write lands three steps later and each shelf borrows the other's register. `in` is the low
// shelf's output a pass back until the low shelf's closing step lands the channel's input in it, and
// `lo` is that low shelf output from then until the high shelf's closing step lands it again.
struct Eq
{
	int base;
	int src, in, lo, hi;
	Decor decor[7];
};

// The left channel, whose steps publish three of the base program's delay reads, then the right one,
// which publishes the LFO of its page. The seven steps are the channel's input on the ROM's own word,
// then the low shelf's -a1, b1 and b0, then the high shelf's -a1, b1 and b0, and each row's decor list
// is in that order.
const Eq EQS[2] = {
	//  base   src  in   lo   hi   the seven steps' decor, in that order
	{  0x165,   5, 103, 104, 105, { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}, {DEC_READ_INDEX, 0, 0}, {DEC_CAPTURE, 24, 0}, {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}, {DEC_READ_INDEX_OLD, 0, 0} } },
	{  0x174,   6, 106, 107, 108, { {DEC_CAPTURE, 60, 0}, {DEC_LFO, 9, 0}, {DEC_NONE, 0, 0}, {DEC_READ_INDEX, 0, 0}, {DEC_CAPTURE, 61, 0}, {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0} } },
};

// One channel's EQ step. The first step takes the input the base program wrote into `in`, and the six
// after it are the two shelves. Each opens on the shelf's own output a pass back, adds the shelf's
// other word on the register that still holds it, and closes the sum on the value the channel's input
// or the low shelf's output has just landed in.
void eq(yss236_core& c, int st, Eq const& q)
{
	int const k = st - q.base;
	decor(c, st, q.decor[k]);
	switch (k)
	{
	case 0:  gain(c, st, q.src, q.in, ROUTE_2); break;   // the channel's input, doubled
	case 1:  term(c, st, q.lo); break;                   // the low shelf's -a1 on its output a pass back
	case 2:  sum(c, st, q.in); break;                    // and its b1, r[in] holds that output until 0x168
	case 3:  close(c, st, q.in, q.lo, ROUTE_2); break;   // its b0 on the input just landed, into the low shelf
	case 4:  term(c, st, q.hi); break;                   // the high shelf's -a1 on its output a pass back
	case 5:  sum(c, st, q.lo); break;                    // and its b1 on the low shelf's output a pass back
	default: close(c, st, q.lo, q.hi, ROUTE_4); break;   // its b0 on that output just landed, into the EQ output
	}
}

// ---------------------------------------------------------------------------------------------
// The four tap positions, and the four crossfade weights
// ---------------------------------------------------------------------------------------------

// One tap position: DETUNE_DET's word scaled onto the channel's LFO cell, or onto the base delay that
// cell produced. The result is in delay samples, so its fraction below one goes into the t register the
// pair interpolates on and its floor is the delay index the pair's two reads take. `cell` is nonzero
// when the word scales a cell and `reg` when it scales a register.
struct Index
{
	int base;
	int cell;
	int reg;
	int treg;
	Decor decor;
};

const Index INDICES[4] = {
	//  base   cell  reg  treg  the decor
	{  0x164,     8,   0,   10, { DEC_CAPTURE,      23, 0 } },   // the first left tap, on m[8]
	{  0x16c,     0, 125,   12, { DEC_CAPTURE,      56, 0 } },   // the second left tap, on r[125]
	{  0x173,     9,   0,   14, { DEC_READ_INDEX_OLD, 0, 0 } },   // the first right tap, on m[9]
	{  0x17b,     0,   2,    7, { DEC_READ_INDEX_OLD, 0, 0 } },   // the second right tap, on r[2]
};

// One tap position's own step: the word on the cell or the register, its fraction into the t register
// and its whole samples into the delay index. The two taps of a channel share the word and differ only
// in what it scales, which is what puts them 256 * w * 64 samples apart on the left.
void tap_index(yss236_core& c, int st, Index const& x)
{
	decor(c, st, x.decor);
	double const y = x.cell ? run(c, { st, 0, x.cell, M2_CELL, false, 0, ADD_OFF, false, ROUTE_1, CLAMP_ZERO })
	                        : run(c, { st, x.reg, 0, M2_REG,   false, 0, ADD_OFF, false, ROUTE_1, CLAMP_ZERO });
	t_put_fraction(c, x.treg, y);   // the position between the two words the pair reads
	load_index(c, y);                // and the same value in whole samples, for the reads themselves
}

// One crossfade weight: its register on the ROM's eighth word and the x16 route, which a t register
// takes as the accumulator's 256th. So a register of 232 is 232 / 16384 by the time the crossfade
// scales with it.
struct Weight { int reg; int treg; Decor decor; };

// The four weights in step order: the left channel's -24 and -40 forms then the right channel's, and
// the last step publishes this page's LFO for the base program as it goes.
const Weight WEIGHTS[4] = {
	//  reg  treg  the decor
	{   123,     9, { DEC_READ_INDEX, 0, 0 } },    // 0x16f: the slot 0x5b, at the index 0x16c loaded
	{   124,    11, { DEC_CAPTURE, 57, 0 } },      // 0x170: the read of 0x16b
	{   126,    13, { DEC_NONE,     0, 0 } },      // 0x171: the right channel's first weight
	{     1,    15, { DEC_LFO,     11, 0 } },      // 0x172: its second, and this page's LFO
};

void weight(yss236_core& c, int st, Weight const& w)
{
	decor(c, st, w.decor);
	t_put(c, w.treg, run(c, { st, w.reg, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_SAT }));
}

// ---------------------------------------------------------------------------------------------
// The four interpolated taps and the two crossfades, two steps each
// ---------------------------------------------------------------------------------------------

// U_TAP is one tap pair: the two words a sample apart, with the position's fraction taken off the
// earlier word and applied to the later one, so the register its second step writes holds the line
// interpolated at the LFO's position. U_XFADE is one channel's crossfade: the second tap on its own
// weight, then the first tap added on top of that, so the register left is the pair summed.
enum unit_t { U_TAP, U_XFADE };

// One of the six. `in` is the tap's earlier word's cell or the crossfade's second tap register, `in2`
// the tap's later word's cell, `pos` the tap's position t register or the crossfade's weight on the
// second tap, `pos2` the crossfade's weight on the first tap, and `out` the register the second step
// closes into.
struct Unit
{
	int base;
	unit_t shape;
	int in, in2, pos, pos2, out;
	Decor decor[2];
};

// The six in step order: the four taps, the left crossfade between the second and third of them, and
// the right crossfade last. The steps between are class 0 and idle, so the gaps are the program's own
// rather than anything these rows work around.
const Unit UNITS[6] = {
	//  base      shape    in  in2  pos  pos2  out  the two steps' decor
	{  0x17c, U_TAP,    56,  57,   10,    0, 119, { {DEC_CAPTURE, 58, 0}, {DEC_NONE, 0, 0}          } },
	{  0x17f, U_TAP,    60,  61,   12,    0, 121, { {DEC_READ_INDEX, 0, 0}, {DEC_CAPTURE, 59, 0}   } },
	{  0x183, U_XFADE, 121,   0,   11,    9, 119, { {DEC_READ_INDEX_OLD, 0, 0}, {DEC_CAPTURE, 62, 0} } },
	{  0x186, U_TAP,    58,  59,   14,    0, 120, { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}              } },
	{  0x18a, U_TAP,    62,  63,    7,    0, 122, { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}              } },
	{  0x18e, U_XFADE, 122,   0,   15,   13, 120, { {DEC_NONE, 0, 0}, {DEC_READ, 0, 12}              } },
};

// One unit's two steps. A tap takes its position's fraction off the earlier word before it applies it
// to the later one, and a crossfade takes the second tap off the running value before it adds the
// first tap's weighted value on top. Both close into the register the next unit reads.
void unit(yss236_core& c, int st, Unit const& u)
{
	int const k = st - u.base;
	decor(c, st, u.decor[k]);
	if (u.shape == U_TAP)
	{
		if (k == 0)
			run(c, { st, 0, u.in,  M2_CELL, false, u.pos, ADD_CELL, true, ROUTE_1, CLAMP_WRAP });
		else
			run(c, { st, 0, u.in2, M2_CELL, false, u.pos, ADD_ACC,  true, ROUTE_1, CLAMP_SAT, u.out });
		return;
	}
	if (k == 0)
		run(c, { st, u.in,  0, M2_REG, false, u.pos,  ADD_REG, true, ROUTE_1, CLAMP_WRAP });
	else
		run(c, { st, u.out, 0, M2_REG, false, u.pos2, ADD_ACC, true, ROUTE_1, CLAMP_SAT, u.out });
}

// Which of the six a step belongs to, or -1 for the class-0 steps between them.
int unit_of(int st)
{
	for (int i = 0; i < 6; i++)
		if (st >= UNITS[i].base && st < UNITS[i].base + 2)
			return i;
	return -1;
}

// ---------------------------------------------------------------------------------------------
// 0x191 to 0x19f, the two copies, the two outputs and the two delay writes
// ---------------------------------------------------------------------------------------------

// The two copies, two steps each: the crossfaded tap on the ROM's near-unity word opening the running
// value and the other tap on the ROM's own zero closing it, so a copy is the crossfade less one part
// in 32768. They are copies because r[119] and r[120] are read again on the way into the two delay
// lines, so each channel's blend has to be somewhere else for its output step to read.
struct Copy { int base; int dst; Decor decor[2]; };

const Copy COPIES[2] = {
	//  base    dst  the two steps' decor
	{  0x191,   121, { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0} } },
	{  0x193,   122, { {DEC_READ, 0, 0}, {DEC_CAPTURE, 25, 0} } },
};

// One of the two: the crossfaded tap on the ROM's near-unity word opens the running value and the other
// tap on the ROM's own zero closes it into `dst`.
void tap_copy(yss236_core& c, int st, Copy const& p)
{
	int const k = st - p.base;
	decor(c, st, p.decor[k]);
	if (k == 0) term(c, st, 119);
	else        close(c, st, 120, p.dst);
}

// One channel's two output steps: DETUNE_PAN's dry word opens the sum on the channel's EQ output and its
// wet word closes it into the register the base program reads, quadrupled. Returns the running value,
// which is what the left line's write at 0x19b takes where there is no latched data to write instead.
struct Output { int base; int out, dry, wet; Decor decor[2]; };

const Output OUTPUTS[2] = {
	//  base    out  dry  wet  the two steps' decor
	{  0x195,    15, 105, 121, { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0} } },
	{  0x19b,    16, 108, 122, { {DEC_NONE, 0, 0}, {DEC_CAPTURE, 27, 0} } },
};

double output(yss236_core& c, int st, Output const& o)
{
	decor(c, st, o.decor[st - o.base]);
	if (st == o.base) return run(c, { st, o.dry });       // the dry word, on the channel's EQ output
	return run(c, { st, o.wet, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_4, CLAMP_SAT, o.out });
}

// One delay line's input: the run of steps that opens on the channel's EQ output, adds the ROM's own
// zeroes on the other two registers, and closes the sum. The left line's closing step writes no register
// at all and only latches its result as the data of the write one step later; the right one's is the
// line's write itself.
struct Line
{
	int base;
	int n;              // the steps in the run
	int src[4];         // the register each step scales, in order
	bool ram;           // the closing step is the line's write rather than a latch
	Decor decor[4];
};

const Line LINES[2] = {
	//  base    n  the registers                    ram  the decor, in step order
	{  0x197, 4, { 105, 108, 120, 119 },  false, { {DEC_READ, 0, 0}, {DEC_CAPTURE, 26, 0}, {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0} } },
	{  0x19d, 3, { 108, 119, 120,   0 },  true,  { {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0}, {DEC_NONE, 0, 0} } },
};

// One step of the run: the first opens the running value on the channel's EQ output, the ones after it
// add the ROM's own zeroes, and the closing one hands the sum to w1[13] as the line's write data. `src`
// is the register each step scales and `ram` whether the closing step is the write itself.
void line(yss236_core& c, int st, Line const& l)
{
	int const k = st - l.base;
	decor(c, st, l.decor[k]);
	if (k == 0) term(c, st, l.src[0]);
	else if (k < l.n - 1) sum(c, st, l.src[k]);
	else
		run(c, { st, l.src[k], 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT,
		         0 /* dr */, false, 0, false, false, true, l.ram });
}

// ---------------------------------------------------------------------------------------------
// The run of steps 0x158 to 0x19f, the whole of the window's program
// ---------------------------------------------------------------------------------------------

// The loop is one iteration per step and carries the m_skip guard step() opens with, so a gate in the
// base program's 0x1xx half skips these steps as it would the interpreter's. Each test below is one
// of the groups the comments at the head of this file open.
void block_0x158(yss236_core& c)
{
	for (int st = 0x158; st < 0x1a0; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		if (st < 0x164)
		{
			// ---- 0x158 to 0x163, the two LFO cells, the two base delays and the four weights ----
			head(c, st);
			continue;
		}
		if (st == 0x164)
		{
			// ---- DETUNE_DET[0]: the first left tap pair's position ----
			tap_index(c, st, INDICES[0]);
			continue;
		}
		if (st < 0x16c)
		{
			// ---- 0x165 to 0x16b, the left channel's input and its two shelves ----
			eq(c, st, EQS[0]);
			continue;
		}
		if (st == 0x16c)
		{
			// ---- DETUNE_DET[1]: the second left tap pair's position ----
			tap_index(c, st, INDICES[1]);
			continue;
		}
		if (st == 0x16d)
		{
			// ---- the right channel's second weight, max(0, |2 * m[9]| - 40), over the first ----
			load(c, st, 1, 0, ADD_REG, false, ROUTE_1, CLAMP_ZERO, 1);
			continue;
		}
		if (st == 0x16e)
		{
			// the idle step between the right weight and the four the weights go into
			idle(c);
			continue;
		}
		if (st < 0x173)
		{
			// ---- 0x16f to 0x172, the four crossfade weights ----
			weight(c, st, WEIGHTS[st - 0x16f]);
			continue;
		}
		if (st == 0x173)
		{
			// ---- DETUNE_DET[2]: the first right tap pair's position ----
			tap_index(c, st, INDICES[2]);
			continue;
		}
		if (st < 0x17b)
		{
			// ---- 0x174 to 0x17a, the right channel's input and its two shelves ----
			eq(c, st, EQS[1]);
			continue;
		}
		if (st == 0x17b)
		{
			// ---- DETUNE_DET[3]: the second right tap pair's position ----
			tap_index(c, st, INDICES[3]);
			continue;
		}
		if (st < 0x191)
		{
			// ---- 0x17c to 0x190, the four interpolated taps and the two crossfades ----
			int const u = unit_of(st);
			if (u < 0)
			{
				// 0x188 is the one idle step that moves a cell: m[63] takes up the read of 0x183
				if (st == 0x188)
					take_capture(c, 63, st);
				idle(c);
			}
			else
				unit(c, st, UNITS[u]);
			continue;
		}
		if (st < 0x195)
		{
			// ---- 0x191 to 0x194, the two copies of the crossfades ----
			tap_copy(c, st, COPIES[(st - 0x191) / 2]);
			continue;
		}
		if (st < 0x197)
		{
			// ---- 0x195 and 0x196, the left output ----
			output(c, st, OUTPUTS[0]);
			continue;
		}
		if (st < 0x19b)
		{
			// ---- 0x197 to 0x19a, what goes into the left line ----
			line(c, st, LINES[0]);
			continue;
		}
		if (st < 0x19d)
		{
			// ---- 0x19b and 0x19c, the right output, on the step that writes the left line ----
			double const y = output(c, st, OUTPUTS[1]);
			if (st == 0x19b)
			{
				// The left line's write rides on the right channel's dry word: the slot 0x66 takes the
				// data 0x19a latched, or this step's own result where there is none, as step() has it.
				double v = yss236_core::clip(y, yss236_core::SAT);
				if (c.haswl)
				{
					v = c.wlatch;
					c.haswl = false;
				}
				write_delay(c, st, v);
			}
			continue;
		}

		// ---- 0x19d to 0x19f, what goes into the right line, whose write is its own last step ----
		line(c, st, LINES[1]);
	}
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m06(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
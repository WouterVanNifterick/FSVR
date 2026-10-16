// fs1r/native/dly_m00.cpp - VOP3-2, the reverb delay window's first program: its 72 steps.
//
// rom::REVERB_DELAY_PROGRAMS[0], the program the window holds for reverb types 0 to 12 (No Effect,
// Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate, White Room, Tunnel, Basement, Canyon). Types 13 to
// 16 of the reverb block (Delay L,C,R, Delay L,R, Echo, Cross Delay) run their programs here instead,
// under the keys dly-m08 to dly-m11, and fx_delay.cpp is the host that feeds those. Under types 0 to
// 12 no DELAY_ROUTES entry matches, so nothing rewrites the window's constants or delay offsets when a
// parameter moves: they are the reverb type's preset (rom::REVERB_DELAY_0_BLOCKS) and they are the
// same thirteen times over, which a read of c.coef[] and c.slot[] under each type confirms. The code
// reads them where they live all the same, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in three groups the comments below open each of:
//
//   0x110-0x126  the send into the block and the host's four tone sections, rev::DELAY_LPF,
//                rev::DELAY_HPF and their two copies, three steps each and each leaving a
//                coefficient in t[12] or t[13]. Three of the twenty steps, 0x113, 0x117 and 0x11b,
//                read the block's own three lines (the slots 0x44, 0x45 and 0x46, in the reverb's
//                area), and those reads go out in m[22], m[23] and m[24] five steps later.
//   0x127-0x14d  the window's own line, read as four pairs with the index added, each read taken up
//                by a cell five steps later and each pair differenced and scaled by t[9], t[11], t[11]
//                or t[10] onto the running value. Those three coefficients and the three index loads
//                are all zero under every type that loads this program, so the four products are zero
//                and what the group adds is the running value on its own.
//   0x14e-0x157  three more reads of the block's own area, published in m[25], m[26] and m[27], the
//                two writes of the window's line (the value 0x146 latched, and 0x157's own result),
//                and the block's output, the cell m[41] to the serial out at 0x156.
//
// The registers: r[123] and r[2] the two inputs of the filter groups, r[124], r[126] and r[1] their
// three states, r[127] the first group's output, r[119], r[121], r[122] and r[120] the four tap sums,
// r[13] and r[14] the running value scaled out. The cells: m[12] the send, written by a window outside
// this one, m[56] to m[63] the eight tap captures, m[22] to m[27] what this window publishes, m[8] to
// m[10] the three LFO values it hands on, m[1] and m[5] the two serial inputs. The t registers:
// t[12] and t[13] the two filter coefficients, t[9], t[10] and t[11] the three tap coefficients.
//
// A delay RAM read is made at one step and taken up by a cell 5 to 11 steps later, which is what
// yss236_core::captured() returns and what an msrc of 4 queues. A multiply by a constant of zero with
// the adder on the running value is the chip's way of writing one register to another, and a step whose
// only job is to clear the running value does the same with the adder off; both appear below and both
// are what step() computes.
//
// Only the fields the program fixes are written out below, and the two runs of steps it repeats, the
// four tone sections and the four delay taps, are a table and a small function each rather than a case
// a step. Everything the host rewrites is read where it lives, as step() reads it: every constant from
// c.coef[st] on the pass and every delay offset through c.addr(), off the c.slot[] entry the host last
// wrote, so no number the firmware chose is folded in here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled
// out by name rather than a step at a time.
//
// vop3_native_check dly-m00: identical, 13 types, 124800 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step does before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// The read of the delay line, which is this window's own line under the index the curve steps loaded
// rather than at the ring pointer. The offset is the step's own slot over four, which is where step()
// takes it from, and the index is floored into an int, as cur_idx() leaves it.
void read_delay_indexed(yss236_core& c, int st, bool older)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, c.cur_idx() + (older ? 1 : 0), false)]);
}

// What a step of this program does with the delay line and the cells on its way past the arithmetic.
// `line` is the read it makes of the delay RAM and `cell` the cell it moves into, `m` being that cell: a
// capture of a read made five to eleven steps earlier, one of the page's LFO values, one of the two
// serial inputs, or the cell it hands to the serial output.
enum line_t { LINE_NONE, LINE_READ, LINE_READ_IDX, LINE_READ_IDX_OLDER };
enum cell_t { CELL_NONE, CELL_CAPTURE, CELL_LFO, CELL_SERIAL_L, CELL_SERIAL_R, CELL_OUT };

struct Decor { line_t line = LINE_NONE; cell_t cell = CELL_NONE; int m = 0; };

// Apply the decor, which comes before the term, because step() reads the delay RAM and moves a cell
// before it does any arithmetic.
void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.line)
	{
		case LINE_READ:           read_delay(c, st); break;
		case LINE_READ_IDX:       read_delay_indexed(c, st, false); break;
		case LINE_READ_IDX_OLDER: read_delay_indexed(c, st, true); break;
		case LINE_NONE: break;
	}
	switch (d.cell)
	{
		case CELL_CAPTURE:  take_capture(c, d.m, st); break;
		case CELL_LFO:      take_lfo(c, d.m, st); break;
		case CELL_SERIAL_L: take_serial(c, d.m, st, 0); break;
		case CELL_SERIAL_R: take_serial(c, d.m, st, 1); break;
		case CELL_OUT:      serial_out(c, st, d.m); break;
		case CELL_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// What a step computes
// ---------------------------------------------------------------------------------------------

// What a step does with the delay line at the end of it. WRITE_LATCHED is 0x153, a class-0 step, which
// line_write() below runs rather than a table row.
enum delay_t { NO_WRITE, WRITE_LATCHED, WRITE_OWN, SET_LATCH };

// What a step does that its arithmetic does not say. `tput` is the t register the step writes, 0 for
// the sixty-six that write none, and `write` what it does with the delay line. `load` is class 1, where
// the step's word is the value rather than a coefficient applied to one, and `unity` is class 3, where
// the multiplier passes its operand through. Every field defaults to what a step does not use, so a row
// says only what is different about its step.
struct Side
{
	int tput = 0;
	delay_t write = NO_WRITE;
	bool load = false;
	bool unity = false;
};

// One step. `sr` is the register it reads and `sm` the cell it latches, which is also the multiplier's
// second input wherever it is not zero, `dr` the register it writes and `treg` the t register the
// multiplier reads as its word in place of the step's own constant. `add` is what the adder adds to the
// product, `route` the scale its result takes and `clamp` the clamp, and `st` is the step itself, which
// is what the index below keys each row on.
struct Word
{
	int st;
	int sr, sm, dr;
	add_t add;
	route_t route;
	clamp_t clamp;
	int treg;
	Decor decor;
	Side side;
};

// One step, at the step the loop is on.
void word_step(yss236_core& c, int st, Word const& w)
{
	decor(c, st, w.decor);
	if (w.side.load)
	{
		// A class-1 step, where the word is the value itself. The t register it leaves is a 1.15 word
		// of it, which is where the tone sections' filter coefficients come from.
		load(c, {.st = st, .sr = w.sr, .sm = w.sm, .add = w.add, .route = w.route, .clamp = w.clamp, .dr = w.dr});
		if (w.side.tput) t_put(c, st, w.side.tput, c.acc);
		return;
	}
	// A class-2 or class-3 term. The three steps of this program that write a t register write the
	// fraction of their result below one delay sample rather than a 1.15 word, and those are the three
	// that load the delay address index as well, which is what the indexed reads take.
	double const y = run(c, {.st = st, .sr = w.sr, .sm = w.sm, .m2 = w.sm ? M2_CELL : M2_REG, .unity = w.side.unity, .treg = w.treg, .add = w.add, .route = w.route, .clamp = w.clamp, .dr = w.dr, .wdata = w.side.write != NO_WRITE, .write = w.side.write == WRITE_OWN});
	if (w.side.tput) { t_put_fraction(c, st, w.side.tput, y); load_index(c, y); }
}

// ---------------------------------------------------------------------------------------------
// The host's four tone sections, 0x111 to 0x11d, three steps each
// ---------------------------------------------------------------------------------------------

// One of the four three-word sections the host's designFilter() writes on the send: rev::DELAY_LPF and
// rev::DELAY_HPF and their two copies, rev::DELAY_LPF_COPY and rev::DELAY_HPF_COPY. `st` is the
// section's first step and `nsteps` how many it runs, always the three of its two coefficients and its
// state, which are the steps in the order the program runs them. The idiom at 0x114, 0x119 and 0x121 is
// the chip's: 2 * |x| - 128, which is a register's value centred on zero, read back as a t multiplier a
// third of the way up (t_write divides by 128).
struct Section { int st, nsteps; Word word[3]; };

// The columns below are Word's, in the order it declares them. The steps around the sections are 0x110,
// which takes the send in, 0x117, which reads the block's second line, and 0x11e to 0x126, the two tone
// groups' states and the tap sums the sections close into. WORDS below holds those and the rest.
constexpr Section SECTIONS[4] = {
    { 0x111, 3, {         { 0x111, 0, 12, 123, ADD_OFF, ROUTE_16, CLAMP_WRAP, 0, {LINE_NONE, CELL_SERIAL_R, 5}, {0, NO_WRITE, false, false} },
                        { 0x112, 0, 0, 125, ADD_ACC, ROUTE_2, CLAMP_WRAP, 0, {}, {0, NO_WRITE, true,  false} },
                        { 0x113, 124, 0, 0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0, {LINE_READ, CELL_NONE, 0}, {0, NO_WRITE, false, true } } } },   // rev::DELAY_LPF,
    { 0x114, 3, {         { 0x114, 0, 0, 124, ADD_ACC, ROUTE_2, CLAMP_SAT, 0, {}, {12, NO_WRITE, true,  false} },
                        { 0x115, 0, 12, 1, ADD_CELL, ROUTE_2, CLAMP_WRAP, 0, {}, {0, NO_WRITE, true,  false} },
                        { 0x116, 123, 0, 2, ADD_REG, ROUTE_2, CLAMP_WRAP, 0, {}, {0, NO_WRITE, true,  false} } } },   // rev::DELAY_HPF,
    { 0x118, 3, {         { 0x118, 1, 0, 0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0, {LINE_NONE, CELL_CAPTURE, 22}, {0, NO_WRITE, false, true } },
                        { 0x119, 0, 0, 1, ADD_ACC, ROUTE_2, CLAMP_SAT, 0, {}, {13, NO_WRITE, true,  false} },
                        { 0x11a, 127, 0, 127, ADD_OFF, ROUTE_1, CLAMP_SAT, 12, {}, {0, NO_WRITE, false, false} } } },   // rev::DELAY_LPF_COPY,
    { 0x11b, 3, {         { 0x11b, 0, 12, 126, ADD_CELL, ROUTE_2, CLAMP_WRAP, 0, {LINE_READ, CELL_NONE, 0}, {0, NO_WRITE, true,  false} },
                        { 0x11c, 124, 0, 0, ADD_OFF, ROUTE_16, CLAMP_SAT, 0, {LINE_NONE, CELL_CAPTURE, 23}, {0, NO_WRITE, false, false} },
                        { 0x11d, 127, 0, 0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0, {}, {0, NO_WRITE, false, false} } } },   // rev::DELAY_HPF_COPY
};

// The section a step is one of the three words of, -1 where it is none. None of the four runs into
// another, so the index is built rather than searched, and a step's place in its section is what its own
// first step tells it.
constexpr auto SECTIONS_AT = index_by_span(SECTIONS);

// One word of one of the four tone sections.
void tone_word(yss236_core& c, int st, Section const& s, int k)
{
	word_step(c, st, s.word[k]);
}

// ---------------------------------------------------------------------------------------------
// The four delay taps, 0x137 to 0x14d and the two steps of each
// ---------------------------------------------------------------------------------------------

// One tap of the window's line: two steps that difference two reads of the line a word apart through
// the tap's coefficient. `st` is the pair's first step and `nsteps` how long it is, always two, one for
// each of the reads. `coef` is the t register that coefficient, `lo` and `hi` the cells the two captures
// of the reads land in, `out` the register the pair closes into, `read` what the first step reads of the
// line and `capture` what the second takes up.
struct Tap { int st, nsteps; int coef, lo, hi, out; Decor read, capture; };

// The four taps in the order the program runs them. The coefficients are t[9] at 0x124, t[11] at 0x12b
// (rev::DELAY_CURVE[0]) and t[10] at 0x13a, and the index the reads take is loaded by those same three
// steps. Under the thirteen reverb types that load this program all three running values there are zero,
// so all three coefficients and all three indices are zero and every product below is zero: the reads
// still happen, the cells still move, and the four sums are the running value on its own.
constexpr Tap TAPS[4] = {
    { 0x137, 2,  9, 56, 57, 119, {LINE_READ_IDX,       CELL_NONE,  0}, {LINE_NONE, CELL_CAPTURE, 61} },
    { 0x13b, 2, 11, 60, 61, 121, {LINE_READ_IDX_OLDER, CELL_NONE,  0}, {LINE_NONE, CELL_CAPTURE, 62} },
    { 0x147, 2, 11, 62, 63, 122, {LINE_READ,           CELL_LFO,   10}, {LINE_NONE, CELL_CAPTURE, 59} },
    { 0x14c, 2, 10, 58, 59, 120, {LINE_NONE,           CELL_CAPTURE, 25}, {LINE_NONE, CELL_NONE,  0} },
};

// The tap a step is one of the two halves of, -1 where it is none. The four pairs are apart and none
// runs into another, so the index is built rather than searched, and a step's place in its pair is what
// the pair's own first step tells it.
constexpr auto TAPS_AT = index_by_span(TAPS);

// One half of a tap pair, k = 0 for the first. The first half differences the first read from itself,
// y = t * lo - lo, which leaves the running value and only wraps. The second differences the other read
// from that, y = t * hi - acc, and closes the pair into the tap's register, saturated. The sub bit is
// the chip's way of differencing the two reads rather than adding them, and both halves scale the cell
// and add it, which is why the first adder is the cell and the second the running value.
void tap_half(yss236_core& c, int st, Tap const& t, int k)
{
	decor(c, st, k ? t.capture : t.read);
	if (!k)
		run(c, {.st = st, .sm = t.lo, .m2 = M2_CELL, .treg = t.coef, .add = ADD_CELL, .sub = true});
	else
		run(c, {.st = st, .sm = t.hi, .m2 = M2_CELL, .treg = t.coef, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = t.out});
}

// ---------------------------------------------------------------------------------------------
// The two steps that stand alone
// ---------------------------------------------------------------------------------------------

// The window's line at 0x153, the program's only class-0 step: it computes nothing and writes the delay
// RAM whatever 0x146 latched, which is why 0x157 carries the flag itself and writes its own result.
// step() moves the x latch before it writes, so this does too.
void line_write(yss236_core& c, int st)
{
	idle(c);
	write_delay_latched(c, st);
}

// The block's output at 0x156: the cell m[41] to this page's serial out, and the running value takes the
// step's word on r[92] with the adder off. The step latches m[41] as its cell operand even though the
// multiplier scales the register, and that latch is what the next few steps shift along.
void block_output(yss236_core& c, int st)
{
	serial_out(c, st, 41);
	run(c, {.st = st, .sr = 92, .sm = 41});
}

// ---------------------------------------------------------------------------------------------
// Every other step of the program, in step order
// ---------------------------------------------------------------------------------------------

// The columns below are Word's, in the order it declares them, and the rest of the program's
// arithmetic. 0x110 takes the send in and 0x117 reads the block's second line; 0x11e to 0x126 are the
// two tone groups' states, the filter coefficients they leave in the t file and the four tap sums;
// 0x127 to 0x136 open the window's line, set the first and second tap coefficients and close the second
// tap; 0x139 to 0x146 open the third and close the fourth and hold what the line write takes; 0x14e to
// 0x157 scale the running value out and write the line's second word. The reads at 0x147, 0x14b and
// 0x14f go out in m[25], m[26] and m[27], taken up at 0x14c, 0x150 and 0x154.
constexpr Word WORDS[] = {
    //  st     sr  sm  dr    add      route     clamp       treg  decor                              side
    {  0x110,  0, 12, 124, ADD_CELL, ROUTE_2, CLAMP_WRAP, 0,    {LINE_NONE, CELL_SERIAL_L,     1}, { 0, NO_WRITE,   true,  false} },
    {  0x117, 124, 0, 127, ADD_OFF, ROUTE_1, CLAMP_SAT, 12,     {LINE_READ, CELL_LFO,         8}, { 0, NO_WRITE,   false, false} },
    {  0x11e,  0, 0, 124, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,                                   {}, { 0, NO_WRITE,   true,  false} },
    {  0x11f,  1, 0, 119, ADD_OFF, ROUTE_1, CLAMP_SAT, 13,                                   {}, { 0, NO_WRITE,   false, false} },
    {  0x120, 126, 0,  0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0,     {LINE_NONE, CELL_CAPTURE,    24}, { 0, NO_WRITE,   false, true } },
    {  0x121,  0, 0, 126, ADD_ACC, ROUTE_2, CLAMP_SAT, 0,                                   {}, {12, NO_WRITE,   true,  false} },
    {  0x122, 119, 0, 119, ADD_OFF, ROUTE_1, CLAMP_SAT, 13,                                   {}, { 0, NO_WRITE,   false, false} },
    {  0x123, 125, 0,  0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0,                                   {},                             {} },
    {  0x124, 124, 0,  0, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,                                   {}, { 9, NO_WRITE,   false, false} },
    {  0x125,  1, 0,  0, ADD_OFF, ROUTE_16, CLAMP_SAT, 0,                                   {},                             {} },
    {  0x126, 119, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x127,  0, 0,  1, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,     {LINE_READ_IDX, CELL_LFO,     9}, { 0, NO_WRITE,   true,  false} },
    {  0x128, 126, 0, 124, ADD_OFF, ROUTE_1, CLAMP_SAT, 12,                                   {}, { 0, NO_WRITE,   false, false} },
    {  0x129, 123, 0, 127, ADD_REG, ROUTE_2, CLAMP_WRAP, 0,                                   {}, { 0, NO_WRITE,   true,  false} },
    {  0x12a,  2, 0,  0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0,                                   {},                             {} },
    {  0x12b,  1, 0,  0, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,  {LINE_READ_IDX_OLDER, CELL_NONE, 0}, {11, NO_WRITE,   false, false} },
    {  0x12c, 124, 0, 124, ADD_OFF, ROUTE_1, CLAMP_SAT, 12,     {LINE_NONE, CELL_CAPTURE,    56}, { 0, NO_WRITE,   false, false} },
    {  0x12d,  3, 0, 87, ADD_OFF, ROUTE_2, CLAMP_SAT, 0,                                   {},                             {} },
    {  0x12e, 88, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x12f, 87, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,     {LINE_READ_IDX, CELL_NONE,    0},                             {} },
    {  0x130, 87, 0, 88, ADD_ACC, ROUTE_2, CLAMP_SAT, 0,     {LINE_NONE, CELL_CAPTURE,    57},                             {} },
    {  0x131, 89, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x132, 88, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x133, 88, 0, 89, ADD_ACC, ROUTE_4, CLAMP_SAT, 0,  {LINE_READ_IDX_OLDER, CELL_NONE, 0},                             {} },
    {  0x134, 126, 0,  0, ADD_OFF, ROUTE_16, CLAMP_SAT, 0,     {LINE_NONE, CELL_CAPTURE,    60},                             {} },
    {  0x135, 124, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x136,  0, 0, 126, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,                                   {}, { 0, NO_WRITE,   true,  false} },
    {  0x139, 127, 0,  0, ADD_OFF, ROUTE_1, CLAMP_ABS, 0,                                   {},                             {} },
    {  0x13a, 126, 0,  0, ADD_ACC, ROUTE_1, CLAMP_ZERO, 0,                                   {}, {10, NO_WRITE,   false, false} },
    {  0x13d,  4, 0, 90, ADD_OFF, ROUTE_2, CLAMP_SAT, 0,                                   {},                             {} },
    {  0x13e, 91, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x13f, 90, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,     {LINE_READ_IDX, CELL_NONE,    0},                             {} },
    {  0x140, 90, 0, 91, ADD_ACC, ROUTE_2, CLAMP_SAT, 0,     {LINE_NONE, CELL_CAPTURE,    63},                             {} },
    {  0x141, 92, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x142, 91, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x143, 91, 0, 92, ADD_ACC, ROUTE_4, CLAMP_SAT, 0,  {LINE_READ_IDX_OLDER, CELL_NONE, 0},                             {} },
    {  0x144, 119, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,     {LINE_NONE, CELL_CAPTURE,    58},                             {} },
    {  0x145, 89, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x146, 92, 0,  0, ADD_ACC, ROUTE_1, CLAMP_SAT, 0,                                   {}, { 0, SET_LATCH,  false, false} },
    {  0x149, 119, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x14a, 121, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x14b, 122, 0, 124, ADD_ACC, ROUTE_1, CLAMP_SAT, 0,    {LINE_READ, CELL_NONE,         0},                             {} },
    {  0x14e, 89, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x14f, 124, 0, 13, ADD_ACC, ROUTE_4, CLAMP_SAT, 0,    {LINE_READ, CELL_NONE,         0},                             {} },
    {  0x150, 121, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,     {LINE_NONE, CELL_CAPTURE,    26},                             {} },
    {  0x151, 122, 0,  0, ADD_ACC, ROUTE_1, CLAMP_WRAP, 0,                                   {},                             {} },
    {  0x152, 120, 0, 125, ADD_ACC, ROUTE_1, CLAMP_SAT, 0,                                   {},                             {} },
    {  0x154, 92, 0,  0, ADD_OFF, ROUTE_1, CLAMP_WRAP, 0,     {LINE_NONE, CELL_CAPTURE,    27},                             {} },
    {  0x155, 125, 0, 14, ADD_ACC, ROUTE_4, CLAMP_SAT, 0,                                   {},                             {} },
    {  0x157, 120, 0,  0, ADD_ACC, ROUTE_1, CLAMP_SAT, 0,                                   {}, { 0, WRITE_OWN,  false, false} },
};

// The word of a step, -1 where the table holds none. The fifty rows are in step order and no two of
// them are the same step, so the index is built once and a step reaches its word rather than walking the
// table and stopping where the walk goes past it.
constexpr auto WORDS_AT = index_by_step(WORDS);

// The run of steps 0x110 to 0x157, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's. A step looks itself up in the three tables and the
// two single steps the program is made of, so the 72 read as the twenty of the two repeated runs, the
// two that stand alone and the fifty of the table, rather than as a switch of cases.
void block_0x110(yss236_core& c)
{
	for_steps<0x110, 0x158>(c, [&](yss236_core& c, auto st)
	{

		// ---- the send and the host's four tone sections ----
		bool done = false;
		if (int i = SECTIONS_AT[st]; i >= 0) { tone_word(c, st, SECTIONS[i], st - SECTIONS[i].st); done = true; }

		// ---- the four delay taps ----
		if (!done)
			if (int i = TAPS_AT[st]; i >= 0) { tap_half(c, st, TAPS[i], st - TAPS[i].st); done = true; }

		// ---- the two steps that stand alone: the line write and the block's output ----
		if (!done && st == 0x153) { line_write(c, st); done = true; }
		if (!done && st == 0x156) { block_output(c, st); done = true; }
		if (done) return;

		// ---- every other step, which WORDS holds in step order ----
		if (int i = WORDS_AT[st]; i >= 0) word_step(c, st, WORDS[i]);
		});
}

}  // namespace

// One run of the reverb delay window's steps, exactly as yss236_core::step() would run them. The
// program is one run; a range no host builds falls back to the interpreter.
void dly_m00(yss236_core& c, int from, int to)
{
	if (from == 0x110 && to == 0x158) { block_0x110(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
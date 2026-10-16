// effects/ins_m03.cpp - VOP3-2, the insertion window's program for type msb 03: its 96 steps.
//
// rom::INSERTION_PROGRAMS[3], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 3: ins-37 ER 1, ins-38 ER 2, ins-39 Gate Rev and
// ins-40 Revrs Gate, all four programmed by EffectHost::earlyReflections() in fx_reverb.cpp. The same
// 96 words run under every one of them; what the type changes is which words the host writes, the
// pattern row erPattern() reads (six entries further on for Gate Rev, eight for Reverse Gate), and
// how many of the twenty gains it leaves at zero. Nothing here knows which one is running.
//
// Every constant the program reads is read from c.coef[st] on the pass and every delay offset from
// the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here: the
// program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own. The program writes no t register and reads none, so the t
// file is left alone.
//
// Unlike ins-m02, which touches no delay RAM at all, this program is most of a delay line: eleven
// reads of ten tap slots plus one allpass point, and nine writes. What it computes, in the groups the
// comments open each of. The names are the host's, from rom_tables.cpp and fx_reverb.cpp.
//   0x1a0-0x1b3  the filters. 0x1a0 and 0x1a1 mix r[7] and r[8] into r[67] at the ROM word 0x2000 each
//                and twice that, so the high pass and the low pass run on one mono signal and the two
//                channels are told apart by their dry signals and their tap lines further on.
//                ins::ER_HPF's five words are the biquad over r[67] to r[70] (0x1a2 to 0x1a6, b0 at
//                0x1a6, b1 at 0x1a3, b2 at 0x1a2, -a1 at 0x1a4, -a2 at 0x1a5), ins::ER_LPF_2's the biquad
//                over r[71] to r[74] (0x1ac to 0x1b0), and ins::ER_LPF's three words the one-pole High
//                Damp filter (0x1b1 to 0x1b3) on the twentieth tap. Between them the steps read the
//                first five right-channel taps, each read landing in a cell five steps later.
//   0x1b4-0x1bb  the first two of the four diffusion rows, ins::ER_ROW_STEPS' first eight words: r[73],
//                the low pass's output, and the cells m[18] and m[19] into the slots 0x6d and 0x6e
//                (ins::ER_ROW_SLOTS[0] and [1]). Those two cells are the rows' own allpass points,
//                ins::ER_ROW_SLOTS[2] and [3], the slots 0x2e and 0x2f, which the base program's steps
//                0x0be and 0x0c2 capture: each row is fed its own output delayed, which is the pair of
//                allpasses the FB Level then feeds the delay line with. Both rows end at r[119], so
//                the second overwrites the first three steps later.
//   0x1bc-0x1c5  the ten left-channel gains, ins::ER_STEPS[0] to [9], on the cells m[22] to m[31].
//                Those cells are not this window's: the base program's steps 0x118 to 0x194 capture
//                them from ins::ER_SLOTS' even entries, the slots 0x44 to 0x63 and 0x64. The sum lands
//                in m[43] and is written into the slot 0x71 by 0x1c7.
//   0x1c6-0x1d1  that write, and the reads of the eighth, ninth and tenth right-channel taps. Seven
//                of these steps are class 0 with no memory op and no ALU: they count and nothing else.
//   0x1d2-0x1db  the ten right-channel gains, ins::ER_STEPS[10] to [19], on the cells m[56] to m[63]
//                and back on m[56] and m[57], which this program fills itself from the slots 0x68 to
//                0x75 (ins::ER_SLOTS' odd entries). The sum lands in m[44] and 0x1db writes it into
//                the slot 0x76. erPattern() leaves ins::ER_STEPS[19] at zero, so the tenth right tap
//                is never heard.
//   0x1dc-0x1df  the two sums with their allpass points, ins::ER_DIFF_SLOTS. earlyReflections()'
//                DIFFUSION case puts the slot 0x65 at delay[0x76] + d + 1 and the slot 0x75 at
//                delay[0x71] + d + 1, so each sum picks the other one's up d + 1 samples later: r[121]
//                is the left sum and r[120] the right.
//   0x1e0-0x1ed  the last two diffusion rows, ins::ER_ROW_STEPS' other eight words: an allpass and a
//                line for each sum, the slots 0x77 and 0x79 for r[120] and 0x78 and 0x7a for r[121]
//                (ins::ER_ROW_SLOTS[4] to [7]), which is what the two registers read back at 0x1e6
//                and 0x1ea.
//   0x1ee-0x1ef  the delay line's input. r[119], the diffusion rows' output, goes in at unity, and
//                ins::ER_FEEDBACK[0], the FB Level word, scales r[75], the High Damp filter's output on
//                the twentieth tap. The write is to the slot 0x7b, ins::ER_SLOTS[21], which is where
//                earlyReflections()'s erPattern() measures every tap's position from: the ten right
//                taps sit the initial delay plus the room size past it, so the line is the tail the
//                taps are read from.
//   0x1f0-0x1f7  the dry and wet mix, ins::ER_WET's four words: r[7] and r[8] at [0] and [2] against
//                r[121] and r[120] at [1] and [3], into r[17] and r[18], the two outputs the base
//                program reads back. 0x1f3 and 0x1f7 write the slots 0x7c and 0x7d with their
//                coefficients, which no parameter writes and which are zero, so both slots take zero
//                every pass; nothing reads them.
//   0x1f8-0x1ff  eight steps that count and nothing else.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] to r[70] the
// high pass, r[71] to r[74] the low pass, r[73] its output; r[75] the High Damp filter's output, the
// delay line's input; r[119] the two first diffusion rows' output; r[120] the right sum, r[121] the
// left; r[17] and r[18] the two outputs.
//
// TAPS below is the program's whole contact with the delay RAM read side: thirteen reads and the
// thirteen steps that take them up into a cell, every one exactly five steps later, which is CAPMIN,
// the nearest the hardware allows a capture to its transfer. No step of this program carries both a
// read and a capture, so the table's two halves never land on the same step and the order between them
// never matters. The rest of the program is five tables, one for each shape the steps repeat: the two
// biquads' coefficient words, the four diffusion rows, the twenty tap gains, the two sums and the two
// output mixes. The ranges between them are the padding the schedule spends, and every step of those
// is idle(), which is what carries its x latch and nothing else.
//
// The old model in vop3_effects.h has a case for these four types and is a loop of twelve taps off one
// line with a hand-picked gain envelope per tap, rising for Reverse Gate and decaying for Gate Reverb.
// It has no delay line's worth of slots, no diffusion rows, no feedback word and no High Damp filter,
// and where the two differ the program above is what runs. It is no reference for any number here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check ins-m03: identical, 4 types, 38400 native passes.
#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The registers and cells the header names, so the arithmetic below can say what it reads
// ---------------------------------------------------------------------------------------------

enum
{
	SIG_L = 7,        // the two signals the base program leaves this window, and the two outputs it
	SIG_R = 8,        // reads back
	OUT_L = 17,
	OUT_R = 18,
	MIX = 67,        // the mono mix both filters run on, and the high pass's own x[-1]
	MIX_X2 = 68,     // the high pass's x[-2]
	HPF_OUT = 69,    // the high pass's output, its own y[-1], and the low pass's input
	HPF_Y2 = 70,     // the high pass's y[-2]
	LPF_X1 = 71,     // the low pass's input delay, its own x[-1]
	LPF_X2 = 72,     // its x[-2]
	LPF_OUT = 73,    // its output, its own y[-1], and the first diffusion row's input
	LPF_Y2 = 74,     // its y[-2]
	DAMP = 75,       // the High Damp filter's output, which is also the delay line's input
	DIFF = 119,      // the first two diffusion rows' output
	SUM_R = 120,     // the right channel's sum, and
	SUM_L = 121,     // the left's: the two sums the mixes close into the outputs
	TAP20 = 32,      // m[32], the twentieth tap, which the base program's step 0x198 captured out of
	TAP20_Y = 42,    // the slot 0x64, and m[42], where 0x1b2 keeps the m[32] of a pass earlier
	LEFT_TOTAL = 43, // m[43] and m[44], where the two channels' twenty-gain sums land
	RIGHT_TOTAL = 44,
	CROSS_R = 33,    // m[33] and m[56], the cell each of the two sums picks the other one's up
	DIFF_L = 56,     // through: m[56] is also the first right-channel tap's cell
};

// ---------------------------------------------------------------------------------------------
// The delay RAM read side: the thirteen reads and the cells that take them up
// ---------------------------------------------------------------------------------------------

// One read of the line and the cell that takes it up. The cell takes the read at `st + CAPMIN`,
// because a capture walks back to the latest transfer of the pass made CAPMIN to 11 steps earlier and
// five is the nearest the hardware allows (s40c), so no read and its capture are ever adjacent. Every
// read below is exactly five steps ahead of the capture that lands it, which is what lets a cell be
// reused three taps later without one tap landing in the cell of the one before it. `st` is the step the
// read is made on, which is what index_by_step keys on.
struct Tap { int st; int cell; };

// The ten right-channel taps of ins::ER_SLOTS' odd entries, the allpass point earlyReflections()'
// DIFFUSION case puts beside the left sum, and the two slots the last two diffusion rows write. The
// cells are m[56] to m[63] and back again, eight cells for thirteen reads, and every block below that
// scales one of them reads its tap from this table and not from a delay read of its own.
constexpr Tap TAPS[13] = {
    //  st    the cell   and what that slot holds
    {  0x1a3, 56 },   // ins::ER_SLOTS[1], the first right tap, read on the high pass's b1 step
    {  0x1a7, 57 },   // and [3], the second, on the first of the two zero words after the high pass
    {  0x1ab, 58 },   // and [5], the third, on the low pass's input step
    {  0x1af, 59 },   // and [7], the fourth, on the low pass's -a2 step
    {  0x1b3, 60 },   // and [9], the fifth, on the High Damp filter's closing step
    {  0x1bf, 61 },   // and [11], the sixth, on the fourth of the left gains
    {  0x1c3, 62 },   // and [13], the seventh, on the eighth of the left gains
    {  0x1cb, 63 },   // and [15], the eighth, in the padding after the left sum's write
    {  0x1cf, 56 },   // and [17], the ninth, in that padding too, so m[56] takes a second tap
    {  0x1d3, 57 },   // and [19], the tenth, on the second of the right gains
    {  0x1d7, 56 },   // ins::ER_DIFF_SLOTS[3], the slot 0x75, the left sum a Diffusion delay back
    {  0x1df, 57 },   // ins::ER_ROW_SLOTS[7], the slot 0x77, the right sum's allpass point
    {  0x1e3, 58 },   // and [5], the slot 0x78, the left sum's allpass point
};

// The step each read is made on: thirteen rows the step loop walked once per step of the run, and the
// index is the same on every pass.
constexpr auto TAPS_AT = index_by_step(TAPS);

// The step each read's capture lands in, CAPMIN after the read. A row's two steps are not neighbours,
// so neither index helper in steps.h can build this one from the table, and it is the same table and
// the same constant the scan used.
constexpr auto tap_capture_at()
{
	std::array<int16_t, yss236_core::STEPS> ix{};
	for (int16_t& i : ix) i = -1;
	for (int k = 0; k < int(sizeof TAPS / sizeof *TAPS); k++)
		ix[TAPS[k].st + yss236_core::CAPMIN] = int16_t(k);
	return ix;
}

constexpr auto TAP_CAPTURE_AT = tap_capture_at();

// The three serial inputs the window publishes and reads nothing of: the two banks of page 13 and the
// left bank of page 14. Each take is on a step that computes something else, and each comes before
// that step's arithmetic, because step() moves a cell before it does anything else.
enum serial_t { SERIAL_L, SERIAL_R };

struct Serial { int st, cell; serial_t bank; };

constexpr Serial SERIALS[3] = {
	{ 0x1a4, 3, SERIAL_L },    // page 13's left input, on the high pass's -a1 step
	{ 0x1a5, 7, SERIAL_R },    // and its right, on the high pass's -a2 step
	{ 0x1c2, 4, SERIAL_L },    // page 14's left input, on the seventh of the left gains
};

// The step each serial take is made on, so the step loop does not walk the three to find its own.
constexpr auto SERIALS_AT = index_by_step(SERIALS);

// ---------------------------------------------------------------------------------------------
// The two biquads: ins::ER_HPF and ins::ER_LPF_2
// ---------------------------------------------------------------------------------------------

// What one step of a section computes, named after the coefficient designFilter() wrote for it: b0,
// b1 and b2 are the numerator's and -a1 and -a2 the denominator's. The two parts whose name ends in
// the register they carry are the steps that close a coefficient and shift a section's z^-1 with it,
// which is how a section holds x[-2] and y[-2] in a register apiece without a cell of its own. The
// last three are not coefficients at all: they are the steps the schedule spends moving the input
// delay along and writing the section's input register.
enum part_t
{
	B2,        // b2 on x[-2], the adder off: the step that opens the section's sum
	B1,        // b1 on x[-1]
	A1,        // -a1 on y[-1]
	A1_X2,     // -a1 on y[-1], and the copy that shifts x[-2] along behind it
	A2_Y2,     // -a2 on y[-2], and the copy that keeps y[-2] itself a pass along
	B0,        // b0 on x[0], closing the section into its output with the section's route
	X2_FETCH,  // the input delay register's own fetch, which is what the next step copies from
	X2_COPY,   // the copy that shifts x[-2] along, on a step of its own
	IN,        // the section's input step, which writes the input delay register
	ZERO_Y2,   // a zero word on the section's y[-2], which opens nothing
	ZERO_OUT,  // and a zero word on its output, which leaves the running value as it stands
};

// One order-2 section in direct form I. `st` is its first step and `nsteps` how many steps it runs,
// which is what index_by_span keys on. The five coefficient steps are always in the order b2, b1, -a1,
// -a2, b0, and the steps either side of them are the ones this section's own registers need. `x1` and
// `x2` are the input's two delays, `y1` and `y2` the output's, `in` the register the section's input
// arrives in, and `out` the one b0 writes, which is what the next stage reads. The output register is
// the section's own y1 in both cases, which is why y1 and out are named apart: one is what -a1 reads
// and the other is what b0 writes, and the program has them as the same register.
struct Section
{
	int st, nsteps;
	int x1, x2, y1, y2, in, out;
	step::route_t route;
	part_t part[8];
};

// ins::ER_HPF's input register is the mono mix itself, and the write to it is 0x1a1, three steps
// before the section's own words. The register therefore holds x[-1] for the whole of b1 and x[0] for
// b0 with nothing to copy, so only x[-2] needs a register of its own and its shift rides on the -a1
// step. ins::ER_LPF_2's input comes out of the high pass and is written by a step of its own, so both
// of its input delays need registers and both shifts sit in front of the five words.
constexpr Section SECTIONS[2] = {
	//  st    n  x1      x2      y1      y2      in       out      route      and the section's steps
	{ 0x1a2, 7, MIX,    MIX_X2, HPF_OUT, HPF_Y2, MIX,    HPF_OUT, ROUTE_2,
	  { B2, B1, A1_X2, A2_Y2, B0, ZERO_Y2, ZERO_OUT } },
	{ 0x1a9, 8, LPF_X1, LPF_X2, LPF_OUT, LPF_Y2, HPF_OUT, LPF_OUT, ROUTE_4,
	  { X2_FETCH, X2_COPY, IN, B2, B1, A1, A2_Y2, B0 } },
};

// The section a step belongs to. The two do not overlap, so the index gives a step its own section
// rather than leaving it to walk the two.
constexpr auto SECTIONS_AT = index_by_span(SECTIONS);

// The section a step belongs to, or null when it belongs to none.
Section const* section_for(int st) { return SECTIONS_AT[st] < 0 ? nullptr : &SECTIONS[SECTIONS_AT[st]]; }

// One section's step k. Five of the parts are the coefficients designFilter wrote and the rest are
// the section's plumbing, which is why they come out of the part list rather than off a fixed shape:
// the schedule interleaves the two biquads' z^-1 copies among their five words, and where a copy
// lands is what separates the two of them.
void section_step(yss236_core& c, int st, Section const& s, int k)
{
	switch (s.part[k])
	{
	case B2:       term(c, st, s.x2); break;                            // b2 on x[-2]
	case B1:       sum(c, st, s.x1); break;                             // b1 on x[-1]
	case A1:       sum(c, st, s.y1); break;                             // -a1 on y[-1]
	case A1_X2:    copy_z(c, st, s.y1, s.x2, ADD_ACC, CLAMP_WRAP); break;
	case A2_Y2:    copy_z(c, st, s.y2, s.y2, ADD_ACC, CLAMP_WRAP); break;
	case B0:       close(c, st, s.x1, s.out, s.route); break;           // b0, closing the section
	case X2_FETCH: sum(c, st, s.x1); break;                             // x[-1]'s own fetch
	case X2_COPY:  copy_z(c, st, s.x1, s.x2, ADD_ACC, CLAMP_WRAP); break;
	case IN:       close(c, st, s.in, s.x1, ROUTE_2); break;            // the section's input step
	case ZERO_Y2:  term(c, st, s.y2); break;                            // a zero word on y[-2]
	case ZERO_OUT: sum(c, st, s.out); break;                            // and one on the output
	}
}

// ---------------------------------------------------------------------------------------------
// The four diffusion rows: ins::ER_ROW_STEPS
// ---------------------------------------------------------------------------------------------

// Where in a row's four steps the line write falls. The first two rows close their allpass point
// before they write the line, so the slot holds the row's own output, and the last two write first,
// so the slot they write is the one the allpass point then reads back a Diffusion delay later.
enum row_shape_t { WRITE_LAST, WRITE_SECOND };

// One diffusion row, ins::ER_ROW_STEPS' four words: the gain on its input, its own allpass point and
// the write of its line. `st` is the row's first step and `nsteps` how many it covers, which is what
// index_by_span keys on. `in` is the register the row reads and `out` the one its allpass point closes
// into, which are the same register for the last three rows and not for the first: the first reads the
// low pass's output and closes into the diffusion output the second row reads. `cell` is the cell the
// allpass point comes off, which the base program's steps 0x0be and 0x0c2 fill by capturing the row's
// output back out of the slot this very row writes.
struct Row { int st, nsteps, in, cell, out; row_shape_t shape; };

// The first two rows run off the low pass's output and cascade: the second reads the register the
// first closed into, which is why both end at r[119] and the second overwrites the first three steps
// later. The last two run off the two sums, each closing back into its own register.
constexpr Row ROWS[4] = {
	//  st    n  in        cell  out      shape
	{ 0x1b4, 4, LPF_OUT,   18, DIFF,    WRITE_LAST },     // ins::ER_ROW_STEPS[0] to [3], into the slot 0x6d
	{ 0x1b8, 4, DIFF,      19, DIFF,    WRITE_LAST },     // and [4] to [7], into the slot 0x6e
	{ 0x1e6, 4, SUM_R,     57, SUM_R,   WRITE_SECOND },   // and [8] to [11], into the slot 0x79, for r[120]
	{ 0x1ea, 4, SUM_L,     58, SUM_L,   WRITE_SECOND },   // and [12] to [15], into the slot 0x7a, for r[121]
};

// The row a step belongs to. The first two sit in the middle of the program and the last two after the
// two sums, so the block is not contiguous, but the four do not overlap.
constexpr auto ROWS_AT = index_by_span(ROWS);

// The row a step belongs to, or null when it belongs to none.
Row const* row_for(int st) { return ROWS_AT[st] < 0 ? nullptr : &ROWS[ROWS_AT[st]]; }

// One row's step k. k = 0 and k = 2 are both the gain on the row's input: the word at k = 2 is zero on
// the first two rows, which leaves the running value as k = 0 left it, and ins::ER_ROW_STEPS' own
// allpass feedback on the last two, which the allpass point a step later adds to. The other two are the
// allpass point and the line write, and which of the two comes first is the whole difference between
// the two pairs of rows.
void row_step(yss236_core& c, int st, Row const& r, int k)
{
	if (k == (r.shape == WRITE_LAST ? 3 : 1))
	{
		// the line write: the row's allpass point added to the running value, which is the row's own
		// output, into the row's slot. It carries the wdata flag and is the writer itself, so it clears
		// any latched data and writes its own result.
		run(c, {.st = st, .sm = r.cell, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		return;
	}
	if (k == (r.shape == WRITE_LAST ? 1 : 3))
	{
		// the allpass point: the row's cell scaled and added to the running value, closed into the
		// row's output register with the x2 route. That register is where the next row reads from and,
		// for the last two rows, where the mix below reads the wet signal back out of.
		run(c, {.st = st, .sm = r.cell, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = r.out});
		return;
	}
	term(c, st, r.in);
}

// ---------------------------------------------------------------------------------------------
// The twenty tap gains: ins::ER_STEPS[0] to [19]
// ---------------------------------------------------------------------------------------------

// What the closing step of a bank of ten does with the delay line. On the left it latches the sum as
// the data of the write two steps along, and that write, 0x1c7, is a class-0 step. On the right the
// closing step is the write itself.
enum gain_write_t { SET_LATCH, WRITE_OWN };

// One channel's ten tap gains, ins::ER_STEPS[0] to [9] on the left and [10] to [19] on the right, one
// word each on the cell that holds the tap. The first opens the sum with its own word and the adder
// off and the other nine add to the running value. The tenth closes the bank: it publishes the whole
// sum into `sum_cell` from its own result and, with it, either latches that result as the data of the
// line write or is the write itself.
struct GainBank
{
	int st, nsteps;
	int sum_cell;
	gain_write_t write;
	int cell[10];     // the ten taps, in the order ins::ER_STEPS numbers them
};

// The left taps are m[22] to m[31], which the base program's steps 0x118 to 0x194 captured out of
// ins::ER_SLOTS' even entries; the right taps are the cells TAPS filled, so the two banks differ in
// where their taps came from and not in the arithmetic that scales them. ins::ER_STEPS[19] is the zero
// word erPattern() leaves, so the tenth right tap contributes nothing to the sum it closes.
constexpr GainBank GAINS[2] = {
	{ 0x1bc, 10, LEFT_TOTAL,  SET_LATCH, { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 } },
	{ 0x1d2, 10, RIGHT_TOTAL, WRITE_OWN,  { 56, 57, 58, 59, 60, 61, 62, 63, 56, 57 } },
};

// The bank a step belongs to. The two banks do not overlap.
constexpr auto GAINS_AT = index_by_span(GAINS);

// The bank a step belongs to, or null when it belongs to none.
GainBank const* gain_for(int st) { return GAINS_AT[st] < 0 ? nullptr : &GAINS[GAINS_AT[st]]; }

// One tap gain. Only the closing step saturates, writes a cell or touches the delay line; the nine
// before it scale their cell and leave the running value to move.
void tap_gain(yss236_core& c, int st, GainBank const& g, int k)
{
	alu a;
	a.st = st;
	a.sm = g.cell[k];
	a.m2 = M2_CELL;
	a.add = k ? ADD_ACC : ADD_OFF;
	if (k == 9)
	{
		a.clamp = CLAMP_SAT;
		a.dm = g.sum_cell;
		a.wdata = true;                                        // s34: the result is the next write's data
		a.write = g.write != SET_LATCH;                        // and on the right this step is that write
	}
	run(c, a);
}

// ---------------------------------------------------------------------------------------------
// The two sums, ins::ER_DIFF_SLOTS
// ---------------------------------------------------------------------------------------------

// One channel's sum and the other channel's, a Diffusion delay back, added to it. `sum` is the cell
// the channel's ten gains added into and `cross` the cell carrying the other channel's sum out of
// its allpass slot, which is ins::ER_DIFF_SLOTS' allpass point: earlyReflections() puts the slot 0x75
// at delay[0x71] + d + 1 and the slot 0x65 at delay[0x76] + d + 1, so each register is its own
// channel with the other one's d + 1 samples behind it.
struct SumPair { int st, nsteps, sum, cross, out; };

constexpr SumPair SUMS[2] = {
	{ 0x1dc, 2, LEFT_TOTAL,  CROSS_R, SUM_L },   // m[43] and m[33], the left sum with the right one behind
	{ 0x1de, 2, RIGHT_TOTAL, DIFF_L, SUM_R },   // and m[44] with m[56], the other way round
};

// The pair a step belongs to, or null when it belongs to none. The two do not overlap.
constexpr auto SUMS_AT = index_by_span(SUMS);

// The pair a step belongs to, or null when it belongs to none.
SumPair const* sum_for(int st) { return SUMS_AT[st] < 0 ? nullptr : &SUMS[SUMS_AT[st]]; }

// One sum and the other one behind it. The first step scales the channel's own sum by the ROM word
// 0x7fff with the adder off, and the second adds the delayed one to it and closes the two into the
// register the mix reads back. steps.h's close() takes a register as its source and this step's source
// is a cell, so this is the general form and not close().
void sum_pair(yss236_core& c, int st, SumPair const& s)
{
	if (st == s.st) term_cell(c, st, s.sum);
	else               run(c, {.st = st, .sm = s.cross, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = s.out});
}

// ---------------------------------------------------------------------------------------------
// The two output mixes, ins::ER_WET
// ---------------------------------------------------------------------------------------------

// One channel's dry and wet mix, ins::ER_WET's two words and the line write that sits beside them.
// `dry` is the signal the base program leaves the window, `wet` that channel's sum, `out` the output
// register the base program reads back, and `spare` the cell the third step writes a slot from.
struct Mix { int st, nsteps, dry, wet, out, spare; };

constexpr Mix MIXES[2] = {
	{ 0x1f1, 3, SIG_L, SUM_L, OUT_L, 20 },   // ins::ER_WET[0] and [1], into r[17], and the slot 0x7c
	{ 0x1f5, 3, SIG_R, SUM_R, OUT_R, 21 },   // ins::ER_WET[2] and [3], into r[18], and the slot 0x7d
};

// The mix a step belongs to, or null when it belongs to none. Each is three steps and the two are
// four apart, with a padding step between them, so they do not overlap.
constexpr auto MIXES_AT = index_by_span(MIXES);

// The mix a step belongs to, or null when it belongs to none.
Mix const* mix_for(int st) { return MIXES_AT[st] < 0 ? nullptr : &MIXES[MIXES_AT[st]]; }

// One mix's three steps: the dry word with the adder off, the wet word closing the sum into the output
// register with the x4 both channels carry, and the write of the slot beside the mix. That third step
// has no mixer word behind it at all: no parameter writes its coefficient, its word is the ROM's own
// 0x7fff or zero, and no step of this program or the base program's reads either slot.
void output_mix(yss236_core& c, int st, Mix const& m)
{
	switch (st - m.st)
	{
	case 0:
		term(c, st, m.dry);
		break;
	case 1:
		close(c, st, m.wet, m.out, ROUTE_4);
		break;
	default:
		run(c, {.st = st, .sm = m.spare, .m2 = M2_CELL, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The blocks of steps that are not a table of anything
// ---------------------------------------------------------------------------------------------

// The mono mix, 0x1a0 and 0x1a1: the two signals the base program leaves the window, each scaled by
// the ROM word 0x2000, the second doubled by its route so both halves land at unity. That is the one
// signal both filters run on, which is why the two channels are told apart by their dry side and by
// their own tap lines rather than by a filter each.
void mono_mix(yss236_core& c, int st)
{
	if (st == 0x1a0) term(c, st, SIG_L);
	else               close(c, st, SIG_R, MIX, ROUTE_2);
}

// The High Damp filter, ins::ER_LPF's three words over the twentieth tap: an order-1 section whose
// input delay lives in two cells rather than in a register, because the tap the base program captured
// into m[32] is the only copy of it there is. 0x1b1 applies b0 to m[32], 0x1b2 applies b1 to m[42] and
// hands the m[32] it latched on to m[42] (msrc 7, the previous operand), and 0x1b3 closes the section
// with -a1 on r[75], the register b0's own output went to a pass earlier.
void high_damp(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1b1:
		term_cell(c, st, TAP20);                 // ins::ER_LPF[0], b0 on the twentieth tap
		break;
	case 0x1b2:
		// ins::ER_LPF[1], b1 on m[42], and the write that leaves m[42] as the m[32] of a pass earlier
		run(c, {.st = st, .sm = TAP20_Y, .m2 = M2_CELL, .add = ADD_ACC, .dm = TAP20_Y, .prev = true});
		break;
	default:
		// ins::ER_LPF[2], -a1 on the filter's own output register, the delay line's input
		close(c, st, DAMP, DAMP, ROUTE_1);
		break;
	}
}

// The delay line's input, 0x1ee and 0x1ef: the first two diffusion rows' output at the ROM word 0x7fff,
// and ins::ER_FEEDBACK[0] scaling the High Damp filter's output on top of it. The second step is the
// write of the slot 0x7b, ins::ER_SLOTS[21], which is where erPattern() measures every tap's position
// from: the ten right taps sit the initial delay plus the room size past it, so this is the tail they
// are read back out of.
void line_input(yss236_core& c, int st)
{
	if (st == 0x1ee) term(c, st, DIFF);
	else
		run(c, {.st = st, .sr = DAMP, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true, .write = true});
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. There is no gate and no conditional skip in this program, so the
// guard never fires here; it is the base program's steps around these that set m_skip.
void block_0x1a0(yss236_core& c)
{
	for_steps<0x1a0, 0x200>(c, [&](yss236_core& c, auto st)
	{

		// The delay RAM reads and the cells that take them up, and the three serial inputs. Both come
		// before the step's own arithmetic, because step() reads the delay RAM and moves a cell before
		// it does anything else. No step carries both a read and a capture, so the read's index settles
		// the step and the capture's is only asked when no read is.
		if (int t = TAPS_AT[st]; t >= 0) read_delay(c, st);
		else if (int t = TAP_CAPTURE_AT[st]; t >= 0) take_capture(c, TAPS[t].cell, st);
		if (int s = SERIALS_AT[st]; s >= 0)
			take_serial(c, SERIALS[s].cell, st, SERIALS[s].bank == SERIAL_R);

		// ---- 0x1a0 and 0x1a1: the two signals in, mixed to mono for both filters ----
		if (st < 0x1a2) { mono_mix(c, st); return; }

		// ---- 0x1a2 to 0x1b0: the two biquads, ins::ER_HPF and ins::ER_LPF_2 ----
		if (Section const* s = section_for(st)) { section_step(c, st, *s, st - s->st); return; }

		// ---- 0x1b1 to 0x1b3: the High Damp filter, ins::ER_LPF over the twentieth tap ----
		if (st < 0x1b4) { high_damp(c, st); return; }

		// ---- 0x1b4 to 0x1bb and 0x1e6 to 0x1ed: the four diffusion rows ----
		if (Row const* r = row_for(st)) { row_step(c, st, *r, st - r->st); return; }

		// ---- 0x1bc to 0x1c5 and 0x1d2 to 0x1db: the twenty tap gains, ten a channel ----
		if (GainBank const* g = gain_for(st)) { tap_gain(c, st, *g, st - g->st); return; }

		// ---- 0x1c6 to 0x1d1: the left sum's write into the slot 0x71 and eleven steps of padding ----
		if (st < 0x1d2) { if (st == 0x1c7) write_delay_latched(c, st); else idle(c); return; }

		// ---- 0x1dc to 0x1df: the two sums, each with the other channel's a Diffusion delay back ----
		if (SumPair const* s = sum_for(st)) { sum_pair(c, st, *s); return; }

		// ---- 0x1e0 to 0x1e5: the padding round the read of the last two rows' allpass point ----
		if (st < 0x1e6) { idle(c); return; }

		// ---- 0x1ee and 0x1ef: the delay line's input ----
		if (st < 0x1f0) { line_input(c, st); return; }

		// ---- 0x1f0 to 0x1f7: the dry and wet mix, ins::ER_WET, and the two writes nothing reads ----
		if (Mix const* m = mix_for(st)) { output_mix(c, st, *m); return; }

		// ---- 0x1f8 to 0x1ff: eight steps that count and nothing else ----
		idle(c);
		});
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m03(yss236_core& c, int from, int to)
{
    if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
// effects/base.cpp - VOP3-2, the base program: its 138 steps outside the windows.
//
// The base program is the one every performance loads. EffectHost::uploadProgram's kind 0 writes
// rom::BASE_PROGRAM into the steps none of the five windows hold, which is what the checker's
// rangesFor(BASE) hands this function: every step 0x000 to 0x0ff minus the reverb's steps, the aux
// window's 0x0e8-0x10f and the three effect windows' steps. That is 138 steps in eleven runs, and
// this function runs them.
//
// Nothing here synthesises. The windows do that, and this program takes what they hand back, pans it,
// sums it, filters it and scales it. In step order:
//
//   0x000-0x007  the pan law. rom::mix::MIXER_A pairs the eight send registers r[11] to r[18] with the
//                eight pan words the host writes: the aux pan at steps 0 and 1, the reverb delay's at
//                2 and 3, the variation's at 4 and 5, the insertion's at 6 and 7. Each step multiplies
//                its own register by the word in that step, so one pass pans the eight sends. PANS
//                below is those eight steps, one row each.
//   0x008-0x00b  four multiplies on r[20] and r[22] into r[121] to r[124]. The words at 0x008 and
//                0x009 are the high-mid band's b1 and b2 (rom::mix::EQ_HIGH_MID is {0x7e,0x7b,0x7a,
//                0x7c,0x7d}, designFilter's b0, b1, -a1, b2, -a2 order), and the words at 0x00a and
//                0x00b are mixer 12's pan pair. Nothing writes r[19] to r[22]: they are what the
//                windows leave, and these four steps weight them into the sums below.
//   0x00c-0x019  the sends into the reverb and the variation, as two halves of seven steps that end in
//                the scaled sum, r[120] for the reverb delay and r[119] for the variation. The steps
//                between the partial sums add coefficients no host write ever reaches, so they leave
//                the running value as it was; what they are for is their delay RAM and their captures.
//                The five reads of slots 0 to 4 at 0x003, 0x007, 0x00b, 0x00f and 0x013 are the
//                reverb's early reflection slots (rom::ER_BASE_SLOTS), and each feeds the capture five
//                steps later into m[56], m[57], m[58], m[59] and m[60].
//   0x03a-0x03d, 0x042-0x045, 0x046-0x04b, 0x050-0x055, 0x056-0x058, 0x05f-0x061, 0x089-0x08d,
//   0x092-0x096, 0x0d8-0x0df, 0x0e0-0x0e7  the aux window's eight differences, which aux.cpp names for
//                that program: one run each for r[46], r[34], r[44], r[32], r[45], r[33], r[43] and
//                r[31], the eight smoothers' difference registers (the diff column of its UNITS
//                table), taken into the running value and scaled by a route of 16 or 4 into r[3] to
//                r[10]. Each run starts by clearing the running value, 0x03a and 0x042 excepted: those
//                two multiply a cell by a word the host leaves at zero.
//   0x09b-0x0a2  the DC blocker, rom::mix::MIXER_HPF {0xa2,0xa1,0xa0} and its copy {0x9e,0x9d,0x9c},
//                one order-1 section per channel, and two more runs of terms.
//   0x066-0x074, 0x0b6-0x0c4  the master EQ's three bands on each channel: low (rom::mix::EQ_LOW
//                {0x6a,0x67,0x66,0x68,0x69}), mid (EQ_MID) and high (EQ_HIGH), each an order-2 section
//                of five steps, the words copied to the second channel's steps by masterEq's
//                copyCoefs. The shape is the same in all three. The -a1 step puts -a1 times the output
//                delay into the running value, the b1 step adds b1 times the input's second delay, the
//                b2 step adds b2 times its first delay and may carry the rcopy that shifts the output
//                delay along, the b0 step writes the section's input scaled by the route into that
//                input delay, and the -a2 step adds -a2 times the last delay and always carries the
//                rcopy that shifts that delay along. Registers r[23] to r[30] on the first channel
//                and r[35] to r[42] on the second. The two steps that read the delay RAM are the
//                reverb's slots 25 to 30 on the first channel and the insertion's 46 to 50 on the
//                second. EQ below is those six sections, and SHAPE says which of the three shapes of
//                section the program's own schedule uses, which is which steps carry the copies.
//   0x075-0x07e, 0x0c5-0x0ce  the other two bands' word steps, EQ_LOW_MID and EQ_HIGH_MID and their
//                copies. All twenty are class-0 steps with no adder, shift, clamp or sub, so they
//                compute nothing: they move the x latch and, where the field says so, a delay RAM read
//                or an m cell move (m[14], m[15], m[18] to m[21], m[56], m[57], m[59], m[60], m[63], and
//                two LFOs). Their words are read at 0x008 to 0x00b above, and the copies' at no step
//                here. WORDS below is those two runs.
//   0x0cf-0x0d0  the master volume, rom::mix::MIXER_B[14] and [15], which mixerParam's MIX_VOLUME case
//                writes as v * v * masterLevel / 127^2, onto r[29] and r[41] by the route's 16 into
//                m[16] and m[17]: the two master outputs, m[16] on the left DAC at 0x002 and m[17] on
//                the right one at the aux window's 0x10e.
//
// ACC below holds every run of terms: the eight aux differences, the two runs that open on a cell
// instead, and the DC blocker's two halves. The eleven runs the program owns do not line up with any
// of the groups above, which is why the loop looks each step up in the tables rather than splitting the
// range.
//
// The registers, so the names above can be checked against the words: r[7] and r[8] two of the summed
// aux differences and r[9] and r[10] two more; r[11] to r[18] the panned sends; r[19] and r[20],
// r[21] and r[22] the windows' returns, which no step here writes; r[23] to r[30] the first channel's
// EQ delays and r[35] to r[42] the second's; r[31] to r[34] and r[43] to r[46] the aux window's eight
// differences; r[119] to r[124] the summed differences and the weighted returns. Of the cells,
// m[16] and m[17] are the two outputs, m[6] and m[23] the serial inputs of page 0, and m[56] to m[60]
// the early reflection taps this program captures; the rest are moved once each and read by a window.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no word the host moves is
// folded in here: the program words are fixed per row and each step's fields are folded into the
// arithmetic below in yss236_core::step()'s order. The old model in vop3_effects.h runs this as a
// float mixer of pans, returns and sends into three Biq bands; its numbers were fitted to recordings
// and nothing here is one of them.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check base: identical, 87 types, 835,200 native passes.

#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step does besides its arithmetic
// ---------------------------------------------------------------------------------------------

// A step of this program reads the delay RAM, and moves a value into a cell or out to a serial
// output. `cell` is the cell it moves into, and the step's own page decides the serial bank, the LFO
// and the output slot, exactly as step() takes them. A step can do both, and 0x043, 0x047 and 0x067
// do: a delay read and a cell move on the same step.
enum decor_t { DEC_NONE, DEC_READ, DEC_CAPTURE, DEC_LFO, DEC_SERIAL_L, DEC_SERIAL_R, DEC_OUT };

struct Decor { decor_t kind; int cell; };

// Apply a step's delay read and its cell move, both before its arithmetic, because step() moves a cell
// or reads the delay RAM before it does any arithmetic.
void decor(yss236_core& c, int st, bool read, Decor const& move)
{
	if (read)
		read_delay(c, st);
	switch (move.kind)
	{
		case DEC_CAPTURE:   take_capture(c, move.cell, st); break;
		case DEC_LFO:       take_lfo(c, move.cell, st); break;
		case DEC_SERIAL_L:  take_serial(c, move.cell, st, 0); break;
		case DEC_SERIAL_R:  take_serial(c, move.cell, st, 1); break;
		case DEC_OUT:       serial_out(c, st, move.cell); break;
		case DEC_NONE: case DEC_READ: break;
	}
}

// ---------------------------------------------------------------------------------------------
// The accumulators: a run of terms, the last of which closes into a register
// ---------------------------------------------------------------------------------------------

// One term of an accumulator. `src` is the register it reads and `cell` the cell, and `m2` says which
// of the two the multiplier scales. `add` is what the adder adds to the product and `route` the route
// its result takes. `dr` is the register it writes, which only the closing term has, and that is the
// only term that saturates: every other one only moves the running value. `write` says the term is
// also the delay RAM write of its step.
struct Term
{
	int src, cell;
	m2_t m2;
	add_t add;
	route_t route;
	int dr;
	bool write;
	bool read;
	Decor move;
};

// One run of terms, one step each: `st` is the run's first step and `nsteps` how many it covers, which
// is what index_by_span keys on. The closing term is the one with a register to write.
struct TermRun { int st, nsteps; Term term[16]; };

// One accumulator's term: the register or cell it reads, the word, the adder, the route, the register
// it closes into, whether it writes the delay RAM, whether it reads the delay RAM, and the cell move.
#define T(sr, cell, m2, add, route, dr, write, read, mk, mc) \
	{ sr, cell, m2, add, route, dr, write, read, { mk, mc } }

// ---- the four weights onto r[121] to r[124] ----
constexpr TermRun WEIGHTS[1] = {{
    0x008, 4,
    { T(22, 0, M2_REG, ADD_OFF, ROUTE_1, 123, false, false, DEC_CAPTURE, 56),
      T(22, 0, M2_REG, ADD_OFF, ROUTE_1, 124, false, false, DEC_NONE, 0),
      T(20, 0, M2_REG, ADD_OFF, ROUTE_1, 121, false, false, DEC_NONE, 0),
      T(20, 0, M2_REG, ADD_OFF, ROUTE_1, 122, false, true,  DEC_NONE, 0) } }};

// ---- the sends: the reverb delay's seven steps, then the variation's seven ----
constexpr TermRun SENDS[1] = {{
    0x00c, 14,
    { T(14,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, false, DEC_CAPTURE, 57),
      T(124, 0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
      T(122, 0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
      T(18,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, true,  DEC_NONE, 0),
      T(12,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_CAPTURE, 58),
      T(16,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
      T(0,  58, M2_CELL, ADD_ACC, ROUTE_4, 120, false, false, DEC_NONE, 0),
      T(13,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, true,  DEC_NONE, 0),
      T(123, 0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_CAPTURE, 59),
      T(121, 0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
      T(17,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
      T(11,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, true,  DEC_NONE, 0),
      T(15,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_CAPTURE, 60),
      T(0,   1, M2_CELL, ADD_ACC, ROUTE_4, 119, false, false, DEC_NONE, 0) } }};

// ---- the twelve accumulators: the aux window's ten runs and the DC blocker's two halves ----
// The first two runs open on a cell rather than a register, because the host leaves the word at those
// steps at zero: they multiply m[23] and m[5], so they contribute nothing and only move the latch.
constexpr TermRun ACC[12] = {
    { 0x03a, 4, { T(  0, 23, M2_CELL, ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 16,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_NONE, 0),
                  T(122,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(124,  0, M2_REG,  ADD_ACC, ROUTE_16, 4, false, false, DEC_NONE, 0) } },
    { 0x042, 4, { T(  0,  5, M2_CELL, ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 15,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_SERIAL_L, 5),
                  T(121,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_SERIAL_R, 60),
                  T(123, 40, M2_REG,  ADD_ACC, ROUTE_16, 3, false, false, DEC_OUT, 40) } },
    { 0x046, 6, { T( 46,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 16,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_LFO, 10),
                  T( 12,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_CAPTURE, 57),
                  T( 18,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(122,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(124,  0, M2_REG,  ADD_ACC, ROUTE_16, 4, false, true,  DEC_NONE, 0) } },
    { 0x050, 6, { T( 34,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, false, DEC_CAPTURE, 59),
                  T( 15,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 11,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 17,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_NONE, 0),
                  T(121,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_CAPTURE, 63),
                  T(123,  0, M2_REG,  ADD_ACC, ROUTE_16, 3, false, false, DEC_NONE, 0) } },
    { 0x056, 3, { T( 44,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(122,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, true,  false, DEC_NONE, 0),
                  T(124,  0, M2_REG,  ADD_ACC, ROUTE_16, 8, false, false, DEC_CAPTURE, 56) } },
    { 0x05f, 3, { T( 32,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, true,  DEC_NONE, 0),
                  T(121,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_CAPTURE, 56),
                  T(123,  0, M2_REG,  ADD_ACC, ROUTE_16, 7, false, false, DEC_NONE, 0) } },
    { 0x089, 5, { T( 45,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 14,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 18,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_NONE, 0),
                  T(122,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_CAPTURE, 57),
                  T(124,  0, M2_REG,  ADD_ACC, ROUTE_16, 6, false, false, DEC_NONE, 0) } },
    { 0x092, 5, { T( 33,  0, M2_REG,  ADD_OFF, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T( 13,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, true,  DEC_NONE, 0),
                  T( 17,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(121,  0, M2_REG,  ADD_ACC, ROUTE_1,  0, false, false, DEC_NONE, 0),
                  T(123,  0, M2_REG,  ADD_ACC, ROUTE_16, 5, false, false, DEC_NONE, 0) } },
    { 0x0d8, 8, { T( 43,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 10,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 14,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 16,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 12,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 18,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(122,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(124,  0, M2_REG,  ADD_ACC, ROUTE_4,  10, false, false, DEC_NONE, 0) } },
    { 0x0e0, 8, { T( 31,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(  9,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 13,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 15,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 11,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T( 17,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(121,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_LFO, 15),
                  T(123,  0, M2_REG,  ADD_ACC, ROUTE_4,   9, false, false, DEC_NONE, 0) } },
    // the DC blocker's second channel: the input out of m[58] into r[21] and into slot 38, then -a1 on
    // r[21] and b0 closing into r[22]
    { 0x09b, 4, { T( 0, 58, M2_CELL, ADD_OFF, ROUTE_16, 21, true,  false, DEC_NONE, 0),
                  T(22,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, false, DEC_CAPTURE, 58),
                  T(21,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(21,  0, M2_REG,  ADD_ACC, ROUTE_1,  22, false, false, DEC_NONE, 0) } },
    // and its first channel: the input out of m[7] into r[19] and back out of slot 39
    { 0x09f, 4, { T( 0,  7, M2_CELL, ADD_OFF, ROUTE_16, 19, false, true,  DEC_NONE, 0),
                  T(20,  0, M2_REG,  ADD_OFF, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(19,  0, M2_REG,  ADD_ACC, ROUTE_1,   0, false, false, DEC_NONE, 0),
                  T(19,  0, M2_REG,  ADD_ACC, ROUTE_1,  20, false, false, DEC_NONE, 0) } },
};

// The accumulator a step belongs to. The twelve runs do not overlap and the table is not in step order,
// so the index is what keeps a step from walking the twelve to find the one run it is in.
constexpr auto ACC_AT = index_by_span(ACC);

// One term of an accumulator, at step st.
void term_step(yss236_core& c, int st, Term const& t)
{
	decor(c, st, t.read, t.move);
	run(c, {.st = st, .sr = t.src, .sm = t.cell, .m2 = t.m2, .add = t.add, .route = t.route, .clamp = t.dr ? CLAMP_SAT : CLAMP_WRAP, .dr = t.dr, .write = t.write});
}

// ---------------------------------------------------------------------------------------------
// The pan law
// ---------------------------------------------------------------------------------------------

// One step of the pan law: the send register on its own pan word, left in the register for the send
// sums below and for the window that reads it back. `cell` is the cell the step latches, which is
// zero on all but 0x002, and that step still scales the register: the cell is there because m[16] is
// what goes to the left DAC, not because the multiplier reads it.
struct Pan { int reg, cell; bool read; Decor move; };
constexpr Pan PANS[8] = {
    { 11,  0, false, {DEC_SERIAL_L, 6} },   // the aux pan, and m[6] takes the page 0 serial input
    { 12,  0, false, {DEC_SERIAL_R, 23} },  // and m[23] the second bank, which Effects::run leaves at zero
    { 13, 16, false, {DEC_OUT, 16} },       // the reverb delay's, and m[16] goes to out[0], the left DAC
    { 14,  0, true,  {DEC_NONE, 0} },       // and the read of the reverb's first early reflection slot
    { 15,  0, false, {DEC_NONE, 0} },       // the variation's
    { 16,  0, false, {DEC_NONE, 0} },
    { 17,  0, false, {DEC_LFO, 8} },        // the insertion's
    { 18,  0, true,  {DEC_NONE, 0} },       // and the read of the reverb's last early reflection slot
};

// ---------------------------------------------------------------------------------------------
// The master EQ's six sections
// ---------------------------------------------------------------------------------------------

// Which of a section's steps carry the z^-1 copies that shift its delay registers along. The -a2 step
// always carries one, which is how y[-2] exists at all; the other two steps are where the schedule
// puts them, and which is which is all that separates the program's three shapes of section.
enum shape_t { COPY_ON_B2, COPY_ON_A1, COPY_ON_A2 };

// One order-2 section in direct form I. The five steps run -a1, b1, b2, -a2 and b0. `st` is the
// section's first step and `nsteps` how many it covers, which is what index_by_span keys on. `a1` is the
// section's output delay and `a2` its second, `b1` its input's second delay and `b2` its first, and
// `in` is the register the section's input arrives in. `out` is where b0 puts the result, which is the
// next section's input. `prev_b1` is the delay register COPY_ON_A1 shifts, which is the one the section
// before this one reads.
struct Section
{
	int st, nsteps, in, b1, b2, a1, a2, out, prev_b1;
	step::route_t route;
	shape_t shape;
	bool read[5];
	Decor move[5];
};

constexpr Section EQ[6] = {
    //  st    n   in   b1  b2   a1  a2  out  prev  route      shape        reads               -a1              b1                b2                -a2                b0
    {  0x066, 5,  9,  23, 25,  24, 26,  25,   0, ROUTE_4,  COPY_ON_B2,
      { false, true, false, false, false },
      { {DEC_NONE, 0}, {DEC_LFO, 11}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
    {  0x06b, 5, 25,  25, 27,  26, 28,  27,  23, ROUTE_4,  COPY_ON_A1,
      { true, false, false, false, true },
      { {DEC_NONE, 0}, {DEC_CAPTURE, 56}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
    {  0x070, 5, 27,  27, 29,  28, 30,  29,   0, ROUTE_16, COPY_ON_A2,
      { false, false, false, true, false },
      { {DEC_CAPTURE, 57}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_CAPTURE, 58} } },
    {  0x0b6, 5, 10,  35, 37,  36, 38,  37,   0, ROUTE_4,  COPY_ON_B2,
      { false, false, false, true, false },
      { {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
    {  0x0bb, 5, 37,  37, 39,  38, 40,  39,  35, ROUTE_4,  COPY_ON_A1,
      { false, false, true, false, false },
      { {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_CAPTURE, 18}, {DEC_NONE, 0} } },
    {  0x0c0, 5, 39,  39, 41,  40, 42,  41,   0, ROUTE_16, COPY_ON_A2,
      { false, true, false, false, false },
      { {DEC_SERIAL_L, 41}, {DEC_NONE, 0}, {DEC_CAPTURE, 19}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
};

// The section a step belongs to. The six do not overlap, so the index gives a step its own section
// rather than leaving it to walk the six looking for one.
constexpr auto EQ_AT = index_by_span(EQ);

// One master EQ section's step k of five: -a1 opens the sum, b1 and b2 add the input's delays, -a2 adds
// the output's second delay and b0 closes the section into its output register.
void eq_step(yss236_core& c, int st, Section const& s, int k)
{
	decor(c, st, s.read[k], s.move[k]);
	switch (k)
	{
	case 0:
		// -a1 on the output delay, and on the second section of the pair the copy that shifts the
		// previous section's input delay along
		if (s.shape == COPY_ON_A1) copy_z(c, st, s.a1, s.prev_b1, ADD_OFF, CLAMP_WRAP);
		else                    term(c, st, s.a1);
		break;
	case 1:
		sum(c, st, s.b1); break;                              // b1 on the input's second delay
	case 2:
		// b2 on the input's first delay, and on the first section of the pair the copy that shifts
		// this section's output delay along
		if (s.shape == COPY_ON_B2) copy_z(c, st, s.b2, s.a1, ADD_ACC, CLAMP_WRAP);
		else                    sum(c, st, s.b2);
		break;
	case 3:
		copy_z(c, st, s.a2, s.a2, ADD_ACC, CLAMP_WRAP); break; // -a2 on y[-2], and y[-2] kept
	default:
		close(c, st, s.in, s.out, s.route); break;            // b0 on the input, into the output
	}
}

// ---------------------------------------------------------------------------------------------
// The two runs of class-0 word steps, and the master volume
// ---------------------------------------------------------------------------------------------

// The two runs of class-0 steps that carry the master EQ's spare words: EQ_LOW_MID and EQ_HIGH_MID and
// their copies. They compute nothing at all, so each step is its decor and the x latch. `st` is the
// run's first step and `nsteps` how many it covers, which is what index_by_span keys on.
struct WordRun { int st, nsteps; bool read[10]; Decor move[10]; };
constexpr WordRun WORDS[2] = {
    { 0x075, 10, { false, false, true,  false, false, false, true,  false, false, false },
               { {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_CAPTURE, 59}, {DEC_NONE, 0},
                 {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_CAPTURE, 60}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
    { 0x0c5, 10, { true,  false, false, false, true,  false, false, false, false, false },
               { {DEC_NONE, 0}, {DEC_CAPTURE, 20}, {DEC_LFO, 14}, {DEC_NONE, 0}, {DEC_NONE, 0},
                 {DEC_CAPTURE, 21}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_NONE, 0}, {DEC_CAPTURE, 56} } },
};

// The word run a step belongs to. Two runs of ten, which do not overlap, so the index gives a step its
// own run rather than leaving it to walk the two looking for one.
constexpr auto WORDS_AT = index_by_span(WORDS);

// The master volume: r[29] and r[41] scaled by its two words by the route's 16 into the two output
// cells, which are the only two steps here that write a cell from their own result.
void master_volume(yss236_core& c, int st)
{
	run(c, {.st = st, .sr = st == 0x0cf ? 29 : 41, .route = ROUTE_16, .clamp = CLAMP_SAT, .dm = st == 0x0cf ? 16 : 17});
}

// ---------------------------------------------------------------------------------------------
// The 138 steps
// ---------------------------------------------------------------------------------------------

// One iteration per step, carrying the m_skip guard step() opens with, so a cskip in the reverb's
// steps skips these as it would the interpreter's. There is no gate in this program; the skips come
// from the reverb's steps around them.
template <int from, int to>
void base_steps(yss236_core& c)
{
	for_steps<from, to>(c, [&](yss236_core& c, auto st)
	{

		// ---- the pan law, the four weights and the two sends: 0x000 to 0x019 ----
		if (st < 0x01a)
		{
			if (st < 0x008)
			{
				Pan const& p = PANS[st];
				decor(c, st, p.read, p.move);
				// r[reg] = the pan word on r[reg]: the send, panned, left in the register for the
				// send sums below and for the window that reads it back
				run(c, {.st = st, .sr = p.reg, .sm = p.cell, .clamp = CLAMP_SAT, .dr = p.reg});
			}
			else if (st < 0x00c)
				term_step(c, st, WEIGHTS[0].term[st - 0x008]);
			else
				term_step(c, st, SENDS[0].term[st - 0x00c]);
			return;
		}

		// ---- the twelve accumulators: the aux differences and the DC blocker ----
		if (int k = ACC_AT[st]; k >= 0)
		{
			term_step(c, st, ACC[k].term[st - ACC[k].st]);
			return;
		}

		// ---- the master EQ's six sections ----
		if (int k = EQ_AT[st]; k >= 0)
		{
			eq_step(c, st, EQ[k], st - EQ[k].st);
			return;
		}

		// ---- the two runs of class-0 word steps ----
		if (int k = WORDS_AT[st]; k >= 0)
		{
			int const w = st - WORDS[k].st;
			decor(c, st, WORDS[k].read[w], WORDS[k].move[w]);
			idle(c);
			return;
		}

		// ---- the master volume ----
		if (st == 0x0cf || st == 0x0d0) master_volume(c, st);
		});
}

#undef T

// The eleven runs of steps the base program owns, in step order.
struct Range { int from, to; void (*run)(yss236_core&); };
#define RANGE(a, b) {a, b, base_steps<a, b>}
const Range RANGES[] = {
	RANGE(0x000, 0x01a), RANGE(0x03a, 0x03e), RANGE(0x042, 0x04c), RANGE(0x050, 0x059),
	RANGE(0x05f, 0x062), RANGE(0x066, 0x07f), RANGE(0x089, 0x08e), RANGE(0x092, 0x097),
	RANGE(0x09b, 0x0a3), RANGE(0x0b6, 0x0d1), RANGE(0x0d8, 0x0e8),
};
#undef RANGE

}  // namespace

// One run of the base program's steps, exactly as yss236_core::step() would run them. The check hands
// this one of the eleven runs above per pass; a range that is not one of them, which no host builds,
// falls back to the interpreter.
void base(yss236_core& c, int from, int to)
{
	for (auto const& rg : RANGES)
		if (rg.from == from && rg.to == to) { rg.run(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
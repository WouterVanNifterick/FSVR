// fs1r/native/var_m09.cpp - VOP3-2, the variation window's program for Karaoke: its 72 steps.
//
// rom::VARIATION_PROGRAMS[9], the program the variation window holds under variation type 24, which
// fx_dispatch.cpp routes to EffectHost::karaoke(). It is one of the twenty-nine programs the window
// shares: nothing here knows which type is running, because the program words are fixed and only the
// constants and the delay offsets move, which fx_delay.cpp writes whenever a parameter does. The code
// reads them where they live on the pass, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in the groups the comments below open each of:
//
//   0x158-0x15f  the two serial inputs and the host's two tone filters, var::KARAOKE_HPF (0x15a, 0x15b,
//                0x15c) and var::KARAOKE_LPF (0x15d, 0x15e, 0x15f), one pole each, three words apiece.
//                The step that sums the two inputs is 0x159: r[103] = 2 * (k * r[6] + k * r[5]), which
//                at the shipped constants is half the sum, so the whole chain runs on the mono. The
//                high-pass's state is r[104] and the low-pass's r[105], the low-pass fed from the
//                high-pass. The two reads that ride on the filter steps (the slots 0x56 and 0x57) are
//                published in m[28] and m[29].
//   0x160-0x16d  the published reads and the first two row taps: the reads of the slots 0x58, 0x59 and
//                0x5A, published in m[30], m[60] and m[61]. 0x167 also takes the page's LFO into m[11].
//   0x16e-0x173  the first of the four row units, the pair var::KARAOKE_ROW_STEPS[0..3] over the slot
//                0x59: 0x16f reads the line (m[60], the read of 0x167) and 0x171 writes it through the
//                latch, which 0x173 spends on the slot 0x5C.
//   0x174-0x177  the second row unit, var::KARAOKE_ROW_STEPS[4..7] over the slot 0x5A: 0x175 reads the
//                line (m[61], the read of 0x16b) and 0x177 writes the slot 0x5D itself.
//   0x178-0x183  the main line. 0x176 takes up the read of 0x16f, the slot 0x5B, which is where
//                EffectHost::karaoke() puts the delay time: var::KARAOKE_SLOTS[1] is set to
//                var::KARAOKE_SLOTS[0]'s plus PITCH_DELAY. 0x178 to 0x17a fold that tap into m[55]
//                through var::KARAOKE_LEVEL, the feedback level, and 0x17d adds it to r[120] and latches
//                the sum, which 0x183 spends on the slot 0x60, var::KARAOKE_SLOTS[0], the line's own
//                write. The two taps at 0x17b and 0x17c are the line's output into the two row units
//                below, half each.
//   0x184-0x18b  the third and fourth row units, var::KARAOKE_ROW_STEPS[8..15] over the slots 0x5E and
//                0x5F: 0x185 reads the third line's tap (m[60], the read of 0x17b) and 0x187 writes the
//                slot 0x61, and 0x189 reads the fourth line's tap (m[61], the read of 0x17f) and 0x18b
//                writes the slot 0x62. 0x187 also takes the page's LFO into m[12].
//   0x18c-0x18f  the two outputs, the dry/wet pan var::KARAOKE_PAN: r[15] = 4 * (k * r[120] + k * r[5])
//                and r[16] = 4 * (k * r[121] + k * r[6]), the row chain's two sums and the two inputs.
//                0x18f reads the slot 0x63 on the way past.
//   0x190-0x19f  the tail: the reads of the slots 0x64 and 0x65 published in m[32] and m[33], and the
//                read of the slot 0x63 published in m[31] at 0x194, five steps after the read each time,
//                which is captured()'s lower bound. The rest of the steps are idle but for the z^-1
//                latch.
//
// Which units the row is made of, and what each word of a unit carries, is EffectHost::diffusers()'s
// business: it writes four words per unit (s28, s24, s26, s22) at var::KARAOKE_ROW_STEPS and the delay
// offsets at var::KARAOKE_ROW_SLOTS, which is the table {0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62, 0x5F},
// a write slot and a read slot for each of the four units. What each step adds up to, with k the word
// that step carries:
//
//   r[103]        0x159       = 2 * (k159 * r[6] + k158 * r[5])              the mono sum
//   r[104]        0x15c       = 2 * (k15c * r[103] + k15b * r[103] + k15a * r[104])
//   r[105]        0x15f       = 2 * (k15f * r[104] + k15e * r[104] + k15d * r[105])
//   line 0x5C     0x171       = k171 * line(0x59) + k170 * r[105]             row 1
//   r[120]        0x16f       = 2 * (k16f * line(0x59) + k16e * r[105])
//   r[120]        0x175       = 2 * (k175 * line(0x5A) + k174 * r[120])      row 2
//   line 0x5D     0x177       = k177 * line(0x5A) + k176 * r[120]
//   m[55]         0x17a       = k17a * m[55] + k179 * line(0x5B) + k178 * line(0x5B)
//   line 0x60     0x17d/0x183 = k17d * r[120] + m[55]                        the main line's write
//   r[120]        0x17b       = 2 * k17b * line(0x5B)   and r[121] at 0x17c = 2 * k17c * line(0x5B)
//   r[120]        0x185       = 2 * (k185 * line(0x5E) + k184 * r[120])      row 3
//   line 0x61     0x187       = k187 * line(0x5E) + k186 * r[120]
//   r[121]        0x189       = 2 * (k189 * line(0x5F) + k188 * r[121])      row 4
//   line 0x62     0x18b       = k18b * line(0x5B) + k18a * r[121]
//   r[15]         0x18d       = 4 * (k18d * r[120] + k18c * r[5])            the pan
//   r[16]         0x18f       = 4 * (k18f * r[121] + k18e * r[6])
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three
// steps allows, which is why the sums above read r[120] where the reader stands before the writer's
// write has landed. The old model in vop3_effects.h's run_delay() has the Karaoke case as one line, a
// feedback off its own tap and a second tap one delay further on, and is no reference for any number
// here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input, r[104] and r[105] the two
// tone-filter poles; r[120] and r[121] the two sums the pan reads. The cells: m[2] and m[6] the two
// serial inputs this block takes for the base program, m[11] and m[12] the two LFO values the window
// publishes, m[28], m[29] and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads,
// m[54] the main line's tap, m[55] the feedback off it, and m[60] and m[61] the row taps, which each
// cell takes twice a pass, once for the row that feeds the line and once for the row that comes off it.
// The delay slots: 0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62 and 0x5F the four row lines and their
// offsets, 0x5B and 0x60 the main line's tap and its write, 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the
// reads the base program and the other windows share. No step here reads at a delay index and none
// writes the t file, so no step of this program is fractional and none takes a multiplier from a t
// register.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m09: identical, 1 type, 9600 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, term_cell and the rest

// The registers and cells the comments above name, so the tables below read as the DSP they run.
constexpr int MONO = 103;      // r[103]: the mono sum of the two inputs, which the tone filters run on
constexpr int HPF = 104;       // r[104]: the high-pass's own state, and the low-pass's input
constexpr int LPF = 105;       // r[105]: the low-pass's own state, which the first row unit's words scale
constexpr int ROW_L = 120;     // r[120]: the first row chain's sum, and the main line's own input
constexpr int ROW_R = 121;     // r[121]: the second row chain's sum, which the pan's right word reads
constexpr int LINE = 54;       // m[54]: the main line's tap, the read of the slot 0x5B
constexpr int FB = 55;         // m[55]: the feedback off that tap, which the line's own write adds back
// m[60] and m[61] each carry two of the row units' read slots a pass, the first and third units' and
// then the second and fourth's, a capture five steps after each read, which is what captured() returns.
constexpr int TAP_ODD = 60;    // m[60]: the first and third units' tap, and what three of the four
constexpr int TAP_EVEN = 61;   //        units write their own line off

// ---------------------------------------------------------------------------------------------
// What a step does with a cell or the delay RAM before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// What a row unit's own line does with the delay line. It carries w1[13], so it either latches its
// result for the class-0 writer two steps along, which is the first unit, or it is that writer itself,
// which writes its own result and clears anything latched. No other step of the program writes the RAM.
enum delay_t { SET_LATCH, WRITE_OWN };

// ---------------------------------------------------------------------------------------------
// 0x158 and 0x159: the two serial inputs and the mono the whole chain runs on
// ---------------------------------------------------------------------------------------------

// The base program reads this window's input off m[2] and m[6], which are the two serial inputs the
// first two steps take, and the second of them closes the two into the mono. The first step has the
// adder off, so it is the left input alone in the running value, and the right one doubles the sum
// into r[103], which at the shipped words is half the two together.
void mono(yss236_core& c, int st)
{
	if (st == 0x158)
	{
		take_serial(c, 2, st, 0);
		term(c, st, 5);                       // acc = k * r[5], the left input alone
	}
	else
	{
		take_serial(c, 6, st, 1);
		close(c, st, 6, MONO, ROUTE_2);       // r[103] = 2 (k r[6] + acc), the mono
	}
}

// ---------------------------------------------------------------------------------------------
// 0x15a to 0x15f: the host's two tone filters, var::KARAOKE_HPF and var::KARAOKE_LPF
// ---------------------------------------------------------------------------------------------

// One pole, three words and three steps: -a1 on the pole's own state a pass back, b1 on the section's
// input added on top of it, and b0 closing the sum into that same register, doubled. `x` is the
// register the section's input is in, which the second and third words scale, and `y1` the pole's own
// state, which the first word scales and the third overwrites.
struct Tone { int base; int x, y1; Decor decor[3]; };

// The high-pass runs on the mono and the low-pass on the high-pass's output, one after the other. The
// read of the base program's own slot rides on each section's b1 step: the slot 0x56 on the high-pass
// and the slot 0x57 on the low-pass, and each is published five steps later.
constexpr Tone TONES[2] = {
	//  base   x     y1   -a1              b1                 b0
	{ 0x15a, MONO, HPF, { {}, {DEC_READ, 0}, {}            } },  // var::KARAOKE_HPF
	{ 0x15d, HPF,  LPF, { {}, {},            {DEC_READ, 0} } },  // var::KARAOKE_LPF
};

// One pole's step at base + k.
void tone(yss236_core& c, int st, Tone const& f)
{
	decor(c, st, f.decor[st - f.base]);
	switch (st - f.base)
	{
	case 0:  term(c, st, f.y1); break;                     // -a1 on the pole's own state, a pass back
	case 1:  sum(c, st, f.x); break;                       // b1 on the section's input
	default: close(c, st, f.x, f.y1, ROUTE_2); break;      // b0 closing into that state, doubled
	}
}

// ---------------------------------------------------------------------------------------------
// 0x160 to 0x16d: the published reads, one cell every four steps
// ---------------------------------------------------------------------------------------------

// The cells the base program and the other windows read this block's delay RAM into, a capture every
// four steps: the two tone filters' reads first, then the first row line's own. Each takes up the read
// of the fifth step before it, which is captured()'s lower bound.
const int PUBLISHED[4] = { 28, 29, 30, TAP_ODD };

// One capture and one read in every four steps, the capture on the first and the read on the last,
// five steps before the next capture takes that read up. The first two captures take the two tone
// filters' reads instead, and the read of the first row line also publishes the page's LFO for the base
// program. Every other step of the block is idle but for the x latch.
void published(yss236_core& c, int st)
{
	switch ((st - 0x160) % 4)
	{
	case 0:  take_capture(c, PUBLISHED[(st - 0x160) / 4], st); break;
	case 3:  read_delay(c, st);                              // the slot this step is over four
			if (st == 0x167) take_lfo(c, 11, st);            // and the page's LFO, on the first row read
			break;
	default: break;
	}
	idle(c);
}

// ---------------------------------------------------------------------------------------------
// 0x16e to 0x18b, the four row units, var::KARAOKE_ROW_STEPS[0..15]
// ---------------------------------------------------------------------------------------------

// One row unit, the four words EffectHost::diffusers() writes for it in the order s28, s24, s26, s22,
// and the delay slots it runs between: the write slot the s22 word lands in and the read slot the
// s24 word comes off, which var::KARAOKE_ROW_SLOTS gives as a pair apiece. `in` is the register both
// of the unit's own words scale, `out` the chain's sum that s24 closes into, `tap` the cell its read
// slot is published in and `wr` the cell its own line is written off.
struct Row
{
	int base;
	int in, out;
	int tap, wr;
	delay_t write;                  // SET_LATCH, or WRITE_OWN where the s22 word is its own writer
	Decor decor[4];
};

// The four units, in the order var::KARAOKE_ROW_STEPS lists them, over the slot pairs {0x5C, 0x59},
// {0x5D, 0x5A}, {0x61, 0x5E} and {0x62, 0x5F} of var::KARAOKE_ROW_SLOTS, a write slot and a read slot
// apiece. The first unit runs on the low-pass's output and the fourth closes the second chain, and it
// is the first and the second of the four that publish the next unit's read slot on their way past.
constexpr Row ROWS[4] = {
	//  base   in      out     tap      wr       write       s28            s24            s26              s22
	{ 0x16e, LPF,   ROW_L, TAP_ODD,  TAP_ODD,  SET_LATCH, { {},                      {DEC_READ, 0}, {DEC_CAPTURE, TAP_EVEN}, {}            } },
	{ 0x174, ROW_L, ROW_L, TAP_EVEN, TAP_EVEN, WRITE_OWN, { {},                      {},            {DEC_CAPTURE, LINE},     {}            } },
	{ 0x184, ROW_L, ROW_L, TAP_ODD,  TAP_ODD,  WRITE_OWN, { {DEC_CAPTURE, TAP_EVEN}, {},            {},                      {DEC_LFO, 12} } },
	{ 0x188, ROW_R, ROW_R, TAP_EVEN, TAP_ODD,  WRITE_OWN, { {},                      {},            {},                      {}            } },
};

// r[out] = 2 * (k * m[cell] + acc): the unit's read of its own line, closing the s28 word into the
// chain's sum. The route is the x2 the row's own words are half of.
void row_read(yss236_core& c, int st, int cell, int out)
{
	run(c, {.st = st, .sm = cell, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = out});
}

// k * m[cell] + acc, saturated, and the delay line's next write: the unit's s22 word, its own line out
// of its read slot. It carries w1[13], so it either latches the result for the writer two steps along
// or is that writer itself, and a writer clears any latched data and writes its own result.
void row_write(yss236_core& c, int st, int cell, delay_t w)
{
	run(c, {.st = st, .sm = cell, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = w == SET_LATCH, .write = w == WRITE_OWN});
}

// One row unit's step at base + k. The first unit's write is two steps behind its own latched result,
// one step of padding and then the class-0 writer, whose step over four is the write slot the firmware
// chose; the other three units write where they stand.
void row_unit(yss236_core& c, int st, Row const& w)
{
	int const k = st - w.base;
	if (k > 3)
	{
		if (k == 5) write_delay_latched(c, st);
		idle(c);
		return;
	}
	decor(c, st, w.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, w.in); break;                    // s28: the unit's own word on its input
	case 1:  row_read(c, st, w.tap, w.out); break;        // s24: its read slot, closing into the chain
	case 2:  term(c, st, w.in); break;                    // s26: the feedback word on the same input
	default: row_write(c, st, w.wr, w.write); break;      // s22: the unit's own line into its write slot
	}
}

// ---------------------------------------------------------------------------------------------
// 0x178 to 0x183: the main line's feedback, its two taps and its own write
// ---------------------------------------------------------------------------------------------

// The main line's own output, at half level, into each of the two row chains' sums.
const int TAPS[2] = { ROW_L, ROW_R };

// 0x17b and 0x17c: 2 * k * m[54] into the register the chain sums into, with the adder off, so the
// line's output is the only term.
void line_tap(yss236_core& c, int st, int out)
{
	run(c, {.st = st, .sm = LINE, .m2 = M2_CELL, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = out});
}

// The block, in two halves. The first six steps are the feedback off the line's own tap and the write
// that spends it: var::KARAOKE_LEVEL's two words on the tap, then this block's own third word on the
// feedback cell, which that step reads and writes at once, then the same tap at half level into each of
// the two row chains, then the chain's sum and the feedback added together and latched for the writer at
// 0x183. The rest is idle but for the reads the last two row units are fed by and the write itself.
void main_line(yss236_core& c, int st)
{
	if (st < 0x17e)
	{
		switch (st)
		{
		case 0x178: term_cell(c, st, LINE); break;     // var::KARAOKE_LEVEL[1] on the line's tap
		case 0x179: sum_cell(c, st, LINE); break;      // var::KARAOKE_LEVEL[0] onto that
		case 0x17a:
			// The feedback onto itself a pass back. The cell still holds the previous pass's sum, which
			// is what the adder adds, and the result goes back into it.
			run(c, {.st = st, .sm = FB, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dm = FB});
			break;
		case 0x17d:
			// The line's own write: the row chain's sum on this step's word, the feedback cell as the
			// adder's input, and the result latched for 0x183.
			run(c, {.st = st, .sr = ROW_L, .sm = FB, .add = ADD_CELL, .clamp = CLAMP_SAT, .wdata = true});
			break;
		case 0x17b:
		case 0x17c:
			if (st == 0x17b) read_delay(c, st);        // the slot 0x5E, the third unit's read slot
			line_tap(c, st, TAPS[st - 0x17b]);
			break;
		}
		return;
	}

	switch (st)
	{
	case 0x17f: read_delay(c, st); break;                 // the slot 0x5F, the fourth unit's read slot
	case 0x180: take_capture(c, TAP_ODD, st); break;      // the read of 0x17b, five steps earlier
	case 0x183: write_delay_latched(c, st); break;        // the line's own write, the slot 0x60
	default: break;
	}
	idle(c);
}

// ---------------------------------------------------------------------------------------------
// 0x18c to 0x18f: the dry/wet pan, var::KARAOKE_PAN's four words
// ---------------------------------------------------------------------------------------------

// One channel's two words: the dry word on the input the base program left in r[5] or r[6] opens the
// running value and the wet word closes the pair into the output register, quadrupled and saturated.
// The read of the base program's slot 0x63 rides on the right channel's closing step, and m[31] takes
// it up five steps later in the tail.
struct Pan { int base; int src, wet, out; Decor decor[2]; };

constexpr Pan PANS[2] = {
	//  base  src  wet     out
	{ 0x18c, 5, ROW_L, 15, { {}, {}             } },
	{ 0x18e, 6, ROW_R, 16, { {}, {DEC_READ, 0 } } },
};

// One channel's step at base + k.
void pan(yss236_core& c, int st, Pan const& p)
{
	decor(c, st, p.decor[st - p.base]);
	switch (st - p.base)
	{
	case 0:  term(c, st, p.src); break;                   // var::KARAOKE_PAN[0] or [2], the dry word
	default: close(c, st, p.wet, p.out, ROUTE_4); break;  // and [1] or [3], the wet word
	}
}

// ---------------------------------------------------------------------------------------------
// The run of steps 0x158 to 0x19f, the whole of the window's program
// ---------------------------------------------------------------------------------------------

// The loop is one iteration per step and carries the m_skip guard step() opens with, so a gate in the
// base program's 0x1xx half skips these steps as it would the interpreter's. Each test below is one of
// the groups the comments at the head of this file open.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{

		if (st < 0x15a) { mono(c, st); return; }                                  // the mono sum
		if (st < 0x160) { tone(c, st, TONES[(st - 0x15a) / 3]); return; }        // the two poles
		if (st < 0x16e) { published(c, st); return; }                             // the published reads
		if (st < 0x178) { row_unit(c, st, ROWS[(st - 0x16e) / 6]); return; }     // row units 1 and 2
		if (st < 0x184) { main_line(c, st); return; }                            // the main line
		if (st < 0x18c) { row_unit(c, st, ROWS[2 + (st - 0x184) / 4]); return; } // row units 3 and 4
		if (st < 0x190) { pan(c, st, PANS[(st - 0x18c) / 2]); return; }          // the two outputs

		// ---- 0x190 to 0x19f, the tail ----
		// Two more of the base program's own reads, the slots 0x64 and 0x65, and the three captures
		// that take up the read of 0x18f and of those two, five steps after each read. Every other
		// step of the tail is idle but for the x latch.
		int const k = st - 0x18f;
		if (k == 4 || k == 8) read_delay(c, st);
		else if (k == 5 || k == 9 || k == 13) take_capture(c, 31 + (k - 5) / 4, st);
		idle(c);
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m09(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

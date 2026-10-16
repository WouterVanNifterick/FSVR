// fs1r/native/var_m08.cpp - VOP3-2, the variation window's program for the four delay types: its 72 steps.
//
// rom::VARIATION_PROGRAMS[8], the program the window holds for the four variation types 20 to 23, and
// one of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives
// msb 8 to EffectHost::delayLCR (lsb 1, type 20), delayLR (lsb 2, type 21), echo (lsb 3, type 22) and
// crossDelay (lsb 4, type 23). Which of the four is running is not a code difference here: the program
// words are the same under all of them and only the constants and the six delay offsets differ, which
// fx_delay.cpp writes when a parameter moves. The code reads them where they live all the same, as
// step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in the groups the comments below open each of:
//
//   0x158-0x165  the two serial inputs and the host's four tone filters, var::DELAY_LPF (0x159, 0x15a,
//                0x15b) and var::DELAY_HPF (0x15c, 0x15d, 0x15e) with their copies var::DELAY_LPF_COPY
//                and var::DELAY_HPF_COPY (0x160 to 0x162 and 0x163 to 0x165), one one-pole each, three
//                words apiece: r[104] and r[105] the left channel's two poles, r[107] and r[108] the
//                right channel's, the second of a side fed from the first. The three reads that ride on
//                the first step of each group (the slots 0x56, 0x57 and 0x58) are published in m[28],
//                m[29] and m[30]. TONES below is those two channels, seven steps each, of which a pole
//                is three.
//   0x166-0x16f  the first two delay taps, the slots 0x59 and 0x5A (var::DELAY_SLOTS[1] and [4]), the
//                third at 0x16f of the slot 0x5B ([2]) and the fourth at 0x173 of the slot 0x5C ([5]).
//                Which line a tap reads is its delay offset and not this file: the program writes two
//                lines, at 0x17b and at 0x183, and a tap's offset says how far behind one of them it
//                sits. PUBLISHED below is every read the window makes and the cell that carries it, and
//                apart from those three reads and the capture of m[30] at 0x168 these ten steps compute
//                nothing, which is what idle() is for.
//   0x170-0x177  the first two taps into r[109] and r[110] (0x170 and 0x174 are class-3 steps: a unity
//                product with the adder off, which is step()'s plain copy) and each through the host's
//                damp pair into r[119] and r[120]: var::DELAY_CURVE (0x172, 0x173) on the first, and
//                var::DELAY_CURVE_COPY (0x176, 0x177) on the second. TAPS below is that pair, four
//                steps each.
//   0x178-0x17b  the left line's write, into the slot 0x5E (var::DELAY_SLOTS[0]): the two channels'
//                damped inputs and the feedback off r[119] and r[120]. 0x17b carries w1[13] and is
//                itself a writer, so it writes its own result and takes no latch. LINES below is that
//                line and the right one, which reaches its slot the other way round.
//   0x17c-0x188  the two wet sums r[124] and r[125] out of the taps, the two outputs r[15] and r[16]
//                after the level word each, and the right line's write into the slot 0x60
//                (var::DELAY_SLOTS[3]): 0x181 carries w1[13] and is not a writer, so it latches its
//                result, and 0x183 writes the slot and takes that latch rather than its own.
//                0x187 hands the page's LFO on. WETS below is the two sums, three steps each, and
//                OUTPUTS below the two outputs.
//   0x189-0x19f  the tail: three reads of the window's own area (the slots 0x63, 0x64 and 0x65)
//                published in m[31], m[32] and m[33], five steps later each, which is captured()'s
//                lower bound and which PUBLISHED below carries with the rest. The steps in between
//                are idle but for the z^-1 latch.
//
// Which of the four types is running is decided by the constants and the six offsets alone. What the
// tail of the program adds up to, each term as the dump's coefficients leave it, with k standing for
// the word that step carries and the letters for the taps the type's own offsets give:
//
//   r[109] r[110]  the first two taps, m[56] and m[57]
//   r[119] r[120]  the same two through the damp pair, which sums to one under every type here
//                  (0x31c7 + 0x4e39 and 0x238e + 0x5c72 are both 0x8000), so the pair is a copy and
//                  what it carries is the tap, not a fraction of it
//   0x17b  the left line   = k178 * r[105] + k179 * r[108]      the two damped inputs
//                           + k17a * r[119] + k17b * r[120]     the feedback off the first two taps
//   0x183  the right line  = k180 * r[120] + k181 * r[119] + k17f * r[108], latched at 0x181
//   r[124] = k17c * m[56] + k17d * m[58] + k17e * m[59]      the left wet sum, three taps
//   r[125] = k182 * m[57] + k183 * m[59] + k184 * m[58]      the right wet sum, the same three taps
//   r[15]  = 4 * 0.25 * r[124]  and r[16] = 4 * 0.25 * r[125], so the two outputs are the two sums
//
//   type 20 Delay L,C,R   k178 = k179 0.5, k17a 0.153, k17b 0   one feedback, the mono sum in one line
//                          r[124] = LCH + 0.62 * CCH, r[125] = RCH + 0.62 * CCH, and all four taps off
//                          that one line, so the right line carries nothing and is written zero
//   type 21 Delay L,R     k178 = k179 0.5, k17a = k17b 0.175     two feedbacks, each half the level
//                          r[124] = LCH, r[125] = RCH
//   type 22 Echo          k178 1.0, k179 0, k17a 0.336           each channel into its own line, each
//                          fed one feedback off its own first tap
//                          r[124] = L1 + 0.238 * L2, r[125] = R1 + 0.238 * R2
//   type 23 Cross Delay   k178 = k179 0.5, k17a 0, k17b 0.366   the mono sum into one line and one
//                          feedback off the other line's tap; the third and fourth taps sit one sample
//                          behind the right line's write, so they read that line a pass back
//                          r[124] = m[56] + 0.72 * m[59], r[125] = m[57]
//
// which tap each type means by a cell is its delay offsets and not this file: var::DELAY_SLOTS[1] is
// the first feedback (L,C,R), the second feedback (L,R), the first left tap (Echo) or the right-to-left
// delay (Cross Delay), and so on down the list. The old model in vop3_effects.h's run_delay() has the
// same shape, two lines and a handful of taps, and is no reference for any number here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] and r[106] the two inputs after the step
// that scales them, r[104] and r[105] the left channel's two damp poles and r[107] and r[108] the
// right channel's; r[109] and r[110] the first two taps, r[119] and r[120] those two through the damp
// pair; r[124] and r[125] the two wet sums. The cells: m[2] and m[6] the two serial inputs this block
// takes for the base program, m[11] and m[12] the two LFO values the window publishes, m[28], m[29]
// and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads, and m[56] to m[59] the
// four delay taps, in step order: the slots 0x59, 0x5A, 0x5B and 0x5C. The delay slots: 0x56, 0x57 and
// 0x58 read what the base program published, and 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x60, 0x63, 0x64 and
// 0x65 are the block's own area, which is where the base program's INIT_DELAY_* offsets put them. No
// step here reads at a delay index and none writes the t file, so no step of this program is
// fractional and none takes a multiplier from a t register.
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
// vop3_native_check var-m08: identical, 4 types, 38400 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, run, idle and the rest

// The registers and cells the comments above name, so the tables below read as the DSP they run rather
// than as a list of numbers.
constexpr int L_IN = 5, R_IN = 6;             // r[5] and r[6], the inputs the base program leaves this window
constexpr int L_OUT = 15, R_OUT = 16;         // r[15] and r[16], the two outputs it reads back
constexpr int L_DAMP = 105, R_DAMP = 108;     // r[105] and r[108], the second pole of each channel,
	// which is the damped input a line takes
constexpr int L_TAP = 119, R_TAP = 120;       // r[119] and r[120], the first two taps through the damp pair
constexpr int L_WET = 124, R_WET = 125;       // r[124] and r[125], the two wet sums
constexpr int TAP1 = 56, TAP2 = 57;           // m[56] and m[57], the two taps the program holds in registers
constexpr int TAP3 = 58, TAP4 = 59;           // m[58] and m[59], the other two, which only the sums read

// ---------------------------------------------------------------------------------------------
// Every read the window publishes, and the cell that carries it
// ---------------------------------------------------------------------------------------------

// One published read: the step that reads the delay RAM and the cell the capture CAPMIN steps later
// takes it up into, which is captured()'s lower bound and therefore the only distance the program uses.
struct Pub { int st, cell; };

// The ten reads, in step order. The slot a read takes is its own step over four, so the table names the
// read and never the slot. The first three are the base program's, off the four tone filters, and the
// last three are out of the window's own area; the four between them are the delay taps, and a tap's
// capture often lands in a different block from the read, which is why the reads are one table of the
// program rather than part of any one group's.
constexpr Pub PUBLISHED[10] = {
    //  the read  the cell     what it carries
    { 0x15b, 28   },  // the left channel's low pole
    { 0x15f, 29   },  // the right channel's low pole
    { 0x163, 30   },  // the right channel's second pole
    { 0x167, TAP1 },  // the first tap, the slot 0x59
    { 0x16b, TAP2 },  // the second, the slot 0x5A
    { 0x16f, TAP3 },  // the third, the slot 0x5B
    { 0x173, TAP4 },  // the fourth, the slot 0x5C
    { 0x18f, 31   },  // the tail's three, the slots 0x63, 0x64 and 0x65
    { 0x193, 32   },
    { 0x197, 33   },
};

// The capture index: PUBLISHED's own indices at the step each read's capture lands at, and -1 everywhere
// else, which is what the old scan answered at a step that is a read and nothing but a read. It is built
// from the read table itself rather than from a second table of captures, so the two cannot fall out of
// step, and it hands back PUBLISHED's indices: a step that carries a capture reaches its row for both.
template <int N>
constexpr std::array<int16_t, yss236_core::STEPS> index_by_lag(Pub const (&rows)[N], int lag)
{
	std::array<int16_t, yss236_core::STEPS> ix{};
	for (int16_t& i : ix) i = -1;
	for (int i = 0; i < N; i++) ix[rows[i].st + lag] = int16_t(i);
	return ix;
}

// Which read a step is, and which of their captures lands at it, so a step reaches both rather than
// looking through the ten for them. The index is the same on every pass and under all four types.
constexpr auto PUBLISHED_AT = index_by_step(PUBLISHED);
constexpr auto CAPTURES_AT = index_by_lag(PUBLISHED, yss236_core::CAPMIN);

// The read or the capture this step carries, if it carries either. step() reads the delay RAM and moves
// the cells before it does any arithmetic, so the step loop runs this before the group's own step. No step
// is both a read and a capture, so one lookup each settles it.
void publish(yss236_core& c, int st)
{
	if (int i = PUBLISHED_AT[st]; i >= 0) read_delay(c, st);
	if (int i = CAPTURES_AT[st]; i >= 0) take_capture(c, PUBLISHED[i].cell, st);
}

// ---------------------------------------------------------------------------------------------
// The host's four tone filters: var::DELAY_LPF and var::DELAY_HPF and their two copies
// ---------------------------------------------------------------------------------------------

// One order-1 pole, which is three of a channel's seven steps. `in` is the register its input is in,
// which the step before the pole wrote, and `out` is the register its output is in, which b0 overwrites
// and which -a1 reads a pass later.
struct Pole { int in, out; route_t route; };

// One channel's seven steps: its own copy of the input on the ROM's ~unity word, doubled, then the low
// pole and the high pole over that, three steps each. The two poles are a cascade of order-1 sections,
// so the high pole's input is the low pole's output, and each pole's b0 step carries the route that
// restores the scale designFilter()'s own (16 >> scale) took out of the coefficients.
struct Tone
{
	int base;
	int src, in;              // the base program's input register, and this channel's own copy of it
	Pole pole[2];             // var::DELAY_LPF and var::DELAY_HPF, or their copies
	Decor decor[7];           // what each of the seven steps moves before it computes
};

// The left channel first, which is where the two serial inputs land for the base program, then the right
// one, which publishes the page's LFO. The two rows are the same seven steps on the same two registers
// apart, one channel's block after the other's.
constexpr Tone TONES[2] = {
    //  base   src  in   the two poles                        the seven steps' decor
    {  0x158, L_IN, 103, { { 103, 104, ROUTE_2 }, { 104, L_DAMP, ROUTE_4 } },
        { {DEC_SERIAL_L, 2}, {DEC_SERIAL_R, 6}, {}, {},
          {}, {}, {} } },
    {  0x15f, R_IN, 106, { { 106, 107, ROUTE_2 }, { 107, R_DAMP, ROUTE_4 } },
        { {DEC_LFO, 11}, {}, {}, {},
          {}, {}, {} } },
};

// One channel's seven steps. The first takes the input the base program left this window and starts
// nothing, and the six after it are the two poles: -a1 on the pole's own output a pass back, which
// opens the running value, then the host's b1 on its input a pass back, then b0 on that input again,
// which closes the sum into the pole's own register.
void tone(yss236_core& c, int st, Tone const& t)
{
	int const k = st - t.base;
	decor(c, st, t.decor[k]);
	if (k == 0)
	{
		// the channel's input alone on the ROM's ~unity word, doubled and saturating into its register
		gain(c, st, t.src, t.in, ROUTE_2);
		return;
	}
	Pole const& p = t.pole[(k - 1) / 3];
	if (k % 3 == 1)      term(c, st, p.out);                  // the pole's -a1 on its own output a pass back
	else if (k % 3 == 2) sum(c, st, p.in);                    // the host's b1 on its input a pass back
	else                 close(c, st, p.in, p.out, p.route);   // and b0 on that input again, closing the pole
}

// ---------------------------------------------------------------------------------------------
// The first two taps, through the host's damp pair
// ---------------------------------------------------------------------------------------------

// One tap through the host's damp pair, four steps: the class-3 step that copies the cell whole into a
// register, a step of padding, then the pair's two words, of which the first opens the running value on
// the register and the second closes it into r[119] or r[120], saturating. var::DELAY_CURVE's two words
// are 0x31c7 and 0x4e39 and var::DELAY_CURVE_COPY's 0x238e and 0x5c72, and each pair sums to 0x8000, so
// what the pair carries is the tap whole and not a fraction of it.
struct TapDamp
{
	int base;
	int cell;               // the tap the class-3 step copies
	int reg;                // the register it lands in, which the damp pair's two words scale
	int out;                // and the register the pair closes into, which a line reads back
};

// The two taps the program holds in registers of its own, one per channel, each the feedback the same
// channel's line is built on.
constexpr TapDamp TAPS[2] = {
    //  base   cell  reg  out
    { 0x170, TAP1, 109, L_TAP },  // var::DELAY_CURVE on the left tap
    { 0x174, TAP2, 110, R_TAP },  // and var::DELAY_CURVE_COPY on the right one
};

// One of those four steps.
void tap_damp(yss236_core& c, int st, TapDamp const& t)
{
	switch (st - t.base)
	{
	case 0:
		// the class-3 step: a unity product with the adder off, which is step()'s plain copy
		run(c, {.st = st, .sm = t.cell, .m2 = M2_CELL, .unity = true, .dr = t.reg});
		break;
	case 2: term(c, st, t.reg); break;            // var::DELAY_CURVE[1], the low half of the pair
	case 3: close(c, st, t.reg, t.out); break;    // and its [0], the high half, saturating
	default: idle(c); break;                      // the padding between the copy and the pair
	}
}

// ---------------------------------------------------------------------------------------------
// The two feedback lines, the slots var::DELAY_SLOTS[0] and [3]
// ---------------------------------------------------------------------------------------------

// What a step does with a delay slot. WRITE_OWN is a step that carries w1[13] and is the writer itself:
// it clears any latched data and writes its own result. SET_LATCH is a step that only latches its result
// as the data of the next write, which is a step or two further on. WRITE_LATCHED is that writer: it
// writes the latch rather than its own result. NO_WRITE leaves the delay RAM alone.
enum delay_t { NO_WRITE, WRITE_LATCHED, WRITE_OWN, SET_LATCH };

// The two fields run() takes for a step that hands its result to a delay slot.
void delay_write(step::alu& a, delay_t w)
{
	a.wdata = w != NO_WRITE && w != WRITE_LATCHED;
	a.write = w != NO_WRITE && w != SET_LATCH;
}

// One line: the channel's damped input and the two taps through the damp pair, each on its own word, the
// last of them closing the sum into the line's slot. Which line a tap sits behind is its delay offset and
// not this table: the left line's four words are var::DELAY_WORDS[11], [12], [7] and [9] and the right
// line's three are [13], [8] and [10], and the slots are the block's own area, so a tap of one line
// counts off the other.
struct Line
{
	int base;
	int terms;               // how many words the line has, the last of which closes the sum
	int reg[4];              // the registers the words scale, in step order
	delay_t write;           // what the closing word leaves for the delay RAM
};

// The left line, which writes the slot 0x5E in its own closing word, then the right line, whose closing
// word only latches, because its writer is the middle step of the right channel's wet sum two steps on.
constexpr Line LINES[2] = {
    //  base   terms  the words' registers                      what the closing word does
    { 0x178, 4, { L_DAMP, R_DAMP, L_TAP, R_TAP }, WRITE_OWN },
    { 0x17f, 3, { R_DAMP, R_TAP, L_TAP },         SET_LATCH },
};

// One of a line's words. The first opens the running value on the channel's damped input, the words
// before the last add their level to it, and the last closes the sum and hands it to the delay slot.
void line(yss236_core& c, int st, Line const& l)
{
	int const k = st - l.base;
	if (k == 0)
	{
		term(c, st, l.reg[0]);                 // var::DELAY_WORDS[11] or [13]: the damped input
		return;
	}
	if (k < l.terms - 1)
	{
		sum(c, st, l.reg[k]);                  // the feedback level off one of the two damped taps
		return;
	}
	step::alu a;
	a.st = st;
	a.sr = l.reg[k];
	a.add = ADD_ACC;
	a.clamp = CLAMP_SAT;                       // a closing word saturates, the words before it wrap
	delay_write(a, l.write);
	run(c, a);
}

// ---------------------------------------------------------------------------------------------
// The two wet sums, out of the four taps
// ---------------------------------------------------------------------------------------------

// One channel's wet sum: three taps, each on its own word, the last closing the sum into r[124] or
// r[125]. The left channel's words are var::DELAY_WORDS[4] and [6] and the right channel's is [5], and
// what a type leaves a word at zero is how it drops a tap out of its sum: the ROM's own words on 0x17c
// and 0x17f are the two channels' input shares, and Delay L,R leaves 0x17c at zero.
struct Wet
{
	int base;
	int tap[3];               // the cells the three words scale, in step order
	int out;
	delay_t write;            // what the middle step does with the delay RAM
};

// The two sums, three steps each, on the same three taps in the other order. The right channel's middle
// step is the right line's writer, which is why that line closes a word early and writes here.
constexpr Wet WETS[2] = {
    //  base   the three taps' cells              out    what the middle step does
    { 0x17c, { TAP1, TAP3, TAP4 }, L_WET, NO_WRITE      },
    { 0x182, { TAP2, TAP4, TAP3 }, R_WET, WRITE_LATCHED },
};

// One of a wet sum's three steps: the first opens the running value on its tap, the middle adds the next
// tap to it, and the last closes the sum into the channel's wet register.
void wet(yss236_core& c, int st, Wet const& w)
{
	int const k = st - w.base;
	step::alu a;
	a.st = st;
	a.m2 = M2_CELL;
	a.sm = w.tap[k];
	if (k == 0)
	{
		a.add = ADD_OFF;               // the first tap opens the running value
	}
	else if (k == 1)
	{
		a.add = ADD_ACC;               // the middle tap is added to it, and is the right line's writer
		delay_write(a, w.write);
	}
	else
	{
		a.add = ADD_ACC;               // and the third closes the sum into the channel's wet register
		a.clamp = CLAMP_SAT;
		a.dr = w.out;
	}
	run(c, a);
}

// ---------------------------------------------------------------------------------------------
// The two outputs, var::DELAY_WORDS[0] to [3]
// ---------------------------------------------------------------------------------------------

// One channel's two output steps: the dry word on the channel's damped input opens the running value, and
// the wet word on the channel's wet sum closes it, quadrupled, into the register the base program reads
// back. `lfo` is the cell the first step publishes the page's LFO into, which is only the right
// channel's: the left page's has already gone out to the base program as m[11].
struct Out { int base; int dry, wet, reg; int lfo; };

constexpr Out OUTPUTS[2] = {
    //  base   dry     wet     reg     lfo
    { 0x185, L_DAMP, L_WET, L_OUT, 0  },
    { 0x187, R_DAMP, R_WET, R_OUT, 12 },
};

// One of a channel's two output steps.
void output(yss236_core& c, int st, Out const& o)
{
	if (st == o.base)
	{
		if (o.lfo) take_lfo(c, o.lfo, st);
		term(c, st, o.dry);            // the dry word on the channel's damped input
		return;
	}
	// the wet word on the channel's wet sum, closing the pair into the output, four times and wrapping
	run(c, {.st = st, .sr = o.wet, .add = ADD_ACC, .route = ROUTE_4, .dr = o.reg});
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

		// the ten published reads and the captures that take them up, before any arithmetic
		publish(c, st);

		if (st < 0x166)
		{
			// ---- 0x158 to 0x165, the two inputs and the host's four tone filters ----
			tone(c, st, TONES[(st - 0x158) / 7]);
			return;
		}
		if (st < 0x170)
		{
			// ---- 0x166 to 0x16f, the first three taps: three reads with the padding between them ----
			idle(c);
			return;
		}
		if (st < 0x178)
		{
			// ---- 0x170 to 0x177, the first two taps and the host's damp pair ----
			tap_damp(c, st, TAPS[(st - 0x170) / 4]);
			return;
		}
		if (st < 0x17c)
		{
			// ---- 0x178 to 0x17b, the left line, which writes the slot 0x5E itself ----
			line(c, st, LINES[0]);
			return;
		}
		if (st < 0x17f)
		{
			// ---- 0x17c to 0x17e, the left channel's wet sum ----
			wet(c, st, WETS[0]);
			return;
		}
		if (st < 0x182)
		{
			// ---- 0x17f to 0x181, the right line, whose closing word only latches ----
			line(c, st, LINES[1]);
			return;
		}
		if (st < 0x185)
		{
			// ---- 0x182 to 0x184, the right channel's wet sum, the right line's writer in the middle ----
			wet(c, st, WETS[1]);
			return;
		}
		if (st < 0x189)
		{
			// ---- 0x185 to 0x188, the two outputs, a dry word and a wet word each ----
			output(c, st, OUTPUTS[(st - 0x185) / 2]);
			return;
		}

		// ---- 0x189 to 0x19f, the tail, which is idle but for the three reads publish() takes up ----
		idle(c);
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m08(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
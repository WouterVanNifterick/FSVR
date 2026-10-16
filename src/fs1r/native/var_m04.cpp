// fs1r/native/var_m04.cpp - VOP3-2, the variation window's program for Auto Wah and Touch Wah: its 72 steps.
//
// rom::VARIATION_PROGRAMS[4], the program the window holds for the two variation types 11 and 12, and one
// of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 4 to
// Auto Wah and Touch Wah and to nothing else. The same program words run under both and only the constants
// move, so this native is one function for both types and neither is excluded. Which type is running is not
// a code difference here: fx_drive.cpp's autoWah() and touchWah() write the word lists below and leave the
// rest of the two types' own constants, which is why the Auto Wah's word at 0x169 is zero and the Touch
// Wah's is not, and why only autoWah() writes the depth word at 0x167 and only touchWah() the sensitivity
// word at 0x171.
//
//   WAH_FILTER      {0x15B, 0x159, 0x15A}       the left channel's low shelf: b0, b1, -a1
//   WAH_FILTER_2    {0x15E, 0x15C, 0x15D}       the left channel's high shelf, the same three
//   WAH_FILTER_3    {0x162, 0x160, 0x161}       the right channel's low shelf
//   WAH_FILTER_4    {0x165, 0x163, 0x164}       the right channel's high shelf
//   WAH_CUTOFF      {0x166}                     the cutoff
//   WAH_DEPTH       {0x167}                     the LFO depth on it: autoWah() writes it, touchWah()
//                                                does not, so the sweep is the Auto Wah's
//   WAH_SENSE       {0x170, 0x175}              the resonance: wahSense() writes one word into both
//   WAH_ATTACK      {0x171}                     the Touch Wah's sensitivity, autoWah() leaves it
//   WAH_HPF         {0x16D, 0x16B, 0x16C}       touchWah()'s input highpass, its words only
//   WAH_LPF         {0x181, 0x17F, 0x180}       touchWah()'s input lowpass, its words only
//   WAH_PAN         {0x185, 0x184, 0x187, 0x186} the dry and wet, interleaved: dry, wet, dry, wet
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, so each is b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and
// one of the output. fx_design.cpp's designFilter scales its words by 2048 * (16 >> scale), and the step
// carries 2^scale as its result route, which is why the low shelf's b0 sits on a step that doubles and the
// high shelf's on one that quadruples: scale 1 against highScale's 2.
//
// The 72 steps, in the groups the loop below splits them into:
//
//   0x158-0x165  the two channels' EQ, two order-1 shelves each. 0x158 takes the left input r[5] into
//                r[103] through the ROM's ~unity word and doubled, and 0x159 to 0x15b are the low shelf:
//                b1 times r[103] a pass back, -a1 times r[104], then b0 times r[103] and the sum
//                doubled into r[104]. 0x15c to 0x15e are the high shelf over r[104] into r[105]. 0x15f
//                to 0x165 are the same four steps for the right channel, r[6] into r[106] to r[108], on
//                the other channel's copies of the same words. 0x15b, 0x15f and 0x163 also read the delay
//                RAM, and 0x15c and 0x15d take the serial inputs into m[2] and m[6].
//   0x166-0x167  the cutoff the LFO sweeps. 0x166 is the class-1 load of WAH_CUTOFF's word, which
//                fx_common.h's wahCutoff() makes 3052 * v / 16 + 7837, so the running value runs from
//                about 30 to 128. 0x167 adds WAH_DEPTH's word times m[8], the window's first LFO, which
//                0x117 wrote just before this block, and publishes lfo[11] into m[11].
//   0x168        that cutoff into r[119] and t[9], with the ROM's own word at 0x168 scaling in r[113],
//                the lowpass state below: zero under Auto Wah, so r[119] is the cutoff itself, and
//                0x5000 under Touch Wah, whose lowpass carries signal.
//   0x169-0x16d  the highpass touchWah() designs, an order-1 section over r[109] into r[110] on the
//                words {0x16B, 0x16C, 0x16D}. 0x169 and 0x16a, which bring r[105] and r[108] into r[109],
//                are the two types' own words and the Auto Wah's are zero, so under it r[109] and r[110]
//                stay zero and this section is silent.
//   0x16e        the cutoff's one-pole: r[119] = t[9] * r[119], and t[9] takes the result, so the gain the
//                resonator below is swept by follows the cutoff.
//   0x16f-0x171  the resonator's input, left channel. 0x16f takes m[54] less the EQ output r[105].
//                0x170 adds WAH_SENSE's word times the state r[114] and saturates, into m[56]. 0x171 is
//                WAH_ATTACK's word times r[110] scaled by 16 and rectified, into m[58]: the Touch Wah's
//                sensitivity, on the highpass's output and zero under Auto Wah.
//   0x172-0x173  the second half of the smoother: 0x172 multiplies r[119] by t[9] and 0x173, another
//                class-1 load, adds its own word (128 * 0x0080 = 0.5 under Auto Wah), saturates and puts
//                the result back into t[9].
//   0x174-0x175  the same two steps as 0x16f and 0x170 for the right channel, over m[55], r[108] and
//                r[115], into m[57].
//   0x176-0x177  the rectified level's partner: the same running value with negatives clamped away, so
//                r[111] takes what m[58] does not hold.
//   0x178-0x179  the resonator's two accumulators: r[114] and r[115] take t[9] times their input cell
//                plus themselves, which with the resonance word above is the filter the wah sweeps.
//   0x17a-0x17e  the lowpass's input: four steps build the running value from r[112], r[111] and m[58] on
//                the ROM's own words and 0x17e writes r[112].
//   0x17f-0x181  the lowpass touchWah() designs, an order-1 section over r[112] into r[113] on the words
//                {0x17F, 0x180, 0x181}. It is the chain 0x168 reads, so under Touch Wah the cutoff goes
//                through it.
//   0x182-0x183  the resonator's feedback: m[54] and m[55] take t[9] times r[114] and r[115] plus
//                themselves, which is what 0x16f and 0x174 subtract from the input and closes the loop.
//   0x184-0x187  the dry and wet: WAH_PAN's four words. 0x184 and 0x186 scale r[114] and r[115] and
//                0x185 and 0x187 add the dry r[105] and r[108] and write the outputs r[15] and r[16].
//                dryWet() runs the variation blocks fully wet, so the dry words are zero and the two
//                outputs are the states at the window's gain.
//   0x188-0x19f  the tail. 0x188 to 0x18e, 0x190 to 0x192, 0x195, 0x196 and 0x199 to 0x19b are class 0
//                with no RAM mark and no ALU, so only the x latch runs. 0x18f reads the delay RAM and
//                publishes lfo[12] into m[12], 0x193 and 0x197 read, and 0x194, 0x198 and 0x19c publish
//                the three reads five steps before them.
//
// Every order-1 section in the program is three steps of one shape, and SECTIONS below is all six of them,
// one row each: the two shelves each channel's EQ runs, the highpass and the lowpass. Each section is
//
//   b1   the word on the section's own input, with no adder, which opens the sum
//   -a1  the word on the section's own output a pass back
//   b0   the word on the input again, closing the sum into the output with the section's route
//
// which is what fx_common.cpp's shelves() and fx_design.cpp's designFilter() write the words for. The
// route on a closing step is the 2^scale the designer took out of the words: 2 on the low shelves and the
// highpass, 4 on the high shelves against highScale's 2, and 1 on the lowpass. The rest of the program is
// per channel, and RES and MIX below are the two rows of each run of it, the left channel's and the right
// channel's registers and cells side by side.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x1f2 write
// the first, 0x08d the second) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005).
// r[103] to r[105] the left channel's EQ: input, low shelf, high shelf; r[106] to r[108] the right
// channel's. r[109] and r[110] the highpass, r[112] and r[113] the lowpass, r[114] and r[115] the
// resonator's two states, r[119] the cutoff and then the smoother's state, r[111] the rectified level's
// partner. The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[8] the
// window's first LFO, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six published
// delay RAM reads, m[54] and m[55] the resonator's two feedback accumulators, m[56] and m[57] its two
// inputs and m[58] the sensitivity level. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, read at
// the ring pointer with no index; this program writes none of them, so the lines behind these taps are
// filled elsewhere. Each capture takes the read five steps before it (captured()'s lower bound), so
// m[28] carries the slot 0x56 word, m[29] the slot 0x57 word, m[30] the slot 0x58 word, and m[31], m[32]
// and m[33] the slots 0x63, 0x64 and 0x65 words.
//
// The one t register the program uses is t[9], written at 0x168, 0x16e and 0x173 and read at 0x16e, 0x172,
// 0x178, 0x179, 0x182 and 0x183: the cutoff, then the smoother's gain, then the resonator's gain.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out by
// name rather than a step at a time.
//
// vop3_native_check var-m04: identical, 2 types, 19200 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, load, run, idle and the rest

// ---------------------------------------------------------------------------------------------
// The order-1 sections: the EQ's four shelves and touchWah()'s two input filters
// ---------------------------------------------------------------------------------------------

// What a step's own words move before it does any arithmetic: a cell from the hardware side, or a read
// of the delay RAM pushed into the history a capture further down the program takes up. The two LFO
// values this window publishes are taken at 0x167 and 0x18f instead, where no section runs.
enum decor_t { DEC_NONE, DEC_READ, DEC_CAPTURE, DEC_SERIAL };

// One step's decor. `cell` is the cell a capture or a serial input publishes into, and on a read the slot
// it reads, which is its own step over four and never read from here: read_delay() takes the slot off the
// step, as step() does. `bank` is which of the two serial input banks a DEC_SERIAL step reads, 0 the left
// and 1 the right.
struct Decor { decor_t kind; int cell; int bank; };

// Which row of SECTIONS is which, in the order the sections' steps run.
enum { SEC_LOW_L = 0, SEC_HIGH_L, SEC_LOW_R, SEC_HIGH_R, SEC_HPF, SEC_LPF };

// One order-1 section, in direct form I. `in` is the register its input is in and `out` the register its
// output goes to, which the -a1 step reads a pass later and the b0 step overwrites. `route` is the
// closing step's, which is the 2^scale the designer took out of the three words. The decor is one entry
// per step, in step order.
struct Section
{
	int base;                  // the first of the three steps
	int in, out;
	step::route_t route;
	Decor decor[3];
};

// The six sections: the two shelves each channel's EQ runs, and the highpass and the lowpass touchWah()
// runs its cutoff through. Only the EQ's four carry decor, because the base program reads what each of
// them publishes once and then gets on with it: three delay RAM reads and the captures that follow them
// five steps later, and the two serial inputs this window takes for the base program.
constexpr Section SECTIONS[6] = {
	//  base   in   out  route      b1                   -a1                  b0
	{ 0x159, 103, 104, ROUTE_2, { {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0}, {DEC_READ,   0x56, 0} } },
	{ 0x15c, 104, 105, ROUTE_4, { {DEC_SERIAL,  2, 0}, {DEC_SERIAL,  6, 1}, {DEC_NONE,    0, 0} } },
	{ 0x160, 106, 107, ROUTE_2, { {DEC_CAPTURE, 28, 0}, {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0} } },
	{ 0x163, 107, 108, ROUTE_4, { {DEC_READ,   0x58, 0}, {DEC_CAPTURE, 29, 0}, {DEC_NONE,    0, 0} } },
	{ 0x16b, 109, 110, ROUTE_2, { {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0} } },
	{ 0x17f, 112, 113, ROUTE_1, { {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0}, {DEC_NONE,    0, 0} } },
};

// One section's three steps. The decor the step's own words carry comes first, because step() moves a cell
// or reads the delay RAM before it fetches or adds, and then the section's term for this step.
void section(yss236_core& c, int st, Section const& s, int k)
{
	switch (s.decor[k].kind)
	{
		case DEC_READ:    read_delay(c, st); break;
		case DEC_CAPTURE: take_capture(c, s.decor[k].cell, st); break;
		case DEC_SERIAL:  take_serial(c, s.decor[k].cell, st, s.decor[k].bank); break;
		case DEC_NONE: break;
	}
	switch (k)
	{
	case 0:  term(c, st, s.in); break;                         // b1 on x[n], opening the sum
	case 1:  sum(c, st, s.out); break;                         // -a1 on y[n-1]
	default: close(c, st, s.in, s.out, s.route); break;        // b0 on x[n], closing into y[n]
	}
}

// ---------------------------------------------------------------------------------------------
// 0x158 to 0x165, the two channels' EQ: one input step and two shelves each
// ---------------------------------------------------------------------------------------------

// The EQ's two input steps, one per channel, and the only steps of the block that are not part of a
// section: the channel's serial input through the ROM's ~unity word and doubled into the register its low
// shelf reads a pass later. The right channel's also reads the delay RAM, which m[28] takes up five steps
// after it.
struct EqIn { int st, in, out; bool reads_delay; };

constexpr EqIn EQ_IN[2] = {
	//  st   in   out  reads_delay
	{ 0x158, 5, 103, false },
	{ 0x15f, 6, 106, true  },
};

void eq(yss236_core& c, int st)
{
	// Which channel's row this step is in: the left channel's steps run first and the right's start at
	// 0x15f, so the step says which input step and which pair of shelves it is.
	EqIn const& e = EQ_IN[st < EQ_IN[1].st ? 0 : 1];
	if (st == e.st)
	{
		if (e.reads_delay) read_delay(c, st);
		gain(c, st, e.in, e.out, ROUTE_2);
		return;
	}
	// The four shelves, each channel's high one on its low one's output, three steps each. The two
	// channels are seven steps apart rather than six, because the right channel's input step sits
	// between them, so each channel's pair is counted from its own first shelf.
	int const ch = st < SECTIONS[SEC_LOW_R].base ? SEC_LOW_L : SEC_LOW_R;
	Section const& s = SECTIONS[ch + (st - SECTIONS[ch].base) / 3];
	section(c, st, s, st - s.base);
}

// ---------------------------------------------------------------------------------------------
// The cutoff the LFO sweeps (0x166 to 0x168, 0x16e, 0x172 and 0x173) and touchWah()'s filter over it
// ---------------------------------------------------------------------------------------------

// 0x166 and 0x167: the cutoff itself, and the LFO's depth on it. fx_common.h's wahCutoff() makes the word
// 3052 * v / 16 + 7837, so the class-1 load puts the running value in the range about 30 to 128, and the
// depth word on top of it is the Auto Wah's alone: touchWah() writes no such word.
void cutoff(yss236_core& c, int st)
{
	if (st == 0x166)
	{
		// the class-1 load of WAH_CUTOFF's word: p = 128 * k, and nothing else
		load(c, {.st = st});
		return;
	}
	take_lfo(c, 11, st);        // m[11]: the LFO value this window publishes for the base program
	sum_cell(c, st, 8);         // p = k m[8] + p: WAH_DEPTH's word on the window's first LFO
}

// 0x168, 0x16e, 0x172 and 0x173: the four steps that make t[9] the swept gain. The program writes t[9]
// three times and reads it five times, and every step below that scales the resonator scales by it, so
// the cutoff's own one-pole is what keeps the sweep smooth.
void swept_gain(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x168:
		// r[119] = saturate(k r[113] + p), and t[9] takes the result as a 1.15 word. The word on r[113],
		// the lowpass touchWah() designs below, is zero under Auto Wah and 0x5000 under Touch Wah, so
		// under the first r[119] is the cutoff on its own and under the second the lowpass's output is
		// scaled into it.
		take_capture(c, 30, st);                                  // m[30]: the read of 0x163, five steps back
		t_put(c, st, 9, run(c, {.st = st, .sr = 113, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = 119}));
		return;
	case 0x16e:
		// r[119] = wrap(t[9] r[119]), and t[9] takes that back, so the gain the resonator runs by follows
		// the cutoff a sample at a time instead of stepping with it
		t_put(c, st, 9, run(c, {.st = st, .sr = 119, .treg = 9, .dr = 119}));
		return;
	case 0x172:
		// p = t[9] r[119]: the same product, left in the running value for the step below to add to
		run(c, {.st = st, .sr = 119, .treg = 9});
		return;
	default:
		// 0x173: the class-1 load adds its own word to that, 128 * 0x0080 = 0.5 under Auto Wah,
		// saturates, and t[9] takes it: the one-pole's second half
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_SAT});
		t_put(c, st, 9, c.acc);
		return;
	}
}

// 0x169 to 0x16d: the highpass touchWah() designs, an order-1 section over the two EQ outputs summed into
// its own input register.
void highpass(yss236_core& c, int st)
{
	if (st < SECTIONS[SEC_HPF].base)
	{
		// The two words that bring the EQ outputs together are the two types' own and the Auto Wah's are
		// zero, so under it r[109] and r[110] stay zero and the section below is silent. Under Touch Wah
		// this is the only signal the resonator sees, both channels' shelves summed.
		if (st == 0x169) term(c, st, 105);   // the left EQ output on its own
		else              close(c, st, 108, 109);   // and the right one added to it, into r[109]
		return;
	}
	section(c, st, SECTIONS[SEC_HPF], st - SECTIONS[SEC_HPF].base);
}

// ---------------------------------------------------------------------------------------------
// The resonator: two channels of a state register, a feedback accumulator and an input cell
// ---------------------------------------------------------------------------------------------

// One channel of the resonator. `open` and `sense` are the two steps its input takes, and the right
// channel's come five steps after the left's because the smoother and the sensitivity level sit between
// them; `state` and `fb` are the accumulator's and the feedback's own steps, adjacent per channel. `eq`
// is that channel's EQ output, `y` its state register, and `fb_cell` and `res_cell` its two cells: the
// feedback accumulator the input is taken against, and the input itself.
struct Resonator
{
	int open, sense, state, fb;
	int eq, y, fb_cell, res_cell;
};

constexpr Resonator RES[2] = {
	// open  sense state fb     eq   y  fb_cell res_cell
	{ 0x16f, 0x170, 0x178, 0x182, 105, 114, 54, 56 },
	{ 0x174, 0x175, 0x179, 0x183, 108, 115, 55, 57 },
};

// 0x16f and 0x174: the channel's own feedback accumulator less its EQ output, which is what the resonator
// runs on. The feedback the accumulator closes into at 0x182 is taken off again here, and that is the
// loop: the state register times the swept gain goes back round through this cell for ever.
void resonator_open(yss236_core& c, int st, Resonator const& r)
{
	// p = k m[fb_cell] - r[eq]
	run(c, {.st = st, .sr = r.eq, .sm = r.fb_cell, .m2 = M2_CELL, .add = ADD_REG, .sub = true});
}

// 0x170 and 0x175: WAH_SENSE's word on the channel's own state, subtracted from what the step above left
// in the running value and saturated, into the cell the accumulator below reads.
void resonator_sense(yss236_core& c, int st, Resonator const& r)
{
	// p = saturate(k r[y] - p); m[res_cell] takes it
	run(c, {.st = st, .sr = r.y, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dm = r.res_cell});
}

// 0x171: the Touch Wah's sensitivity, WAH_ATTACK's word on the highpass's own output r[110] scaled by 16
// and rectified, into m[58]. autoWah() writes no such word and leaves the one there at zero, so this is
// the Touch Wah's level alone, and both channels' partners below run against it.
void sensitivity(yss236_core& c, int st)
{
	run(c, {.st = st, .sr = 110, .route = ROUTE_16, .clamp = CLAMP_ABS, .dm = 58});
}

// 0x178 and 0x179: the swept gain on the channel's input cell, added to its own state. That register is
// both the resonator's output, which the dry and wet below take, and the value the feedback closes from.
void resonator_state(yss236_core& c, int st, Resonator const& r)
{
	// r[y] = saturate(t[9] m[res_cell] + r[y])
	run(c, {.st = st, .sr = r.y, .sm = r.res_cell, .m2 = M2_CELL, .treg = 9, .add = ADD_REG, .clamp = CLAMP_SAT, .dr = r.y});
}

// 0x182 and 0x183: the same swept gain on the state register, added to the cell the input above subtracts.
void resonator_fb(yss236_core& c, int st, Resonator const& r)
{
	// m[fb_cell] = saturate(t[9] r[y] + m[fb_cell]), clipped into the cell
	run(c, {.st = st, .sr = r.y, .sm = r.fb_cell, .treg = 9, .add = ADD_CELL, .clamp = CLAMP_SAT, .dm = r.fb_cell});
}

// ---------------------------------------------------------------------------------------------
// The sensitivity level's partners, the lowpass's input, and the dry and wet
// ---------------------------------------------------------------------------------------------

// The sensitivity level's two partners, a pair of steps each. The first step of a pair puts the
// complement of the level into the running value with the negatives clamped away, and the second closes
// that with the level itself on top into a register of its own. Both first steps read r[111]: the first
// pair writes that register back, the second the lowpass's input register. `from_cell` is which way the
// first step takes the level off: 0x176 off the cell the sensitivity step wrote, 0x17d off the running
// value that pair has by then put it in.
struct Partner
{
	int rect, close;            // the pair's two steps
	int read, out;              // the register the first reads and the one the second writes
	int cell;                   // m[58], the sensitivity level, which both steps scale
	bool from_cell;
};

constexpr Partner PARTNER[2] = {
	// rect  close  read  out  cell  from_cell
	{ 0x176, 0x177,  111,  111, 58, true  },
	{ 0x17d, 0x17e,  111,  112, 58, false },
};

void level_partner(yss236_core& c, int st, Partner const& p)
{
	if (st == p.rect)
	{
		// p = max(0, k r[read] - the level): the same value with the negatives clamped away
		if (p.from_cell)
			run(c, {.st = st, .sr = p.read, .sm = p.cell, .add = ADD_CELL, .sub = true, .clamp = CLAMP_ZERO});
		else
			run(c, {.st = st, .sr = p.read, .add = ADD_ACC, .sub = true, .clamp = CLAMP_ZERO});
		return;
	}
	// r[out] = saturate(k m[58] + p), the partner's own register
	run(c, {.st = st, .sm = p.cell, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = p.out});
}

// 0x17a to 0x17e: the lowpass's input, which is what the section below at 0x17f to 0x181 runs on.
void lowpass_in(yss236_core& c, int st)
{
	if (st < PARTNER[1].rect)
	{
		// Three steps on the ROM's own words: the lowpass's input register into the running value, then
		// that same term again added to the running value over 32768 (the adder names a zero m cell, and
		// that is where a divided running value comes from), then the sensitivity level subtracted.
		if (st == 0x17a) term(c, st, 112);                                    // p = k r[112]
		else if (st == 0x17b) run(c, {.st = st, .sr = 112, .add = ADD_CELL});  // p = k r[112] + p / 32768
		else                run(c, {.st = st, .sm = 58, .m2 = M2_CELL, .add = ADD_ACC, .sub = true});  // p = k m[58] - p
		return;
	}
	// 0x17d and 0x17e: the level's second partner, which closes into the lowpass's input register r[112].
	level_partner(c, st, PARTNER[1]);
}

// The dry and the wet, two steps per channel. WAH_PAN's wet word on the resonator's state opens the
// running value and the dry word on the channel's EQ output closes it, quadrupled, into that channel's
// output register. dryWet() runs the variation blocks fully wet, so the dry words are zero and the two
// outputs are the states at the window's own gain.
struct Mix
{
	int wet, dry;               // the pair's two steps
	int y, eq, out;             // the resonator's state, the EQ output and the output register
};

constexpr Mix MIX[2] = {
	//  wet   dry     y   eq  out
	{ 0x184, 0x185, 114, 105, 15 },
	{ 0x186, 0x187, 115, 108, 16 },
};

void out_mix(yss236_core& c, int st, Mix const& x)
{
	if (st == x.wet) term(c, st, x.y);                             // WAH_PAN's wet word on the state
	else              close(c, st, x.eq, x.out, ROUTE_4);          // and the dry word, into r[15] or r[16]
}

// ---------------------------------------------------------------------------------------------
// 0x188 to 0x19f, the tail: three published reads, their captures, the second LFO, and the padding
// ---------------------------------------------------------------------------------------------

// The reads are at 0x18f, 0x193 and 0x197 and each capture follows five steps later, which is
// captured()'s lower bound, so the tail is idle between them but for the x latch.
void tail(yss236_core& c, int st)
{
	int const k = st - 0x18f;
	if (k == 0 || k == 4 || k == 8)
	{
		read_delay(c, st);               // the slots 0x63, 0x64 and 0x65, at the ring pointer
		if (k == 0) take_lfo(c, 12, st); // m[12]: the window's second LFO value, on the same step
	}
	else if (k == 5 || k == 9 || k == 13)
		take_capture(c, 31 + (k - 5) / 4, st);      // m[31], m[32] and m[33] take the three reads up
	idle(c);
}

// ---------------------------------------------------------------------------------------------
// The program
// ---------------------------------------------------------------------------------------------

// The run of steps 0x158 to 0x19f, the whole of the window's program, one iteration per step. The loop
// carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips these
// steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{

		if (st < 0x166) { eq(c, st); return; }                            // 0x158-0x165, both channels' EQ
		if (st < 0x168) { cutoff(c, st); return; }                        // 0x166-0x167, the swept cutoff
		if (st == 0x168) { swept_gain(c, st); return; }                   // t[9] takes it
		if (st < 0x16e) { highpass(c, st); return; }                      // 0x169-0x16d, the input highpass
		if (st == 0x16e) { swept_gain(c, st); return; }                   // 0x16e, the cutoff's one-pole
		if (st == RES[0].open || st == RES[1].open)
		{
			resonator_open(c, st, RES[st == RES[1].open]);                  // 0x16f and 0x174
			return;
		}
		if (st == RES[0].sense || st == RES[1].sense)
		{
			resonator_sense(c, st, RES[st == RES[1].sense]);                // 0x170 and 0x175
			return;
		}
		if (st == 0x171) { sensitivity(c, st); return; }                   // 0x171, the Touch Wah's level
		if (st < 0x174) { swept_gain(c, st); return; }                    // 0x172-0x173, the smoother
		if (st < 0x178) { level_partner(c, st, PARTNER[0]); return; }      // 0x176-0x177, the first partner
		if (st < 0x17a) { resonator_state(c, st, RES[st - RES[0].state]); return; }   // 0x178-0x179
		if (st < 0x17f) { lowpass_in(c, st); return; }                    // 0x17a-0x17e, the lowpass's input
		if (st < 0x182)
		{
			section(c, st, SECTIONS[SEC_LPF], st - SECTIONS[SEC_LPF].base); // 0x17f-0x181, the lowpass
			return;
		}
		if (st < 0x184) { resonator_fb(c, st, RES[st - RES[0].fb]); return; }          // 0x182-0x183
		if (st < 0x188) { out_mix(c, st, MIX[(st - MIX[0].wet) / 2]); return; }          // 0x184-0x187
		tail(c, st);                                                          // 0x188-0x19f
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m04(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

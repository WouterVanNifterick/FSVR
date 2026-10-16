// effects/var_m10.cpp - VOP3-2, the variation window's program for the reverbs: its 72 steps.
//
// rom::VARIATION_PROGRAMS[10], the program the variation window holds under the four variation types
// 25 to 28, which fx_dispatch.cpp routes to EffectHost::variationReverb(): Hall, Room, Stage and Plate.
// It is one of the twenty-nine programs the window shares, so nothing here knows which of the four is
// running. The program words are fixed and only the constants and the delay offsets move, which
// fx_reverb.cpp writes whenever a parameter does, so the code reads them where they live on the pass,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in the groups the step loop below runs one function each, and the repeated
// units of those groups are the tables below: TONES the two tone poles, COPIES the eight z^-1 copies,
// UNITS the two diffusion units' four words each, LOWPASSES the two damping poles and PANS the two
// channels' pan words. The host names every group it programs, in EffectHost::variationReverb() and the
// tables it calls:
//   var::REVERB_HPF {0x15C,0x15B,0x15A}, var::REVERB_LPF {0x15F,0x15E,0x15D}
//   var::REVERB_ROW_STEPS {0x178,0x179,0x17A,0x17B,0x170,0x171,0x172,0x173}, var::REVERB_ROW_SLOTS {0x60,0x5B,0x5C,0x59}
//   var::REVERB_PRE {0x5D,0x5A}, var::REVERB_WIDTH {0x177}
//   var::REVERB_COMBS {0x19B,0x186,0x185,0x184,0x19F,0x189,0x188,0x187}
//   var::REVERB_ER_IDS {0x66,0x5E,0x61,0x5F,0x62}
//   var::REVERB_W {0x191,0x160,0x161,0x162,0x163,0x164,0x192,0x193,0x18F,0x18E}
//   var::REVERB_PAN {0x195,0x197,0x196,0x198}
// and the two 72-step halves run as:
//
//   0x158-0x15f  the two serial inputs, the mono sum and the host's two tone filters, the high-pass
//                var::REVERB_HPF and the low-pass var::REVERB_LPF, one pole each and three words apiece.
//                0x159 is the mono sum r[103] = 2 * (k * r[6] + k * r[5]), so the whole reverb runs on
//                the mono. The high-pass's state is r[104], the low-pass's r[105], the low-pass fed from
//                the high-pass. The reads of the slots 0x56 and 0x57 ride on the high-pass's second word
//                and the low-pass's third, and are published in m[28] and m[29].
//   0x160-0x164  m[28] takes up the read of 0x15b, the read of the slot 0x58 rides on 0x163, and m[29]
//                takes up the read of 0x15f. 0x164 is var::REVERB_W[5]: r[119] = k * r[118], the level
//                the diffusion parameter picks, on the base program's own return r[118].
//   0x165-0x166  r[109] = 2 * (k * m[55] + k * r[105]), the pre-delay tap published for the base
//                program, and 0x166 passes it on. m[55] is m[57] as 0x191 left it a pass back.
//   0x167-0x16e  nine rcopy steps that carry no arithmetic, only the z^-1 copies r[110] to r[117]. They
//                hold the reads of the slots 0x59 and 0x5A, publish m[30] (the read of 0x58) and m[56]
//                (the read of 0x59), and take the page's LFO into m[11].
//   0x16f-0x177  the second diffusion unit, var::REVERB_ROW_STEPS[4..7] over the slot 0x5C, and the
//                pre-delay. 0x16f reads the slot 0x5B and m[58] takes it up five steps later; 0x170
//                to 0x173 are the unit's four words, and 0x173 writes the slot 0x5C itself. 0x177 is
//                var::REVERB_WIDTH[0], the FB_LEVEL width: the slot 0x5D takes the unit's tap r[120]
//                mixed with the pre-delay tap m[57] behind it.
//   0x178-0x183  the first diffusion unit, var::REVERB_ROW_STEPS[0..3], driven by the pre-delay tap
//                m[57] rather than by the input. 0x179 publishes the unit's tap in m[63], 0x17b latches
//                the line's own write and 0x183 spends it on the slot 0x60, the unit's write. The read
//                of the slot 0x5E rides on 0x17b.
//   0x184-0x189  the two damping lowpasses, the (x, x, a) triples var::REVERB_COMBS[1..3] and
//                [5..7]: r[106] off r[107], and r[108] off the early reflection m[60]. 0x187 reads the
//                slot 0x61 and takes the page's LFO into m[12].
//   0x18a-0x18d  m[61] takes up the read of the slot 0x61, five steps after 0x187. 0x18a is a dead step:
//                its M_PREVIOUS write of m[54] is on a class-0 step, which step() returns before, so
//                nothing ever writes m[54] and 0x188 reads it as zero.
//   0x18e-0x193  the two output sums, var::REVERB_W[9] and [8] into r[120] and var::REVERB_W[6] and [7]
//                into r[121], each summing two early reflections and one level term. 0x191 is
//                var::REVERB_W[0] and copies m[57] into m[55], so m[55] is the pre-delay tap a pass
//                back; 0x190 publishes the read of the slot 0x62 in m[62].
//   0x194-0x198  m[31], m[32] and m[33] take up the reads of the slots 0x63, 0x64 and 0x65 for the base
//                program and the other windows, and the pan var::REVERB_PAN scales the two sums into
//                r[15] and r[16]: r[15] = 4 * (k * r[121] + k * r[5]) and r[16] = 4 * (k * r[120] + k * r[6]).
//                At the shipped constants k is a quarter and the two inputs carry nothing, so each
//                output is its own sum.
//   0x199-0x19f  the two comb gains, var::REVERB_COMBS[0] and [4]: the slot 0x66 takes the unit's tap
//                m[63] times its gain plus the first damper's state r[106], the slot 0x67 the same tap
//                plus the second damper's r[108]. m[33] takes up the read of the slot 0x65 on the way past.
//                The slots 0x66 and 0x67 are the bases the early reflections hang off, so combDelays2()
//                sets 0x5E and 0x61 from 0x66 and 0x5F and 0x62 from 0x67, each pair the same offset and
//                the second shorter by the diffusion.
//
// The two diffusion units are what EffectHost::diffusers() programs, and each is the same four words in
// the same roles: the row's two feedforward words and its tap gain and its feedback, which the host
// calls s28, s26, s24 and s22 and writes to unit[0..3]. UNITS below is those two rows of four words,
// one row per unit, and unit() runs them. With k the word the step carries, a unit whose line is read
// from slot L and whose input is x computes:
//
//   tap        0x179 / 0x171   = 2 * (s24 * line(L) + s28 * x), saturating at +-128, into m[63] / r[120]
//   write      0x17b / 0x173   = clip(2 * (s22 * line(L) + s26 * x), +-32), into the unit's write slot
//   with x = m[57] and L = the slot 0x5B for the first unit, and x = r[105] and L = the slot 0x59 for
//   the second. The two damping lowpasses are one poles each, y = 2 * (x * in + a * y_prev) over three
//   words apiece, and the two comb gains are the RT60 gains the damping loss scales.
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three steps
// allows, which is why the sums above read r[120] where the reader stands before the writer's write has
// landed. The old model in vop3_effects.h's run_delay() has a single Schroeder tail with eight combs and
// four all-passes for every reverb, and is no reference for any number or for any shape here: this program
// runs two units, one driven by the other's tap through the initial delay, and four early reflections.
//
// Two cells this program never fills, which is worth knowing before reading the steps that read them.
// m[59] is captured at 0x180, and captured() takes the latest delay RAM read made five to eleven steps
// earlier: the window is steps 373 to 379 and the read at 0x17f is step 383, four too late, so the
// capture returns m[59]'s own value and the cell stays at zero. 0x183 reads it, so the first damping
// lowpass's input r[107] is zero and r[106] stays at zero too. m[54] is written only by 0x18a, whose
// M_PREVIOUS is on a class-0 step, so it stays at zero as well and 0x188's term on it is zero. Both are
// transcriptions of step() as it stands rather than simplifications of them.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input; r[104] and r[105] the two
// tone-filter poles; r[106], r[107] and r[108] the two damping lowpasses and the first one's input;
// r[109] the pre-delay return this window hands the base program; r[118] the base program's own return
// and r[119] its level; r[120] the second unit's tap and the right output sum, r[121] the left one. The
// cells: m[2] and m[6] the two serial inputs this block takes, m[11] and m[12] the two LFO values the
// window publishes, m[28] to m[30] and m[31] to m[33] the six reads the base program and the other
// windows share, m[54] the cell nothing writes, m[55] the pre-delay tap a pass back, m[56] and m[58] the
// two units' lines, m[57] the pre-delay tap, m[59] the cell that stays at zero, m[60] to m[62] the three
// early reflections the outputs sum, and m[63] the first unit's tap. The delay slots: 0x5C and 0x60 the
// two units' write slots, 0x59 and 0x5B their read slots, 0x5D and 0x5A the pre-delay's write and read,
// 0x66 and 0x67 the early reflections' two base offsets with 0x5E, 0x61, 0x5F and 0x62 the four taps off
// them, and 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the reads the base program shares. No step here reads
// at a delay index, none writes the t file with its own result and none sets the compare flag, so no
// step of this program is fractional, no step takes a multiplier from a t register and no step branches.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m10: identical, 45 types, 432000 native passes. The 45 are the four variation
// types 25 to 28 and the 41 insertion types the sweep visits while the window still holds this program.

#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, run and the rest

// The registers and cells more than one of the blocks below touches, named as the head of this file
// names them, so the tables read as the DSP they run rather than as a list of numbers.
constexpr int MONO = 103;        // r[103]: the mono the whole reverb runs on
constexpr int DAMP1 = 106;       // r[106]: the first damping lowpass's own state
constexpr int DAMP_IN = 107;     // r[107]: its input, which 0x183 computes
constexpr int DAMP2 = 108;       // r[108]: the second damping lowpass's own state
constexpr int PRE = 109;         // r[109]: the pre-delay return the base program reads back
constexpr int LEVEL = 119;       // r[119]: the level the diffusion picks, on r[118] a pass back
constexpr int TAP2 = 120;        // r[120]: the second unit's tap, and the right output sum from 0x18f
constexpr int SUM_L = 121;       // r[121]: the left output sum

constexpr int DEAD = 54;         // m[54]: the cell whose only write is on a class-0 step
constexpr int PRE_BACK = 55;     // m[55]: the pre-delay tap as 0x191 left it a pass back
constexpr int LINE2 = 56;        // m[56]: the second unit's line, the read of the slot 0x59
constexpr int PREDELAY = 57;     // m[57]: the pre-delay tap, the read of the slot 0x5A
constexpr int LINE1 = 58;        // m[58]: the first unit's line, the read of the slot 0x5B
constexpr int EMPTY = 59;        // m[59]: the cell the capture at 0x180 never fills, so it stays zero
constexpr int ER1 = 60;          // m[60]: the first early reflection, off the 0x67 write
constexpr int ER2 = 61;          // m[61]: the second early reflection, off the 0x66 write
constexpr int ER3 = 62;          // m[62]: the third early reflection, off the 0x67 write
constexpr int TAP1 = 63;         // m[63]: the first unit's tap, which the two comb gains weigh

// ---------------------------------------------------------------------------------------------
// What a step does with a cell or the delay RAM before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// A step that is more than a term: it reads the delay RAM, publishes a capture of an earlier read or
// takes a serial input or an LFO. `cell` is the slot a DEC_READ reads, which is its own step over
// four, or the cell the move lands in. DEC_READ_LFO is the one step shape that moves twice: 0x167 and
// 0x187 read the delay RAM and take the page's LFO on the same word.
enum decor_t { DEC_NONE, DEC_READ, DEC_CAPTURE, DEC_LFO, DEC_READ_LFO };

struct Decor { decor_t kind; int cell; };

void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_READ:     read_delay(c, st); break;
		case DEC_CAPTURE:  take_capture(c, d.cell, st); break;
		case DEC_LFO:      take_lfo(c, d.cell, st); break;
		case DEC_READ_LFO: read_delay(c, st); take_lfo(c, d.cell, st); break;
		case DEC_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// 0x158 to 0x15f: the two serial inputs and the host's two tone filters
// ---------------------------------------------------------------------------------------------

// 0x158 and 0x159: the two serial inputs this window publishes for the base program, and the mono
// every block below runs on. The left input alone opens the running value and the right one closes
// the sum, doubled and saturated, into r[103].
void inputs(yss236_core& c, int st)
{
	if (st == 0x158)
	{
		take_serial(c, 2, st, 0);       // m[2] takes the left input
		term(c, st, 5);                 // acc = k * r[5], on the left input alone
	}
	else
	{
		take_serial(c, 6, st, 1);       // m[6] takes the right input
		close(c, st, 6, MONO, ROUTE_2); // r[103] = 2 * (k * r[6] + acc), the mono
	}
}

// One of the host's two one-pole tone filters over three words, `state` the pole's own register and
// `in` the register it is fed from. The first word is the pole's own coefficient on its state a pass
// back, the second that filter's feedforward coefficient on its input, and the third the same input
// again, closing the sum into the state through `route`: the high-pass runs the plain sum and the
// low-pass doubles it, and it is fed from the high-pass's output.
struct Tone
{
	int base;
	int state;
	int in;
	route_t route;
	Decor decor[3];
};

constexpr Tone TONES[2] = {
	//  base  state  in  route     -a1 on the state      b1 on the input         b0 closing
	{ 0x15a,   104, 103, ROUTE_1, { {DEC_NONE, 0},      {DEC_READ, 0x56},      {DEC_NONE, 0}     } },
	{ 0x15d,   105, 104, ROUTE_2, { {DEC_NONE, 0},      {DEC_NONE, 0},         {DEC_READ, 0x57} } },
};

void tone(yss236_core& c, int st, Tone const& f)
{
	int const k = st - f.base;
	decor(c, st, f.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, f.state); break;                 // -a1 on the pole's own state
	case 1:  sum(c, st, f.in); break;                     // b1 on the filter's input
	default: close(c, st, f.in, f.state, f.route); break; // b0 closing the sum into the pole
	}
}

// ---------------------------------------------------------------------------------------------
// 0x160 to 0x166: the two published reads, the level, and the pre-delay return
// ---------------------------------------------------------------------------------------------

// The three words 0x161 to 0x163 scale, which are the copy registers of the run below, r[110] two steps
// on and r[115] five, and those hold the x operand the step before each latched, a pass behind by the
// time these words read them.
const int COPY_TERMS[3] = { 110, 112, 115 };

// 0x160 to 0x164: the two reads the base program and the other windows share, each taken up into a
// cell five steps after the read, and the level the diffusion parameter picks. m[28] takes up the read
// of 0x15b and m[29] the read of 0x15f, the read of the slot 0x58 rides on the third of the three words
// between them, and 0x164 closes the whole run into r[119] as var::REVERB_W[5], the level on the base
// program's own return r[118].
void level(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x160:
		take_capture(c, 28, st);      // m[28] takes up the read of the slot 0x56
		term_cell(c, st, PRE_BACK);   // and the pre-delay tap a pass back opens the sum
		break;
	case 0x163:
		read_delay(c, st);            // the slot 0x58, which m[30] takes up at 0x168
		sum(c, st, COPY_TERMS[2]);
		break;
	case 0x164:
		take_capture(c, 29, st);      // m[29] takes up the read of the slot 0x57
		close(c, st, 118, LEVEL);     // and the level closes the run into r[119]
		break;
	default:
		sum(c, st, COPY_TERMS[st - 0x161]);
		break;
	}
}

// 0x165 and 0x166: the pre-delay the base program reads back at r[109], which is twice the word on
// m[55], the tap 0x191 left a pass back, and the class-3 step that carries that register on onto the
// running value for whoever reads it next.
void pre_delay_return(yss236_core& c, int st)
{
	if (st == 0x165) run(c, {.st = st, .sm = PRE_BACK, .m2 = M2_CELL, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = PRE});
	else run(c, {.st = st, .sr = PRE, .unity = true});   // the class-3 pass-through of r[109]
}

// ---------------------------------------------------------------------------------------------
// 0x167 to 0x16e: the z^-1 copies
// ---------------------------------------------------------------------------------------------

// The eight steps hold the x operand the step before latched in a register of their own, r[110] to
// r[117]: the class-3 word is 1.0, so what each writes is the delay and the running value is nothing
// anyone reads. The read of the slot 0x59 and the page's LFO ride on the first, m[30] takes up the
// read of 0x163 on the second, the pre-delay's read of the slot 0x5A on the fifth and m[56] takes it
// up on the sixth.
constexpr Decor COPIES[8] = {
	//  the slot 0x59 and m[11]   m[30]         -           -           the slot 0x5A   m[56]         -       -
	{DEC_READ_LFO, 11}, {DEC_CAPTURE, 30}, {DEC_NONE, 0}, {DEC_NONE, 0},
	{DEC_READ, 0x5A},   {DEC_CAPTURE, 56}, {DEC_NONE, 0}, {DEC_NONE, 0},
};

void copies(yss236_core& c, int st)
{
	int const k = st - 0x167;
	decor(c, st, COPIES[k]);
	// r[110 + k] takes the step before's x operand, and the word is 1.0 so the accumulator is idle.
	run(c, {.st = st, .sr = 110 + k, .unity = true, .dr = 110 + k, .z1 = true});
}

// ---------------------------------------------------------------------------------------------
// 0x16f to 0x183: the two diffusion units and the pre-delay
// ---------------------------------------------------------------------------------------------

// One diffusion unit's four words, the same four in the same roles in both units, which is what
// EffectHost::diffusers() programs: the feedforward gain s28 on the unit's input opens the sum, the
// line's tap gain s24 closes it doubled into the tap, s26 on the input opens it again and the line's
// feedback s22 closes it into the unit's write. The unit's input is the cell `in_cell` where it is
// the pre-delay and the register `in_reg` where it is the filtered mono, and `line` is the unit's own
// line, published five steps earlier from the read of its slot. The tap closes into `tap_out`, a cell
// for the first unit and a register for the second, and `own_write` says the closing word is itself
// the delay RAM write, which the first unit's is not: 0x17b latches that write and 0x183 spends it.
struct Unit
{
	int base;
	int in_reg;
	int in_cell;
	int line;
	int tap_out;       // the register or the cell the tap closes into
	bool tap_cell;     // which of the two it is
	bool own_write;    // the feedback word is itself the unit's write
	Decor decor[4];
};

constexpr Unit UNITS[2] = {
	//  base  in   cell  line  tap  tap_cell  own_write  the four words' decor
	{ 0x170, 105,    0, LINE2, 120, false,     true, { {DEC_CAPTURE, PREDELAY}, {DEC_NONE, 0},
													   {DEC_NONE, 0},           {DEC_NONE, 0}    } },
	{ 0x178,   0, PREDELAY, LINE1, TAP1, true, false,   { {DEC_NONE, 0},          {DEC_NONE, 0},
													   {DEC_NONE, 0},           {DEC_READ, 0x5E} } },
};

void unit(yss236_core& c, int st, Unit const& u)
{
	int const k = st - u.base;
	decor(c, st, u.decor[k]);
	if (k == 1)
	{
		// s24, the line's tap gain, closing what s28 opened: doubled and saturated into the tap.
		run(c, {.st = st, .sm = u.line, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = u.tap_cell ? 0 : u.tap_out, .dm = u.tap_cell ? u.tap_out : 0});
		return;
	}
	if (k == 3)
	{
		// s22, the line's feedback, closing what s26 opened: the unit's write, which this word either
		// makes itself or latches for the step eight later that does. w1[13] is carried either way.
		run(c, {.st = st, .sm = u.line, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .wdata = true, .write = u.own_write});
		return;
	}
	// k 0 and k 2: the two feedforward words, s28 and s26, on the unit's input, neither of which the
	// adder reads, so each opens the running value rather than adding to it.
	if (u.in_cell) term_cell(c, st, u.in_cell);
	else            term(c, st, u.in_reg);
}

// 0x16f to 0x177: the second diffusion unit and the pre-delay its tap starts. 0x16f reads the unit's
// line at the slot 0x5B and m[58] takes it up five steps later, the unit's four words follow, and
// 0x177 is var::REVERB_WIDTH[0]: the slot 0x5D, which var::REVERB_PRE[0] is, takes the unit's tap
// r[120] with the pre-delay tap m[57] added behind it, saturated. The two steps between are idle.
void predelay(yss236_core& c, int st)
{
	if (st == 0x16f) { read_delay(c, st); idle(c); return; }                  // the read of the slot 0x5B
	if (st == 0x174) { take_capture(c, LINE1, st); idle(c); return; }         // m[58] takes it up
	if (st == 0x177)                                                          // var::REVERB_WIDTH[0]
	{
		run(c, {.st = st, .sr = TAP2, .sm = PREDELAY, .m2 = M2_CELL, .add = ADD_REG, .clamp = CLAMP_SAT, .wdata = true, .write = true});
		return;
	}
	if (st < 0x174) { unit(c, st, UNITS[0]); return; }     // the unit's four words
	idle(c);                                               // 0x175 and 0x176
}

// 0x178 to 0x183: the first diffusion unit, which runs on the pre-delay rather than on the input.
// Its four words are the same, then the two steps before its write are idle, the read of the slot
// 0x5F rides past and m[60] takes it up five steps later at 0x184, and m[59]'s capture at 0x180 finds
// no read inside its window, so the cell stays at zero.
void first_unit(yss236_core& c, int st)
{
	if (st < 0x17c) { unit(c, st, UNITS[1]); return; }
	if (st == 0x17f) { read_delay(c, st); idle(c); return; }             // the read of the slot 0x5F
	if (st == 0x180) { take_capture(c, EMPTY, st); idle(c); return; }   // m[59], which stays zero
	if (st == 0x183)
	{
		// The unit's write into the slot 0x60, which is whatever 0x17b latched: this word does not
		// carry w1[13], so it spends the latch rather than writing its own result. What it does
		// compute is the first damping lowpass's input, twice the word on the cell nothing fills.
		run(c, {.st = st, .sm = EMPTY, .m2 = M2_CELL, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = DAMP_IN, .write = true});
		return;
	}
	idle(c);                                                  // 0x17c to 0x17e and 0x181, 0x182
}

// ---------------------------------------------------------------------------------------------
// 0x184 to 0x18d: the two damping lowpasses, and the second early reflection's cell
// ---------------------------------------------------------------------------------------------

// One of the two damping lowpasses, the (x, x, a) triple var::REVERB_COMBS[1..3] and [5..7], which
// together are one pole: `state` is its own register, which the a word reads a pass back and the
// second x word overwrites, and the two x words scale `first_reg` or `first_cell` and `second_reg` or
// `second_cell`, one of each pair being zero. The first pole is fed a register and the second a cell,
// and the second pole's first x word is on m[54], which nothing writes.
struct Lowpass
{
	int base;
	int state;
	int first_reg, first_cell;
	int second_reg, second_cell;
	Decor decor[3];
};

constexpr Lowpass LOWPASSES[2] = {
	//  base  state  the first x word     the second x word    the a word and b0's decor
	{ 0x184, DAMP1, DAMP_IN,      0, DAMP_IN,      0, { {DEC_CAPTURE, ER1}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
	{ 0x187, DAMP2,       0, DEAD,          0, ER1,     { {DEC_READ_LFO, 12}, {DEC_NONE, 0}, {DEC_NONE, 0} } },
};

void lowpass(yss236_core& c, int st, Lowpass const& f)
{
	int const k = st - f.base;
	decor(c, st, f.decor[k]);
	switch (k)
	{
	case 0:  term(c, st, f.state); break;                       // the a word on the pole's own state
	case 1:  if (f.first_cell) sum_cell(c, st, f.first_cell);   // the first x word
	        else                sum(c, st, f.first_reg);
	        break;
	default:
	        if (f.second_cell)                                   // and the second closing the pole
	            run(c, {.st = st, .sm = f.second_cell, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = f.state});
	        else
	            close(c, st, f.second_reg, f.state, ROUTE_2);
	        break;
	}
}

// 0x18a to 0x18d: nothing but the second early reflection's cell. The read of the slot 0x62 leaves
// 0x18b for m[62] to take up at 0x190, the read of the slot 0x61 at 0x187 for m[61] to take up here,
// and the other two steps are idle, 0x18a among them: its M_PREVIOUS write of m[54] is on a class-0
// step, which step() returns before, so nothing ever writes m[54].
constexpr Decor ER_CELL[4] = {
	//  0x18a, the dead step   the slot 0x62   m[61]     0x18d
	{DEC_NONE, 0}, {DEC_READ, 0x62}, {DEC_CAPTURE, 61}, {DEC_NONE, 0},
};

// ---------------------------------------------------------------------------------------------
// 0x18e to 0x19f: the two output sums, the pan and the two comb gains
// ---------------------------------------------------------------------------------------------

// 0x18e to 0x193: the two output sums, one for each channel. The right one is two early reflections with
// the base program's level r[119] behind the first, closing into r[120]; the left one is the early
// reflection m[62] closing the tap a pass back and the cell m[59] into r[121]. Two words sit between
// them: 0x190 is the class-3 step that carries the pre-delay tap m[57] onto the running value, which
// 0x191 then replaces with its own word on m[55] as var::REVERB_W[0] and publishes m[57] into m[55]
// again, and m[55] is the tap 0x165 hands the base program.
void output_sums(yss236_core& c, int st)
{
	switch (st)
	{
	// var::REVERB_W[9]: the first early reflection m[60], with the level r[119] added behind it
	case 0x18e:
		run(c, {.st = st, .sr = LEVEL, .sm = ER1, .m2 = M2_CELL, .add = ADD_REG});
		break;
	// var::REVERB_W[8]: the second early reflection m[61] closing that into r[120], with the read of
	// the slot 0x63 riding on it for m[31] to take up at 0x194
	case 0x18f:
		read_delay(c, st);
		run(c, {.st = st, .sm = ER2, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = TAP2});
		break;
	// the pre-delay tap on the running value, and the read of the slot 0x62 published in m[62]
	case 0x190:
		take_capture(c, ER3, st);
		run(c, {.st = st, .sm = PREDELAY, .m2 = M2_CELL, .unity = true});
		break;
	// var::REVERB_W[0]: the tap a pass back scaled, and republished as this pass's own
	case 0x191:
		run(c, {.st = st, .sm = PRE_BACK, .m2 = M2_CELL, .dm = PRE_BACK, .prev = true});
		break;
	// var::REVERB_W[6]: the cell m[59], which stays at zero, onto that
	case 0x192:
		sum_cell(c, st, EMPTY);
		break;
	default:  // var::REVERB_W[7]: the early reflection m[62] closing the left sum into r[121]
		read_delay(c, st);            // the slot 0x64, which m[32] takes up at 0x198
		run(c, {.st = st, .sm = ER3, .m2 = M2_CELL, .add = ADD_ACC, .route = ROUTE_2, .clamp = CLAMP_SAT, .dr = SUM_L});
		break;
	}
}

// One channel's two pan words, var::REVERB_PAN. The first scales the input the base program left this
// window and the second adds that channel's output sum on top, quadrupled and saturated into the
// register the base program reads back.
struct Pan
{
	int base;
	int in;         // the input the base program leaves this window in
	int sum;        // the output sum the wet word adds
	int out;        // the register the pair closes into, r[15] or r[16]
	Decor decor[2];
};

constexpr Pan PANS[2] = {
	//  base  in  sum   out   the input word      the sum word
	{ 0x195,   5, SUM_L,  15, { {DEC_NONE, 0},    {DEC_NONE, 0}     } },
	{ 0x197,   6, TAP2,  16, { {DEC_READ, 0x65}, {DEC_CAPTURE, 32} } },
};

void pan(yss236_core& c, int st, Pan const& p)
{
	decor(c, st, p.decor[st - p.base]);
	if (st == p.base) term(c, st, p.in);            // the input word, on the channel's own input
	else close(c, st, p.sum, p.out, ROUTE_4);      // and the sum word closing the pair, quadrupled
}

// The two comb gains, var::REVERB_COMBS[0] and [4], which are the RT60 gains the damping loss scales.
// Each applies its word to the first unit's tap m[63] with that comb's own damper state r[106] or
// r[108] added to it behind, saturates the sum, and writes it into the slot the early reflections
// hang off, which is the word's own step over four: the slots 0x66 and 0x67, which combDelays2() gives
// the four taps 0x5E, 0x61, 0x5F and 0x62 as offsets from. Each carries w1[13] and is itself a
// writer, so it writes its own result.
const int COMB_DAMPER[2] = { DAMP1, DAMP2 };

void comb_gain(yss236_core& c, int st)
{
	if (st == 0x19c) { take_capture(c, 33, st); idle(c); return; }   // m[33], the read of the slot 0x65
	if (st != 0x19b && st != 0x19f) { idle(c); return; }            // the four steps between
	run(c, {.st = st, .sr = COMB_DAMPER[(st - 0x19b) / 4], .sm = TAP1, .m2 = M2_CELL, .add = ADD_REG, .clamp = CLAMP_SAT, .wdata = true, .write = true});
}

// ---------------------------------------------------------------------------------------------
// The run of steps 0x158 to 0x19f, the whole of the window's program
// ---------------------------------------------------------------------------------------------

// The loop is one iteration per step and carries the m_skip guard step() opens with, so a gate in the
// base program's 0x1xx half skips these steps as it would the interpreter's. Each test below is one of
// the groups the head of this file opens.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{

		if (st < 0x15a) { inputs(c, st); return; }                          // 0x158, 0x159
		if (st < 0x160) { tone(c, st, TONES[(st - 0x15a) / 3]); return; }  // 0x15a to 0x15f
		if (st < 0x165) { level(c, st); return; }                            // 0x160 to 0x164
		if (st < 0x167) { pre_delay_return(c, st); return; }                // 0x165, 0x166
		if (st < 0x16f) { copies(c, st); return; }                           // 0x167 to 0x16e
		if (st < 0x178) { predelay(c, st); return; }                        // 0x16f to 0x177
		if (st < 0x184) { first_unit(c, st); return; }                     // 0x178 to 0x183
		if (st < 0x18a) { lowpass(c, st, LOWPASSES[(st - 0x184) / 3]); return; }  // 0x184 to 0x189
		if (st < 0x18e) {                                                  // 0x18a to 0x18d
			decor(c, st, ER_CELL[st - 0x18a]);
			idle(c);
			return;
		}
		if (st < 0x194) { output_sums(c, st); return; }                        // 0x18e to 0x193
		if (st == 0x194) { take_capture(c, 31, st); idle(c); return; }        // m[31]
		if (st < 0x199) { pan(c, st, PANS[(st - 0x195) / 2]); return; }        // 0x195 to 0x198
		comb_gain(c, st);                                                      // 0x199 to 0x19f
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m10(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects
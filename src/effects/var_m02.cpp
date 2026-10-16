// effects/var_m02.cpp - VOP3-2, the variation window's program for Distortion, Overdrive and Amp Sim:
// its 72 steps.
//
// rom::VARIATION_PROGRAMS[2], the program the window holds for the three variation types 17 to 19, and the
// only program among the twelve that these three share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 2 to
// Distortion, Overdrive and Amp Sim and to nothing else. Under each of them the same program words run and
// only the constants and the delay offsets move, so this native is one function for all three and no type
// is excluded. Which of the three is running is not a code difference here; the host's parameters are.
//
// The host writes this program's coefficients through the word lists in fx_drive.cpp's DRIVES[0] and
// rom_tables.cpp's var:: namespace, and those lists name most of the steps below: DIST_DRIVE
// {0x158, 0x15b, 0x15f, 0x180}, DIST_INPUT {0x15e, 0x15c, 0x15d}, DIST_OUTPUT {0x198, 0x196, 0x197},
// DIST_LOW_SHELF {0x18f, 0x18c, 0x18b, 0x18d, 0x18e}, DIST_PEAK {0x18a, 0x187, 0x186, 0x189, 0x188},
// DIST_LPF2 {0x185, 0x182, 0x181, 0x184, 0x183}, DIST_BAND {0x15f, 0x162, 0x166, 0x16c, 0x170, 0x173,
// 0x174, 0x175, 0x176, 0x177, 0x178, 0x179} and DIST_PAN {0x191, 0x192, 0x193, 0x194}. What the steps
// compute, in the groups the comments open:
//
//   0x158-0x15e  the drive and the input highpass. 0x158 scales the feedback register r[108] by the
//                drive gain the host derives from the Drive parameter, 0x159 and 0x15a add the two
//                serial inputs into r[103], and 0x15b is a pole on r[119] the same drive drives.
//                0x15c to 0x15e are the input highpass's three words (DIST_INPUT), a biquad whose two
//                input cells m[2] and m[6] this window publishes for it.
//   0x15f-0x166  the rectifier. 0x15f is the second pole the drive drives, 0x161 takes the running
//                value's absolute value into m[56], which is the level everything below works against,
//                0x162 and 0x166 are poles, 0x164 subtracts that level from the signal with negatives
//                clamped away, and 0x165 is the level's one-pole follower into r[105].
//   0x167-0x16e  the same pair again for the second half of the shaper: 0x168 to 0x16b mirror the
//                rectifier into r[106], 0x16c is a pole and 0x16e scales r[106] into r[124].
//   0x16f-0x178  the two class-1 loads and the band sum. 0x16f loads the running value into r[125] and
//                0x171 adds its own constant to r[124], both class-1 steps (s24: a class-1 load reads
//                back as v / 256, which is why the 128 * k form appears). 0x170 and 0x173 are poles, and
//                0x174 to 0x178 sum the five poles r[119] to r[123], the twelve band words DIST_BAND
//                ending here.
//   0x179-0x18f  the tone stack and the ladder. 0x180 is the gain the host derives from the drive and
//                the output level. 0x181 to 0x185 are the second lowpass (DIST_LPF2), 0x186 to 0x18a the
//                mid peak (DIST_PEAK) and 0x18b to 0x18f the low shelf (DIST_LOW_SHELF): three filters of
//                five words each, every one of them on the last one's output. The four rcopy steps 0x183,
//                0x188, 0x18d and 0x18e each write the previous step's x operand, and the first three are
//                what hold r[110], r[112] and r[114], the one-pass delays those two-tap structures close
//                on. r[116] is a fourth, on its own, holding the low shelf's own output a pass back.
//   0x190-0x19f  the dry and wet. 0x190 is the one step here the host parameterises for neither this
//                family nor the others: it takes r[118] into the t file as a 1.15 word, and 0x195 reads
//                t[9] back as the multiplier that scales r[104] into r[107]. 0x191 to 0x194 are the pan
//                and dry/wet words (DIST_PAN) and the two outputs r[15] and r[16]. 0x196 to 0x198 are the
//                output lowpass (DIST_OUTPUT), 0x199 and 0x19a are the two class-3 steps that publish the
//                inputs into m[54] and m[55], and 0x19c publishes the last read.
//
// Inside those runs the program repeats itself, so what is below is tables and loops rather than 72
// steps. POLES is the eight steps that are a pole's gain of 16 from one pole register to the next.
// PUBLISHED is the six delay reads the window publishes and the cell each one lands in five steps later.
// FILTERS is the three five-step filters of the tone stack, the same five words apart in registers. The
// band sum's five terms are a loop over r[119] to r[123], and the tail from 0x19b is step::idle() over a
// range test.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs the base program
// writes, r[15] and r[16] the two outputs, which the base program reads as the stereo pair after r[13]
// and r[14], r[103] the input sum and r[104] the input highpass's output, r[105] and r[106] the
// rectifier's two followers, r[107] the level term, r[108] the output filter's tail and the register the
// drive gain reads, r[109], r[111], r[113] and r[115] the ladder's four stage outputs with r[110], r[112],
// r[114] and r[116] the four one-pass delays between them, r[117] the biased signal and r[118] its
// one-pole, and r[119] to r[123] the five poles the band sum adds. The cells: m[2] and m[6] the serial
// inputs this window publishes, m[11] and m[12] the two LFO values, m[28] to m[30] and m[31] to m[33] the
// six published reads, m[54] and m[55] the two inputs echoed into cells, and m[56] the level the
// rectifier works against. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring
// pointer with no index; this program writes none of them, so the lines behind these taps are filled
// elsewhere.
//
// The one t register the program uses is t[9], written at 0x190 and read at 0x195.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and effects/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m02: identical, 3 types, 28800 native passes.

#include "effects/programs.h"
#include "effects/steps.h"

namespace fs1r::effects {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// The tables of the program's repeated units
// ---------------------------------------------------------------------------------------------

// The one t register the program uses: 0x190 writes the level into it and 0x195 multiplies by it.
constexpr int T_LEVEL = 9;

// The first of the five pole registers r[119] to r[123] that the band sum adds.
constexpr int BAND_FIRST = 119;

// How far after a published read its capture lands: captured()'s lower bound, which is the only distance
// this program publishes anything at, so the capture index below is the read index walked this far on.
constexpr int PUBLISH_LAG = 5;

// A pole: r[out] = saturate(16 * k * r[in]), which is the shape every pole in this program has. The
// last five rows are the chain the band sum adds, one register to the next and back to the first; the
// first three are the drive's own, 0x158 seeding the chain out of r[108] and 0x15b and 0x15f folding a
// published delay read into r[119] on the way past.
struct Pole { int st; int in, out; };

constexpr Pole POLES[8] = {
	{ 0x158, 108, 119 },
	{ 0x15b, 119, 119 },
	{ 0x15f, 119, 119 },
	{ 0x162, 119, 120 },
	{ 0x166, 120, 121 },
	{ 0x16c, 121, 122 },
	{ 0x170, 122, 123 },
	{ 0x173, 123, 119 },
};

// Which pole a step is, so the step reaches its pole rather than looking through the eight for it. The
// index is the same on every pass and under all three types.
constexpr auto POLES_AT = index_by_step(POLES);

// One delay read the window publishes for the base program and the cell it lands in. The two are five
// steps apart on every one of them, which is captured()'s lower bound, and a read's slot is its own step
// over four, so the step below is all the offset the table carries.
struct Published { int st, cell; };

constexpr Published PUBLISHED[6] = {
	{ 0x15b, 28 },   // the slot 0x56
	{ 0x15f, 29 },   // 0x57
	{ 0x163, 30 },   // 0x58
	{ 0x18f, 31 },   // 0x63
	{ 0x193, 32 },   // 0x64
	{ 0x197, 33 },   // 0x65
};

// The capture index: PUBLISHED's own indices at the step each read's capture lands at, and -1 everywhere
// else, which is what the old scan answered at a step that is a read and nothing but a read. It is built
// from the read table itself rather than from a second table of captures, so the two cannot fall out of
// step, and it hands back PUBLISHED's indices: a step that carries a capture reaches its row for both.
template <int N>
constexpr std::array<int16_t, yss236_core::STEPS> index_by_lag(Published const (&rows)[N], int lag)
{
	std::array<int16_t, yss236_core::STEPS> ix{};
	for (int16_t& i : ix) i = -1;
	for (int i = 0; i < N; i++) ix[rows[i].st + lag] = int16_t(i);
	return ix;
}

// Which read a step is, and which of their captures lands at it, so a step reaches both rather than
// looking through the six for them. The index is the same on every pass and under all three types.
constexpr auto PUBLISHED_AT = index_by_step(PUBLISHED);
constexpr auto CAPTURES_AT = index_by_lag(PUBLISHED, PUBLISH_LAG);

// What a filter's step carries besides its term: the delay RAM read it makes, which is its own slot, and
// the LFO it publishes for the base program.
struct Decor { int slot, lfo; };

// One filter of the tone stack, in five steps. `in` is the register the stage before it wrote and the one
// its closing step scales, `x1` holds that input a pass older, and `y1` and `y2` are what the fourth and
// third steps add: the two taps on the filter's own output. On the first two filters y1 is a pass back
// and y2 two passes back, and y1 is the filter's own output register; on the low shelf the order is the
// other way round and y1 is a register of its own, which is what holds_y1 says.
struct Filter
{
	int base;                 // the first of the five steps
	int in, x1, y1, y2, out;  // the registers the five steps move, in the order they move them
	route_t route;            // the closing step's route
	bool holds_y1;            // the y1 tap keeps its own delay register as well as adding it
	Decor decor[5];
};

// The three filters of the tone stack, DIST_LPF2, DIST_PEAK and DIST_LOW_SHELF, each one on the last
// one's output. The low shelf's closing step is the odd one: it carries a delay read and the LFO of page
// 12 on the way past, it takes the x16 route the other two do not, and its y1 tap is the fourth of the
// rcopy steps the header above counts, the one on its own.
constexpr Filter FILTERS[3] = {
	//  base   in   x1   y1   y2   out  route     holds_y1  decor per step: the slot read, then the LFO
	{  0x181, 109, 110, 111, 112, 111, ROUTE_2,  false, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
	{  0x186, 111, 112, 113, 114, 113, ROUTE_2,  false, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
	{  0x18b, 113, 114, 116, 115, 115, ROUTE_16, true,  { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0x63, 12 } } },
};

// ---------------------------------------------------------------------------------------------
// The repeated units
// ---------------------------------------------------------------------------------------------

// The delay reads this window publishes and the captures that take them up five steps later. Both come
// before the step's arithmetic, because step() moves a cell and reads the delay RAM before the
// multiplier runs. No step is both a read and a capture, so one lookup each settles it.
void published(yss236_core& c, int st)
{
	if (int i = PUBLISHED_AT[st]; i >= 0) read_delay(c, st);
	if (int i = CAPTURES_AT[st]; i >= 0) take_capture(c, PUBLISHED[i].cell, st);
}

// One of the program's pole steps, if this is one of them: the caller moves on when this says so.
bool pole(yss236_core& c, int st)
{
	int const i = POLES_AT[st];
	if (i < 0) return false;
	gain(c, st, POLES[i].in, POLES[i].out, ROUTE_16);
	return true;
}

// One filter's step. The decor the step's own words carry comes first, and then the filter's term: the
// five words are the input a pass old, the input itself, the y2 tap, the y1 tap, and the input again
// closing the sum into the output.
void filter(yss236_core& c, int st, Filter const& f, int k)
{
	if (f.decor[k].slot) read_delay(c, st);
	if (f.decor[k].lfo) take_lfo(c, f.decor[k].lfo, st);
	switch (k)
	{
	case 0: term(c, st, f.x1); break;                               // x[n-1], held in r[x1]
	case 1: sum(c, st, f.in); break;                                // x[n]
	case 2: copy_z(c, st, f.y2, f.x1, ADD_ACC, CLAMP_WRAP); break;  // the y2 tap, keeping x[n-1] for step 0
	case 3:
		// The y1 tap. The low shelf's is a register of its own rather than its output, so this step
		// keeps that delay as well as adding it.
		if (f.holds_y1) copy_z(c, st, f.y1, f.y1, ADD_ACC, CLAMP_WRAP);
		else            sum(c, st, f.y1);
		break;
	default: close(c, st, f.in, f.out, f.route); break;             // x[n] again, into y[n]
	}
}

// r[dr] = saturate(k * m[sm] + p): a sum closed into a register off a cell rather than off a register,
// which is how a one-pole follower of the rectifier's level is written.
void close_cell(yss236_core& c, int st, int sm, int dr)
{
	run(c, {.st = st, .sm = sm, .m2 = M2_CELL, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = dr});
}

// A class-3 step (s19: the multiplier passes its second input through): a serial input read out of a
// register and echoed into a cell, which this window's own mix reads a pass later.
void echo(yss236_core& c, int st, int sr, int cell)
{
	run(c, {.st = st, .sr = sr, .unity = true, .dm = cell});
}

// ---------------------------------------------------------------------------------------------
// The five runs the step loop dispatches to: the header's groups, with the two shaper halves one run
// ---------------------------------------------------------------------------------------------

// 0x158 to 0x15e, the drive and the input highpass. The poles 0x158 and 0x15b are POLES', and 0x15b's
// delay read with them; what is left is the input sum and the biquad's own three words, DIST_INPUT.
void drive_and_input_hpass(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x159: term(c, st, 5); break;                   // acc = k * r[5], the left input on its own
	case 0x15a: close(c, st, 6, 103, ROUTE_2); break;    // r[103] = 2 (k r[6] + acc): the input sum
	case 0x15c:                                          // m[2] for the base program, then r[103]
		take_serial(c, 2, st, 0);
		term(c, st, 103);
		break;
	case 0x15d:                                          // m[6] for it as well, then r[104]
		take_serial(c, 6, st, 1);
		sum(c, st, 104);
		break;
	case 0x15e: close(c, st, 103, 104, ROUTE_2); break;  // r[104] = 2 (k r[103] + acc), the biquad's word
	}
}

// 0x15f to 0x16e, the rectifier and the same pair again for the second half of the shaper. The poles
// 0x15f, 0x162, 0x166 and 0x16c are POLES', and 0x15f's delay read and 0x160's and 0x163's with them.
void rectifier(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x160: idle(c); break;                         // the capture of 0x15b lands here and nothing else
	case 0x161:
		// The level everything below works against: the input highpass's output with its absolute value,
		// published into m[56] for the two shapers to take off the signal.
		run(c, {.st = st, .sr = 104, .route = ROUTE_16, .clamp = CLAMP_ABS, .dm = 56});
		break;
	case 0x163: idle(c); break;                         // the read of the slot 0x58 and nothing else
	case 0x164:
		// The first shaper's difference: r[105] scaled by its own word with the level off it, negatives
		// clamped away so the shaper only ever folds the signal down.
		run(c, {.st = st, .sr = 105, .sm = 56, .add = ADD_CELL, .sub = true, .clamp = CLAMP_ZERO});
		break;
	case 0x165: close_cell(c, st, 56, 105); break;      // r[105]: the level's one-pole follower
	case 0x167:                                         // m[11] for the base program, then r[106]
		take_lfo(c, 11, st);
		term(c, st, 106);
		break;
	case 0x168:
		// The second shaper's input. Its adder takes m[0], which is no cell, so the running value enters
		// as acc / 32768: the accumulator read back as a cell word would read it.
		run(c, {.st = st, .sr = 106, .add = ADD_CELL});
		break;
	case 0x169:
		// The level off the running value this time, and off r[106] at the step below.
		run(c, {.st = st, .sm = 56, .m2 = M2_CELL, .add = ADD_ACC, .sub = true});
		break;
	case 0x16a:
		run(c, {.st = st, .sr = 105, .add = ADD_ACC, .sub = true, .clamp = CLAMP_ZERO});
		break;
	case 0x16b: close_cell(c, st, 56, 106); break;      // r[106]: the same follower, the other shaper
	case 0x16d: idle(c); break;                         // class 0, no RAM mark and no ALU
	case 0x16e: gain(c, st, 106, 124, ROUTE_1); break;  // r[124] = k r[106], the shaper's output
	}
}

// 0x16f to 0x178, the two class-1 loads and the band sum. The poles 0x170 and 0x173 are POLES'.
void band_sum(yss236_core& c, int st)
{
	if (st >= 0x174)
	{
		// The band sum: the five poles the chain above ran, each with its own word out of DIST_BAND,
		// added into one running value. The first opens the sum and the rest add to it.
		int const i = st - 0x174;
		if (i == 0) term(c, st, BAND_FIRST);
		else        sum(c, st, BAND_FIRST + i);
		return;
	}
	switch (st)
	{
	case 0x16f:
		// s24: a class-1 load reads back as v / 256, which is why the chip's 128 * k appears as one
		// constant here. This one adds the running value the pole chain left and keeps the result.
		load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = 125});
		break;
	case 0x171:
		// The same load off its own register, so its adder's input is r[124] and not the running value:
		// the shaper's output with a constant added.
		load(c, {.st = st, .sr = 124, .add = ADD_REG, .clamp = CLAMP_SAT, .dr = 124});
		break;
	case 0x172: idle(c); break;  // class 0, no RAM mark and no ALU
	}
}

// 0x179 to 0x18f, the tone stack and the ladder. The three filters are FILTERS', fifteen steps of this
// run; what is left is the gain the host derives, the bias the ladder works against and its follower.
void tone_stack(yss236_core& c, int st)
{
	if (st >= 0x181)
	{
		Filter const& f = FILTERS[(st - 0x181) / 5];
		filter(c, st, f, st - f.base);
		return;
	}
	switch (st)
	{
	case 0x179: close(c, st, 119, 119, ROUTE_1); break;  // r[119] = k r[119] + acc: the chain's trim
	case 0x17a: term(c, st, 125); break;                 // the class-1 load's own output, unscaled
	case 0x17b:
		// The same sum with the x16 route and no register of its own: the ladder's input, saturated.
		run(c, {.st = st, .sr = 124, .add = ADD_ACC, .route = ROUTE_16, .clamp = CLAMP_SAT});
		break;
	case 0x17c:
		// The bias the ladder works against, a class-1 constant with its negatives clamped away.
		load(c, {.st = st, .add = ADD_ACC, .route = ROUTE_16, .clamp = CLAMP_ZERO, .dr = 117});
		break;
	case 0x17d: term(c, st, 117); break;
	case 0x17e: sum(c, st, 118); break;
	case 0x17f: close(c, st, 117, 118, ROUTE_1); break;  // r[118]: the bias's one-pole follower
	case 0x180: gain(c, st, 119, 109, ROUTE_1); break;   // r[109], the gain the host derives: in
	}
}

// 0x190 to 0x19f, the dry and wet, the pan, the outputs and the tail.
void dry_and_wet(yss236_core& c, int st)
{
	if (st >= 0x19b)
	{
		// The tail: five idle steps, class 0 throughout, and nothing else. The capture that takes up
		// 0x197's read lands at 0x19c, and PUBLISHED has already put it in m[33] by the time we get here.
		idle(c);
		return;
	}
	switch (st)
	{
	case 0x190:
		// The level, and the t word the output section scales by: t[9], the one t register this program
		// uses, which 0x195 reads back.
		t_put(c, st, T_LEVEL, run(c, {.st = st, .sr = 118, .route = ROUTE_2, .clamp = CLAMP_SAT}));
		break;
	case 0x191: term_cell(c, st, 54); break;             // the left input, out of m[54]
	case 0x192: close(c, st, 115, 15, ROUTE_4); break;   // r[15] = 4 (k r[115] + acc): the left output
	case 0x193: term_cell(c, st, 55); break;             // the right input, out of m[55]
	case 0x194: close(c, st, 115, 16, ROUTE_4); break;   // r[16] = 4 (k r[115] + acc): the right one
	case 0x195:
		// r[107] = t[9] * r[104]: the level term, the input highpass's output scaled by the word 0x190
		// left in the t file.
		run(c, {.st = st, .sr = 104, .treg = T_LEVEL, .clamp = CLAMP_SAT, .dr = 107});
		break;
	case 0x196: term(c, st, 107); break;
	case 0x197: sum(c, st, 108); break;                  // its read of the slot 0x65 is PUBLISHED's
	case 0x198: close(c, st, 107, 108, ROUTE_2); break;  // r[108]: the output filter's tail
	case 0x199: echo(c, st, 5, 54); break;  // the two class-3 steps, echoing the two inputs into cells
	case 0x19a: echo(c, st, 6, 55); break;  // the pan words at 0x191 and 0x193 read a pass later
	}
}

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
	for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
	{
		published(c, st);  // a delay read or the capture of one, before any arithmetic
		if (pole(c, st)) return;
		if (st < 0x15f)      drive_and_input_hpass(c, st);
		else if (st < 0x16f) rectifier(c, st);
		else if (st < 0x179) band_sum(c, st);
		else if (st < 0x190) tone_stack(c, st);
		else                 dry_and_wet(c, st);
		});
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m02(yss236_core& c, int from, int to)
{
	if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::effects

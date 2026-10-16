// fs1r/native/ins_m04.cpp - VOP3-2, the insertion window's program for Auto Wah + Distortion, Auto Wah +
// Overdrive, Touch Wah + Distortion and Touch Wah + Overdrive: its 96 steps.
//
// rom::INSERTION_PROGRAMS[4], the program the insertion window holds under the four types
// fx_dispatch.cpp's INSERTION_ROUTES files at msb 4: ins-14 A-Wah+Dist and ins-15 A-Wah+Odrv on
// EffectHost::autoWah, ins-16 T-Wah+Dist and ins-17 T-Wah+Odrv on EffectHost::touchWah. The same 96
// words run under every one of them; what the type changes is which words fx_drive.cpp's two handlers
// write, so nothing here knows which one is running. Nothing in this program touches the delay RAM.
//
// Every constant the program reads is read from c.coef[st] on the pass, so no number the firmware chose
// is folded in here: the program words are fixed per row and each step's fields are spelled out below
// with fs1r/native/steps.h's shapes, in the order yss236_core::step() runs its own.
//
//   0x1a0-0x1ad  the two channels' EQ, two order-1 shelves each, the low and the high shelf of
//                fx_common.cpp's shelves() over ins::LPF_WAH, ins::HPF_WAH and their copies
//                ins::LPF_WAH_C and ins::HPF_WAH_C. 0x1a0 and 0x1a7 take the two inputs the base
//                program leaves the window into r[67] and r[69], and 0x1a5, 0x1a6 and 0x1a7 take the
//                serial inputs of page 13 and that page's LFO into m[3], m[7] and m[13]. The high
//                shelves' outputs are r[68] and r[70] and the low shelves' are the cells m[42] and
//                m[43].
//   0x1ae-0x1b0  ins::WAH_MODE's pair: r[76] takes the square root of a half times m[48] and m[49],
//                the two channels' wah outputs summed into one signal for the band filters, and
//                0x1af and ins::WAH_HPF_2's b1 step at 0x1b0 publish m[50] and m[51], that mix one
//                pass old, for the dry side of the output below.
//   0x1b0-0x1b2  ins::WAH_HPF_2, the distortion's fixed input highpass, over r[76] into r[77].
//   0x1b3-0x1b5  ins::WAH_LPF_2, its fixed input lowpass, over r[77] into r[78].
//   0x1b6-0x1bb  ins::WAH_MODE2's pair, which only touchWah() writes, and ins::WAH_HPF, the input
//                highpass it designs, over r[71] into r[72]. Under Auto Wah the pair's two words are
//                zero, r[71] and r[72] stay zero and this path is silent.
//   0x1b8-0x1c0  the drive. ins::WAH_DRIVE's first word at 0x1b8 carries the signal into r[122], and
//                the two words fx_common.cpp's storeDrive() writes, 0x1bc and 0x1c0, square it.
//   0x1bd-0x1c2  ins::WAH_SENSE's pair on each channel: acc = k * m[44] - r[68] and then the sense
//                word times r[74] less that, which is what the resonator's feedback is taken from, so
//                0x1c1 and 0x1c2 are the same two steps on the right over m[45] and r[70].
//   0x1bf        ins::WAH_ATTACK, the Touch Wah's sensitivity word times the input highpass's output,
//                rectified into m[56].
//   0x1c3-0x1cc  ins::WAH_MIRROR's three words, the rectifier round m[56], and ins::WAH_LPF, the
//                input lowpass touchWah() designs, which runs over that level in m[46] and m[47].
//   0x1cd-0x1dc  four of ins::WAH_BAND's twelve words, the waveshaper's: 0x1cd is the gain
//                fx_design.cpp's storeEdge() derives and 0x1d1, 0x1d4, 0x1d8 and 0x1dc are the shape
//                word it cascades, the last of them landing in m[57].
//   0x1ce-0x1d7  the sweep. 0x1ce is ins::WAH_CUTOFF's class-1 load, 0x1cf ins::WAH_DEPTH's word
//                times the page's LFO on top of it, and 0x1d0 to 0x1d7 the one-pole smoother that
//                turns that, with the level in m[47], into t[9], with 0x1d2, 0x1d5 and 0x1d9 the
//                padding between them.
//   0x1da-0x1db  the resonator itself: r[74] and r[75] take t[9] times m[58] and m[59] plus
//                themselves, which is the state the wah filters with.
//   0x1dc-0x1e2  the rest of ins::WAH_BAND: 0x1dd carries the zero storeEdge() leaves in its seventh
//                word, and 0x1de to 0x1e2 are the five terms of the series, each scaled by the
//                6553 storeEdge() writes, summed into r[122].
//   0x1e3-0x1e5  the resonator's two feedback accumulators m[44] and m[45], which 0x1bd and 0x1c1
//                subtract, and ins::WAH_DRIVE's fourth word, the gain storeDrive() derives from the
//                drive, into r[79].
//   0x1e6-0x1ea  ins::WAH_LPF2, the lowpass fx_drive.cpp's wahDistortionTone() designs: b0 at 0x1ea,
//                b1 at 0x1e7, b2 at 0x1e6, -a1 at 0x1e9 and -a2 at 0x1e8, over r[79] into r[81].
//   0x1eb-0x1ef  ins::WAH_PEAK, the same five steps over r[81] into r[83].
//   0x1f0-0x1f4  ins::WAH_LOW_SHELF2, the same five steps over r[83] into r[85].
//   0x1f5-0x1fc  ins::WAH_WET's eight words. 0x1f5 and 0x1f6 mix the dry word times r[68] with the
//                wet word times r[74] into m[48], and 0x1f7 and 0x1f8 the same for the right channel
//                into m[49]; 0x1f9 to 0x1fc take the dry words on m[50] and m[51] instead and the
//                wet word on the tone chain's output r[85], and put the sums into the window's outputs
//                r[17] and r[18].
//   0x1fd-0x1ff  three class-0 steps with no ALU, so only the x latch runs.
//
// So the wah and the distortion are two chains and not one: the EQ feeds the resonator r[74] and
// r[75], whose state the band filters and then the drive and the waveshaper take, and the output mix
// puts the resonator's own signal back beside the shaper's. The old model in vop3_effects.h runs a
// biquad whose cutoff the LFO or the envelope sweeps and then a waveshaper on its output, with the
// drive and the edge hand-written as a power law; it has none of this program's words, so it is no
// reference for any number here.
//
// The registers: r[7] and r[8] the two signals the base program leaves the window; r[67] and r[69] the
// two channels' EQ inputs and r[68] and r[70] the two channels' EQ outputs; r[71], r[72] the Touch
// Wah's input highpass and its output; r[73] the rectifier's output; r[74], r[75] the resonator's two
// states; r[76] the mode pair, r[77] the band highpass and r[78] the band lowpass; r[79] the drive's
// gain, r[81], r[83] and r[85] the three tone filters' outputs; r[80], r[82], r[84] and r[86] their
// four delay registers; r[121] the smoother's state; r[122] the drive, then the waveshaper's output;
// r[123] to r[125] the shaper's cascade; r[17] and r[18] the two outputs. The cells: m[3], m[4] the
// serial inputs this block takes for the base program, m[7] the second bank's, m[13] the page's LFO,
// m[42] and m[43] the two low shelves' outputs, m[44] and m[45] the resonator's feedback
// accumulators, m[46] and m[47] the level pair, m[48] and m[49] the two channels' wah outputs, m[50]
// and m[51] the same mix one pass old, m[56] the sensitivity level, m[57] the shaper's fourth term and
// m[58], m[59] the sense feedback.
//
// The one t register the program uses is t[9], written at 0x1d0, 0x1d3 and 0x1d7 and read at 0x1d3,
// 0x1d6, 0x1da, 0x1db, 0x1e3 and 0x1e4: the cutoff the host wrote at 0x1ce, smoothed, and then the
// gain the resonator runs at.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check ins-m04: identical, 4 types, 38400 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z, gain, load and the rest

// ---------------------------------------------------------------------------------------------
// The registers and cells the header names, so the arithmetic below can say what it reads
// ---------------------------------------------------------------------------------------------

enum
{
	SIG_L = 7,        // the two signals in, r[7] and r[8]: the panned sends the base program leaves here
	SIG_R = 8,
	OUT_L = 17,       // the two outputs the base program reads back
	OUT_R = 18,
	EQ_L_IN = 67,     // the left channel's EQ input, the register ins::LPF_WAH and ins::HPF_WAH read
	EQ_L = 68,        // and the left channel's EQ output, ins::HPF_WAH's own register
	EQ_R_IN = 69,
	EQ_R = 70,
	IN_LPF = 71,      // ins::WAH_MODE2's pair summed into r[71], the Touch Wah's input highpass
	IN_HPF = 72,      // and its output, ins::WAH_HPF's
	RECT = 73,        // the rectifier's output, which the mirror differences against the level
	RES_L = 74,       // the resonator's two states, which the cutoff's gain runs into
	RES_R = 75,
	MODE = 76,        // ins::WAH_MODE's pair: the two channels' wah signals as one signal for the bands
	BAND_HPF = 77,    // ins::WAH_HPF_2's output, the distortion's input highpass
	BAND_LPF = 78,    // and ins::WAH_LPF_2's, the band lowpass the drive is taken from
	DRIVE_GAIN = 79,  // ins::WAH_DRIVE's fourth word, and the tone chain's input
	TONE_LPF = 81,    // ins::WAH_LPF2's output, and ins::WAH_PEAK's input
	TONE_PEAK = 83,   // ins::WAH_PEAK's output, and ins::WAH_LOW_SHELF2's input
	TONE_SHELF = 85,  // ins::WAH_LOW_SHELF2's output, and the wet share of both output mixes
	SMOOTH = 121,     // the cutoff smoother's state
	DRIVE = 122,      // the drive, then the waveshaper's output
	EDGE_1 = 123,     // ins::WAH_BAND's cascade, the three shape words the shaper runs on
	EDGE_2 = 124,
	EDGE_3 = 125,
	SERIAL_L = 3,     // m[3] and m[7] the two banks of page 13, and m[4] page 14's left input
	SERIAL_R = 7,
	SERIAL_2 = 4,
	SHELF_L = 42,     // m[42] and m[43], the two low shelves' outputs
	SHELF_R = 43,
	RES_FB_L = 44,    // m[44] and m[45], the resonator's two feedback accumulators
	RES_FB_R = 45,
	LEVEL_L = 46,     // m[46] and m[47], the level pair the mirror ends in and the smoother reads
	LEVEL_R = 47,
	WAH_L = 48,       // m[48] and m[49], the two channels' wah signals
	WAH_R = 49,
	WAH_L_OLD = 50,   // and m[50] and m[51], the same two one pass old: the dry side of the outputs
	WAH_R_OLD = 51,
	ATTACK = 56,      // m[56], the Touch Wah's sensitivity level
	EDGE_4 = 57,      // m[57], the shaper's fourth shape word
	SENSE_L = 58,     // m[58] and m[59], what the two sense steps leave, which drive the resonator
	SENSE_R = 59,
	LFO_CELL = 13,    // m[13], this page's own LFO, which the cutoff's depth word multiplies
	CUTOFF_T = 9,     // the one t register the program uses, the smoothed cutoff
};

// ---------------------------------------------------------------------------------------------
// What a step carries besides its arithmetic
// ---------------------------------------------------------------------------------------------

// Four of the 96 steps move a cell on their way past, and none of them touches the delay RAM: three
// take the serial inputs the base program reads and the last takes this page's own LFO, which the
// cutoff's depth word multiplies at 0x1cf. The take comes before the step's arithmetic, because step()
// moves a cell before it multiplies, adds or loads anything.
enum decor_t { DEC_NONE, DEC_SERIAL_L, DEC_SERIAL_R, DEC_LFO };

struct Decor { int st; decor_t kind; int cell; };

const Decor DECORS[4] = {
    //  the step  the decor       the cell
    {  0x1a5, DEC_SERIAL_L, SERIAL_L },   // on the left channel's high shelf's -a1 step
    {  0x1a6, DEC_SERIAL_R, SERIAL_R },   // and on the step that closes that shelf into r[68]
    {  0x1a7, DEC_LFO,      LFO_CELL },   // on the right channel's first step, the gain off r[8]
    {  0x1c4, DEC_SERIAL_L, SERIAL_2 },   // on the rectifier's step into r[73]
};

void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_SERIAL_L: take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R: take_serial(c, d.cell, st, 1); break;
		case DEC_LFO:      take_lfo(c, d.cell, st); break;
		case DEC_NONE: break;
	}
}

// ---------------------------------------------------------------------------------------------
// The two channels' EQ, 0x1a0 to 0x1ad
// ---------------------------------------------------------------------------------------------

// One channel's EQ, seven steps: the signal the base program leaves this window scaled by the ROM's own
// half word into `in`, then ins::LPF_WAH's three words as an order-1 low shelf whose output is the cell
// `shelf`, then ins::HPF_WAH's three words as an order-1 high shelf over that cell, whose output is the
// register `out`. designFilter emits b0, b1, -a1, and this schedule runs them -a1, b1, b0, so the
// shelf's output is written a step before the high shelf reads it and the low one has to be a cell.
struct Eq { int base, sig, in, shelf, out; };

const Eq EQS[2] = {
    //  base  sig    in        shelf         out
    { 0x1a0, SIG_L, EQ_L_IN,  SHELF_L, EQ_L },      // the left channel, over ins::LPF_WAH and ins::HPF_WAH
    { 0x1a7, SIG_R, EQ_R_IN,  SHELF_R, EQ_R },      // and the right, over their copies
};

// One channel's EQ, step k of seven.
void eq(yss236_core& c, int st, Eq const& e, int k)
{
	switch (k)
	{
	case 0:
		// the channel's signal into the chain, a half at a time, and it saturates as it goes
		gain(c, st, e.sig, e.in, ROUTE_2);
		break;
	case 1:
		term(c, st, e.in);                 // ins::LPF_WAH[1], b1 on the chain's own input
		break;
	case 2:
		sum_cell(c, st, e.shelf);          // ins::LPF_WAH[2], -a1 on the low shelf's output
		break;
	case 3:
		// ins::LPF_WAH[0], b0 closing the low shelf into the cell the high shelf reads
		run(c, {st, e.in, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_2, CLAMP_SAT, 0, false, e.shelf});
		break;
	case 4:
		term_cell(c, st, e.shelf);         // ins::HPF_WAH[1], b1 on the low shelf's output
		break;
	case 5:
		sum(c, st, e.out);                 // ins::HPF_WAH[2], -a1 on the high shelf's own output
		break;
	default:
		// ins::HPF_WAH[0], b0 closing the high shelf into its register, a quarter at a time
		run(c, {st, 0, e.shelf, M2_CELL, false, 0, ADD_ACC, false, ROUTE_4, CLAMP_SAT, e.out});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// ins::WAH_MODE's pair and the two steps that publish the mix one pass old
// ---------------------------------------------------------------------------------------------

// ins::WAH_MODE's pair, 0x1ae and 0x1af: the two channels' wah signals, m[48] and m[49], each scaled by
// the square root of a half the host writes at both steps, into one signal in r[76] for the band filters
// to work from. The closing step also publishes the left channel's own signal, the operand of the step
// before, into m[50], and 0x1b0 publishes the right one's into m[51]: those two cells are the dry side
// of the output mix, one pass old, which is what keeps the wet path's own feedback from ringing.
void mode_pair(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1ae:
		term_cell(c, st, WAH_L);          // ins::WAH_MODE[0]: the left channel's wah signal on its own
		break;
	case 0x1af:
		// ins::WAH_MODE[1]: r[76] = the right channel's wah signal + acc, and m[50] takes the left one
		run(c, {st, 0, WAH_R, M2_CELL, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, MODE,
				false, WAH_L_OLD, true});
		break;
	default:
		// 0x1b0, ins::WAH_HPF_2's own b1 word on r[76], which also hands the right channel's wah signal
		// on one pass old in m[51]
		run(c, {st, MODE, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, 0,
				false, WAH_R_OLD, true});
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The three band sections, 0x1b0 to 0x1b5 and 0x1b9 to 0x1bb
// ---------------------------------------------------------------------------------------------

// One of the three two-word band sections, three steps: b1 on the section's input, -a1 on its own
// output, and b0 closing the pair into that output a quarter at a time. These three are the only
// sections in the program that carry no z^-1: each of them reads its own output register for the -a1
// term, so the section's whole delay is that register one pass back. `open` is the clamp of the opening
// term, which is the one field the schedule does not make the same in all three: the band lowpass's
// saturates where the two highpasses wrap. `publish` is the cell the section's first step also hands
// the step before's operand into, and only the first of the three has one.
struct Band { int base, in, out; route_t route; clamp_t open; int publish; };

const Band BANDS[3] = {
    //  base  in        out         route      open         publish
    { 0x1b0, MODE,     BAND_HPF,   ROUTE_2,   CLAMP_WRAP,  WAH_R_OLD },  // ins::WAH_HPF_2
    { 0x1b3, BAND_HPF, BAND_LPF,   ROUTE_2,   CLAMP_SAT,   0 },           // ins::WAH_LPF_2
    { 0x1b9, IN_LPF,   IN_HPF,     ROUTE_2,   CLAMP_WRAP,  0 },           // ins::WAH_HPF
};

// The band section a step belongs to, or null when it belongs to none: the two the distortion carries
// from r[76] to r[78], and the one the Touch Wah's input passes through on its own way in.
Band const* band_for(int st)
{
	for (Band const& b : BANDS)
		if (st >= b.base && st < b.base + 3) return &b;
	return nullptr;
}

// One band's step k of three.
void band(yss236_core& c, int st, Band const& b, int k)
{
	if (k == 2)
	{
		close(c, st, b.in, b.out, b.route);       // b0 on the input, closing the section into its output
		return;
	}
	if (k == 1)
	{
		sum(c, st, b.out);                         // -a1 on the section's own output
		return;
	}
	if (b.publish)
		// b1 on the input, opening the sum, and the step before's operand hands on one pass old
		run(c, {st, b.in, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_1, b.open, 0, false, b.publish, true});
	else
		run(c, {st, b.in, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_1, b.open});
}

// ---------------------------------------------------------------------------------------------
// ins::WAH_MODE2, the Touch Wah's input highpass, and ins::WAH_DRIVE's two squares
// ---------------------------------------------------------------------------------------------

// ins::WAH_DRIVE's words at 0x1bc and 0x1c0: r[122] = 16 * k * r[122]. storeDrive() writes the same
// word at both, so the signal the drive's first word carried in is squared twice on its way through the
// program, once before each channel's sense pair.
void drive_square(yss236_core& c, int st) { gain(c, st, DRIVE, DRIVE, ROUTE_16); }

// 0x1b6 and 0x1b7 are ins::WAH_MODE2's pair, the two channels' EQ outputs summed into r[71], and 0x1b8
// is ins::WAH_DRIVE's first word: the band lowpass's output a sixteenth at a time into r[122], which is
// what the two squares above then work on. Only touchWah() writes the pair, and it writes zeros, so
// under Auto Wah r[71] stays zero and the input highpass after it is silent: that is the path the Touch
// Wah's sensitivity level is taken through.
void touch_input(yss236_core& c, int st)
{
	if (st == 0x1b6)      term(c, st, EQ_L);                       // ins::WAH_MODE2[0]
	else if (st == 0x1b7) close(c, st, EQ_R, IN_LPF, ROUTE_1);     // ins::WAH_MODE2[1]
	else                  gain(c, st, BAND_LPF, DRIVE, ROUTE_16);   // ins::WAH_DRIVE[0], the signal in
}

// One channel's sense pair. The first step differences the signal that fed the resonator against the
// accumulator that state has been running into, and the second applies ins::WAH_SENSE's word to the
// state, which is the value the resonator's own step at 0x1da runs on. wahSense() writes the same word
// at both steps of the pair.
struct Sense { int base, in, state, feedback, out; };

const Sense SENSES[2] = {
    //  base  in        state  feedback  out
    { 0x1bd, EQ_L,     RES_L, RES_FB_L, SENSE_L },   // the left channel's, and the right's at 0x1c1
    { 0x1c1, EQ_R,     RES_R, RES_FB_R, SENSE_R },
};

// 0x1bc to 0x1c2, with ins::WAH_ATTACK's one step in the middle of the two sense pairs.
void drive_and_sense(yss236_core& c, int st)
{
	if (st == 0x1bc || st == 0x1c0) { drive_square(c, st); return; }
	if (st == 0x1bf)
	{
		// ins::WAH_ATTACK: m[56] = |16 * k * r[72]|, the sensitivity level, which the clamp takes the
		// absolute value of and which the mirror below rectifies
		run(c, {st, IN_HPF, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_ABS, 0, false, ATTACK});
		return;
	}
	Sense const& s = SENSES[(st - SENSES[0].base) / 4];
	if (st == s.base)
		// acc = k * m[feedback] - r[in]: the accumulator against the signal that drove it
		run(c, {st, s.in, s.feedback, M2_CELL, false, 0, ADD_REG, true, ROUTE_1, CLAMP_WRAP});
	else
		// ins::WAH_SENSE[0] and [1]: m[out] = saturate(k * r[state] - acc)
		run(c, {st, s.state, 0, M2_REG, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_SAT, 0, false, s.out});
}

// ---------------------------------------------------------------------------------------------
// The mirror and the level pair, 0x1c3 to 0x1cc
// ---------------------------------------------------------------------------------------------

// ins::WAH_LPF's three words, which are the input lowpass touchWah() designs over the level: b1 on
// m[46], -a1 on m[47], and b0 closing the pair into m[47]. m[47] is what the cutoff's smoother reads at
// 0x1d0, so the level the mirror decays is what the cutoff sweeps on.
void level_pair(yss236_core& c, int st)
{
	switch (st - 0x1ca)
	{
	case 0: term_cell(c, st, LEVEL_L); break;                     // ins::WAH_LPF[1]
	case 1: sum_cell(c, st, LEVEL_R); break;                      // ins::WAH_LPF[2]
	default:                                                       // ins::WAH_LPF[0]
		run(c, {st, 0, LEVEL_L, M2_CELL, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, 0, false, LEVEL_R});
		break;
	}
}

// ins::WAH_MIRROR's three words and the level they end in, 0x1c3 to 0x1c9. storeWahRelease() writes the
// first two steps with the same word and the third with its complement, so the chain is the level off
// the rectifier's own output, less itself a word later, with the negatives held at the zero both
// subtract steps clamp to. The words at 0x1c5, 0x1c7 and 0x1c9 are the ROM's own zeros and no host
// write reaches them, which is what leaves the decay to the mirror's words alone.
void mirror(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1c3:
		// ins::WAH_MIRROR[0]: acc = k * r[73] - m[56], and the negatives are held at zero
		run(c, {st, RECT, ATTACK, M2_REG, false, 0, ADD_CELL, true, ROUTE_1, CLAMP_ZERO});
		break;
	case 0x1c4:
		// r[73] = saturate(k * m[56] + acc), the rectifier's own output, and m[4] from the decor above
		run(c, {st, 0, ATTACK, M2_CELL, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, RECT});
		break;
	case 0x1c5:
		term_cell(c, st, LEVEL_L);         // the level on its own, so the next word adds to it
		break;
	case 0x1c6:
		// ins::WAH_MIRROR[1]: the same word again on m[46], plus the level over 32768, which is what
		// the adder offers when it names r[0] and there is no r[0]
		run(c, {st, 0, LEVEL_L, M2_CELL, false, 0, ADD_REG, false, ROUTE_1, CLAMP_WRAP});
		break;
	case 0x1c7:
		run(c, {st, 0, ATTACK, M2_CELL, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_WRAP});
		break;
	case 0x1c8:
		// ins::WAH_MIRROR[2]: the same subtraction off r[73], the negatives held at zero again
		run(c, {st, RECT, 0, M2_REG, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_ZERO});
		break;
	case 0x1c9:
		run(c, {st, 0, ATTACK, M2_CELL, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, 0, false, LEVEL_L});
		break;
	default:
		level_pair(c, st);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// ins::WAH_BAND's edge cascade and the cutoff, 0x1cd to 0x1d9
// ---------------------------------------------------------------------------------------------

// The four steps of ins::WAH_BAND that cascade the edge's shape word. Each applies its own word to the
// last one's output a sixteenth at a time, and the fourth lands in a cell because ins::WAH_BAND's
// series below runs on the cell rather than on a register.
struct Edge { int base, src, dst, cell; };

const Edge EDGES[4] = {
    //  base  src     dst     cell
    { 0x1cd, DRIVE,  EDGE_1, 0       },   // ins::WAH_BAND[1], the edge's gain word fx_design.cpp derives
    { 0x1d1, EDGE_1, EDGE_2, 0       },   // ins::WAH_BAND[2]
    { 0x1d4, EDGE_2, EDGE_3, 0       },   // ins::WAH_BAND[3]
    { 0x1d8, EDGE_3, 0,       EDGE_4 },   // ins::WAH_BAND[4], the last of the cascade, into m[57]
};

Edge const* edge_for(int st)
{
	for (Edge const& e : EDGES)
		if (st == e.base) return &e;
	return nullptr;
}

// One of the four steps: the word on the last one's output, into a register or into a cell.
void edge_step(yss236_core& c, int st, Edge const& e)
{
	run(c, {st, e.src, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_SAT, e.dst, false, e.cell});
}

// The cutoff and its smoother, 0x1ce to 0x1d7, with 0x1d2, 0x1d5 and 0x1d9 the padding between. 0x1ce is
// ins::WAH_CUTOFF's class-1 load, which reads its own word as a value rather than applying it to
// anything, and 0x1cf adds ins::WAH_DEPTH's word on the page's LFO on top of it. 0x1d0 then takes the
// level off m[47], writes the smoother's state and hands the same value to t[9], three steps ahead;
// 0x1d3 multiplies the state by that t[9] and hands the result on again, 0x1d6 multiplies once more and
// leaves the running value, and 0x1d7 is a class-1 load of the ROM's own half added to that, which is
// what stops the smoother standing still, and hands it to t[9] as well. Every t write is the register
// form, so t_put() and not the fraction one.
void cutoff(yss236_core& c, int st)
{
	switch (st)
	{
	case 0x1ce:
		load(c, st, 0, 0);                                       // acc = 128 * ins::WAH_CUTOFF[0]
		break;
	case 0x1cf:
		sum_cell(c, st, LFO_CELL);                               // ins::WAH_DEPTH: the word on m[13] + acc
		break;
	case 0x1d0:
		// r[121] = saturate(k * m[47] + acc), and the same value over 128 goes into t[9]
		t_put(c, CUTOFF_T, run(c, {st, 0, LEVEL_R, M2_CELL, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, SMOOTH}));
		break;
	case 0x1d3:
		// r[121] = t[9] * r[121], which wraps where the other steps saturate, and the same value goes on
		t_put(c, CUTOFF_T, run(c, {st, SMOOTH, 0, M2_REG, false, CUTOFF_T, ADD_OFF, false, ROUTE_1, CLAMP_WRAP, SMOOTH}));
		break;
	case 0x1d6:
		run(c, {st, SMOOTH, 0, M2_REG, false, CUTOFF_T, ADD_OFF, false, ROUTE_1, CLAMP_WRAP});
		break;
	default:
		// the class-1 load that adds the half the ROM's own word at this step is: t[9] takes
		// saturate(0.5 + acc), and it is the clamped value that goes in, not the raw sum
		load(c, st, 0, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT);
		t_put(c, CUTOFF_T, c.acc);
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// The resonator and ins::WAH_BAND's series, 0x1da to 0x1e2
// ---------------------------------------------------------------------------------------------

// The resonator's two channels. The state's step runs t[9], the smoothed cutoff, on the sense value and
// adds the state itself; the accumulator's step runs the same gain on the state and adds the
// accumulator, and the sense pair above differences the channel's EQ output against that accumulator,
// which is what closes the loop. The two steps of a channel are nine steps apart, because the drive's
// second square, the shaper's cascade and ins::WAH_BAND's series run between them.
struct Resonator { int state_step, acc_step, state, feedback, drive; };

const Resonator RESONATOR[2] = {
    //   state  acc     state  feedback  drive
    {   0x1da, 0x1e3,  RES_L, RES_FB_L, SENSE_L },
    {   0x1db, 0x1e4,  RES_R, RES_FB_R, SENSE_R },
};

Resonator const* resonator_for(int st)
{
	for (Resonator const& p : RESONATOR)
		if (st == p.state_step || st == p.acc_step) return &p;
	return nullptr;
}

// One of the resonator's two steps, the state or the accumulator.
void resonator_step(yss236_core& c, int st, Resonator const& p)
{
	if (st == p.state_step)
		// r[state] = saturate(t[9] * m[drive] + r[state])
		run(c, {st, p.state, p.drive, M2_CELL, false, CUTOFF_T, ADD_REG, false, ROUTE_1, CLAMP_SAT, p.state});
	else
		// m[feedback] = saturate(t[9] * r[state] + m[feedback])
		run(c, {st, p.state, p.feedback, M2_REG, false, CUTOFF_T, ADD_CELL, false, ROUTE_1, CLAMP_SAT, 0, false, p.feedback});
}

// ins::WAH_BAND's series, 0x1dd to 0x1e2: six steps over the shaper's own output and the three edge
// words, the fifth of them reading the fourth edge word out of m[57]. storeEdge() builds a geometric
// series and writes the same 6553 into the last five of these six steps, which are ins::WAH_BAND[7] to
// [11]; the word at 0x1dd is the one it leaves at zero, and that is what opens the sum.
void shaper(yss236_core& c, int st)
{
	int const k = st - 0x1dd;
	if (k == 0)      term(c, st, DRIVE);                     // ins::WAH_BAND[6], on its own
	else if (k == 4) sum_cell(c, st, EDGE_4);                // ins::WAH_BAND[10], the fourth shape word
	else if (k == 5) close(c, st, DRIVE, DRIVE, ROUTE_1);    // ins::WAH_BAND[11], closing the sum
	else             sum(c, st, DRIVE + k);                  // ins::WAH_BAND[7] to [9], the edge words
}

// ---------------------------------------------------------------------------------------------
// The tone chain's three sections, 0x1e6 to 0x1f4
// ---------------------------------------------------------------------------------------------

// What one step of a tone section computes, named for the coefficient designFilter() wrote for it: b0,
// b1 and b2 are the numerator's and -a1 and -a2 the denominator's. The three that end in X1 or Y2 are
// the steps that carry the section's z^-1, which is how it holds the input a pass back or the output
// two passes back in a register of its own, and the two that end in OPEN are the two shapes a section's
// first step has, one per coefficient the schedule opens that section on.
enum part_t
{
	B1_OPEN,    // b1 on the input, opening the sum with the adder off
	B2_OPEN,    // and b0's sibling b2 on the input a pass back, which is how the lowpass opens
	B1,         // b1 on the input, added to the sum
	B2,         // b2 on the input a pass back, and the z^-1 that keeps it
	A1,         // -a1 on the output a pass back, added to the sum
	A2,         // -a2 on the output two passes back, added to the sum
	A2_IN,      // -a2 on the output two passes back, and the z^-1 that shifts the input a pass back
	A2_Y2,      // -a2 on the output two passes back, and the z^-1 that keeps the output two passes back
	B0,         // b0 on the input, closing the section into its output with the section's route
};

// One of the distortion's three order-2 tone sections, in direct form I. `in` is the register the
// section's input arrives in and `out` the one b0 writes, which is also its y[-1]; `x1` holds the input
// a pass back and `y2` the output two passes back. The four delay registers are r[80], r[82], r[84]
// and r[86], one per section boundary: each section's x1 is the section before's y2, so a z^-1 shifts
// one register that both of them read, and only the last section carries two shifts of its own.
struct Tone { int base, in, x1, y2, out; route_t route; part_t part[5]; };

const Tone TONES[3] = {
    //  base  in         x1  y2  out        route      the five steps, in the order the schedule runs them
    { 0x1e6, DRIVE_GAIN, 80, 82, TONE_LPF,  ROUTE_2,  { B2_OPEN, B1,     A2_IN, A1,    B0 } },   // ins::WAH_LPF2
    { 0x1eb, TONE_LPF,   82, 84, TONE_PEAK, ROUTE_2,  { B1_OPEN, B2,     A2,    A1,    B0 } },   // ins::WAH_PEAK
    { 0x1f0, TONE_PEAK,  84, 86, TONE_SHELF,ROUTE_16, { B1_OPEN, B2,     A1,    A2_Y2, B0 } },   // ins::WAH_LOW_SHELF2
};

Tone const* tone_for(int st)
{
	for (Tone const& t : TONES)
		if (st >= t.base && st < t.base + 5) return &t;
	return nullptr;
}

// One tone section's step k of five.
void tone_step(yss236_core& c, int st, Tone const& t, int k)
{
	switch (t.part[k])
	{
	case B1_OPEN: term(c, st, t.in); break;                              // b1, the adder off
	case B2_OPEN: term(c, st, t.x1); break;                              // b2, the adder off
	case B1:      sum(c, st, t.in); break;
	case B2:      copy_z(c, st, t.x1, t.x1, ADD_ACC, CLAMP_WRAP); break;  // and x[-1] kept
	case A1:      sum(c, st, t.out); break;
	case A2:      sum(c, st, t.y2); break;
	case A2_IN:   copy_z(c, st, t.y2, t.x1, ADD_ACC, CLAMP_WRAP); break;  // and x[-1] kept
	case A2_Y2:   copy_z(c, st, t.y2, t.y2, ADD_ACC, CLAMP_WRAP); break;  // and y[-2] kept
	default:      close(c, st, t.in, t.out, t.route); break;             // b0 on the input, into the output
	}
}

// ---------------------------------------------------------------------------------------------
// The dry and wet mix, 0x1f5 to 0x1fc
// ---------------------------------------------------------------------------------------------

// One pair of ins::WAH_WET's eight words. The first step applies the dry word on its own and the second
// adds the wet word on top of it, a quarter at a time, into the cell the base program reads or into one
// of the window's output registers. The first two pairs are the two channels, the resonator's own state
// as the wet side; the last two are the same two mixes as they were one pass before, taken out of the
// cells the first two published, with the tone chain's output as the wet side instead, and they are what
// the base program reads back.
struct Mix { int base, dry, wet, cell, out; m2_t dry_m2; };

const Mix MIXES[4] = {
    //  base  dry        wet         cell      out
    { 0x1f5, EQ_L,      RES_L,      WAH_L,     0,      M2_REG  },   // ins::WAH_WET[0] and [1], the left
    { 0x1f7, EQ_R,      RES_R,      WAH_R,     0,      M2_REG  },   // ins::WAH_WET[4] and [5], the right
    { 0x1f9, WAH_L_OLD, TONE_SHELF, 0,         OUT_L,  M2_CELL },   // ins::WAH_WET[2] and [3], into r[17]
    { 0x1fb, WAH_R_OLD, TONE_SHELF, 0,         OUT_R,  M2_CELL },   // ins::WAH_WET[6] and [7], into r[18]
};

// One mix's two steps.
void mix(yss236_core& c, int st, Mix const& m, int k)
{
	if (k == 0)
	{
		if (m.dry_m2 == M2_CELL) term_cell(c, st, m.dry);     // the mix of the pass before, off the cell
		else                     term(c, st, m.dry);          // this pass's channel, off the EQ output
		return;
	}
	// the wet word on top of the running value, into the cell or the output register
	run(c, {st, m.wet, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_4, CLAMP_SAT, m.out, false, m.cell});
}

// ---------------------------------------------------------------------------------------------
// The 96 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x1a0 to 0x1ff, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program skips these steps
// as it would the interpreter's. Each branch below names the block its step belongs to and hands it the
// step's own index into that block, which is all the arithmetic needs: every block's table holds the
// registers and the shapes of its steps, and nothing here is a step at a time.
void block_0x1a0(yss236_core& c)
{
	for (int st = 0x1a0; st < 0x200; st++)
	{
		c.begin_step();
		if (st < c.m_skip) continue;

		// ---- the four cells this window publishes, before the step's own arithmetic ----
		for (Decor const& d : DECORS)
			if (d.st == st) decor(c, st, d);

		// ---- 0x1a0 to 0x1ad, the two channels' EQ: a signal in and two order-1 shelves a side ----
		if (st < 0x1ae)
		{
			Eq const& e = EQS[(st - EQS[0].base) / 7];
			eq(c, st, e, st - e.base);
			continue;
		}

		// ---- 0x1ae and 0x1af, ins::WAH_MODE's pair, the two channels' wah signals as one ----
		if (st < 0x1b0) { mode_pair(c, st); continue; }

		// ---- the three band sections: the distortion's at 0x1b0 and 0x1b3, the Touch Wah's at 0x1b9 ----
		if (Band const* b = band_for(st)) { band(c, st, *b, st - b->base); continue; }

		// ---- 0x1b6 to 0x1b8, ins::WAH_MODE2's pair and ins::WAH_DRIVE's first word ----
		if (st < 0x1bc) { touch_input(c, st); continue; }

		// ---- 0x1bc to 0x1c2, the drive's two squares, the sense pair on both channels and the attack ----
		if (st < 0x1c3) { drive_and_sense(c, st); continue; }

		// ---- 0x1c3 to 0x1cc, the mirror round the level and ins::WAH_LPF over it ----
		if (st < 0x1cd) { mirror(c, st); continue; }

		// ---- 0x1cd to 0x1d9, the edge's cascade, the cutoff's smoother and the padding between them ----
		if (st < 0x1da)
		{
			if (Edge const* e = edge_for(st)) { edge_step(c, st, *e); continue; }
			if (st == 0x1d2 || st == 0x1d5 || st == 0x1d9) { idle(c); continue; }
			cutoff(c, st);
			continue;
		}

		// ---- 0x1da and 0x1db, the resonator's two states, and 0x1e3 and 0x1e4, its accumulators ----
		if (Resonator const* p = resonator_for(st)) { resonator_step(c, st, *p); continue; }

		// ---- 0x1dc to 0x1e2, the fourth edge word and ins::WAH_BAND's series ----
		if (st < 0x1e3)
		{
			if (st == 0x1dc)   // ins::WAH_BAND[5], the fourth shape word, off the cell it landed in
				run(c, {st, 0, EDGE_4, M2_CELL, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_SAT, DRIVE});
			else
				shaper(c, st);
			continue;
		}

		// ---- 0x1e5, ins::WAH_DRIVE's fourth word: the gain storeDrive() derives, into the tone chain ----
		if (st == 0x1e5) { gain(c, st, DRIVE, DRIVE_GAIN, ROUTE_1); continue; }

		// ---- 0x1e6 to 0x1f4, the tone chain's three sections ----
		if (Tone const* t = tone_for(st)) { tone_step(c, st, *t, st - t->base); continue; }

		// ---- 0x1f5 to 0x1fc, the dry and wet mix into the window's two outputs ----
		if (st < 0x1fd)
		{
			Mix const& m = MIXES[(st - MIXES[0].base) / 2];
			mix(c, st, m, st - m.base);
			continue;
		}

		// ---- 0x1fd to 0x1ff, three class-0 steps with no ALU, so only the x latch runs ----
		idle(c);
	}
}

}  // namespace

// One run of the insertion window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void ins_m04(yss236_core& c, int from, int to)
{
	if (from == 0x1a0 && to == 0x200) { block_0x1a0(c); return; }
	for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

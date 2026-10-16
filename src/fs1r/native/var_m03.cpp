// fs1r/native/var_m03.cpp - VOP3-2, the variation window's program for HM Enhancer, Noise Gate and
// Compressor: its 72 steps.
//
// rom::VARIATION_PROGRAMS[3], the program the window holds for the three variation types 14 to 16, and the
// only program among the twelve that these three share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 3 to
// HM Enhancer, Noise Gate and Compressor and to nothing else. Under each of them the same program words
// run and only the constants and the delay offsets move, so this native is one function for all three and
// no type is excluded. Which of the three is running is not a code difference here; the host's parameters
// are: fx_drive.cpp's DYNAMICS[0] holds the word lists this row's parameters are written through, and
// var::HM_DRIVE is the enhancer's own two.
//
//   DYN_HPF2      {0x15E, 0x15A, 0x15B, 0x15C, 0x15D}   the left input's filter
//   DYN_HPF2_COPY {0x164, 0x160, 0x161, 0x162, 0x163}   the right input's, the same five words
//   DYN_THRESHOLD {0x165}
//   HM_DRIVE      {0x166, 0x167}                        the enhancer's drive only
//   DYN_RATIO     {0x170, 0x173, 0x171, 0x172, 0x174, 0x175, 0x176, 0x177}
//   DYN_ATTACK    {0x17B, 0x179, 0x17A}
//   DYN_RELEASE   {0x17E, 0x183, 0x185}
//   DYN_HPF       {0x18E, 0x18C, 0x18D}                  the left output's filter
//   DYN_HPF_COPY  {0x194, 0x192, 0x193}                  the right output's
//   DYN_OUT       {0x195, 0x196, 0x197, 0x199}           dry left, wet left, dry right, wet right
//
// fx_design.cpp's designFilter writes an order-2 filter as b0, b1, b2, -a1, -a2 in that list order, and
// an order-1 one as b0, b1, -a1. The two input filters are designed as HPF2 for the enhancer and as HPF1
// for the compressor and the gate, and HPF1 leaves b2 and -a2 at zero, so the five words carry three
// coefficients and two zeros for those two types. The two output filters are HPF1 for the enhancer and
// GAIN_ONLY for the other two, over the same three words.
//
// The 72 steps, in the groups the loop below splits them into:
//
//   0x158-0x15e  the left input and its filter. 0x158 takes the left serial input into r[119] through
//                the ROM's ~unity word and 0x159 doubles the running value into r[103], which is the
//                filter's input. 0x15a to 0x15e are the filter's five words: b1 times the input, then
//                b2 times r[104] (the input a pass back, which 0x15b copies from the previous step's x
//                operand), then -a1 times r[105] (the output a pass back), then -a2 times r[106] (which
//                0x15d copies from r[104] a pass earlier), and 0x15e closes the loop by writing
//                r[105] = 2 * (b0 * r[103] + that sum).
//   0x15f-0x164  the right input and the same filter again: 0x15f doubles r[6] into r[107], and 0x160
//                to 0x164 are b1, b2, -a1, -a2, b0 over r[108], r[109] and r[110] exactly as 0x15a to
//                0x15e are over r[104], r[105] and r[106]. 0x160 and 0x164 also publish the captured
//                delay RAM reads.
//   0x165-0x167  the threshold and the drive. 0x165 is DYN_THRESHOLD's word and scales the level the
//                release chain left in m[55] into r[126]. 0x166 and 0x167 are HM_DRIVE: the two filtered
//                inputs times the drive, doubled, into r[1] and r[2], which is the pair every step below
//                works on. 0x166 publishes the LFO value of its own page into m[11].
//   0x168-0x16f  the threshold ladder. Eight class-1 steps, each adding its own constant to r[126] and
//                clamping negatives away, so each register holds max(0, r[126] + 128 * k) with a
//                different k. The ROM's constants for this row step the level by 64, 0.5, 2, 8, 32, 128
//                and 2048, and r[126] saturates at 128, so r[124] and r[125] read zero under them and
//                the other five are the ladder the ratio curve below reads. 0x168 publishes a captured
//                read into m[30].
//   0x170-0x177  the ratio curve: DYN_RATIO's eight words, a weighted sum of the ladder's eight
//                registers. fx_common.cpp's storeRatio writes a row of the COMP_RATIO table, so this is
//                the gain reduction as a function of the level, one straight line per pair of taps.
//   0x178-0x17b  the one-pole smoother the curve goes through: r[115] takes the curve, and 0x179, 0x17a
//                and 0x17b are its three words, b, t and b again, so r[116] = 2b * r[115] + t * r[116]
//                with storeCompAttack's (0x8000 - t) / 2 for b. 0x17b also takes the result into t[9] as
//                a 1.15 word, which is the gain the steps below read.
//   0x17c-0x17e  the detector. 0x17c and 0x17d add the two driven channels and take the absolute value,
//                with the ROM's fixed 0.0442 word, into r[119], which is the level everything below
//                works against. 0x17e is DYN_RELEASE's first word: the level comes off m[54] and the
//                running value clamps negatives away.
//   0x17f-0x181  the gain applied to the two channels. 0x17f and 0x180 multiply r[1] and r[2] by t[9]
//                and take each product into t[10] and t[11] as 1.15 words. 0x181 is the first of the
//                two accumulators over the level: it adds m[54] to r[119], saturates and writes m[54]
//                back.
//   0x182-0x186  the second accumulator, and the level the threshold reads. 0x182 and 0x183 put
//                m[55] on the running value (0x183 is DYN_RELEASE's second word), 0x184 subtracts it
//                from the level, 0x185 is DYN_RELEASE's third word and clamps, and 0x186 adds the level
//                back and writes m[55]. 0x165 reads m[55] a pass later, which closes the loop: the
//                threshold scales what the release chain accumulated.
//   0x187-0x18e  the left output's filter. 0x187 and 0x188 multiply the two driven inputs by t[10] and
//                t[11] into r[122] and r[123], and 0x189 and 0x18a put r[1] and r[120] on the running
//                value, which is the filter's input. 0x18b writes the filter's state r[111], then 0x18c
//                (b1) and 0x18d (-a1) add the feedback terms, 0x18d also copying the sum into r[112] as
//                the one-pass delay, and 0x18e (b0) writes r[112] again with the whole sum. Both writes
//                land, 0x18d's at 0x190 and 0x18e's at 0x191, and nothing reads r[112] between them, so
//                the value 0x18d leaves is the one 0x18e replaces. 0x187 publishes the LFO value of its
//                page into m[12].
//   0x18f-0x194  the right output's filter and the same three words: 0x18f and 0x190 put r[2] and r[121]
//                on the running value, 0x191 writes the state r[113], 0x192 (b1) and 0x193 (-a1) add the
//                feedback terms, and 0x194 (b0) writes r[114]. Only 0x194 writes the delay register here,
//                and 0x194 publishes a captured read into m[31].
//   0x195-0x199  the dry and wet: DYN_OUT's four words. 0x195 scales the left input r[5] and 0x196 adds
//                r[112] to it and writes the left output r[15]; 0x197 and 0x199 do the same for r[6] and
//                r[114] into r[16]. dynamicsLevel writes zeros into the first and third of the four and
//                the type's gain into the second and fourth, so the two dry terms are off for the
//                compressor and the gate; the enhancer's MIX_LEVEL writes the gain itself.
//   0x19a-0x19f  the tail. 0x19a, 0x19b and 0x19d to 0x19f are class 0 with no RAM mark and no ALU, so
//                only the x latch runs, and 0x19c publishes the last captured read into m[33].
//
// Every group above that the program runs more than once is a table below and a loop over it: the two
// input filters (IN_FILTERS, six steps each), the drive (DRIVE, two), the ladder (LADDER, eight), the
// ratio curve's taps (RATIO, eight), the gain on each channel (GAIN and OUT_GAIN, two each), the two
// output filters (OUT_FILTERS, six each) and the dry and wet mix (OUT_MIX, five). Each carries the
// cell move or the delay RAM read its steps' words put in front of the arithmetic, so the six published
// reads come off the tables and not out of the cases.
//
// The registers: r[5] and r[6] the two serial inputs the base program writes, r[15] and r[16] the two
// outputs, r[1] and r[2] the two driven inputs, r[103] and r[107] the two filter inputs doubled, r[104]
// and r[108] their one-pass delays, r[105] and r[109] their states and outputs, r[106] and r[110] their
// outputs delayed again, r[111] and r[113] the output filters' states with r[112] and r[114] their
// sums and one-pass delays, r[119] the left input's gain at first and the level from 0x17d on, r[120]
// and r[121] the two channels after t[9], r[122] and r[123] the two after t[10] and t[11], r[126] the
// level the threshold scales, and r[124] and r[125] the ladder's two taps this row's constants hold at
// zero. The cells: m[2] and m[6] the two serial inputs, m[11] and m[12] the two LFO values, m[28] to
// m[33] the six published delay RAM reads, m[54] and m[55] the release chain's two accumulators. The
// delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring pointer with no index; this
// program writes none of them, so the lines behind these taps are filled elsewhere. Each capture takes
// the read five steps before it (captured()'s lower bound), so m[28] carries the slot 0x56 word, m[29]
// the slot 0x57 word, m[30] the slot 0x58 word, and m[31], m[32] and m[33] the slots 0x63, 0x64 and
// 0x65 words.
//
// The three t registers the program uses are t[9], written at 0x17b and read at 0x17f and 0x180, and
// t[10] and t[11], written at 0x17f and 0x180 and read at 0x187 and 0x188.
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
// vop3_native_check var-m03: identical, 3 types, 28800 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, copy_z and the rest

// ---------------------------------------------------------------------------------------------
// What a step carries before its arithmetic
// ---------------------------------------------------------------------------------------------

// The cells a step takes from the hardware side, and the delay RAM reads it makes, both of which
// step() does before the ALU: a serial input, an LFO value, a captured read or a read of its own.
enum decor_t { DEC_NONE, DEC_SERIAL, DEC_LFO, DEC_CAPTURE, DEC_READ };

// What that move is. `cell` is the cell a serial input, an LFO or a capture lands in, and on a
// DEC_READ it is the slot, which read_delay() takes off the step's own number rather than off here.
// `bank` is which of the two serial banks a DEC_SERIAL step reads, 0 or 1, and is left out (and so
// zero) on every other kind.
struct Decor { decor_t kind; int cell; int bank; };

// The move one step's words carry, which step() makes before the arithmetic, so the unit below can
// put it ahead of that step's own term.
void carries(yss236_core& c, int st, Decor d)
{
    switch (d.kind)
    {
        case DEC_SERIAL:  take_serial(c, d.cell, st, d.bank); break;
        case DEC_LFO:     take_lfo(c, d.cell, st); break;
        case DEC_CAPTURE: take_capture(c, d.cell, st); break;
        case DEC_READ:    read_delay(c, st); break;
        case DEC_NONE: break;
    }
}

// ---------------------------------------------------------------------------------------------
// The two input filters, 0x159 to 0x164
// ---------------------------------------------------------------------------------------------

// One input filter and the six steps it runs, an input step and the section's five words. `in` is the
// register the channel's doubled input is in, which b1 and b0 read a pass later than the input step
// wrote it; `x1` holds that input a pass older, which b2 reads and which the same step copies;
// `y1` is the filter's output a pass back, which -a1 reads and b0 overwrites; `y2` is that output two
// passes back, which -a2 reads and copies the same way. `adds` says the input step joins the running
// value, which the left channel's does because 0x158 has already put the left input there, and the
// right channel's does not because nothing has.
struct InFilter
{
    int base, in, x1, y1, y2;
    bool adds;
    Decor decor[6];      // the input step, then b1, b2, -a1, -a2 and b0
};

constexpr InFilter IN_FILTERS[2] = {
    // base   in  x1   y1   y2  adds  input              b1                 b2                   -a1             -a2                  b0
    { 0x159, 103, 104, 105, 106, true,  { {DEC_SERIAL, 6, 1}, {DEC_NONE, 0},  {DEC_READ, 0x56},  {DEC_NONE, 0},  {DEC_NONE, 0},     {DEC_NONE, 0}     } },
    { 0x15f, 107, 108, 109, 110, false, { {DEC_READ, 0x57},   {DEC_CAPTURE, 28},{DEC_NONE, 0},    {DEC_NONE, 0},  {DEC_READ, 0x58},  {DEC_CAPTURE, 29} } },
};

// One input filter's six steps: the channel's input, then b1, b2, -a1, -a2 and b0 over it. Both
// input steps read r[6], the right serial input, because the left channel's is the stereo mix 0x158
// opened and the step at 0x159 only closes it. Each section runs
//
//   y[n] = 2 (b0 x[n] + b1 x[n-1] + b2 x[n-2] + (-a1) y[n-1] + (-a2) y[n-2])
//
// with the x2 route on the closing step, which is what restores the scale designFilter() took out of
// the coefficients.
void input_filter(yss236_core& c, int st, InFilter const& f, int k)
{
    carries(c, st, f.decor[k]);
    switch (k)
    {
    case 0:                                   // the channel's input, doubled into the filter's own
        if (f.adds) close(c, st, 6, f.in, ROUTE_2);   // left: on top of the stereo mix already there
        else         gain(c, st, 6, f.in, ROUTE_2);    // right: its own word, the adder off
        break;
    case 1:  term(c, st, f.in); break;                                 // b1 on x[n-1]
    case 2:  copy_z(c, st, f.x1, f.x1, ADD_ACC, CLAMP_WRAP); break;    // b2 on x[n-1], and x[n-1] copied
    case 3:  sum(c, st, f.y1); break;                                  // -a1 on y[n-1]
    case 4:  copy_z(c, st, f.y2, f.y2, ADD_ACC, CLAMP_WRAP); break;    // -a2 on y[n-2], and y[n-2] copied
    default: close(c, st, f.in, f.y1, ROUTE_2); break;                // b0 on x[n], closing into y[n]
    }
}

// ---------------------------------------------------------------------------------------------
// The threshold, the drive and the ladder, 0x165 to 0x16f
// ---------------------------------------------------------------------------------------------

// DYN_THRESHOLD's word on the level the release chain left in m[55], into r[126]. Everything the
// compressor's curve does is a function of this one register: the ladder below steps it down and the
// curve reads the steps.
void threshold(yss236_core& c, int st)
{
    run(c, {.st = st, .sm = 55, .m2 = M2_CELL, .clamp = CLAMP_SAT, .dr = 126});
}

// One channel of the drive: HM_DRIVE's word on that channel's filtered input, doubled, into the pair
// of driven registers every step below works on.
struct Drive { int filtered, driven; };
constexpr Drive DRIVE[2] = { {105, 1}, {109, 2} };

void drive(yss236_core& c, int st, int k)
{
    if (k == 0) take_lfo(c, 11, st);      // m[11] takes the LFO of this page, on the left channel only
    gain(c, st, DRIVE[k].filtered, DRIVE[k].driven, ROUTE_2);
}

// The threshold ladder's eight registers: r[126] each one moved by its own constant (the ROM's are all
// negative for this row, so the ladder steps down) and clamped at zero. They are the taps the ratio
// curve below reads, each at its own step of the level.
const int LADDER[8] = { 119, 120, 121, 122, 123, 124, 125, 126 };

// One rung: a class-1 step, which is the one shape that loads a word rather than applying it. It adds
// r[126] to 128 times the ROM's constant for this row and holds the negatives at zero.
void ladder(yss236_core& c, int st, int k)
{
    if (k == 0) take_capture(c, 30, st);   // m[30] takes up the slot 0x58 read of 0x163
    load(c, {.st = st, .sr = 126, .add = ADD_REG, .clamp = CLAMP_ZERO, .dr = LADDER[k]});
}

// ---------------------------------------------------------------------------------------------
// The ratio curve and the smoother, 0x170 to 0x17b
// ---------------------------------------------------------------------------------------------

// One tap of the ratio curve: the word on the ladder register it names, the first opening the sum and
// the rest joining it. `route` is the scale the ROM has that word at. Nothing is written, so the
// curve goes on in the running value alone.
struct Tap { int reg; route_t route; };
constexpr Tap RATIO[8] = {
    { 119, ROUTE_2 }, { 121, ROUTE_2 }, { 122, ROUTE_2 }, { 120, ROUTE_2 },
    { 123, ROUTE_2 }, { 124, ROUTE_4 }, { 125, ROUTE_1 }, { 126, ROUTE_1 },
};

void ratio_tap(yss236_core& c, int st, int k)
{
    Tap const& t = RATIO[k];
    if (k == 0) run(c, {.st = st, .sr = t.reg, .route = t.route});   // the first term
    else        run(c, {.st = st, .sr = t.reg, .add = ADD_ACC, .route = t.route});   // the rest
}

// The one-pole smoother the curve goes through: the curve loaded into r[115], then DYN_ATTACK's three
// words in order, b, t and b again, so r[116] = b r[115] + t r[116] on the chip's own scale. The
// result is published as t[9], which is the gain the two channels below read.
void smoother(yss236_core& c, int st, int k)
{
    switch (k)
    {
    case 0: load(c, {.st = st, .add = ADD_ACC, .clamp = CLAMP_ZERO, .dr = 115}); break;  // the curve into r[115]
    case 1: term(c, st, 115); break;                                           // b on x[n-1]
    case 2: sum(c, st, 116); break;                                            // t on y[n-1]
    default:                                                            // b again, on x[n], into y[n]
        t_put(c, st, 9, run(c, {.st = st, .sr = 115, .add = ADD_ACC, .clamp = CLAMP_SAT, .dr = 116}));
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// The detector and the release chain, 0x17c to 0x186
// ---------------------------------------------------------------------------------------------

// DYN_RELEASE's fall: the level comes off the accumulator's own value, negatives clamp away, and the
// result goes back into the cell. `store` says the cell takes it: 0x17e writes m[54], and 0x185, in
// the middle of the slow chain below, leaves the value in the running value for 0x186 to add the
// level back to.
void release_fall(yss236_core& c, int st, bool store)
{
    alu s{ st, 0, 54, M2_CELL, false, 0, ADD_ACC, true, ROUTE_1, CLAMP_ZERO, 0, false, 54 };
    if (!store) s.dm = 0;
    run(c, s);
}

// The level the whole block works against: the two driven channels summed, rectified, and taken at
// sixteen times with the ROM's fixed word into r[119], then the release chain's first fall.
void detector(yss236_core& c, int st, int k)
{
    switch (k)
    {
    case 0: term(c, st, 1); break;      // k * r[1]: the left driven input, opening the sum
    case 1:  // the right one closing it, and the absolute value that makes it a magnitude
        run(c, {.st = st, .sr = 2, .add = ADD_ACC, .route = ROUTE_16, .clamp = CLAMP_ABS, .dr = 119});
        break;
    default: release_fall(c, st, true); break;
    }
}

// The gain t[9] on one channel: r[dr] = saturate(t[treg] * r[src]), and the result written out as a
// 1.15 word of its own where `tdest` says so. That is how 0x17f and 0x180 hand the smoothed gain to
// the output filters, and the pair at 0x187 and 0x188 read t[10] and t[11] back and publish nothing.
struct ChanGain { int src, dr, treg, tdest; };
constexpr ChanGain GAIN[2] = { {1, 120, 9, 10}, {2, 121, 9, 11} };
constexpr ChanGain OUT_GAIN[2] = { {1, 122, 10, 0}, {2, 123, 11, 0} };

void channel_gain(yss236_core& c, int st, ChanGain const& g)
{
    double const y = run(c, {.st = st, .sr = g.src, .treg = g.treg, .clamp = CLAMP_SAT, .dr = g.dr});
    if (g.tdest) t_put(c, st, g.tdest, y);
}

// The level smoothed into the two accumulators, 0x181 to 0x186. m[54] is the fast one: the level comes
// back in on top of what m[54] holds and m[54] takes it. m[55] is the slow one, and it is the cell
// DYN_RELEASE's other two words decay, which is what the threshold reads a pass later.
void release_chain(yss236_core& c, int st, int k)
{
    switch (k)
    {
    case 0:     // the fast accumulator's rise: r[119] + m[54], written back into m[54]
        run(c, {.st = st, .sr = 119, .sm = 54, .unity = true, .add = ADD_CELL, .clamp = CLAMP_SAT, .dm = 54});
        break;
    case 1: term_cell(c, st, 55); break;   // k * m[55]
    case 2:     // the same word again, added to the running value over 32768: DYN_RELEASE's second word
        run(c, {.st = st, .sm = 55, .m2 = M2_CELL, .add = ADD_REG});
        break;
    case 3:     // the decayed m[55] off the new level: r[119] - acc, with nothing written
        run(c, {.st = st, .sr = 119, .unity = true, .add = ADD_ACC, .sub = true});
        break;
    case 4: release_fall(c, st, false); break;   // DYN_RELEASE's third word
    default:    // and the level back on top of the fall, into m[55] for the threshold to scale
        run(c, {.st = st, .sr = 119, .unity = true, .add = ADD_ACC, .clamp = CLAMP_SAT, .dm = 55});
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// The two output filters, 0x187 to 0x194
// ---------------------------------------------------------------------------------------------

// One output filter's six steps over one channel. `src` is the driven input the filter's input sum
// opens on, `wet` the same channel through the gain, and `pre` the same channel at full scale, which
// is what the filter itself runs on: r[state] takes the whole of the input sum, the b1 word then reads
// the state, and b0 closes back into r[out], which is the register the -a1 word has been reading.
struct OutFilter
{
    int base;
    int src, wet, pre, state, out;
    Decor decor[6];
};

constexpr OutFilter OUT_FILTERS[2] = {
    // base   src  wet  pre  state out  input            wet                  state               -a1             -a2                  b0
    { 0x189,   1, 120, 122, 111, 112, { {DEC_LFO, 12},     {DEC_NONE, 0},  {DEC_NONE, 0},   {DEC_NONE, 0}, {DEC_NONE, 0},     {DEC_NONE, 0}      } },
    { 0x18f,   2, 121, 123, 113, 114, { {DEC_READ, 0x63},   {DEC_NONE, 0},  {DEC_NONE, 0},   {DEC_NONE, 0}, {DEC_READ, 0x64},   {DEC_CAPTURE, 31}   } },
};

void output_filter(yss236_core& c, int st, OutFilter const& f, int k)
{
    carries(c, st, f.decor[k]);
    switch (k)
    {
    case 0:  term(c, st, f.src); break;      // the dry driven input, opening the sum
    case 1:  sum(c, st, f.wet); break;       // the same channel through the gain
    case 2:  // r[state] = saturate(16 (k r[pre] + acc)): the whole input sum, and the state with it
        run(c, {.st = st, .sr = f.pre, .add = ADD_ACC, .route = ROUTE_16, .clamp = CLAMP_SAT, .dr = f.state});
        break;
    case 3:  term(c, st, f.state); break;    // DYN_HPF's b1 word on the state
    case 4:  sum(c, st, f.out); break;       // and its -a1 word on the output, the one-pass delay
    default: close(c, st, f.state, f.out, ROUTE_2); break;   // b0 closing into that same register
    }
}

// ---------------------------------------------------------------------------------------------
// The dry and wet, 0x195 to 0x199
// ---------------------------------------------------------------------------------------------

// One step of the output mix: the register its term reads, the output it closes into (0 where it only
// moves the sum along) and whether that term opens the sum or joins it.
struct Mix { int reg, dr; add_t add; };

constexpr Mix OUT_MIX[5] = {
    {  5,  0, ADD_OFF },   // 0x195 the dry left, opening the left channel's mix
    {112, 15, ADD_ACC },   // 0x196 the wet left, closing it into r[15]
    {  6,  0, ADD_OFF },   // 0x197 the dry right, and the read of the slot 0x65
    {112,  0, ADD_ACC },   // 0x198 the word between them, summing r[112], the left channel's wet
    {114, 16, ADD_ACC },   // 0x199 the wet right, closing it into r[16]
};

void out_mix(yss236_core& c, int st, int k)
{
    if (k == 2) read_delay(c, st);            // the slot 0x65 read, on the right dry word
    else if (k == 3) take_capture(c, 32, st);  // m[32] takes the slot 0x64 read of 0x193 up
    Mix const& mix = OUT_MIX[k];
    if (mix.dr) close(c, st, mix.reg, mix.dr, ROUTE_4);   // the two wet words, into the outputs
    else if (mix.add == ADD_OFF) term(c, st, mix.reg);     // the dry words, each opening its channel
    else sum(c, st, mix.reg);
}

// ---------------------------------------------------------------------------------------------
// The 72 steps, 0x158 to 0x19f
// ---------------------------------------------------------------------------------------------

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
    for_steps<0x158, 0x1a0>(c, [&](yss236_core& c, auto st)
    {

        if (st < 0x159)
        {
            // ---- 0x158, the stereo mix's first term: the left serial input into m[2] for the base
            //      program, and into the running value for the filter 0x159 closes it into ----
            take_serial(c, 2, st, 0);
            term(c, st, 5);
            return;
        }

        if (st < 0x165)
        {
            // ---- the left input's filter, then the right input's over its own registers ----
            InFilter const& f = IN_FILTERS[st < 0x15f ? 0 : 1];
            input_filter(c, st, f, st - f.base);
            return;
        }

        if (st < 0x168)
        {
            // ---- the threshold, and the drive the level is measured against ----
            if (st == 0x165) { threshold(c, st); return; }
            drive(c, st, st - 0x166);
            return;
        }

        if (st < 0x170) { ladder(c, st, st - 0x168); return; }
        if (st < 0x178) { ratio_tap(c, st, st - 0x170); return; }
        if (st < 0x17c) { smoother(c, st, st - 0x178); return; }
        if (st < 0x17f) { detector(c, st, st - 0x17c); return; }

        if (st < 0x181)
        {
            // ---- the smoothed gain on the two channels, each published as a word of its own ----
            channel_gain(c, st, GAIN[st - 0x17f]);
            return;
        }

        if (st < 0x187) { release_chain(c, st, st - 0x181); return; }

        if (st < 0x189)
        {
            // ---- the two words the output filters read their channels through ----
            if (st == 0x187) take_lfo(c, 12, st);      // m[12] takes the LFO of this page
            channel_gain(c, st, OUT_GAIN[st - 0x187]);
            return;
        }

        if (st < 0x195)
        {
            // ---- the left output's filter, then the right output's over its own registers ----
            OutFilter const& f = OUT_FILTERS[st < 0x18f ? 0 : 1];
            output_filter(c, st, f, st - f.base);
            return;
        }

        if (st < 0x19a) { out_mix(c, st, st - 0x195); return; }

        // ---- the tail: class 0 with no RAM mark and no ALU, so only the x latch moves, bar m[33]
        //      taking up the slot 0x65 read of 0x197 on the way past ----
        if (st == 0x19c) take_capture(c, 33, st);
        idle(c);
        });
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m03(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
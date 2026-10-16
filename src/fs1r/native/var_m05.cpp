// fs1r/native/var_m05.cpp - VOP3-2, the variation window's program for Phaser1 and Phaser2: its 72 steps.
//
// rom::VARIATION_PROGRAMS[5], the program the window holds for the two variation types 5 and 6, and one
// of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 5 to
// Phaser1 and Phaser2 and to nothing else. The same program words run under both and only the constants
// move, so this native is one function for both types and neither is excluded.
//
//   PH_FILTER     {0x15B, 0x159, 0x15A}         the left channel's low shelf: b0, b1, -a1
//   PH_FILTER_2   {0x15E, 0x15C, 0x15D}         the left channel's high shelf, the same three
//   PH_FILTER_3   {0x162, 0x160, 0x161}         the right channel's low shelf
//   PH_FILTER_4   {0x165, 0x163, 0x164}         the right channel's high shelf
//   PH_FB         {0x166, 0x17A}               the two channels' phase shift
//   PH_STAGES     {0x167, 0x168, ... 0x16E}    the eight section weights, one each
//   PH_DIR        {0x171, 0x172, 0x173}        the diffuse tap; only phaser1() writes these
//   PH_WIDTH1     {0x179}                      the feedback width, phaser1()'s one word
//   PH_WIDTH2     {0x178, 0x18B}               the feedback width, phaser2()'s two words
//   PH_PAN        {0x19C, 0x19D, 0x19E, 0x19F} the wet and dry pairs, left then right
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, each b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and one of
// the output, the low shelf on a step that doubles its result and the high shelf on one that quadruples
// it.
//
// What the two types differ by is which words they write. phaser1() puts the phase shift 258 a step into
// both feedback words, its width word at 0x179 alone, and one half at one of the three diffuse words;
// phaser2() splits the shift 258 and 242 between the two feedback words, its width word at 0x178 and
// 0x18B, and leaves the diffuse words at the ROM's. The ROM's own word at 0x172 is 0x4000, so under
// Phaser2, which has no Diffuse parameter, r[119] is the right cascade's first section and the left
// output's wet signal is that; under Phaser1 with Diffuse off it is zero and with it on it is one of
// r[121], r[114] or r[117]. PH_PAN's four words are the same list for both, except that phaser1()
// negates the last when Diffuse is on.
//
// The 72 steps, in the groups the step loop below splits them into:
//
//   0x158-0x165  the two channels' EQ, two order-1 shelves each. 0x158 takes the left input r[5] into
//                r[103] through the ROM's ~unity word and doubled, and 0x159 to 0x15b are the low shelf:
//                b1 times r[103] a pass back, -a1 times r[104], then b0 times r[103] and the sum
//                doubled into r[104]. 0x15c to 0x15e are the high shelf over r[104] into r[105], the left
//                EQ output. 0x15f to 0x165 are the same four steps for the right channel, r[6] into
//                r[106] to r[108]. 0x15b, 0x15f and 0x163 also read the delay RAM, and 0x158 and 0x159
//                take the two serial inputs into m[2] and m[6]. EQ below is those two channels, seven
//                steps each: the input and then a shelf over it, the low one doubled and the high one
//                quadrupled.
//   0x166        the left cascade's swept gain: the class-1 load is acc = saturate(128 * k + m[8]), the
//                phase shift with the first LFO's depth on it, and r[122] and t[10] take it.
//   0x167-0x16e  the eight section weights. Each word scales one section's output into the running sum,
//                and the two halves are the two cascades: m[55], r[110], r[111] and r[112] on the left,
//                r[115] to r[118] on the right, so each cascade's sections 2 to 5 and neither one's
//                first. Both Stage handlers leave exactly two of the eight at HALF and the other six at
//                zero, and 0x16a and 0x16e double the sums into r[120] and r[121], which cancels the half
//                and leaves the two open sections of each cascade added up. The ROM's own words, which
//                are what the two types run with until Stage moves, leave 0x169 and 0x16b at half. SUMS
//                below is those two halves, four steps each.
//   0x16f        t[10] = r[122] = saturate(t[10] * r[122]), the left gain squared.
//   0x170-0x173  the diffuse tap: four steps over r[120], r[121], r[114] and r[117], of which only one
//                word is ever half, and r[119] takes the sum doubled. The same four steps as a stage sum,
//                so it is the third row of SUMS.
//   0x174-0x175  the left gain finished. The class-1 load of the ROM's 0x7ffe puts about 128 in the
//                accumulator and 0x175 takes it off t[10] * r[122], so t[10] ends up the phase shift to
//                the fourth less 128.
//   0x176-0x179  the feedback the left cascade starts from. The two ROM's quarters add the EQ outputs,
//                the two width words add the stage sums, and m[54] takes the sum saturated.
//   0x17a        the right cascade's swept gain, the same class-1 load on PH_FB[1] and m[9], the second
//                LFO's depth.
//   0x17b-0x17c  the left cascade's first section and the only place the cross-channel feedback enters:
//                acc = t[10] * r[109] - m[54], then r[109] = saturate(t[10] * m[54] - acc).
//   0x17d        t[11] = r[122] = saturate(t[11] * r[122]), the right gain squared.
//   0x17e-0x17f  the left cascade's second section, on the same r[109]: acc = t[10] * m[55] - r[109],
//                then m[55] = saturate(t[10] * r[109] + acc). No register takes it; PH_STAGES[0] is the
//                only step that reads m[55] besides 0x181.
//   0x180        the class-1 load of the ROM's zero, and m[56] takes the accumulator, which is the
//                constant the right gain subtracts at 0x17d and 0x189.
//   0x181-0x188  the left cascade's third to fifth sections, the same two steps each: r[110] against
//                m[55], r[111] against r[110] and r[112] against r[111], with the class-3 steps 0x183
//                and 0x186 putting the earlier register into the accumulator the step subtracts from.
//                FEEDS below is the left cascade's first three sections, two steps each, and CHAINS the
//                plain three-step sections: its last two, then the right cascade's five.
//   0x189        the right gain to the fourth: acc = t[11] * r[122] - m[56], and t[11] takes it.
//   0x18a-0x18c  what the right cascade starts from: r[113] = saturate(r[120] + PH_WIDTH2[1] * r[121]),
//                the ROM's ~unity word on the left sum.
//   0x18d-0x19b  the right cascade's five sections, r[114] to r[118] each against the register before
//                it, the same two steps as the left's and with t[11] as the gain.
//   0x19c-0x19f  the dry and wet, PH_PAN's four words. r[15] takes the diffuse tap r[119] on the dry
//                word and the left EQ output r[105] on the wet word, both quadrupled; r[16] the right
//                stage sum r[121] and the right EQ output r[108] the same way. dryWet() runs the
//                variation blocks fully wet, so the two wet words are near zero.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d write
// them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005).
// r[103] to r[105] the left channel's EQ: input, low shelf, high shelf; r[106] to r[108] the right
// channel's. r[109] the left cascade's first section, m[55] its second, r[110] to r[112] its third to
// fifth; r[113] the right cascade's input and r[114] to r[118] its five sections. r[119] the diffuse
// tap, r[120] and r[121] the two stage sums, r[122] the swept gain's scratch, which all six gain steps
// write in turn. The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[8]
// and m[9] the window's two LFOs, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the
// six published delay RAM reads, m[54] the cross-channel feedback, m[55] the left cascade's second
// section and m[56] the ROM's zero. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, read at the
// ring pointer with no index; this program writes none of them, so the lines behind these taps are
// filled elsewhere. Each capture takes the read five steps before it (captured()'s lower bound), so
// m[28] carries the slot 0x56 word, m[29] the slot 0x57 word, m[30] the slot 0x58 word, and m[31],
// m[32] and m[33] the slots 0x63, 0x64 and 0x65 words.
//
// The two t registers: t[10] the left cascade's gain, written at 0x166, 0x16f and 0x175 and read by its
// ten section steps and by the two gain steps; t[11] the right cascade's, written at 0x17a, 0x17d and
// 0x189 and read the same way. A t write saturates the accumulator and divides it by 128, so the left
// gain is the right gain less 1 wherever the two phase shift words and the two LFO depths agree, which
// is what phaser1() sets up and what makes the two sweeps run against each other; phaser2() splits the
// two apart, 258 and 242 a step, and gives the second LFO its own depth and phase. The sections
// themselves are first order and each one is its own register against the one before it, one pass back,
// which is the shape of the old model's `y = g * x + ap[i]; ap[i] = x - g * y` in git show
// 29f589c^:src/fs1r/chips/vop3_effects.h, written here with the signs and the scaling the words give it.
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
// vop3_native_check var-m05: identical, 2 types, 19200 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, run and the rest

// The registers, cells and t registers the comments above name, so the tables below read as the DSP
// they run rather than as a list of numbers.
constexpr int TL = 10, TR = 11;      // t[10] and t[11]: the two cascades' swept gains
constexpr int GAIN = 122;            // r[122]: the swept gain's scratch, which every gain step reads
constexpr int FEEDBACK = 54;         // m[54]: the cross-channel feedback the left cascade starts from
constexpr int SECTION2 = 55;         // m[55]: the left cascade's second section, a cell and not a register
constexpr int ZERO = 56;             // m[56]: the ROM's own zero, which the right gain subtracts

// ---------------------------------------------------------------------------------------------
// What a step does with a cell or the delay RAM before it does any arithmetic
// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// The two channels' EQ, two order-1 shelves each
// ---------------------------------------------------------------------------------------------

// One channel's seven steps. `src` is the input the base program leaves this window in, `in` this
// channel's own copy of it through the ROM's ~unity word doubled, `lo` the low shelf's output and `hi`
// the high shelf's, which is the channel's EQ output. Each shelf is b1 * x[n-1] - a1 * y[n-1] with
// b0 * x[n] closing the sum, the low shelf closing doubled and the high one quadrupled.
struct Eq
{
    int base;
    int src, in, lo, hi;
    Decor decor[7];
};

// The left channel, whose two serial inputs land in m[2] and m[6] for the base program, then the right
// one, which publishes the two reads the base program asked for. The seven steps are the channel's
// input on the ROM's ~unity word and then the low shelf's b1, -a1 and b0, then the high shelf's b1,
// -a1 and b0, and each row's decor list is in that order.
constexpr Eq EQS[2] = {
    //  base   src  in   lo   hi   the seven steps' decor, in that order
    { 0x158, 5, 103, 104, 105, { {DEC_SERIAL_L, 2}, {DEC_SERIAL_R, 6}, {}, {DEC_READ, 0}, {},            {},                {} } },
    { 0x15f, 6, 106, 107, 108, { {DEC_READ, 0},     {DEC_CAPTURE, 28}, {}, {},            {DEC_READ, 0}, {DEC_CAPTURE, 29}, {} } },
};

// One channel's EQ step. The first step takes the input the base program wrote and starts nothing, and
// the six after it are the two shelves: each scales the shelf's input a pass back, takes that shelf's
// own output a pass back off it, and closes the sum on the input again.
void eq(yss236_core& c, int st, Eq const& q)
{
    int const k = st - q.base;
    decor(c, st, q.decor[k]);
    switch (k)
    {
    case 0:  gain(c, st, q.src, q.in, ROUTE_2); break;    // the channel's input on the ROM's ~unity word
    case 1:  term(c, st, q.in); break;                    // the low shelf's b1 on x[n-1]
    case 2:  sum(c, st, q.lo); break;                     // and its -a1 on y[n-1]
    case 3:  close(c, st, q.in, q.lo, ROUTE_2); break;    // b0 on x[n] closing into the low shelf, doubled
    case 4:  term(c, st, q.lo); break;                    // the high shelf's b1 on the low shelf's output
    case 5:  sum(c, st, q.hi); break;                     // and its -a1 on its own output
    default: close(c, st, q.lo, q.hi, ROUTE_4); break;    // b0 closing into the EQ output, quadrupled
    }
}

// ---------------------------------------------------------------------------------------------
// The weighted runs of four: the section weights and the diffuse tap
// ---------------------------------------------------------------------------------------------

// Four words each scaled onto the running value, of which the last closes the run: a scaled sum
// through the run's route into `out`, saturated. The first word is a register in two of the runs and
// a cell in the third, which is the left cascade's second section and is what `first_cell` says.
struct Sum
{
    int base;
    int first_reg;
    int first_cell;
    int src[3];          // the second, third and fourth words' registers
    int out;
    route_t route;
};

// The two halves of the eight section weights, which are the two cascades, and then the diffuse tap,
// which is the same four steps on its own four registers.
constexpr Sum SUMS[3] = {
    //  base  first  cell      the three registers        out  route
    { 0x167, 0,   SECTION2, { 110, 111, 112 }, 120, ROUTE_2 },
    { 0x16b, 115, 0,        { 116, 117, 118 }, 121, ROUTE_2 },
    { 0x170, 120, 0,        { 121, 114, 117 }, 119, ROUTE_2 },
};

// One of those four steps, at base to base + 3.
void sum4(yss236_core& c, int st, Sum const& s)
{
    switch (st - s.base)
    {
    case 0:
        if (s.first_cell) term_cell(c, st, s.first_cell);  // the run opens on a cell
        else              term(c, st, s.first_reg);        // or on a register
        break;
    case 3:  close(c, st, s.src[2], s.out, s.route); break;
    default: sum(c, st, s.src[st - s.base - 1]); break;
    }
}

// ---------------------------------------------------------------------------------------------
// The two cascades' swept gain, which the six gain steps between 0x166 and 0x189 compute
// ---------------------------------------------------------------------------------------------

// 0x166 and 0x17a, the phase shift word with a cascade's LFO depth on it: a class-1 load adds the
// depth out of its own cell, and both r[122] and the cascade's t register take the result, so the two
// cascades sweep against each other.
void gain_load(yss236_core& c, int st, int tn, int depth)
{
    load(c, {.st = st, .sm = depth, .add = ADD_CELL, .clamp = CLAMP_SAT, .dr = GAIN});
    t_put(c, st, tn, c.acc);
}

// 0x16f and 0x17d: t[tn] = r[122] = saturate(t[tn] * r[122]), the gain squared and back into its own
// t register.
void gain_square(yss236_core& c, int st, int tn)
{
    t_put(c, st, tn, run(c, {.st = st, .sr = GAIN, .treg = tn, .clamp = CLAMP_SAT, .dr = GAIN}));
}

// 0x175 and 0x189: the square on r[122] times the gain, less what that cascade subtracts, which is
// the offset the class-1 load at 0x174 left in the running value on the left and the ROM's own zero
// cell on the right.
void gain_to_fourth(yss236_core& c, int st, int tn, add_t add, int cell)
{
    t_put(c, st, tn, run(c, {.st = st, .sr = GAIN, .sm = cell, .treg = tn, .add = add, .sub = true, .clamp = CLAMP_SAT}));
}

// ---------------------------------------------------------------------------------------------
// The two cascades, ten first order sections, each on the one before it a pass back
// ---------------------------------------------------------------------------------------------

// Which of the two shapes a section takes where it is not the plain register-against-register pair.
// SH_TAP is a section fed from a cell, which is the cross-channel feedback or the cell a pass back:
// its first step subtracts the cell and its second writes the section's register from it. SH_CELL is
// the one section whose own output is that cell, so the pair is the other way round: the first step
// scales the cell and subtracts the register, the second scales the register and writes the cell.
enum feed_t { SH_TAP, SH_CELL };

// One of the left cascade's first three sections, two steps at base and base + 1, working between one
// register and one cell. `gain` is t[10], the left cascade's gain, which every section step scales by.
struct Feed
{
    int base;
    int gain;
    int reg;
    int cell;
    feed_t shape;
};

// The first section, on the cross-channel feedback the left cascade starts from; the second, whose own
// output is the cell; and the third, on that cell a pass back.
constexpr Feed FEEDS[3] = {
    //  base        gain  reg  cell      shape
    { 0x17b, TL, 109, FEEDBACK, SH_TAP  },
    { 0x17e, TL, 109, SECTION2, SH_CELL },
    { 0x181, TL, 110, SECTION2, SH_TAP  },
};

// One of those two steps: the first opens the running value on the section's input and the second
// closes it into the section's output, which the next section reads a pass back.
void feed(yss236_core& c, int st, Feed const& f)
{
    if (f.shape == SH_TAP)
    {
        if (st == f.base)
            run(c, {.st = st, .sr = f.reg, .sm = f.cell, .treg = f.gain, .add = ADD_CELL, .sub = true});
        else
            run(c, {.st = st, .sm = f.cell, .m2 = M2_CELL, .treg = f.gain, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = f.reg});
    }
    else
    {
        if (st == f.base)
            run(c, {.st = st, .sr = f.reg, .sm = f.cell, .m2 = M2_CELL, .treg = f.gain, .add = ADD_REG, .sub = true});
        else
            run(c, {.st = st, .sr = f.reg, .treg = f.gain, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dm = f.cell});
    }
}

// A plain section of either cascade, three steps. `in` is the register the unity step puts into the
// running value and the closing step scales, `out` the register the middle step scales and the closing
// step writes, so the section is y = g * x - z with z the previous section's output a pass back.
struct Chain
{
    int base;
    int gain;
    int in, out;
    Decor decor[3];
};

// The left cascade's last two sections and then the right cascade's five, the same three steps each:
// the unity step that carries the section before's output, the section, and the step that writes the
// section's own output register. The rows are three steps apart, so a step's row is its own offset
// over three from the block's base.
constexpr Chain CHAINS[7] = {
    //  base        gain  in  out  the three steps' decor, in that order
    { 0x183, TL, 110, 111, { {},            {},                {}                } },
    { 0x186, TL, 111, 112, { {DEC_LFO, 12}, {},                {}                } },
    { 0x18d, TR, 113, 114, { {},            {},                {DEC_READ, 0}     } },
    { 0x190, TR, 114, 115, { {},            {},                {}                } },
    { 0x193, TR, 115, 116, { {DEC_READ, 0}, {DEC_CAPTURE, 31}, {}                } },
    { 0x196, TR, 116, 117, { {},            {DEC_READ, 0},     {DEC_CAPTURE, 32} } },
    { 0x199, TR, 117, 118, { {},            {},                {}                } },
};

// One of those three steps, at base to base + 2.
void chain(yss236_core& c, int st, Chain const& s)
{
    int const k = st - s.base;
    decor(c, st, s.decor[k]);
    switch (k)
    {
    case 0:  run(c, {.st = st, .sr = s.in, .unity = true}); break;                    // the class-3 unity step
    case 1:  run(c, {.st = st, .sr = s.out, .treg = s.gain, .add = ADD_ACC, .sub = true}); break;
    default: run(c, {.st = st, .sr = s.in, .treg = s.gain, .add = ADD_ACC, .sub = true, .clamp = CLAMP_SAT, .dr = s.out}); break;
    }
}

// ---------------------------------------------------------------------------------------------
// What each cascade starts from, and the two outputs
// ---------------------------------------------------------------------------------------------

// 0x176 to 0x179, the feedback the left cascade starts from: the two ROM's quarters add the channels'
// EQ outputs to the running value and the two width words add the two cascades' section sums on top,
// and m[54] takes the sum saturated.
void left_feedback(yss236_core& c, int st)
{
    switch (st)
    {
    case 0x176: term(c, st, 105); break;      // a quarter of the left EQ output on the ROM's own word
    case 0x177: run(c, {.st = st, .sr = 108, .add = ADD_ACC, .route = ROUTE_2}); break;
    case 0x178: sum(c, st, 120); break;       // the left cascade's four sections, on PH_WIDTH1's word
    case 0x179:  // the right cascade's four sections close the sum and m[54] takes it saturated
        run(c, {.st = st, .sr = 121, .add = ADD_ACC, .clamp = CLAMP_SAT, .dm = FEEDBACK});
        break;
    }
}

// 0x18a to 0x18c, what the right cascade starts from: the ROM's own word on the right EQ output opens
// the running value, PH_WIDTH2's second word adds the right cascade's section sum, and the ROM's
// ~unity word on the left cascade's sum closes the three into r[113].
void right_input(yss236_core& c, int st)
{
    switch (st)
    {
    case 0x18a: run(c, {.st = st, .sr = 108, .route = ROUTE_2}); break;
    case 0x18b: sum(c, st, 121); break;       // the right cascade's four sections, on PH_WIDTH2's word
    default:    close(c, st, 120, 113); break;
    }
}

// One channel's two output steps, at base and base + 1. PH_PAN's wet word scales the channel's EQ
// output onto the running value and its dry word adds the wet signal, the diffuse tap on the left and
// the right cascade's sum on the right, which the closing step quadruples into the register the base
// program reads back.
struct Output
{
    int base;
    int out;
    int wet;
    int dry;
};

constexpr Output OUTPUTS[2] = {
    //  base   out  wet  dry
    { 0x19c, 15, 105, 119 },
    { 0x19e, 16, 108, 121 },
};

void output(yss236_core& c, int st, Output const& o)
{
    if (st == o.base) term(c, st, o.wet);     // the wet word, on the channel's EQ output
    else close(c, st, o.dry, o.out, ROUTE_4); // and the dry word closing the pair into the output
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

        if (st < 0x166)
        {
            // ---- 0x158 to 0x165, the two channels' EQ ----
            eq(c, st, EQS[(st - 0x158) / 7]);
            return;
        }
        if (st == 0x166)
        {
            // ---- the left cascade's swept gain, the phase shift with the first LFO's depth on it ----
            take_lfo(c, 11, st);      // this page's LFO, published for the base program
            gain_load(c, st, TL, 8);
            return;
        }
        if (st < 0x16f)
        {
            // ---- 0x167 to 0x16e, the eight section weights, four a cascade ----
            if (st == 0x168) take_capture(c, 30, st);      // the read of 0x163, five steps earlier
            sum4(c, st, SUMS[(st - 0x167) / 4]);
            return;
        }
        if (st == 0x16f)
        {
            // ---- the left gain squared ----
            gain_square(c, st, TL);
            return;
        }
        if (st < 0x174)
        {
            // ---- 0x170 to 0x173, the diffuse tap ----
            sum4(c, st, SUMS[2]);
            return;
        }
        if (st == 0x174)
        {
            // ---- the constant the left gain's fourth power comes off: the ROM's 0x7ffe, about 128 ----
            load(c, {.st = st});
            return;
        }
        if (st == 0x175)
        {
            // ---- the left gain to the fourth, less that constant ----
            gain_to_fourth(c, st, TL, ADD_ACC, 0);
            return;
        }
        if (st < 0x17a)
        {
            // ---- 0x176 to 0x179, the feedback the left cascade starts from ----
            left_feedback(c, st);
            return;
        }
        if (st == 0x17a)
        {
            // ---- the right cascade's swept gain, the same load on the second LFO's depth ----
            gain_load(c, st, TR, 9);
            return;
        }
        if (st < 0x189)
        {
            // ---- 0x17b to 0x188, the left cascade, with the right gain's own steps between its
            //      first three sections ----
            if (st == 0x17d)
            {
                gain_square(c, st, TR);
                return;
            }
            if (st == 0x180)
            {
                // the ROM's own zero, which m[56] takes and the right gain subtracts
                load(c, {.st = st, .clamp = CLAMP_SAT, .dm = ZERO});
                return;
            }
            if (st < 0x183) feed(c, st, FEEDS[(st - 0x17b) / 3]);
            else            chain(c, st, CHAINS[(st - 0x183) / 3]);
            return;
        }
        if (st == 0x189)
        {
            // ---- the right gain to the fourth, less the zero cell ----
            gain_to_fourth(c, st, TR, ADD_CELL, ZERO);
            return;
        }
        if (st < 0x18d)
        {
            // ---- 0x18a to 0x18c, what the right cascade starts from ----
            right_input(c, st);
            return;
        }
        if (st < 0x19c)
        {
            // ---- 0x18d to 0x19b, the right cascade's five sections ----
            chain(c, st, CHAINS[2 + (st - 0x18d) / 3]);
            return;
        }

        // ---- 0x19c to 0x19f, the dry and wet ----
        if (st == 0x19c) take_capture(c, 33, st);      // the read of 0x197, five steps earlier
        output(c, st, OUTPUTS[(st - 0x19c) / 2]);
        });
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m05(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

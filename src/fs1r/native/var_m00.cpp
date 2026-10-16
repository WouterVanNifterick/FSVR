// fs1r/native/var_m00.cpp - VOP3-2, the variation window's program for No Effect, Chorus, Celeste,
// Flanger and Symphonic: its 72 steps.
//
// rom::VARIATION_PROGRAMS[0], the program the window holds for the five variation types 0 to 4, and the
// only program among the twelve that these five share. Under each of them the same program words run
// and only the constants (rom::var's word lists, the LFO depths and the feedback level) and the eight
// delay offsets (rom::var::MOD_SLOTS) move, so this native is one function for all five and no type is
// excluded. Which of the five is running is not a code difference here; the host's parameters are.
//
// What the 72 steps compute, in the groups the comments below open:
//
//   0x158-0x15a  the three registers the block's pan balance works from: r[124], r[123] and r[125] off
//                the shared cell m[8], and the two serial inputs published as m[2] and m[6].
//   0x15b-0x163  the first three reads of the block's own area (the slots 0x56, 0x57 and 0x58) and the
//                envelope network: r[124], r[126], r[127], r[1] and r[2] decay and ramp against the
//                coefficients t[12] and t[13], and the two of them the block reads back.
//   0x164-0x16e  the m[29] and m[30] captures, two more envelope steps, and the first modulated delay
//                position: 0x16c takes r[124]'s running value, clamps it, writes its fraction to t[9] and
//                loads it as the delay index the taps below read at.
//   0x16f-0x184  the left channel: the input path r[5] to r[103], r[104] and r[105]; the four taps of
//                the left delay line (the slots 0x5b, 0x5c, 0x5d and 0x5e), each read at the modulated
//                index and captured five steps later into m[56], m[57], m[60] and m[61]; and the two
//                pairwise tap sums in r[119] and r[121], each t[9] or t[11] against the cell beside it.
//   0x185-0x19f  the right channel, the mirror of the left one: the input path r[6] to r[106], r[107]
//                and r[108], the four taps of the right delay line read in the order 0x5f, 0x60, 0x61,
//                0x62 into m[62], m[63], m[58] and m[59], the two pairwise sums in r[122] and r[120],
//                the two outputs r[15] and r[16], the three published reads of the slots 0x63, 0x64
//                and 0x65 into m[31], m[32] and m[33], and the two delay line writes, the left one from
//                the write latch set at 0x18e and taken at 0x19b, the right one written directly at
//                0x19f.
//
// So the block is a pair of modulated delay lines with an envelope on each, not a filter: the eight
// taps are read at one moving index, each pair of neighbouring cells is interpolated by the fraction
// t[9], t[10] or t[11] that the three position steps published, and the four pairwise sums are the
// stereo mix the outputs close from.
//
// The registers, as the groups above name them: r[5] and r[6] the two serial inputs, r[103], r[106] the
// scaled inputs, r[104], r[105], r[107], r[108] the input accumulators, r[15] and r[16] the two outputs,
// which the base program reads at 0x018 and 0x03b as the later stereo pair after r[13] and r[14],
// r[119] to r[122] the four tap sums, r[123] to r[127] and r[1], r[2] the envelope and ramp registers.
// The cells: m[8] the shared balance cell, m[2] and m[6] the serial inputs this window publishes, m[11]
// and m[12] the two LFO values it publishes, m[28] to m[33] the six published reads and m[56] to m[63]
// the eight modulated delay taps. The delay slots: 0x66 and 0x67 the two delay lines, 0x5b to 0x62 the
// eight taps (rom::var::MOD_SLOTS), 0x56 to 0x58 and 0x63 to 0x65 the six reads it hands on.
//
// The t registers: t[12] and t[13] the two decay coefficients the envelope network multiplies by, and
// t[9], t[10] and t[11] the three fractional delay positions, each of which also loads the delay index
// the taps at that position's four steps read at. Every one of the six is written from a running value
// the step before it clamps, so they move with the block and not with a parameter directly.
//
// The steps come in three shapes' worth of repetition, and those are the tables below. ENVELOPES is the
// envelope network's four-step cycle, which runs twice, once per envelope. CHAINS is the seven-step
// input path a channel's r[in] runs through on the way into its delay line, once per channel. TAPS is
// the four two-step interpolations between neighbouring tap cells, one per pairwise sum. The tail past
// 0x191 is two three-step mix units and two two-step output units, MIXES and OUTPUTS, plus the two
// delay line writes. Everything else is a single step of one of the shapes below, called by what it
// computes: a balance register off the shared cell, a ramp, an envelope decay pole, a scaled register,
// a modulated delay position, the half of a tap interpolation.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own. One shape steps.h has no name for is a delay RAM read
// at the moving index the position steps load (w2[3] adds the index to the memory cycle), which
// read_index() below spells out; everything else is steps.h's own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, and fs1r/native/steps.h is those shapes, so the arithmetic below is spelled out
// by name rather than a step at a time.
//
// vop3_native_check var-m00: identical, 22 types, 211200 native passes.

#include "fs1r/native/native.h"
#include "fs1r/native/steps.h"

namespace fs1r::native {

namespace {

using namespace step;   // the step vocabulary: term, sum, close, gain, load and the rest

// ---------------------------------------------------------------------------------------------
// The delay RAM reads
// ---------------------------------------------------------------------------------------------

// A delay RAM read at the moving index the position steps load. The slot is still the step's own over
// four, and `older` adds the one word the memop's high bit asks for, exactly as step() takes it:
// steps.h's read_delay() reaches the ring pointer only, because no other program reads at an index.
void read_index(yss236_core& c, int st, int older)
{
    c.push_hist(st, c.ram[c.addr(st >> 2, older + c.cur_idx(), false)]);
}

// How a step reads the delay RAM: nowhere, at the ring pointer or one word older there, or at the
// moving index with the same two choices.
enum read_t { NO_READ, AT_RING, AT_RING_OLDER, AT_INDEX, AT_INDEX_OLDER };

void read_shape(yss236_core& c, int st, read_t how)
{
    switch (how)
    {
        case AT_RING:        read_delay(c, st); break;
        case AT_RING_OLDER:  read_delay(c, st, true); break;
        case AT_INDEX:       read_index(c, st, 0); break;
        case AT_INDEX_OLDER: read_index(c, st, 1); break;
        default: break;                            // NO_READ
    }
}

// ---------------------------------------------------------------------------------------------
// The class-1 loads: the step's word loaded as a value rather than applied to one
// ---------------------------------------------------------------------------------------------

// r[dr] = the step's word doubled onto the balance cell m[8]. These three, into r[124], r[1] and
// r[126], are the envelope registers the whole block's pan balance runs from.
void add_cell(yss236_core& c, int st, int dr)
{
    load(c, st, 0, 8, ADD_CELL, false, ROUTE_2, CLAMP_WRAP, dr);
}

// r[dr] = the step's word doubled onto the running value, wrapping. Once only, into r[125], which is
// the balance register the later |...| step reads back.
void add_acc(yss236_core& c, int st, int dr)
{
    load(c, st, 0, 0, ADD_ACC, false, ROUTE_2, CLAMP_WRAP, dr);
}

// r[dr] = the step's word doubled onto r[123], which is the balance cell on its own through the
// 16x its own step's route took. Into r[2] and r[127].
void add_reg(yss236_core& c, int st, int dr)
{
    load(c, st, 123, 0, ADD_REG, false, ROUTE_2, CLAMP_WRAP, dr);
}

// r[dr] = the step's word doubled onto the running value and saturated, and t[tn] takes that same
// value as a 1.15 word. This is where an envelope's decay coefficient is published: the next cycle's
// two decay steps multiply by it.
void decay_word(yss236_core& c, int st, int dr, int tn)
{
    load(c, st, 0, 0, ADD_ACC, false, ROUTE_2, CLAMP_SAT, dr);
    t_put(c, tn, c.acc);
}

// r[dr] = the step's own word loaded onto the running value, with the negatives clamped away: a ramp,
// and the position the block's delay lines are read at.
void ramp(yss236_core& c, int st, int dr)
{
    load(c, st, 0, 0, ADD_ACC, false, ROUTE_1, CLAMP_ZERO, dr);
}

// ---------------------------------------------------------------------------------------------
// The class-2 and class-3 steps
// ---------------------------------------------------------------------------------------------

// acc = |r[sr]|: a class-3 step, where the multiplier passes the register through and the clamp alone
// shapes it. These three read the envelope registers the class-1 loads wrote.
void abs_read(yss236_core& c, int st, int sr)
{
    run(c, {st, sr, 0, M2_REG, true, 0, ADD_OFF, false, ROUTE_1, CLAMP_ABS, 0});
}

// acc = |the step's word on r[sr]|: the same clamp on a register whose word sets the running value.
void abs_term(yss236_core& c, int st, int sr)
{
    run(c, {st, sr, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_1, CLAMP_ABS, 0});
}

// r[dr] = t[tn] * r[sr], saturated: one pole of an envelope register's decay, t[tn] being the
// coefficient a decay_word() step published.
void decay(yss236_core& c, int st, int tn, int sr, int dr)
{
    run(c, {st, sr, 0, M2_REG, false, tn, ADD_OFF, false, ROUTE_1, CLAMP_SAT, dr});
}

// acc = 16 * the step's word * r[sr], saturated: a register scaled into the running value for the next
// step to add a second register to.
void scale(yss236_core& c, int st, int sr)
{
    run(c, {st, sr, 0, M2_REG, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_SAT, 0});
}

// acc = the step's word on r[sr] added to the running value, negatives clamped away; t[tn] takes the
// fraction below one delay sample of that value and the delay index takes its floor. A modulated delay
// position: the fraction the four tap interpolations multiply by, and the index every read below runs
// at until the next position step replaces it.
void position(yss236_core& c, int st, int sr, int tn)
{
    run(c, {st, sr, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_ZERO, 0});
    t_put_fraction(c, tn, c.acc);
    load_index(c, c.acc);
}

// The first half of a tap interpolation: acc = t[tn] * m[cell] - m[cell], the same cell scaled by the
// fraction less one. It is left in the running value for the second half to take back.
void tap_ramp(yss236_core& c, int st, int tn, int cell)
{
    run(c, {st, 0, cell, M2_CELL, false, tn, ADD_CELL, true, ROUTE_1, CLAMP_WRAP, 0});
}

// The second half: r[dr] = t[tn] * m[cell] - acc, where `cell` is the cell after the first half's.
// That running value is the first cell scaled by the fraction less one, so taking it back leaves
// m[cell - 1] + t[tn] * (m[cell] - m[cell - 1]): the linear interpolation between two neighbouring
// samples of the delay line, which the delay RAM's own word could not give on its own.
void tap_interp(yss236_core& c, int st, int tn, int cell, int dr)
{
    run(c, {st, 0, cell, M2_CELL, false, tn, ADD_ACC, true, ROUTE_1, CLAMP_SAT, dr});
}

// ---------------------------------------------------------------------------------------------
// The three repeated runs: the envelope cycle, the input chain and the tap pairs
// ---------------------------------------------------------------------------------------------

// One envelope cycle's four steps. `pos` is the t register the two decay steps multiply by and `tn`
// the one the cycle's own decay word lands in, and the two are each other's: each cycle publishes the
// envelope it read into the other one's coefficient register, so the two envelopes trade levels.
// `src` and `dst` are an envelope register and the one step behind it that the decay runs along,
// `take` the cell the block's published read lands in, and `env` the register the |...| reading and
// the decay word both work on.
struct Envelope { int base; int pos, tn; int src, dst; int take, env; read_t read; };

// The envelope network runs this cycle twice: once for the pair r[124] and r[127], once for the pair
// r[1] and r[119]. The second cycle's decay word is r[126]'s, which the steps past it read back, and
// it is the first cycle's opening step that reads the block's second published slot.
const Envelope ENVELOPES[2] = {
    //  base    pos  tn  src  dst  take  env  read
    {  0x15f,  12, 13, 124, 127,   28,   1, AT_RING },
    {  0x167,  13, 12,   1, 119,   30, 126, NO_READ },
};

// One envelope cycle, run one step at a time.
void envelope_cycle(yss236_core& c, int st, Envelope const& e, int k)
{
    switch (k)
    {
    case 0:  read_shape(c, st, e.read); decay(c, st, e.pos, e.src, e.dst); break;
    case 1:  take_capture(c, e.take, st);                     // the read the block publishes
             abs_read(c, st, e.env); break;                   // acc = |r[env]|
    case 2:  decay_word(c, st, e.env, e.tn); break;           // r[env] and t[tn] off that
    default: decay(c, st, e.pos, e.dst, e.dst); break;        // the delay's own pole
    }
}

// One channel's input path, the seven steps its serial input runs through on the way into its delay
// line. `in` is r[5] or r[6], `scaled` the register the input is doubled into, `y1` and `y2` the two
// passes of the comb the delay line reads back, and `take` the cell the published read lands in.
struct Chain { int base; int in, scaled, y1, y2; int take; };

// The two channels' paths, the right one the mirror of the left with every register three higher and
// the delay slots the other four of the eight taps.
const Chain CHAINS[2] = {
    //  base    in  scaled  y1  y2  take
    {  0x175,   5,    103, 104, 105,   57 },
    {  0x185,   6,    106, 107, 108,   63 },
};

// One channel's input path, run one step at a time. Both reads are at the moving index, the second one
// word older, and each is captured five steps later into one cell of the tap pair it feeds.
void input_chain(yss236_core& c, int st, Chain const& ch, int k)
{
    switch (k)
    {
    case 0:  gain(c, st, ch.in, ch.scaled, ROUTE_2); break;          // r[scaled] = 2 k r[in]
    case 1:  term(c, st, ch.y1); break;                              // the running value off r[y1]
    case 2:  read_shape(c, st, AT_INDEX); sum(c, st, ch.scaled); break;
    case 3:  take_capture(c, ch.take, st); close(c, st, ch.scaled, ch.y1, ROUTE_2); break;
    case 4:  term(c, st, ch.y2); break;
    case 5:  sum(c, st, ch.y1); break;
    default: read_shape(c, st, AT_INDEX_OLDER); close(c, st, ch.y1, ch.y2, ROUTE_4); break;
    }
}

// One tap pair's two steps. `base` is the pair's first step, `pos` the t register holding the
// fraction, `tap` the cell the pair's interpolation starts from and `tap + 1` the one it ends at,
// `mix` the register the pair closes into, `read` and `lfo` what the first step takes on its way past,
// and `take_first` and `take_second` the cells the two steps publish into.
struct Tap { int base; int pos, tap, mix; int take_first, take_second; read_t read; int lfo; };

// The four interpolations, one per pairwise sum, in the order the program reaches them. Each pair runs
// over two neighbouring tap cells the block captured five steps earlier: m[56] and m[57], which took
// the reads of the slots 0x5b and 0x5c, at t[9]; m[60] and m[61] off 0x5d and 0x5e at t[11]; m[62] and
// m[63] off 0x5f and 0x60 at t[11]; and m[58] and m[59] off 0x61 and 0x62 at t[10]. The first two
// close into r[119] and r[121], the left channel's two sums, and the last two into r[122] and r[120],
// the right channel's, so the mix reads the four sums across in that order. The first step of each pair
// also makes the block's next read: 0x17f reads the slot 0x5f, whose capture the pair at 0x18f
// interpolates, and 0x18f reads the slot 0x63, which is one of the six reads published rather than a
// tap. The pair at 0x194 has no read of its own and publishes that one instead.
const Tap TAPS[4] = {
    //  base    pos  tap  mix  take_first  take_second  read              lfo
    {  0x17f,   9,  56, 119,           0,          61, AT_INDEX,         0 },
    {  0x183,  11,  60, 121,           0,          62, AT_INDEX_OLDER,  0 },
    {  0x18f,  11,  62, 122,           0,          59, AT_RING,        12 },
    {  0x194,  10,  58, 120,          31,           0, NO_READ,         0 },
};

// One tap pair, run one step at a time.
void tap_pair(yss236_core& c, int st, Tap const& tp)
{
    if (st == tp.base)
    {
        read_shape(c, st, tp.read);
        if (tp.lfo) take_lfo(c, tp.lfo, st);
        if (tp.take_first) take_capture(c, tp.take_first, st);
        tap_ramp(c, st, tp.pos, tp.tap);
        return;
    }
    if (tp.take_second) take_capture(c, tp.take_second, st);
    tap_interp(c, st, tp.pos, tp.tap + 1, tp.mix);
}

// ---------------------------------------------------------------------------------------------
// The tail: the mix units, the output units and the two delay line writes
// ---------------------------------------------------------------------------------------------

// One mix unit's three steps, which is where the four tap sums reach the stereo pair. `seed` is the
// register the running value starts from, `add` the one added to it, `src` and `out` the last step's
// own word on `src` closed into `out`. The left unit runs first and its closing step reads the block's
// own area; the right unit's seeding step is where that read is taken up.
struct Mix { int base; int seed, add; int src, out; route_t route; int take; read_t read; };

const Mix MIXES[2] = {
    //  base   seed  add  src  out  route     take  read
    {  0x191,  119, 121, 122, 124, ROUTE_1,   0,   AT_RING },
    {  0x198,  121, 122, 120, 125, ROUTE_1,  32,   NO_READ },
};

void mix_step(yss236_core& c, int st, Mix const& mx)
{
    switch (st - mx.base)
    {
    case 0:  if (mx.take) take_capture(c, mx.take, st); term(c, st, mx.seed); break;
    case 1:  sum(c, st, mx.add); break;
    default: read_shape(c, st, mx.read); close(c, st, mx.src, mx.out, mx.route); break;
    }
}

// One output unit's two steps. `feed` seeds the running value from the channel's own input
// accumulator and the sum is closed into `out`, the register the base program reads as one half of
// the later stereo pair, with the x4 route. The left unit's closing step reads the block's own area;
// the right unit's seeding step is where that read is taken up.
struct Output { int base, feed, src, out; int take; read_t read; };

const Output OUTPUTS[2] = {
    //  base  feed  src  out  take  read
    {  0x196,  105, 124,  15,    0,  AT_RING },
    {  0x19c,  108, 125,  16,   33,  NO_READ },
};

void output_step(yss236_core& c, int st, Output const& o)
{
    if (st == o.base)
    {
        if (o.take) take_capture(c, o.take, st);
        term(c, st, o.feed);
        return;
    }
    read_shape(c, st, o.read);
    close(c, st, o.src, o.out, ROUTE_4);
}

// The left delay line's own write, thirteen steps after 0x18e latched its value. The step computes
// nothing, so all it moves is the x latch.
void write_left_delay(yss236_core& c, int st)
{
    idle(c);
    if (c.haswl) { c.ram[c.addr(st >> 2, 0, false)] = c.wlatch; c.haswl = false; }
}

// The right delay line's write, which the step that computes the value makes itself: its result is
// clipped and goes to the slot, and the write latch the left line would have taken is cleared.
void write_right_delay(yss236_core& c, int st)
{
    double const y = run(c, {st, 120, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, 0});
    c.haswl = false;
    write_delay(c, st, yss236_core::clip(y, yss236_core::SAT));
}

// ---------------------------------------------------------------------------------------------
// The 72 steps
// ---------------------------------------------------------------------------------------------

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
    for (int st = 0x158; st < 0x1a0; st++)
    {
        c.begin_step();
        if (st < c.m_skip) continue;

        if (st < 0x15b)
        {
            // ---- the three balance registers and the two serial inputs this window publishes ----
            switch (st)
            {
            case 0x158: take_serial(c, 2, st, 0); add_cell(c, st, 124); break;
            case 0x159:
                take_serial(c, 6, st, 1);
                // r[123] = 16 * the step's word * m[8]: the balance cell alone, through the 16x this
                // step's own route takes, and the two steps at 0x15e and 0x171 then read back
                run(c, {st, 0, 8, M2_CELL, false, 0, ADD_OFF, false, ROUTE_16, CLAMP_WRAP, 123});
                break;
            default:    add_acc(c, st, 125); break;
            }
            continue;
        }

        if (st < 0x15f)
        {
            // ---- the first published read, then the three envelope registers off it ----
            if (st == 0x15b)
            {
                read_shape(c, st, AT_RING);        // the slot 0x56, the block's first published read
                abs_read(c, st, 124);
                continue;
            }
            switch (st)
            {
            case 0x15c: decay_word(c, st, 124, 12); break;
            case 0x15d: add_cell(c, st, 1); break;
            default:    add_reg(c, st, 2); break;
            }
            continue;
        }

        if (st < 0x167)
        {
            // ---- the first envelope cycle, the third balance register, and the feedback pair ----
            if (st < 0x163)
            {
                Envelope const& e = ENVELOPES[0];
                envelope_cycle(c, st, e, st - e.base);
                continue;
            }
            switch (st)
            {
            case 0x163: read_shape(c, st, AT_RING); add_cell(c, st, 126); break;
            case 0x164: take_capture(c, 29, st); scale(c, st, 124); break;
            case 0x165: sum(c, st, 127); break;    // the envelope's own delay, added back
            default:    ramp(c, st, 124); break;   // the sum closed into the envelope register
            }
            continue;
        }

        if (st < 0x16f)
        {
            // ---- the second envelope cycle and the first modulated delay position ----
            if (st < 0x16b)
            {
                Envelope const& e = ENVELOPES[1];
                envelope_cycle(c, st, e, st - e.base);
                continue;
            }
            switch (st)
            {
            case 0x16b: abs_term(c, st, 125); break;
            case 0x16c: position(c, st, 124, 9); break;
            case 0x16d: scale(c, st, 1); break;
            default:    sum(c, st, 119); break;
            }
            continue;
        }

        if (st < 0x175)
        {
            // ---- the left channel's envelope, its LFO, and its first two taps ----
            switch (st)
            {
            case 0x16f:
                read_shape(c, st, AT_INDEX);       // the slot 0x5b at the index t[9] loaded
                take_lfo(c, 11, st);
                ramp(c, st, 1);
                break;
            case 0x170: decay(c, st, 12, 126, 124); break;
            case 0x171: add_reg(c, st, 127); break;
            case 0x172: abs_term(c, st, 2); break;
            case 0x173:
                read_shape(c, st, AT_INDEX_OLDER);  // the slot 0x5c, one word further on
                position(c, st, 1, 11);
                break;
            default:
                take_capture(c, 56, st);
                decay(c, st, 12, 124, 124);
                break;
            }
            continue;
        }

        if (st < 0x17c)
        {
            // ---- the left channel's input path, seven steps from r[5] into its delay line ----
            Chain const& ch = CHAINS[0];
            input_chain(c, st, ch, st - ch.base);
            continue;
        }

        if (st < 0x185)
        {
            // ---- the left channel's feedback, its second pair and its third delay position ----
            if (st < 0x17f)
            {
                switch (st)
                {
                case 0x17c: take_capture(c, 60, st); scale(c, st, 126); break;
                case 0x17d: sum(c, st, 124); break;
                default:    ramp(c, st, 126); break;
                }
                continue;
            }
            if (st < 0x181) { tap_pair(c, st, TAPS[0]); continue; }
            if (st < 0x183)
            {
                if (st == 0x181) abs_term(c, st, 127);
                else              position(c, st, 126, 10);
                continue;
            }
            tap_pair(c, st, TAPS[1]);
            continue;
        }

        if (st < 0x18c)
        {
            // ---- the right channel's input path, the mirror of the left one ----
            Chain const& ch = CHAINS[1];
            input_chain(c, st, ch, st - ch.base);
            continue;
        }

        if (st < 0x191)
        {
            // ---- the right channel's feedback, its LFO and its two third taps ----
            switch (st)
            {
            case 0x18c: take_capture(c, 58, st); term(c, st, 119); break;
            case 0x18d: sum(c, st, 105); break;
            case 0x18e:
            {
                // The left delay line's value is latched here, thirteen steps before 0x19b writes it.
                double const y = run(c, {st, 108, 0, M2_REG, false, 0, ADD_ACC, false, ROUTE_1, CLAMP_SAT, 0});
                c.wlatch = yss236_core::clip(y, yss236_core::SAT);
                c.haswl = true;
                break;
            }
            case 0x18f:
                read_shape(c, st, AT_RING);        // the slot 0x63, the block's fourth published read
                take_lfo(c, 12, st);
                tap_ramp(c, st, TAPS[2].pos, TAPS[2].tap);
                break;
            default:
                take_capture(c, TAPS[2].take_second, st);
                tap_interp(c, st, TAPS[2].pos, TAPS[2].tap + 1, TAPS[2].mix);
                break;
            }
            continue;
        }

        // ---- the tail: the mix units, the last tap pair, the outputs and the delay line writes ----
        switch (st)
        {
        case 0x191: case 0x192: case 0x193:
        case 0x198: case 0x199: case 0x19a:
            mix_step(c, st, MIXES[st < 0x198 ? 0 : 1]);
            break;
        case 0x194: take_capture(c, 31, st); tap_ramp(c, st, TAPS[3].pos, TAPS[3].tap); break;
        case 0x195: tap_interp(c, st, TAPS[3].pos, TAPS[3].tap + 1, TAPS[3].mix); break;
        case 0x196: case 0x197:
        case 0x19c: case 0x19d:
            output_step(c, st, OUTPUTS[st < 0x19c ? 0 : 1]);
            break;
        case 0x19b: write_left_delay(c, st); break;
        case 0x19e: term(c, st, 108); break;             // the right delay line's own seeding step
        default:    write_right_delay(c, st); break;     // 0x19f
        }
    }
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m00(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
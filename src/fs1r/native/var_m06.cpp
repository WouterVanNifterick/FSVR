// fs1r/native/var_m06.cpp - VOP3-2, the variation window's program for Ens Detune: its 72 steps.
//
// rom::VARIATION_PROGRAMS[6], the program the window holds for the one variation type 7, and one of the
// twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 6 to
// Ens Detune and to nothing else.
//
//   DETUNE_DET          {0x164, 0x16C, 0x173, 0x17B} the four detune words, each one a delay index
//   DETUNE_LOW_SHELF    {0x168, 0x167, 0x166}         the left channel's low shelf: b0, b1, -a1
//   DETUNE_HIGH_SHELF   {0x16B, 0x16A, 0x169}         the left channel's high shelf, the same three
//   DETUNE_LOW_SHELF_COPY  {0x177, 0x176, 0x175}      the right channel's low shelf
//   DETUNE_HIGH_SHELF_COPY {0x17A, 0x179, 0x178}      the right channel's high shelf
//   DETUNE_PAN          {0x195, 0x19B, 0x196, 0x19C}  the dry and wet pairs, left then right
//   DETUNE_SLOTS        {0x66, 0x59, 0x5A, 0x5B, 0x5C, 0x67, 0x5D, 0x5E, 0x5F, 0x60}
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, each b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and one
// of the output, the low shelf on a step that doubles its result and the high shelf on one that
// quadruples it. What comes out of them is what goes into the two delay lines.
//
// fx_modulation.cpp's ensembleDetune() writes DETUNE_DET[0] and DETUNE_DET[1] with one word and
// DETUNE_DET[2] and DETUNE_DET[3] with another, both off rom::DETUNE[], so each channel's two taps share
// one word; and DETUNE_SLOTS[0] and [5] are where each channel writes, [1] to [4] and [6] to [9] the four
// slots it reads, one delay value each for all four, so within a tap pair the two words are one sample
// apart and the 'older' bit on the second read is what makes it so. INIT_DELAY_L and INIT_DELAY_R set
// those four offsets to the base plus one plus the parameter, which puts the whole line that many
// samples behind the write.
//
// The 72 steps, in the groups the comments in the switch open:
//
//   0x158-0x163  the two LFO cells into six registers, and the three oldest delay words published.
//                m[2] and m[6] take the serial inputs for the base program; the class-3 steps 0x158 and
//                0x159 put m[8] and m[9] doubled into r[123] and r[126], 0x15d and 0x162 take their
//                absolute values into r[124] and r[1], and the four class-1 loads leave the two base
//                delays r[125] and r[2] at max(0, 64 + the LFO) and the four crossfade weights
//                max(0, |2 * the LFO| - 24) and max(0, |2 * the LFO| - 40) in r[123], r[124] on the
//                left and r[126], r[1] on the right, the same two on both sides. 0x15b, 0x15f and 0x163
//                read the slots 0x56, 0x57 and 0x58 at the ring pointer, and 0x160, 0x164 and 0x168
//                publish them into m[22], m[23] and m[24].
//   0x164        DETUNE_DET[0]: the left tap pair's index. The result is the word times m[8], and
//                t[10] takes its fraction and the delay index its floor, so the index is in samples.
//   0x165-0x16b  the left channel's input r[103] and its two order-1 shelves into r[104] and r[105]:
//                the low shelf is -a1, b1 and b0 and the high shelf the same three, on the words
//                DETUNE_LOW_SHELF and DETUNE_HIGH_SHELF. 0x167 and 0x16b also read the slots 0x59 and
//                0x5a, the first tap pair, at the index 0x164 loaded, the second one word older.
//   0x16c        DETUNE_DET[1]: the second left tap pair's index, the same word times r[125], which is
//                the first one's plus 64 of the LFO's units, so the two taps are 256 * w * 64 samples
//                apart, w the word both share. With the ROM's own 0.034943 that is 572 samples, which
//                is what the index holds with the LFO cell at zero.
//   0x16d        the right channel's second crossfade weight, max(0, |2 * m[9]| - 40), into r[1],
//                which 0x162 left at |2 * m[9]|; r[126] keeps the -24 form 0x163 left for the first.
//   0x16f-0x172  the four crossfade weights, each the register times one eighth and sixteen, into
//                t[9], t[11], t[13] and t[15]. 0x172 also publishes the LFO of its page into m[11].
//   0x173        DETUNE_DET[2]: the first right tap pair's index, the word times m[9], and the read of
//                the slot 0x5c on the same step is the second word of the first pair.
//   0x174-0x17a  the right channel's input r[106] and its two shelves into r[107] and r[108], the
//                three steps each on DETUNE_LOW_SHELF_COPY and DETUNE_HIGH_SHELF_COPY. 0x175 publishes
//                the LFO of its page into m[9]. 0x177 and 0x17b read the slots 0x5d and 0x5e, the first
//                right tap pair, at the index 0x173 loaded.
//   0x17b        DETUNE_DET[3]: the second right tap pair's index, the same word times r[2].
//   0x17c-0x17d  the first left tap: m[56] and m[57] are the slot 0x59 and 0x5a words one sample apart,
//                and t[10] is the fraction between them, so r[119] is that pair interpolated.
//   0x17f-0x180  the second left tap, the slots 0x5b and 0x5c on t[12], into r[121].
//   0x183-0x184  the left crossfade: r[119] takes t[9] of its own value and one less t[11] of r[121],
//                so the two taps are summed with the weights the LFO cells give.
//   0x186-0x187  the first right tap, the slots 0x5d and 0x5e on t[14], into r[120].
//   0x18a-0x18b  the second right tap, the slots 0x5f and 0x60 on t[7], into r[122].
//   0x18e-0x18f  the right crossfade: r[120] takes t[13] of its own value and one less t[15] of r[122].
//                0x18f also reads the slot 0x63 at the ring pointer, publishes the LFO of its page into
//                m[12], and 0x188 publishes that read into m[63].
//   0x191-0x194  the two copies. The ROM's own words are a near-unity and a zero, so r[121] ends up
//                r[119] less one part in 32768 and r[122] r[120] the same. They are copies because
//                0x19a writes r[119] again with what goes into the left line, so the left blend has to
//                be somewhere else by the time 0x196 reads it.
//   0x195-0x19f  the outputs and the two delay writes. DETUNE_PAN's four words: r[15] takes the dry word
//                on r[105] and the wet word on r[121], both quadrupled, and r[16] the same on r[108]
//                and r[122]. dryWet() gives the variation windows no dry and this type a wet word of
//                8186, so each output is 0.9993 of its tap. 0x19a latches that left output and 0x19b
//                writes it into the slot 0x66, the left line; 0x19d and 0x19e build the right one, the
//                ROM's zero words on r[119] and r[120] adding nothing to it, and 0x19f writes it into
//                the slot 0x67 itself, latching nothing.
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d write
// them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005). r[103] the left input,
// r[104] and r[105] its two shelves; r[106] the right input, r[107] and r[108] its two. r[123], r[124]
// and r[125] the left LFO's three, r[126], r[1] and r[2] the right's; r[119] and r[120] the two
// interpolated and crossfaded taps, r[121] and r[122] the copies the outputs read. The cells: m[2] and
// m[6] the serial inputs this block takes for the base program, m[8] and m[9] the window's two LFOs and
// the ones 0x172 and 0x175 publish as m[11] and m[12], m[22] to m[27] the six published delay reads,
// m[56] to m[63] the eight tap words, in pair order: left, left, right, right, left, left, right, right.
// The delay slots: 0x59 to 0x5c the left line's four words and 0x5d to 0x60 the right one's, all at the
// base the host set and the delay index added to the ring pointer; 0x66 and 0x67 are where each line is
// written. Each capture takes the read five steps before it (captured()'s lower bound), so m[56] is the
// slot 0x59 word, m[57] the slot 0x5a word, m[58] the slot 0x5d word, m[59] the slot 0x5e word, m[60] the
// slot 0x5b word, m[61] the slot 0x5c word, m[62] the slot 0x5f word and m[63] the slot 0x60 word, and
// m[22] to m[27] the slots 0x56, 0x57, 0x58, 0x64, 0x65 and 0x65.
//
// The eight t registers: t[10] and t[12] the two left tap positions, t[14] and t[7] the two right, each
// the fraction below one sample of the index its step queued; t[9] and t[11] the left crossfade weights
// and t[13] and t[15] the right ones, each a register divided by 16384. A t write divides by 128, so a
// fraction is the accumulator's 256th and a weight is twice its register over 32768.
//
// How far the taps sweep is the LFO cells' range: chips/vop3.cpp's lfo() scales the wave by the depth
// and multiplies by RANGE, so a full-depth bipolar saw fills m[8] and m[9] to +-128, and the check's
// run has m[8] at 127.96. A tap's index is then 256 * w * m[8] samples, the base delay r[125] one more
// 64 of the same, and the four weights 232/16384 and 216/16384 at their largest, so the second tap
// carries almost all of each channel's output.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m06: identical, 1 type, 9600 native passes.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
    double* const r = c.r;                 // the 128 registers
    double* const m = c.m;                 // the 64 cells, which carry the values between the steps
    double xv, k, g, m2, prod, a, y, v;

    // The step's x operand: the register r[sr] into the latch every step but a class-2/3 one moves, and
    // xv alongside it for the multiplier.
    auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };
    // The t file write of a step whose result is y, tfrac saying which of the two forms it takes.
    auto twr = [&](double yy, int tn, bool tfrac) {
        double const w = yy * 256.0;
        double const tv = tfrac ? (w - std::floor(w))
                                 : (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0;
        c.queue_t(tn, tv);
    };

    for (int st = 0x158; st < 0x1a0; st++)
    {
        c.begin_step();
        if (st < c.m_skip) continue;
        switch (st)
        {
// ---- 0x158 to 0x163, the two LFO cells and the three published reads --------------------------
// m[2] takes the left serial input; r[123] = wrap(2 * m[8]), the first LFO cell doubled
        case 0x158:
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            m2 = c.src;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(123, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[6] takes the right serial input; r[126] = wrap(2 * m[9]), the second LFO cell doubled
        case 0x159:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(0);
            c.psrc = c.src; c.src = m[9];
            m2 = c.src;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(126, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x15a:
            fetch(0);
            break;
// a read of the slot 0x56; the left base delay's half: acc = 0.5 * r[123], which is m[8]
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[125] = the class-1 load of the half, 64 + acc with negatives clamped away: the left base delay
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(125, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// r[124] = |r[123]|, the first LFO cell's absolute value, and the accumulator the next step reads
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 3));
            y = c.acc = yss236_core::accmode(y, 3);
            break;
// r[123] = the class-1 load of -24 + |2 * m[8]|, the first left crossfade weight before the factor
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(123, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// a read of the slot 0x57; the right base delay's half: acc = 0.5 * r[126], which is m[9]
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[22] takes up the read of 0x15b; r[2] = the class-1 load of 64 + m[9], the right base delay
        case 0x160:
            c.queue_d(22, c.captured(m[22], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(2, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// r[124] = the class-1 load of -40 + r[124], the second left crossfade weight before the factor
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(124, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// r[1] = |r[126]|, the second LFO cell's absolute value, and the accumulator the next step reads
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(1, yss236_core::mode(y, 3));
            y = c.acc = yss236_core::accmode(y, 3);
            break;
// a read of the slot 0x58; r[126] = the class-1 load of -24 + |2 * m[9]|, the first right weight
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(126, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// ---- 0x164, the first left tap pair's delay index ---------------------------------------------
// m[23] takes up the read of 0x15f; DETUNE_DET[0]: acc = k * m[8], its fraction into t[10] and its
// floor into the delay index
        case 0x164:
            c.queue_d(23, c.captured(m[23], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            m2 = c.src;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(y, 10, true);
            c.queue_i(std::floor(y * 256.0));
            break;
// ---- 0x165 to 0x16b, the left channel's input and its two shelves -----------------------------
// r[103] = saturate(2 * (0.5 * r[5])), the left input through the low shelf's b0 word
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// DETUNE_LOW_SHELF's -a1: acc = k * r[104], the left shelf's output a pass back
        case 0x166:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x59 at the delay index 0x164 loaded; DETUNE_LOW_SHELF's b1, plus -a1
        case 0x167:
            c.push_hist(st, c.ram[c.addr(0x59, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[24] takes up the read of 0x163; DETUNE_LOW_SHELF's b0: r[104] = saturate(2 * (k * r[103] + acc))
        case 0x168:
            c.queue_d(24, c.captured(m[24], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// DETUNE_HIGH_SHELF's -a1: acc = k * r[105]
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// DETUNE_HIGH_SHELF's b1: acc = k * r[104] + acc
        case 0x16a:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x5a one word older than 0x167; DETUNE_HIGH_SHELF's b0: r[105] = saturate(4 * (k * r[104] + acc))
        case 0x16b:
            c.push_hist(st, c.ram[c.addr(0x5a, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x16c, the second left tap pair's delay index --------------------------------------------
// m[56] takes up the read of 0x167; DETUNE_DET[1]: acc = k * r[125], its fraction into t[12] and its
// floor into the delay index
        case 0x16c:
            c.queue_d(56, c.captured(m[56], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(y, 12, true);
            c.queue_i(std::floor(y * 256.0));
            break;
// r[1] = the class-1 load of -40 + r[1], the second right crossfade weight over the first
        case 0x16d:
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(1, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
// the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x16e:
            fetch(0);
            break;
// ---- 0x16f to 0x172, the four crossfade weights -----------------------------------------------
// a read of the slot 0x5b at the delay index 0x16c loaded; t[9] = saturate(16 * (0.125 * r[123]))
        case 0x16f:
            c.push_hist(st, c.ram[c.addr(0x5b, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 9, false);
            break;
// m[57] takes up the read of 0x16b; t[11] = saturate(16 * (0.125 * r[124]))
        case 0x170:
            c.queue_d(57, c.captured(m[57], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 11, false);
            break;
// t[13] = saturate(16 * (0.125 * r[126])), the right channel's first weight, the -24 form
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 13, false);
            break;
// m[11] takes the LFO of this page; t[15] = saturate(16 * (0.125 * r[1])), the second right weight
        case 0x172:
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 15, false);
            break;
// ---- 0x173, the first right tap pair's delay index --------------------------------------------
// a read of the slot 0x5c one word older than 0x16f; DETUNE_DET[2]: acc = k * m[9], its fraction into
// t[14] and its floor into the delay index
        case 0x173:
            c.push_hist(st, c.ram[c.addr(0x5c, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(0);
            c.psrc = c.src; c.src = m[9];
            m2 = c.src;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(y, 14, true);
            c.queue_i(std::floor(y * 256.0));
            break;
// ---- 0x174 to 0x17a, the right channel's input and its two shelves ----------------------------
// m[60] takes up the read of 0x16f; r[106] = saturate(2 * (0.5 * r[6])), the right input
        case 0x174:
            c.queue_d(60, c.captured(m[60], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[9] takes the LFO of this page; DETUNE_LOW_SHELF_COPY's -a1: acc = k * r[107]
        case 0x175:
            c.queue_d(9, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// DETUNE_LOW_SHELF_COPY's b1: acc = k * r[106] + acc
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x5d at the delay index 0x173 loaded; DETUNE_LOW_SHELF_COPY's b0: r[107] = saturate(2 * (k * r[106] + acc))
        case 0x177:
            c.push_hist(st, c.ram[c.addr(0x5d, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[61] takes up the read of 0x173; DETUNE_HIGH_SHELF_COPY's -a1: acc = k * r[108]
        case 0x178:
            c.queue_d(61, c.captured(m[61], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// DETUNE_HIGH_SHELF_COPY's b1: acc = k * r[107] + acc
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// DETUNE_HIGH_SHELF_COPY's b0: r[108] = saturate(4 * (k * r[107] + acc)), the right EQ output
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x17b, the second right tap pair's delay index -------------------------------------------
// a read of the slot 0x5e one word older than 0x177; DETUNE_DET[3]: acc = k * r[2], its fraction into
// t[7] and its floor into the delay index
        case 0x17b:
            c.push_hist(st, c.ram[c.addr(0x5e, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(y, 7, true);
            c.queue_i(std::floor(y * 256.0));
            break;
// ---- 0x17c to 0x17d, the first left tap interpolated -----------------------------------------
// m[58] takes up the read of 0x177; acc = t[10] * m[56] - m[56], the first tap word less the fraction
        case 0x17c:
            c.queue_d(58, c.captured(m[58], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[119] = saturate(t[10] * m[57] - acc), the pair interpolated at t[10]
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x17e:
            fetch(0);
            break;
// ---- 0x17f to 0x180, the second left tap interpolated ----------------------------------------
// a read of the slot 0x5f at the delay index 0x17b loaded; acc = t[12] * m[60] - m[60]
        case 0x17f:
            c.push_hist(st, c.ram[c.addr(0x5f, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            m2 = c.src;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[59] takes up the read of 0x17b; r[121] = saturate(t[12] * m[61] - acc), that pair interpolated
        case 0x180:
            c.queue_d(59, c.captured(m[59], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x181:
            fetch(0);
            break;
        case 0x182:
            fetch(0);
            break;
// ---- 0x183 to 0x184, the left crossfade ------------------------------------------------------
// a read of the slot 0x60 one word older than 0x17f; acc = t[11] * r[121] - r[121], the second tap
        case 0x183:
            c.push_hist(st, c.ram[c.addr(0x60, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.x;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[62] takes up the read of 0x17f; r[119] = saturate(t[9] * r[119] - acc), the two taps crossfaded
        case 0x184:
            c.queue_d(62, c.captured(m[62], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x185:
            fetch(0);
            break;
// ---- 0x186 to 0x187, the first right tap interpolated ----------------------------------------
// acc = t[14] * m[58] - m[58], the first right tap word less the fraction
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[14];
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[120] = saturate(t[14] * m[59] - acc), that pair interpolated at t[14]
        case 0x187:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[14];
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[63] takes up the read of 0x183; the idle step otherwise: class 0, no RAM mark and no ALU
        case 0x188:
            c.queue_d(63, c.captured(m[63], st));
            fetch(0);
            break;
        case 0x189:
            fetch(0);
            break;
// ---- 0x18a to 0x18b, the second right tap interpolated ---------------------------------------
// acc = t[7] * m[62] - m[62], the second right tap word less the fraction
        case 0x18a:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[7];
            fetch(0);
            c.psrc = c.src; c.src = m[62];
            m2 = c.src;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[122] = saturate(t[7] * m[63] - acc), that pair interpolated at t[7]
        case 0x18b:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[7];
            fetch(0);
            c.psrc = c.src; c.src = m[63];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18c:
            fetch(0);
            break;
        case 0x18d:
            fetch(0);
            break;
// ---- 0x18e to 0x18f, the right crossfade -----------------------------------------------------
// acc = t[15] * r[122] - r[122], the second right tap
        case 0x18e:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[15];
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.x;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x63 at the ring pointer, and m[12] takes the LFO of this page;
// r[120] = saturate(t[13] * r[120] - acc), the two right taps crossfaded
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[13];
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x190:
            fetch(0);
            break;
// ---- 0x191 to 0x194, the two copies -----------------------------------------------------------
// acc = the near-unity word on r[119], the first tap, a pass back
        case 0x191:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[121] = saturate(acc): the ROM's zero kills the r[120] this step would add, so the copy of r[119]
        case 0x192:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// a read of the slot 0x64 at the ring pointer; acc = the ROM's zero on r[119]
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[25] takes up the read of 0x193; r[122] = saturate(the near-unity word on r[120] + acc), the copy
        case 0x194:
            c.queue_d(25, c.captured(m[25], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x195 to 0x19f, the outputs and the two delay writes ------------------------------------
// DETUNE_PAN's dry word on the left: acc = k * r[105], the left EQ output
        case 0x195:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// DETUNE_PAN's wet word: r[15] = saturate(4 * (k * r[121] + acc)), the left output
        case 0x196:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// a read of the slot 0x65 at the ring pointer; DETUNE_PAN's second dry word: acc = k * r[105]
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[26] takes up the read of 0x197; the ROM's zero on r[108] over that accumulator
        case 0x198:
            c.queue_d(26, c.captured(m[26], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the ROM's zero on r[120] over the same accumulator, the right line's input
        case 0x199:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[119] = saturate(acc) and the result is latched as the data of the next delay write
        case 0x19a:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            c.wlatch = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = true;
            break;
// DETUNE_PAN's second dry word on the right, and the slot 0x66 write takes the latched left output
        case 0x19b:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            c.ram[c.addr(0x66, 0, false)] = c.wlatch;
            c.haswl = false;
            break;
// m[27] takes up the read of 0x197; DETUNE_PAN's fourth word: r[16] = saturate(4 * (k * r[122] + acc))
        case 0x19c:
            c.queue_d(27, c.captured(m[27], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the near-unity word on r[108], the right EQ output
        case 0x19d:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the ROM's zero on r[119] over that accumulator, the right line's input
        case 0x19e:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the slot 0x67 write takes this step's own result, and its wdata bit latches nothing
        case 0x19f:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.haswl = false;
            c.ram[c.addr(0x67, 0, false)] = yss236_core::clip(y, yss236_core::SAT);
            break;
        }
    }
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m06(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
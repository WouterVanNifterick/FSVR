// fs1r/native/var_m01.cpp - VOP3-2, the variation window's program for Rotary SP, Tremolo and Auto
// Pan: its 72 steps.
//
// rom::VARIATION_PROGRAMS[1], the program the window holds for the three variation types 8 to 10, and the
// only program among the twelve that these three share. Under each of them the same program words run and
// only the constants (rom::var's word lists, the LFO depths and the feedback level) and the eight delay
// offsets (rom::var::MOD_SLOTS) move, so this native is one function for all three and no type is
// excluded. Which of the three is running is not a code difference here; the host's parameters are.
//
// What the 72 steps compute, in the groups the comments open:
//
//   0x158-0x15a  the two serial inputs this window publishes, m[2] and m[6], and the three registers the
//                rest of the block works from, r[123], r[125] and r[1], each taken off the shared cell
//                m[8] and doubled.
//   0x15b-0x166  the three decay chains those three feed. Each folds a published slot's read through
//                |r|, then a saturating doubling that writes t[9], t[11] or t[13], then two multiplies
//                into r[124], r[126] and r[2]. m[28], m[29] and m[30] carry the three reads on.
//   0x167-0x16e  the fractional delay positions: t[15] and t[7] off the cell m[9], the first with an
//                index load of its own, and the two modulated positions r[123] and r[1], each clamped at
//                zero, which the taps below read at.
//   0x16f-0x17e  the left channel. The first tap (the slot 0x5b, read past the ring pointer) and the LFO
//                publish m[11] with t[9]; the second tap's capture m[62] and t[10] with its index load;
//                the input path r[5] to r[103], r[104] and r[105] against the taps 0x5c and 0x5d, which
//                are the two- and four-sample combs the delay line is written through; the captures m[56]
//                and m[57] with t[14] and t[12]; and the right position r[125] clamped at zero.
//   0x17f-0x185  the right channel, the mirror of the left one: the input path r[6] to r[106], r[107] and
//                r[108], the taps 0x5f and 0x60, the captures m[60], m[61] and m[58], and the tap read
//                0x61.
//   0x186-0x18e  the four interpolations, each written as its fraction times the second tap less the
//                fraction times the first, so m[56] and m[57] on t[10] make r[119], m[60] and m[61] on
//                t[14] make r[121] and m[62] and m[63] on t[7] make r[122]; then the three of them scaled
//                by (1 + t[9]), (1 + t[13]) and (1 + t[15]), which is where this block differs from the
//                chorus program's and what makes the amplitude and pitch move together.
//   0x18f-0x197  the second interpolation off the pair m[58] and m[59] on t[12] into r[120], its scaling by
//                (1 + t[11]), and the two output sums that add the three scaled values to the running
//                one.
//   0x198-0x19f  the two outputs r[15] and r[16], the captures m[32] and m[33] of the reads at 0x64 and
//                0x65, the two delay line writes into the slots 0x66 and 0x67, and the idle step 0x19e
//                between them.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs, r[103] and r[106]
// the scaled inputs, r[104], r[105], r[107] and r[108] the input accumulators, r[15] and r[16] the two
// outputs, which the base program reads at 0x018 and 0x03b as the later stereo pair after r[13] and
// r[14], r[119] to r[122] the four interpolated tap sums, r[123], r[125] and r[1] the three followers
// that end as two delay positions, r[124], r[126] and r[2] the products between them. The cells: m[8]
// the shared cell, m[2] and m[6] the serial inputs this window publishes, m[9] the cell the t[15] and
// t[7] positions scale off, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six
// published reads and m[56] to m[63] the eight taps. The delay slots: 0x66 and 0x67 the two delay lines,
// 0x5a to 0x61 the eight taps (rom::var::MOD_SLOTS), 0x56 to 0x58 and 0x63 to 0x65 the six reads it hands
// on.
//
// The t registers: t[7], t[10], t[12] and t[14] the four fractional tap positions, t[9], t[11], t[13] and
// t[15] the decay and scaling coefficients. Every one of the eight is written from a running value the
// step itself produced, so they move with the block and not with a parameter directly.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half
// skips these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
    double* const r = c.r;                 // the 128 registers
    double* const m = c.m;                 // the 64 cells, which carry the values between the steps
    double xv, k, g, m2, prod, a, y, v, outv;

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
        // ---- 0x158 to 0x15a, the published serial inputs and the three followers --------------------
        case 0x158:  // m[2] takes serial bank 0 page 10; r[123] = 2 * m[8]
            c.queue_d(2, c.in[0][10]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(123, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x159:  // m[6] takes serial bank 1 page 10; r[125] = 2 * m[8]
            c.queue_d(6, c.in[1][10]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(125, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x15a:  // r[1] = 2 * m[8]
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(1, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;

        // ---- 0x15b to 0x166, the three decay chains -----------------------------------------------
        case 0x15b:  // a read of the slot 0x56; acc = |r[123]|
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x15c:  // r[123] = saturate(2 * (acc + 128 * k)); t[9] = the running value as a 1.15 word
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(123, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            twr(c.acc, 9, false);
            break;
        case 0x15d:  // acc = |r[125]|
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x15e:  // r[125] = saturate(2 * (acc + 128 * k)); t[11] = the running value as a 1.15 word
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(125, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            twr(c.acc, 11, false);
            break;
        case 0x15f:  // a read of the slot 0x57; acc = |r[1]|
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x160:  // m[28] takes up the read of 0x15b; r[1] = saturate(2 * (acc + 128 * k)); t[13]
            c.queue_d(28, c.captured(m[28], st));
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(1, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            twr(c.acc, 13, false);
            break;
        case 0x161:  // r[124] = saturate(t[9] * r[123])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x162:  // r[126] = saturate(t[11] * r[125])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(126, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x163:  // a read of the slot 0x58; r[2] = saturate(t[13] * r[1])
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[13];
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(2, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x164:  // m[29] takes up the read of 0x15f; r[124] = saturate(t[9] * r[124])
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x165:  // r[126] = saturate(t[11] * r[126])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(126, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x166:  // r[2] = saturate(t[13] * r[2])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[13];
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(2, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x167 to 0x16e, the fractional delay positions ----------------------------------------
        case 0x167:  // t[15] = m[9] * k as a 1.15 word
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(0);
            c.psrc = c.src; c.src = m[9];
            m2 = c.src; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 15, false);
            break;
        case 0x168:  // m[30] takes up the read of 0x163; t[7] = its fraction, and the delay index
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(0);
            c.psrc = c.src; c.src = m[9];
            m2 = c.src; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 7, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x169:  // acc = 16 * k * r[123]
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16a:  // acc = k * r[124] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x16b:  // a read of the slot 0x5a at the delay index; r[123] = the running value, negatives clamped away
            c.push_hist(st, c.ram[c.addr(0x5a, c.cur_idx(), false)]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(123, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16c:  // acc = 16 * k * r[1]
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16d:  // acc = k * r[2] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x16e:  // r[1] = the running value, negatives clamped away
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(1, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;

        // ---- 0x16f to 0x17e, the left channel ------------------------------------------------------
        case 0x16f:  // a read of the slot 0x5b one word older at the delay index;
                     // m[11] takes the LFO 11; t[9] = the running value as a 1.15 word
            c.push_hist(st, c.ram[c.addr(0x5b, 1 + c.cur_idx(), false)]);
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 9, false);
            break;
        case 0x170:  // m[62] takes up the read of 0x16b; t[10] = its fraction, and the delay index
            c.queue_d(62, c.captured(m[62], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 10, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x171:  // r[103] = saturate(2 * k * r[5]), the scaled left input
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x172:  // nothing but the reading of r[104]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x173:  // a read of the slot 0x5c at the delay index; acc = k * r[103] + the running value
            c.push_hist(st, c.ram[c.addr(0x5c, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x174:  // r[104] = saturate(2 * (k * r[103] + acc)), the two-sample comb
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x175:  // m[63] takes up the read of 0x173; nothing but the reading of r[105]
            c.queue_d(63, c.captured(m[63], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x176:  // acc = k * r[104] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x177:  // a read of the slot 0x5d one word older at the delay index;
                     // r[105] = saturate(4 * (k * r[104] + acc)), the four-sample comb
            c.push_hist(st, c.ram[c.addr(0x5d, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x178:  // m[56] takes up the read of 0x173; t[14] = its fraction, and the delay index
            c.queue_d(56, c.captured(m[56], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 14, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x179:  // acc = 16 * k * r[125]
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x17a:  // acc = k * r[126] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17b:  // a read of the slot 0x5e at the delay index; r[125] = the running value, negatives clamped away
            c.push_hist(st, c.ram[c.addr(0x5e, c.cur_idx(), false)]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(125, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x17c:  // m[57] takes up the read of 0x177; t[13] = the running value as a 1.15 word
            c.queue_d(57, c.captured(m[57], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 13, false);
            break;
        case 0x17d:  // t[11] = the running value as a 1.15 word
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 11, false);
            break;
        case 0x17e:  // t[12] = the running value's fraction, and the delay index
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(c.acc, 12, true);
            c.queue_i(std::floor(y * 256.0));
            break;

        // ---- 0x17f to 0x185, the right channel -----------------------------------------------------
        case 0x17f:  // a read of the slot 0x5f one word older at the delay index;
                     // r[106] = saturate(2 * k * r[6]), the scaled right input
            c.push_hist(st, c.ram[c.addr(0x5f, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x180:  // m[60] takes up the read of 0x17f; nothing but the reading of r[107]
            c.queue_d(60, c.captured(m[60], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x181:  // acc = k * r[106] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x182:  // r[107] = saturate(2 * (k * r[106] + acc)), the two-sample comb
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x183:  // a read of the slot 0x60 at the delay index; nothing but the reading of r[108]
            c.push_hist(st, c.ram[c.addr(0x60, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x184:  // m[61] takes up the read of 0x183; acc = k * r[107] + the running value
            c.queue_d(61, c.captured(m[61], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x185:  // r[108] = saturate(4 * (k * r[107] + acc)), the four-sample comb
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x186 to 0x18e, the four interpolations and the three scalings ------------------------
        case 0x186:  // acc = m[56] * (t[10] - 1), the left tap pair's first half
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x187:  // a read of the slot 0x61 one word older at the delay index;
                     // r[119] = saturate(t[10] * m[57] - acc), the left pair interpolated
            c.push_hist(st, c.ram[c.addr(0x61, 1 + c.cur_idx(), false)]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x188:  // m[58] takes up the read of 0x17b; acc = m[60] * (t[14] - 1)
            c.queue_d(58, c.captured(m[58], st));
            g = c.t[14];
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x189:  // r[121] = saturate(t[14] * m[61] - acc), the m[60] pair interpolated
            g = c.t[14];
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18a:  // acc = m[62] * (t[7] - 1)
            g = c.t[7];
            fetch(0);
            c.psrc = c.src; c.src = m[62];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18b:  // r[122] = saturate(t[7] * m[63] - acc), the m[62] pair interpolated
            g = c.t[7];
            fetch(0);
            c.psrc = c.src; c.src = m[63];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18c:  // m[59] takes up the read of 0x183; r[119] = saturate(r[119] * (1 + t[9]))
            c.queue_d(59, c.captured(m[59], st));
            g = c.t[9];
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.x;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18d:  // r[121] = saturate(r[121] * (1 + t[13]))
            g = c.t[13];
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.x;
            y = (prod + a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18e:  // r[122] = saturate(r[122] * (1 + t[15]))
            g = c.t[15];
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.x;
            y = (prod + a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x18f to 0x197, the second interpolation, its scaling and the two output sums ----------
        case 0x18f:  // a read of the slot 0x63; m[12] takes the LFO 12; acc = m[58] * (t[12] - 1)
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            g = c.t[12];
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x190:  // r[120] = saturate(t[12] * m[59] - acc), the m[58] pair interpolated
            g = c.t[12];
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x191:  // nothing but the reading of r[119]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x192:  // acc = k * r[121] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x193:  // a read of the slot 0x64; r[119] = saturate(k * r[122] + acc)
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x194:  // m[31] takes up the read of 0x18f; r[120] = saturate(r[120] * (1 + t[11]))
            c.queue_d(31, c.captured(m[31], st));
            g = c.t[11];
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.x;
            y = (prod + a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x195:  // nothing but the reading of r[121]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x196:  // acc = k * r[122] + the running value
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x197:  // a read of the slot 0x65; r[120] = saturate(k * r[120] + acc)
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x198 to 0x19f, the two outputs and the two delay line writes -------------------------
        case 0x198:  // m[32] takes up the read of 0x193; nothing but the reading of r[105]
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x199:  // r[15] = saturate(4 * (k * r[119] + acc)), the left output
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x19a:  // nothing but the reading of r[105]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x19b:  // the left delay line: its own clipped result into the slot 0x66
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x66, 0, false)] = outv;
            break;
        case 0x19c:  // m[33] takes up the read of 0x197; nothing but the reading of r[108]
            c.queue_d(33, c.captured(m[33], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x19d:  // r[16] = saturate(4 * (k * r[120] + acc)), the right output
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x19e:  // the idle step between the two writes: class 0, no RAM mark and no ALU,
                     // so only the x latch every class but 2 and 3 still runs
            fetch(0);
            break;
        case 0x19f:  // the right delay line: its own clipped result into the slot 0x67
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x67, 0, false)] = outv;
            break;
        }
    }
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m01(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
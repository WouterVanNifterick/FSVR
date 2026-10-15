// fs1r/native/var_m00.cpp - VOP3-2, the variation window's program for No Effect, Chorus, Celeste,
// Flanger and Symphonic: its 72 steps.
//
// rom::VARIATION_PROGRAMS[0], the program the window holds for the five variation types 0 to 4, and the
// only program among the twelve that these five share. Under each of them the same program words run
// and only the constants (rom::var's word lists, the LFO depths and the feedback level) and the eight
// delay offsets (rom::var::MOD_SLOTS) move, so this native is one function for all five and no type is
// excluded. Which of the five is running is not a code difference here; the host's parameters are.
//
// What the 72 steps compute, in the groups the comments open:
//
//   0x158-0x15a  the three registers the block's pan balance works from: r[124], r[123] and r[125] off
//                the shared cell m[8], and the two serial inputs published as m[2] and m[6].
//   0x15b-0x163  the first three reads of the block's own area (the slots 0x56, 0x57 and 0x58) and the
//                envelope network: r[124], r[126], r[127], r[1] and r[2] decay and ramp against the
//                coefficients t[12] and t[13], and the two of them the block reads back.
//   0x164-0x16e  the m[29] and m[30] captures, two more envelope steps, and the first modulated delay
//                position: 0x16c takes r[124]'s running value, clamps it, writes its fraction to t[9] and
//                loads it as the delay index the taps below read at.
//   0x16f-0x184  the left channel: the four taps of the left delay line (the slots 0x5b, 0x5c, 0x5d and
//                0x5e), each read at the modulated index and captured five steps later into m[56], m[57],
//                m[60] and m[61]; the input path r[5] to r[103], r[104] and r[105]; and the two pairwise
//                tap sums in r[119] and r[121], each t[9] or t[11] against the cell beside it.
//   0x185-0x19f  the right channel, the mirror of the left one: the input path r[6] to r[106], r[107]
//                and r[108], the four taps of the right delay line read in the order 0x5f, 0x60, 0x61,
//                0x62 into m[62], m[63], m[58] and m[59], the two pairwise sums in r[122] and r[120],
//                the two outputs r[15] and r[16], the three published reads of the slots 0x63, 0x64
//                and 0x65 into m[31], m[32] and m[33], and the two delay line writes, the left one from
//                the write latch set at 0x18e and taken at 0x19b, the right one written directly at
//                0x19f.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs, r[103], r[106] the
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
        // ---- 0x158 to 0x15a, the balance registers and the published serial inputs -----------------------
        case 0x158:  // m[2] takes serial bank 0 page 10; r[124] = 2 * m[8]
            c.queue_d(2, c.in[0][10]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(124, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x159:  // m[6] takes serial bank 1 page 10; r[123] = m[8] through the word's own 16x
            c.queue_d(6, c.in[1][10]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            k = yss236_core::s16(c.coef[st]);
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(123, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x15a:  // r[125] = 2 * the running value
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(125, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;

        // ---- 0x15b to 0x163, the first three published reads and the envelope network -------------------
        case 0x15b:  // a read of the slot 0x56; acc = |r[124]|
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x15c:  // r[124] = saturate(2 * (acc - 0.5)); t[12] = the running value as a 1.15 word
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(124, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            twr(c.acc, 12, false);
            break;
        case 0x15d:  // r[1] = 2 * m[8]
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(1, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x15e:  // r[2] = 2 * r[123]
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(2, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x15f:  // a read of the slot 0x57; r[127] = saturate(t[12] * r[124])
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(127, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x160:  // m[28] takes up the read of 0x15b; acc = |r[1]|
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x161:  // r[1] = saturate(2 * (acc - 0.5)); t[13] = the running value as a 1.15 word
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
        case 0x162:  // r[127] = saturate(t[12] * r[127])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(127);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(127, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x163:  // a read of the slot 0x58; r[126] = 2 * m[8]
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(126, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;

        // ---- 0x164 to 0x16e, two more captures and the first delay position ---------------------------
        case 0x164:  // m[29] takes up the read of 0x15f; acc = r[124] / 2
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x165:  // the running value passes through, reading r[127]
            k = yss236_core::s16(c.coef[st]);
            fetch(127);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x166:  // r[124] = the running value plus 0.5, negatives clamped away
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(124, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x167:  // r[119] = saturate(t[13] * r[1])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[13];
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x168:  // m[30] takes up the read of 0x163; acc = |r[126]|
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x169:  // r[126] = saturate(2 * (acc - 0.5)); t[12] = the running value as a 1.15 word
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(126, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            twr(c.acc, 12, false);
            break;
        case 0x16a:  // r[119] = saturate(t[13] * r[119])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[13];
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16b:  // the running value is cleared: the coefficient is zero, so r[125] only feeds the latch
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x16c:  // acc = r[124]; t[9] = its fraction, and the delay index
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(c.acc, 9, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x16d:  // acc = r[1] / 2
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16e:  // the running value passes through, reading r[119]
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;

        // ---- 0x16f to 0x184, the left channel ---------------------------------------------------------
        case 0x16f:  // a read of the slot 0x5b at the delay index; m[11] takes the LFO 11; r[1] = acc + 0.5
            c.push_hist(st, c.ram[c.addr(0x5b, c.cur_idx(), false)]);
            c.queue_d(11, c.lfo[11]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(1, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x170:  // r[124] = saturate(t[12] * r[126])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x171:  // r[127] = 2 * r[123]
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 2.0;
            c.queue_r(127, yss236_core::mode(v, 0));
            c.acc = yss236_core::accmode(v, 0);
            break;
        case 0x172:  // acc = |r[2]|
            k = yss236_core::s16(c.coef[st]);
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x173:  // a read of the slot 0x5c one word older at the delay index; t[11] = acc's fraction, and the index
            c.push_hist(st, c.ram[c.addr(0x5c, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(c.acc, 11, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x174:  // m[56] takes up the read of 0x16f; r[124] = saturate(t[12] * r[124])
            c.queue_d(56, c.captured(m[56], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[12];
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x175:  // r[103] = saturate(r[5]), the scaled left input
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x176:  // nothing but the reading of r[104]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x177:  // a read of the slot 0x5d at the delay index; the running value passes through
            c.push_hist(st, c.ram[c.addr(0x5d, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x178:  // m[57] takes up the read of 0x173; r[104] = saturate(r[103] + 2 * acc)
            c.queue_d(57, c.captured(m[57], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x179:  // nothing but the reading of r[105]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17a:  // the running value passes through, reading r[104]
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17b:  // a read of the slot 0x5e one word older at the delay index; r[105] = saturate(r[104] + 4 * acc)
            c.push_hist(st, c.ram[c.addr(0x5e, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x17c:  // m[60] takes up the read of 0x177; acc = r[126] / 2
            c.queue_d(60, c.captured(m[60], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x17d:  // the running value passes through, reading r[124]
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17e:  // r[126] = the running value plus 0.5, negatives clamped away
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            k = yss236_core::s16(c.coef[st]);
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(126, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x17f:  // a read of the slot 0x5f at the delay index; acc = m[56] * (t[9] - 1)
            c.push_hist(st, c.ram[c.addr(0x5f, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x180:  // m[61] takes up the read of 0x17b; r[119] = saturate(t[9] * m[57] - acc)
            c.queue_d(61, c.captured(m[61], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x181:  // the running value is cleared; r[127] only feeds the latch
            k = yss236_core::s16(c.coef[st]);
            fetch(127);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        case 0x182:  // acc = r[126], negatives clamped away; t[10] = its fraction, and the delay index
            k = yss236_core::s16(c.coef[st]);
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            twr(c.acc, 10, true);
            c.queue_i(std::floor(y * 256.0));
            break;
        case 0x183:  // a read of the slot 0x60 one word older at the delay index; acc = m[60] * (t[11] - 1)
            c.push_hist(st, c.ram[c.addr(0x60, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x184:  // m[62] takes up the read of 0x17f; r[121] = saturate(t[11] * m[61] - acc)
            c.queue_d(62, c.captured(m[62], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x185 to 0x19f, the right channel, the outputs and the two delay line writes --------------
        case 0x185:  // r[106] = saturate(r[6]), the scaled right input
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x186:  // nothing but the reading of r[107]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x187:  // a read of the slot 0x61 at the delay index; the running value passes through
            c.push_hist(st, c.ram[c.addr(0x61, c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x188:  // m[63] takes up the read of 0x183; r[107] = saturate(r[106] + 2 * acc)
            c.queue_d(63, c.captured(m[63], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x189:  // nothing but the reading of r[108]; the running value is cleared
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18a:  // the running value passes through, reading r[107]
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18b:  // a read of the slot 0x62 one word older at the delay index; r[108] = saturate(r[107] + 4 * acc)
            c.push_hist(st, c.ram[c.addr(0x62, 1 + c.cur_idx(), false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18c:  // m[58] takes up the read of 0x187; the running value is cleared
            c.queue_d(58, c.captured(m[58], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18d:  // the running value passes through, reading r[105]
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18e:  // the left delay line's value is latched for the write at 0x19b
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.wlatch = outv; c.haswl = true;
            break;
        case 0x18f:  // a read of the slot 0x63; m[12] takes the LFO 12; acc = m[62] * (t[11] - 1)
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(0);
            c.psrc = c.src; c.src = m[62];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x190:  // m[59] takes up the read of 0x18b; r[122] = saturate(t[11] * m[63] - acc)
            c.queue_d(59, c.captured(m[59], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(0);
            c.psrc = c.src; c.src = m[63];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x191:  // the running value is cleared; r[119] only feeds the latch
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x192:  // the running value passes through, reading r[121]
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x193:  // a read of the slot 0x64; r[124] = saturate(acc)
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x194:  // m[31] takes up the read of 0x18f; acc = m[58] * (t[10] - 1)
            c.queue_d(31, c.captured(m[31], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src; prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x195:  // r[120] = saturate(t[10] * m[59] - acc)
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            m2 = c.src; prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x196:  // acc = r[105] / 4
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x197:  // a read of the slot 0x65; r[15] = saturate(4 * acc), the left output
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x198:  // m[32] takes up the read of 0x193; the running value is cleared
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x199:  // the running value passes through, reading r[122]
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x19a:  // r[125] = saturate(acc)
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(125, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x19b:  // the left delay line: the latched value into the slot 0x66
            if (c.haswl) { c.ram[c.addr(0x66, 0, false)] = c.wlatch; c.haswl = false; }
            c.px = c.x; c.haspx = c.hasx; c.hasx = false; c.x = 0.0;
            break;
        case 0x19c:  // m[33] takes up the read of 0x197; acc = r[108] / 4
            c.queue_d(33, c.captured(m[33], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x19d:  // r[16] = saturate(4 * acc), the right output
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x19e:  // the running value is cleared; r[108] only feeds the latch
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x19f:  // the right delay line: its own clipped result into the slot 0x67
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
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
void var_m00(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
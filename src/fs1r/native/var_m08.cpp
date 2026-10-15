// fs1r/native/var_m08.cpp - VOP3-2, the variation window's program for the four delay types: its 72 steps.
//
// rom::VARIATION_PROGRAMS[8], the program the window holds for the four variation types 20 to 23, and
// one of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives
// msb 8 to EffectHost::delayLCR (lsb 1, type 20), delayLR (lsb 2, type 21), echo (lsb 3, type 22) and
// crossDelay (lsb 4, type 23). Which of the four is running is not a code difference here: the program
// words are the same under all of them and only the constants and the six delay offsets differ, which
// fx_delay.cpp writes when a parameter moves. The code reads them where they live all the same, as
// step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in the groups the comments below open each of:
//
//   0x158-0x165  the two serial inputs and the host's four tone filters, var::DELAY_LPF (0x159, 0x15a,
//                0x15b) and var::DELAY_HPF (0x15c, 0x15d, 0x15e) with their copies var::DELAY_LPF_COPY
//                and var::DELAY_HPF_COPY (0x160 to 0x162 and 0x163 to 0x165), one one-pole each, three
//                words apiece: r[104] and r[105] the left channel's two poles, r[107] and r[108] the
//                right channel's, the second of a side fed from the first. The three reads that ride on
//                the first step of each group (the slots 0x56, 0x57 and 0x58) are published in m[28],
//                m[29] and m[30].
//   0x166-0x16f  the first two delay taps, the slots 0x59 and 0x5A (var::DELAY_SLOTS[1] and [4]), the
//                third at 0x16f of the slot 0x5B ([2]) and the fourth at 0x173 of the slot 0x5C ([5]).
//                Which line a tap reads is its delay offset and not this file: the program writes two
//                lines, at 0x17b and at 0x183, and a tap's offset says how far behind one of them it
//                sits.
//   0x170-0x177  the first two taps into r[109] and r[110] (0x170 and 0x174 are class-3 steps: a unity
//                product with the adder off, which is step()'s plain copy) and each through the host's
//                damp pair into r[119] and r[120]: var::DELAY_CURVE (0x172, 0x173) on the first, and
//                var::DELAY_CURVE_COPY (0x176, 0x177) on the second.
//   0x178-0x17b  the left line's write, into the slot 0x5E (var::DELAY_SLOTS[0]): the two channels'
//                damped inputs and the feedback off r[119] and r[120]. 0x17b carries w1[13] and is
//                itself a writer, so it writes its own result and takes no latch.
//   0x17c-0x188  the two wet sums r[124] and r[125] out of the taps, the two outputs r[15] and r[16]
//                after the level word each, and the right line's write into the slot 0x60
//                (var::DELAY_SLOTS[3]): 0x181 carries w1[13] and is not a writer, so it latches its
//                result, and 0x183 writes the slot and takes that latch rather than its own.
//                0x187 hands the page's LFO on.
//   0x189-0x19f  the tail: three reads of the window's own area (the slots 0x63, 0x64 and 0x65)
//                published in m[31], m[32] and m[33], five steps later each, which is captured()'s
//                lower bound. The rest of the steps are idle but for the z^-1 latch.
//
// Which of the four types is running is decided by the constants and the six offsets alone. What the
// tail of the program adds up to, each term as the dump's coefficients leave it, with k standing for
// the word that step carries and the letters for the taps the type's own offsets give:
//
//   r[109] r[110]  the first two taps, m[56] and m[57]
//   r[119] r[120]  the same two through the damp pair, which sums to one under every type here
//                  (0x31c7 + 0x4e39 and 0x238e + 0x5c72 are both 0x8000), so the pair is a copy and
//                  what it carries is the tap, not a fraction of it
//   0x17b  the left line   = k178 * r[105] + k179 * r[108]      the two damped inputs
//                           + k17a * r[119] + k17b * r[120]     the feedback off the first two taps
//   0x183  the right line  = k180 * r[120] + k181 * r[119] + k17f * r[108], latched at 0x181
//   r[124] = k17c * m[56] + k17d * m[58] + k17e * m[59]      the left wet sum, three taps
//   r[125] = k182 * m[57] + k183 * m[59] + k184 * m[58]      the right wet sum, the same three taps
//   r[15]  = 4 * 0.25 * r[124]  and r[16] = 4 * 0.25 * r[125], so the two outputs are the two sums
//
//   type 20 Delay L,C,R   k178 = k179 0.5, k17a 0.153, k17b 0   one feedback, the mono sum in one line
//                          r[124] = LCH + 0.62 * CCH, r[125] = RCH + 0.62 * CCH, and all four taps off
//                          that one line, so the right line carries nothing and is written zero
//   type 21 Delay L,R     k178 = k179 0.5, k17a = k17b 0.175     two feedbacks, each half the level
//                          r[124] = LCH, r[125] = RCH
//   type 22 Echo          k178 1.0, k179 0, k17a 0.336           each channel into its own line, each
//                          fed one feedback off its own first tap
//                          r[124] = L1 + 0.238 * L2, r[125] = R1 + 0.238 * R2
//   type 23 Cross Delay   k178 = k179 0.5, k17a 0, k17b 0.366   the mono sum into one line and one
//                          feedback off the other line's tap; the third and fourth taps sit one sample
//                          behind the right line's write, so they read that line a pass back
//                          r[124] = m[56] + 0.72 * m[59], r[125] = m[57]
//
// which tap each type means by a cell is its delay offsets and not this file: var::DELAY_SLOTS[1] is
// the first feedback (L,C,R), the second feedback (L,R), the first left tap (Echo) or the right-to-left
// delay (Cross Delay), and so on down the list. The old model in vop3_effects.h's run_delay() has the
// same shape, two lines and a handful of taps, and is no reference for any number here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] and r[106] the two inputs after the step
// that scales them, r[104] and r[105] the left channel's two damp poles and r[107] and r[108] the
// right channel's; r[109] and r[110] the first two taps, r[119] and r[120] those two through the damp
// pair; r[124] and r[125] the two wet sums. The cells: m[2] and m[6] the two serial inputs this block
// takes for the base program, m[11] and m[12] the two LFO values the window publishes, m[28], m[29]
// and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads, and m[56] to m[59] the
// four delay taps, in step order: the slots 0x59, 0x5A, 0x5B and 0x5C. The delay slots: 0x56, 0x57 and
// 0x58 read what the base program published, and 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x60, 0x63, 0x64 and
// 0x65 are the block's own area, which is where the base program's INIT_DELAY_* offsets put them. No
// step here reads at a delay index and none writes the t file, so no step of this program is
// fractional and none takes a multiplier from a t register.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m08: identical, 4 types, 38400 native passes.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per
// step and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips
// these steps as it would the interpreter's.
void block_0x158(yss236_core& c)
{
    double* const r = c.r;                 // the 128 registers
    double* const m = c.m;                 // the 64 cells
    double xv, k, prod, a, y, outv;

    // The step's x operand: the register r[sr] into the latch every step but a class-2/3 one moves, and
    // xv alongside it for the multiplier.
    auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };

    for (int st = 0x158; st < 0x1a0; st++)
    {
        c.begin_step();
        if (st < c.m_skip) continue;
        switch (st)
        {
// ---- 0x158 to 0x165, the two inputs and the host's four tone filters ----------------------------
// m[2] takes the left serial input; r[103] = saturate(2 * 0.5 * r[5]), the left input alone
        case 0x158:
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[6] takes the right serial input; the left pole's -a1 on its own state, var::DELAY_LPF[2]
        case 0x159:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_LPF[1] on the left input a pass back
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_LPF[0]: the pole's b0 on the left input, doubled and saturating, into r[104], and the
// read of the slot 0x56 the base program published (published in m[28] at 0x160)
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// var::DELAY_HPF[2]: the second pole's -a1 on r[105]
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_HPF[1] on the first pole's state, r[104]
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_HPF[0]: the second pole's b0, four times and saturating, into r[105]
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[11] takes the page's LFO; r[106] = saturate(2 * 0.5 * r[6]), the right input alone, and the read
// of the slot 0x57 the base program published (published in m[29] at 0x164)
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            c.queue_d(11, c.lfo[10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[28] takes up the read of 0x15b; the right pole's -a1 on r[107], var::DELAY_LPF_COPY[2]
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_LPF_COPY[1] on the right input
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_LPF_COPY[0]: the right pole's b0, doubled and saturating, into r[107]
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// var::DELAY_HPF_COPY[2] on r[108], and the read of the slot 0x58 (published in m[30] at 0x168)
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[29] takes up the read of 0x15f; var::DELAY_HPF_COPY[1] on r[107]
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_HPF_COPY[0]: the right second pole's b0, four times and saturating, into r[108]
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x166 to 0x16f, the first two taps --------------------------------------------------------
// nothing but the z^-1 latch
        case 0x166:
            fetch(0);
            break;
// the first tap, the slot 0x59 (var::DELAY_SLOTS[1])
        case 0x167:
            c.push_hist(st, c.ram[c.addr(0x59, 0, false)]);
            fetch(0);
            break;
// m[30] takes up the read of 0x163, for a window outside this one
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            fetch(0);
            break;
        case 0x169: case 0x16a:
            fetch(0);
            break;
// the second tap, the slot 0x5A (var::DELAY_SLOTS[4])
        case 0x16b:
            c.push_hist(st, c.ram[c.addr(0x5A, 0, false)]);
            fetch(0);
            break;
// m[56] takes up the read of 0x167: the first tap, five steps later, which is captured()'s lower bound
        case 0x16c:
            c.queue_d(56, c.captured(m[56], st));
            fetch(0);
            break;
        case 0x16d: case 0x16e:
            fetch(0);
            break;
// the third tap, the slot 0x5B (var::DELAY_SLOTS[2])
        case 0x16f:
            c.push_hist(st, c.ram[c.addr(0x5B, 0, false)]);
            fetch(0);
            break;
// ---- 0x170 to 0x177, the taps into r[109] and r[110] and the host's damp pair ------------------
// m[57] takes up the read of 0x16b, and r[109] = m[56] (class 3: unity product, adder off)
        case 0x170:
            c.queue_d(57, c.captured(m[57], st));
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            prod = c.src;                  // class 3's product is its m operand whole; k is not read
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(109, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x171:
            fetch(0);
            break;
// var::DELAY_CURVE[1], the low half of the damp pair, on r[109]
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_CURVE[0]: the high half onto the running value, saturating, into r[119]; and the read of
// the fourth tap, the slot 0x5C (var::DELAY_SLOTS[5]), which m[59] takes up at 0x178
        case 0x173:
            c.push_hist(st, c.ram[c.addr(0x5C, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[58] takes up the read of 0x16f, and r[110] = m[57] (class 3, a plain copy)
        case 0x174:
            c.queue_d(58, c.captured(m[58], st));
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            prod = c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(110, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x175:
            fetch(0);
            break;
// var::DELAY_CURVE_COPY[1], the low half of the other damp pair, on r[110]
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_CURVE_COPY[0]: the high half onto the running value, saturating, into r[120]
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x178 to 0x17b, the left line's write into the slot 0x5E ------------------------------------
// m[59] takes up the read of 0x173; the left channel's damped input at half level
        case 0x178:
            c.queue_d(59, c.captured(m[59], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the right channel's damped input, at whatever share the type leaves this word
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[7]: the first feedback level, on r[119]
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[9]: the second feedback level on r[120], and the write of the left line into the
// slot 0x5E. This step carries w1[13] and is itself a writer, so it writes its own result and takes
// no latch.
        case 0x17b:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            if (c.haswl) { outv = c.wlatch; c.haswl = false; }
            c.ram[c.addr(0x5E, 0, false)] = outv;
            break;
// ---- 0x17c to 0x188, the two wet sums, the two outputs and the right line's write ---------------
// the first tap, at whatever share the type leaves this word (Delay L,R leaves it zero)
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[4], Echo's second delay level, on the third tap
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[6], the centre level, on the fourth tap: the left wet sum r[124]
        case 0x17e:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the right channel's damped input, at whatever share the type leaves this word
        case 0x17f:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[8]: the first feedback level of the right line on r[120]. This step reads no
// memory: the window's own area has no read here.
        case 0x180:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[10]: the second feedback level on r[119]. This step carries w1[13] and is not a
// writer, so it latches its result and leaves the write to 0x183, which writes the slot 0x60
// (var::DELAY_SLOTS[3]) and takes that latch rather than its own result.
        case 0x181:
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.wlatch = outv;
            c.haswl = true;
            break;
// the second tap at full level
        case 0x182:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::DELAY_WORDS[5], Echo's second delay level, on the fourth tap. This step is the writer and
// does not carry w1[13], so it writes into the slot 0x60 whatever 0x181 latched.
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            if (c.haswl) { outv = c.wlatch; c.haswl = false; }
            c.ram[c.addr(0x60, 0, false)] = outv;
            break;
// the third tap onto the running value: the right wet sum r[125]
        case 0x184:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(125, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// nothing but the running value, which the next step folds into the output
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[15], the left output: the left wet sum at a quarter and four times, wrapping
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[12] takes the page's LFO, and the running value for 0x188
        case 0x187:
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[16], the right output: the right wet sum at a quarter and four times, wrapping
        case 0x188:
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// ---- 0x189 to 0x19f, the tail: three published reads, the rest idle -----------------------------
        case 0x189: case 0x18a: case 0x18b: case 0x18c: case 0x18d: case 0x18e:
            fetch(0);
            break;
// a read of the slot 0x63, five steps before the capture at 0x194 that takes it
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            fetch(0);
            break;
        case 0x190: case 0x191: case 0x192:
            fetch(0);
            break;
// a read of the slot 0x64, five steps before 0x198
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            fetch(0);
            break;
// m[31] takes up the read of 0x18f
        case 0x194:
            c.queue_d(31, c.captured(m[31], st));
            fetch(0);
            break;
        case 0x195: case 0x196:
            fetch(0);
            break;
// a read of the slot 0x65, five steps before 0x19c
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            fetch(0);
            break;
// m[32] takes up the read of 0x193
        case 0x198:
            c.queue_d(32, c.captured(m[32], st));
            fetch(0);
            break;
        case 0x199: case 0x19a: case 0x19b:
            fetch(0);
            break;
// m[33] takes up the read of 0x197
        case 0x19c:
            c.queue_d(33, c.captured(m[33], st));
            fetch(0);
            break;
        case 0x19d: case 0x19e: case 0x19f:
            fetch(0);
            break;
        }
    }
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m08(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
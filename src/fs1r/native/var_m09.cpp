// fs1r/native/var_m09.cpp - VOP3-2, the variation window's program for Karaoke: its 72 steps.
//
// rom::VARIATION_PROGRAMS[9], the program the variation window holds under variation type 24, which
// fx_dispatch.cpp routes to EffectHost::karaoke(). It is one of the twenty-nine programs the window
// shares: nothing here knows which type is running, because the program words are fixed and only the
// constants and the delay offsets move, which fx_delay.cpp writes whenever a parameter does. The code
// reads them where they live on the pass, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in the groups the comments below open each of:
//
//   0x158-0x15f  the two serial inputs and the host's two tone filters, var::KARAOKE_HPF (0x15a, 0x15b,
//                0x15c) and var::KARAOKE_LPF (0x15d, 0x15e, 0x15f), one pole each, three words apiece.
//                The step that sums the two inputs is 0x159: r[103] = 2 * (k * r[6] + k * r[5]), which
//                at the shipped constants is half the sum, so the whole chain runs on the mono. The
//                high-pass's state is r[104] and the low-pass's r[105], the low-pass fed from the
//                high-pass. The two reads that ride on the filter steps (the slots 0x56 and 0x57) are
//                published in m[28] and m[29].
//   0x160-0x16d  the published reads and the first two row taps: the reads of the slots 0x58, 0x59 and
//                0x5A, published in m[30], m[60] and m[61]. 0x167 also takes the page's LFO into m[11].
//   0x16e-0x173  the first of the four row units, the pair var::KARAOKE_ROW_STEPS[0..3] over the slot
//                0x59: 0x16f reads the line (m[60], the read of 0x167) and 0x171 writes it through the
//                latch, which 0x173 spends on the slot 0x5C.
//   0x174-0x177  the second row unit, var::KARAOKE_ROW_STEPS[4..7] over the slot 0x5A: 0x175 reads the
//                line (m[61], the read of 0x16b) and 0x177 writes the slot 0x5D itself.
//   0x178-0x183  the main line. 0x176 takes up the read of 0x16f, the slot 0x5B, which is where
//                EffectHost::karaoke() puts the delay time: var::KARAOKE_SLOTS[1] is set to
//                var::KARAOKE_SLOTS[0]'s plus PITCH_DELAY. 0x178 to 0x17a fold that tap into m[55]
//                through var::KARAOKE_LEVEL, the feedback level, and 0x17d adds it to r[120] and latches
//                the sum, which 0x183 spends on the slot 0x60, var::KARAOKE_SLOTS[0], the line's own
//                write. The two taps at 0x17b and 0x17c are the line's output into the two row units
//                below, half each.
//   0x184-0x18b  the third and fourth row units, var::KARAOKE_ROW_STEPS[8..15] over the slots 0x5E and
//                0x5F: 0x185 reads the third line's tap (m[60], the read of 0x17b) and 0x187 writes the
//                slot 0x61, and 0x189 reads the fourth line's tap (m[61], the read of 0x17f) and 0x18b
//                writes the slot 0x62. 0x187 also takes the page's LFO into m[12].
//   0x18c-0x18f  the two outputs, the dry/wet pan var::KARAOKE_PAN: r[15] = 4 * (k * r[120] + k * r[5])
//                and r[16] = 4 * (k * r[121] + k * r[6]), the row chain's two sums and the two inputs.
//                0x18f reads the slot 0x63 on the way past.
//   0x190-0x19f  the tail: the reads of the slots 0x64 and 0x65 published in m[32] and m[33], and the
//                read of the slot 0x63 published in m[31] at 0x194, five steps after the read each time,
//                which is captured()'s lower bound. The rest of the steps are idle but for the z^-1
//                latch.
//
// Which units the row is made of, and what each word of a unit carries, is EffectHost::diffusers()'s
// business: it writes four words per unit (s28, s24, s26, s22) at var::KARAOKE_ROW_STEPS and the delay
// offsets at var::KARAOKE_ROW_SLOTS, which is the table {0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62, 0x5F},
// a write slot and a read slot for each of the four units. What each step adds up to, with k the word
// that step carries:
//
//   r[103]        0x159       = 2 * (k159 * r[6] + k158 * r[5])              the mono sum
//   r[104]        0x15c       = 2 * (k15c * r[103] + k15b * r[103] + k15a * r[104])
//   r[105]        0x15f       = 2 * (k15f * r[104] + k15e * r[104] + k15d * r[105])
//   line 0x5C     0x171       = k171 * line(0x59) + k170 * r[105]             row 1
//   r[120]        0x16f       = 2 * (k16f * line(0x59) + k16e * r[105])
//   r[120]        0x175       = 2 * (k175 * line(0x5A) + k174 * r[120])      row 2
//   line 0x5D     0x177       = k177 * line(0x5A) + k176 * r[120]
//   m[55]         0x17a       = k17a * m[55] + k179 * line(0x5B) + k178 * line(0x5B)
//   line 0x60     0x17d/0x183 = k17d * r[120] + m[55]                        the main line's write
//   r[120]        0x17b       = 2 * k17b * line(0x5B)   and r[121] at 0x17c = 2 * k17c * line(0x5B)
//   r[120]        0x185       = 2 * (k185 * line(0x5E) + k184 * r[120])      row 3
//   line 0x61     0x187       = k187 * line(0x5E) + k186 * r[120]
//   r[121]        0x189       = 2 * (k189 * line(0x5F) + k188 * r[121])      row 4
//   line 0x62     0x18b       = k18b * line(0x5B) + k18a * r[121]
//   r[15]         0x18d       = 4 * (k18d * r[120] + k18c * r[5])            the pan
//   r[16]         0x18f       = 4 * (k18f * r[121] + k18e * r[6])
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three
// steps allows, which is why the sums above read r[120] where the reader stands before the writer's
// write has landed. The old model in vop3_effects.h's run_delay() has the Karaoke case as one line, a
// feedback off its own tap and a second tap one delay further on, and is no reference for any number
// here.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input, r[104] and r[105] the two
// tone-filter poles; r[120] and r[121] the two sums the pan reads. The cells: m[2] and m[6] the two
// serial inputs this block takes for the base program, m[11] and m[12] the two LFO values the window
// publishes, m[28], m[29] and m[30] the three tone-filter reads, m[31] to m[33] the three tail reads,
// m[54] the main line's tap, m[55] the feedback off it, and m[60] and m[61] the row taps, which each
// cell takes twice a pass, once for the row that feeds the line and once for the row that comes off it.
// The delay slots: 0x5C, 0x59, 0x5D, 0x5A, 0x61, 0x5E, 0x62 and 0x5F the four row lines and their
// offsets, 0x5B and 0x60 the main line's tap and its write, 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the
// reads the base program and the other windows share. No step here reads at a delay index and none
// writes the t file, so no step of this program is fractional and none takes a multiplier from a t
// register.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m09: identical, 1 type, 9600 native passes.

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
// ---- 0x158 to 0x15f, the two inputs and the host's two tone filters -----------------------------
// m[2] takes the left serial input; the left input alone, at the running value
        case 0x158:
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[6] takes the right serial input; r[103] = saturate(2 * (k * r[6] + k * r[5])), the mono sum the
// whole chain runs on
        case 0x159:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// var::KARAOKE_HPF[2]: the high-pass pole's -a1 on its own state, r[104]
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_HPF[1]: b1 on the mono, onto that pole term, and the read of the slot 0x56 the base
// program published (published in m[28] at 0x160)
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_HPF[0]: b0 on the mono, doubled and saturating, into r[104]
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// var::KARAOKE_LPF[2]: the low-pass pole's -a1 on r[105]
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_LPF[1]: b1 on the high-pass's output, r[104]
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_LPF[0]: b0 on r[104], doubled and saturating, into r[105], and the read of the slot
// 0x57 the base program published (published in m[29] at 0x164)
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x160 to 0x16d, the published reads and the first two row taps -----------------------------
// m[28] takes up the read of 0x15b
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            fetch(0);
            break;
        case 0x161: case 0x162:
            fetch(0);
            break;
// the read of the slot 0x58, which m[30] takes up at 0x168
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            fetch(0);
            break;
// m[29] takes up the read of 0x15f
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            fetch(0);
            break;
        case 0x165: case 0x166:
            fetch(0);
            break;
// the first row line's tap, the slot 0x59, and the page's LFO into m[11]
        case 0x167:
            c.push_hist(st, c.ram[c.addr(0x59, 0, false)]);
            c.queue_d(11, c.lfo[11]);
            fetch(0);
            break;
// m[30] takes up the read of 0x163
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            fetch(0);
            break;
        case 0x169: case 0x16a:
            fetch(0);
            break;
// the second row line's tap, the slot 0x5A
        case 0x16b:
            c.push_hist(st, c.ram[c.addr(0x5A, 0, false)]);
            fetch(0);
            break;
// m[60] takes up the read of 0x167: the first row line's tap, five steps later
        case 0x16c:
            c.queue_d(60, c.captured(m[60], st));
            fetch(0);
            break;
        case 0x16d:
            fetch(0);
            break;
// ---- 0x16e to 0x173, the first row unit, var::KARAOKE_ROW_STEPS[0..3] ----------------------------
// the row's input word on the filtered mono
        case 0x16e:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the read of the slot 0x5B, the main line's tap, on the row's sum, doubled and saturating, into
// r[120]
        case 0x16f:
            c.push_hist(st, c.ram[c.addr(0x5B, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[61] takes up the read of 0x16b: the second row line's tap; the row's feedback word on the
// filtered mono
        case 0x170:
            c.queue_d(61, c.captured(m[61], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_ROW_STEPS[3]: the row's own line, the slot 0x5C, which this step latches and 0x173
// writes. It carries w1[13] and is not a writer, so it latches its result and leaves the write to
// 0x173.
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.wlatch = outv;
            c.haswl = true;
            break;
        case 0x172:
            fetch(0);
            break;
// the write of the first row line into the slot 0x5C: whatever 0x171 latched
        case 0x173:
            fetch(0);
            if (c.haswl) { c.ram[c.addr(0x5C, 0, false)] = c.wlatch; c.haswl = false; }
            break;
// ---- 0x174 to 0x177, the second row unit, var::KARAOKE_ROW_STEPS[4..7] ---------------------------
// the row's input word on r[120], the first unit's sum
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the second line's tap, m[61], onto that, doubled and saturating, into r[120] again
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[54] takes up the read of 0x16f: the main line's tap, the slot 0x5B
        case 0x176:
            c.queue_d(54, c.captured(m[54], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_ROW_STEPS[7]: the second row line's own write into the slot 0x5D. This step carries
// w1[13] and is itself a writer, so it writes its own result and takes no latch.
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x5D, 0, false)] = outv;
            break;
// ---- 0x178 to 0x183, the main line's feedback and its write ---------------------------------------
// var::KARAOKE_LEVEL[1]: the feedback level on the main line's tap, m[54]
        case 0x178:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_LEVEL[0]: the same word again, onto the running value
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the feedback onto itself a pass back, into m[55]. The step reads its own cell and writes it, and
// writes it because the adder is not off: the cell still holds the previous pass's sum.
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
// the main line's output, the slot 0x5E's read is on the way past, at half level into r[120]
        case 0x17b:
            c.push_hist(st, c.ram[c.addr(0x5E, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the same tap at half level into r[121]
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the main line's write: r[120] and the feedback m[55] together, saturating, latched for 0x183
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[55];
            prod = k * xv;
            a = c.src;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.wlatch = outv;
            c.haswl = true;
            break;
        case 0x17e:
            fetch(0);
            break;
// the third row line's tap, the slot 0x5F, on the way to m[61]
        case 0x17f:
            c.push_hist(st, c.ram[c.addr(0x5F, 0, false)]);
            fetch(0);
            break;
// m[60] takes up the read of 0x17b: the third row line's tap, five steps later
        case 0x180:
            c.queue_d(60, c.captured(m[60], st));
            fetch(0);
            break;
        case 0x181: case 0x182:
            fetch(0);
            break;
// the main line's write into the slot 0x60, var::KARAOKE_SLOTS[0]: whatever 0x17d latched. This
// step does not carry w1[13], so it writes the latch rather than its own result.
        case 0x183:
            fetch(0);
            if (c.haswl) { c.ram[c.addr(0x60, 0, false)] = c.wlatch; c.haswl = false; }
            break;
// ---- 0x184 to 0x18b, the third and fourth row units, var::KARAOKE_ROW_STEPS[8..15] ----------------
// m[61] takes up the read of 0x17f: the fourth row line's tap; the row's input word on r[120], which
// is half the main line's tap from 0x17b
        case 0x184:
            c.queue_d(61, c.captured(m[61], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the third line's tap, m[60], onto that, doubled and saturating, into r[120] again
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the row's feedback word on r[120]
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_ROW_STEPS[11]: the third row line's own write into the slot 0x61, and the page's LFO
// into m[12]. Carries w1[13] and is a writer, so it writes its own result.
        case 0x187:
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x61, 0, false)] = outv;
            break;
// the row's input word on r[121], half the main line's tap from 0x17c
        case 0x188:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the fourth line's tap, m[61], onto that, doubled and saturating, into r[121] again
        case 0x189:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the row's feedback word on r[121], which 0x18b writes into the slot 0x62
        case 0x18a:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_ROW_STEPS[15]: the fourth row line's own write into the slot 0x62. Carries w1[13] and
// is a writer, so it writes its own result.
        case 0x18b:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x62, 0, false)] = outv;
            break;
// ---- 0x18c to 0x18f, the pan and the two outputs ------------------------------------------------
// var::KARAOKE_PAN[0]: the left input on the running value
        case 0x18c:
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_PAN[1]: r[120], the wet sum, onto that, four times and saturating, into r[15]
        case 0x18d:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// var::KARAOKE_PAN[2]: the right input on the running value
        case 0x18e:
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// var::KARAOKE_PAN[3]: r[121], the wet sum, onto that, four times and saturating, into r[16]; the
// read of the slot 0x63 rides on this step and m[31] takes it up at 0x194
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x190 to 0x19f, the tail: three published reads, the rest idle -----------------------------
        case 0x190: case 0x191: case 0x192:
            fetch(0);
            break;
// the read of the slot 0x64, five steps before the capture at 0x198 that takes it
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
// the read of the slot 0x65, five steps before 0x19c
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
void var_m09(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

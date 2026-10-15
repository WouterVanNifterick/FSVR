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
// The 72 steps, in the groups the comments in the switch open:
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
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m03: identical, 3 types, 28800 native passes.

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
        // ---- 0x158 to 0x15e, the left input and its filter ---------------------------------------
        // r[119] = wrap(k * r[5]), the left serial input through the ROM's ~unity word
        case 0x158:
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[103] = saturate(2 * acc), the left input doubled and the filter's input
        case 0x159:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // b1 * r[103]
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // a read of the slot 0x56; r[104] takes the previous step's x operand, which is r[103] of
        // the pass before; then b2 * r[104] + acc
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(104, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // -a1 * r[105] + acc, r[105] the output of the pass before
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[106] takes the previous step's x operand, r[105] as it was when 0x15c read it;
        // then -a2 * r[106] + acc
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(106, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[105] = saturate(2 * (b0 * r[103] + acc)), the filter's output
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x15f to 0x164, the right input and the same filter ----------------------------------
        // a read of the slot 0x57; r[107] = saturate(2 * k * r[6]), the right input doubled
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // m[28] takes up the read of 0x15f, which captured the slot 0x56 word; b1 * r[107]
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[108] takes the previous step's x operand; b2 * r[108] + acc
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(108, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // -a1 * r[109] + acc
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // a read of the slot 0x58; r[110] takes the previous step's x operand; -a2 * r[110] + acc
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(110, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // m[29] takes up the read of 0x163, the slot 0x57 word;
        // r[109] = saturate(2 * (b0 * r[107] + acc)), the right filter's output
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x165 to 0x167, the threshold and the drive -----------------------------------------
        // the class-2 step: DYN_THRESHOLD's word scales the level m[55] holds into r[126]
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(126, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // m[11] takes the LFO of this page; r[1] = saturate(2 * k * r[105]), HM_DRIVE on the left
        case 0x166:
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(1, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // r[2] = saturate(2 * k * r[109]), HM_DRIVE on the right
        case 0x167:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(2, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x168 to 0x16f, the threshold ladder ------------------------------------------------
        // m[30] takes up the read of 0x163, the slot 0x58 word; r[119] = max(0, r[126] - 64)
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(119, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[120] = max(0, r[126] - 0.5)
        case 0x169:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(120, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[121] = max(0, r[126] - 2)
        case 0x16a:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(121, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[122] = max(0, r[126] - 8)
        case 0x16b:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(122, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[123] = max(0, r[126] - 32)
        case 0x16c:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(123, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[124] = max(0, r[126] - 128), which this row's constant holds at zero
        case 0x16d:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(124, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[125] = max(0, r[126] - 2048), zero for the same reason
        case 0x16e:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(125, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // r[126] = max(0, r[126] - 64)
        case 0x16f:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(126, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // ---- 0x170 to 0x177, the ratio curve -----------------------------------------------------
        // acc = 2 * k * r[119], the first term of DYN_RATIO
        case 0x170:
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = 2 * (k * r[121] + acc)
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = 2 * (k * r[122] + acc)
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = 2 * (k * r[120] + acc)
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = 2 * (k * r[123] + acc)
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = 4 * (k * r[124] + acc)
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[125] + acc
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[126] + acc, the last term: the curve the smoother takes
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // ---- 0x178 to 0x17b, the one-pole smoother ------------------------------------------------
        // the class-1 load: r[115] = max(0, 128 * k + acc), the curve into the smoother
        case 0x178:
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(115, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        // acc = k * r[115], the first of DYN_ATTACK's three words
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[116] + acc, the smoother's own pole
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[116] = wrap(k * r[115] + acc), the smoother's state, and t[9] takes the result
        // as the 1.15 word the gain below is read from
        case 0x17b:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(116, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 9, false);
            break;
        // ---- 0x17c to 0x17e, the detector ---------------------------------------------------------
        // acc = k * r[1], the left driven input
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[119] = |16 * (k * r[2] + acc)|, both channels summed and rectified: the level
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(119, yss236_core::mode(y, 3));
            y = c.acc = yss236_core::accmode(y, 3);
            break;
        // DYN_RELEASE's first word: acc = max(0, k * m[54] - the level), and m[54] takes it
        case 0x17e:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x17f to 0x181, the gain applied to the two channels ----------------------------------
        // r[120] = saturate(t[9] * r[1]); t[10] takes it as a 1.15 word
        case 0x17f:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 10, false);
            break;
        // r[121] = saturate(t[9] * r[2]); t[11] takes it as a 1.15 word
        case 0x180:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 11, false);
            break;
        // the class-3 step: acc = saturate(r[119] + m[54]), and m[54] takes it
        case 0x181:
            fetch(119);
            c.psrc = c.src; c.src = m[54];
            m2 = xv; prod = m2;
            a = c.src;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x182 to 0x186, the second accumulator, and the level the threshold reads --------------
        // acc = k * m[55], the second accumulator's first term
        case 0x182:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_RELEASE's second word: acc = k * m[55] + acc / 32768
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = c.acc / 32768.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the class-3 step: acc = wrap(r[119] - acc)
        case 0x184:
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_RELEASE's third word: acc = max(0, k * m[54] - acc)
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        // the class-3 step: acc = saturate(r[119] + acc), and m[55] takes it, which is what 0x165
        // reads a pass later
        case 0x186:
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x187 to 0x18e, the left output's filter ----------------------------------------------
        // m[12] takes the LFO of this page; r[122] = saturate(t[10] * r[1])
        case 0x187:
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // r[123] = saturate(t[11] * r[2])
        case 0x188:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(123, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // acc = k * r[1]
        case 0x189:
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[120] + acc, which with this row's constants is the filter's input
        case 0x18a:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[111] = saturate(16 * (k * r[122] + acc)), the filter's state
        case 0x18b:
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(111, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // DYN_HPF's second word: acc = k * r[111]
        case 0x18c:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_HPF's third word: r[112] = wrap(acc), the one-pass delay. Its write lands at 0x190
        // and 0x18e's lands at 0x191, and no step reads r[112] between them
        case 0x18d:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_HPF's first word: r[112] = saturate(2 * (k * r[111] + acc))
        case 0x18e:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(112, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x18f to 0x194, the right output's filter --------------------------------------------
        // a read of the slot 0x63; acc = k * r[2]
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(2);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[121] + acc, the right filter's input
        case 0x190:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[113] = saturate(16 * (k * r[123] + acc)), its state
        case 0x191:
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(113, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // DYN_HPF_COPY's second word: acc = k * r[113]
        case 0x192:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // a read of the slot 0x64; DYN_HPF_COPY's third word: acc = k * r[114] + acc
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // m[31] takes up the read of 0x18f, the slot 0x63 word;
        // DYN_HPF_COPY's first word: r[114] = saturate(2 * (k * r[113] + acc))
        case 0x194:
            c.queue_d(31, c.captured(m[31], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(114, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x195 to 0x199, the dry and wet ------------------------------------------------------
        // DYN_OUT's first word: acc = k * r[5], the dry left
        case 0x195:
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_OUT's second word: r[15] = saturate(4 * (k * r[112] + acc)), the left output
        case 0x196:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // a read of the slot 0x65; DYN_OUT's third word: acc = k * r[6], the dry right
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // m[32] takes up the read of 0x197, the slot 0x64 word; acc = k * r[112] + acc
        case 0x198:
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // DYN_OUT's fourth word: r[16] = saturate(4 * (k * r[114] + acc)), the right output
        case 0x199:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x19a to 0x19f, the tail -------------------------------------------------------------
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19a:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19b:
            fetch(0);
            break;
        // m[33] takes up the read of 0x197, the slot 0x65 word
        case 0x19c:
            c.queue_d(33, c.captured(m[33], st));
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19d:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19e:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19f:
            fetch(0);
            break;
        }
    }
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

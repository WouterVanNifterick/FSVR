// fs1r/native/var_m02.cpp - VOP3-2, the variation window's program for Distortion, Overdrive and Amp Sim:
// its 72 steps.
//
// rom::VARIATION_PROGRAMS[2], the program the window holds for the three variation types 17 to 19, and the
// only program among the twelve that these three share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 2 to
// Distortion, Overdrive and Amp Sim and to nothing else. Under each of them the same program words run and
// only the constants and the delay offsets move, so this native is one function for all three and no type
// is excluded. Which of the three is running is not a code difference here; the host's parameters are.
//
// The host writes this program's coefficients through the word lists in fx_drive.cpp's DRIVES[0] and
// rom_tables.cpp's var:: namespace, and those lists name most of the steps below: DIST_DRIVE
// {0x158, 0x15b, 0x15f, 0x180}, DIST_INPUT {0x15e, 0x15c, 0x15d}, DIST_OUTPUT {0x198, 0x196, 0x197},
// DIST_LOW_SHELF {0x18f, 0x18c, 0x18b, 0x18d, 0x18e}, DIST_PEAK {0x18a, 0x187, 0x186, 0x189, 0x188},
// DIST_LPF2 {0x185, 0x182, 0x181, 0x184, 0x183}, DIST_BAND {0x15f, 0x162, 0x166, 0x16c, 0x170, 0x173,
// 0x174, 0x175, 0x176, 0x177, 0x178, 0x179} and DIST_PAN {0x191, 0x192, 0x193, 0x194}. What the steps
// compute, in the groups the comments open:
//
//   0x158-0x15e  the drive and the input highpass. 0x158 scales the feedback register r[108] by the
//                drive gain the host derives from the Drive parameter, 0x159 and 0x15a add the two
//                serial inputs into r[103], and 0x15b is a pole on r[119] the same drive drives.
//                0x15c to 0x15e are the input highpass's three words (DIST_INPUT), a biquad whose two
//                input cells m[2] and m[6] this window publishes for it.
//   0x15f-0x166  the rectifier. 0x15f is the second pole the drive drives, 0x161 takes the running
//                value's absolute value into m[56], which is the level everything below works against,
//                0x162 and 0x166 are poles, 0x164 subtracts that level from the signal with negatives
//                clamped away, and 0x165 is the level's one-pole follower into r[105].
//   0x167-0x16e  the same pair again for the second half of the shaper: 0x168 to 0x16b mirror the
//                rectifier into r[106], 0x16c is a pole and 0x16e scales r[106] into r[124].
//   0x16f-0x178  the two class-1 loads and the band sum. 0x16f loads the running value into r[125] and
//                0x171 adds its own constant to r[124], both class-1 steps (s24: a class-1 load reads
//                back as v / 256, which is why the 128 * k form appears). 0x170 and 0x173 are poles, and
//                0x174 to 0x178 sum the five poles r[119] to r[123], the twelve band words DIST_BAND
//                ending here.
//   0x179-0x18e  the tone stack and the ladder. 0x180 is the gain the host derives from the drive and
//                the output level. 0x181 to 0x185 are the second lowpass (DIST_LPF2), 0x186 to 0x18a the
//                mid peak (DIST_PEAK) and 0x18b to 0x18e the low shelf (DIST_LOW_SHELF). The four rcopy
//                steps 0x183, 0x188, 0x18d and 0x18e are the last word of each of those filters, and
//                each one writes the previous step's x operand, so r[110], r[112] and r[114] are the
//                one-pass delays that close those two-tap structures. r[116] is a fourth, on its own.
//   0x18f-0x19f  the dry and wet. 0x190 is the one step here the host parameterises for neither this
//                family nor the others: it takes r[118] into the t file as a 1.15 word, and 0x195 reads
//                t[9] back as the multiplier that scales r[104] into r[107]. 0x191 to 0x194 are the pan
//                and dry/wet words (DIST_PAN) and the two outputs r[15] and r[16]. 0x196 to 0x198 are the
//                output lowpass (DIST_OUTPUT), 0x199 and 0x19a are the two class-3 steps that publish the
//                inputs into m[54] and m[55], and 0x19c publishes the last read.
//
// The registers, as the tables above name them: r[5] and r[6] the two serial inputs the base program
// writes, r[15] and r[16] the two outputs, which the base program reads as the stereo pair after r[13]
// and r[14], r[103] the input sum and r[104] the input highpass's output, r[105] and r[106] the
// rectifier's two followers, r[107] the level term, r[108] the output filter's tail and the register the
// drive gain reads, r[109], r[111], r[113] and r[115] the ladder's four stage outputs with r[110], r[112],
// r[114] and r[116] the four one-pass delays between them, r[117] the biased signal and r[118] its
// one-pole, and r[119] to r[123] the five poles the band sum adds. The cells: m[2] and m[6] the serial
// inputs this window publishes, m[11] and m[12] the two LFO values, m[28] to m[30] and m[31] to m[33] the
// six published reads, m[54] and m[55] the two inputs echoed into cells, and m[56] the level the
// rectifier works against. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring
// pointer with no index; this program writes none of them, so the lines behind these taps are filled
// elsewhere.
//
// The one t register the program uses is t[9], written at 0x190 and read at 0x195.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m02: identical, 3 types, 28800 native passes.

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
        // ---- 0x158 to 0x15e, the drive and the input highpass ---------------------------------------
        case 0x158:  // r[119] = saturate(16 * k * r[108])
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x159:  // acc = k * r[5]
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x15a:  // r[103] = saturate(2 * (k * r[6] + acc))
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x15b:  // a read of the slot 0x56; r[119] = saturate(16 * k * r[119])
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x15c:  // m[2] takes serial bank 0 page 10; acc = k * r[103]
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x15d:  // m[6] takes serial bank 1 page 10; acc = k * r[104] + acc
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x15e:  // r[104] = saturate(2 * (k * r[103] + acc))
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x15f to 0x166, the rectifier -------------------------------------------------------
        case 0x15f:  // a read of the slot 0x57; r[119] = saturate(16 * k * r[119])
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x160:  // m[28] takes up the read of 0x15b; nothing but the x latch runs
            c.queue_d(28, c.captured(m[28], st));
            fetch(0);
            break;
        case 0x161:  // acc = |16 * k * r[104]|; m[56] = the level the rectifier works against
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 3);
            c.queue_d(56, yss236_core::clip(y, yss236_core::DSAT));
            break;
        case 0x162:  // r[120] = saturate(16 * k * r[119])
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x163:  // a read of the slot 0x58; nothing but the x latch runs
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            fetch(0);
            break;
        case 0x164:  // m[29] takes up the read of 0x15f; the rectifier:
                     // acc = k * r[105] - m[56], negatives clamped away
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[56];
            m2 = xv; prod = k * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        case 0x165:  // r[105] = saturate(k * m[56] + acc)
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x166:  // r[121] = saturate(16 * k * r[120])
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x167 to 0x16e, the same pair for the second half of the shaper ----------------------
        case 0x167:  // m[11] takes the LFO 10; acc = k * r[106]
            c.queue_d(11, c.lfo[10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x168:  // m[30] takes up the read of 0x163; acc = k * r[106] + acc / 32768
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc / 32768.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x169:  // acc = k * m[56] - acc
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x16a:  // acc = k * r[105] - acc, negatives clamped away
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        case 0x16b:  // r[106] = saturate(k * m[56] + acc)
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16c:  // r[122] = saturate(16 * k * r[121])
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x16d:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        case 0x16e:  // r[124] = saturate(k * r[106])
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(124, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;

        // ---- 0x16f to 0x178, the two class-1 loads and the band sum -------------------------------
        case 0x16f:  // the class-1 load: r[125] = saturate(128 * k + acc)
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(125, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            break;
        case 0x170:  // r[123] = saturate(16 * k * r[122])
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(123, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x171:  // the class-1 load off its own register: r[124] = saturate(128 * k + r[124])
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(124, yss236_core::mode(v, 1));
            c.acc = yss236_core::accmode(v, 1);
            break;
        case 0x172:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        case 0x173:  // r[119] = saturate(16 * k * r[123])
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x174:  // acc = k * r[119], the band sum's first term
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x175:  // acc = k * r[120] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x176:  // acc = k * r[121] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x177:  // acc = k * r[122] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x178:  // acc = k * r[123] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;

        // ---- 0x179 to 0x18e, the tone stack and the ladder ----------------------------------------
        case 0x179:  // r[119] = saturate(k * r[119])
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x17a:  // acc = k * r[125]
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17b:  // acc = saturate(16 * (k * r[124] + acc))
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x17c:  // the class-1 load: r[117] = 16 * (128 * k + acc), negatives clamped away
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 16.0;
            c.queue_r(117, yss236_core::mode(v, 2));
            c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x17d:  // acc = k * r[117]
            k = yss236_core::s16(c.coef[st]);
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17e:  // acc = k * r[118] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17f:  // r[118] = saturate(k * r[117] + acc)
            k = yss236_core::s16(c.coef[st]);
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(118, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x180:  // r[109] = saturate(k * r[119])
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x181:  // acc = k * r[110]
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x182:  // acc = k * r[109] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x183:  // r[110] takes the previous step's x operand; acc = k * r[112] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(110, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x184:  // acc = k * r[111] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x185:  // r[111] = saturate(2 * (k * r[109] + acc))
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(111, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x186:  // acc = k * r[112]
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x187:  // acc = k * r[111] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x188:  // r[112] takes the previous step's x operand; acc = k * r[114] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(112, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x189:  // acc = k * r[113] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18a:  // r[113] = saturate(2 * (k * r[111] + acc))
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(113, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x18b:  // acc = k * r[114], the clear before the last stage
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18c:  // acc = k * r[113] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18d:  // r[114] takes the previous step's x operand; acc = k * r[115] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(114, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18e:  // r[116] takes the previous step's x operand; acc = k * r[116] + acc
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(116, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;

        // ---- 0x18f to 0x19f, the dry and wet, the pan and the outputs ------------------------------
        case 0x18f:  // a read of the slot 0x63; m[12] takes the LFO 12;
                     // r[115] = saturate(16 * (k * r[113] + acc))
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(115, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x190:  // acc = saturate(2 * k * r[118]); t[9] = the running value as a 1.15 word
            k = yss236_core::s16(c.coef[st]);
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 9, false);
            break;
        case 0x191:  // acc = k * m[54], the input echoed into its cell a pass earlier
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x192:  // r[15] = saturate(4 * (k * r[115] + acc)), the left output
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x193:  // a read of the slot 0x64; acc = k * m[55]
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x194:  // m[31] takes up the read of 0x18f; r[16] = saturate(4 * (k * r[115] + acc))
            c.queue_d(31, c.captured(m[31], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x195:  // r[107] = saturate(t[9] * r[104])
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x196:  // acc = k * r[107]
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x197:  // a read of the slot 0x65; acc = k * r[108] + acc
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x198:  // m[32] takes up the read of 0x193; r[108] = saturate(2 * (k * r[107] + acc))
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        case 0x199:  // the class-3 step: m[54] = r[5] as a cell, and the running value takes it
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
        case 0x19a:  // the class-3 step: m[55] = r[6] as a cell, and the running value takes it
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;                            // class 3 is unity
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
        case 0x19b:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        case 0x19c:  // m[33] takes up the read of 0x197; nothing but the x latch runs
            c.queue_d(33, c.captured(m[33], st));
            fetch(0);
            break;
        case 0x19d:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        case 0x19e:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        case 0x19f:  // the idle step: class 0, no RAM mark and no ALU, so only the x latch
            fetch(0);
            break;
        }
    }
}

}  // namespace

// One run of the variation window's steps, exactly as yss236_core::step() would run them. The program is
// one run; a range no host builds falls back to the interpreter.
void var_m02(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

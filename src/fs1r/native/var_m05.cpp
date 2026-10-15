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
// The 72 steps, in the groups the comments in the switch open:
//
//   0x158-0x165  the two channels' EQ, two order-1 shelves each. 0x158 takes the left input r[5] into
//                r[103] through the ROM's ~unity word and doubled, and 0x159 to 0x15b are the low shelf:
//                b1 times r[103] a pass back, -a1 times r[104], then b0 times r[103] and the sum
//                doubled into r[104]. 0x15c to 0x15e are the high shelf over r[104] into r[105], the left
//                EQ output. 0x15f to 0x165 are the same four steps for the right channel, r[6] into
//                r[106] to r[108]. 0x15b, 0x15f and 0x163 also read the delay RAM, and 0x158 and 0x159
//                take the two serial inputs into m[2] and m[6].
//   0x166        the left cascade's swept gain: the class-1 load is acc = saturate(128 * k + m[8]), the
//                phase shift with the first LFO's depth on it, and r[122] and t[10] take it.
//   0x167-0x16e  the eight section weights. Each word scales one section's output into the running sum,
//                and the two halves are the two cascades: m[55], r[110], r[111] and r[112] on the left,
//                r[115] to r[118] on the right, so each cascade's sections 2 to 5 and neither one's
//                first. Both Stage handlers leave exactly two of the eight at HALF and the other six at
//                zero, and 0x16a and 0x16e double the sums into r[120] and r[121], which cancels the half
//                and leaves the two open sections of each cascade added up. The ROM's own words, which
//                are what the two types run with until Stage moves, leave 0x169 and 0x16b at half.
//   0x16f        t[10] = r[122] = saturate(t[10] * r[122]), the left gain squared.
//   0x170-0x173  the diffuse tap: four steps over r[120], r[121], r[114] and r[117], of which only one
//                word is ever half, and r[119] takes the sum doubled.
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
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m05: identical, 2 types, 19200 native passes.

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
// ---- 0x158 to 0x165, the two channels' EQ, two order-1 shelves each ------------------------
// m[2] takes the left serial input; r[103] = saturate(2 * k * r[5]), the left input through the ROM's ~unity word and doubled
        case 0x158:
            c.queue_d(2, c.in[0][10]);
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
// m[6] takes the right serial input; the left low shelf's b1: k * r[103], the input a pass back
        case 0x159:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the left low shelf's -a1: k * r[104] + acc, its output a pass back
        case 0x15a:
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
// a read of the slot 0x56; the left low shelf's b0: r[104] = saturate(2 * (k * r[103] + acc))
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
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
// the left high shelf's b1: k * r[104]
        case 0x15c:
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
// the left high shelf's -a1: k * r[105] + acc
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the left high shelf's b0: r[105] = saturate(4 * (k * r[104] + acc)), the left EQ output
        case 0x15e:
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
// a read of the slot 0x57; r[106] = saturate(2 * k * r[6]), the right input doubled
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
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
// m[28] takes up the read of 0x15b; the right low shelf's b1: k * r[106]
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// the right low shelf's -a1: k * r[107] + acc
        case 0x161:
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
// the right low shelf's b0: r[107] = saturate(2 * (k * r[106] + acc))
        case 0x162:
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
// a read of the slot 0x58; the right high shelf's b1: k * r[107]
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
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
// m[29] takes up the read of 0x15f; the right high shelf's -a1: k * r[108] + acc
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
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
// the right high shelf's b0: r[108] = saturate(4 * (k * r[107] + acc)), the right EQ output
        case 0x165:
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
// ---- 0x166, the left cascade's swept gain -----------------------------------------------------
// m[11] takes the LFO of this page; PH_FB[0]'s word: acc = saturate(128 * k + m[8]), the phase shift with the first LFO's depth on it; r[122] and t[10] take it
        case 0x166:
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            a = c.src;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(122, yss236_core::mode(v, 1));
            y = c.acc = yss236_core::accmode(v, 1);
            twr(y, 10, false);
            break;
// ---- 0x167 to 0x16e, the eight section weights ------------------------------------------------
// PH_STAGES[0]: acc = k * m[55], the left cascade's second section, and the sum starts here
        case 0x167:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[30] takes up the read of 0x163; PH_STAGES[1]: acc = k * r[110] + acc
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_STAGES[2]: acc = k * r[111] + acc
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_STAGES[3]: r[120] = saturate(2 * (k * r[112] + acc)), the left cascade's four sections summed
        case 0x16a:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// PH_STAGES[4]: acc = k * r[115], which starts the right cascade's sum
        case 0x16b:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_STAGES[5]: acc = k * r[116] + acc
        case 0x16c:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_STAGES[6]: acc = k * r[117] + acc
        case 0x16d:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_STAGES[7]: r[121] = saturate(2 * (k * r[118] + acc)), the right cascade's four sections summed
        case 0x16e:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x16f, the left gain squared -------------------------------------------------------------
// t[10] = r[122] = saturate(t[10] * r[122]), so the left gain is the phase shift squared
        case 0x16f:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 10, false);
            break;
// ---- 0x170 to 0x173, the diffuse tap -----------------------------------------------------------
// acc = k * r[120], the left sum on the ROM's own word at this step, which is zero
        case 0x170:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_DIR[0]: acc = k * r[121] + acc, the right sum on the same accumulator
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_DIR[1]: acc = k * r[114] + acc, the right cascade's first section
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_DIR[2]: r[119] = saturate(2 * (k * r[117] + acc)), the diffuse tap
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x174 to 0x175, the left gain finished ----------------------------------------------------
// the class-1 load of the ROM's 0x7ffe: acc = 128 * k, about 128, and nothing else
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = 0.0;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 0);
            break;
// acc = saturate(t[10] * r[122] - acc); t[10] takes it, so the left gain is the phase shift to the fourth less 128
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 10, false);
            break;
// ---- 0x176 to 0x179, the feedback the left cascade starts from --------------------------------
// acc = k * r[105], a quarter of the left EQ output at the ROM's own word
        case 0x176:
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
// acc = 2 * (k * r[108] + acc), a quarter of the right EQ output over the same sum
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = k * r[120] + acc; this word is the ROM's zero unless phaser2() writes it with the width
        case 0x178:
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
// PH_WIDTH1's word: m[54] = saturate(k * r[121] + acc), the feedback the left cascade starts from
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
// ---- 0x17a, the right cascade's swept gain ----------------------------------------------------
// PH_FB[1]'s word: the same class-1 load on m[9], the second LFO's depth; r[122] and t[11] take it
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[9];
            a = c.src;
            v = 128.0 * k + a;
            v *= 1.0;
            c.queue_r(122, yss236_core::mode(v, 1));
            y = c.acc = yss236_core::accmode(v, 1);
            twr(y, 11, false);
            break;
// ---- 0x17b to 0x17c, the left cascade's first section ----------------------------------------
// acc = t[10] * r[109] - m[54], the left cascade's first section, on the cross-channel feedback
        case 0x17b:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(109);
            c.psrc = c.src; c.src = m[54];
            m2 = xv;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[109] = saturate(t[10] * m[54] - acc), that section's output and the next section's input
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x17d, the right gain squared ------------------------------------------------------------
// t[11] = r[122] = saturate(t[11] * r[122]), so the right gain is the phase shift squared
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(122, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 11, false);
            break;
// ---- 0x17e to 0x17f, the left cascade's second section ---------------------------------------
// acc = t[10] * m[55] - r[109], the second section, still on the first section's output
        case 0x17e:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(109);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src;
            prod = g * m2;
            a = xv;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[55] = saturate(t[10] * r[109] + acc), its output, which PH_STAGES[0] scales into the sum
        case 0x17f:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
// ---- 0x180, the zero the right gain subtracts --------------------------------------------------
// the class-1 load of the ROM's zero, and m[56] takes the accumulator: what the right gain subtracts
        case 0x180:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = 0.0;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 1);
            c.queue_d(56, yss236_core::clip(y, yss236_core::DSAT));
            break;
// ---- 0x181 to 0x188, the left cascade's third to fifth sections -------------------------------
// acc = t[10] * r[110] - m[55], the third section
        case 0x181:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(110);
            c.psrc = c.src; c.src = m[55];
            m2 = xv;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[110] = saturate(t[10] * m[55] - acc), its output
        case 0x182:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(110, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the class-3 step: acc = r[110], what the next section subtracts
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = t[10] * r[111] - acc, the fourth section
        case 0x184:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[111] = saturate(t[10] * r[110] - acc), its output
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(111, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// m[12] takes the LFO of this page; the class-3 step: acc = r[111]
        case 0x186:
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = t[10] * r[112] - acc, the fifth section
        case 0x187:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[112] = saturate(t[10] * r[111] - acc), its output, which PH_STAGES[3] scales into the sum
        case 0x188:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[10];
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(112, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x189, the right gain to the fourth ------------------------------------------------------
// acc = t[11] * r[122] - m[56]; t[11] takes it, so the right gain is the phase shift to the fourth
        case 0x189:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(122);
            c.psrc = c.src; c.src = m[56];
            m2 = xv;
            prod = g * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 11, false);
            break;
// ---- 0x18a to 0x18c, what the right cascade starts from ----------------------------------------
// acc = 2 * (k * r[108]) on the ROM's own zero word, so nothing
        case 0x18a:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// PH_WIDTH2[1]'s word: acc = k * r[121] + acc, the right sum through the width
        case 0x18b:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[113] = saturate(k * r[120] + acc) on the ROM's ~unity word: what the right cascade starts from
        case 0x18c:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(113, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x18d to 0x19b, the right cascade's five sections ----------------------------------------
// the class-3 step: acc = r[113], what the first section subtracts
        case 0x18d:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = t[11] * r[114] - acc, the right cascade's first section
        case 0x18e:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x63; r[114] = saturate(t[11] * r[113] + acc), its output
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(114, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the class-3 step: acc = r[114]
        case 0x190:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = t[11] * r[115] - acc, the second section
        case 0x191:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[115] = saturate(t[11] * r[114] - acc), its output
        case 0x192:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(115, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// a read of the slot 0x64; the class-3 step: acc = r[115]
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[31] takes up the read of 0x18f; acc = t[11] * r[116] - acc, the third section
        case 0x194:
            c.queue_d(31, c.captured(m[31], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[116] = saturate(t[11] * r[115] - acc), its output
        case 0x195:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(116, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the class-3 step: acc = r[116]
        case 0x196:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x65; acc = t[11] * r[117] - acc, the fourth section
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[32] takes up the read of 0x193; r[117] = saturate(t[11] * r[116] - acc), its output
        case 0x198:
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(117, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// the class-3 step: acc = r[117]
        case 0x199:
            k = yss236_core::s16(c.coef[st]);
            g = 1.0;
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// acc = t[11] * r[118] - acc, the fifth section
        case 0x19a:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[118] = saturate(t[11] * r[117] - acc), its output, which PH_STAGES[7] scales into the sum
        case 0x19b:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[11];
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            c.queue_r(118, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x19c to 0x19f, the dry and wet -----------------------------------------------------------
// m[33] takes up the read of 0x197; PH_PAN's wet word: acc = k * r[105], the left EQ output
        case 0x19c:
            c.queue_d(33, c.captured(m[33], st));
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
// PH_PAN's dry word: r[15] = saturate(4 * (k * r[119] + acc)), the left output
        case 0x19d:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// PH_PAN's wet word: acc = k * r[108], the right EQ output
        case 0x19e:
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
// PH_PAN's dry word: r[16] = saturate(4 * (k * r[121] + acc)), the right output
        case 0x19f:
            k = yss236_core::s16(c.coef[st]);
            g = k;
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv;
            prod = g * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        }
    }
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

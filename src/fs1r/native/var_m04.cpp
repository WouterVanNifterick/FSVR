// fs1r/native/var_m04.cpp - VOP3-2, the variation window's program for Auto Wah and Touch Wah: its 72 steps.
//
// rom::VARIATION_PROGRAMS[4], the program the window holds for the two variation types 11 and 12, and one
// of the twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 4 to
// Auto Wah and Touch Wah and to nothing else. The same program words run under both and only the constants
// move, so this native is one function for both types and neither is excluded. Which type is running is not
// a code difference here: fx_drive.cpp's autoWah() and touchWah() write the word lists below and leave the
// rest of the two types' own constants, which is why the Auto Wah's word at 0x169 is zero and the Touch
// Wah's is not, and why only autoWah() writes the depth word at 0x167 and only touchWah() the sensitivity
// word at 0x171.
//
//   WAH_FILTER      {0x15B, 0x159, 0x15A}       the left channel's low shelf: b0, b1, -a1
//   WAH_FILTER_2    {0x15E, 0x15C, 0x15D}       the left channel's high shelf, the same three
//   WAH_FILTER_3    {0x162, 0x160, 0x161}       the right channel's low shelf
//   WAH_FILTER_4    {0x165, 0x163, 0x164}       the right channel's high shelf
//   WAH_CUTOFF      {0x166}                     the cutoff
//   WAH_DEPTH       {0x167}                     the LFO depth on it: autoWah() writes it, touchWah()
//                                                does not, so the sweep is the Auto Wah's
//   WAH_SENSE       {0x170, 0x175}              the resonance: wahSense() writes one word into both
//   WAH_ATTACK      {0x171}                     the Touch Wah's sensitivity, autoWah() leaves it
//   WAH_HPF         {0x16D, 0x16B, 0x16C}       touchWah()'s input highpass, its words only
//   WAH_LPF         {0x181, 0x17F, 0x180}       touchWah()'s input lowpass, its words only
//   WAH_PAN         {0x185, 0x184, 0x187, 0x186} the dry and wet, interleaved: dry, wet, dry, wet
//
// fx_common.cpp's shelves() designs the four shelves as order-1 filters from EQ Low Freq/Gain and High
// Freq/Gain, so each is b0 * x[n] + b1 * x[n-1] - a1 * y[n-1] over one register's delay of the input and
// one of the output. fx_design.cpp's designFilter scales its words by 2048 * (16 >> scale), and the step
// carries 2^scale as its result route, which is why the low shelf's b0 sits on a step that doubles and the
// high shelf's on one that quadruples: scale 1 against highScale's 2.
//
// The 72 steps, in the groups the comments in the switch open:
//
//   0x158-0x165  the two channels' EQ, two order-1 shelves each. 0x158 takes the left input r[5] into
//                r[103] through the ROM's ~unity word and doubled, and 0x159 to 0x15b are the low shelf:
//                b1 times r[103] a pass back, -a1 times r[104], then b0 times r[103] and the sum
//                doubled into r[104]. 0x15c to 0x15e are the high shelf over r[104] into r[105]. 0x15f
//                to 0x165 are the same four steps for the right channel, r[6] into r[106] to r[108], on
//                the other channel's copies of the same words. 0x15b, 0x15f and 0x163 also read the delay
//                RAM, and 0x15c and 0x15d take the serial inputs into m[2] and m[6].
//   0x166-0x167  the cutoff the LFO sweeps. 0x166 is the class-1 load of WAH_CUTOFF's word, which
//                fx_common.h's wahCutoff() makes 3052 * v / 16 + 7837, so the running value runs from
//                about 30 to 128. 0x167 adds WAH_DEPTH's word times m[8], the window's first LFO, which
//                0x117 wrote just before this block, and publishes lfo[11] into m[11].
//   0x168        that cutoff into r[119] and t[9], with the ROM's own word at 0x168 scaling in r[113],
//                the lowpass state below: zero under Auto Wah, so r[119] is the cutoff itself, and
//                0x5000 under Touch Wah, whose lowpass carries signal.
//   0x169-0x16d  the highpass touchWah() designs, an order-1 section over r[109] into r[110] on the
//                words {0x16B, 0x16C, 0x16D}. 0x169 and 0x16a, which bring r[105] and r[108] into r[109],
//                are the two types' own words and the Auto Wah's are zero, so under it r[109] and r[110]
//                stay zero and this section is silent.
//   0x16e        the cutoff's one-pole: r[119] = t[9] * r[119], and t[9] takes the result, so the gain the
//                resonator below is swept by follows the cutoff.
//   0x16f-0x171  the resonator's input, left channel. 0x16f takes m[54] less the EQ output r[105].
//                0x170 adds WAH_SENSE's word times the state r[114] and saturates, into m[56]. 0x171 is
//                WAH_ATTACK's word times r[110] scaled by 16 and rectified, into m[58]: the Touch Wah's
//                sensitivity, on the highpass's output and zero under Auto Wah.
//   0x172-0x173  the second half of the smoother: 0x172 multiplies r[119] by t[9] and 0x173, another
//                class-1 load, adds its own word (128 * 0x0080 = 0.5 under Auto Wah), saturates and puts
//                the result back into t[9].
//   0x174-0x175  the same two steps as 0x16f and 0x170 for the right channel, over m[55], r[108] and
//                r[115], into m[57].
//   0x176-0x177  the rectified level's partner: the same running value with negatives clamped away, so
//                r[111] takes what m[58] does not hold.
//   0x178-0x179  the resonator's two accumulators: r[114] and r[115] take t[9] times their input cell
//                plus themselves, which with the resonance word above is the filter the wah sweeps.
//   0x17a-0x17e  the lowpass's input: four steps build the running value from r[112], r[111] and m[58] on
//                the ROM's own words and 0x17e writes r[112].
//   0x17f-0x181  the lowpass touchWah() designs, an order-1 section over r[112] into r[113] on the words
//                {0x17F, 0x180, 0x181}. It is the chain 0x168 reads, so under Touch Wah the cutoff goes
//                through it.
//   0x182-0x183  the resonator's feedback: m[54] and m[55] take t[9] times r[114] and r[115] plus
//                themselves, which is what 0x16f and 0x174 subtract from the input and closes the loop.
//   0x184-0x187  the dry and wet: WAH_PAN's four words. 0x184 and 0x186 scale r[114] and r[115] and
//                0x185 and 0x187 add the dry r[105] and r[108] and write the outputs r[15] and r[16].
//                dryWet() runs the variation blocks fully wet, so the dry words are zero and the two
//                outputs are the states at the window's gain.
//   0x188-0x19f  the tail. 0x188 to 0x18e, 0x190 to 0x192, 0x195, 0x196 and 0x199 to 0x19b are class 0
//                with no RAM mark and no ALU, so only the x latch runs. 0x18f reads the delay RAM and
//                publishes lfo[12] into m[12], 0x193 and 0x197 read, and 0x194, 0x198 and 0x19c publish
//                the three reads five steps before them.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x1f2 write
// the first, 0x08d the second) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005).
// r[103] to r[105] the left channel's EQ: input, low shelf, high shelf; r[106] to r[108] the right
// channel's. r[109] and r[110] the highpass, r[112] and r[113] the lowpass, r[114] and r[115] the
// resonator's two states, r[119] the cutoff and then the smoother's state, r[111] the rectified level's
// partner. The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[8] the
// window's first LFO, m[11] and m[12] the two LFO values it publishes, m[28] to m[33] the six published
// delay RAM reads, m[54] and m[55] the resonator's two feedback accumulators, m[56] and m[57] its two
// inputs and m[58] the sensitivity level. The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, read at
// the ring pointer with no index; this program writes none of them, so the lines behind these taps are
// filled elsewhere. Each capture takes the read five steps before it (captured()'s lower bound), so
// m[28] carries the slot 0x56 word, m[29] the slot 0x57 word, m[30] the slot 0x58 word, and m[31], m[32]
// and m[33] the slots 0x63, 0x64 and 0x65 words.
//
// The one t register the program uses is t[9], written at 0x168, 0x16e and 0x173 and read at 0x16e, 0x172,
// 0x178, 0x179, 0x182 and 0x183: the cutoff, then the smoother's gain, then the resonator's gain.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m04: identical, 2 types, 19200 native passes.

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
        // r[103] = saturate(2 * k * r[5]), the left input through the ROM's ~unity word and doubled
        case 0x158:
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(103, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // the left low shelf's b1: k * r[103], the input a pass back
        case 0x159:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the left low shelf's -a1: k * r[104] + acc, its output a pass back
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // a read of the slot 0x56; the left low shelf's b0: r[104] = saturate(2 * (k * r[103] + acc))
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // m[2] takes the left serial input; the left high shelf's b1: k * r[104]
        case 0x15c:
            c.queue_d(2, c.in[0][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // m[6] takes the right serial input; the left high shelf's -a1: k * r[105] + acc
        case 0x15d:
            c.queue_d(6, c.in[1][10]);
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the left high shelf's b0: r[105] = saturate(4 * (k * r[104] + acc)), the left EQ output
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // a read of the slot 0x57; r[106] = saturate(2 * k * r[6]), the right input doubled
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // m[28] takes up the read of 0x15b; the right low shelf's b1: k * r[106]
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the right low shelf's -a1: k * r[107] + acc
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the right low shelf's b0: r[107] = saturate(2 * (k * r[106] + acc))
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // a read of the slot 0x58; the right high shelf's b1: k * r[107]
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // m[29] takes up the read of 0x15f; the right high shelf's -a1: k * r[108] + acc
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the right high shelf's b0: r[108] = saturate(4 * (k * r[107] + acc)), the right EQ output
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x166 to 0x167, the cutoff the LFO sweeps ----------------------------------------------
        // the class-1 load of WAH_CUTOFF's word: acc = 128 * k, and nothing else
        case 0x166:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = 0.0;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 0);
            break;
        // m[11] takes the LFO of this page; WAH_DEPTH's word times m[8] on top of the cutoff
        case 0x167:
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[8];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // ---- 0x168, the cutoff into r[119] and t[9] ------------------------------------------------
        // m[30] takes up the read of 0x163; r[119] = saturate(k * r[113] + acc); t[9] takes it
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            twr(y, 9, false);
            break;
        // ---- 0x169 to 0x16d, the highpass touchWah() designs ---------------------------------------
        // the left EQ output into the running value; zero under Auto Wah, whose word here is zero
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[109] = saturate(k * r[108] + acc), the highpass's input from the right EQ output
        case 0x16a:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // the highpass's b1: k * r[109]
        case 0x16b:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the highpass's -a1: k * r[110] + acc
        case 0x16c:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the highpass's b0: r[110] = saturate(2 * (k * r[109] + acc))
        case 0x16d:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(110, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x16e, the cutoff's one-pole ----------------------------------------------------------
        // r[119] = wrap(t[9] * r[119]); t[9] takes the result, so the swept gain follows the cutoff
        case 0x16e:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 0));
            y = c.acc = yss236_core::accmode(y, 0);
            twr(y, 9, false);
            break;
        // ---- 0x16f to 0x171, the resonator's input, left channel ------------------------------------
        // acc = k * m[54] - r[105], the feedback accumulator less the left EQ output
        case 0x16f:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = xv;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // WAH_SENSE's word: acc = saturate(k * r[114] - acc); m[56] takes it, the resonator's input
        case 0x170:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(56, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // WAH_ATTACK's word: acc = |16 * k * r[110]|; m[58] takes it, the sensitivity level
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 16.0;
            y = c.acc = yss236_core::accmode(y, 3);
            c.queue_d(58, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x172 to 0x173, the second half of the smoother ----------------------------------------
        // acc = t[9] * r[119]
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = g * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the class-1 load: acc = saturate(128 * k + acc); t[9] takes it
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 1);
            twr(y, 9, false);
            break;
        // ---- 0x174 to 0x175, the same two steps for the right channel ------------------------------
        // acc = k * m[55] - r[108], the feedback accumulator less the right EQ output
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = xv;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // WAH_SENSE's word again: acc = saturate(k * r[115] - acc); m[57] takes it
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(57, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x176 to 0x177, the sensitivity level's partner ----------------------------------------
        // acc = max(0, k * r[111] - m[58]): the same value with negatives clamped away
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[58];
            m2 = xv; prod = k * m2;
            a = c.src;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        // r[111] = saturate(k * m[58] + acc)
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(111, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x178 to 0x179, the resonator's accumulators ------------------------------------------
        // r[114] = saturate(t[9] * m[56] + r[114]), the left channel's state and output
        case 0x178:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(114);
            c.psrc = c.src; c.src = m[56];
            m2 = c.src; prod = g * m2;
            a = xv;
            y = (prod + a) * 1.0;
            c.queue_r(114, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // r[115] = saturate(t[9] * m[57] + r[115]), the right channel's
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(115);
            c.psrc = c.src; c.src = m[57];
            m2 = c.src; prod = g * m2;
            a = xv;
            y = (prod + a) * 1.0;
            c.queue_r(115, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x17a to 0x17e, the lowpass's input ---------------------------------------------------
        // acc = k * r[112]
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[112] + acc / 32768, the m cell this step names is zero so the adder takes p / 32768
        case 0x17b:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc / 32768.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * m[58] - acc
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = max(0, k * r[111] - acc)
        case 0x17d:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        // r[112] = saturate(k * m[58] + acc), the lowpass's input
        case 0x17e:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(112, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x17f to 0x181, the lowpass touchWah() designs ----------------------------------------
        // the lowpass's b1: k * r[112]
        case 0x17f:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the lowpass's -a1: k * r[113] + acc
        case 0x180:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the lowpass's b0: r[113] = saturate(k * r[112] + acc), which 0x168 reads back
        case 0x181:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(113, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x182 to 0x183, the resonator's feedback ----------------------------------------------
        // acc = saturate(t[9] * r[114] + m[54]); m[54] takes it, what 0x16f subtracts
        case 0x182:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(114);
            c.psrc = c.src; c.src = m[54];
            m2 = xv; prod = g * m2;
            a = c.src;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // acc = saturate(t[9] * r[115] + m[55]); m[55] takes it, what 0x174 subtracts
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            g = c.t[9];
            fetch(115);
            c.psrc = c.src; c.src = m[55];
            m2 = xv; prod = g * m2;
            a = c.src;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
        // ---- 0x184 to 0x187, the dry and wet ------------------------------------------------------
        // WAH_PAN's wet word: acc = k * r[114]
        case 0x184:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the dry word, zero for this window: r[15] = saturate(4 * (k * r[105] + acc)), the left output
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // WAH_PAN's wet word: acc = k * r[115]
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // the dry word: r[16] = saturate(4 * (k * r[108] + acc)), the right output
        case 0x187:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
        // ---- 0x188 to 0x19f, the tail -------------------------------------------------------------
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x188:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x189:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x18a:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x18b:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x18c:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x18d:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x18e:
            fetch(0);
            break;
        // a read of the slot 0x63; m[12] takes the LFO of this page
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x190:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x191:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x192:
            fetch(0);
            break;
        // a read of the slot 0x64; class 0, no ALU, so only the x latch
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            fetch(0);
            break;
        // m[31] takes up the read of 0x18f; class 0, no ALU, so only the x latch
        case 0x194:
            c.queue_d(31, c.captured(m[31], st));
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x195:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x196:
            fetch(0);
            break;
        // a read of the slot 0x65; class 0, no ALU, so only the x latch
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            fetch(0);
            break;
        // m[32] takes up the read of 0x193; class 0, no ALU, so only the x latch
        case 0x198:
            c.queue_d(32, c.captured(m[32], st));
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x199:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19a:
            fetch(0);
            break;
        // the idle step: class 0, no RAM mark and no ALU, so only the x latch
        case 0x19b:
            fetch(0);
            break;
        // m[33] takes up the read of 0x197; class 0, no ALU, so only the x latch
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
void var_m04(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
// fs1r/native/var_m07.cpp - VOP3-2, the variation window's program for 3-Band EQ: its 72 steps.
//
// rom::VARIATION_PROGRAMS[13], the program the window holds for the one variation type 13, and one of the
// twelve programs those twenty-nine types share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 13 to
// 3-Band EQ and to nothing else.
//
//   EQ3_LOW_SHELF2         {0x15E,0x15A,0x15B,0x15C,0x15D}  the left channel's low band: b0, b1, b2, -a1, -a2
//   EQ3_PEAK               {0x163,0x15F,0x160,0x161,0x162}  the left channel's middle band, the same five
//   EQ3_HIGH_SHELF2        {0x168,0x164,0x165,0x166,0x167}  the left channel's high band, the same five
//   EQ3_LOW_SHELF2_COPY    {0x16E,0x16A,0x16B,0x16C,0x16D}  the right channel's low band
//   EQ3_PEAK_COPY          {0x173,0x16F,0x170,0x171,0x172}  the right channel's middle band
//   EQ3_HIGH_SHELF2_COPY   {0x178,0x174,0x175,0x176,0x177}  the right channel's high band
//   EQ3_A                  {0x158,0x159,0x169}               the two words that sum the left input, and the right one
//   EQ3_B                  {0x179,0x17A,0x17B}               the two outputs and the word between them
//
// fx_tone.cpp's threeBandEq() designs each band with designFilter(2, 1, ...) for the two lower bands and
// designFilter(2, 4, ...) for the high one, which writes the five words b0, b1, b2, -a1 and -a2 into
// the list above and copies them to the other channel's list. Each word is the designed coefficient
// times 2048 * (16 >> scale), which the chip's s16() halves, and the b0 step's own route multiplies the
// whole section it closes, so the five words stand for the coefficients as designed: a shift of one on
// 0x15E, 0x163, 0x16E and 0x173, and a shift of three on 0x168 and 0x178, both restore the factor the
// designer's scale took out. What the section computes is then the difference equation
//
//   y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] + (-a1) y[n-1] + (-a2) y[n-2]
//
// one order-2 section per band, each on the previous band's output: three cascaded sections per channel,
// and nothing else in the program.
//
// The type's own preset holds all three gains at 0 dB, and then each band's numerator word pair is the
// same polynomial as its denominator pair: the left low band 1.0000, -1.949341, +0.950012 against the
// same 1.949341 and -0.950012 on its own output, the middle 1.0000, -1.952026, +0.971985, and the high
// 1.0000, -0.437500, +0.047852 (all off the dump's k values with each band's route applied). Each
// section is therefore all-pass, the cascade is transparent, and the window passes the summed input
// through unchanged until a band is moved. INFERRED from those words, not measured.
//
// The MODE parameter (index 14) is the stereo switch, and it is the only one that touches EQ3_A and
// EQ3_B. In mono it puts a quarter on both of the summing words and leaves the right output's word at
// zero; in stereo it halves the first, zeroes the second and gives the right output its share:
//
//   stereo  coef[0x158] = HALF     coef[0x159] = 0        coef[0x169] = HALF
//           r[103] = 2 * (0 * R + HALF * L) = L, r[111] = 2 * HALF * R = R,
//           coef[0x179] = x  coef[0x17A] = 0     coef[0x17B] = x
//           r[15] = 4 x r[109] and r[16] = 4 (x r[117] + 0), one chain each
//   mono    coef[0x158] = QUARTER  coef[0x159] = QUARTER  coef[0x169] = HALF
//           r[103] = 2 * (0.25 R + 0.25 L) = (L + R) / 2, r[111] = 2 * 0.5 * R = R,
//           coef[0x179] = x  coef[0x17A] = x     coef[0x17B] = 0
//           r[15] = 4 x r[109] and r[16] = 4 (0 + x r[109]), so both outputs are the left chain's
//
// and x is typeGain(VARIATION, QUARTER), the type's own level, the same word on both output steps. So in
// mono the right chain still runs and its output register r[117] is multiplied by zero. The type's own
// preset holds the mono words (QUARTER, QUARTER, HALF at 0x158, 0x159, 0x169, and zero at 0x17B), which
// is what the checker runs it under.
//
// The 72 steps, in the groups the comments in the switch open:
//
//   0x158-0x159  EQ3_A. m[2] and m[6] take the serial inputs for the base program, and the two words
//                sum the left input into r[103]: the first into the running value, the second doubling
//                that plus a quarter of the right one. 0x158 has no adder input and no route, so it is
//                the left input alone into the accumulator.
//   0x15a-0x15e  the left low band. 0x15a is b1 on the input a pass back, 0x15b b2 on the copy of that
//                input one pass older still and the read of the slot 0x56 the base program published,
//                0x15c and 0x15d -a1 and -a2 on the band's own output a pass and two passes back, and
//                0x15e b0 on the current input, doubled and saturating, into r[105]. The copy the b2
//                step needs is r[104]: 0x15b is an rcopy step, so it takes the value 0x15a latched.
//   0x15f-0x163  the left middle band, the same five steps on EQ3_PEAK: b1 on the low band's output a
//                pass back, b2 on r[106] (the copy 0x160's rcopy leaves, which is that same value one
//                pass older when 0x160 reads it) and the read of the slot 0x57, -a1 and -a2 on the
//                middle band's own output in r[107] and r[108], and b0 into r[107]. m[28] takes up the
//                read of 0x15b, the slot 0x56.
//   0x164-0x168  the left high band on EQ3_HIGH_SHELF2, five steps on r[109], r[110] and the input
//                register of the middle band. m[29] takes up the read of 0x15f and m[30] that of 0x163,
//                the slot 0x58. 0x166 also publishes the LFO of its page into m[11].
//   0x169-0x16e  the right channel: EQ3_A[2] takes the right input into r[111] with no adder input, so
//                it is the right input doubled, and EQ3_LOW_SHELF2_COPY is the left low band again on
//                r[112], r[113] and r[114].
//   0x16f-0x173  the right middle band on EQ3_PEAK_COPY, into r[115], with the copy 0x170 leaves in
//                r[114] for its b2 step.
//   0x174-0x178  the right high band on EQ3_HIGH_SHELF2_COPY, into r[117], with the copy 0x175 leaves
//                in r[116] and 0x177's in r[118].
//   0x179-0x17b  EQ3_B. 0x179 quadruples its word on r[109] into the left output r[15] with no adder
//                input, 0x17a leaves the same word on r[109] in the accumulator without a route, and
//                0x17b adds its own word on r[117] to that and quadruples the sum into r[16].
//   0x17c-0x19f  the tail, class 0 throughout and idle but for the three reads and the four published
//                cells: the slots 0x63, 0x64 and 0x65 on 0x18f, 0x193 and 0x197, and the captures at
//                0x194, 0x198 and 0x19c that take them into m[31], m[32] and m[33], each five steps
//                later, which is captured()'s lower bound. 0x186 publishes the LFO of its page into
//                m[12].
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window (0x096 and 0x08d
// write them) and r[15] and r[16] the two outputs it reads back (0x004 and 0x005). r[103] the summed
// left input, r[105] the left low band, r[107] the left middle band, r[109] the left high band and the
// left output; r[111] the right input, r[113] the right low band, r[115] the right middle band, r[117]
// the right high band and the right output. Four registers per channel hold the section delays, each one
// written by an rcopy step that copies the value the step before it latched, and read by the b2 step of
// the same section on the next pass:
//
//   r[104] the summed input a pass older, the left low band's b2; r[112] the right input a pass older,
//   the right low band's b2. r[106] the left low band's output a pass older, the left middle band's b2,
//   and two passes older it is that band's -a2; r[114] the same two on the right. r[108] the left
//   middle band's output a pass older, the left high band's b2, and its -a2 two passes back; r[116]
//   the same on the right. r[110] and r[118] the two high bands' outputs a pass older, their own b2.
//
// The cells: m[2] and m[6] the serial inputs this block takes for the base program, m[11] and m[12] the
// two LFOs the window publishes, m[28] to m[33] the six published delay reads, in step order: the slots
// 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65.
// The delay slots: 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65, all read at the ring pointer, which is where
// the base program's INIT_DELAY_* offsets put them. Nothing here writes the delay RAM and nothing reads
// at a delay index, so no band is a delay line and no step in the program is a tap: the variation
// window's 3-Band EQ is three cascaded order-2 sections per channel and no memory at all.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below in the
// order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m07: identical, 1 type, 9600 native passes.

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
    double xv, k, prod, a, y;

    // The step's x operand: the register r[sr] into the latch every step but a class-2/3 one moves, and
    // xv alongside it for the multiplier.
    auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };

    for (int st = 0x158; st < 0x1a0; st++)
    {
        c.begin_step();
        if (st < c.m_skip) continue;
        switch (st)
        {
// ---- 0x158 to 0x159, EQ3_A: the serial inputs and the summed left one ------------------------
// m[2] takes the left serial input; acc = k * r[5], the left input into the running value alone
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
// m[6] takes the right serial input; r[103] = saturate(2 * (k * r[6] + acc)), the summed left input
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
// ---- 0x15a to 0x15e, the left low band on EQ3_LOW_SHELF2 -------------------------------------
// b1 on the summed input a pass back
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// a read of the slot 0x56; b2 on r[104], which is this step's own copy of the value 0x15a latched, the
// summed input two passes back, and the running value, so the b1 term comes along
        case 0x15b:
            c.push_hist(st, c.ram[c.addr(0x56, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(104, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a1 on the band's own output a pass back, r[105]
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a2 on that output two passes back, r[106]
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b0 on the summed input itself, doubled and saturating, into r[105]: the band's output
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(105, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x15f to 0x163, the left middle band on EQ3_PEAK ----------------------------------------
// b1 on the low band's output a pass back
        case 0x15f:
            c.push_hist(st, c.ram[c.addr(0x57, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[28] takes up the read of 0x15b; b2 on r[106], the low band's output two passes back, and this
// step's own copy of the value 0x15f latched, which is that output a pass back. Both 0x15d and this
// step read r[106] on the next pass, as that band's -a2 and this one's b2.
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(106, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a1 on the middle band's output a pass back, r[107]
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a2 on that output two passes back, r[108]
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b0 on the low band's output itself, doubled and saturating, into r[107]
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x164 to 0x168, the left high band on EQ3_HIGH_SHELF2 ------------------------------------
// m[29] takes up the read of 0x15f; b1 on the middle band's output a pass back
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[108], the middle band's output two passes back, and this step's own copy of the value 0x164
// latched, which is that output a pass back, into r[108]
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(108, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[11] takes the LFO of this step's page; -a1 on the high band's output a pass back, r[109]
        case 0x166:
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[110], the high band's output two passes back, and this step's copy of the value 0x166
// latched, which is that output a pass back, into r[110]
        case 0x167:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(110, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// m[30] takes up the read of 0x163; b0 on the middle band's output itself, sixteened and
// saturating, into r[109]: the left channel's output of the whole EQ
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x169 to 0x16e, the right channel's input and low band on EQ3_LOW_SHELF2_COPY -------------
// EQ3_A[2]: r[111] = saturate(2 * k * r[6]), the right input doubled with no adder input
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(111, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// b1 on the right input a pass back
        case 0x16a:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[112], the copy of the right input, and the running value; the copy goes into r[112]
        case 0x16b:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(112, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a1 on the right low band's output a pass back, r[113]
        case 0x16c:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a2 on that output two passes back, r[114]
        case 0x16d:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b0 on the right input itself, doubled and saturating, into r[113]
        case 0x16e:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(113, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x16f to 0x173, the right middle band on EQ3_PEAK_COPY -------------------------------------
// b1 on the right low band's output a pass back
        case 0x16f:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[114], the right low band's output two passes back, and this step's copy of the value 0x16f
// latched, which is that output a pass back, into r[114]
        case 0x170:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(114, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a1 on the right middle band's output a pass back, r[115]
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a2 on that output two passes back, r[116]
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b0 on the right low band's output itself, doubled and saturating, into r[115]
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(115, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x174 to 0x178, the right high band on EQ3_HIGH_SHELF2_COPY ------------------------------
// b1 on the right middle band's output a pass back
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[116], the right middle band's output two passes back, and this step's copy of the value
// 0x174 latched, which is that output a pass back, into r[116]
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(116, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// -a1 on the right high band's output a pass back, r[117]
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b2 on r[118], the right high band's output two passes back, and this step's copy of the value 0x176
// latched, which is that output a pass back, into r[118]
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(118, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// b0 on the right middle band's output itself, sixteened and saturating, into r[117]: the right
// channel's output of the whole EQ
        case 0x178:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 16.0;
            c.queue_r(117, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x179 to 0x17b, EQ3_B: the two outputs ---------------------------------------------------
// r[15] = saturate(4 * k * r[109]), the left channel's high band alone, no adder input
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// acc = k * r[109], the same word with no route and no register: what 0x17b adds its own word to
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
// r[16] = saturate(4 * (k * r[117] + acc)), so the right output is that word on the right high band
// plus whatever 0x17a left: the left one in mono, nothing in stereo
        case 0x17b:
            k = yss236_core::s16(c.coef[st]);
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            break;
// ---- 0x17c to 0x19f, the tail: three published reads, four published cells, the rest idle -------
// the idle steps: class 0, no RAM mark and no ALU, so only the x latch
        case 0x17c: case 0x17d: case 0x17e: case 0x17f: case 0x180: case 0x181: case 0x182:
        case 0x183: case 0x184: case 0x185:
            fetch(0);
            break;
// m[12] takes the LFO of this step's page
        case 0x186:
            c.queue_d(12, c.lfo[12]);
            fetch(0);
            break;
        case 0x187: case 0x188: case 0x189: case 0x18a: case 0x18b: case 0x18c: case 0x18d:
        case 0x18e:
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
void var_m07(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
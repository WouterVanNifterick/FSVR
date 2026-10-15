// fs1r/native/var_m10.cpp - VOP3-2, the variation window's program for the reverbs: its 72 steps.
//
// rom::VARIATION_PROGRAMS[10], the program the variation window holds under the four variation types
// 25 to 28, which fx_dispatch.cpp routes to EffectHost::variationReverb(): Hall, Room, Stage and Plate.
// It is one of the twenty-nine programs the window shares, so nothing here knows which of the four is
// running. The program words are fixed and only the constants and the delay offsets move, which
// fx_reverb.cpp writes whenever a parameter does, so the code reads them where they live on the pass,
// as step() reads them, because that is the contract and not because the numbers are known here.
//
// What the 72 steps compute, in the groups the comments below open each of. The host names every group
// it programs, in EffectHost::variationReverb() and the tables it calls:
//   var::REVERB_HPF {0x15C,0x15B,0x15A}, var::REVERB_LPF {0x15F,0x15E,0x15D}
//   var::REVERB_ROW_STEPS {0x178,0x179,0x17A,0x17B,0x170,0x171,0x172,0x173}, var::REVERB_ROW_SLOTS {0x60,0x5B,0x5C,0x59}
//   var::REVERB_PRE {0x5D,0x5A}, var::REVERB_WIDTH {0x177}
//   var::REVERB_COMBS {0x19B,0x186,0x185,0x184,0x19F,0x189,0x188,0x187}
//   var::REVERB_ER_IDS {0x66,0x5E,0x61,0x67,0x5F,0x62}
//   var::REVERB_W {0x191,0x160,0x161,0x162,0x163,0x164,0x192,0x193,0x18F,0x18E}
//   var::REVERB_PAN {0x195,0x197,0x196,0x198}
// and the two 72-step halves run as:
//
//   0x158-0x15f  the two serial inputs, the mono sum and the host's two tone filters, the high-pass
//                var::REVERB_HPF and the low-pass var::REVERB_LPF, one pole each and three words apiece.
//                0x159 is the mono sum r[103] = 2 * (k * r[6] + k * r[5]), so the whole reverb runs on
//                the mono. The high-pass's state is r[104], the low-pass's r[105], the low-pass fed from
//                the high-pass. The reads of the slots 0x56 and 0x57 ride on the two middle words of the
//                filters and are published in m[28] and m[29].
//   0x160-0x164  m[28] takes up the read of 0x15b, the read of the slot 0x58 rides on 0x163, and m[29]
//                takes up the read of 0x15f. 0x164 is var::REVERB_W[5]: r[119] = k * r[118], the level
//                the diffusion parameter picks, on the base program's own return r[118].
//   0x165-0x166  r[109] = 2 * (k * m[55] + k * r[105]), the pre-delay tap published for the base
//                program, and 0x166 passes it on. m[55] is m[57] as 0x191 left it a pass back.
//   0x167-0x16e  nine rcopy steps that carry no arithmetic, only the z^-1 copies r[110] to r[117]. They
//                hold the reads of the slots 0x59 and 0x5A, publish m[30] (the read of 0x58) and m[56]
//                (the read of 0x59), and take the page's LFO into m[11].
//   0x16f-0x177  the second diffusion unit, var::REVERB_ROW_STEPS[4..7] over the slot 0x5C, and the
//                pre-delay. 0x16f reads the slot 0x5B and m[58] takes it up five steps later; 0x170
//                to 0x173 are the unit's four words, and 0x173 writes the slot 0x5C itself. 0x177 is
//                var::REVERB_WIDTH[0], the FB_LEVEL width: the slot 0x5D takes the unit's tap r[120]
//                mixed with the pre-delay tap m[57] behind it.
//   0x178-0x183  the first diffusion unit, var::REVERB_ROW_STEPS[0..3], driven by the pre-delay tap
//                m[57] rather than by the input. 0x179 publishes the unit's tap in m[63], 0x17b latches
//                the line's own write and 0x183 spends it on the slot 0x60, the unit's write. The read
//                of the slot 0x5E rides on 0x17b.
//   0x184-0x189  the two damping lowpasses, the (x, x, a) triples var::REVERB_COMBS[1..3] and
//                [5..7]: r[106] off r[107], and r[108] off the early reflection m[60]. 0x187 reads the
//                slot 0x61 and takes the page's LFO into m[12].
//   0x18a-0x18d  m[61] takes up the read of the slot 0x61, five steps after 0x187. 0x18a is a dead step:
//                its M_PREVIOUS write of m[54] is on a class-0 step, which step() returns before, so
//                nothing ever writes m[54] and 0x188 reads it as zero.
//   0x18e-0x193  the two output sums, var::REVERB_W[9] and [8] into r[120] and var::REVERB_W[6] and [7]
//                into r[121], each summing two early reflections and one level term. 0x191 is
//                var::REVERB_W[0] and copies m[57] into m[55], so m[55] is the pre-delay tap a pass
//                back; 0x190 publishes the read of the slot 0x62 in m[62].
//   0x194-0x198  m[31], m[32] and m[33] take up the reads of the slots 0x63, 0x64 and 0x65 for the base
//                program and the other windows, and the pan var::REVERB_PAN scales the two sums into
//                r[15] and r[16]: r[15] = 4 * (k * r[121] + k * r[5]) and r[16] = 4 * (k * r[120] + k * r[6]).
//                At the shipped constants k is a quarter and the two inputs carry nothing, so each
//                output is its own sum.
//   0x199-0x19f  the two comb gains, var::REVERB_COMBS[0] and [4]: the slot 0x66 takes the unit's tap
//                m[63] times its gain plus the first damper's state r[106], the slot 0x67 the same tap
//                plus the second damper's r[108]. m[33] takes up the read of the slot 0x65 on the way past.
//                The slots 0x66 and 0x67 are the bases the early reflections hang off, so combDelays2()
//                sets 0x5E and 0x61 from 0x66 and 0x5F and 0x62 from 0x67, each pair the same offset and
//                the second shorter by the diffusion.
//
// The two diffusion units are what EffectHost::diffusers() programs, and each is the same four words in
// the same roles: the row's two feedforward words and its tap gain and its feedback, which the host
// calls s28, s26, s24 and s22 and writes to unit[0..3]. With k the word the step carries, a unit whose
// line is read from slot L and whose input is x computes:
//
//   tap        0x179 / 0x171   = 2 * (s24 * line(L) + s28 * x), saturating at +-128, into m[63] / r[120]
//   write      0x17b / 0x173   = clip(2 * (s22 * line(L) + s26 * x), +-32), into the unit's write slot
//   with x = m[57] and L = the slot 0x5B for the first unit, and x = r[105] and L = the slot 0x59 for
//   the second. The two damping lowpasses are one poles each, y = 2 * (x * in + a * y_prev) over three
//   words apiece, and the two comb gains are the RT60 gains the damping loss scales.
//
// Every register a step reads is read a pass back or a few steps earlier, as the latency of three steps
// allows, which is why the sums above read r[120] where the reader stands before the writer's write has
// landed. The old model in vop3_effects.h's run_delay() has a single Schroeder tail with eight combs and
// four all-passes for every reverb, and is no reference for any number or for any shape here: this program
// runs two units, one driven by the other's tap through the initial delay, and four early reflections.
//
// Two cells this program never fills, which is worth knowing before reading the steps that read them.
// m[59] is captured at 0x180, and captured() takes the latest delay RAM read made five to eleven steps
// earlier: the window is steps 373 to 379 and the read at 0x17f is step 383, four too late, so the
// capture returns m[59]'s own value and the cell stays at zero. 0x183 reads it, so the first damping
// lowpass's input r[107] is zero and r[106] stays at zero too. m[54] is written only by 0x18a, whose
// M_PREVIOUS is on a class-0 step, so it stays at zero as well and 0x188's term on it is zero. Both are
// transcriptions of step() as it stands rather than simplifications of them.
//
// The registers: r[5] and r[6] the two inputs the base program leaves this window and r[15] and r[16]
// the two outputs it reads back (0x004 and 0x005); r[103] the mono input; r[104] and r[105] the two
// tone-filter poles; r[106], r[107] and r[108] the two damping lowpasses and the first one's input;
// r[109] the pre-delay return this window hands the base program; r[118] the base program's own return
// and r[119] its level; r[120] the second unit's tap and the right output sum, r[121] the left one. The
// cells: m[2] and m[6] the two serial inputs this block takes, m[11] and m[12] the two LFO values the
// window publishes, m[28] to m[30] and m[31] to m[33] the six reads the base program and the other
// windows share, m[54] the cell nothing writes, m[55] the pre-delay tap a pass back, m[56] and m[58] the
// two units' lines, m[57] the pre-delay tap, m[59] the cell that stays at zero, m[60] to m[62] the three
// early reflections the outputs sum, and m[63] the first unit's tap. The delay slots: 0x5C and 0x60 the
// two units' write slots, 0x59 and 0x5B their read slots, 0x5D and 0x5A the pre-delay's write and read,
// 0x66 and 0x67 the early reflections' two base offsets with 0x5E, 0x61, 0x5F and 0x62 the four taps off
// them, and 0x56, 0x57, 0x58, 0x63, 0x64 and 0x65 the reads the base program shares. No step here reads
// at a delay index, none writes the t file with its own result and none sets the compare flag, so no
// step of this program is fractional, no step takes a multiplier from a t register and no step branches.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order yss236_core::step() runs its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m10: identical, 45 types, 432000 native passes. The 45 are the four variation
// types 25 to 28 and the 41 insertion types the sweep visits while the window still holds this program.

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
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// m[6] takes the right serial input; r[103] = saturate(2 * (k * r[6] + k * r[5])), the mono sum the
// whole reverb runs on
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
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_HPF[2]: the high-pass pole's -a1 on its own state, r[104]
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_HPF[1]: b1 on the mono, onto that pole term, and the read of the slot 0x56 the base
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
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_HPF[0]: b0 on the mono into r[104], saturating
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(104, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_LPF[2]: the low-pass pole's -a1 on r[105]
        case 0x15d:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_LPF[1]: b1 on the high-pass's output, r[104]
        case 0x15e:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_LPF[0]: b0 on r[104], doubled and saturating, into r[105], and the read of the slot 0x57
// the base program published (published in m[29] at 0x164)
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
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x160 to 0x164, the two published reads and the base program's return under a level ----------
// m[28] takes up the read of 0x15b, the slot 0x56
        case 0x160:
            c.queue_d(28, c.captured(m[28], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// the read of the slot 0x58, which m[30] takes up at 0x168
        case 0x163:
            c.push_hist(st, c.ram[c.addr(0x58, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// m[29] takes up the read of 0x15f, the slot 0x57; var::REVERB_W[5]: the level the diffusion picks, on
// the base program's own return r[118], into r[119]
        case 0x164:
            c.queue_d(29, c.captured(m[29], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(118);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            c.queue_r(119, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x165 to 0x16e, the pre-delay return and the rcopy run that publishes three reads -------------
// the pre-delay tap m[55] and the filtered mono into r[109], the base program's own return of the
// initial delay
        case 0x165:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(109, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// class 3: r[109] passes through onto the running value
        case 0x166:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// the read of the slot 0x59, which m[56] takes up at 0x16c, and the page's LFO into m[11]
        case 0x167:
            c.push_hist(st, c.ram[c.addr(0x59, 0, false)]);
            c.queue_d(11, c.lfo[11]);
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(110, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// m[30] takes up the read of 0x163, the slot 0x58
        case 0x168:
            c.queue_d(30, c.captured(m[30], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(111, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(112, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x16a:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(113, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// the read of the slot 0x5A, which m[57] takes up at 0x170: the pre-delay
        case 0x16b:
            c.push_hist(st, c.ram[c.addr(0x5A, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(114, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// m[56] takes up the read of 0x167, the slot 0x59: the second unit's line
        case 0x16c:
            c.queue_d(56, c.captured(m[56], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(115, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x16d:
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(116, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
        case 0x16e:
            k = yss236_core::s16(c.coef[st]);
            fetch(117);
            c.psrc = c.src; c.src = m[0];
            prod = xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            c.queue_r(117, c.haspx ? c.px : xv);
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x16f to 0x177, the second diffusion unit and the pre-delay ----------------------------------
// the read of the slot 0x5B, which m[58] takes up at 0x174: the first unit's line
        case 0x16f:
            c.push_hist(st, c.ram[c.addr(0x5B, 0, false)]);
            fetch(0);
            break;
// m[57] takes up the read of 0x16b, the slot 0x5A: the pre-delay tap; var::REVERB_ROW_STEPS[4]: the
// unit's s28, the filtered mono behind a minus
        case 0x170:
            c.queue_d(57, c.captured(m[57], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_ROW_STEPS[5]: the unit's s24, the line's tap gain onto that term, doubling and
// saturating, into r[120]
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_ROW_STEPS[6]: the unit's s26, the filtered mono again, this time in front
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_ROW_STEPS[7]: the unit's s22, the line's feedback, into the slot 0x5C the unit writes.
// This step carries w1[13] and is itself a writer, so it writes its own result and takes no latch.
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[56];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x5C, 0, false)] = outv;
            break;
// m[58] takes up the read of 0x16f, the slot 0x5B
        case 0x174:
            c.queue_d(58, c.captured(m[58], st));
            fetch(0);
            break;
        case 0x175:
            fetch(0);
            break;
        case 0x176:
            fetch(0);
            break;
// var::REVERB_WIDTH[0]: the FB_LEVEL width. The slot 0x5D, which var::REVERB_PRE[0] is, takes the
// unit's tap r[120] mixed with the pre-delay tap m[57] behind it, saturating. Carries w1[13] and is a
// writer, so it writes its own result.
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[57];
            prod = k * c.src;
            a = c.x;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x5D, 0, false)] = outv;
            break;
// ---- 0x178 to 0x183, the first diffusion unit, driven by the pre-delay -----------------------------
// var::REVERB_ROW_STEPS[0]: the unit's s28, the pre-delay tap behind a minus
        case 0x178:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_ROW_STEPS[1]: the unit's s24, the line's tap gain onto that term, doubling and
// saturating. The result is the unit's tap, which the two comb gains below weigh, so it goes out as a
// cell: m[63].
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.queue_d(63, yss236_core::clip(y, yss236_core::DSAT));
            break;
// var::REVERB_ROW_STEPS[2]: the unit's s26, the pre-delay tap in front
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_ROW_STEPS[3]: the unit's s22, the line's feedback. This step latches its result and
// 0x183 spends it, and the read of the slot 0x5E rides on it.
        case 0x17b:
            c.push_hist(st, c.ram[c.addr(0x5E, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[58];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.wlatch = outv;
            c.haswl = true;
            break;
        case 0x17c:
            fetch(0);
            break;
        case 0x17d:
            fetch(0);
            break;
        case 0x17e:
            fetch(0);
            break;
// the read of the slot 0x5F, which m[60] takes up at 0x184: the first early reflection off the 0x67 write
        case 0x17f:
            c.push_hist(st, c.ram[c.addr(0x5F, 0, false)]);
            fetch(0);
            break;
// m[59] takes up a read of this pass, and there is none in its window, so the cell keeps its own value.
// 0x183 reads it, so what the first damping lowpass is fed is zero.
        case 0x180:
            c.queue_d(59, c.captured(m[59], st));
            fetch(0);
            break;
        case 0x181:
            fetch(0);
            break;
        case 0x182:
            fetch(0);
            break;
// the first unit's write into the slot 0x60, var::REVERB_ROW_SLOTS[0]: whatever 0x17b latched. This
// step does not carry w1[13], so it writes the latch rather than its own result; what it does compute
// is the first damping lowpass's input r[107].
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 2.0;
            c.queue_r(107, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            if (c.haswl) { outv = c.wlatch; c.haswl = false; }
            c.ram[c.addr(0x60, 0, false)] = outv;
            break;
// ---- 0x184 to 0x189, the two damping lowpasses ------------------------------------------------
// m[60] takes up the read of 0x17f, the slot 0x5F; var::REVERB_COMBS[3]: the first lowpass's a on its
// own state r[106]
        case 0x184:
            c.queue_d(60, c.captured(m[60], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_COMBS[2]: the same lowpass's first x word, on r[107]
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_COMBS[1]: its second x word, doubled and saturating, into r[106]
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(106, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// the read of the slot 0x61, which m[61] takes up at 0x18c, and the page's LFO into m[12];
// var::REVERB_COMBS[7]: the second lowpass's a on its own state r[108]
        case 0x187:
            c.push_hist(st, c.ram[c.addr(0x61, 0, false)]);
            c.queue_d(12, c.lfo[12]);
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_COMBS[6]: the same lowpass's first x word, on the cell m[54], which nothing writes
        case 0x188:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_COMBS[5]: its second x word, on the early reflection m[60], doubled and saturating, into
// r[108]. The two words differ by the damping loss, so together they are one pole.
        case 0x189:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(108, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x18a to 0x18d, the second early reflection's cell, and a step that does nothing --------------
// dead: the M_PREVIOUS write of m[54] is on a class-0 step, and step() returns before it
        case 0x18a:
            fetch(0);
            break;
// the read of the slot 0x62, which m[62] takes up at 0x190
        case 0x18b:
            c.push_hist(st, c.ram[c.addr(0x62, 0, false)]);
            fetch(0);
            break;
// m[61] takes up the read of 0x187, the slot 0x61: the first early reflection off the 0x66 write
        case 0x18c:
            c.queue_d(61, c.captured(m[61], st));
            fetch(0);
            break;
        case 0x18d:
            fetch(0);
            break;
// ---- 0x18e to 0x193, the two output sums ---------------------------------------------------------
// var::REVERB_W[9]: the first early reflection m[60] and the base program's level r[119]
        case 0x18e:
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[60];
            prod = k * c.src;
            a = c.x;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_W[8]: the second early reflection m[61] onto that, doubled and saturating, into r[120];
// the read of the slot 0x63 rides on this step and m[31] takes it up at 0x194
        case 0x18f:
            c.push_hist(st, c.ram[c.addr(0x63, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[61];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(120, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// m[62] takes up the read of 0x18b, the slot 0x62: the second early reflection off the 0x67 write.
// class 3, so the cell passes through onto the running value
        case 0x190:
            c.queue_d(62, c.captured(m[62], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[57];
            prod = c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_W[0]: the pre-delay tap m[55], and the same tap copied into m[55] a pass back for 0x165
        case 0x191:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            prod = k * c.src;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.queue_d(55, c.psrc);
            break;
// var::REVERB_W[6]: the cell m[59], which stays at zero, onto that
        case 0x192:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[59];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_W[7]: the early reflection m[62] onto that, doubled and saturating, into r[121]; the read
// of the slot 0x64 rides on this step and m[32] takes it up at 0x198
        case 0x193:
            c.push_hist(st, c.ram[c.addr(0x64, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[62];
            prod = k * c.src;
            a = c.acc;
            y = (prod + a) * 2.0;
            c.queue_r(121, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x194 to 0x198, three published reads and the pan --------------------------------------------
// m[31] takes up the read of 0x18f, the slot 0x63
        case 0x194:
            c.queue_d(31, c.captured(m[31], st));
            fetch(0);
            break;
// var::REVERB_PAN[0]: the left input on the running value
        case 0x195:
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_PAN[1]: r[121], the left output sum, onto that, four times and saturating, into r[15]
        case 0x196:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(15, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_PAN[2]: the right input on the running value, and the read of the slot 0x65
        case 0x197:
            c.push_hist(st, c.ram[c.addr(0x65, 0, false)]);
            k = yss236_core::s16(c.coef[st]);
            fetch(6);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// var::REVERB_PAN[3]: r[120], the right output sum, onto that, four times and saturating, into r[16];
// m[32] takes up the read of 0x193, the slot 0x64
        case 0x198:
            c.queue_d(32, c.captured(m[32], st));
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            prod = k * xv;
            a = c.acc;
            y = (prod + a) * 4.0;
            c.queue_r(16, yss236_core::mode(y, 1));
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            break;
// ---- 0x199 to 0x19f, the two comb gains ---------------------------------------------------------
        case 0x199:
            fetch(0);
            break;
        case 0x19a:
            fetch(0);
            break;
// var::REVERB_COMBS[0]: the first comb's gain on the unit's tap m[63], plus the first lowpass's state
// r[106], into the slot 0x66 the first pair of early reflections hangs off. Carries w1[13] and is a
// writer, so it writes its own result.
        case 0x19b:
            k = yss236_core::s16(c.coef[st]);
            fetch(106);
            c.psrc = c.src; c.src = m[63];
            prod = k * c.src;
            a = c.x;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            outv = yss236_core::clip(y, yss236_core::SAT);
            c.haswl = false;
            c.ram[c.addr(0x66, 0, false)] = outv;
            break;
// m[33] takes up the read of 0x197, the slot 0x65
        case 0x19c:
            c.queue_d(33, c.captured(m[33], st));
            fetch(0);
            break;
        case 0x19d:
            fetch(0);
            break;
        case 0x19e:
            fetch(0);
            break;
// var::REVERB_COMBS[4]: the second comb's gain on the same tap, plus the second lowpass's state r[108],
// into the slot 0x67 the second pair of early reflections hangs off. Carries w1[13] and is a writer, so
// it writes its own result.
        case 0x19f:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[63];
            prod = k * c.src;
            a = c.x;
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
void var_m10(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native
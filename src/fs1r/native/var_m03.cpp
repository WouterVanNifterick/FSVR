// fs1r/native/var_m03.cpp - VOP3-2, the variation window's program for HM Enhancer, Noise Gate and
// Compressor: its 72 steps.
//
// rom::VARIATION_PROGRAMS[3], the program the window holds for the three variation types 14 to 16, and the
// only program among the twelve that these three share: fx_dispatch.cpp's VARIATION_ROUTES gives msb 3 to
// HM Enhancer, Noise Gate and Compressor and to nothing else. Under each of them the same program words
// run and only the constants and the delay offsets move, so this native is one function for all three and
// no type is excluded. Which of the three is running is not a code difference here; the host's parameters
// are, through the DYNAMICS word lists in fx_drive.cpp and the var:: lists in rom_tables.cpp.
//
// A straight transcription of the steps: one case each, in step order, each step's arithmetic written out
// for its fixed fields in the order yss236_core::step() runs its own.
//
// Every constant the program reads is read from c.coef[st] on the pass, and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in here:
// the program words are fixed per row and each step's fields are folded into the arithmetic below.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x158 to 0x19f, the whole of the window's program. The loop is one iteration per step
// and carries the m_skip guard step() opens with, so a gate in the base program's 0x1xx half skips these
// steps as it would the interpreter's.
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
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x15c:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x162:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x169:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(120, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16a:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(121, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16b:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(122, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16c:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(123, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16d:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(124, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16e:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(125, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x16f:
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            a = c.x;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(126, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x170:
            k = yss236_core::s16(c.coef[st]);
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x171:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x172:
            k = yss236_core::s16(c.coef[st]);
            fetch(122);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x173:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(123);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 2.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x175:
            k = yss236_core::s16(c.coef[st]);
            fetch(124);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 4.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x176:
            k = yss236_core::s16(c.coef[st]);
            fetch(125);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x177:
            k = yss236_core::s16(c.coef[st]);
            fetch(126);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x178:
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = c.acc;
            v = 128.0 * yss236_core::s16(c.coef[st]) + a;
            v *= 1.0;
            c.queue_r(115, yss236_core::mode(v, 2));
            y = c.acc = yss236_core::accmode(v, 2);
            break;
        case 0x179:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x17a:
            k = yss236_core::s16(c.coef[st]);
            fetch(116);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x17c:
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x181:
            fetch(119);
            c.psrc = c.src; c.src = m[54];
            m2 = xv; prod = m2;
            a = c.src;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(54, yss236_core::clip(y, yss236_core::DSAT));
            break;
        case 0x182:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x183:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = c.acc / 32768.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x184:
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x185:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = c.acc;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 2);
            break;
        case 0x186:
            fetch(119);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 1);
            c.queue_d(55, yss236_core::clip(y, yss236_core::DSAT));
            break;
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
        case 0x189:
            k = yss236_core::s16(c.coef[st]);
            fetch(1);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18a:
            k = yss236_core::s16(c.coef[st]);
            fetch(120);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x18c:
            k = yss236_core::s16(c.coef[st]);
            fetch(111);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        case 0x18d:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x190:
            k = yss236_core::s16(c.coef[st]);
            fetch(121);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x192:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x195:
            k = yss236_core::s16(c.coef[st]);
            fetch(5);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
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
        case 0x19a:
            fetch(0);
            break;
        case 0x19b:
            fetch(0);
            break;
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

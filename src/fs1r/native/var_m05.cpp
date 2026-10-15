// fs1r/native/var_m05.cpp - VOP3-2, the variation window's program for Phaser1 and Phaser2: its 72 steps.
//
// rom::VARIATION_PROGRAMS[5], the program the window holds for the two variation types 5 and 6. Straight
// transcription of the 72 steps 0x158 to 0x19f as yss236_core::step() runs them; the structures are
// named in the second pass.
//
// Every constant is read from c.coef[st] on the pass and every delay offset through c.addr() off the
// c.slot[] entry, so nothing the firmware chose is folded in here.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step.

#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// The run of steps 0x158 to 0x19f, the whole of the window's program.
void block_0x158(yss236_core& c)
{
    double* const r = c.r;
    double* const m = c.m;
    double xv, k, g, m2, prod, a, y, v;

    auto fetch = [&](int sr) { xv = sr ? r[sr] : 0.0; c.px = c.x; c.haspx = c.hasx; c.x = xv; c.hasx = sr != 0; };
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
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = 0.0;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 0);
            break;
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

void var_m05(yss236_core& c, int from, int to)
{
    if (from == 0x158 && to == 0x1a0) { block_0x158(c); return; }
    for (int st = from; st < to; st++) c.step(st);
}

}  // namespace fs1r::native

// fs1r/native/var_m04.cpp - VOP3-2, the variation window's program for Auto Wah and Touch Wah: its 72 steps.
//
// A straight transcription of the steps 0x158 to 0x19f as yss236_core::step() runs them, one block per
// step in step order. Every constant is read from c.coef[st] on the pass and every delay offset through
// c.addr() off the c.slot[] entry the host last wrote, so no number the firmware chose is folded in
// here: the program words are fixed per row and each step's fields are folded into the arithmetic below
// in the order step() does its own.
//
// docs/vop3_native/README.md is the brief and what a native owes the core; yss236_core::step() is the
// definition of a step, which is why every case below follows its order exactly.
//
// vop3_native_check var-m04: identical, 2 types.

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
        // r[103] = saturate(2 * k * r[5]), the left serial input doubled
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
        // acc = k * r[103]
        case 0x159:
            k = yss236_core::s16(c.coef[st]);
            fetch(103);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[104] + acc
        case 0x15a:
            k = yss236_core::s16(c.coef[st]);
            fetch(104);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[104] = saturate(2 * (k * r[103] + acc))
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
        // m[2] takes the left serial input; acc = k * r[104]
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
        // m[6] takes the right serial input; acc = k * r[105] + acc
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
        // r[105] = saturate(4 * (k * r[104] + acc))
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
        // r[106] = saturate(2 * k * r[6]), the right serial input doubled
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
        // m[28] takes up the read of 0x15b; acc = k * r[106]
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
        // acc = k * r[107] + acc
        case 0x161:
            k = yss236_core::s16(c.coef[st]);
            fetch(107);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[107] = saturate(2 * (k * r[106] + acc))
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
        // a read of the slot 0x58; acc = k * r[107]
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
        // m[29] takes up the read of 0x15f; acc = k * r[108] + acc
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
        // r[108] = saturate(4 * (k * r[107] + acc))
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
        // the class-1 load: acc = 128 * k, the cutoff word
        case 0x166:
            k = yss236_core::s16(c.coef[st]);
            fetch(0);
            c.psrc = c.src; c.src = m[0];
            a = 0.0;
            v = 128.0 * k + a;
            v *= 1.0;
            y = c.acc = yss236_core::accmode(v, 0);
            break;
        // m[11] takes the LFO of this page; acc = k * m[8] + acc
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
        // acc = k * r[105]
        case 0x169:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[109] = saturate(k * r[108] + acc)
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
        // acc = k * r[109]
        case 0x16b:
            k = yss236_core::s16(c.coef[st]);
            fetch(109);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[110] + acc
        case 0x16c:
            k = yss236_core::s16(c.coef[st]);
            fetch(110);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[110] = saturate(2 * (k * r[109] + acc))
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
        // r[119] = wrap(t[9] * r[119]); t[9] takes the result
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
        // acc = k * m[54] - r[105]
        case 0x16f:
            k = yss236_core::s16(c.coef[st]);
            fetch(105);
            c.psrc = c.src; c.src = m[54];
            m2 = c.src; prod = k * m2;
            a = xv;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = saturate(k * r[114] - acc); m[56] takes it
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
        // acc = |16 * k * r[110]|; m[58] takes it
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
        // acc = k * m[55] - r[108]
        case 0x174:
            k = yss236_core::s16(c.coef[st]);
            fetch(108);
            c.psrc = c.src; c.src = m[55];
            m2 = c.src; prod = k * m2;
            a = xv;
            y = (prod - a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = saturate(k * r[115] - acc); m[57] takes it
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
        // acc = max(0, k * r[111] - m[58])
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
        // r[114] = saturate(t[9] * m[56] + r[114])
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
        // r[115] = saturate(t[9] * m[57] + r[115])
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
        // acc = k * r[112] + acc / 32768
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
        // r[112] = saturate(k * m[58] + acc)
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
        // acc = k * r[112]
        case 0x17f:
            k = yss236_core::s16(c.coef[st]);
            fetch(112);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // acc = k * r[113] + acc
        case 0x180:
            k = yss236_core::s16(c.coef[st]);
            fetch(113);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = c.acc;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[113] = saturate(k * r[112] + acc)
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
        // acc = saturate(t[9] * r[114] + m[54]); m[54] takes it
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
        // acc = saturate(t[9] * r[115] + m[55]); m[55] takes it
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
        // acc = k * r[114]
        case 0x184:
            k = yss236_core::s16(c.coef[st]);
            fetch(114);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[15] = saturate(4 * (k * r[105] + acc))
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
        // acc = k * r[115]
        case 0x186:
            k = yss236_core::s16(c.coef[st]);
            fetch(115);
            c.psrc = c.src; c.src = m[0];
            m2 = xv; prod = k * m2;
            a = 0.0;
            y = (prod + a) * 1.0;
            y = c.acc = yss236_core::accmode(y, 0);
            break;
        // r[16] = saturate(4 * (k * r[108] + acc))
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
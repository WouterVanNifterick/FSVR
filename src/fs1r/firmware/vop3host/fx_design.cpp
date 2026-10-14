// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The filter designer and the small float helpers. Every sh:: call is one soft-float call of
// the firmware, in its operand order (softfloat.h), so the words come out bit for bit.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {

constexpr uint32_t FX_DB_SQRT = 0x37176C;   // f32: 10^((g - 64) / 40) for gain bytes 52..76

// one coefficient to a word: S*c rounded half away from zero, as a float
float round_word(float S, float c)
{
    const float f = sh::fmul(S, c);
    const double d = sh::ftod(f);
    return sh::dtof(sh::dadd(d, sh::fgt(0.0f, c) ? -0.5 : 0.5));
}

// the rounding error of a word: (int16)word - S*c
float word_error(float S, float c, float w)
{
    return sh::fsub(sh::fmul(S, c), sh::itof(int16_t(sh::ftoi(w))));
}

float bump(float w, double by)
{
    return sh::dtof(sh::dadd(sh::ftod(w), by));
}

}  // namespace

// flash 0x37414 fxDesignFilter(order, scale, kind, freq, gain, q, steps)
//   kind 0 LPF1, 1 HPF1, 2 low shelf 1, 3 high shelf 1, 4 LPF2, 5 HPF2, 6 low shelf 2,
//   7 high shelf 2, 8 BPF2, 9 notch, 10 peaking, 11 gain only, >= 12 all zero.
//   t = tan(pi f / fs) from FX_TAN[freq], G = 10^((gain-64)/20), Gi its inverse, Gs/Gsi the
//   square roots, w = 10 t / q. The words are b0, b1, -a1 (order 1: 3 words) or
//   b0, b1, b2, -a1, -a2 (order 2: 5 words; any other order writes 4: b0, b2, -a1, -a2),
//   scaled by 2048 * (16 >> scale) and rounded with a sum-of-errors compensation.
void Host::fxDesignFilter(int order, int scale, int kind, int freq, int gain, int q, uint32_t steps)
{
    order &= 0xFF; scale &= 0xFF; kind &= 0xFF; freq &= 0xFF;
    const uint32_t g = uint32_t(gain) & 0xFF, qb = uint32_t(q) & 0xFF;
    const float G = rom::f32(rom::FX_DB + 4u * uint32_t(int32_t(g) - 52));
    const float Gi = rom::f32(rom::FX_DB + 4u * uint32_t(76 - int32_t(g)));
    const float Gs = rom::f32(FX_DB_SQRT + 4u * uint32_t(int32_t(g) - 52));
    const float Gsi = rom::f32(FX_DB_SQRT + 4u * uint32_t(76 - int32_t(g)));
    const float t = rom::f32(rom::FX_TAN + 4u * uint32_t(freq));
    const float t2 = sh::fmul(t, t);
    const bool boost = g >= 64;

    float c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0;   // b0, b1, -a1, b2, -a2
    using sh::ftod; using sh::dtof; using sh::dadd; using sh::dsub; using sh::dmul; using sh::ddiv;
    using sh::fadd; using sh::fsub; using sh::fmul; using sh::fdiv; using sh::fneg; using sh::utof;

    switch (kind) {
    case 0: {   // 0x374B0: first-order low-pass with gain
        const float gt = fmul(G, t);
        c0 = c1 = dtof(ddiv(ftod(gt), dadd(ftod(t), 1.0)));
        c2 = dtof(ddiv(dsub(1.0, ftod(t)), dadd(ftod(t), 1.0)));
        break;
    }
    case 1: {   // 0x3753E: first-order high-pass with gain
        c0 = dtof(ddiv(ftod(G), dadd(ftod(t), 1.0)));
        c1 = fneg(c0);
        c2 = dtof(ddiv(dsub(1.0, ftod(t)), dadd(ftod(t), 1.0)));
        break;
    }
    case 2: {   // 0x375CE: first-order low shelf
        const float x = boost ? fmul(t, G) : t;
        const float y = boost ? t : fmul(t, Gi);
        c0 = dtof(ddiv(dadd(ftod(x), 1.0), dadd(ftod(y), 1.0)));
        c1 = dtof(ddiv(dadd(ftod(x), -1.0), dadd(ftod(y), 1.0)));
        c2 = dtof(ddiv(dsub(1.0, ftod(y)), dadd(ftod(y), 1.0)));
        break;
    }
    case 3: {   // 0x376D4: first-order high shelf, single precision
        const float A = boost ? G : 1.0f;
        const float B = boost ? 1.0f : Gi;
        const float apt = fadd(A, t);
        const float bpt = fadd(B, t);
        c0 = fdiv(bpt, apt);
        c1 = fdiv(bpt, fsub(A, t));
        c2 = fdiv(bpt, fsub(t, B));
        break;
    }
    case 4: case 5: {   // 0x37768 / 0x3788A: second-order low-pass / high-pass with Q
        const float w = dtof(ddiv(dmul(ftod(t), 10.0), ftod(utof(qb))));
        const float D = dtof(dadd(dadd(ftod(w), ftod(t2)), 1.0));
        if (kind == 4) {
            c0 = fdiv(D, fmul(G, t2));
            c3 = c0;
            c1 = fadd(c0, c0);
        } else {
            c0 = fdiv(D, G);
            c3 = c0;
            c1 = fsub(c0, fneg(c0));
        }
        c2 = dtof(ddiv(dsub(dsub(2.0, ftod(t2)), ftod(t2)), ftod(D)));
        c4 = dtof(ddiv(dadd(dsub(ftod(w), ftod(t2)), -1.0), ftod(D)));
        break;
    }
    case 6: {   // 0x379E8: second-order low shelf
        if (boost) {
            const float u = fmul(t, Gs);
            const float u2 = fmul(u, u);
            const float D = dtof(dadd(dadd(dadd(ftod(t), ftod(t)), ftod(t2)), 1.0));
            c0 = dtof(ddiv(dadd(dadd(dadd(ftod(u), ftod(u)), ftod(u2)), 1.0), ftod(D)));
            c1 = dtof(ddiv(dadd(ftod(fadd(u2, u2)), -2.0), ftod(D)));
            c3 = dtof(ddiv(dadd(dsub(dsub(ftod(u2), ftod(u)), ftod(u)), 1.0), ftod(D)));
            c2 = dtof(ddiv(dsub(dsub(2.0, ftod(t2)), ftod(t2)), ftod(D)));
            c4 = dtof(ddiv(dadd(dsub(dadd(ftod(t), ftod(t)), ftod(t2)), -1.0), ftod(D)));
        } else {
            const float v = fmul(t, Gsi);
            const float v2 = fmul(v, v);
            const float D = dtof(dadd(dadd(dadd(ftod(v), ftod(v)), ftod(v2)), 1.0));
            c0 = dtof(ddiv(dadd(dadd(dadd(ftod(t), ftod(t)), ftod(t2)), 1.0), ftod(D)));
            c1 = dtof(ddiv(dadd(ftod(fadd(t2, t2)), -2.0), ftod(D)));
            c3 = dtof(ddiv(dadd(dsub(dsub(ftod(t2), ftod(t)), ftod(t)), 1.0), ftod(D)));
            c2 = dtof(ddiv(dsub(dsub(2.0, ftod(v2)), ftod(v2)), ftod(D)));
            c4 = dtof(ddiv(dadd(dsub(dadd(ftod(v), ftod(v)), ftod(v2)), -1.0), ftod(D)));
        }
        break;
    }
    case 7: {   // 0x37E70: second-order high shelf
        if (boost) {
            const float D = dtof(dadd(dadd(dadd(ftod(t), ftod(t)), ftod(t2)), 1.0));
            const float gpt = fadd(Gs, t);
            c0 = fdiv(D, fmul(gpt, gpt));
            const float tt = fadd(t2, t2);
            c1 = fdiv(D, fsub(G, fsub(G, tt)));
            const float gmt = fsub(t, Gs);
            c3 = fdiv(D, fmul(gmt, gmt));
            c2 = dtof(ddiv(dsub(dsub(2.0, ftod(t2)), ftod(t2)), ftod(D)));
            c4 = dtof(ddiv(dadd(dsub(dadd(ftod(t), ftod(t)), ftod(t2)), -1.0), ftod(D)));
        } else {
            const float gpt = fadd(Gsi, t);
            const float D = fmul(gpt, gpt);
            c0 = dtof(ddiv(dadd(dadd(dadd(ftod(t), ftod(t)), ftod(t2)), 1.0), ftod(D)));
            c1 = dtof(ddiv(dadd(ftod(fadd(t2, t2)), -2.0), ftod(D)));
            c3 = dtof(ddiv(dadd(dsub(dsub(ftod(t2), ftod(t)), ftod(t)), 1.0), ftod(D)));
            const float f166 = fsub(t2, Gi);
            const float f167 = fsub(t2, f166);
            c2 = fdiv(D, fadd(f167, Gi));
            const float f170 = fsub(Gsi, t);
            const float f171 = fsub(t, Gsi);
            c4 = fdiv(D, fmul(f171, f170));
        }
        break;
    }
    case 10: {   // 0x3840C: peaking EQ
        const float w = dtof(ddiv(dmul(ftod(t), 10.0), ftod(utof(qb))));
        if (boost) {
            const float wg = fmul(w, G);
            const float D = dtof(dadd(dadd(ftod(w), ftod(t2)), 1.0));
            c0 = dtof(ddiv(dadd(dadd(ftod(wg), ftod(t2)), 1.0), ftod(D)));
            c1 = dtof(ddiv(dadd(ftod(fadd(t2, t2)), -2.0), ftod(D)));
            c2 = fneg(c1);
            c3 = dtof(ddiv(dadd(dsub(ftod(t2), ftod(wg)), 1.0), ftod(D)));
            c4 = dtof(ddiv(dadd(dsub(ftod(w), ftod(t2)), -1.0), ftod(D)));
        } else {
            const float wg = fmul(w, Gi);
            const float D = dtof(dadd(dadd(ftod(wg), ftod(t2)), 1.0));
            c0 = dtof(ddiv(dadd(dadd(ftod(w), ftod(t2)), 1.0), ftod(D)));
            c1 = dtof(ddiv(dadd(ftod(fadd(t2, t2)), -2.0), ftod(D)));
            c2 = fneg(c1);
            c3 = dtof(ddiv(dadd(dsub(ftod(t2), ftod(w)), 1.0), ftod(D)));
            c4 = dtof(ddiv(dadd(dsub(ftod(wg), ftod(t2)), -1.0), ftod(D)));
        }
        break;
    }
    case 11:   // 0x38726: plain gain
        c0 = G;
        break;
    }

    // ---- 0x38776: the emitter ----
    const float S = fmul(sh::itof(16 >> scale), 2048.0f);
    auto word = [&](float w) -> uint32_t { return uint32_t(int16_t(sh::ftoi(w))); };
    auto raw = [&](float w) -> uint32_t { return uint32_t(sh::ftoi(w)); };

    if (order == 1) {
        float w0 = round_word(S, c0), w1 = round_word(S, c1);
        const float w2 = round_word(S, c2);
        const float e0 = word_error(S, c0, w0), e1 = word_error(S, c1, w1);
        if (sh::dgt(-0.5, ftod(fadd(e1, e0)))) {
            if (sh::fge(e1, e0)) w0 = bump(w0, 1.0); else w1 = bump(w1, 1.0);
        } else if (sh::dlt(0.5, ftod(fadd(e1, e0)))) {
            if (sh::flt(e1, e0)) w0 = bump(w0, -1.0); else w1 = bump(w1, -1.0);
        }
        set_coef(rom::u16(steps), word(w0));
        set_coef(rom::u16(steps + 2), word(w1));
        set_coef(rom::u16(steps + 4), raw(w2));
        writeSweWordListSub(3, steps);
        return;
    }
    // 0x38A82: five words (b0, b1, b2, -a1, -a2), rounded in the firmware's order
    float w0 = round_word(S, c0), w1 = round_word(S, c1), w3 = round_word(S, c3);
    float w2 = round_word(S, c2), w4 = round_word(S, c4);
    const float e0 = word_error(S, c0, w0), e1 = word_error(S, c1, w1), e3 = word_error(S, c3, w3);
    const float e2 = word_error(S, c2, w2), e4 = word_error(S, c4, w4);
    if (sh::dgt(-0.5, ftod(fadd(fadd(e1, e0), e3)))) {
        if (sh::fge(e3, e1)) { if (sh::fge(e1, e0)) w0 = bump(w0, 1.0); else w1 = bump(w1, 1.0); }
        else { if (sh::fge(e3, e0)) w0 = bump(w0, 1.0); else w3 = bump(w3, 1.0); }
    } else if (sh::dlt(0.5, ftod(fadd(fadd(e1, e0), e3)))) {
        if (sh::flt(e3, e1)) { if (sh::flt(e1, e0)) w0 = bump(w0, -1.0); else w1 = bump(w1, -1.0); }
        else { if (sh::flt(e3, e0)) w0 = bump(w0, -1.0); else w3 = bump(w3, -1.0); }
    }
    if (sh::dgt(-0.5, ftod(fadd(e4, e2)))) {
        if (sh::fge(e4, e2)) w2 = bump(w2, 1.0); else w4 = bump(w4, 1.0);
    } else if (sh::dlt(0.5, ftod(fadd(e4, e2)))) {
        if (sh::flt(e4, e2)) w2 = bump(w2, -1.0); else w4 = bump(w4, -1.0);
    }
    // 0x38FFE
    set_coef(rom::u16(steps), word(w0));
    set_coef(rom::u16(steps + 2), word(w1));
    set_coef(rom::u16(steps + 4), word(w3));
    set_coef(rom::u16(steps + 6), raw(w2));
    set_coef(rom::u16(steps + 8), word(w4));
    writeSweWordListSub(5, steps);
}

// flash 0x39108 fxFloatCurveB(scale, v, gain, steps): two words from a gain byte and a mix
// byte v: k = (v + 8) / 18 gives (k G, (1 - k) G), with 1.0 nudged below full scale when unscaled
void Host::fxFloatCurveB(int scale, int v, int gain, uint32_t steps)
{
    scale &= 0xFF; v &= 0xFF;
    const float G = rom::f32(rom::FX_DB + 4u * uint32_t((gain & 0xFF) - 52));
    const float k = sh::dtof(sh::ddiv(sh::dadd(sh::utod(uint32_t(v)), 8.0), 18.0));
    float c0 = sh::fmul(k, G);
    const float c1 = sh::dtof(sh::dmul(sh::dsub(1.0, sh::ftod(k)), sh::ftod(G)));
    if (!sh::dne(1.0, sh::ftod(c0)) && scale == 0)
        c0 = sh::from_bits(0x3F7FFE09);
    const float S = sh::fmul(sh::itof(16 >> scale), 2048.0f);
    const float w0 = round_word(S, c0), w1 = round_word(S, c1);
    set_coef(rom::u16(steps), uint32_t(sh::ftoi(w0)));
    set_coef(rom::u16(steps + 2), uint32_t(int16_t(sh::ftoi(w1))));
    writeSweWordListSub(2, steps);
}

// flash 0x394F8 fxFillBand(v, steps): x = 4080 - 16 v into steps 2..5, a geometric-series
// gain word into step 1, 0 into step 6 and 6553 into steps 7..11; steps 1..5 go to the chip.
void Host::fxFillBand(int v, uint32_t steps)
{
    const int32_t x = 4080 - ((v & 0xFF) << 4);
    const float f2 = sh::fdiv(2048.0f, sh::itof(int16_t(x)));
    float acc = sh::fadd(1.0f, f2);
    acc = sh::fadd(sh::fmul(acc, f2), 1.0f);
    acc = sh::fadd(sh::fmul(acc, f2), 1.0f);
    acc = sh::fadd(sh::fmul(acc, f2), 1.0f);
    const float f10 = sh::fdiv(5.0f, acc);
    const int32_t i12 = sh::ftoi(sh::fdiv(f10, 32767.0f));
    set_coef(rom::u16(steps + 2), uint32_t(i12));
    for (int i = 2; i <= 5; i++)
        set_coef(rom::u16(steps + 2 * i), uint32_t(x));
    set_coef(rom::u16(steps + 12), 0);
    for (int i = 7; i <= 11; i++)
        set_coef(rom::u16(steps + 2 * i), 6553);
    writeSweWordListSub(5, steps + 2);
}

// flash 0x39390 / 0x3943C / 0x394BA: copy n constants between step lists and write them
void Host::fxCopyWords3(uint32_t src, uint32_t dst)
{
    for (int i = 0; i < 3; i++)
        set_coef(rom::u16(dst + 2 * i), coef(rom::u16(src + 2 * i)));
    writeSweWordListSub(3, dst);
}
void Host::fxCopyWords5(uint32_t src, uint32_t dst)
{
    for (int i = 0; i < 5; i++)
        set_coef(rom::u16(dst + 2 * i), coef(rom::u16(src + 2 * i)));
    writeSweWordListSub(5, dst);
}
void Host::fxCopyWords2(uint32_t src, uint32_t dst)
{
    for (int i = 0; i < 2; i++)
        set_coef(rom::u16(dst + 2 * i), coef(rom::u16(src + 2 * i)));
    writeSweWordListSub(2, dst);
}

// 0x39C758 fxDesignFromTable(index, steps): a first-order low shelf from the (freq, gain)
// byte pair at 0x370646 when its gain byte is nonzero
void Host::fxDesignFromTable(int index, uint32_t steps)
{
    const uint32_t row = 0x370646 + 2u * (uint32_t(index) & 0xFF);
    const int32_t gain = rom::s8(row + 1);
    if ((gain & 0xFF) != 0)
        fxDesignFilter(1, 2, 2, rom::s8(row), gain, 0, steps);
}

}  // namespace fs1r::vop3host

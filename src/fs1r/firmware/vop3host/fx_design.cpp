// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The filter designer and the small float helpers. Each line is one step of the firmware's
// arithmetic, in its precision and its order, so the words come out bit for bit.
#include "effect_host.h"

namespace fs1r::vop3host {

namespace {


// one coefficient to a word: S*c rounded half away from zero, as a float
float round_word(float S, float c)
{
    const float f = S * c;
    const double d = double(f);
    return float(d + (0.0f > c ? -0.5 : 0.5));
}

// the rounding error of a word: (int16)word - S*c
float word_error(float S, float c, float w)
{
    return float(int16_t(int(w))) - S * c;
}

float bump(float w, double by)
{
    return float(double(w) + by);
}

}  // namespace

// flash 0x37414 designFilter(order, scale, kind, freq, gain, q, steps)
//   kind 0 LPF1, 1 HPF1, 2 low shelf 1, 3 high shelf 1, 4 LPF2, 5 HPF2, 6 low shelf 2,
//   7 high shelf 2, 10 peaking, 11 gain only.
//   t = tan(pi f / fs) from the TAN table, G = 10^((gain-64)/20), Gi its inverse, Gs/Gsi the
//   square roots, w = 10 t / q. The words are b0, b1, -a1 (order 1) or b0, b1, b2, -a1, -a2
//   (order 2), scaled by 2048 * (16 >> scale) and rounded with a sum-of-errors compensation.
//   Lower-case names are single precision; T, T2 and the casts mark where the firmware
//   computes in double.
void EffectHost::designFilter(int order, int scale, int kind, int freq, int gain, int q, const uint16_t* steps)
{
    order &= 0xFF; scale &= 0xFF; kind &= 0xFF; freq &= 0xFF;
    const uint32_t g = uint32_t(gain) & 0xFF, qb = uint32_t(q) & 0xFF;
    const float G = rom::DB[uint32_t(int32_t(g) - 52)];
    const float Gi = rom::DB[uint32_t(76 - int32_t(g))];
    const float Gs = rom::DB_SQRT[uint32_t(int32_t(g) - 52)];
    const float Gsi = rom::DB_SQRT[uint32_t(76 - int32_t(g))];
    const float t = rom::TAN[freq];
    const float t2 = t * t;
    const double T = double(t), T2 = double(t2);
    const bool boost = g >= 64;

    float c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0;   // b0, b1, -a1, b2, -a2

    switch (kind) {
    case 0: {   // 0x374B0: first-order low-pass with gain
        const float gt = G * t;
        c0 = c1 = float(double(gt) / (T + 1.0));
        c2 = float((1.0 - T) / (T + 1.0));
        break;
    }
    case 1: {   // 0x3753E: first-order high-pass with gain
        c0 = float(double(G) / (T + 1.0));
        c1 = -c0;
        c2 = float((1.0 - T) / (T + 1.0));
        break;
    }
    case 2: {   // 0x375CE: first-order low shelf
        const float x = boost ? t * G : t;
        const float y = boost ? t : t * Gi;
        c0 = float((double(x) + 1.0) / (double(y) + 1.0));
        c1 = float((double(x) - 1.0) / (double(y) + 1.0));
        c2 = float((1.0 - double(y)) / (double(y) + 1.0));
        break;
    }
    case 3: {   // 0x376D4: first-order high shelf, single precision
        const float A = boost ? G : 1.0f;
        const float B = boost ? 1.0f : Gi;
        const float apt = A + t;
        const float bpt = B + t;
        c0 = apt / bpt;
        c1 = (t - A) / bpt;
        c2 = (B - t) / bpt;
        break;
    }
    case 4: case 5: {   // 0x37768 / 0x3788A: second-order low-pass / high-pass with Q
        const float w = float(T * 10.0 / double(float(qb)));
        const float D = float(double(w) + T2 + 1.0);
        if (kind == 4) {
            c0 = G * t2 / D;
            c3 = c0;
            c1 = c0 + c0;
        } else {
            c0 = G / D;
            c3 = c0;
            c1 = -c0 - c0;
        }
        c2 = float((2.0 - T2 - T2) / double(D));
        c4 = float((double(w) - T2 - 1.0) / double(D));
        break;
    }
    case 6: {   // 0x379E8: second-order low shelf
        if (boost) {
            const float u = t * Gs;
            const float u2 = u * u;
            const float D = float(T + T + T2 + 1.0);
            c0 = float((double(u) + double(u) + double(u2) + 1.0) / double(D));
            c1 = float((double(u2 + u2) - 2.0) / double(D));
            c3 = float((double(u2) - double(u) - double(u) + 1.0) / double(D));
            c2 = float((2.0 - T2 - T2) / double(D));
            c4 = float((T + T - T2 - 1.0) / double(D));
        } else {
            const float v = t * Gsi;
            const float v2 = v * v;
            const float D = float(double(v) + double(v) + double(v2) + 1.0);
            c0 = float((T + T + T2 + 1.0) / double(D));
            c1 = float((double(t2 + t2) - 2.0) / double(D));
            c3 = float((T2 - T - T + 1.0) / double(D));
            c2 = float((2.0 - double(v2) - double(v2)) / double(D));
            c4 = float((double(v) + double(v) - double(v2) - 1.0) / double(D));
        }
        break;
    }
    case 7: {   // 0x37E70: second-order high shelf
        if (boost) {
            const float D = float(T + T + T2 + 1.0);
            const float gpt = Gs + t;
            c0 = gpt * gpt / D;
            const float tt = t2 + t2;
            c1 = (tt - G - G) / D;
            const float gmt = Gs - t;
            c3 = gmt * gmt / D;
            c2 = float((2.0 - T2 - T2) / double(D));
            c4 = float((T + T - T2 - 1.0) / double(D));
        } else {
            const float gpt = Gsi + t;
            const float D = gpt * gpt;
            c0 = float((T + T + T2 + 1.0) / double(D));
            c1 = float((double(t2 + t2) - 2.0) / double(D));
            c3 = float((T2 - T - T + 1.0) / double(D));
            const float f166 = Gi - t2;
            const float f167 = f166 - t2;
            c2 = (f167 + Gi) / D;
            const float f170 = t - Gsi;
            const float f171 = Gsi - t;
            c4 = f171 * f170 / D;
        }
        break;
    }
    case 10: {   // 0x3840C: peaking EQ
        const float w = float(T * 10.0 / double(float(qb)));
        if (boost) {
            const float wg = w * G;
            const float D = float(double(w) + T2 + 1.0);
            c0 = float((double(wg) + T2 + 1.0) / double(D));
            c1 = float((double(t2 + t2) - 2.0) / double(D));
            c2 = -c1;
            c3 = float((T2 - double(wg) + 1.0) / double(D));
            c4 = float((double(w) - T2 - 1.0) / double(D));
        } else {
            const float wg = w * Gi;
            const float D = float(double(wg) + T2 + 1.0);
            c0 = float((double(w) + T2 + 1.0) / double(D));
            c1 = float((double(t2 + t2) - 2.0) / double(D));
            c2 = -c1;
            c3 = float((T2 - double(w) + 1.0) / double(D));
            c4 = float((double(wg) - T2 - 1.0) / double(D));
        }
        break;
    }
    case 11:   // 0x38726: plain gain
        c0 = G;
        break;
    }

    // ---- 0x38776: the emitter ----
    const float S = float(16 >> scale) * 2048.0f;
    auto word = [&](float w) -> uint32_t { return uint32_t(int16_t(int(w))); };
    auto raw = [&](float w) -> uint32_t { return uint32_t(int(w)); };

    if (order == 1) {
        float w0 = round_word(S, c0), w1 = round_word(S, c1);
        const float w2 = round_word(S, c2);
        const float e0 = word_error(S, c0, w0), e1 = word_error(S, c1, w1);
        if (-0.5 > double(e1 + e0)) {
            if (e1 >= e0) w0 = bump(w0, 1.0); else w1 = bump(w1, 1.0);
        } else if (0.5 < double(e1 + e0)) {
            if (e1 < e0) w0 = bump(w0, -1.0); else w1 = bump(w1, -1.0);
        }
        coef[steps[0]] = word(w0);
        coef[steps[1]] = word(w1);
        coef[steps[2]] = raw(w2);
        writeCoefs(3, steps);
        return;
    }
    // 0x38A82: five words (b0, b1, b2, -a1, -a2), rounded in the firmware's order
    float w0 = round_word(S, c0), w1 = round_word(S, c1), w3 = round_word(S, c3);
    float w2 = round_word(S, c2), w4 = round_word(S, c4);
    const float e0 = word_error(S, c0, w0), e1 = word_error(S, c1, w1), e3 = word_error(S, c3, w3);
    const float e2 = word_error(S, c2, w2), e4 = word_error(S, c4, w4);
    if (-0.5 > double(e1 + e0 + e3)) {
        if (e3 >= e1) { if (e1 >= e0) w0 = bump(w0, 1.0); else w1 = bump(w1, 1.0); }
        else { if (e3 >= e0) w0 = bump(w0, 1.0); else w3 = bump(w3, 1.0); }
    } else if (0.5 < double(e1 + e0 + e3)) {
        if (e3 < e1) { if (e1 < e0) w0 = bump(w0, -1.0); else w1 = bump(w1, -1.0); }
        else { if (e3 < e0) w0 = bump(w0, -1.0); else w3 = bump(w3, -1.0); }
    }
    if (-0.5 > double(e4 + e2)) {
        if (e4 >= e2) w2 = bump(w2, 1.0); else w4 = bump(w4, 1.0);
    } else if (0.5 < double(e4 + e2)) {
        if (e4 < e2) w2 = bump(w2, -1.0); else w4 = bump(w4, -1.0);
    }
    // 0x38FFE
    coef[steps[0]] = word(w0);
    coef[steps[1]] = word(w1);
    coef[steps[2]] = word(w3);
    coef[steps[3]] = raw(w2);
    coef[steps[4]] = word(w4);
    writeCoefs(5, steps);
}

// flash 0x39108 storeHighDamp(scale, v, gain, steps): two words from a gain byte and a mix
// byte v: k = (v + 8) / 18 gives (k G, (1 - k) G), with 1.0 nudged below full scale when unscaled
void EffectHost::storeHighDamp(int scale, int v, int gain, const uint16_t* steps)
{
    scale &= 0xFF; v &= 0xFF;
    const float G = rom::DB[uint32_t((gain & 0xFF) - 52)];
    const float k = float((double(uint32_t(v)) + 8.0) / 18.0);
    float c0 = k * G;
    const float c1 = float((1.0 - double(k)) * double(G));
    if (double(c0) == 1.0 && scale == 0)
        c0 = 0.99997f;
    const float S = float(16 >> scale) * 2048.0f;
    const float w0 = round_word(S, c0), w1 = round_word(S, c1);
    coef[steps[0]] = int(w0);
    coef[steps[1]] = int16_t(int(w1));
    writeCoefs(2, steps);
}

// flash 0x394F8 storeEdge(v, steps): x = 4080 - 16 v into steps 2..5, a geometric-series
// gain word into step 1, 0 into step 6 and 6553 into steps 7..11; steps 1..5 go to the chip.
void EffectHost::storeEdge(int v, const uint16_t* steps)
{
    const int32_t x = 4080 - ((v & 0xFF) << 4);
    const float f2 = float(int16_t(x)) / 2048.0f;
    float acc = 1.0f + f2;
    acc = acc * f2 + 1.0f;
    acc = acc * f2 + 1.0f;
    acc = acc * f2 + 1.0f;
    const float f10 = acc / 5.0f;
    const int32_t i12 = int(32767.0f / f10);
    coef[steps[1]] = i12;
    for (int i = 2; i <= 5; i++)
        coef[steps[i]] = x;
    coef[steps[6]] = 0;
    for (int i = 7; i <= 11; i++)
        coef[steps[i]] = 6553;
    writeCoefs(5, steps + 1);
}

// 0x39390, 0x3943C, 0x394BA: n constants copied from one step list to another, and written
void EffectHost::copyCoefs(int n, const uint16_t* src, const uint16_t* dst)
{
    for (int i = 0; i < n; i++)
        coef[dst[i]] = coef[src[i]];
    writeCoefs(n, dst);
}

// 0x39C758 designTypeShelf(index, steps): a first-order low shelf from the (freq, gain)
// byte pair of the reverb type when its gain byte is nonzero
void EffectHost::designTypeShelf(int index, const uint16_t* steps)
{
    const int8_t* row = rom::REVERB_SHELF + 2u * (uint32_t(index) & 0xFF);
    const int32_t gain = row[1];
    if ((gain & 0xFF) != 0)
        designFilter(1, 2, 2, row[0], gain, 0, steps);
}

}  // namespace fs1r::vop3host

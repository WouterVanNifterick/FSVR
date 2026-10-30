// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The filter bank's sixteen voices, W at a time: what VoiceFilter::run (filters/voice_filter.h) computes for one,
// operation for operation on lanes. Compiled once per instruction set (kernel/README.md); dsp_check pins it.
#include <cmath>
#include "kernel/simd.h"
#include "filters/lanes.h"

namespace fsvr::kernel::FSVR_ISA_NS {

using namespace fsvr::simd::FSVR_ISA_NS;
using fsvr::filters::Lanes;
using fsvr::filters::Shared;

namespace {

// dsp/word.h's operations, on lanes. word.h is the reference and is not included here.
inline vd clip(vd v, double lo, double hi) { return max(min(v, set1(hi)), set1(lo)); }   // v < hi ? v : hi, then lo < m ? m : lo
inline vd sat(vd y) { return clip(y, -128.0, 128.0 - 1.0 / 256); }
inline vd pos(vd y) { return max(sat(y), set1(0.0)); }                                   // 0.0 < y ? y : 0.0
inline vd cell(vd y) { return clip(y, -128.0, 128.0); }
inline vd q15(vd y) { const vd w = mul(y, set1(256.0)); return div(min(max(w, set1(-32768.0)), set1(32767.0)), set1(32768.0)); }
inline double floored_mod(double a, double m) { double v = ::fmod(a, m); if (v != 0 && ((m < 0) != (v < 0))) v += m; return v; }
// Wraps add and take back the range; a lane that left it goes the long way, one at a time.
inline vd wrap_in(vd y, double half) {
    const vd a = add(y, set1(half));
    const vm in = mand(ge(a, set1(0.0)), lt(a, set1(2 * half)));
    vd r = sub(a, set1(half));
    if (!all(in)) {
        const int m = movemask(in);
        for (int l = 0; l < W; l++) if (!(m >> l & 1)) r = setlane(r, l, floored_mod(lane(a, l), 2 * half) - half);
    }
    return r;
}
inline vd wrap_sum(vd y) { return wrap_in(y, 256.0); }

}  // namespace

void filter_bank(Lanes& L, const Shared& g, const double* in, const double* eg, const double* lfo, double* out) {
    const vd zero = set1(0.0), two = set1(2.0), four = set1(4.0), c128 = set1(128.0);
    const vd eg_zero = set1(g.eg_zero), c61 = set1(g.c61), c62 = set1(g.c62), c63 = set1(g.c63);
    for (int s = 0; s < Lanes::N; s += W) {
        // the input, by its gain, x4. The chip's second serial bank adds its silence first.
        const vd x = sat(mul(add(zero, wrap_sum(add(mul(load(L.gain + s), load(in + s)), zero))), four));

        // the cutoff word, glided where a slot asks: the EG by its depth over the EG's own zero, LFO2 by its depth, x2
        const vd cutoff = load(L.cutoff + s), glide = load(L.glide + s);
        vd cs = load(L.cutoff_s + s);
        const vm gliding = gt(glide, zero);
        const vm snap = mor(isnan(cs), lt(abs(sub(cutoff, cs)), set1(1e-9)));
        const vd glided = select(snap, cutoff, add(cs, mul(sub(cutoff, cs), glide)));
        const vd c = select(gliding, glided, cutoff);
        store(L.cutoff_s + s, select(gliding, glided, cs));
        const vd fc = pos(add(mul(c128, c), wrap_sum(mul(add(mul(load(L.lfo_depth + s), load(lfo + s)), wrap_sum(add(mul(load(L.eg_depth + s), load(eg + s)), eg_zero))), two))));

        // The coefficients follow the cutoff word alone. Where any lane's moved the group's are made again: a lane
        // whose word stood still gets the same bits back, since they are a function of the word and its slot's words.
        vd kf = load(L.kf + s), kb = load(L.kb + s), kp = load(L.kp + s);
        if (any(ne(fc, load(L.memo_fc + s)))) {
            store(L.memo_fc + s, fc);
            const vd a = pos(mul(add(mul(load(L.exp_slope + s), fc), c62), two));         // the pole gain, by three squarings
            const vd b = pos(add(mul(q15(a), a), c61));
            const vd cc = pos(add(mul(q15(b), b), zero));
            const vd pole = add(mul(q15(cc), cc), zero);
            const vd g1 = pos(pole);
            const vd r1 = sat(sub(mul(load(L.type_k + s), g1), c63));                      // the resonance
            const vd r2 = sat(add(mul(load(L.reso_a + s), r1), c63));
            const vd fb = q15(sat(add(mul(load(L.fb_scale + s), sat(mul(add(mul(c128, load(L.reso_bias + s)), neg(r2)), two))), zero)));
            const vd g2 = cell(sat(add(mul(q15(pos(pole)), g1), zero)));                 // the pole gain squared
            const vd fg = pos(add(mul(fb, g2), g1));
            kf = q15(fg);
            kb = q15(sat(add(cell(fg), sat(sub(r2, cell(fg))))));
            kp = q15(pos(mul(add(mul(load(L.pole_k + s), g2), g1), four)));
            store(L.kf + s, kf); store(L.kb + s, kb); store(L.kp + s, kp);
        }

        // the two-pole section, then the two one-poles after it
        const vd x1 = load(L.x1 + s), s2 = load(L.s2 + s), s3 = load(L.s3 + s), s4 = load(L.s4 + s), s5 = load(L.s5 + s);
        const vd reso_b = load(L.reso_b + s);
        const vd diff = wrap_sum(sub(mul(reso_b, x1), wrap_sum(sub(mul(reso_b, x), x))));
        const vd bp = sat(add(mul(kb, s2), diff));
        const vd hp = sat(add(mul(load(L.stage_k + s), s3), bp));
        const vd n2 = sat(mul(add(mul(kf, cell(hp)), wrap_sum(add(mul(load(L.damp_k + s), s2), zero))), four));
        const vd n3 = sat(add(mul(kf, cell(n2)), s3));
        const vd n4 = sat(sub(mul(kp, n3), wrap_sum(sub(mul(kp, s4), s4))));
        const vd n5 = sat(sub(mul(kp, n4), wrap_sum(sub(mul(kp, s5), s5))));

        // the output ramp, and the type's tap through it
        const vd lv = pos(add(mul(c128, load(L.ramp + s)), load(L.level + s)));
        vd t = zero;
        t = select(loadmask(L.tap[Lanes::LPF24] + s), n5, t);
        t = select(loadmask(L.tap[Lanes::LPF18] + s), n4, t);
        t = select(loadmask(L.tap[Lanes::LPF12] + s), n3, t);
        t = select(loadmask(L.tap[Lanes::HPF] + s), hp, t);
        t = select(loadmask(L.tap[Lanes::BPF] + s), n2, t);
        t = select(loadmask(L.tap[Lanes::BEF] + s), bp, t);
        store(out + s, cell(sat(mul(add(mul(q15(lv), t), zero), load(L.route + s)))));

        store(L.x1 + s, x); store(L.s2 + s, n2); store(L.s3 + s, n3); store(L.s4 + s, n4); store(L.s5 + s, n5); store(L.level + s, lv);
    }
}

}  // namespace fsvr::kernel::FSVR_ISA_NS

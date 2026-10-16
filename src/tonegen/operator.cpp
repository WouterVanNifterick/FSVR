// tonegen/operator.cpp - the grain windows and the operator display. INFERRED: the constants are in fs1r/cal.h.
#include "tonegen/operator.h"
#include <vector>
#include "fs1r/tables.h"

float g_win[2][8][1025];       // [asymmetric?][skirt]: the two window families
float g_winDC[2][8];           // each window's mean, which a grain under a DC carrier does not carry

void init_tables() {
    fm::init();
    // MEASURED 2026-09-19 (FS1R.unlock's skirt sweep, all sixty partials) and 2026-09-24 (the same take
    // read again, plus 04_formant_2): the grain window is sin^p with p = 2 * 2^skirt on every form, and
    // there are two families. The "2" forms (all2, odd2, res2) are the symmetric window, whose line
    // amplitudes are the binomial row C(p, p/2 - k) to 0.4 to 0.7 dB over eight skirts. The "1" forms
    // (all1, odd1, res1) and the formant are the SAME exponent on the rising half only; the falling
    // half stays sin^2 whatever the skirt. That is what the period-averaged waveform shows directly
    // (a rise that sharpens with the skirt on a fall that never moves) and it puts all1, odd1 and res1
    // within 0.4 to 1.3 dB over every skirt where a sin^p at any exponent left 6 to 17. The formant is
    // that same asymmetric window stretched over its bandwidth: 04_formant_2's sixteen skirt segments
    // land inside 0.4 to 1.8 dB where the sqrt(2)-per-step symmetric fit could not get under 2.5.
    // docs/skirt.md, docs/formant.md.
    for (int f = 0; f < 2; f++)
        for (int s = 0; s < 8; s++) {
            double p = cal::WIN_SKIRT * pow(cal::WIN_SKIRT_STEP, s);
            double acc = 0;
            for (int i = 0; i <= 1024; i++) {
                double x = sin(PI * i / 1024.0);
                g_win[f][s][i] = (float)pow(x, f && i > 512 ? cal::WIN_SKIRT : p);
                if (i < 1024) acc += g_win[f][s][i];
            }
            g_winDC[f][s] = (float)(acc / 1024.0);
        }
}

// Ours, for a display only: the unit has no such readout. Voiced operator v alone, from the same op_sample
// the channels play, at a nominal 100 Hz with no modulation: n points over two periods, scaled to a peak of
// 1. The grain forms open their first window a period in, so a period runs first and is dropped. The
// formant's carrier is put four harmonics up so that its grains show, and its window and the res forms'
// ratio come from byte 6 as notes.cpp hands them to a channel.
void op_wave(const OpV& v, float* out, int n) {
    OpState s;
    const double f0 = 100.0;
    double pitch = f0, fop = v.form == 7 ? 4.0 * f0 : f0;
    if (v.form == 7) {
        int tw = TRANS[clampi(v.transpose + 24, 0, 48)];
        if (tw) pitch *= fm::exp2(tw / 1024.0);
    }
    s.wl7 = std::min(cal::FRMT_WL_MAX, pitch / (cal::FRMT_BW_HZ0 * fm::exp2(v.bw / cal::FRMT_BW_DB)));
    const int period = (int)(SR / f0);
    for (int i = 0; i < period; i++) op_sample(s, v, pitch, fop, v.bw, 0, 1);
    std::vector<double> y(2 * (size_t)period);
    double peak = 1e-9;
    for (auto& x : y) peak = std::max(peak, std::fabs(x = op_sample(s, v, pitch, fop, v.bw, 0, 1)));
    for (int i = 0; i < n; i++) {   // each point the largest sample of its share, so a narrow pulse still shows
        size_t a = (size_t)i * y.size() / std::max(n, 1), b = std::max(a + 1, (size_t)(i + 1) * y.size() / std::max(n, 1));
        double m = y[a];
        for (size_t k = a; k < b; k++)
            if (std::fabs(y[k]) > std::fabs(m)) m = y[k];
        out[i] = (float)(m / peak);   // the shape, full height
    }
}

// tonegen/noise.h - the unvoiced operator's noise band and resonance carrier. INFERRED: the tables are in fs1r/cal.h.
#pragma once
#include <algorithm>
#include <cmath>
#include "fs1r/cal.h"
#include "math/fastmath.h"

// The unvoiced resonance carrier and the noise it takes with it, cal::URES_*, interpolated over the register.
static inline void ures(int reg, int res, double& dc, double& noiseGain) {
    if ((res & 7) < 4) { dc = 0; noiseGain = 1; return; }
    double r = std::clamp((double)reg, (double)cal::URES_REG[0], (double)cal::URES_REG[4]);
    int i = 0; while (i < 3 && cal::URES_REG[i + 1] <= r) i++;
    double f = (r - cal::URES_REG[i]) / (cal::URES_REG[i + 1] - cal::URES_REG[i]); int k = (res & 7) - 4;
    dc = cal::URES_DC[i][k] + (cal::URES_DC[i + 1][k] - cal::URES_DC[i][k]) * f;
    noiseGain = pow(10.0, (cal::URES_NOISE_DB[i][k] + (cal::URES_NOISE_DB[i + 1][k] - cal::URES_NOISE_DB[i][k]) * f) / 20.0);
}
// The noise band's two coefficients and its peak gain against the bandwidth register and the skirt,
// read off cal.h's tables: piecewise linear in the register, and the skirt a per-step multiplier that
// is itself interpolated across the three registers it was measured at.
struct NoiseBand { double a1, a2, gdb; };
static inline NoiseBand noise_band(int reg, int skirt) {
    reg = clampi(reg, 0, cal::NOISE_BW_CLAMP);
    int i = 0; while (i < 14 && cal::NOISE_REG[i + 1] <= reg) i++;
    double f = (double)(reg - cal::NOISE_REG[i]) / (cal::NOISE_REG[i + 1] - cal::NOISE_REG[i]);
    NoiseBand b = {cal::NOISE_A1[i] + (cal::NOISE_A1[i + 1] - cal::NOISE_A1[i]) * f,
                   cal::NOISE_A2[i] + (cal::NOISE_A2[i + 1] - cal::NOISE_A2[i]) * f,
                   cal::NOISE_G_DB[i] + (cal::NOISE_G_DB[i + 1] - cal::NOISE_G_DB[i]) * f};
    if (skirt > 0) {
        double r = std::clamp((double)reg, cal::NOISE_SK_REG[0], cal::NOISE_SK_REG[2]);
        int k = r < cal::NOISE_SK_REG[1] ? 0 : 1;
        double g = (r - cal::NOISE_SK_REG[k]) / (cal::NOISE_SK_REG[k + 1] - cal::NOISE_SK_REG[k]);
        double m1 = cal::NOISE_SK_M1[k] + (cal::NOISE_SK_M1[k + 1] - cal::NOISE_SK_M1[k]) * g;
        double m2 = cal::NOISE_SK_M2[k] + (cal::NOISE_SK_M2[k + 1] - cal::NOISE_SK_M2[k]) * g;
        double dg = cal::NOISE_SK_DG[k] + (cal::NOISE_SK_DG[k + 1] - cal::NOISE_SK_DG[k]) * g;
        b.a1 = std::min(1.0, b.a1 * pow(m1, skirt)); b.a2 = std::min(1.0, b.a2 * pow(m2, skirt)); b.gdb += dg * skirt;
    }
    return b;
}
// Variance of two one-poles in series, each with unit DC gain, driven by unit variance white noise.
static inline double noise_band_var(double a1, double a2) {
    double p1 = 1 - a1, p2 = 1 - a2;
    if (std::abs(p1 - p2) < 1e-6) { double q = 1 - p1 * p1; return a1 * a1 * a1 * a1 * (1 + p1 * p1) / (q * q * q); }
    double s = p1 * p1 / (1 - p1 * p1) + p2 * p2 / (1 - p2 * p2) - 2 * p1 * p2 / (1 - p1 * p2);
    return a1 * a1 * a2 * a2 / ((p1 - p2) * (p1 - p2)) * s;
}

// fs1r/chips/vop3_core.h - the VOP3 (YSS236) as measured: one microcode engine for VOP3-1 (filter) and
// VOP3-2 (effects), which are the same chip (docs/vop3_isa.md, "One chip").
//
// MEASURED, and only that. This is a port of tools/vop3_interp.py, which is the reference: every rule
// below is asserted there against the live unit (384 DC probes in docs/vop3_probes.json, the shipped
// Hall1/Hall9 step responses, the d[] cell taps). tools/vop3_core_check.py runs both on the same
// program and requires bit-identical DAC output, so a rule changes in the Python first, then here.
// Constants carry the session that set them; the ISA doc has the evidence.
//
// Not modelled yet (open in docs/vop3_isa.md section 8): the w[] / rd-en gain file's writer (G is the
// measured table), VOP3-1's per-channel register banks (one bank here; VOP3-2 needs one), the mid-pass
// arrival of the external input buses, and what the op-0 k-0 mem-1 steps (Hall1 0x57/0x157/0x19f) write.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

struct Vop3 {
    // ---------------------------------------------------------------- constants (session that set them)
    static constexpr int    MEMSZ = 1 << 18;   // s16: one 2^18-word delay ring, pointer steps once per pass
    static constexpr int    REGION = 1 << 16;  // s31: mmode bits 3:2 pick a region. ponytail: size guessed
    static constexpr int    LAT = 3;           // s24 r10: a register written at step n is read from n + 3
    static constexpr int    DLAT = 3;          // s30/s31: d[] writes and captures land 3 steps later
    static constexpr double RANGE = 128.0;     // s24: registers span [-128, 128), word / 256
    static constexpr double ACC = 256.0;       // s24: the running value wraps at +-256 (mode 0)
    static constexpr double SAT = 8.0;         // s14: the DRAM word clips at +-8
    static constexpr double DSAT = 128.0;      // s27: d[] is wider than the DRAM word
    static constexpr double LOADK = 128.0;     // s14: a class-1 load v reads back as v / 256
    static constexpr int    DAC_L = 0x10, DAC_R = 0x11;   // s27: the DAC plays d[10] / d[11]
    static constexpr double RSCALE[4] = {1.0, 2.0, 4.0, 16.0};   // s24: route 0..3 scales the result
    // s18/s25: rd-en with rB set takes its gain from this fixed file (no step writes it)
    static constexpr double G[16] = {0.99988, 0.10144, -0.07422, 0.0, -0.33203, 0.0, 0.99988, 0.0,
                                     -0.50757, 0.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0};

    struct Step { uint16_t w[5]; };            // r10 r9 r8 r7 r6
    std::vector<Step> prog = std::vector<Step>(512);
    std::vector<uint16_t> coef = std::vector<uint16_t>(512);
    int offs[64] = {};                         // DRAM slot offsets (registers 0xD/0xE)

    // ---------------------------------------------------------------- state
    double r[128] = {}, d[128] = {}, acc = 0, inp = 0, dprev = 0;
    double x = 0, px = 0, src = 0, psrc = 0, wlatch = 0;
    bool hasx = true, haspx = false, haswl = false;
    std::vector<double> mem = std::vector<double>(MEMSZ);
    int ptr = 0;
    long n = 0;
    struct Pend { long due; double* p; double v; };
    std::vector<Pend> rq, dq;                  // register writes, d[] writes (FIFO: one latency each)
    struct Xfer { int slot; double v; };
    std::vector<Xfer> hist;                    // this pass's DRAM transfers, what a capture picks up

    static double s16(uint16_t k) { return (int16_t)k / 32768.0; }
    static double pymod(double a, double m) {  // Python's float %, for bit-identical wraps
        double r_ = std::fmod(a, m);
        if (r_ != 0 && ((m < 0) != (r_ < 0))) r_ += m;
        return r_;
    }
    static double mode(double y, int m) {      // s24: r7[15:14] 0 wrap, 1 saturate, 2 clamp < 0, 3 |y|
        if (m == 0) return pymod(y + RANGE, 2 * RANGE) - RANGE;
        y = std::fmax(-RANGE, std::fmin(RANGE - 1.0 / 256, y));
        return m == 2 ? std::fmax(0.0, y) : m == 3 ? std::fabs(y) : y;
    }
    static double accmode(double y, int m) { return m == 0 ? pymod(y + ACC, 2 * ACC) - ACC : mode(y, m); }
    static double clip(double v, double c) { return std::fmax(-c, std::fmin(c, v)); }

    int addr(int slot, int older, int mm) const {
        return (ptr + offs[slot & 0x3F] + older + REGION * (mm >> 2 & 3)) & (MEMSZ - 1);
    }
    double captured(int slot, double old) const {   // s16: the transfer of slot s - 2, else s - 1, else hold
        for (int want = slot - 2; want <= slot - 1; want++)
            for (size_t i = hist.size(); i-- > 0;)
                if (hist[i].slot == want) return hist[i].v;
        return old;
    }
    void dput(int c, double v) { dq.push_back({n + DLAT, &d[c], v}); }
    static void drain(std::vector<Pend>& q, long now) {
        size_t i = 0;
        for (; i < q.size() && q[i].due <= now; i++) *q[i].p = q[i].v;
        if (i) q.erase(q.begin(), q.begin() + i);
    }

    void step(int st) {
        n++;
        drain(rq, n);
        drain(dq, n);
        const uint16_t *w = prog[st].w, r10 = w[0], r9 = w[1], r8 = w[2], r7 = w[3], r6 = w[4];
        const int mem_ = r10 & 7, ra = r9 & 0x7F, daddr = r8 >> 7, mm = r8 & 0x7F, route = r7 >> 12 & 3;
        const int op = r7 >> 6 & 7, cls = r6 >> 14, rb = r6 >> 7 & 0x7F, f6c = r6 & 0x3F, m = r7 >> 14;
        const int hi = st >= 0x100 ? 0x40 : 0;  // s31: steps >= 0x100 use the upper 64 d[] cells
        const bool xfer = (st & 3) == 3 || (st & 3) == 1;   // s16 phase 3, s17/s30 phase 1 too
        const int slot = st >> 2;
        if (xfer && (mem_ == 2 || mem_ == 3 || mem_ == 4 || mem_ == 6))   // s16: read (4/6 one word older)
            hist.push_back({slot, mem[addr(slot, mem_ & 4 ? 1 : 0, mm)]});
        if ((daddr & 0x180) == 0x100) {          // s16/s30: capture into d[n]
            int c = (daddr & 0x3F) | hi;
            dq.push_back({n + DLAT, &d[c], captured(slot, d[c])});
        }
        if (cls != 2 && cls != 3) {              // s27 r12: every step fetches x = r[rB] for the z^-1 latch
            px = x; haspx = hasx; hasx = rb != 0; x = rb ? r[rb] : 0;
        }
        if (cls == 1) {                          // s24/s27a: r[rA] = k/256 + s, route-scaled; s takes it
            double v = (LOADK * s16(coef[st]) + acc) * RSCALE[route];
            rq.push_back({n + LAT, &r[ra], mode(v, m)});
            acc = accmode(v, m);
            if ((daddr & 0x180) == 0x180 && (r9 >> 8 & 0x1F))   // s29: class 1 with r9[12:8] writes d[n]
                dput((daddr & 0x3F) | hi, clip(acc, DSAT));
            return;
        }
        if (cls == 0) return;                    // s33: class 0 (mem 1 included) writes nothing
        double k = s16(coef[st]);
        bool rden = r7 >> 4 & 1;
        double g = cls == 3 ? 1.0 : (rden && rb) ? G[r7 & 0xF] : k;   // s19 class 3 = unity; s18 G[rsrc]
        r[0] = inp;                              // s24: rB = 0 names r[0], the chip input
        double xv = r[rb];
        px = x; haspx = hasx; x = xv; hasx = rb != 0;
        psrc = src; src = d[f6c | hi];           // s29: op 4 / op 5 / op 7 source is d[r6[5:0]]
        double s = (r7 >> 10 & 1) ? -acc : acc;  // s12: sel negates s
        double y;
        if (op == 5 && rb && cls == 2) y = std::fmax(xv, 0.0) + g * src;   // s30: comb write
        else if (cls == 3 && op == 5) y = xv;
        else switch (op) {
            case 0: y = s + g * xv; break;
            case 1: y = g * xv + (rb ? xv : 0.0); break;   // s15: op 1 with rB is x + k x
            case 2: case 3: y = g * xv; break;
            case 4: y = s + g * src; break;      // s30: FIR tap
            case 5: y = s < 0 ? -std::ldexp(1.0, -17) : 0.0; break;
            case 6: y = 0; break;
            default: y = g * src; break;         // op 7
        }
        if (r7 >> 11 & 1) y = std::fabs(y);      // s15: r7[11] rectifies
        y *= RSCALE[route];
        if (ra && (r9 >> 7 & 1))                 // s27: rA | 0x80 stores the previous step's x (z^-1)
            rq.push_back({n + LAT, &r[ra], haspx ? px : xv});
        else if (ra)
            rq.push_back({n + LAT, &r[ra], mode(y, m)});
        y = acc = accmode(y, m);
        double out = clip(y, SAT);
        if ((daddr & 0x1C0) == 0x1C0) dput((daddr & 0x3F) | hi, psrc);   // s32/s33: bit 6 writes prev d[f6c]
        else if ((daddr & 0x180) == 0x180 && (op != 7 || (daddr & 0x3F) != f6c)) dput((daddr & 0x3F) | hi, clip(y, DSAT));
        if (op == 5 && rb) { wlatch = out; haswl = true; }   // s30: op 5 with rB loads the DRAM write latch
        if (xfer && mem_ == 1) {                 // s16: write
            if (haswl) { out = wlatch; haswl = false; }
            mem[addr(slot, 0, mm)] = out;
            hist.push_back({slot, out});
        }
    }

    // One pass. ponytail: test image 0 takes the audio input in d[1] / d[5]; the shipped programs' inputs
    // are register writes by the hardware (s22: r[03..08]), set them through r[] before calling.
    void pass(double in = 0) {
        inp = in; d[1] = d[5] = in;
        for (int st = 0; st < 512; st++) step(st);
        hist.clear();
        ptr = (ptr - 1) & (MEMSZ - 1);
    }
    // (L, R) as the unit plays them after this pass, readout units (d / 4): 18-bit word, floor; L leaves a
    // pass after R (s28).
    void dac(double& L, double& R) {
        auto q = [](double v) { return std::floor(clip(v / 4, 8.0) * 16384) / 16384; };
        L = q(dprev); R = q(d[DAC_R]); dprev = d[DAC_L];
    }
};

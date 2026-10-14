// vop3_jit_check - the compiled VOP3 program against the interpreter, byte for byte.
//
//   build.bat bench                       builds bin\vop3_jit_check.exe with the vop3host flags
//   vop3_jit_check [samples per type]     default 9600
//
// Two effect sections, one held on the interpreter, step through every reverb, variation and insertion
// type on the same noise, each type loaded over the last so the program changes under running state. Two
// filter sections do the same through the six filter types, with notes starting and releasing on all
// sixteen slots. After every pass the two cores must hold the same state, and the first field that does not
// is printed with the pass it went wrong in. Exit code 0 when everything matched.
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include "fs1r/effects.h"
#include "fs1r/filter.h"

namespace H = fs1r::vop3host;

// The first field two cores differ in, or "" when they match. The delay RAM is only compared when asked:
// it is 2 MB.
static std::string diff(const yss236_core& a, const yss236_core& b, bool ram) {
    auto at = [](const char* name, const void* x, const void* y, size_t bytes, size_t elem) -> std::string {
        for (size_t i = 0; i < bytes / elem; i++)
            if (std::memcmp((const char*)x + i * elem, (const char*)y + i * elem, elem)) return std::string(name) + "[" + std::to_string(i) + "]";
        return "";
    };
    std::string w;
#define F(name, field) if (!(w = at(name, &a.field, &b.field, sizeof a.field, sizeof a.field[0])).empty()) return w;
#define S(field) if (std::memcmp(&a.field, &b.field, sizeof a.field)) return #field;
    F("r", r) F("m", m) F("t", t) F("out", out)
    S(acc) S(x) S(px) S(hasx) S(haspx) S(src) S(psrc) S(wlatch) S(haswl) S(idx) S(ptr) S(n) S(m_skip) S(m_flag) S(m_nhist) S(m_iqn)
    for (int i = 0; i < 4; i++) {
        const char* q[3] = {"m_rq", "m_dq", "m_tq"};
        const yss236_core::pend* pa[3] = {a.m_rq, a.m_dq, a.m_tq}; const yss236_core::pend* pb[3] = {b.m_rq, b.m_dq, b.m_tq};
        for (int k = 0; k < 3; k++)
            if (pa[k][i].reg != pb[k][i].reg || std::memcmp(&pa[k][i].v, &pb[k][i].v, 8)) return std::string(q[k]) + "[" + std::to_string(i) + "]";
    }
    for (int i = 0; i < a.m_iqn; i++)
        if (a.m_iq_at[i] != b.m_iq_at[i] || std::memcmp(&a.m_iq_v[i], &b.m_iq_v[i], 8)) return "m_iq[" + std::to_string(i) + "]";
    if (ram && !(w = at("ram", a.ram.data(), b.ram.data(), a.ram.size() * 8, 8)).empty()) return w;
    return "";
#undef F
#undef S
}

static uint32_t seed = 12345;
static double noise() { seed = seed * 1664525u + 1013904223u; return (int32_t)seed * (0.2 / 2147483648.0); }

static int fails = 0;
static long long passes = 0;
// one pass of both: false, and a report, at the first difference in the state or the output, or when b did
// not run compiled code (the comparison would be the interpreter against itself)
static bool same(const char* what, int type, int i, const yss236_core& a, const yss236_core& b, bool output, bool ram = false) {
    passes++;
    std::string w = !b.m_jit ? "nothing, b is not compiled," : diff(a, b, ram || i % 256 == 255);
    if (w.empty() && output) return true;
    std::printf("  %s %d: %s differs after pass %d\n", what, type, w.empty() ? "the output" : w.c_str(), i);
    fails++;
    return false;
}

// What the programs compared so far exercise
static int seen[8];
static void tally(const yss236_core& k) {
    for (const auto& o : k.ops) {
        seen[0] += o.gate; seen[1] += o.cskip == 6; seen[2] += o.cskip == 4; seen[3] += o.cskip == 1;
        seen[4] += !o.ctlop && o.dm && (o.msrc == 4 || o.msrc == 5); seen[5] += !o.ctlop && o.idxa; seen[6] += !o.ctlop && o.idxw;
        seen[7] += !o.ctlop && o.mmode >= 2 && o.dr && o.rcopy;
    }
}
static void coverage(const char* what) {
    std::printf("%-10s gates %d, skips if positive %d, if negative %d, always %d, captures %d, index reads %d, index loads %d, z^-1 copies %d\n",
        what, seen[0], seen[1], seen[2], seen[3], seen[4], seen[5], seen[6], seen[7]);
    std::memset(seen, 0, sizeof seen);
}

static uint64_t rng = 88172645463325252ull;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return uint32_t(rng); }

int main(int argc, char** argv) {
    const int N = argc > 1 ? std::atoi(argv[1]) : 9600;

    // The effect section: every type of each block, loaded over the last
    auto ea = std::make_unique<Effects>(), eb = std::make_unique<Effects>();
    ea->chip.core.interpret = true;
    uint8_t c[H::COMMON_BYTES] = {};
    c[0x10] = 127; c[0x11] = 64; c[0x12] = 24;
    c[H::EQ_LOW_GAIN] = c[H::EQ_MID_GAIN] = c[H::EQ_HIGH_GAIN] = 64;
    c[H::REVERB_PAN] = c[H::VARIATION_PAN] = c[H::INSERTION_PAN] = 64;
    c[H::REVERB_RETURN] = c[H::VARIATION_RETURN] = 100; c[H::INSERTION_LEVEL] = 127;
    c[H::INSERTION_TO_REVERB] = c[H::INSERTION_TO_VARIATION] = c[H::VARIATION_TO_REVERB] = 40;
    ea->load(c); eb->load(c);
    const struct { const char* name; int byte, types; } blocks[] = {
        {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
    for (auto& bl : blocks) {
        int bad = 0;
        for (int type = 0; type < bl.types; type++) {
            ea->host.setByte(bl.byte, type); eb->host.setByte(bl.byte, type);
            tally(eb->chip.core);
            for (int i = 0; i < N; i++) {
                const double x = noise() + (i % 4800 == 0 ? 0.9 : 0.0);
                const double bus[8] = {x, -x, x, 0.5 * x, x, x, -x, x};
                double la, ra, lb, rb;
                ea->run(bus, la, ra); eb->run(bus, lb, rb);
                if (!same(bl.name, type, i, ea->chip.core, eb->chip.core, !std::memcmp(&la, &lb, 8) && !std::memcmp(&ra, &rb, 8))) {
                    bad++;
                    break;
                }
            }
        }
        std::printf("%-10s %2d types: %s\n", bl.name, bl.types, bad ? "DIFFERS" : "identical");
    }
    coverage("effects");

    // The filter section: each of the six types on every slot, notes starting and releasing
    auto fa = std::make_unique<Filter>(), fb = std::make_unique<Filter>();
    fa->chip.core.interpret = true;
    int bad = 0;
    for (int type = 0; type < 6; type++) {
        using namespace H::filter;
        uint8_t v[H::FilterHost::VOICE_BYTES] = {}, r[H::FilterHost::PART_BYTES] = {};
        for (int i = 0x18; i <= 0x23; i++) r[i] = 64;
        r[PART_LFO2_SPEED] = r[PART_LFO2_DEPTH] = 64;
        v[FILTER_TYPE] = (uint8_t)type; v[CUTOFF] = 70; v[RESONANCE] = 90; v[KEY_SCALE_DEPTH] = 64; v[KEY_SCALE_POINT] = 60;
        v[INPUT_GAIN] = 12; v[EG_DEPTH] = 90; v[CUTOFF_LFO2] = 40;
        for (int i = EG_L4; i <= EG_L3; i++) v[i] = 20 + 10 * (i - EG_L4);
        for (Filter* f : {fa.get(), fb.get()}) {
            f->reset();
            for (int p = 0; p < 4; p++) f->sync(p, v, r, 0, 0, 0);
            for (int s = 0; s < 16; s++) f->host.noteOn(s, s & 3, 36 + 3 * s, 64 + 4 * s);
        }
        tally(fb->chip.core);
        for (int i = 0; i < N; i++) {
            if (i == N / 2) for (Filter* f : {fa.get(), fb.get()}) for (int s = 0; s < 16; s += 2) f->host.noteOff(s);
            double xa[16], xb[16];
            for (int s = 0; s < 16; s++) xa[s] = xb[s] = noise();
            fa->run(xa); fb->run(xb);
            if (!same("filter", type, i, fa->chip.core, fb->chip.core, !std::memcmp(xa, xb, sizeof xa))) {
                bad++;
                break;
            }
        }
    }
    std::printf("%-10s %2d types: %s\n", "filter", 6, bad ? "DIFFERS" : "identical");
    coverage("filter");

    // Random programs, coefficients and slot offsets: field combinations the firmware's programs may never
    // use, results that wrap, skips either way, gates and triggers, and words rewritten between passes.
    // A control step is kept to one in eight so that the skips leave something to run.
    {
        auto ka = std::make_unique<yss236_core>(), kb = std::make_unique<yss236_core>();
        ka->interpret = true;
        const int PROGRAMS = N / 24, PASSES = 64;
        bad = 0;
        for (int p = 0; p < PROGRAMS && !bad; p++) {
            for (yss236_core* k : {ka.get(), kb.get()}) k->reset();
            for (int st = 0; st < yss236_core::STEPS; st++) {
                uint16_t w[5];
                for (uint16_t& x : w) x = (uint16_t)rnd();
                if (rnd() % 8) w[1] &= 0x7fff;
                const uint16_t k = (uint16_t)rnd();
                for (yss236_core* c : {ka.get(), kb.get()}) { std::memcpy(c->prog[st], w, sizeof w); c->coef[st] = k; }
            }
            for (int s = 0; s < yss236_core::SLOTS; s++) {
                const uint16_t hi = (uint16_t)rnd(), lo = (uint16_t)rnd();
                for (yss236_core* c : {ka.get(), kb.get()}) { c->slot[s][0] = hi; c->slot[s][1] = lo; }
            }
            for (yss236_core* c : {ka.get(), kb.get()}) c->decode_all();
            tally(*kb);
            for (int i = 0; i < PASSES; i++) {
                for (int s = 0; s < 4 * yss236_core::SLOTS_IO; s++) ka->in[s / 16][s % 16] = kb->in[s / 16][s % 16] = (int32_t)rnd() * (16.0 / 2147483648.0);
                for (int s = 0; s < yss236_core::LFOS; s++) ka->lfo[s] = kb->lfo[s] = (int32_t)rnd() * (128.0 / 2147483648.0);
                if (rnd() % 8 == 0) { ka->trigger(); kb->trigger(); }
                if (rnd() % 4 == 0) {
                    const int st = rnd() % yss236_core::STEPS, w = rnd() % 5; const uint16_t v = (uint16_t)rnd();
                    for (yss236_core* c : {ka.get(), kb.get()}) { c->prog[st][w] = v; c->decode(st); }
                }
                ka->pass(); kb->pass();
                if (!same("random", p, i, *ka, *kb, true, i == PASSES - 1)) { bad++; break; }
            }
        }
        std::printf("%-10s %2d programs of %d passes: %s\n", "random", PROGRAMS, PASSES, bad ? "DIFFERS" : "identical");
        coverage("random");
    }
    std::printf("%lld passes compared, %s\n", passes, fails ? "FAIL" : "all identical");
    return fails ? 1 : 0;
}

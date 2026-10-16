// vop3_native_check - the native filters and effects against the FS1R's DSP programs on the chip interpreter, bit for bit.
//
//   vop3_native_check                  the filter, every effect program, then the self check
//   vop3_native_check <key> [passes]   one: flt, base, aux, or a window's program as ins-mNN, dly-mNN, var-mNN
//   vop3_native_check --selftest       the check must call a hair off different
//   vop3_native_check --map            the program each window holds under every effect type, as JSON lines
//   vop3_native_check --dump <key> [dir]   the program's steps as the chip holds them, decoded; with dir also
//                                      <dir>/<key>.prog and .coef for tools/vop3_disasm.py
//
// flt is the voice filter (filters/voice_filter.h) against the chip running the firmware's filter program,
// compared slot by slot every sample. The effects (effects/effects.h) run against the chip running the
// firmware's effect programs (vop3_chip_effects.h): both take the same performance, every effect type of
// each block in turn, the same parameter changes and the same input, and after every sample the two must
// hold the same registers, cells, t registers and outputs, and every 256 samples the same delay memory. Each
// type is loaded over the last, so a program starts under running state, and halfway through a type every
// effect parameter is set to a random value. A key checks the types whose window holds that program.
//
// The last line is for tools/vop3_native.py: RESULT <key> identical|differs|unported|unused <types> <passes>.
// Exit code 0 when everything compared identical, 1 when something differed or never ran, 2 for a bad key.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "effects/effects.h"
#include "filters/filter.h"
#include "vop3_chip_effects.h"
#include "vop3_chip_filter.h"

namespace H = fs1r::vop3host;
namespace N = fs1r::effects;

// What a key names: the voice filter, the base program, or a window's program
enum Area { FLT = -2, BASE = -1, AUX = H::AUX, INS = H::INSERTION, DLY = H::REVERB_DELAY, VAR = H::VARIATION };
struct Target { std::string key; int area; int msb; };

static bool parse(const std::string& key, Target& t) {
    t = {key, 0, 0};
    if (key == "flt") { t.area = FLT; return true; }
    if (key == "base" || key == "rev") { t.area = BASE; return true; }   // the reverb is part of the base program
    if (key == "aux") { t.area = AUX; return true; }
    const struct { const char* prefix; int area; } windows[] = {{"ins-m", INS}, {"dly-m", DLY}, {"var-m", VAR}};
    for (auto& w : windows)
        if (key.size() == 7 && !key.compare(0, 5, w.prefix) && std::isdigit((unsigned char)key[5]) && std::isdigit((unsigned char)key[6])) {
            t.area = w.area;
            t.msb = std::atoi(key.c_str() + 5);
            return true;
        }
    return false;
}

// How many programs the EPROM has for each window
static int programCount(int w) {
    switch (w) {
        case AUX: return 1;
        case INS: return int(std::size(H::rom::INSERTION_PROGRAMS)) / H::WINDOWS[w].nsteps;
        case DLY: return int(std::size(H::rom::REVERB_DELAY_PROGRAMS)) / H::WINDOWS[w].nsteps;
        case VAR: return int(std::size(H::rom::VARIATION_PROGRAMS)) / H::WINDOWS[w].nsteps;
    }
    return 0;
}

// The type msb whose program the window holds, or -1 when it holds none of the EPROM's
static int loaded(const yss236_core& c, int w) {
    const H::Window& win = H::WINDOWS[w];
    for (int msb = 0; msb < programCount(w); msb++) {
        bool same = true;
        for (int i = 0; i < win.nsteps && same; i++)
            same = !std::memcmp(c.prog[win.steps[i]], win.programs[msb * win.nsteps + i], sizeof c.prog[0]);
        if (same) return msb;
    }
    return -1;
}

// A step list as runs of consecutive steps. These are the window's own runs: a program's entry point is
// handed one of them per call, and rom::STEPS_* lists them in exactly this shape, so the maximal runs
// here are the ones the program already splits its block into.
static uint32_t seed;
static double noise() { seed = seed * 1664525u + 1013904223u; return (int32_t)seed * (0.2 / 2147483648.0); }
static uint64_t rng;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return uint32_t(rng); }

struct Tally { int types = 0; long long passes = 0; bool bad = false; };

// The steps a key's program runs at
static std::pair<int, int> stepsOf(const Target& t) {
    if (t.area == FLT) return {0, 512};
    if (t.area == BASE) return {0x000, 0x0e8};
    const H::Window& w = H::WINDOWS[t.area];
    return {w.steps[0], w.steps[0] + w.nsteps};
}

// Whether the chip holds the key's program now: the base program always, a window's when it was loaded
static bool holds(const yss236_core& c, const Target& t) {
    return t.area == BASE || t.area == AUX || loaded(c, t.area) == t.msb;
}

// The first thing two sections differ in after a sample, or "": the outputs, then the state the programs
// share. The chip's latches and pipeline are not compared: the native section has none, and a program reads
// none of them from before it (tools/fx_lift --seal).
static std::string differs(const yss236_core& chip, const Effects& b, double la, double ra, double lb, double rb, bool ram) {
    // the chip's last three steps' writes are still in its pipeline at the end of a sample; land them
    yss236_core::pend const* q[3] = {chip.m_rq, chip.m_dq, chip.m_tq};
    struct { double r[128], m[64], t[16]; } a;
    std::memcpy(a.r, chip.r, sizeof a.r); std::memcpy(a.m, chip.m, sizeof a.m); std::memcpy(a.t, chip.t, sizeof a.t);
    for (int i = 1; i <= 4; i++) {
        const int k = int((chip.n + i) & 3);
        if (q[0][k].reg > 0) a.r[q[0][k].reg] = q[0][k].v;
        if (q[1][k].reg >= 0) a.m[q[1][k].reg] = q[1][k].v;
        if (q[2][k].reg >= 0) a.t[q[2][k].reg] = q[2][k].v;
    }
    auto at = [](const char* name, const double* x, const double* y, int n) -> std::string {
        for (int i = 0; i < n; i++) if (std::memcmp(&x[i], &y[i], 8)) return std::string(name) + "[" + std::to_string(i) + "]";
        return "";
    };
    std::string w;
    if (std::memcmp(&la, &lb, 8) || std::memcmp(&ra, &rb, 8)) return "the output";
    if (!(w = at("r", a.r, b.state.r, 128)).empty() || !(w = at("m", a.m, b.state.m, 64)).empty() ||
        !(w = at("t", a.t, b.state.t, 16)).empty() || !(w = at("out", chip.out, b.state.out, 16)).empty()) return w;
    if (chip.ptr != b.state.ptr) return "the ring pointer";
    if (ram) for (int i = 0; i < yss236_core::RAM_WORDS; i++)
        if (std::memcmp(&chip.ram[i], &b.state.ram[i], 8)) return "ram[" + std::to_string(i) + "]";
    return "";
}

static void performance(uint8_t* c) {
    std::memset(c, 0, H::COMMON_BYTES);
    c[0x10] = 127; c[0x11] = 64; c[0x12] = 24;
    c[H::EQ_LOW_GAIN] = c[H::EQ_MID_GAIN] = c[H::EQ_HIGH_GAIN] = 64;
    c[H::REVERB_PAN] = c[H::VARIATION_PAN] = c[H::INSERTION_PAN] = 64;
    c[H::REVERB_RETURN] = c[H::VARIATION_RETURN] = 100; c[H::INSERTION_LEVEL] = 127;
    c[H::INSERTION_TO_REVERB] = c[H::INSERTION_TO_VARIATION] = c[H::VARIATION_TO_REVERB] = 40;
}

// Every effect type of each block, on the chip and on the native section. A key's program counts the
// samples its window held it (the key "" counts every sample). `tamper` puts a hair on one register of the
// native section a sample, which the self check expects to be caught.
static void checkEffects(const Target* t, int n, Tally& tl, bool quiet, bool tamper = false) {
    auto ea = std::make_unique<ChipEffects>();
    auto eb = std::make_unique<Effects>();
    uint8_t c[H::COMMON_BYTES];
    performance(c);
    ea->load(c); eb->load(c);
    const struct { const char* name; uint32_t byte; int types; } blocks[] = {
        {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
    for (auto& bl : blocks)
        for (int type = 0; type < bl.types; type++) {
            ea->host.setByte(bl.byte, type); eb->host.setByte(bl.byte, type);
            const bool runs = !t || holds(ea->chip.core, *t);
            const int passes = runs ? n : 16;   // a type without the program still runs a little, so state carries over
            tl.types += runs;
            for (int i = 0; i < passes; i++) {
                if (runs && i == passes / 2) {
                    // every effect parameter, pan, send, return and EQ byte at random, the types left alone
                    uint8_t ca[H::COMMON_BYTES], cb[H::COMMON_BYTES];
                    std::memcpy(ca, ea->host.perf, sizeof ca);
                    for (uint32_t k = H::REVERB_PARAM_BYTES; k <= H::EQ_HIGH_SHAPE; k++)
                        if (k != H::REVERB_TYPE && k != H::VARIATION_TYPE && k != H::INSERTION_TYPE) ca[k] = uint8_t(rnd() % 128);
                    std::memcpy(cb, ca, sizeof cb);
                    for (uint32_t k = 0; k < H::COMMON_BYTES; k++)
                        if (ca[k] != ea->host.perf[k]) { ea->host.store(k, ca[k]); ea->host.byteChanged(k); }
                    ea->host.runPosted();
                    eb->sync(cb);
                }
                const double x = noise() + (i % 4800 == 0 ? 0.9 : 0.0);
                const double bus[8] = {x, -x, x, 0.5 * x, x, x, -x, x};
                double la, ra, lb, rb;
                ea->run(bus, la, ra); eb->run(bus, lb, rb);
                if (tamper) eb->state.r[15] += 1.0 / 65536;
                tl.passes += runs;
                const std::string w = differs(ea->chip.core, *eb, la, ra, lb, rb, i % 256 == 255 || i == passes - 1);
                if (!w.empty()) {
                    if (!quiet) std::printf("  %s type %d: %s differs after sample %d\n", bl.name, type, w.c_str(), i);
                    tl.bad = true;
                    return;
                }
            }
        }
}

// The voice filter: fs1r::filters::VoiceFilter, sixteen of them in a Filter::Bank,
// against the chip running the firmware's filter program (vop3_chip_filter.h), every slot's output compared
// every sample. The six filter types, notes starting and releasing, cutoff and resonance moving halfway, the
// host's clear, and a slot killed and taken again; `tamper` puts a hair on one voice's state each sample,
// which the self check expects to be caught.
static void checkFilter(int n, Tally& tl, bool quiet, bool tamper = false) {
    auto fa = std::make_unique<ChipFilter>();
    auto fb = std::make_unique<Filter::Bank>();
    for (int type = 0; type < 6; type++) {
        using namespace H::filter;
        uint8_t v[H::FilterHost::VOICE_BYTES] = {}, r[H::FilterHost::PART_BYTES] = {};
        for (int i = 0x18; i <= 0x23; i++) r[i] = 64;
        r[PART_LFO2_SPEED] = r[PART_LFO2_DEPTH] = 64;
        v[FILTER_TYPE] = (uint8_t)type; v[CUTOFF] = 70; v[RESONANCE] = 90; v[KEY_SCALE_DEPTH] = 64; v[KEY_SCALE_POINT] = 60;
        v[INPUT_GAIN] = 12; v[EG_DEPTH] = 90; v[CUTOFF_LFO2] = 40; v[LFO2_WAVE] = uint8_t(type % 6); v[LFO2_KEY_SYNC] = type & 1;
        for (int i = EG_L4; i <= EG_L3; i++) v[i] = 20 + 10 * (i - EG_L4);
        auto both = [&](auto&& f) { f(*fa); f(*fb); };
        both([&](auto& f) {
            f.reset();
            for (int p = 0; p < 4; p++) f.sync(p, v, r, 0, 0, 0);
            for (int s = 0; s < 16; s++) f.host.noteOn(s, s & 3, 36 + 3 * s, 64 + 4 * s);
        });
        tl.types++;
        for (int i = 0; i < n; i++) {
            if (i == n / 2) {
                v[CUTOFF] = uint8_t(rnd() % 128); v[RESONANCE] = uint8_t(rnd() % 128);
                const int mod = int(rnd() % 255) - 127;
                both([&](auto& f) {
                    for (int p = 0; p < 4; p++) f.sync(p, v, r, mod, mod / 2, -mod / 3);
                    for (int s = 0; s < 16; s += 2) f.host.noteOff(s);
                    f.host.setSlotCutoffMod(3, mod); f.host.flush();
                });
            }
            if (i == 3 * n / 4) both([&](auto& f) { f.host.kill(5); f.host.noteOn(5, 2, 77, 30); });
            double xa[16], xb[16];
            for (int s = 0; s < 16; s++) xa[s] = xb[s] = noise() + (i % 2400 == 0 ? 0.9 : 0.0);
            fa->run(xa); fb->run(xb);
            if (tamper) fb->voice[7].s3 += 1.0 / 65536;
            tl.passes++;
            for (int s = 0; s < 16; s++)
                if (std::memcmp(&xa[s], &xb[s], 8)) {
                    if (!quiet) std::printf("  filter type %d: slot %d differs after sample %d (chip %.17g, native %.17g)\n", type, s, i, xa[s], xb[s]);
                    tl.bad = true;
                    return;
                }
        }
    }
}

// One key: "identical", "differs" or "unused" (no type loads it)
static const char* check(const Target& t, int n, Tally& tl, bool quiet = false, bool tamper = false) {
    seed = 12345; rng = 88172645463325252ull;
    tl = {};
    if (t.area == FLT) checkFilter(n, tl, quiet, tamper); else checkEffects(&t, n, tl, quiet, tamper);
    if (tl.bad) return "differs";
    if (!tl.types) return "unused";
    return "identical";
}

// The check must call a voice or a program a hair off different
static int selfTest() {
    int fails = 0;
    for (const char* key : {"flt", "base"}) {
        Target t; parse(key, t);
        Tally tl;
        const std::string off = check(t, 256, tl, true, true);
        const bool good = off == "differs";
        std::printf("selftest %-5s a hair off %s: %s\n", key, off.c_str(), good ? "ok" : "FAIL");
        fails += !good;
    }
    return fails;
}

// Which program each window holds under every effect type, for the tracker
static void map() {
    auto e = std::make_unique<ChipEffects>();
    uint8_t c[H::COMMON_BYTES] = {};
    e->load(c);
    const struct { const char* name; uint32_t byte; int types; } blocks[] = {
        {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
    for (auto& bl : blocks)
        for (int type = 0; type < bl.types; type++) {
            e->host.setByte(bl.byte, type);
            const yss236_core& k = e->chip.core;
            std::printf("{\"block\": \"%s\", \"type\": %d, \"aux\": %d, \"ins\": %d, \"dly\": %d, \"var\": %d}\n",
                bl.name, type, loaded(k, AUX), loaded(k, INS), loaded(k, DLY), loaded(k, VAR));
        }
}

// The program's steps as the core holds them under the first type that runs it: words, constant and decoded
// fields per step. With a folder, also the whole program and its constants in vop3_disasm.py's format
// (big-endian): python3 tools/vop3_disasm.py <dir>/<key>.prog --coef <dir>/<key>.coef
static int dump(const std::string& key, const char* dir) {
    Target t;
    if (!parse(key, t)) { std::printf("no program is called %s\n", key.c_str()); return 2; }
    auto e = std::make_unique<ChipEffects>();
    auto f = std::make_unique<ChipFilter>();
    const yss236_core* k = &f->chip.core;
    std::string under = "the filter chip as booted, every slot's type at LPF24";
    if (t.area != FLT) {
        uint8_t c[H::COMMON_BYTES] = {};
        e->load(c);
        k = nullptr;
        const struct { const char* name; uint32_t byte; int types; } blocks[] = {
            {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
        for (auto& bl : blocks)
            for (int type = 0; type < bl.types && !k; type++) {
                e->host.setByte(bl.byte, type);
                if (holds(e->chip.core, t)) { k = &e->chip.core; under = std::string(bl.name) + " type " + std::to_string(type); }
            }
        if (!k) { std::printf("no effect type loads %s\n", key.c_str()); return 1; }
    }
    const auto [from, to] = stepsOf(t);
    std::printf("%s under %s: steps 0x%03x-0x%03x\n", key.c_str(), under.c_str(), from, to - 1);
    for (int st = from; st < to; st++) {
            const yss236_core::op& o = k->ops[st];
            const uint16_t* w = k->prog[st];
            std::printf("0x%03x  %04x %04x %04x %04x %04x  k=%04x(%+.6f)  ", st, w[0], w[1], w[2], w[3], w[4], k->coef[st], int16_t(k->coef[st]) / 32768.0);
            if (o.ctlop) { std::printf("control: %s target 0x%03x\n", o.gate ? "gate" : "skip", (st & 0x100) | o.target); continue; }
            std::printf("class %d dr %d sr %d sm %d dm %d msrc %d asel %d m1 %d t %d m2m %d sub %d shift %d clamp %d memop %d idxa %d idxw %d tbl %d wdata %d tfrac %d tw %d tn %d rcopy %d sout %d cmp %d dwr %d\n",
                o.mmode, o.dr, o.sr, o.sm, o.dm, o.msrc, o.asel, o.m1, o.t, o.m2m, o.sub, o.shift, o.clamp, o.memop, o.idxa, o.idxw, o.tbl, o.wdata, o.tfrac, o.tw, o.tn, o.rcopy, o.sout, o.cmp, o.dwr);
    }
    if (dir) {
        const std::string base = std::string(dir) + "/" + key;
        FILE* pf = std::fopen((base + ".prog").c_str(), "wb");
        FILE* cf = std::fopen((base + ".coef").c_str(), "wb");
        if (!pf || !cf) { std::printf("cannot write %s.prog / .coef\n", base.c_str()); return 1; }
        for (int st = 0; st < yss236_core::STEPS; st++) {
            for (int i = 0; i < 5; i++) { const uint8_t b[2] = {uint8_t(k->prog[st][i] >> 8), uint8_t(k->prog[st][i])}; std::fwrite(b, 1, 2, pf); }
            const uint8_t b[2] = {uint8_t(k->coef[st] >> 8), uint8_t(k->coef[st])}; std::fwrite(b, 1, 2, cf);
        }
        std::fclose(pf); std::fclose(cf);
        std::printf("wrote %s.prog and %s.coef\n", base.c_str(), base.c_str());
    }
    return 0;
}

static int one(const std::string& key, int n) {
    Target t;
    if (!parse(key, t)) { std::printf("no program is called %s (flt, base, aux, ins-mNN, dly-mNN, var-mNN)\nRESULT %s unported 0 0\n", key.c_str(), key.c_str()); return 2; }
    Tally tl;
    const char* r = check(t, n, tl);
    std::printf("%-7s %d types, %lld samples: %s\nRESULT %s %s %d %lld\n", key.c_str(), tl.types, tl.passes, r, key.c_str(), r, tl.types, tl.passes);
    return std::strcmp(r, "identical") ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && !std::strcmp(argv[1], "--map")) { map(); return 0; }
    if (argc > 1 && !std::strcmp(argv[1], "--selftest")) return selfTest() ? 1 : 0;
    if (argc > 2 && !std::strcmp(argv[1], "--dump")) return dump(argv[2], argc > 3 ? argv[3] : nullptr);
    if (argc > 1) return one(argv[1], argc > 2 ? std::atoi(argv[2]) : 9600);
    int fails = one("flt", 9600) != 0;
    // every type of every block once, all programs at once, and the delay memory compared every 256 samples
    Tally tl;
    seed = 12345; rng = 88172645463325252ull;
    checkEffects(nullptr, 2400, tl, false);
    std::printf("effects %d types, %lld samples: %s\n", tl.types, tl.passes, tl.bad ? "differs" : "identical");
    fails += tl.bad;
    fails += selfTest();
    std::printf("%s\n", fails ? "FAIL" : "all identical");
    return fails ? 1 : 0;
}

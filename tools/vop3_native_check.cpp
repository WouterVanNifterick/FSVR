// vop3_native_check - the VOP3 programs written in C++ (src/effects/) against the interpreter, bit for bit.
//
//   vop3_native_check                  every ported program, then the self check
//   vop3_native_check <key> [passes]   one program, default 9600 passes for each type that runs it
//   vop3_native_check --selftest       a native that only calls step() must match, one a hair off must not
//   vop3_native_check --map            the program each window holds under every effect type, as JSON lines
//   vop3_native_check --dump <key> [dir]   the program's steps as the core holds them, decoded; with dir also
//                                      <dir>/<key>.prog and .coef for tools/vop3_disasm.py
//
// flt is the voice filter (filters/voice_filter.h) against the chip running the firmware's filter program,
// compared slot by slot every sample. For the effect programs: two effect sections take the same performance,
// the same parameter changes
// and the same input. Both run on the interpreter, and one hands the program's steps to the native function
// whenever that program is loaded. After every pass the two cores must hold the same state, and the first
// field that differs is printed with the type and the pass. Each type is loaded over the last, so a program
// starts under running state, and halfway through a type's passes every effect parameter is set to a random
// value, so its constants change under it too.
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
#include "effects/programs.h"
#include "vop3_chip_filter.h"
#include "vop3_state_diff.h"

namespace H = fs1r::vop3host;
namespace N = fs1r::effects;
using Ranges = std::vector<yss236_core::native_run>;

// The program under test runs through this, so the check can tell that it ran at all.
static N::Run g_run;
static long long g_calls;
static void counted(yss236_core& c, int from, int to) { g_calls++; g_run(c, from, to); }

// What a key names: the filter chip, the base program or the reverb's steps of it, or a window's program
enum Area { FLT = -2, BASE = -1, REV = H::REVERB, AUX = H::AUX, INS = H::INSERTION, DLY = H::REVERB_DELAY, VAR = H::VARIATION };
struct Target { std::string key; int area; int msb; };

static bool parse(const std::string& key, Target& t) {
    t = {key, 0, 0};
    if (key == "flt") { t.area = FLT; return true; }
    if (key == "base") { t.area = BASE; return true; }
    if (key == "rev") { t.area = REV; return true; }
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
static Ranges runs(const std::vector<bool>& in) {
    Ranges r;
    for (int st = 0; st < yss236_core::STEPS; st++)
        if (in[st]) {
            if (!r.empty() && r.back().to == st) r.back().to++;
            else r.push_back({st, st + 1, counted});
        }
    return r;
}
static std::vector<bool> windowSteps(int w) {
    std::vector<bool> in(yss236_core::STEPS);
    for (int i = 0; i < H::WINDOWS[w].nsteps; i++) in[H::WINDOWS[w].steps[i]] = true;
    return in;
}

// Where the target's native runs on this core now: nothing while its program is not loaded
static Ranges rangesFor(const Target& t, const yss236_core& c) {
    if (t.area == FLT) return {{0, yss236_core::STEPS, counted}};
    if (t.area == BASE) {
        std::vector<bool> in(yss236_core::STEPS, true);
        for (int w = 0; w < 5; w++)
            for (int i = 0; i < H::WINDOWS[w].nsteps; i++) in[H::WINDOWS[w].steps[i]] = false;
        return runs(in);
    }
    if (t.area == REV) return runs(windowSteps(REV));
    return loaded(c, t.area) == t.msb ? runs(windowSteps(t.area)) : Ranges{};
}

static uint32_t seed;
static double noise() { seed = seed * 1664525u + 1013904223u; return (int32_t)seed * (0.2 / 2147483648.0); }
static uint64_t rng;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return uint32_t(rng); }

struct Tally { int types = 0; long long passes = 0; bool bad = false; };

// The delay RAM, 2 MB, is compared every 256 passes and at the end of each type
static bool same(const char* what, int type, int i, const yss236_core& a, const yss236_core& b, bool output, bool quiet, bool ram = false) {
    const std::string w = vop3_state_diff(a, b, ram || i % 256 == 255);
    if (w.empty() && output) return true;
    if (!quiet) std::printf("  %s type %d: %s differs after pass %d\n", what, type, w.empty() ? "the output" : w.c_str(), i);
    return false;
}

// Every effect type of each block on two effect sections, the native standing in on one
static void checkEffects(const Target& t, int n, Tally& tl, bool quiet) {
    auto ea = std::make_unique<Effects>(), eb = std::make_unique<Effects>();
    // a is the interpreter and nothing else; b runs the target's steps in C++ where it can
    ea->chip.core.interpret = true;
    uint8_t c[H::COMMON_BYTES] = {};
    c[0x10] = 127; c[0x11] = 64; c[0x12] = 24;
    c[H::EQ_LOW_GAIN] = c[H::EQ_MID_GAIN] = c[H::EQ_HIGH_GAIN] = 64;
    c[H::REVERB_PAN] = c[H::VARIATION_PAN] = c[H::INSERTION_PAN] = 64;
    c[H::REVERB_RETURN] = c[H::VARIATION_RETURN] = 100; c[H::INSERTION_LEVEL] = 127;
    c[H::INSERTION_TO_REVERB] = c[H::INSERTION_TO_VARIATION] = c[H::VARIATION_TO_REVERB] = 40;
    ea->load(c); eb->load(c);
    const struct { const char* name; uint32_t byte; int types; } blocks[] = {
        {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
    for (auto& bl : blocks)
        for (int type = 0; type < bl.types; type++) {
            ea->host.setByte(bl.byte, type); eb->host.setByte(bl.byte, type);
            // The check runs one program at a time and counts the calls, so it owns the ranges: it re-pins
            // them before every pass, since a host write in between would otherwise install the whole set.
            const Ranges pin = rangesFor(t, eb->chip.core);
            auto pinRanges = [&]{ eb->chip.core.natives = pin; eb->chip.programDirty = false; };
            pinRanges();
            const bool runs = !pin.empty();
            // a type that does not run the program still runs both sections a little, so state carries over
            const int passes = runs ? n : 16;
            tl.types += runs;
            for (int i = 0; i < passes; i++) {
                if (runs && i == passes / 2) {
                    // every effect parameter, pan, send, return and EQ byte at random, the types left alone
                    uint8_t ca[H::COMMON_BYTES], cb[H::COMMON_BYTES];
                    std::memcpy(ca, ea->host.perf, sizeof ca);
                    for (uint32_t k = H::REVERB_PARAM_BYTES; k <= H::EQ_HIGH_SHAPE; k++)
                        if (k != H::REVERB_TYPE && k != H::VARIATION_TYPE && k != H::INSERTION_TYPE) ca[k] = uint8_t(rnd() % 128);
                    std::memcpy(cb, ca, sizeof cb);
                    ea->sync(ca); eb->sync(cb);
                }
                const double x = noise() + (i % 4800 == 0 ? 0.9 : 0.0);
                const double bus[8] = {x, -x, x, 0.5 * x, x, x, -x, x};
                double la, ra, lb, rb;
                pinRanges();
                ea->run(bus, la, ra); eb->run(bus, lb, rb);
                tl.passes += runs;
                if (!same(bl.name, type, i, ea->chip.core, eb->chip.core, !std::memcmp(&la, &lb, 8) && !std::memcmp(&ra, &rb, 8), quiet)) {
                    tl.bad = true;
                    return;
                }
            }
            if (runs && !same(bl.name, type, passes - 1, ea->chip.core, eb->chip.core, true, quiet, true)) { tl.bad = true; return; }
        }
}

// The self check's natives: the interpreter itself, and the interpreter leaving every register a hair off.
// Not the running value: a later step overwrites it before anything reads it, so that native is no different
// from the program, and the check rightly calls it identical.
static void passThrough(yss236_core& c, int from, int to) { for (int st = from; st < to; st++) c.step(st); }
static void hairOff(yss236_core& c, int from, int to) { passThrough(c, from, to); for (double& v : c.r) v += 1.0 / 65536; }

// The filter has no chip program any more: fs1r::filters::VoiceFilter, sixteen of them in a Filter::Bank,
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

// One program: "identical", "differs" (also when it never ran) or "unused" (no type loads it)
static const char* check(const Target& t, N::Run run, int n, Tally& tl, bool quiet = false) {
    g_run = run; g_calls = 0; seed = 12345; rng = 88172645463325252ull;
    tl = {};
    if (t.area == FLT) checkFilter(n, tl, quiet, run == hairOff); else checkEffects(t, n, tl, quiet);
    if (tl.bad) return "differs";
    if (!tl.types) return "unused";
    if (t.area != FLT && !g_calls) { if (!quiet) std::printf("  the native was attached but never called\n"); return "differs"; }
    return "identical";
}

// What actually ships: the ranges fs1r::effects::install() picks from the words the chip holds, rather
// than the ones this file would pick for a single key. Every program at once, on both chips, against the
// interpreter, so a program installed on the wrong steps or missing from a window's own shows up here.
static int installedCheck(int n) {
    Tally tl;
    g_run = nullptr;
    int fails = 0;
    {   // the effect chip: every block on every type, with install() picking all of them at once
        auto ea = std::make_unique<Effects>(), eb = std::make_unique<Effects>();
        ea->chip.core.interpret = true;
        eb->chip.core.interpret = false;
        uint8_t c[H::COMMON_BYTES] = {};
        c[0x10] = 127; c[0x11] = 64; c[0x12] = 24;
        c[H::EQ_LOW_GAIN] = c[H::EQ_MID_GAIN] = c[H::EQ_HIGH_GAIN] = 64;
        c[H::REVERB_PAN] = c[H::VARIATION_PAN] = c[H::INSERTION_PAN] = 64;
        c[H::REVERB_RETURN] = c[H::VARIATION_RETURN] = 100; c[H::INSERTION_LEVEL] = 127;
        c[H::INSERTION_TO_REVERB] = c[H::INSERTION_TO_VARIATION] = c[H::VARIATION_TO_REVERB] = 40;
        ea->load(c); eb->load(c);
        const struct { const char* name; uint32_t byte; int types; } blocks[] = {
            {"reverb", H::REVERB_TYPE, 17}, {"variation", H::VARIATION_TYPE, 29}, {"insertion", H::INSERTION_TYPE, 41}};
        for (auto& bl : blocks)
            for (int type = 0; type < bl.types; type++) {
                ea->host.setByte(bl.byte, type); eb->host.setByte(bl.byte, type);
                N::install_effect(eb->chip.core);
                tl.types++;
                for (int i = 0; i < n; i++) {
                    if (i == n / 2) {
                        uint8_t ca[H::COMMON_BYTES], cb[H::COMMON_BYTES];
                        std::memcpy(ca, ea->host.perf, sizeof ca);
                        for (uint32_t k = H::REVERB_PARAM_BYTES; k <= H::EQ_HIGH_SHAPE; k++)
                            if (k != H::REVERB_TYPE && k != H::VARIATION_TYPE && k != H::INSERTION_TYPE) ca[k] = uint8_t(rnd() % 128);
                        std::memcpy(cb, ca, sizeof cb);
                        ea->sync(ca); eb->sync(cb);
                        N::install_effect(eb->chip.core);
                    }
                    const double x = noise() + (i % 4800 == 0 ? 0.9 : 0.0);
                    const double bus[8] = {x, -x, x, 0.5 * x, x, x, -x, x};
                    double la, ra, lb, rb;
                    ea->run(bus, la, ra); eb->run(bus, lb, rb);
                    tl.passes++;
                    if (!same((std::string("installed ") + bl.name).c_str(), type, i, ea->chip.core, eb->chip.core,
                              !std::memcmp(&la, &lb, 8) && !std::memcmp(&ra, &rb, 8), false)) { fails++; break; }
                }
                N::install_effect(eb->chip.core);
            }
    }
    std::printf("installed  %d type-settings, %lld passes: %s\n", tl.types, tl.passes, fails ? "differs" : "identical");
    return fails;
}


static int selfTest() {
    int fails = 0;
    for (const char* key : {"flt", "base", "rev", "aux"}) {
        Target t; parse(key, t);
        Tally tl;
        const std::string ok = check(t, passThrough, 256, tl, true), off = check(t, hairOff, 256, tl, true);
        const bool good = ok == "identical" && off == "differs";
        std::printf("selftest %-5s pass-through %s, a hair off %s: %s\n", key, ok.c_str(), off.c_str(), good ? "ok" : "FAIL");
        fails += !good;
    }
    return fails;
}

// Which program each window holds under every effect type, for the tracker
static void map() {
    auto e = std::make_unique<Effects>();
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
    auto e = std::make_unique<Effects>();
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
                if (!rangesFor(t, e->chip.core).empty()) { k = &e->chip.core; under = std::string(bl.name) + " type " + std::to_string(type); }
            }
        if (!k) { std::printf("no effect type loads %s\n", key.c_str()); return 1; }
    }
    const Ranges r = rangesFor(t, *k);
    std::printf("%s under %s: steps", key.c_str(), under.c_str());
    for (auto& x : r) std::printf(" 0x%03x-0x%03x", x.from, x.to - 1);
    std::printf("\n");
    for (auto& x : r)
        for (int st = x.from; st < x.to; st++) {
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

static int one(const std::string& key, N::Run run, int n) {
    Target t;
    if (!parse(key, t)) { std::printf("no program is called %s (flt, base, rev, aux, ins-mNN, dly-mNN, var-mNN)\nRESULT %s unported 0 0\n", key.c_str(), key.c_str()); return 2; }
    if (!run && t.area != FLT) { std::printf("%s has no native in src/effects/programs.cpp yet\nRESULT %s unported 0 0\n", key.c_str(), key.c_str()); return 2; }
    Tally tl;
    const char* r = check(t, run, n, tl);
    std::printf("%-6s %d types, %lld native passes: %s\nRESULT %s %s %d %lld\n", key.c_str(), tl.types, tl.passes, r, key.c_str(), r, tl.types, tl.passes);
    return std::strcmp(r, "identical") ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && !std::strcmp(argv[1], "--map")) { map(); return 0; }
    if (argc > 1 && !std::strcmp(argv[1], "--selftest")) return selfTest() ? 1 : 0;
    if (argc > 2 && !std::strcmp(argv[1], "--dump")) return dump(argv[2], argc > 3 ? argv[3] : nullptr);
    if (argc > 1 && !std::strcmp(argv[1], "--installed")) return installedCheck(argc > 2 ? std::atoi(argv[2]) : 480) ? 1 : 0;
    if (argc > 1) {
        const N::Program* p = N::find(argv[1]);
        return one(argv[1], p ? p->run : nullptr, argc > 2 ? std::atoi(argv[2]) : 9600);
    }
    int fails = one("flt", nullptr, 9600) != 0;      // the voice filter, which is no chip program
    for (const N::Program* p = N::PROGRAMS; p->key; p++) fails += one(p->key, p->run, 2400) != 0;
    fails += selfTest();
    fails += installedCheck(2400);
    std::printf("%s\n", fails ? "FAIL" : "all identical");
    return fails ? 1 : 0;
}

// FSVR on Hollow: the FS1R engine (fs1rLib) behind the FSVR skin. The processor is a controller for
// fs1r::Device and nothing more (AGENTS.md): every param is a sysex parameter change into the engine, as
// the skin's data/fs1r_sysex.json addresses it, and what the engine holds comes back out of its own bulk
// dumps. Around that sit the parts of the GUI the unit never had: the bank manager (factory banks and a
// user library of .syx files, library.h), the morph square (four corner voices per part, blended into the
// one voice the engine plays), Import Audio (audio_fseq.h), the output monitor and the file menu.
//
// Threads: the audio thread sends param changes and MIDI and renders; a worker loads patches, files and
// libraries and brings what the engine did back into the params. ctl guards what both touch (the model of
// the engine's bytes, the morph corners); the audio thread only ever try-locks it.
#include <hollow/hollow.h>
#include "core/core.h"
#include "embedded_skin.h"
#include "audio_fseq.h"
#include "fs1r.h"
#include "library.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#include <xmmintrin.h>
#endif

namespace hollow {   // the factory banks, embedded by plugin/CMakeLists.txt
extern const EmbeddedFile kFactoryFiles[];
extern const size_t kFactoryFileCount;
}

namespace fsvr {

using hollow::Json;
using hollow::State;

// ---- small helpers ---------------------------------------------------------------------------------

// Denormals flushed to zero while the engine renders (docs/performance.md: without it a heavy
// performance costs half as much again), the host's own mode back afterwards.
struct NoDenormals {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    unsigned old = _mm_getcsr();
    NoDenormals() { _mm_setcsr(old | 0x8040); }   // flush to zero, denormals are zero
    ~NoDenormals() { _mm_setcsr(old); }
#elif defined(__aarch64__)
    uint64_t old = 0;
    NoDenormals() {
        asm volatile("mrs %0, fpcr" : "=r"(old));
        asm volatile("msr fpcr, %0" : : "r"(old | (1ull << 24)));   // FZ
    }
    ~NoDenormals() { asm volatile("msr fpcr, %0" : : "r"(old)); }
#endif
};

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string b64(const uint8_t* d, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        for (int k = 0; k < 4; ++k) s += k <= (int)std::min<size_t>(n - i, 3) ? kB64[v >> (18 - 6 * k) & 63] : '=';
    }
    return s;
}

static std::vector<uint8_t> unb64(const std::string& s) {
    std::vector<uint8_t> out;
    uint32_t v = 0;
    int bits = 0;
    for (char c : s) {
        const char* p = std::strchr(kB64, c);
        if (!p || !c) continue;
        v = v << 6 | (uint32_t)(p - kB64);
        if ((bits += 6) >= 8) out.push_back((uint8_t)(v >> (bits -= 8)));
    }
    return out;
}

static const hollow::EmbeddedFile* factoryFile(const char* name) {
    for (size_t i = 0; i < hollow::kFactoryFileCount; ++i)
        if (!std::strcmp(hollow::kFactoryFiles[i].path, name)) return &hollow::kFactoryFiles[i];
    return nullptr;
}

static std::vector<uint8_t> factoryBytes(const char* name) {
    const hollow::EmbeddedFile* f = factoryFile(name);
    return f ? std::vector<uint8_t>(f->data, f->data + f->size) : std::vector<uint8_t>();
}

// The skin's own copy of a data file: HOLLOW_SKIN_DIR's when set (live editing), else the embedded one.
static std::string skinFile(const std::string& path) {
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
    const char* dir = std::getenv("HOLLOW_SKIN_DIR");
    if (dir && *dir) {
        std::vector<uint8_t> b;
        if (readFile(std::string(dir) + "/" + path, b)) return std::string(b.begin(), b.end());
    }
    for (size_t i = 0; i < hollow::kSkinFileCount; ++i)
        if (path == hollow::kSkinFiles[i].path) return std::string((const char*)hollow::kSkinFiles[i].data, hollow::kSkinFiles[i].size);
    return {};
}

// The factory voice number (the EPROM's order: PrA, PrB native, PrC..PrK DX) of a part's bank and
// program bytes; -1 for off and Int (engine: bank_voice_index).
static int factoryVoice(int bank, int program) {
    program = std::clamp(program, 0, 127);
    if (bank == 2 || bank == 3) return (bank - 2) * 128 + program;
    if (bank >= 4 && bank <= 12) return 256 + (bank - 4) * 128 + program;
    return -1;
}

// ---- the engine's bytes, and where each param lives in them ----------------------------------------

enum Area { System, Performance, Voice };

struct Model {
    uint8_t sys[76] = {}, perf[400] = {}, voice[4][608] = {};
    uint8_t* area(int a, int part) { return a == System ? sys : a == Performance ? perf : voice[part]; }
    // The engine's bulk dumps (Device::getState) into these; false if one was missing.
    bool parse(const std::vector<uint8_t>& s, std::vector<uint8_t>* fseq = nullptr) {
        int seen = 0;
        for (size_t i = 0; i + 11 < s.size();) {
            if (s[i] != 0xF0) { ++i; continue; }
            size_t j = i + 1;
            while (j < s.size() && s[j] != 0xF7) ++j;
            if (j >= s.size()) break;
            const int ah = s[i + 6];
            const uint8_t* d = s.data() + i + 9;
            const size_t n = j - 1 - (i + 9);
            if (ah == 0x00 && n >= 76) { std::memcpy(sys, d, 76); seen |= 1; }
            if (ah == 0x10 && n >= 400) { std::memcpy(perf, d, 400); seen |= 2; }
            if (ah >= 0x40 && ah <= 0x43 && n >= 608) { std::memcpy(voice[ah - 0x40], d, 608); seen |= 4 << (ah - 0x40); }
            if (ah == 0x60 && fseq) fseq->assign(d, d + n);
            i = j + 1;
        }
        return seen == 63;
    }
};

struct Field {
    int h = 0, m = 0, l = 0, shift = 0, width = 0, k = 0;
    bool wide = false;
    double x = 0;                  // "x": the byte is the plain value times x
    std::vector<int> raw;          // "raw": the byte of each label
    int area = -1, part = 0, off = 0;
    bool morph = false;            // a voice field that blends between the morph corners
    double min = 0;                // the param's minimum (labels index from it)

    int word(const uint8_t* b) const {
        int v = wide ? b[off] << 7 | b[off + 1] : b[off];
        return width ? v >> shift & ((1 << width) - 1) : v;
    }
    void set(uint8_t* b, int raw7) const {
        int v = wide ? b[off] << 7 | b[off + 1] : b[off];
        if (width) {
            const int mask = ((1 << width) - 1) << shift;
            v = (v & ~mask) | (raw7 << shift & mask);
        } else {
            v = raw7;
        }
        if (wide) { b[off] = (uint8_t)(v >> 7 & 0x7F); b[off + 1] = (uint8_t)(v & 0x7F); }
        else b[off] = (uint8_t)(v & 0x7F);
    }
    int toRaw(double plain) const {
        if (!raw.empty()) return raw[(size_t)std::clamp((int)std::lround(plain - min), 0, (int)raw.size() - 1)];
        if (x) return (int)std::lround(plain * x);
        return (int)std::lround(plain) + k;
    }
    double toPlain(int r) const {
        if (!raw.empty()) {
            for (size_t i = 0; i < raw.size(); ++i)
                if (raw[i] == r) return min + (double)i;
            return min;
        }
        if (x) return r / x;
        return r - k;
    }
    bool locate() {   // area and offset from the address
        if (h == 0 && m == 0 && l < 76) { area = System; off = l; }
        else if (h == 0x10 && m == 0) { area = Performance; off = l < 0x50 ? l : 80 + l - 0x50; }
        else if (h == 0x10 && m == 1) { area = Performance; off = 128 + l; }
        else if (h >= 0x30 && h <= 0x33 && m == 0) { area = Performance; off = 192 + 52 * (h - 0x30) + l; }
        else if (h >= 0x40 && h <= 0x43 && m == 0) { area = Voice; part = h - 0x40; off = l; }
        else if (h >= 0x60 && h <= 0x63 && m < 8) { area = Voice; part = h - 0x60; off = 112 + 62 * m + l; }
        else return false;
        return off + (wide ? 1 : 0) < (area == System ? 76 : area == Performance ? 400 : 608);
    }
};

static const char* const kCorner[4] = {"tl", "tr", "bl", "br"};   // A to D, the morph_edit values 0..3

// ---- the processor ---------------------------------------------------------------------------------

class Fsvr final : public hollow::Processor {
public:
    explicit Fsvr(State& s) : st(s), base(s.size(), 0), sel(s.size(), 0), fields(s.size()) {
        buildFields();
        factory = parseSyx(factoryBytes("fs1r_performances.syx"), "Yamaha FS1R");
        factory.voices = parseSyx(factoryBytes("fs1r_presets.syx"), "").voices;
        factory.fseqs = parseSyx(factoryBytes("fs1r_fseqs.syx"), "").fseqs;
        dev.setSampleRate(fs1r::ENGINE_RATE);
        // A fresh instance is what its params say (FSVR's init performance and voice): the engine takes
        // them as whole bulks, over its own init images for the bytes no param names.
        std::vector<uint8_t> dump;
        dev.getState(dump);
        model.parse(dump);
        for (size_t i = 0; i < st.size(); ++i) {
            base[i] = sel[i] = st.get(i);
            if (fields[i].area >= 0) fields[i].set(model.area(fields[i].area, fields[i].part), fields[i].toRaw(base[i]));
        }
        for (int p = 0; p < 4; ++p)
            for (auto& c : corners[p]) std::memcpy(c.data(), model.voice[p], 608);
        pushModel();
        loadsSeen = st.loads();
        // What the LCD and the browser name is what plays: the worker's first look loads that factory
        // performance (A001 on a fresh instance), unless a session arrives first.
        sel[(size_t)perfProgram] = std::nan("");
        lib.rescan(true);
        st.setData("library.dir", (std::filesystem::u8path(lib.dir) / "").u8string());
        writeLists();
        names();
        worker = std::thread([this] { run(); });
    }

    ~Fsvr() override {
        {
            std::lock_guard<std::mutex> g(wakeLock);
            quit = true;
        }
        wake.notify_all();
        worker.join();
    }

    void prepare(double rate, int) override {
        dev.setSampleRate(rate);
        hostRate = rate;
    }

    void midi(int frame, const uint8_t* b, int n) override {
        if (n <= 0 || events >= kEvents || used + (size_t)n > arena.size()) return;
        std::memcpy(arena.data() + used, b, (size_t)n);
        ev[events++] = {frame, used, n};
        used += (size_t)n;
    }

    void process(const float* const*, float* const* out, int frames) override {
        const NoDenormals flush;
        std::unique_lock<std::mutex> lk(ctl, std::try_to_lock);
        const unsigned loads = st.loads();
        const bool live = lk.owns_lock() && !(loads & 1) && loads == loadsSeen;
        if (live) syncParams();
        int at = 0;
        for (int e = 0; e < events; ++e) {
            const int t = std::clamp(ev[e].frame, at, frames);
            if (t > at) dev.process(out[0] + at, out[1] + at, t - at);
            at = t;
            handle(arena.data() + ev[e].at, ev[e].n, live);
        }
        events = 0;
        used = 0;
        if (at < frames) dev.process(out[0] + at, out[1] + at, frames - at);
        lk = {};
        const size_t r = ringAt;
        for (int i = 0; i < frames; ++i) ring[(r + (size_t)i) % ring.size()] = 0.5f * (out[0][i] + out[1][i]);
        ringAt = (r + (size_t)frames) % ring.size();
        st.setVoices(dev.activeNotes());
        while (dev.nextMidiOut(outMsg))
            if (sendMidi) sendMidi(std::max(frames - 1, 0), outMsg.data(), (int)outMsg.size());
    }

    // The engine and the morph corners go into the saved instance as text data.
    void saving() override {
        std::lock_guard<std::mutex> g(ctl);
        step();   // a session or a program change the worker has not reached yet is part of what is saved
        std::vector<uint8_t> dump;
        dev.getState(dump);
        st.setData("fsvr.engine", b64(dump.data(), dump.size()));
        std::vector<uint8_t> c;
        for (auto& part : corners)
            for (auto& v : part) c.insert(c.end(), v.begin(), v.end());
        st.setData("fsvr.morph", b64(c.data(), c.size()));
    }

private:
    State& st;
    fs1r::Device dev;
    std::vector<double> base;          // each param's value as the engine last had it (the audio thread's)
    std::vector<double> sel;           // and as the worker last acted on it (loads, pages, requests)
    std::vector<Field> fields;
    std::vector<int> fieldParams;      // params with a field, in skin order
    std::vector<int> voiceParams[4];   // a part's voice params
    Model model;
    std::array<std::array<uint8_t, 608>, 4> corners[4];
    int lastCorner[4] = {0, 0, 0, 0};  // the corner the pages show with Edit All
    Bank factory;
    Library lib;
    std::mutex ctl;
    unsigned loadsSeen = 0;
    double hostRate = fs1r::ENGINE_RATE;

    // params by id
    int P(const std::string& id) const { return st.indexOf(id); }
    int perfProgram = -1, perfBank = -1, perfUser = -1, fseqBank = -1, fseqNumber = -1, fseqUser = -1, fseqPart = -1, panic = -1;
    int partBank[4], partProgram[4], partUser[4], morphX[4], morphY[4], jitterX[4], jitterY[4], morphSeed[4], morphEdit[4];
    int browseBank = -1, browseCategory = -1, browsePerf = -1, browseFseq = -1, browseVoice[4], knob[4];

    // MIDI waiting for its place in the block
    static constexpr int kEvents = 1024;
    struct Event { int frame; size_t at; int n; };
    Event ev[kEvents];
    int events = 0;
    std::vector<uint8_t> arena = std::vector<uint8_t>(1 << 17);
    size_t used = 0;
    std::vector<uint8_t> outMsg = std::vector<uint8_t>(64);
    std::array<uint8_t, 619> voiceBulk;
    int bankMsb[16], bankLsb[16];
    int perfBankSel = -1;              // the performance bank a bank select chose (0x40 Int .. 0x43 PrC)
    uint32_t noise[4] = {1, 1, 1, 1};  // per part, the morph jitter's generator
    int seedSeen[4] = {-1, -1, -1, -1};
    std::atomic<int> lastNote{-1};
    std::atomic<bool> adoptWanted{false};

    // the output, for the harmonic display
    std::vector<float> ring = std::vector<float>(8192);
    std::atomic<size_t> ringAt{0};

    std::thread worker;
    std::mutex wakeLock;
    std::condition_variable wake;
    bool quit = false;
    bool notify = false;               // the worker changed params; the host re-reads them
    std::vector<int> browsed[3];       // the browsed bank's rows as U numbers: performances, voices, Fseqs
    std::vector<uint8_t> fseqShown;    // the Fseq the page's display last had

    // ---- setup -----------------------------------------------------------------------------------

    void buildFields() {
        Json table;
        std::string err;
        if (!hollow::parseJson(skinFile("data/fs1r_sysex.json"), table, &err)) std::fprintf(stderr, "FSVR: fs1r_sysex.json: %s\n", err.c_str());
        for (auto& m : table.members) {
            const int i = st.indexOf(m.first);
            if (i < 0) continue;
            const Json& j = m.second;
            Field f;
            f.h = j["a"][0].integer();
            f.m = j["a"][1].integer();
            f.l = j["a"][2].integer();
            f.shift = j["s"].integer();
            f.width = j["w"].integer();
            f.wide = j["wide"].flag();
            f.k = j["k"].integer();
            f.x = j["x"].num();
            for (auto& r : j["raw"].items) f.raw.push_back(r.integer());
            f.min = st.def((size_t)i).min;
            if (!f.locate()) continue;
            if (f.area == Voice) {
                // Switches and choices come from the nearest corner, amounts blend (docs/fsvr-skin.md, Morph).
                const std::string& id = m.first;
                const auto& labels = st.def((size_t)i).labels;
                const bool choice = (!labels.empty() && labels.size() <= 24) || id.find(".alg.") != std::string::npos ||
                                    id.find(".fseq_track.") != std::string::npos || id.find("_ctrl.") != std::string::npos && id.find(".op.") != std::string::npos;
                f.morph = !choice;
                voiceParams[f.part].push_back(i);
            }
            fields[(size_t)i] = f;
            fieldParams.push_back(i);
        }
        perfProgram = P("perf.program"); perfBank = P("perf.bank"); perfUser = P("perf.user");
        fseqBank = P("fseq.bank"); fseqNumber = P("fseq.number"); fseqUser = P("fseq.user"); fseqPart = P("fseq.part");
        panic = P("gui.panic");
        browseBank = P("browse.bank"); browseCategory = P("browse.category"); browsePerf = P("browse.perf"); browseFseq = P("browse.fseq");
        for (int p = 0; p < 4; ++p) {
            const std::string s = "p" + std::to_string(p + 1);
            partBank[p] = P("part.bank." + s); partProgram[p] = P("part.program." + s); partUser[p] = P("part.user." + s);
            morphX[p] = P("part.morph_x." + s); morphY[p] = P("part.morph_y." + s);
            jitterX[p] = P("part.morph_jitter_x." + s); jitterY[p] = P("part.morph_jitter_y." + s);
            morphSeed[p] = P("part.morph_seed." + s); morphEdit[p] = P("part.morph_edit." + s);
            browseVoice[p] = P("browse.voice." + s);
            knob[p] = P("knob." + std::to_string(p + 1));
        }
        for (int c = 0; c < 16; ++c) bankMsb[c] = bankLsb[c] = -1;
    }

    double get(int i) const { return i >= 0 ? st.get((size_t)i) : 0; }
    void put(int i, double v) {   // a param the worker sets itself: no action follows from it
        if (i < 0) return;
        st.set((size_t)i, v);
        base[(size_t)i] = sel[(size_t)i] = st.get((size_t)i);
        notify = true;
    }
    int edit(int p) const { return std::clamp((int)std::lround(get(morphEdit[p])), 0, 4); }

    // ---- to the engine ------------------------------------------------------------------------------

    void send(const Field& f, const uint8_t* bytes) {
        const int dev_ = model.sys[0x49];   // the unit's device number: its own, or 16 for all
        const int v = f.wide ? bytes[f.off] << 7 | bytes[f.off + 1] : bytes[f.off];
        const uint8_t m[10] = {0xF0, 0x43, (uint8_t)(0x10 | (dev_ < 16 ? dev_ : 0)), 0x5E, (uint8_t)f.h, (uint8_t)f.m, (uint8_t)f.l,
                               (uint8_t)(v >> 7 & 0x7F), (uint8_t)(v & 0x7F), 0xF7};
        dev.sendMidi(m, 10);
    }

    void sendVoice(int p, const uint8_t* v) {   // one bulk to the part's voice: the engine decodes it once
        voiceBulk = {0xF0, 0x43, 0x00, 0x5E, 0x04, 0x60, (uint8_t)(0x40 + p), 0x00, 0x00};
        std::memcpy(voiceBulk.data() + 9, v, 608);
        int sum = 0;
        for (size_t i = 4; i < 617; ++i) sum += voiceBulk[i];
        voiceBulk[617] = (uint8_t)(-sum & 0x7F);
        voiceBulk[618] = 0xF7;
        dev.sendMidi(voiceBulk.data(), voiceBulk.size());
    }

    void pushModel() {   // the whole model as bulks
        for (auto& m : {bulk(0x00, 0, 0, model.sys, 76), bulk(0x10, 0, 0, model.perf, 400)}) dev.sendMidi(m.data(), m.size());
        for (int p = 0; p < 4; ++p) sendVoice(p, model.voice[p]);
    }

    // The morph corners of part p blended at (x, y) in 0..1, y up: amounts weighted by nearness to each
    // corner, everything else from the nearest.
    void blend(int p, double x, double y, uint8_t* out) const {
        x = std::clamp(x, 0.0, 1.0);
        y = std::clamp(y, 0.0, 1.0);
        const double w[4] = {(1 - x) * y, x * y, (1 - x) * (1 - y), x * (1 - y)};
        const int near = (int)(std::max_element(w, w + 4) - w);
        std::memcpy(out, corners[p][near].data(), 608);
        for (int i : voiceParams[p]) {
            const Field& f = fields[(size_t)i];
            if (!f.morph) continue;
            double v = 0;
            for (int c = 0; c < 4; ++c) v += w[c] * f.word(corners[p][c].data());
            f.set(out, (int)std::lround(v));
        }
    }
    void position(int p, double& x, double& y) const { x = get(morphX[p]) / 100; y = get(morphY[p]) / 100; }
    bool cornersDiffer(int p) const {
        for (int c = 1; c < 4; ++c)
            if (corners[p][c] != corners[p][0]) return true;
        return false;
    }

    // The part's blended voice to the engine, when it differs from what the engine has.
    void remorph(int p) {
        uint8_t v[608];
        double x, y;
        position(p, x, y);
        blend(p, x, y, v);
        if (std::memcmp(v, model.voice[p], 608) == 0) return;
        std::memcpy(model.voice[p], v, 608);
        sendVoice(p, v);
    }

    // Audio thread: every param that moved since the engine last heard of it.
    void syncParams() {
        bool morphed[4] = {false, false, false, false};
        for (int i : fieldParams) {
            const double v = st.get((size_t)i);
            if (v == base[(size_t)i]) continue;
            base[(size_t)i] = v;
            const Field& f = fields[(size_t)i];
            const int raw = f.toRaw(v);
            if (f.area != Voice) {
                uint8_t* b = model.area(f.area, 0);
                f.set(b, raw);
                send(f, b);
                continue;
            }
            const int p = f.part, e = edit(p);
            for (int c = 0; c < 4; ++c)
                if (e == 4 || e == c) f.set(corners[p][c].data(), raw);
            if (!f.morph) {   // a choice: the nearest corner's, the rest by the blend
                morphed[p] = true;
                continue;
            }
            double x, y;
            position(p, x, y);
            const double w[4] = {(1 - x) * y, x * y, (1 - x) * (1 - y), x * (1 - y)};
            double b = 0;
            for (int c = 0; c < 4; ++c) b += w[c] * f.word(corners[p][c].data());
            if ((int)std::lround(b) == f.word(model.voice[p])) continue;
            f.set(model.voice[p], (int)std::lround(b));
            send(f, model.voice[p]);
        }
        for (int p = 0; p < 4; ++p) {
            for (int i : {morphX[p], morphY[p]})
                if (i >= 0 && st.get((size_t)i) != base[(size_t)i]) {
                    base[(size_t)i] = st.get((size_t)i);
                    morphed[p] = true;
                }
            if (morphed[p]) remorph(p);
            if (knob[p] >= 0 && st.get((size_t)knob[p]) != base[(size_t)knob[p]]) {   // KN1..KN4 on their control numbers
                base[(size_t)knob[p]] = st.get((size_t)knob[p]);
                const int cc = model.sys[0x16 + p], v = std::clamp((int)std::lround(base[(size_t)knob[p]]), 0, 127);
                for (int ch : listeningChannels()) {
                    const uint8_t m[3] = {(uint8_t)(0xB0 | ch), (uint8_t)cc, (uint8_t)v};
                    dev.sendMidi(m, 3);
                }
            }
        }
    }

    // ---- MIDI --------------------------------------------------------------------------------------

    int perfChannel() const { return model.sys[0x09]; }   // 0..15, 16 all, 127 off
    bool listens(int p, int ch) const {                    // Synth::part_listens
        const int rc = model.perf[192 + 52 * p + 4], pc = perfChannel();
        if (rc == 0x7F) return false;
        if (rc == 0x10) return pc == 0x10 || (pc != 0x7F && ch == pc);
        return rc == ch;
    }
    std::vector<int> listeningChannels() const {
        std::vector<int> out;
        for (int ch = 0; ch < 16; ++ch)
            for (int p = 0; p < 4; ++p)
                if (listens(p, ch)) { out.push_back(ch); break; }
        return out;
    }

    void handle(const uint8_t* b, int n, bool live) {
        const int st8 = b[0] & 0xF0, ch = b[0] & 0x0F;
        if (b[0] == 0xF0) {                       // a patch or parameter change from an editor
            dev.sendMidi(b, (size_t)n);
            adoptWanted = true;
            return;
        }
        if (st8 == 0x90 && n >= 3 && b[2]) {
            lastNote = b[1];
            for (int p = 0; p < 4 && live; ++p) jitter(p, ch);
        }
        if (st8 == 0xB0 && n >= 3) {
            const int cc = b[1], v = b[2];
            if (cc == 0) bankMsb[ch] = v;
            if (cc == 32) {                        // the engine's own rule (midi.cpp control_change)
                bankLsb[ch] = v;
                if (model.sys[0x12] && bankMsb[ch] == 0x3F) {
                    if (v >= 0x40 && v <= 0x43) perfBankSel = v;
                    for (int p = 0; p < 4; ++p)
                        if (v <= 0x0B && listens(p, ch)) st.set((size_t)partBank[p], v + 1);
                }
            }
            if (cc != 1 && cc != 64 && cc != 11) adoptWanted = true;   // most write a part byte
        }
        if (st8 == 0xC0 && n >= 2) {
            programChange(ch, b[1]);
            return;                               // the engine loads programs only out of an EPROM
        }
        dev.sendMidi(b, (size_t)n);
    }

    // Program change as the unit takes it, out of the factory banks and the user library: on the
    // performance channel in Performance mode a performance of the bank selected (Int = the user's), in
    // Multi mode a voice for each part on the channel, from its bank.
    void programChange(int ch, int prog) {
        if (!model.sys[0x13]) return;              // program change receive off
        const int pc = perfChannel();
        if (model.sys[0x08] == 0) {
            if (!(pc == 0x10 || (pc != 0x7F && ch == pc))) return;
            int bank = perfBankSel;
            if (bank < 0x40) bank = get(perfBank) == 1 ? 0x40 : 0x41 + (int)get(perfProgram) / 128;
            if (bank == 0x40) {
                if (lib.perf(prog + 1)) { st.set((size_t)perfUser, prog + 1); st.set((size_t)perfBank, 1); }
            } else {
                st.set((size_t)perfProgram, (bank - 0x41) * 128 + prog);
                st.set((size_t)perfBank, 0);
            }
            return;
        }
        for (int p = 0; p < 4; ++p) {
            if (!listens(p, ch) || get(partBank[p]) == 0) continue;
            if (get(partBank[p]) == 1) st.set((size_t)partUser[p], prog + 1);
            else st.set((size_t)partProgram[p], prog + 1);
        }
    }

    // Morph random: each note nudges the part's position by up to the random amounts, from its seed.
    void jitter(int p, int ch) {
        const double jx = get(jitterX[p]) / 100, jy = get(jitterY[p]) / 100;
        if ((jx <= 0 && jy <= 0) || !listens(p, ch) || !cornersDiffer(p)) return;
        const int seed = (int)get(morphSeed[p]);
        if (seed != seedSeen[p]) { seedSeen[p] = seed; noise[p] = 2654435761u * (uint32_t)(seed + 1); }
        auto rnd = [&] { noise[p] = noise[p] * 1664525u + 1013904223u; return (noise[p] >> 8) / double(1 << 24) * 2 - 1; };
        double x, y;
        position(p, x, y);
        const double dx = rnd() * jx, dy = rnd() * jy;
        uint8_t v[608];
        blend(p, x + dx, y + dy, v);
        if (std::memcmp(v, model.voice[p], 608) == 0) return;
        std::memcpy(model.voice[p], v, 608);
        sendVoice(p, v);
    }

    // ---- the worker --------------------------------------------------------------------------------

    void run() {
        auto tick = std::chrono::steady_clock::now();
        int n = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> w(wakeLock);
                if (wake.wait_until(w, tick += std::chrono::milliseconds(33), [this] { return quit; })) return;
            }
            {
                std::lock_guard<std::mutex> g(ctl);
                step();
            }
            requests();
            if (++n % 30 == 0 && lib.rescan()) {
                std::lock_guard<std::mutex> g(ctl);
                writeLists();
            }
            if (n % 2 == 0) harmonics();
            if (notify && n % 8 == 0) {   // at most four times a second: a rescan of 3,000 params is not free for a host
                notify = false;
                if (paramsChanged) paramsChanged();
            }
        }
    }

    void step() {   // under ctl: a session that arrived, what the engine did, what the params ask for
        const unsigned loads = st.loads();
        if (!(loads & 1) && loads != loadsSeen) restore(loads);
        if (adoptWanted.exchange(false)) adopt();
        selectors();
    }

    // A session came back: the engine as it was saved, the corners too, and every param as the host holds it.
    void restore(unsigned loads) {
        loadsSeen = loads;
        const std::vector<uint8_t> eng = unb64(st.data("fsvr.engine"));
        const std::vector<uint8_t> mor = unb64(st.data("fsvr.morph"));
        for (size_t i = 0; i < st.size(); ++i) sel[i] = st.get(i);
        if (!eng.empty()) {
            dev.allNotesOff();
            dev.setState(eng.data(), eng.size());
            std::vector<uint8_t> fseq;
            model.parse(eng, &fseq);
            showFseq(fseq);
            for (size_t i = 0; i < st.size(); ++i) base[i] = st.get(i);
        } else {
            for (double& b : base) b = std::nan("");   // an older session: send every param again
        }
        for (int p = 0; p < 4; ++p)
            for (int c = 0; c < 4; ++c)
                if (mor.size() == 4 * 4 * 608) std::memcpy(corners[p][c].data(), mor.data() + (size_t)(p * 4 + c) * 608, 608);
                else std::memcpy(corners[p][c].data(), model.voice[p], 608);
        writeLists();
        names();
    }

    // What the engine holds, back into the params: every field whose bytes changed under us (a load, a
    // bulk from the host, a controller that writes a part byte). A changed voice goes into the morph
    // corners the part edits (all four with Edit All) and the part plays the blend again.
    void adopt() {
        std::vector<uint8_t> dump, fseq;
        dev.getState(dump);
        Model now;
        if (!now.parse(dump, &fseq)) return;
        showFseq(fseq);
        for (int i : fieldParams) {
            const Field& f = fields[(size_t)i];
            if (f.area == Voice) continue;
            const int was = f.word(model.area(f.area, 0)), is = f.word(now.area(f.area, 0));
            if (was != is) put(i, f.toPlain(is));
        }
        std::memcpy(model.sys, now.sys, 76);
        std::memcpy(model.perf, now.perf, 400);
        for (int p = 0; p < 4; ++p) {
            if (std::memcmp(now.voice[p], model.voice[p], 608) == 0) continue;
            const int e = edit(p);
            for (int c = 0; c < 4; ++c)
                if (e == 4 || e == c) std::memcpy(corners[p][c].data(), now.voice[p], 608);
            std::memcpy(model.voice[p], now.voice[p], 608);
            remorph(p);
            show(p);
        }
    }

    // The Fseq page's display draws the loaded Fseq from text data fseq.display, an entry in the form of
    // the skin's data/fseqs.json: per frame the pitch and each track's frequency (the word's high byte) and
    // level, as hex strings of one byte a frame.
    void showFseq(const std::vector<uint8_t>& d) {
        if (d == fseqShown) return;
        fseqShown = d;
        if (d.size() < 32 + 50) { st.setData("fseq.display", ""); return; }
        const int total = (int)(d.size() - 32) / 50, end = d[0x1E] << 7 | d[0x1F];
        const int n = end ? std::min(total, end + 1) : total;
        auto hex = [&](int at) {
            std::string h;
            char b[3];
            for (int k = 0; k < n; ++k) { std::snprintf(b, sizeof b, "%02x", d[32 + (size_t)k * 50 + (size_t)at]); h += b; }
            return "\"" + h + "\"";
        };
        auto tracks = [&](int at) {
            std::string t = "[";
            for (int i = 0; i < 8; ++i) t += (i ? "," : "") + hex(at + i);
            return t + "]";
        };
        st.setData("fseq.display", "{\"frames\":" + std::to_string(n) + ",\"pitch\":" + hex(0) + ",\"vfreq\":" + tracks(2) + ",\"vlevel\":" + tracks(0x12) +
                                        ",\"ufreq\":" + tracks(0x1A) + ",\"ulevel\":" + tracks(0x2A) + "}");
    }

    // The pages show the corner a part edits: the picked one, or with Edit All the last one picked.
    void show(int p) {
        const int e = edit(p), c = e < 4 ? e : lastCorner[p];
        for (int i : voiceParams[p]) put(i, fields[(size_t)i].toPlain(fields[(size_t)i].word(corners[p][c].data())));
        names();
    }

    void names() {
        for (int p = 0; p < 4; ++p)
            for (int c = 0; c < 4; ++c) {
                std::string n;
                for (int k = 0; k < 10; ++k) n += corners[p][c][(size_t)k] >= 32 && corners[p][c][(size_t)k] < 127 ? (char)corners[p][c][(size_t)k] : ' ';
                while (!n.empty() && n.back() == ' ') n.pop_back();
                st.setData("morph.p" + std::to_string(p + 1) + "." + kCorner[c], n);
            }
    }

    bool moved(int i) {   // a param the worker acts on changed since it last looked
        if (i < 0 || st.get((size_t)i) == sel[(size_t)i]) return false;
        sel[(size_t)i] = st.get((size_t)i);
        return true;
    }

    void selectors() {
        bool perf = false, fseq = false;
        for (int i : {perfProgram, perfBank, perfUser}) perf |= moved(i);
        for (int i : {fseqBank, fseqNumber, fseqUser}) fseq |= moved(i);
        if (perf) {
            if (get(perfBank) == 1) loadUserPerf((int)get(perfUser));
            else loadFactoryPerf((int)get(perfProgram));
        }
        for (int p = 0; p < 4; ++p) {
            bool voice = false;
            for (int i : {partBank[p], partProgram[p], partUser[p]}) voice |= moved(i);
            if (voice && !perf) loadPartVoice(p);
            if (moved(morphEdit[p])) {
                if (edit(p) < 4) lastCorner[p] = edit(p);
                show(p);
            }
        }
        if (fseq && !perf) {
            if (get(fseqBank) == 1) loadFactoryFseq((int)get(fseqNumber));
            else loadUserFseq((int)get(fseqUser));
        }
        if (moved(panic) && get(panic) > 0) dev.allNotesOff();
        bool lists = moved(browseBank) | moved(browseCategory);
        if (lists) writeLists();
        if (moved(browsePerf) && get(browsePerf) >= 1) pick(0, (int)get(browsePerf), 0);
        if (moved(browseFseq) && get(browseFseq) >= 1) pick(2, (int)get(browseFseq), 0);
        for (int p = 0; p < 4; ++p)
            if (moved(browseVoice[p]) && get(browseVoice[p]) >= 1) pick(1, (int)get(browseVoice[p]), p);
    }

    // ---- loading -----------------------------------------------------------------------------------

    void loadVoiceItem(const Item& it, int p) {
        const std::vector<uint8_t> m = it.address ? readdress(it.syx, 0x40 + p, 0, 0) : it.syx;   // a DX voice takes the part it is given
        dev.loadSyx(m.data(), m.size(), 0, p);
    }

    void loadFactoryPerf(int index) {
        if (index < 0 || index >= (int)factory.perfs.size()) return;
        const Item& it = factory.perfs[(size_t)index];
        dev.allNotesOff();
        dev.loadSyx(it.syx.data(), it.syx.size(), 0, 0);
        followPerf(it, nullptr);
        put(perfBank, 0);
        st.setData("perf.user.name", "");
        adopt();
    }

    void loadUserPerf(int n) {
        const Item* it = lib.perf(n);
        if (!it) return;
        const Bank& b = lib.banks[(size_t)lib.perfs[(size_t)n - 1].bank];
        dev.allNotesOff();
        dev.loadSyx(it->syx.data(), it->syx.size(), 0, 0);
        followPerf(*it, &b);
        put(perfBank, 1);
        put(perfUser, n);
        st.setData("perf.user.name", it->name);
        adopt();
    }

    // A performance names its voices and its Fseq by bank and number: the factory's, or Int for the
    // user's own, which in a bank that holds its own internal voices means those.
    void followPerf(const Item& perf, const Bank* bank) {
        const uint8_t* d = perf.data();
        for (int p = 0; p < 4; ++p) {
            const int vb = d[192 + 52 * p + 1], prog = d[192 + 52 * p + 2];
            if (bank && perf.partVoice[p] >= 0) loadVoiceItem(bank->voices[(size_t)perf.partVoice[p]], p);
            else if (vb >= 2 && factoryVoice(vb, prog) < (int)factory.voices.size()) loadVoiceItem(factory.voices[(size_t)factoryVoice(vb, prog)], p);
            else if (vb == 1) {
                if (const Item* v = internal(bank, &Bank::voices, prog)) loadVoiceItem(*v, p);
                else if (const Item* u = lib.voice(prog + 1)) loadVoiceItem(*u, p);
            }
        }
        if (bank && perf.fseq >= 0) {
            const Item& f = bank->fseqs[(size_t)perf.fseq];
            dev.loadSyx(f.syx.data(), f.syx.size(), 0, 0);
        } else if ((d[0x15] & 7) != 0) {
            const int num = d[0x17];
            if (d[0x16] & 1) {
                if (num < (int)factory.fseqs.size()) dev.loadSyx(factory.fseqs[(size_t)num].syx.data(), factory.fseqs[(size_t)num].syx.size(), 0, 0);
            } else if (const Item* f = internal(bank, &Bank::fseqs, num)) {
                dev.loadSyx(f->syx.data(), f->syx.size(), 0, 0);
            } else if (const Item* u = lib.fseq(num + 1)) {
                dev.loadSyx(u->syx.data(), u->syx.size(), 0, 0);
                put(fseqUser, num + 1);
            }
        }
    }

    // A bank's internal memory item by its number: the bulk dumped from that number, else the n-th.
    static const Item* internal(const Bank* b, std::vector<Item> Bank::*list, int n) {
        if (!b) return nullptr;
        const std::vector<Item>& items = b->*list;
        for (auto& it : items)
            if (it.number == n) return &it;
        return n >= 0 && n < (int)items.size() ? &items[(size_t)n] : nullptr;
    }

    void loadPartVoice(int p) {
        const int vb = (int)get(partBank[p]);
        const Item* it = vb == 1 ? lib.voice((int)get(partUser[p]))
                       : factoryVoice(vb, (int)get(partProgram[p]) - 1) >= 0 ? &factory.voices[(size_t)factoryVoice(vb, (int)get(partProgram[p]) - 1)] : nullptr;
        if (!it) return;
        dev.allNotesOff();
        loadVoiceItem(*it, p);
        adopt();
    }

    void loadFactoryFseq(int n) {
        if (n < 0 || n >= (int)factory.fseqs.size()) return;
        dev.loadSyx(factory.fseqs[(size_t)n].syx.data(), factory.fseqs[(size_t)n].syx.size(), 0, 0);
        adopt();
    }

    void loadUserFseq(int n) {
        const Item* it = lib.fseq(n);
        if (!it) return;
        dev.loadSyx(it->syx.data(), it->syx.size(), 0, 0);
        adopt();
    }

    // A row of the browsed user bank: list 0 performances, 1 voices (into part p), 2 Fseqs.
    void pick(int list, int row, int p) {
        if (row < 1 || row > (int)browsed[list].size()) return;
        const int n = browsed[list][(size_t)row - 1];
        if (list == 0) {
            put(perfUser, n);
            put(perfBank, 1);
            loadUserPerf(n);
        } else if (list == 1) {
            put(partUser[p], n);
            put(partBank[p], 1);
            loadPartVoice(p);
        } else {
            put(fseqUser, n);
            put(fseqBank, 0);
            loadUserFseq(n);
        }
    }

    // ---- lists -------------------------------------------------------------------------------------

    std::string perfRow(const Item& it) const {
        const uint8_t* d = it.data();
        std::string voices, chans;
        std::vector<std::string> seen;
        for (int p = 0; p < 4; ++p) {
            const uint8_t* q = d + 192 + 52 * p;
            const int bank = q[1], ch = q[4], mx = q[3];
            if (!bank) continue;
            char code[16];
            std::snprintf(code, sizeof code, "%c%03d", bank >= 2 ? "  ABCDEFGHIJK"[bank] : 'U', q[2] + 1);
            voices += (voices.empty() ? "" : " ") + std::string(code);
            std::string c = ch == 16 ? "Perf" : ch == 127 ? "off" : mx < 16 && mx > ch ? std::to_string(ch + 1) + "-" + std::to_string(mx + 1) : std::to_string(ch + 1);
            if (std::find(seen.begin(), seen.end(), c) == seen.end()) seen.push_back(c);
        }
        std::sort(seen.begin(), seen.end());
        for (auto& c : seen) chans += (chans.empty() ? "" : ", ") + c;
        return it.name + "\t" + kCategories[std::clamp(it.category, 0, 22)] + "\t" + voices + "\t" + chans;
    }

    void writeLists() {
        std::string perfs, voices, fseqs, banks;
        for (auto& r : lib.perfs) perfs += perfRow(lib.banks[(size_t)r.bank].perfs[(size_t)r.index]) + "\n";
        for (auto& r : lib.voices) {
            const Item& v = lib.banks[(size_t)r.bank].voices[(size_t)r.index];
            voices += v.name + "\t" + kCategories[v.address ? std::clamp(v.category, 0, 22) : 0] + "\n";
        }
        for (auto& r : lib.fseqs) {
            const Item& f = lib.banks[(size_t)r.bank].fseqs[(size_t)r.index];
            fseqs += f.name + "\t" + std::to_string(f.frames) + " frames\n";
        }
        for (auto& b : lib.banks) banks += b.name + "\n";
        st.setData("perf.user.list", perfs);
        st.setData("voice.user.list", voices);
        st.setData("fseq.user.list", fseqs);
        st.setData("bank.list", banks);
        // The browsed bank's rows, filtered by the category column (0 All, 1 User, then the categories).
        const int b = (int)get(browseBank) - 1, cat = (int)get(browseCategory) - 2;
        std::string rows[3];
        for (auto& v : browsed) v.clear();
        if (b >= 0 && b < (int)lib.banks.size()) {
            const Bank& bank = lib.banks[(size_t)b];
            for (int i = 0; i < (int)bank.perfs.size(); ++i)
                if (cat < 0 || bank.perfs[(size_t)i].category == cat) {
                    browsed[0].push_back(lib.number(lib.perfs, b, i));
                    rows[0] += "U" + std::to_string(browsed[0].back()) + "\t" + perfRow(bank.perfs[(size_t)i]) + "\n";
                }
            for (int i = 0; i < (int)bank.voices.size(); ++i) {
                const Item& v = bank.voices[(size_t)i];
                if (cat >= 0 && (v.address ? v.category : 0) != cat) continue;
                browsed[1].push_back(lib.number(lib.voices, b, i));
                rows[1] += "U" + std::to_string(browsed[1].back()) + "\t" + v.name + "\t" + kCategories[v.address ? std::clamp(v.category, 0, 22) : 0] + "\n";
            }
            for (int i = 0; i < (int)bank.fseqs.size(); ++i) {
                browsed[2].push_back(lib.number(lib.fseqs, b, i));
                rows[2] += "U" + std::to_string(browsed[2].back()) + "\t" + bank.fseqs[(size_t)i].name + "\t" + std::to_string(bank.fseqs[(size_t)i].frames) + " frames\n";
            }
        }
        st.setData("browse.perf.list", rows[0]);
        st.setData("browse.voice.list", rows[1]);
        st.setData("browse.fseq.list", rows[2]);
    }

    // ---- requests from the GUI ---------------------------------------------------------------------

    std::string take(const std::string& key) {
        std::string v = st.data(key);
        if (!v.empty()) st.setData(key, "");
        return v;
    }

    void message(const std::string& s) { st.setData("fsvr.message", s); }

    int selectedPart() const {   // the GUI's {part} var, out of its saved state
        Json ui;
        if (!hollow::parseJson(st.ui(), ui)) return 0;
        const std::string p = ui["vars"]["part"].str("p1");
        return p.size() == 2 && p[1] >= '1' && p[1] <= '4' ? p[1] - '1' : 0;
    }

    void requests() {
        if (std::string path = take("sysex.import"); !path.empty()) importSyx(path, false);
        if (std::string path = take("fseq.import"); !path.empty()) importSyx(path, true);
        if (std::string path = take("sysex.export"); !path.empty()) exportSyx(path, true);
        if (std::string path = take("perf.store"); !path.empty()) exportSyx(path, false);
        if (std::string path = take("fseq.export"); !path.empty()) exportFseq(path);
        if (std::string path = take("fseq.import_audio"); !path.empty()) importAudio(path);
        for (int p = 0; p < 4; ++p)
            if (take("morph.request.p" + std::to_string(p + 1)) == "normalize") {
                std::lock_guard<std::mutex> g(ctl);
                const int e = edit(p), c = e < 4 ? e : lastCorner[p];
                for (auto& corner : corners[p]) corner = corners[p][c];
                remorph(p);
                show(p);
            }
    }

    // Import SysEx: the file becomes a bank of the library, named after it, and what it holds first loads
    // (its first performance, else its first voice into the selected part, else its first Fseq).
    void importSyx(const std::string& path, bool fseqFirst) {
        std::string err;
        std::lock_guard<std::mutex> g(ctl);
        const int k = lib.import(path, err);
        if (k < 0) { message(err); return; }
        const Bank& b = lib.banks[(size_t)k];
        put(browseBank, k + 1);
        writeLists();
        message("Imported \"" + b.name + "\": " + std::to_string(b.perfs.size()) + " performances, " + std::to_string(b.voices.size()) +
                " voices, " + std::to_string(b.fseqs.size()) + " Fseqs");
        if (!b.fseqs.empty() && (fseqFirst || (b.perfs.empty() && b.voices.empty()))) {
            const int n = lib.number(lib.fseqs, k, 0);
            put(fseqUser, n);
            put(fseqBank, 0);
            loadUserFseq(n);
        } else if (!b.perfs.empty()) {
            const int n = lib.number(lib.perfs, k, 0);
            loadUserPerf(n);
        } else if (!b.voices.empty()) {
            const int p = selectedPart(), n = lib.number(lib.voices, k, 0);
            put(partUser[p], n);
            put(partBank[p], 1);
            loadPartVoice(p);
        }
    }

    // Export SysEx writes the whole unit (system, performance, its four voices and its Fseq); Store the
    // performance with its voices and Fseq, and when it lands in the library it is a user performance.
    void exportSyx(const std::string& path, bool system) {
        std::vector<uint8_t> dump, out;
        {
            std::lock_guard<std::mutex> g(ctl);
            dev.getState(dump);
        }
        for (size_t i = 0; i < dump.size();) {
            size_t j = i + 1;
            while (j < dump.size() && dump[j] != 0xF7) ++j;
            if (system || dump[i + 6] != 0x00) out.insert(out.end(), dump.begin() + (long)i, dump.begin() + (long)std::min(j + 1, dump.size()));
            i = j + 1;
        }
        if (!writeFile(path, out)) { message("Cannot write " + path); return; }
        std::lock_guard<std::mutex> g(ctl);
        lib.rescan();
        writeLists();
        const int k = lib.bankOf(path);
        if (!system && k >= 0 && !lib.banks[(size_t)k].perfs.empty()) {
            put(perfUser, lib.number(lib.perfs, k, 0));
            put(perfBank, 1);
            st.setData("perf.user.name", lib.banks[(size_t)k].perfs[0].name);
        }
        message("Saved " + std::filesystem::u8path(path).filename().u8string());
    }

    void exportFseq(const std::string& path) {
        std::vector<uint8_t> dump, fseq;
        {
            std::lock_guard<std::mutex> g(ctl);
            dev.getState(dump);
        }
        Model m;
        m.parse(dump, &fseq);
        if (fseq.empty()) { message("No Fseq is loaded"); return; }
        if (!writeFile(path, bulk(0x60, 0, 0, fseq.data(), fseq.size()))) message("Cannot write " + path);
        else message("Saved " + std::filesystem::u8path(path).filename().u8string());
    }

    // Import Audio: the analysis runs here, off the lock; the Fseq it makes is a bank of its own in the
    // library, named after the file, and plays on the performance's Fseq part (part 1 if none has it).
    void importAudio(const std::string& path) {
        message("Analysing " + std::filesystem::u8path(path).filename().u8string() + "...");
        std::vector<float> mono;
        double rate = 0;
        std::string err;
        if (!decodeAudio(path, mono, rate, err)) { message(err); return; }
        const std::string stem = std::filesystem::u8path(path).stem().u8string();
        int frames = 0;
        const std::vector<uint8_t> data = audioToFseq(mono, rate, stem, frames);
        std::lock_guard<std::mutex> g(ctl);
        const int k = lib.add(stem, bulk(0x60, 0, 0, data.data(), data.size()), err);
        if (k < 0) { message(err); return; }
        writeLists();
        const int n = lib.number(lib.fseqs, k, 0);
        put(fseqUser, n);
        put(fseqBank, 0);
        if (get(fseqPart) == 0) put(fseqPart, 1);
        loadUserFseq(n);
        message("Made \"" + lib.banks[(size_t)k].name + "\": " + std::to_string(frames) + " frames");
    }

    // ---- the monitor -------------------------------------------------------------------------------

    // The 32 harmonics of the last note played, their level in the output (-60..0 dB as 0..1), for the
    // monitor's harmonic display.
    void harmonics() {
        float out[32] = {};
        const int note = lastNote;
        if (note >= 0) {
            const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const size_t N = 4096, at = ringAt;
            for (int h = 0; h < 32; ++h) {
                const double f = f0 * (h + 1);
                if (f >= hostRate / 2) break;
                const double w = 2 * std::cos(2 * 3.14159265358979323846 * f / hostRate);
                double s1 = 0, s2 = 0;
                for (size_t i = 0; i < N; ++i) {   // Goertzel under a Hann window
                    const double x = ring[(at + ring.size() - N + i) % ring.size()] * (0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * i / N));
                    const double s = x + w * s1 - s2;
                    s2 = s1;
                    s1 = s;
                }
                const double mag = std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - w * s1 * s2)) * 4 / N;
                out[h] = mag > 1e-6 ? (float)std::clamp((20 * std::log10(mag) + 60) / 60, 0.0, 1.0) : 0.0f;
            }
        }
        st.setScope(out, 32);
    }
};

} // namespace fsvr

namespace hollow {

const Info& pluginInfo() {
    static const Info info{"studio.musica.fsvr", "FSVR", "musica.studio", "https://github.com/musicastudio/FSVR", FSVR_VERSION,
                           "Yamaha FS1R", /*instrument*/ true, /*inputs*/ 0, /*outputs*/ 2, /*midiIn*/ true, /*midiOut*/ true};
    return info;
}

std::unique_ptr<Processor> createProcessor(State& state) { return std::make_unique<fsvr::Fsvr>(state); }

} // namespace hollow

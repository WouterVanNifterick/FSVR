// patchfiles/performance.cpp - a DX performance as an FS1R performance bulk and the voices of its parts. Ours.
//
// The unit has no such conversion: the firmware takes only VCED and ACED. So every choice here and in the
// formats that fill a PartSetup is ours, not the hardware's. The bulk starts from the engine's init
// performance (init_perf), and a DX performance changes only its name and the part bytes PartSetup names.
#include "dx7.h"
#include "presets/performance.h"
#include <memory>

namespace fsvr::patchfiles {

constexpr size_t PERF_SIZE = 400, COMMON = 192, PART = 52, PERF_NAME = 12;
constexpr uint8_t BANK_OFF = 0, BANK_INT = 1;

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

Bytes performanceBulk(const std::string& name, const PartSetup* parts) {
    auto P = std::make_unique<Perf>();
    init_perf(*P);
    for (size_t k = 0; k < PERF_NAME; ++k) {
        const char c = k < name.size() ? name[k] : ' ';
        P->c[k] = (uint8_t)(c >= 32 && c < 127 ? c : ' ');
    }
    for (int i = 0; i < 4; ++i) {
        const PartSetup& s = parts[i];
        uint8_t* p = P->part[i].p;
        // Ours: a part with no DX voice is off, so it neither sounds nor names a voice the library would look up.
        p[1] = s.on ? BANK_INT : BANK_OFF;
        p[2] = (uint8_t)clamp(s.voice, 0, 127);
        p[4] = (uint8_t)(s.on ? s.channel : OFF);
        p[8] = (uint8_t)clamp(24 + s.shift, 0, 48);
        p[9] = (uint8_t)clamp(64 + s.detune, 0, 127);
        p[0x0B] = (uint8_t)clamp(s.volume, 0, 127);
        p[0x0E] = (uint8_t)clamp(s.pan, 1, 127);
        p[0x0F] = (uint8_t)clamp(s.low, 0, 127);
        p[0x10] = (uint8_t)clamp(s.high, 0, 127);
    }
    Bytes m = {0xF0, 0x43, 0x00, 0x5E, PERF_SIZE >> 7, PERF_SIZE & 0x7F, 0x10, 0x00, 0x00};
    m.insert(m.end(), P->c, P->c + sizeof P->c);
    m.insert(m.end(), P->fx, P->fx + sizeof P->fx);
    for (const Part& pt : P->part) m.insert(m.end(), pt.p, pt.p + PART);
    m.push_back(checksum(m.data() + 4, m.size() - 4));
    m.push_back(0xF7);
    static_assert(sizeof(Perf::c) + sizeof(Perf::fx) == COMMON, "the 192 common bytes");
    return m;
}

// Which banks a voice may come from: any, only those behind a bank select, or only those with none.
enum class From { Any, Selected, Unselected };

// A voice number (0-127) in banks of the kind given: one read in this file, else one read earlier in the files
// read with it, copied in.
static int voiceFor(Reading& r, int number, From from) {
    if (number < 0 || number > 127) return -1;
    const size_t n = (size_t)number;
    auto fits = [&](bool selected) { return from == From::Any || selected == (from == From::Selected); };
    if (r.numbered[n] >= 0 && fits(r.selected[n])) return r.numbered[n];
    if (r.before[n] >= 0 && fits(r.beforeSelected[n])) return r.before[n];
    const std::vector<Pending::Voice>& earlier = r.pending.numbered;
    if (n >= earlier.size() || earlier[n].item.syx.empty() || !fits(earlier[n].selected)) return -1;
    if (std::find(r.borrowed.begin(), r.borrowed.end(), earlier[n].file) == r.borrowed.end()) r.borrowed.push_back(earlier[n].file);
    Item it = earlier[n].item;
    it.at = it.len = 0;   // its bytes are in another file
    it.slot = -1;
    it.vmem.reset();
    r.bank.voices.push_back(std::move(it));
    const int k = (int)r.bank.voices.size() - 1;
    if (r.numbered[n] < 0) { r.numbered[n] = k; r.selected[n] = earlier[n].selected; }
    return k;
}

// Ours, settled against the TX802 factory set (P.SYX and A1, A2, B1, B2 beside it): a TX802 voice number counts
// I01-I64, C01-C64, then the ROM's presets A01-A64 and B01-B64, and the factory performances name the presets
// (0x82 is A03, ReverbBras). The presets cannot be in a dump of the unit's memory. The set sends them as the four
// files A1, A2, B1, B2, VMEMs with no bank select, which number on 0-127 in that order; so a preset voice is
// looked up at its number less 128, among banks that came with no bank select. When the TX802 performances read
// name a preset voice, those banks are taken to be the presets, and none of them then stands for a voice of
// the unit's memory.
constexpr int TX802_PRESETS = 128;

static bool presetBanks(const Reading& r) {
    for (const DxPerformance& d : r.dx)
        for (const PartSetup& s : d.part)
            if (d.tx802 && s.on && s.voice >= TX802_PRESETS) return true;
    return false;
}

static int partVoice(Reading& r, const DxPerformance& d, int number, bool presets) {
    if (!d.tx802) return voiceFor(r, number, From::Any);
    if (number >= TX802_PRESETS) return voiceFor(r, number - TX802_PRESETS, From::Unselected);
    return voiceFor(r, number, presets ? From::Selected : From::Any);
}

// Ours: a dump of the internal memory and one of the cartridge both send their banks as voices 1-64, and
// nothing in either says which it is. A file whose banks are all in 1-64 and whose performances name a
// cartridge voice (65-128) is taken to be the cartridge's dump, as the DX7IIFD factory CART.SYX is.
static void cartridgeBanks(Reading& r) {
    bool low = false, high = false, names = false;
    for (int v = 0; v < 128; ++v)
        if (r.numbered[(size_t)v] >= 0) (v < 64 ? low : high) = true;
    for (const DxPerformance& d : r.dx)
        for (const PartSetup& s : d.part) names |= s.on && s.voice >= 64 && s.voice < 128;
    if (!low || high || !names) return;
    for (size_t v = 0; v < 64; ++v) {
        r.numbered[v + 64] = r.numbered[v];
        r.selected[v + 64] = r.selected[v];
        r.numbered[v] = -1;
    }
}

// The DX performances read so far as items, their voices found by number in the banks read so far.
static void resolve(Reading& r) {
    cartridgeBanks(r);
    const bool presets = presetBanks(r);
    for (DxPerformance& d : r.dx) {
        int found = 0, missing = 0, voice[4] = {-1, -1, -1, -1};
        for (int i = 0; i < 4; ++i) {
            PartSetup& s = d.part[i];
            if (!s.on) continue;
            voice[i] = partVoice(r, d, s.voice, presets);
            if (voice[i] < 0) { s.on = false; ++missing; } else ++found;
        }
        const std::string what = (r.bank.name.empty() ? "" : r.bank.name + ", ") + d.kind;
        if (!found) { r.report.skipped.push_back(what + " whose voices are not in the files read"); continue; }
        if (missing) r.report.warnings.push_back(what + " voice not in the files read, its part left off");
        Item it;
        it.name = text(reinterpret_cast<const uint8_t*>(d.name.data()), std::min(d.name.size(), PERF_NAME));
        it.address = 0x10;
        it.fromDx = true;
        it.syx = performanceBulk(it.name, d.part);
        it.at = d.at;
        it.len = d.len;
        for (int i = 0; i < 4; ++i) it.partVoice[i] = d.part[i].on ? voice[i] : -1;
        r.bank.perfs.push_back(std::move(it));
    }
    r.dx.clear();
}

void newDump(Reading& r) {
    resolve(r);
    for (size_t v = 0; v < 128; ++v)
        if (r.numbered[v] >= 0) {
            r.before[v] = r.numbered[v];
            r.beforeSelected[v] = r.selected[v];
            r.numbered[v] = -1;
        }
}

void finishPerformances(Reading& r) {
    resolve(r);
    if (!r.borrowed.empty()) {
        std::string files;
        for (const std::string& f : r.borrowed) files += (files.empty() ? "" : ", ") + (f.empty() ? std::string("a file") : f);
        r.report.warnings.push_back((r.bank.name.empty() ? "" : r.bank.name + ", ") + "performances play voices from " + files + " read with it");
    }
    // This file's banks, by number, for the performances in the files read after it.
    r.pending.numbered.resize(128);
    for (size_t v = 0; v < 128; ++v) {
        const bool now = r.numbered[v] >= 0;
        const int k = now ? r.numbered[v] : r.before[v];
        if (k >= 0 && r.bank.voices[(size_t)k].slot >= 0) r.pending.numbered[v] = {r.bank.voices[(size_t)k], now ? r.selected[v] : r.beforeSelected[v], r.bank.name};
    }
}

} // namespace fsvr::patchfiles

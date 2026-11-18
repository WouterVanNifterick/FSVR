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

// A voice number's voice: one read in this file, else one read earlier in the files read with it, copied in.
static int voiceFor(Reading& r, int number) {
    if (number < 0 || number > 127) return -1;
    if (r.numbered[(size_t)number] >= 0) return r.numbered[(size_t)number];
    const std::vector<Item>& earlier = r.pending.numbered;
    if ((size_t)number >= earlier.size() || earlier[(size_t)number].syx.empty()) return -1;
    Item it = earlier[(size_t)number];
    it.at = it.len = 0;   // its bytes are in another file
    it.slot = -1;
    it.vmem.reset();
    r.bank.voices.push_back(std::move(it));
    return r.numbered[(size_t)number] = (int)r.bank.voices.size() - 1;
}

void finishPerformances(Reading& r) {
    bool cartridge = false;
    for (DxPerformance& d : r.dx) {
        int found = 0, missing = 0, voice[4] = {-1, -1, -1, -1};
        for (int i = 0; i < 4; ++i) {
            PartSetup& s = d.part[i];
            if (!s.on) continue;
            voice[i] = voiceFor(r, s.voice);
            // Ours: a dump holds the internal voices or the cartridge's, and nothing in it says which, so a
            // number past 64 that no bank in the read holds is looked up in the first 64.
            if (voice[i] < 0 && s.voice >= 64 && (voice[i] = voiceFor(r, s.voice - 64)) >= 0) cartridge = true;
            if (voice[i] < 0) { s.on = false; ++missing; } else ++found;
        }
        const std::string what = (r.bank.name.empty() ? "" : r.bank.name + ", ") + d.kind;
        if (!found) { r.report.skipped.push_back(what + " whose voices are not in the files read"); continue; }
        if (missing) r.report.warnings.push_back(what + " voice not in the files read, its part left off");
        Item it;
        it.name = text(reinterpret_cast<const uint8_t*>(d.name.data()), std::min(d.name.size(), PERF_NAME));
        it.address = 0x10;
        it.syx = performanceBulk(it.name, d.part);
        it.at = d.at;
        it.len = d.len;
        for (int i = 0; i < 4; ++i) it.partVoice[i] = d.part[i].on ? voice[i] : -1;
        r.bank.perfs.push_back(std::move(it));
    }
    if (cartridge) r.report.warnings.push_back((r.bank.name.empty() ? "" : r.bank.name + ", ") + "cartridge voices 65-128 read from the banks of voices 1-64");
    r.dx.clear();
}

} // namespace fsvr::patchfiles

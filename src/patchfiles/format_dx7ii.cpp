// patchfiles/format_dx7ii.cpp - a DX7II performance: the edit buffer, PCED, F0 43 0n 7E <count> "LM  8973PE"
// <51> cs F7, and the 32 of PMEM, "LM  8973PM" <32 x 51>. Ours. The checksum is over the tag and the data,
// as the factory dumps have it. The record's layout is the DX7II's PCED (DX7IIFD MIDI data format).
//
// Single, dual and split go onto parts 1 and 2, voice A on part 1. Their voices are looked up by number,
// 0-63 internal and 64-127 cartridge, once the whole file is read (performance.cpp).
#include "dx7.h"
#include <cmath>

namespace fsvr::patchfiles {

constexpr size_t RECORD_SIZE = 51, TAG = 10, NAME_AT = 31, NAME_SIZE = 20;
constexpr int SINGLE = 0, DUAL = 1, SPLIT = 2;

static std::string trimmed(const uint8_t* p, size_t n) {
    std::string s = text(p, n);
    s.erase(0, s.find_first_not_of(' '));
    return s;
}

static DxPerformance performance(const uint8_t* b) {
    DxPerformance d;
    d.kind = "a DX7II performance";
    d.name = trimmed(b + NAME_AT, NAME_SIZE);
    const int mode = b[0] & 3, ddtn = b[6] & 7, split = b[7] & 0x7F, balance = std::min(b[15] & 0x7F, 100) - 50;
    const double total = std::min(b[16] & 0x7F, 99) / 99.0;
    PartSetup& a = d.part[0];
    a.on = true;
    a.voice = b[1] & 0x7F;
    a.shift = (b[13] & 0x3F) - 24;
    a.volume = (int)std::lround(127 * total);
    if (mode != DUAL && mode != SPLIT) return d;
    PartSetup& v = d.part[1];
    v = a;
    v.voice = b[2] & 0x7F;
    v.shift = (b[14] & 0x3F) - 24;
    // Ours, after the DX7II's own: balance at -50 leaves voice A full and B silent, at +50 the other way,
    // each falling linearly from the centre; the gain goes on the part volume.
    a.volume = (int)std::lround(127 * total * std::min(1.0, (50 - balance) / 50.0));
    v.volume = (int)std::lround(127 * total * std::min(1.0, (50 + balance) / 50.0));
    // Ours: dual detune raises A and lowers B by its own step, one FS1R part detune step each.
    a.detune = ddtn;
    v.detune = -ddtn;
    if (mode == SPLIT) { a.high = std::max(0, split - 1); v.low = split; }   // voice B from the split point up
    return d;
}

bool readDx7ii(const Message& m, Reading& r) {
    const bool bank = lmBulk(m, "LM  8973PM");
    if (!bank && !lmBulk(m, "LM  8973PE")) return false;
    const size_t data = 6 + TAG, n = (m.len - data - 2) / RECORD_SIZE;
    if (!n) return false;
    checkSum(m, 6, TAG + n * RECORD_SIZE, bank ? "PMEM" : "PCED", r);
    for (size_t k = 0; k < (bank ? n : 1); ++k) {
        DxPerformance d = performance(m.p + data + k * RECORD_SIZE);
        d.at = m.at;
        d.len = m.len;
        r.dx.push_back(std::move(d));
    }
    r.open = -1;
    return true;
}

} // namespace fsvr::patchfiles

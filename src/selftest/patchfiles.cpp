// selftest/patchfiles.cpp - the patch file reader driven with small files built here, one per format and container.
#include "patchfiles/patchfiles.h"
#include <cstdio>
#include <cstring>

namespace {

using Bytes = std::vector<uint8_t>;
int fails = 0;
void ck(const char* what, bool ok) { if (!ok) { printf("  FAIL %s\n", what); fails++; } }

uint8_t sum7(const uint8_t* p, size_t n) { int s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return (uint8_t)(-s & 0x7F); }

// A Yamaha bulk: F0 43 0n <format> <count> <data> <checksum> F7, the checksum over the data.
Bytes dxBulk(uint8_t format, uint8_t c1, uint8_t c2, const Bytes& data) {
    Bytes m = {0xF0, 0x43, 0x00, format, c1, c2};
    m.insert(m.end(), data.begin(), data.end());
    m.push_back(sum7(data.data(), data.size())); m.push_back(0xF7);
    return m;
}

// An FS1R voice bulk (608 bytes) for part 1, named.
Bytes fs1rVoice(const char* name) {
    Bytes d(608, 0); memcpy(d.data(), name, strlen(name)); d[0x0E] = 5;
    Bytes m = {0xF0, 0x43, 0x00, 0x5E, 608 >> 7, 608 & 0x7F, 0x40, 0x00, 0x00};
    m.insert(m.end(), d.begin(), d.end());
    m.push_back(sum7(m.data() + 4, m.size() - 4)); m.push_back(0xF7);
    return m;
}

// A packed VMEM record, named, every byte valid.
Bytes record(const char* name) {
    Bytes r(128, 0);
    for (int op = 0; op < 6; op++) { uint8_t* p = r.data() + op * 17; for (int k = 0; k < 4; k++) { p[k] = 99; p[4 + k] = 99; } p[12] = 7 << 3; p[15] = 2; }
    for (int k = 0; k < 4; k++) { r[102 + k] = 99; r[106 + k] = 50; }
    r[117] = 24;
    char n[11]; snprintf(n, sizeof n, "%-10s", name); memcpy(r.data() + 118, n, 10);
    return r;
}

Bytes vmemData() {
    Bytes d;
    for (int v = 0; v < 32; v++) { char n[11]; snprintf(n, sizeof n, "VOICE%02d", v + 1); Bytes r = record(n); d.insert(d.end(), r.begin(), r.end()); }
    return d;
}

Bytes cat(std::initializer_list<Bytes> parts) { Bytes b; for (auto& p : parts) b.insert(b.end(), p.begin(), p.end()); return b; }

// The VCED a DX voice item sends, after its ACED.
const uint8_t* vcedOf(const fsvr::Item& it) {
    size_t f = it.syx.size(); while (f > 0 && it.syx[f - 1] != 0xF0) f--;
    return f && it.syx.size() >= f - 1 + 163 && it.syx[f + 2] == 0x00 && it.syx[f + 4] == 0x1B ? it.syx.data() + f + 5 : nullptr;
}
// The ACED before it, or null.
const uint8_t* acedOf(const fsvr::Item& it) {
    return it.syx.size() > 6 + 49 && it.syx[3] == 0x05 && it.syx[5] == 0x31 ? it.syx.data() + 6 : nullptr;
}

} // namespace

int selftest_patchfiles() {
    using namespace fsvr;
    fails = 0;

    // A sysex stream: an FS1R voice, and a DX VCED with the ACED before it, read as two voices.
    { Bytes vced(155, 0); memcpy(vced.data() + 145, "DX SINGLE ", 10);
      Bytes aced(49, 0); aced[6] = 7;
      const Bytes f = cat({fs1rVoice("Native"), dxBulk(0x05, 0x00, 0x31, aced), dxBulk(0x00, 0x01, 0x1B, vced)});
      Report r; const Bank b = readPatches(f, "Stream", &r);
      ck("a sysex stream reads an FS1R voice and a DX VCED", b.voices.size() == 2 && b.voices[0].name == "Native" && b.voices[0].address == 0x40 &&
                                                            b.voices[0].category == 5 && b.voices[1].name == "DX SINGLE" && !b.voices[1].address);
      ck("a DX VCED carries the ACED before it", b.voices.size() == 2 && acedOf(b.voices[1]) && acedOf(b.voices[1])[6] == 7 && vcedOf(b.voices[1]) &&
                                                 b.voices[1].at == fs1rVoice("Native").size() && b.voices[1].at + b.voices[1].len == f.size());
      ck("a clean stream reports nothing", r.skipped.empty() && r.warnings.empty()); }

    // A DX7 VMEM (20 00) and a DX5's (10 00) both read as 32 voices, each knowing its slot.
    for (uint8_t count : {0x20, 0x10}) {
        const Bytes f = dxBulk(0x09, count, 0x00, vmemData());
        const Bank b = readPatches(f, "Bank");
        ck(count == 0x20 ? "a DX7 VMEM reads as 32 voices" : "a DX5 VMEM (byte count 10 00) reads as 32 voices",
           b.voices.size() == 32 && b.voices[0].name == "VOICE01" && b.voices[31].name == "VOICE32" && b.voices[31].slot == 31 &&
           b.voices[5].at == 0 && b.voices[5].len == f.size() && vcedOf(b.voices[5]) && !acedOf(b.voices[5]));
    }

    return fails;
}

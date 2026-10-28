// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// io/factory.h - the factory banks without the EPROM: the performances, voices and Fseqs the plug-in
// embeds (plugin/generated), reached by the engine's own index numbers. A performance loaded this way
// renders what the EPROM path renders (perfbench --check-factory proves it), so the bench and the
// render checks run on a machine that has no image.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "fs1r.h"

namespace fs1r_factory {

struct Blobs {
    std::vector<uint8_t> perfs, voices, fseqs;   // fs1r_performances.syx, fs1r_presets.syx, fs1r_fseqs.syx
    std::string root;                            // the repository, where plugin/generated was found

    // The repository root is FSVR_ROOT, else the first folder holding plugin/generated among the
    // executable's folder and its parents (bin/ sits in the root) and the working directory's.
    bool load(const char* argv0) {
        std::vector<std::string> tries;
        if (const char* r = getenv("FSVR_ROOT")) tries.push_back(r);
        std::string exe = argv0 ? argv0 : "";
        size_t k = exe.find_last_of("\\/");
        std::string dir = k == std::string::npos ? "." : exe.substr(0, k);
        for (int up = 0; up < 3; up++) { tries.push_back(dir); dir += "/.."; }
        tries.push_back("."); tries.push_back("..");
        for (auto& t : tries)
            if (read(t + "/plugin/generated/fs1r_performances.syx", perfs) &&
                read(t + "/plugin/generated/fs1r_presets.syx", voices) &&
                read(t + "/plugin/generated/fs1r_fseqs.syx", fseqs)) { root = t; return true; }
        return false;
    }
    static bool read(const std::string& path, std::vector<uint8_t>& out) {
        FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
        out.clear(); uint8_t tmp[65536]; size_t n;
        while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) out.insert(out.end(), tmp, tmp + n);
        fclose(f);
        return !out.empty();
    }
    // The n-th dump's bytes, or null.
    const uint8_t* dump(const std::vector<uint8_t>& blob, int n) const {
        for (size_t i = 0; i < blob.size(); i++) {
            if (blob[i] != 0xF0) continue;
            if (n-- == 0) return blob.data() + i;
            while (i < blob.size() && blob[i] != 0xF7) i++;
        }
        return nullptr;
    }
};

// The voice a part's bank and program bytes name, as presets/rom.cpp's bank_voice_index reads them: PrA and
// PrB are the native voices 0..255, PrC..PrK the DX7 voices 256..1407, any other bank the program alone.
inline int voice_index(int bank, int prog) {
    prog &= 0x7F;
    if (bank == 2 || bank == 3) return (bank - 2) * 128 + prog;
    if (bank >= 4 && bank <= 12) return 256 + (bank - 4) * 128 + prog;
    return prog;
}

inline bool voice(fsvr::Device& dev, const Blobs& b, int part, int index) {
    return dev.loadSyx(b.voices.data(), b.voices.size(), index, part);
}
inline bool fseq(fsvr::Device& dev, const Blobs& b, int number) {   // 1..90, as loadRomFseq counts
    return dev.loadSyx(b.fseqs.data(), b.fseqs.size(), number - 1, 0);
}
// The performance, then each part's voice as its bank and program bytes name it, then the Fseq when the
// performance has an Fseq part and plays a preset one: what perf_from_bytes does with the EPROM in hand.
inline bool performance(fsvr::Device& dev, const Blobs& b, int index) {
    const uint8_t* d = b.dump(b.perfs, index);
    if (!d || !dev.loadSyx(b.perfs.data(), b.perfs.size(), index, 0)) return false;
    const uint8_t* c = d + 9;                    // the 400 bytes: common, effects, then 52 a part at 192
    for (int p = 0; p < 4; p++) {
        const int bank = c[192 + 52 * p + 1], prog = c[192 + 52 * p + 2];
        if (bank && !voice(dev, b, p, voice_index(bank, prog))) return false;
    }
    if ((c[0x15] & 7) && (c[0x16] & 1) && !fseq(dev, b, c[0x17] + 1)) return false;
    return true;
}

}   // namespace fs1r_factory

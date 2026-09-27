#include "library.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace fsvr {

const char* const kCategories[23] = {"--", "Piano", "Chromatic", "Organ", "Guitar", "Bass", "Strings", "Ensemble", "Brass", "Reed",
                                     "Pipe", "Synth Lead", "Synth Pad", "Synth FX", "Ethnic", "Percussive", "Sound FX", "Drums",
                                     "Synth Comp", "Vocal", "Combination", "Material Wave", "Sequence"};

static std::string text(const uint8_t* p, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += p[i] >= 32 && p[i] < 127 ? (char)p[i] : ' ';
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::vector<uint8_t> bulk(int ah, int am, int al, const uint8_t* data, size_t size) {
    std::vector<uint8_t> m = {0xF0, 0x43, 0x00, 0x5E, (uint8_t)(size >> 7 & 0x7F), (uint8_t)(size & 0x7F), (uint8_t)ah, (uint8_t)am, (uint8_t)al};
    m.insert(m.end(), data, data + size);
    int sum = 0;
    for (size_t i = 4; i < m.size(); ++i) sum += m[i];
    m.push_back((uint8_t)(-sum & 0x7F));
    m.push_back(0xF7);
    return m;
}

std::vector<uint8_t> readdress(const std::vector<uint8_t>& syx, int ah, int am, int al) {
    if (syx.size() < 12 || syx[3] != 0x5E) return syx;
    return bulk(ah, am, al, syx.data() + 9, syx.size() - 11);
}

// A DX7 VMEM voice (128 packed bytes) as VCED (155), DX7 sysex format: six operators, OP6 first, then the
// voice's common block.
static void unpackVmem(const uint8_t* v, uint8_t* o) {
    for (int op = 0; op < 6; ++op) {
        const uint8_t* p = v + op * 17;
        uint8_t* d = o + op * 21;
        for (int k = 0; k < 11; ++k) d[k] = p[k] & 0x7F;              // rates, levels, break point, depths
        d[11] = p[11] & 3; d[12] = p[11] >> 2 & 3;                      // curves
        d[13] = p[12] & 7; d[20] = p[12] >> 3 & 15;                     // rate scaling, detune
        d[14] = p[13] & 3; d[15] = p[13] >> 2 & 7;                      // amp mod, key velocity sensitivity
        d[16] = p[14] & 0x7F;                                           // output level
        d[17] = p[15] & 1; d[18] = p[15] >> 1 & 31; d[19] = p[16] & 0x7F;   // mode, coarse, fine
    }
    for (int k = 0; k < 8; ++k) o[126 + k] = v[102 + k] & 0x7F;         // pitch EG
    o[134] = v[110] & 31; o[135] = v[111] & 7; o[136] = v[111] >> 3 & 1;
    for (int k = 0; k < 4; ++k) o[137 + k] = v[112 + k] & 0x7F;         // LFO speed, delay, depths
    o[141] = v[116] & 1; o[142] = v[116] >> 1 & 7; o[143] = v[116] >> 4 & 7;
    o[144] = v[117] & 0x7F;
    for (int k = 0; k < 10; ++k) o[145 + k] = v[118 + k] & 0x7F;
}

static Item dxVoice(const uint8_t* vced, const std::vector<uint8_t>& aced) {
    Item it;
    it.name = text(vced + 145, 10);
    it.syx = aced;
    std::vector<uint8_t> m = {0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B};
    m.insert(m.end(), vced, vced + 155);
    int sum = 0;
    for (int k = 0; k < 155; ++k) sum += vced[k];
    m.push_back((uint8_t)(-sum & 0x7F));
    m.push_back(0xF7);
    it.syx.insert(it.syx.end(), m.begin(), m.end());
    return it;
}

Bank parseSyx(const std::vector<uint8_t>& b, const std::string& name) {
    Bank bank;
    bank.name = name;
    std::vector<uint8_t> aced;   // a DX ACED waits for the VCED it completes
    int open = -1;               // the performance that voice and Fseq bulks right after it belong to
    const size_t n = b.size();
    for (size_t i = 0; i < n;) {
        if (b[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < n && b[j] != 0xF7) ++j;
        if (j >= n) break;
        const uint8_t* m = b.data() + i;
        const size_t len = j - i + 1;
        std::vector<uint8_t> msg(m, m + len);
        i = j + 1;
        if (len < 8 || m[1] != 0x43 || (m[2] & 0xF0) != 0) continue;
        if (m[3] == 0x5E && len >= 11) {
            const int ah = m[6];
            const uint8_t* d = m + 9;
            const size_t dl = len - 11;
            Item it;
            it.address = ah;
            it.syx = msg;
            if ((ah == 0x10 || ah == 0x11) && dl >= 400) {
                it.name = text(d, 12);
                it.category = d[14];
                it.number = ah == 0x11 ? m[8] : -1;
                bank.perfs.push_back(it);
                open = (int)bank.perfs.size() - 1;
            } else if (((ah >= 0x40 && ah <= 0x43) || ah == 0x51) && dl >= 608) {
                it.name = text(d, 10);
                it.category = d[0x0E];
                it.number = ah == 0x51 ? m[8] : -1;
                bank.voices.push_back(it);
                if (ah <= 0x43 && open >= 0) bank.perfs[(size_t)open].partVoice[ah - 0x40] = (int)bank.voices.size() - 1;
                else open = -1;
            } else if ((ah == 0x60 || ah == 0x61 || ah == 0x70) && dl >= 32) {
                it.name = text(d, 8);
                const int total = 128 * ((d[0x1B] & 3) + 1), end = d[0x1E] << 7 | d[0x1F];
                it.frames = end ? std::min(total, end + 1) : total;
                it.number = ah == 0x61 ? m[8] : -1;
                bank.fseqs.push_back(it);
                if (open >= 0) bank.perfs[(size_t)open].fseq = (int)bank.fseqs.size() - 1;
                open = -1;
            } else {
                open = -1;
            }
        } else if (m[3] == 0x05 && m[4] == 0x00 && m[5] == 0x31) {
            aced = msg;
        } else if (m[3] == 0x00 && m[4] == 0x01 && m[5] == 0x1B && len >= 6 + 155 + 2) {
            bank.voices.push_back(dxVoice(m + 6, aced));
            aced.clear();
            open = -1;
        } else if (m[3] == 0x09 && m[4] == 0x20 && m[5] == 0x00 && len >= 6 + 4096 + 2) {
            uint8_t vced[155];
            for (int k = 0; k < 32; ++k) {
                unpackVmem(m + 6 + 128 * k, vced);
                bank.voices.push_back(dxVoice(vced, {}));
            }
            open = -1;
        }
    }
    return bank;
}

bool readFile(const std::string& utf8Path, std::vector<uint8_t>& out) {
    std::ifstream f(fs::u8path(utf8Path), std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

bool writeFile(const std::string& utf8Path, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    fs::create_directories(fs::u8path(utf8Path).parent_path(), ec);
    std::ofstream f(fs::u8path(utf8Path), std::ios::binary | std::ios::trunc);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

Library::Library() {
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)   // read once
#endif
    const char* env = std::getenv("FSVR_LIBRARY");
    if (env && *env) {
        dir = env;
        return;
    }
    fs::path docs;
#ifdef _WIN32
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) docs = p;
    CoTaskMemFree(p);
#else
    const char* home = std::getenv("HOME");
    docs = fs::path(home ? home : ".") / "Documents";
#endif
    dir = (docs / "FSVR" / "Library").u8string();
}

bool Library::rescan(bool force) {
    std::vector<fs::path> files;
    std::string stamp;
    std::error_code ec;
    for (fs::directory_iterator it(fs::u8path(dir), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || lower(it->path().extension().u8string()) != ".syx") continue;
        files.push_back(it->path());
    }
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) { return lower(a.stem().u8string()) < lower(b.stem().u8string()); });
    for (auto& f : files) {
        auto t = fs::last_write_time(f, ec).time_since_epoch().count();
        stamp += f.filename().u8string() + "|" + std::to_string(fs::file_size(f, ec)) + "|" + std::to_string((long long)t) + "\n";
    }
    if (!force && stamp == stamp_) return false;
    stamp_ = stamp;
    banks.clear();
    for (auto& f : files) {
        std::vector<uint8_t> bytes;
        if (!readFile(f.u8string(), bytes)) continue;
        Bank b = parseSyx(bytes, f.stem().u8string());
        if (b.empty()) continue;
        b.path = f.u8string();
        banks.push_back(std::move(b));
    }
    perfs.clear();
    voices.clear();
    fseqs.clear();
    for (int k = 0; k < (int)banks.size(); ++k) {
        for (int i = 0; i < (int)banks[(size_t)k].perfs.size(); ++i) perfs.push_back({k, i});
        for (int i = 0; i < (int)banks[(size_t)k].voices.size(); ++i) voices.push_back({k, i});
        for (int i = 0; i < (int)banks[(size_t)k].fseqs.size(); ++i) fseqs.push_back({k, i});
    }
    return true;
}

std::string Library::uniqueName(std::string stem) const {
    for (char& c : stem)
        if (std::strchr("<>:\"/\\|?*", c) || (unsigned char)c < 32) c = '_';
    if (stem.empty()) stem = "Bank";
    std::error_code ec;
    std::string name = stem;
    for (int k = 2; fs::exists(fs::u8path(dir) / fs::u8path(name + ".syx"), ec); ++k) name = stem + " " + std::to_string(k);
    return name;
}

int Library::bankOf(const std::string& path) const {
    std::error_code ec;
    for (int k = 0; k < (int)banks.size(); ++k)
        if (fs::equivalent(fs::u8path(banks[(size_t)k].path), fs::u8path(path), ec)) return k;
    return -1;
}

int Library::add(const std::string& name, const std::vector<uint8_t>& bytes, std::string& err) {
    const std::string path = (fs::u8path(dir) / fs::u8path(uniqueName(name) + ".syx")).u8string();
    if (!writeFile(path, bytes)) {
        err = "cannot write " + path;
        return -1;
    }
    rescan(true);
    return bankOf(path);
}

int Library::import(const std::string& path, std::string& err) {
    std::vector<uint8_t> bytes;
    if (!readFile(path, bytes)) {
        err = "cannot read " + path;
        return -1;
    }
    if (parseSyx(bytes, "").empty()) {
        err = "no performances, voices or Fseqs in " + path;
        return -1;
    }
    return add(fs::u8path(path).stem().u8string(), bytes, err);
}

int Library::number(const std::vector<Ref>& list, int bank, int index) const {
    for (size_t k = 0; k < list.size(); ++k)
        if (list[k].bank == bank && list[k].index == index) return (int)k + 1;
    return 0;
}

} // namespace fsvr

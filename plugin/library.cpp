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

int nameLength(const Item& it) { return it.address >= 0x60 ? 8 : it.address >= 0x40 || !it.address ? 10 : 12; }

// The checksum of the last message in syx: a native bulk's over its byte count, address and data, a DX
// VCED's over its 155 data bytes.
static void resum(std::vector<uint8_t>& syx) {
    size_t f = syx.size();
    while (f > 0 && syx[f - 1] != 0xF0) --f;   // just past the last F0: a DX voice's VCED follows its ACED
    if (f == 0 || syx.size() < f + 5) return;
    const size_t start = f - 1 + (syx[f + 2] == 0x5E ? 4 : 6), cs = syx.size() - 2;
    int sum = 0;
    for (size_t i = start; i < cs; ++i) sum += syx[i];
    syx[cs] = (uint8_t)(-sum & 0x7F);
}

std::vector<uint8_t> renamed(const Item& it, const std::string& name) {
    if (it.slot >= 0) return vmemRenamed(it, name);
    std::vector<uint8_t> s = it.syx;
    const int n = nameLength(it);
    size_t at = 9;
    if (!it.address) {   // a DX voice: VCED bytes 145..154, after the ACED
        at = s.size();
        while (at > 0 && s[at - 1] != 0xF0) --at;
        at += 5 + 145;
    }
    if (at + (size_t)n + 2 > s.size()) return s;
    for (int k = 0; k < n; ++k) {
        const char c = k < (int)name.size() ? name[(size_t)k] : ' ';
        s[at + (size_t)k] = (uint8_t)(c >= 32 && c < 127 ? c : ' ');
    }
    resum(s);
    return s;
}

std::vector<uint8_t> recategorized(const Item& it, int category) {
    std::vector<uint8_t> s = it.syx;
    const size_t at = it.address == 0x10 || it.address == 0x11 ? 9 + 14 : it.address >= 0x40 && it.address <= 0x51 ? 9 + 0x0E : 0;
    if (!at || at + 2 > s.size()) return s;
    s[at] = (uint8_t)std::clamp(category, 0, 22);
    resum(s);
    return s;
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

// A folder the system will not let the app read lists as empty, and the banks just vanish. macOS does that to
// ~/Documents for an app it has no grant for, and a new version of an unsigned app is a new app to it.
static std::string unreadable(const std::string& what, const std::error_code& ec) {
    std::string s = "Cannot read " + what + ": " + ec.message();
#ifdef __APPLE__
    if (ec == std::errc::operation_not_permitted)
        s += ". Allow FSVR under System Settings, Privacy & Security, Files & Folders";
#endif
    return s;
}

bool Library::rescan(bool force) {
    std::vector<fs::path> files;
    std::string stamp;
    std::error_code ec;
    error.clear();
    for (fs::directory_iterator it(fs::u8path(dir), ec), end; !ec && it != end; it.increment(ec)) {
        const std::string ext = lower(it->path().extension().u8string());
        if (!it->is_regular_file(ec) || (ext != ".syx" && ext != ".fs1")) continue;   // an .fs1 is a performance and its four voices
        files.push_back(it->path());
    }
    if (ec && ec != std::errc::no_such_file_or_directory) error = unreadable(dir, ec);   // no folder yet is an empty library
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) { return lower(a.stem().u8string()) < lower(b.stem().u8string()); });
    for (auto& f : files) {
        auto t = fs::last_write_time(f, ec).time_since_epoch().count();
        stamp += f.filename().u8string() + "|" + std::to_string(fs::file_size(f, ec)) + "|" + std::to_string((long long)t) + "\n";
    }
    if (!force && stamp == stamp_) return false;
    stamp_ = stamp;
    banks = builtin;
    for (auto& f : files) {
        std::vector<uint8_t> bytes;
        if (!readFile(f.u8string(), bytes)) {
            if (error.empty()) error = "Cannot read " + f.u8string();
            continue;
        }
        Bank b = readPatches(bytes, f.stem().u8string());
        if (b.empty()) continue;
        b.path = f.u8string();
        banks.push_back(std::move(b));
    }
    perfs.clear();
    voices.clear();
    fseqs.clear();
    const int nb = (int)builtin.size();
    for (int j = 0; j < (int)banks.size(); ++j) {
        const int k = (j + nb) % (int)banks.size();   // the folder's banks, then the built-in ones
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
    const int k = bankOf(path);
    if (k < 0) err = error.empty() ? "cannot find " + path + " after writing it" : error;   // written, but the folder cannot be read back
    return k;
}

bool importPaths(const std::vector<std::string>& paths, std::vector<std::string>& files, std::string& err) {
    std::error_code ec;
    for (auto& path : paths) {
        const fs::path p = fs::u8path(path);
        if (!fs::is_directory(p, ec)) { files.push_back(path); continue; }
        std::vector<std::string> found;
        for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().filename().u8string()[0] == '.') {   // .DS_Store, .git
                if (it->is_directory(ec)) it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec)) continue;
            if (files.size() + found.size() >= kImportFiles) {
                err = "More than 2,000 files in " + p.filename().u8string() + ": pick a smaller folder";
                return false;
            }
            found.push_back(it->path().u8string());
        }
        std::sort(found.begin(), found.end());
        files.insert(files.end(), found.begin(), found.end());
    }
    return true;
}

// Why nothing could be read in a file: what its reading skipped, without the file's name before it.
static std::string unreadable(const Report& r, const std::string& file) {
    std::string why;
    for (auto& s : r.skipped) {
        const std::string w = s.compare(0, file.size() + 2, file + ": ") == 0 ? s.substr(file.size() + 2) : s;
        if (why.find(w) == std::string::npos) why += (why.empty() ? "" : "; ") + w;
    }
    return why.empty() ? "no performances, voices or Fseqs" : why;
}

static void append(Report* to, const Report& r) {
    if (!to) return;
    to->skipped.insert(to->skipped.end(), r.skipped.begin(), r.skipped.end());
    to->warnings.insert(to->warnings.end(), r.warnings.begin(), r.warnings.end());
}

int Library::import(const std::string& path, std::string& err, Report* report) {
    std::vector<uint8_t> bytes;
    if (!readFile(path, bytes)) {
        err = "Cannot read " + path;
        return -1;
    }
    const std::string file = fs::u8path(path).filename().u8string();
    Report own;
    const Bank b = readPatches(bytes, file, &own);
    if (b.empty()) {
        err = "Nothing FSVR can read in " + file + ": " + unreadable(own, file);
        return -1;
    }
    append(report, own);
    return add(fs::u8path(path).stem().u8string(), b.rewrapped.empty() ? bytes : b.rewrapped, err);
}

// Whether a file's performances find their voices and Fseqs in any bank it shares: no bulk dumped from a
// numbered internal memory, and every part naming an Int voice (bank byte 1), and an Int Fseq, plays the one
// that came right after its performance rather than one found by number.
static bool selfContained(const Bank& b) {
    auto numbered = [](const std::vector<Item>& v) { return std::any_of(v.begin(), v.end(), [](const Item& it) { return it.number >= 0; }); };
    if (numbered(b.perfs) || numbered(b.voices) || numbered(b.fseqs)) return false;
    for (auto& perf : b.perfs) {
        const uint8_t* d = perf.data();
        for (int p = 0; p < 4; ++p)
            if (d[192 + 52 * p + 1] == 1 && perf.partVoice[p] < 0) return false;
        if ((d[0x15] & 7) != 0 && !(d[0x16] & 1) && perf.fseq < 0) return false;
    }
    return true;
}

// A shared bank holds its files' items in the order readPatches binds them again: every voice and Fseq no
// performance claims first, then each performance with the voices and Fseq it claims right after it. Left
// where it was, a loose voice after another file's performance would join that performance.
std::vector<int> Library::importAll(const std::vector<std::string>& paths, std::string& err, Report* report, const std::string& name) {
    std::vector<std::string> made;   // the files written, the shared bank's first
    std::vector<uint8_t> loose, claimed;
    std::string folder = name, firstUnread, waiting;   // waiting: the file that left the supplement pending holds
    int unread = 0;
    Pending pending;   // an AMEM or ACED waiting for the bank or voice in the next file
    auto fail = [&](const std::string& why) { err += (err.empty() ? "" : "; ") + why; };
    auto write = [&](const std::string& name, const std::vector<uint8_t>& bytes, bool first) {
        std::string e;
        const int k = add(name, bytes, e);
        if (k < 0) fail(e);
        else made.insert(first ? made.begin() : made.end(), banks[(size_t)k].path);
    };
    for (auto& path : paths) {
        std::vector<uint8_t> bytes;
        if (!readFile(path, bytes)) { fail("cannot read " + path); continue; }
        const fs::path p = fs::u8path(path);
        const std::string file = p.filename().u8string();
        Report own;
        pending.folder = p.parent_path().u8string();
        const std::vector<uint8_t> aced = pending.aced, amem = pending.amem;
        const Bank b = readPatches(bytes, file, &own, &pending);
        const bool supplement = pending.aced != aced || pending.amem != amem;   // it left one for the next file
        if (supplement) waiting = file;
        if (b.empty() && !supplement) {
            if (!unread++) firstUnread = file + " (" + unreadable(own, file) + ")";
            continue;
        }
        append(report, own);
        if (b.empty()) continue;
        if (!selfContained(b)) { write(p.stem().u8string(), b.rewrapped.empty() ? bytes : b.rewrapped, false); continue; }
        if (folder.empty()) folder = p.parent_path().filename().u8string();
        std::vector<bool> voiceClaimed(b.voices.size()), fseqClaimed(b.fseqs.size());
        for (auto& perf : b.perfs) {
            for (int v : perf.partVoice)
                if (v >= 0) voiceClaimed[(size_t)v] = true;
            if (perf.fseq >= 0) fseqClaimed[(size_t)perf.fseq] = true;
        }
        for (size_t i = 0; i < b.voices.size(); ++i)
            if (!voiceClaimed[i]) loose.insert(loose.end(), b.voices[i].syx.begin(), b.voices[i].syx.end());
        for (size_t i = 0; i < b.fseqs.size(); ++i)
            if (!fseqClaimed[i]) loose.insert(loose.end(), b.fseqs[i].syx.begin(), b.fseqs[i].syx.end());
        for (auto& perf : b.perfs) {
            claimed.insert(claimed.end(), perf.syx.begin(), perf.syx.end());
            for (int v : perf.partVoice)
                if (v >= 0) claimed.insert(claimed.end(), b.voices[(size_t)v].syx.begin(), b.voices[(size_t)v].syx.end());
            if (perf.fseq >= 0) claimed.insert(claimed.end(), b.fseqs[(size_t)perf.fseq].syx.begin(), b.fseqs[(size_t)perf.fseq].syx.end());
        }
    }
    loose.insert(loose.end(), claimed.begin(), claimed.end());
    if (!loose.empty()) write(folder.empty() ? "Imported" : folder, loose, true);
    if (unread == 1) fail("nothing FSVR can read in " + firstUnread);
    else if (unread > 1) fail("nothing FSVR can read in " + std::to_string(unread) + " files, " + firstUnread + " first");
    if (!pending.unpaired().empty()) fail(waiting + ": " + pending.unpaired());
    std::vector<int> out;   // each add() rescanned, so the indexes are read once all are written
    for (auto& path : made)
        if (const int k = bankOf(path); k >= 0) out.push_back(k);
    return out;
}

bool Library::splice(int bank, std::vector<Splice> edits, std::string& err) {
    if (bank < 0 || bank >= (int)banks.size()) return false;
    if (banks[(size_t)bank].readonly()) { err = "\"" + banks[(size_t)bank].name + "\" is read only"; return false; }
    const std::string path = banks[(size_t)bank].path;
    std::vector<uint8_t> b;
    if (!readFile(path, b)) { err = "Cannot read " + path; return false; }
    std::sort(edits.begin(), edits.end(), [](const Splice& x, const Splice& y) { return x.at > y.at; });   // back to front
    for (auto& e : edits) {
        e.at = std::min(e.at, b.size());   // past the end: appended
        const size_t len = std::min(e.len, b.size() - e.at);
        b.erase(b.begin() + (long)e.at, b.begin() + (long)(e.at + len));
        b.insert(b.begin() + (long)e.at, e.bytes.begin(), e.bytes.end());
    }
    if (readPatches(b, "").empty()) return remove(bank, err);
    if (!writeFile(path, b)) { err = "Cannot write " + path; return false; }
    rescan(true);
    return true;
}

bool Library::remove(int bank, std::string& err) {
    if (bank < 0 || bank >= (int)banks.size()) return false;
    if (banks[(size_t)bank].readonly()) { err = "\"" + banks[(size_t)bank].name + "\" is read only"; return false; }
    const fs::path from = fs::u8path(banks[(size_t)bank].path), bin = fs::u8path(dir) / "Deleted";
    std::error_code ec;
    fs::create_directories(bin, ec);
    fs::path to = bin / from.filename();
    for (int k = 2; fs::exists(to, ec); ++k) to = bin / fs::u8path(from.stem().u8string() + " " + std::to_string(k) + ".syx");
    fs::rename(from, to, ec);
    if (ec) { err = "Cannot move " + from.u8string() + ": " + ec.message(); return false; }
    rescan(true);
    return true;
}

bool Library::rename(int bank, const std::string& name, std::string& err, std::string* path) {
    if (bank < 0 || bank >= (int)banks.size()) return false;
    if (banks[(size_t)bank].readonly()) { err = "\"" + banks[(size_t)bank].name + "\" is read only"; return false; }
    const fs::path from = fs::u8path(banks[(size_t)bank].path);
    std::error_code ec;
    // Only the case changing keeps the name as it is; anything else is made unique like a new bank's.
    const std::string stem = lower(from.stem().u8string()) == lower(name) ? name : uniqueName(name);
    const fs::path to = fs::u8path(dir) / fs::u8path(stem + ".syx");
    fs::rename(from, to, ec);
    if (ec) { err = "Cannot rename " + from.u8string() + ": " + ec.message(); return false; }
    if (path) *path = to.u8string();
    rescan(true);
    return true;
}

int Library::number(const std::vector<Ref>& list, int bank, int index) const {
    for (size_t k = 0; k < list.size(); ++k)
        if (list[k].bank == bank && list[k].index == index) return (int)k + 1;
    return 0;
}

} // namespace fsvr

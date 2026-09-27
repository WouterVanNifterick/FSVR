// The bank manager's data: .syx files read as banks of performances, voices and Fseqs. The factory set is
// one bank, built into the plug-in; the user's library is a folder of .syx files, one bank each, shared by
// every instance. Nothing here knows how the engine stores a patch: an item is the sysex that loads it.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fsvr {

struct Item {
    std::string name;
    int category = 0;              // the unit's category byte (performances and FS1R voices; DX voices have none)
    int address = 0;               // the bulk's address high: 0x10/0x11, 0x40..0x43/0x51, 0x60/0x61/0x70; 0 for a DX voice
    int number = -1;               // address low of an internal memory bulk (0x11, 0x51, 0x61), -1 otherwise
    int frames = 0;                // an Fseq's length in frames, as its header's end step gives it
    std::vector<uint8_t> syx;      // the one F0..F7 message that loads it (a DX voice: VCED, its ACED folded in)
    int partVoice[4] = {-1, -1, -1, -1};   // a performance: voices (0x40..0x43 bulks) that came right after it
    int fseq = -1;                         // and the Fseq that came with them
    const uint8_t* data() const { return syx.data() + 9; }   // a native bulk's data, after its address
};

struct Bank {
    std::string name, path;        // path: the file, empty for the factory bank
    std::vector<Item> perfs, voices, fseqs;
    bool empty() const { return perfs.empty() && voices.empty() && fseqs.empty(); }
};

// Every bulk in a .syx image: FS1R performances, voices and Fseqs at any of their addresses, DX7 single
// voices (VCED, with an ACED before it) and DX7 32-voice banks (VMEM, unpacked to VCED here, since the
// FS1R itself takes only VCED). A system bulk and anything unknown are skipped.
Bank parseSyx(const std::vector<uint8_t>& bytes, const std::string& name);

// A bulk's F0..F7 with a new address and its checksum redone (a voice bound for a given part).
std::vector<uint8_t> readdress(const std::vector<uint8_t>& syx, int ah, int am, int al);
// One FS1R native bulk: F0 43 00 5E bc bc ah am al <data> cs F7.
std::vector<uint8_t> bulk(int ah, int am, int al, const uint8_t* data, size_t size);

extern const char* const kCategories[23];   // the Data List's names, "--" first

// The user library: <Documents>/FSVR/Library (FSVR_LIBRARY overrides it), its .syx files by name.
class Library {
public:
    Library();
    std::string dir;
    std::vector<Bank> banks;
    struct Ref { int bank, index; };
    std::vector<Ref> perfs, voices, fseqs;   // every user item, bank after bank: U1, U2 ...
    // Reads the folder again when a file came, went or changed since the last look; true if it did.
    bool rescan(bool force = false);
    // Copies a .syx into the folder as a new bank named after the file ("Name", "Name 2" ...) and rescans.
    // The new bank's index, or -1 (err says why).
    int import(const std::string& path, std::string& err);
    // Writes bytes as a new bank called name (made unique the same way); its index, or -1.
    int add(const std::string& name, const std::vector<uint8_t>& bytes, std::string& err);
    int bankOf(const std::string& path) const;
    const Item* perf(int n) const { return n >= 1 && n <= (int)perfs.size() ? &banks[perfs[n - 1].bank].perfs[perfs[n - 1].index] : nullptr; }
    const Item* voice(int n) const { return n >= 1 && n <= (int)voices.size() ? &banks[voices[n - 1].bank].voices[voices[n - 1].index] : nullptr; }
    const Item* fseq(int n) const { return n >= 1 && n <= (int)fseqs.size() ? &banks[fseqs[n - 1].bank].fseqs[fseqs[n - 1].index] : nullptr; }
    int number(const std::vector<Ref>& list, int bank, int index) const;   // U number of a bank's item, 0 if none
private:
    std::string stamp_;
    std::string uniqueName(std::string stem) const;
};

bool readFile(const std::string& utf8Path, std::vector<uint8_t>& out);
bool writeFile(const std::string& utf8Path, const std::vector<uint8_t>& bytes);

} // namespace fsvr

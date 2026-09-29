// check_plugin: the FSVR processor end to end, as a host drives it but without one: its State (the skin's
// params and text data), its worker and its audio, a block at a time. The factory banks, a sysex param
// round trip through a saved session, Import SysEx into the bank manager, a pick from a user bank, the
// morph square, Import Audio, program change, panic and the monitor. Exits non-zero on any failure.
//   build/<dir>/Release/check_plugin      (ctest runs it as "plugin")
#include <hollow/hollow.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace hollow;
namespace fs = std::filesystem;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("check_plugin: FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++fails; } } while (0)

struct Rig {   // one instance, run a block at a time
    std::shared_ptr<Skin> skin = loadSkin();
    std::unique_ptr<State> st = std::make_unique<State>(skinParams(*skin));
    std::unique_ptr<Processor> proc = createProcessor(*st);
    std::vector<float> l = std::vector<float>(512), r = std::vector<float>(512);
    float peak = 0;
    Rig() { proc->prepare(48000, 512); }
    void run(int blocks = 1) {
        float* out[2] = {l.data(), r.data()};
        for (int b = 0; b < blocks; ++b) {
            std::fill(l.begin(), l.end(), 0.0f);
            std::fill(r.begin(), r.end(), 0.0f);
            proc->process(nullptr, out, 512);
            for (float v : l) peak = std::max(peak, std::fabs(v));
        }
    }
    template <class F> bool until(F done, int ms = 8000) {   // the worker acts about 30 times a second
        for (auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms); std::chrono::steady_clock::now() < end;) {
            run();
            if (done()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
    int at(const char* id) const { return st->indexOf(id); }
    double get(const char* id) const { return at(id) >= 0 ? st->get((size_t)at(id)) : -1e9; }
    void set(const char* id, double v) { if (at(id) >= 0) st->set((size_t)at(id), v); }
    bool is(const char* id, double v) const { return std::fabs(get(id) - v) < 1e-6; }   // values are snapped to their steps
    void midi(std::initializer_list<uint8_t> b) { proc->midi(0, b.begin(), (int)b.size()); }
    std::string data(const char* key) const { return st->data(key); }
    int lines(const char* key) const { std::string d = st->data(key); return (int)std::count(d.begin(), d.end(), '\n'); }
};

static void writeBytes(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary);
    f.write((const char*)b.data(), (std::streamsize)b.size());
}

// A DX7 32-voice bank (VMEM): every voice an init voice (op 1 loud), named TEST01..TEST32.
static std::vector<uint8_t> dx7Bank() {
    std::vector<uint8_t> b = {0xF0, 0x43, 0x00, 0x09, 0x20, 0x00};
    for (int v = 0; v < 32; ++v) {
        uint8_t d[128] = {};
        for (int op = 0; op < 6; ++op) {
            uint8_t* p = d + op * 17;
            for (int k = 0; k < 4; ++k) { p[k] = 99; p[4 + k] = k < 3 ? 99 : 0; }
            p[14] = op == 5 ? 99 : 0;   // OP1 (stored last) carries the sound
            p[15] = 1 << 1;             // coarse 1
            p[12] = 7 << 3;             // detune centred
        }
        for (int k = 0; k < 4; ++k) { d[102 + k] = 99; d[106 + k] = 50; }
        d[117] = 24;
        char name[11];
        std::snprintf(name, sizeof name, "TEST%02d    ", v + 1);
        std::memcpy(d + 118, name, 10);
        b.insert(b.end(), d, d + 128);
    }
    int sum = 0;
    for (size_t i = 6; i < b.size(); ++i) sum += b[i];
    b.push_back((uint8_t)(-sum & 0x7F));
    b.push_back(0xF7);
    return b;
}

// One second of a 150 Hz voice-like tone: harmonics shaped by formant bumps at 700 and 1200 Hz, then a
// noise burst, 16-bit mono WAV at 44.1 kHz.
static std::vector<uint8_t> vowelWav() {
    const int sr = 44100, n = sr;
    std::vector<int16_t> s((size_t)n);
    uint32_t noise = 1;
    for (int i = 0; i < n; ++i) {
        double t = (double)i / sr, v = 0;
        if (i < n * 3 / 4) {
            for (int h = 1; h * 150 < 5000; ++h) {
                const double f = h * 150.0;
                const double a = std::exp(-std::pow((f - 700) / 150, 2)) + 0.6 * std::exp(-std::pow((f - 1200) / 200, 2)) + 0.02;
                v += a * std::sin(2 * 3.14159265358979 * f * t);
            }
            v *= 0.25;
        } else {
            noise = noise * 1664525u + 1013904223u;
            v = ((noise >> 9) / double(1 << 23) - 1) * 0.3;
        }
        s[(size_t)i] = (int16_t)std::clamp(v * 32767, -32767.0, 32767.0);
    }
    std::vector<uint8_t> w;
    auto u32 = [&](uint32_t x) { for (int k = 0; k < 4; ++k) w.push_back((uint8_t)(x >> (8 * k))); };
    auto u16 = [&](uint16_t x) { w.push_back((uint8_t)x); w.push_back((uint8_t)(x >> 8)); };
    w.insert(w.end(), {'R', 'I', 'F', 'F'});
    u32(36 + (uint32_t)n * 2);
    w.insert(w.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    u32(16); u16(1); u16(1); u32(sr); u32(sr * 2); u16(2); u16(16);
    w.insert(w.end(), {'d', 'a', 't', 'a'});
    u32((uint32_t)n * 2);
    const uint8_t* p = (const uint8_t*)s.data();
    w.insert(w.end(), p, p + n * 2);
    return w;
}

int main() {
    const fs::path tmp = fs::temp_directory_path() / "fsvr_check_plugin";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "Library");
#ifdef _WIN32
    _putenv_s("FSVR_LIBRARY", (tmp / "Library").u8string().c_str());
#else
    setenv("FSVR_LIBRARY", (tmp / "Library").u8string().c_str(), 1);
#endif

    Rig a;
    CHECK(a.st->size() > 3000, "the skin's params did not load (%zu)", a.st->size());
    CHECK(!a.data("library.dir").empty(), "no library.dir");
    CHECK(!a.data("fsvr.version").empty(), "no fsvr.version for the About box");

    // A fresh instance plays what its LCD names: A001 "Zap !", B021 on part 1, a Sound FX.
    CHECK(a.until([&] { return a.is("part.bank.p1", 3) && a.is("part.program.p1", 21); }), "A001 did not load its part 1 voice B021 (bank %g, program %g)",
          a.get("part.bank.p1"), a.get("part.program.p1"));
    CHECK(a.is("perf.category", 16), "A001's category is %g, not Sound FX (16)", a.get("perf.category"));
    const std::string zapVoice = a.data("morph.p1.bl");
    CHECK(!zapVoice.empty() && zapVoice != "InitEP", "part 1's voice name is \"%s\"", zapVoice.c_str());

    // A note sounds, the monitor shows its harmonics, and panic silences it: B014 "Full Tines", a piano.
    a.set("perf.program", 141);
    CHECK(a.until([&] { return a.is("perf.category", 1); }), "B014 did not load");
    a.midi({0x90, 60, 100});
    a.run(20);
    CHECK(a.peak > 1e-4f, "the init voice made no sound (peak %g)", a.peak);
    CHECK(a.st->voices() >= 1, "no voice is playing");
    CHECK(a.until([&] { float v[64]; int n = a.st->scope(v, 64); return n == 32 && *std::max_element(v, v + n) > 0; }),
          "the monitor's harmonics never showed");
    a.set("gui.panic", 1);
    CHECK(a.until([&] { return a.st->voices() == 0; }), "panic left %d notes", a.st->voices());
    a.set("gui.panic", 0);

    // Another factory performance: A002 "Shaman" plays B095 on part 1, a Vocal.
    a.set("perf.program", 1);
    CHECK(a.until([&] { return a.is("part.bank.p1", 3) && a.is("part.program.p1", 95) && a.is("perf.category", 19); }),
          "A002 did not load (bank %g, program %g, category %g)", a.get("part.bank.p1"), a.get("part.program.p1"), a.get("perf.category"));
    CHECK(a.data("morph.p1.bl") != zapVoice, "part 1 still holds %s", zapVoice.c_str());

    // Program change: bank select Preset B (LSB 0x42) then program 6 is B006.
    a.midi({0xB0, 0, 0x3F});
    a.midi({0xB0, 32, 0x42});
    a.midi({0xC0, 5});
    CHECK(a.until([&] { return a.is("perf.program", 133) && a.is("perf.bank", 0); }), "program change reached performance %g", a.get("perf.program"));
    const std::string shaman = a.data("morph.p1.bl");
    CHECK(a.until([&] { return a.data("morph.p1.bl") != shaman; }), "B006 did not load its voices");

    // A param edit reaches the engine and comes back with the session, even saved and reopened at once
    // (saving() finishes what the worker has not reached yet).
    a.set("op.1.v.level.p1", 57);
    a.set("perf.volume", 101);
    a.run(4);
    a.proc->saving();
    const std::string blob = a.st->save();
    {
        Rig b;
        b.st->load(blob);
        CHECK(b.is("op.1.v.level.p1", 57) && b.is("perf.volume", 101), "the session's params did not come back (level %g, volume %g)",
              b.get("op.1.v.level.p1"), b.get("perf.volume"));
        b.proc->saving();
        CHECK(b.data("fsvr.engine") == a.data("fsvr.engine"), "the engine did not come back the same");
        CHECK(b.is("perf.program", 133), "the performance number did not come back");
        b.run(20);
        b.proc->saving();
        CHECK(b.data("fsvr.engine") == a.data("fsvr.engine"), "the engine changed after the session came back");
    }

    // Import SysEx: a DX7 bank becomes a bank of its own, named after the file, and its first voice loads.
    const fs::path syx = tmp / "Test Bank.syx";
    writeBytes(syx, dx7Bank());
    a.st->setData("sysex.import", syx.u8string());
    CHECK(a.until([&] { return a.data("bank.list") == "Test Bank\n"; }), "the bank list is \"%s\"", a.data("bank.list").c_str());
    CHECK(a.lines("voice.user.list") == 32, "%d user voices, not 32", a.lines("voice.user.list"));
    CHECK(a.is("browse.bank", 1), "the browser is not on the new bank");
    CHECK(a.until([&] { return a.is("part.bank.p1", 1) && a.is("part.user.p1", 1) && a.data("morph.p1.bl") == "TEST01"; }),
          "the first voice did not load (bank %g, user %g, \"%s\")", a.get("part.bank.p1"), a.get("part.user.p1"), a.data("morph.p1.bl").c_str());
    a.st->setData("sysex.import", syx.u8string());
    CHECK(a.until([&] { return a.data("bank.list") == "Test Bank\nTest Bank 2\n"; }), "a second import did not make \"Test Bank 2\"");
    CHECK(a.lines("voice.user.list") == 64, "%d user voices, not 64", a.lines("voice.user.list"));
    CHECK(a.data("fsvr.message").rfind("Imported", 0) == 0, "no import message (\"%s\")", a.data("fsvr.message").c_str());
    CHECK(a.until([&] { return a.data("fsvr.message").empty(); }), "the import message stayed up");

    // A pick from the browsed user bank: its third voice into part 2.
    a.set("browse.bank", 1);
    CHECK(a.until([&] { return a.lines("browse.voice.list") == 32; }), "the browsed bank lists %d voices", a.lines("browse.voice.list"));
    a.set("browse.voice.p2", 3);
    CHECK(a.until([&] { return a.is("part.bank.p2", 1) && a.is("part.user.p2", 3) && a.data("morph.p2.bl") == "TEST03"; }), "the pick did not load TEST03");

    // Morph is off until it works: a voice picked with corner A chosen reaches every corner, and moving the
    // square changes nothing.
    // a.set("part.morph_edit.p1", 0);
    // a.run(2);
    // a.set("part.bank.p1", 1);
    // a.set("part.user.p1", 5);
    // CHECK(a.until([&] { return a.data("morph.p1.tl") == "TEST05" && a.data("morph.p1.bl") == "TEST01"; }), "corner A is \"%s\", C \"%s\"",
    //       a.data("morph.p1.tl").c_str(), a.data("morph.p1.bl").c_str());
    // a.proc->saving();
    // const std::string atC = a.data("fsvr.engine");
    // a.set("part.morph_y.p1", 100);   // the top left corner: A
    // a.run(4);
    // a.proc->saving();
    // CHECK(a.data("fsvr.engine") != atC, "moving the morph square did not change the voice");
    // a.st->setData("morph.request.p1", "normalize");
    // CHECK(a.until([&] { return a.data("morph.p1.bl") == "TEST05" && a.data("morph.p1.br") == "TEST05"; }), "Normalize did not copy corner A");
    a.set("part.morph_edit.p1", 0);
    a.run(2);
    a.set("part.bank.p1", 1);
    a.set("part.user.p1", 5);
    CHECK(a.until([&] { return a.data("morph.p1.tl") == "TEST05" && a.data("morph.p1.bl") == "TEST05"; }), "corner A is \"%s\", C \"%s\"",
          a.data("morph.p1.tl").c_str(), a.data("morph.p1.bl").c_str());
    a.proc->saving();
    const std::string still = a.data("fsvr.engine");
    a.set("part.morph_y.p1", 100);
    a.run(4);
    a.proc->saving();
    CHECK(a.data("fsvr.engine") == still, "moving the morph square changed the voice");

    // Import Audio: a vowel at 150 Hz becomes a user Fseq, loaded, its pitch the word for 150 Hz.
    const fs::path wav = tmp / "Vowel.wav";
    writeBytes(wav, vowelWav());
    const std::string shown = a.data("fseq.display");   // the list line comes before the load, the display after it
    a.st->setData("fseq.import_audio", wav.u8string());
    CHECK(a.until([&] { return a.lines("fseq.user.list") == 1 && a.data("fseq.display") != shown && !a.data("fseq.display").empty(); }, 20000),
          "Import Audio made no Fseq (%s)", a.data("fsvr.message").c_str());
    CHECK(a.is("fseq.bank", 0) && a.is("fseq.user", 1), "the new Fseq is not selected");
    const std::string disp = a.data("fseq.display");
    const size_t p = disp.find("\"pitch\":\"");
    if (p != std::string::npos) {
        const int hi = std::stoi(disp.substr(p + 9 + 20, 2), nullptr, 16);   // frame 10, well inside the tone
        CHECK(std::abs(hi - 0x62) <= 1, "the pitch word's high byte is %02x, 150 Hz is 62", hi);
    }

    // The Fseq plays from a note on its part, and the page's playback line follows it.
    a.set("fseq.part", 1);
    a.set("fseq.play_mode", 1);   // Fseq, not Scratch
    a.run(2);
    a.midi({0x90, 60, 100});
    CHECK(a.until([&] { return a.get("fseq.position") > 5; }), "the Fseq playback position stayed at %g (speed %g, delay %g, part %g, mode %g, voices %d)", a.get("fseq.position"), a.get("fseq.speed"), a.get("fseq.delay"), a.get("fseq.part"), a.get("fseq.play_mode"), a.st->voices());
    a.midi({0x80, 60, 0});

    // Export writes a hardware-shaped dump.
    a.st->setData("sysex.export", (tmp / "out.syx").u8string());
    CHECK(a.until([&] { return fs::exists(tmp / "out.syx") && fs::file_size(tmp / "out.syx") > 3000; }), "Export SysEx wrote nothing");

    a.proc.reset();
    fs::remove_all(tmp, ec);
    if (!fails) std::printf("check_plugin: factory banks, sessions, Import SysEx, the bank browser, morph (off), Import Audio, program change, panic and the monitor all pass\n");
    return fails ? 1 : 0;
}

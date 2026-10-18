// wavetrace - one voice's operators, sample by sample, at several notes: what a change to how operators run
// actually does. Ours.
//
//   wavetrace [-v voice.syx [-p n] | -r eprom.bin -p n] [-set op:byte=v | -set byte=v ...] [-n 36,48,60,72]
//             [-vel 100] [-t 0.5] [-c 4] [-png trace.png]
//
// Each note plays on a fresh engine, part 1 alone, through the tone generator only: no filter, no pan, no
// effects. At -t seconds after key-on it prints every sounding operator's frequency, its phase increment (in
// 2^-23 of a cycle a sample, the phase generator's own unit, fs1r/cal.h), its ratio to the channel's
// fundamental, its phase and its lead, the phase less frequency x time: what the operator gained or lost against
// an oscillator started at key-on and left alone. A key-synced operator at a steady pitch leads by 0 at every
// note; a frequency EG, an LFO or a note-dependent anything shows as a lead that moves with the note.
//
// -png draws -c periods of the fundamental from -t on, one row a note, the channel's output then each sounding
// operator's, each scaled to its own peak. Columns are the fundamental's phase, so rows that agree look alike.
// -set edits the voice before it plays: "7:8=50" is operator 7's byte 8 (the Data List's operator offsets, the
// voiced block 0-34 then the unvoiced 35-61), "0x2C=16" a common byte. Values are decimal or 0x hex.
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "engine/synth.h"

// A PNG of 8-bit RGB, stored rather than compressed: deflate's type-0 blocks need no compressor.
static void write_png(const char* path, int w, int h, const std::vector<uint8_t>& rgb) {
    uint32_t crcTab[256];
    for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcTab[n] = c; }
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    auto be = [](std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s)); };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        be(out, (uint32_t)data.size());
        std::vector<uint8_t> td(type, type + 4); td.insert(td.end(), data.begin(), data.end());
        uint32_t c = 0xFFFFFFFFu; for (uint8_t b : td) c = crcTab[(c ^ b) & 0xFF] ^ (c >> 8);
        out.insert(out.end(), td.begin(), td.end()); be(out, c ^ 0xFFFFFFFFu);
    };
    std::vector<uint8_t> hdr; be(hdr, w); be(hdr, h); hdr.insert(hdr.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", hdr);
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb.begin() + y * w * 3, rgb.begin() + (y + 1) * w * 3); }
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t at = 0; at < raw.size(); at += 65535) {
        const uint16_t n = (uint16_t)std::min<size_t>(65535, raw.size() - at);
        z.push_back(at + n == raw.size()); z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
    }
    uint32_t a = 1, b = 0; for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    be(z, b << 16 | a);
    chunk("IDAT", z); chunk("IEND", {});
    FILE* f = fopen(path, "wb"); if (f) { fwrite(out.data(), 1, out.size(), f); fclose(f); }
}

int main(int argc, char** argv) {
    const char *syx = nullptr, *romPath = nullptr, *png = nullptr;
    int pick = 0, vel = 100, periods = 4; double at = 0.5;
    std::vector<int> notes = {36, 48, 60, 72};
    std::vector<std::string> sets;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v" && i + 1 < argc) syx = argv[++i];
        else if (a == "-r" && i + 1 < argc) romPath = argv[++i];
        else if (a == "-p" && i + 1 < argc) pick = atoi(argv[++i]);
        else if (a == "-set" && i + 1 < argc) sets.push_back(argv[++i]);
        else if (a == "-n" && i + 1 < argc) { notes.clear(); for (char* s = argv[++i]; *s; ) { notes.push_back((int)strtol(s, &s, 10)); if (*s) s++; } }
        else if (a == "-vel" && i + 1 < argc) vel = atoi(argv[++i]);
        else if (a == "-t" && i + 1 < argc) at = atof(argv[++i]);
        else if (a == "-c" && i + 1 < argc) periods = atoi(argv[++i]);
        else if (a == "-png" && i + 1 < argc) png = argv[++i];
        else { printf("usage: wavetrace [-v voice.syx [-p n] | -r eprom.bin -p n] [-set op:byte=v | -set byte=v ...]\n"
                      "                 [-n 36,48,60,72] [-vel 100] [-t 0.5] [-c 4] [-png trace.png]\n"); return 1; }
    }
    init_tables();
    // The voice, once: from a dump, the EPROM or the Init performance's, then the edits.
    auto proto = std::make_unique<Synth>();
    Rom rom;
    if (romPath) { if (!load_rom(rom, romPath)) { printf("cannot read %s\n", romPath); return 1; } rom_voice(rom, pick, proto->perf.part[0].voice); }
    if (syx) {
        FILE* f = fopen(syx, "rb"); if (!f) { printf("cannot open %s\n", syx); return 1; }
        std::vector<uint8_t> d(1 << 20); d.resize(fread(d.data(), 1, d.size(), f)); fclose(f);
        std::lock_guard<std::mutex> lk(proto->mtx);
        if (!load_sysex(*proto, nullptr, d.data(), d.size(), pick, 0)) { printf("no voice #%d in %s\n", pick, syx); return 1; }
    }
    Voice voice = proto->perf.part[0].voice;
    for (auto& s : sets) {
        int op = 0, byte, v; const char* e = s.c_str();
        if (strchr(e, ':')) { op = (int)strtol(e, (char**)&e, 0); e++; }
        byte = (int)strtol(e, (char**)&e, 0); if (*e != '=') { printf("bad -set %s\n", s.c_str()); return 1; }
        v = (int)strtol(e + 1, nullptr, 0);
        const int addr = op ? 112 + (op - 1) * 62 + byte : byte;
        if (op < 0 || op > 8 || addr < 0 || addr >= 608 || (op && byte >= 62)) { printf("bad -set %s\n", s.c_str()); return 1; }
        voice.raw[addr] = (uint8_t)v;
    }
    decode_voice(voice);
    printf("voice \"%s\" algorithm %d, %d samples a second, at %.3f s\n", voice.name, voice.alg + 1, SR, at);

    struct Row { int note; double f0, off; std::vector<double> t[9]; bool sounds[9]; };
    std::vector<Row> rows;
    for (int note : notes) {
        auto S = std::make_unique<Synth>(); Part& pt = S->perf.part[0];
        pt.p[1] = 2; pt.p[4] = 0x10; pt.voice = voice;
        srand(1); S->midi_in(0x90, note, vel);
        Chan* c = nullptr; for (auto& x : S->ch) if (x.active && x.part == 0) { c = &x; break; }
        if (!c) { printf("note %d: no channel\n", note); continue; }
        const long start = (long)(at * SR);
        float l, r; for (long k = 0; k < start; k++) S->render(&l, &r, 1);
        Row row; row.note = note; row.f0 = c->f0;
        const double cycles = c->f0 * start / SR; row.off = (ceil(cycles) - cycles) * SR / c->f0;   // to the next cycle of f0
        double phase[8], fop[8], word[8]; uint32_t inc[8];
        for (int o = 0; o < 8; o++) phase[o] = c->op[o].phase / 4294967296.0, fop[o] = c->op[o].fop, inc[o] = c->op[o].inc, word[o] = c->op[o].word;
        const long len = (long)((periods + 1) * SR / c->f0) + 2;
        for (long k = 0; k < len; k++) {
            S->render(&l, &r, 1);
            row.t[0].push_back(c->last);
            for (int o = 0; o < 8; o++) row.t[o + 1].push_back(c->op[o].out);
        }
        // An operator sounds if it comes within 60 dB of the loudest: a level-0 operator sits at -96 dB.
        double peak[9] = {}, loudest = 0;
        for (int j = 0; j < 9; j++) { for (double v : row.t[j]) peak[j] = std::max(peak[j], fabs(v)); if (j) loudest = std::max(loudest, peak[j]); }
        for (int j = 0; j < 9; j++) row.sounds[j] = j == 0 || (peak[j] > 0 && peak[j] > loudest * 1e-3);
        printf("\nnote %d  f0 %.3f Hz\n  op  form  word        freq Hz     increment  ratio   phase   lead\n", note, c->f0);
        for (int o = 0; o < 8; o++) {
            if (!row.sounds[o + 1]) continue;
            double lead = phase[o] - fop[o] * start / SR; lead -= floor(lead + 0.5);   // -0.5 .. 0.5 of a turn
            printf("  %d   %d     %10.3f  %10.3f  %9u  %7.4f  %.4f  %+.4f%s\n", o + 1, voice.v[o].form, word[o], fop[o], inc[o], fop[o] / c->f0, phase[o], lead, voice.v[o].keysync ? "" : "   (key sync off)");
        }
        rows.push_back(std::move(row));
    }
    if (!png || rows.empty()) return 0;
    // One panel a trace: the channel, then each operator that sounds at any note, the same panels on every row.
    bool any[9] = {true}; for (auto& r : rows) for (int j = 1; j < 9; j++) any[j] = any[j] || r.sounds[j];
    std::vector<int> panels; for (int j = 0; j < 9; j++) if (any[j]) panels.push_back(j);
    const int W = 960, PH = 72, H = (int)(rows.size() * panels.size()) * PH;
    std::vector<uint8_t> img((size_t)W * H * 3, 0);
    auto put = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) { if (x >= 0 && x < W && y >= 0 && y < H) { uint8_t* p = &img[((size_t)y * W + x) * 3]; p[0] = r; p[1] = g; p[2] = b; } };
    static const uint8_t COL[9][3] = {{235, 235, 235}, {90, 150, 255}, {110, 200, 110}, {200, 200, 80}, {230, 170, 60}, {230, 120, 60}, {220, 80, 80}, {210, 110, 210}, {150, 120, 255}};
    printf("\n%s: %zu rows (notes", png, rows.size());
    for (auto& r : rows) printf(" %d", r.note);
    printf("), each the panels");
    for (int j : panels) printf(j ? " op%d" : " out", j);
    printf(", %d periods of f0\n", periods);
    int y0 = 0;
    for (auto& r : rows) for (int j : panels) {
        const std::vector<double>& v = r.t[j];
        double m = 0; for (double x : v) m = std::max(m, fabs(x));
        const int mid = y0 + PH / 2;
        for (int x = 0; x < W; x++) put(x, y0, 60, 60, 60), put(x, mid, 40, 40, 40);
        for (int p = 1; p < periods; p++) for (int y = y0 + 1; y < y0 + PH; y += 3) put(p * W / periods, y, 50, 50, 50);
        int last = -1;
        for (int x = 0; x < W && m > 0; x++) {
            const double k = r.off + (double)x / W * periods * SR / r.f0; const int i = (int)k;
            if (i + 1 >= (int)v.size()) break;
            const double s = v[i] + (v[i + 1] - v[i]) * (k - i);
            const int y = mid - (int)lround(s / m * (PH / 2 - 4));
            const int a = last < 0 ? y : last;
            for (int yy = std::min(a, y); yy <= std::max(a, y); yy++) put(x, yy, COL[j][0], COL[j][1], COL[j][2]);
            last = y;
        }
        y0 += PH;
    }
    write_png(png, W, H, img);
    return 0;
}

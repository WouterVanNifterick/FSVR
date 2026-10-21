// The fseq_edit kind: an editor for a formant sequence, the FS1R's Fseq (hollow/docs/skin-format.md).
// The document is the Fseq bulk the processor publishes under "key", base64: a 32-byte header, then 50
// bytes a frame (the pitch, eight voiced formant frequencies and levels, eight unvoiced). It is drawn as
// ribbons in lanes over time, edited with tools and range operations, with undo, a clipboard, a frame
// selection, zoom on the time axis, the loop and start markers dragged as the params they are, and a
// playhead run on the frame period between the position's updates. Edits go back as text data "edit",
// commands arrive under "cmd". Everything the editor keeps lives in Gui::kindShared, so it survives a
// page switch and a rebuild and goes with the window.
#include "core.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace hollow {

namespace {

using Clock = std::chrono::steady_clock;

// ---- base64, the document's encoding -----------------------------------------------------------

const char* const kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64enc(const uint8_t* d, size_t n) {
    std::string s;
    s.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        s += kB64[v >> 18 & 63];
        s += kB64[v >> 12 & 63];
        s += i + 1 < n ? kB64[v >> 6 & 63] : '=';
        s += i + 2 < n ? kB64[v & 63] : '=';
    }
    return s;
}

std::vector<uint8_t> b64dec(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size() / 4 * 3);
    unsigned v = 0;
    int bits = 0;
    for (char ch : s) {
        int k;
        if (ch >= 'A' && ch <= 'Z') k = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') k = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') k = ch - '0' + 52;
        else if (ch == '+') k = 62;
        else if (ch == '/') k = 63;
        else continue;
        v = v << 6 | (unsigned)k;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)(v >> bits & 255)); }
    }
    return out;
}

uint64_t hashOf(const std::vector<uint8_t>& d) {   // FNV-1a, to tell the processor's echo of an edit from a new document
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : d) { h ^= b; h *= 1099511628211ull; }
    return h;
}

// ---- the document ------------------------------------------------------------------------------

constexpr int kHeader = 32, kFrame = 50, kPitch = 32, kTracks = 33, kMaxWord = 32766, kUndoDepth = 32;
constexpr double kWordsPerSemitone = 1024.0 / 12;   // a word is 1/1024 octave

struct Doc {
    std::vector<uint8_t> bytes;
    int frames() const { return bytes.size() >= kHeader ? (int)((bytes.size() - kHeader) / kFrame) : 0; }
    uint8_t* frame(int i) { return bytes.data() + kHeader + (size_t)i * kFrame; }
    const uint8_t* frame(int i) const { return bytes.data() + kHeader + (size_t)i * kFrame; }
    int word(int at) const { return bytes.size() > (size_t)at + 1 ? bytes[(size_t)at] << 7 | bytes[(size_t)at + 1] : 0; }
};

// Tracks: 0..7 the voiced formant frequencies, 8..15 their levels, 16..23 the unvoiced frequencies,
// 24..31 their levels, 32 the pitch. A level is an attenuation, 0 loudest, 127 silent.
bool isLevel(int t) { return t < kPitch && (t & 8); }
int opOf(int t) { return t & 7; }
int layerOf(int t) { return t >= 16 && t < kPitch ? 1 : 0; }
int levelOf(int t) { return t == kPitch || isLevel(t) ? -1 : t + 8; }   // a frequency track's level
int freqOf(int t) { return isLevel(t) ? t - 8 : t; }

double readValue(const uint8_t* f, int t) {
    if (t == kPitch) return f[0] * 256 + (f[1] & 0x7F) * 2;
    const int op = opOf(t);
    if (isLevel(t)) return f[(layerOf(t) ? 0x2A : 0x12) + op] & 0x7F;
    return f[(layerOf(t) ? 0x1A : 2) + op] * 256 + (f[(layerOf(t) ? 0x22 : 0x0A) + op] & 0x7F) * 2;
}

void writeValue(uint8_t* f, int t, double v) {
    if (t != kPitch && isLevel(t)) { f[(layerOf(t) ? 0x2A : 0x12) + opOf(t)] = (uint8_t)std::clamp((int)std::lround(v), 0, 127); return; }
    const int w = std::clamp((int)std::lround(v), 0, kMaxWord);
    const int hi = t == kPitch ? 0 : (layerOf(t) ? 0x1A : 2) + opOf(t), lo = t == kPitch ? 1 : (layerOf(t) ? 0x22 : 0x0A) + opOf(t);
    f[hi] = (uint8_t)(w >> 8);
    f[lo] = (uint8_t)((w & 0xFF) >> 1);
}

std::vector<double> readTrack(const Doc& d, int t) {
    std::vector<double> v((size_t)d.frames());
    for (int i = 0; i < d.frames(); ++i) v[(size_t)i] = readValue(d.frame(i), t);
    return v;
}

void writeTrack(Doc& d, int t, const std::vector<double>& v) {
    for (int i = 0; i < d.frames() && i < (int)v.size(); ++i) writeValue(d.frame(i), t, v[(size_t)i]);
}

double sampleAt(const std::vector<double>& v, double s) {   // a track between its frames, held at the ends
    if (v.empty()) return 0;
    s = std::clamp(s, 0.0, (double)(v.size() - 1));
    const size_t i = (size_t)std::floor(s);
    const double f = s - (double)i;
    return i + 1 < v.size() ? v[i] * (1 - f) + v[i + 1] * f : v[i];
}

double hzOf(double w) { return 440 * std::pow(2.0, (w - 26861) / 1024); }
double wordOfHz(double hz) { return 26861 + 1024 * std::log2(std::max(hz, 1e-3) / 440); }
double midiOf(double w) { return 69 + (w - 26861) / kWordsPerSemitone; }
double wordOfMidi(double m) { return 26861 + (m - 69) * kWordsPerSemitone; }
double ampOf(double level, double levelDb) { return std::pow(10.0, -level * levelDb / 20); }
double levelOfAmp(double a, double levelDb) { return a <= 1e-9 ? 127 : std::clamp(-20 * std::log10(a) / levelDb, 0.0, 127.0); }

// How opaque a track is drawn at a level (an attenuation, so a high level is a quiet frame): the square
// root of its amplitude, so it halves every 12 dB down and loud and quiet stay apart all the way.
double traceAlpha(double level, double levelDb) { return std::sqrt(ampOf(level, levelDb)); }

// ---- what the editor keeps -----------------------------------------------------------------------

enum Tool { Select, Loop, Draw, Line, Move, Scale, Amplify, Silence, Warp, Stretch };
const char* const kToolNames[10] = {"select", "loop", "draw", "line", "move", "scale", "amplify", "silence", "warp", "stretch"};

struct Clip {
    int frames = 0;
    std::vector<std::pair<int, std::vector<double>>> tracks;
};

struct Gesture {
    bool active = false, moved = false;
    int tool = Select, target = -1, lane = -1, marker = -1, edge = -1, anchor = 0;
    double f0 = 0, c0 = 0, v0 = 0;   // the press: frame, curve, and the value under it
    int from = 0, to = 0;            // the span the tool works on
    std::vector<int> tracks;
    std::vector<std::pair<double, double>> path;   // draw: the points so far, (frame, value)
    Doc src;                         // the document at the press: every move starts from it
};

struct Shared : KindState {
    Doc doc;
    std::string docSeen;
    uint64_t sent[8] = {};
    int sentAt = 0;
    std::vector<std::vector<uint8_t>> undo, redo;
    std::string lastPath;
    Clock::time_point lastPush;
    int selFrom = -1, selTo = -1;    // inclusive frames, -1 none
    int selWritten[2] = {-1, -1};    // the sel params as the widget last wrote them
    Clip clip;
    double x0 = 0, x1 = 0;           // the frames in view; x1 <= x0 is the fit
    int dirtyLo = INT_MAX, dirtyHi = -1, sentLo = INT_MAX, sentHi = -1;   // frames edited since the last flush, and the unconsumed edit's
    std::vector<uint32_t> cache;
    std::string cacheKey;
    Clock::time_point lastGesture;
    bool fast = false;
    Gesture gest;
    Doc preview;
    bool previewOn = false;
    unsigned docVersion = 0, previewVersion = 0, previewSent = 0;
    int hoverTrack = -1, hoverLane = -1;
    double lastPos = -1;
    Clock::time_point lastPosAt;
    int drawnPlayX = INT_MIN;
    std::string paramSig;            // the view's params at the last tick, to repaint when one moves
};

// ---- the widget's fields -------------------------------------------------------------------------

struct Fields {
    int position = -1, loopStart = -1, loopEnd = -1, start = -1, end = -1, tool = -1, layout = -1, freqScale = -1, levelScale = -1;
    int showV = -1, showU = -1, showPitch = -1, showOp[8] = {-1, -1, -1, -1, -1, -1, -1, -1}, selFrom = -1, selLen = -1;
    int opV[8], opU[8], opTrack[8];   // each operator's Fseq switches and the track it follows, -1 when not given
    int selOp = -1, selLayer = 0;     // the app's selected operator, 0..7 (-1 none), and layer, 1 unvoiced
    std::string editKey, cmdKey, rateKey;
    double levelDb = 0.752574;
    uint32_t screen = 0, gutter = 0, grid = 0, keyWhite = 0, keyBlack = 0, keyC = 0, selection = 0, selectionEdge = 0;
    uint32_t colLoopStart = 0, colLoopEnd = 0, colStart = 0, colEnd = 0, cursor = 0, glow = 0, glowU = 0, hover = 0, line2 = 0, lineOff = 0;
    int font = -1;
};

std::string numbered(std::string s, int n) {   // a template with {n} as 1..8
    for (size_t at; (at = s.find("{n}")) != std::string::npos;) s.replace(at, 3, std::to_string(n));
    return s;
}

Fields fieldsOf(Gui& g, const Hit& h) {
    const Widget& w = g.wid(h);
    const Json& j = w.json;
    Fields f;
    auto p = [&](const char* key) { return j.has(key) ? g.paramOf(h, j[key].str()) : -1; };
    auto perOp = [&](const char* key, int (&out)[8]) {
        for (int n = 1; n <= 8; ++n) out[n - 1] = j.has(key) ? g.paramOf(h, numbered(j[key].str(), n)) : -1;
    };
    f.position = p("position"), f.start = p("start"), f.end = p("end"), f.tool = p("tool"), f.layout = p("layout");
    f.freqScale = p("freqScale"), f.levelScale = p("levelScale"), f.showV = p("showV"), f.showU = p("showU"), f.showPitch = p("showPitch");
    f.selFrom = p("selFrom"), f.selLen = p("selLen");
    if (j["loop"].items.size() >= 2) f.loopStart = g.paramOf(h, j["loop"].items[0].str()), f.loopEnd = g.paramOf(h, j["loop"].items[1].str());
    perOp("showOp", f.showOp), perOp("opV", f.opV), perOp("opU", f.opU), perOp("opTrack", f.opTrack);
    if (j.has("selOp")) f.selOp = std::clamp(std::atoi(g.subst(*h.node, j["selOp"].str()).c_str()), 0, 8) - 1;
    f.selLayer = g.subst(*h.node, j["selLayer"].str()) == "u";
    f.editKey = g.subst(*h.node, j["edit"].str()), f.cmdKey = g.subst(*h.node, j["cmd"].str()), f.rateKey = g.subst(*h.node, j["rate"].str());
    f.levelDb = j["levelDb"].num(0.752574);
    f.screen = colourField(g, h, "screen", 0xff0b0d13), f.gutter = colourField(g, h, "gutter", 0xff161a22), f.grid = colourField(g, h, "grid", 0xff2b3346);
    f.keyWhite = colourField(g, h, "keyWhite", 0xffc8ccd4), f.keyBlack = colourField(g, h, "keyBlack", 0xff15171d), f.keyC = colourField(g, h, "keyC", 0xff6666ff);
    f.selection = colourField(g, h, "selection", 0x4062d0ff), f.selectionEdge = colourField(g, h, "selectionEdge", 0xff62d0ff);
    f.colLoopStart = colourField(g, h, "loopStart", 0xff6ee7a8), f.colLoopEnd = colourField(g, h, "loopEnd", 0xfff0b429);
    f.colStart = colourField(g, h, "startColour", 0xff888888), f.colEnd = colourField(g, h, "endColour", 0xfff07178);
    f.cursor = colourField(g, h, "cursor", 0xffe8b84b), f.hover = colourField(g, h, "hover", 0xffffffff);
    f.glow = colourField(g, h, "glow", 0xffffffff), f.glowU = colourField(g, h, "glowU", 0xffffffff);
    f.line2 = colourField(g, h, "line2", 0xfff2f6f6), f.lineOff = colourField(g, h, "lineOff", 0xff4a505b);
    f.font = w.text.font;
    return f;
}

double paramOr(Gui& g, int p, double def) { return p >= 0 ? g.state().get((size_t)p) : def; }
bool flagOr(Gui& g, int p, bool def) { return p >= 0 ? g.state().get((size_t)p) != 0 : def; }

int trackOfOp(Gui& g, const Fields& f, int op) { return f.opTrack[op] >= 0 ? std::clamp((int)paramOr(g, f.opTrack[op], 1) - 1, 0, 7) : op; }

// The first operator whose voiced (or, for an unvoiced track, unvoiced) side follows a track, 0..7, or -1
// when none does. Without the operators' switches, track n is operator n's.
int userOf(Gui& g, const Fields& f, int t) {
    const int (&sw)[8] = layerOf(t) ? f.opU : f.opV;
    if (sw[0] < 0) return opOf(t);
    for (int o = 0; o < 8; ++o)
        if (flagOr(g, sw[o], false) && trackOfOp(g, f, o) == opOf(t)) return o;
    return -1;
}

std::string opSig(Gui& g, const Fields& f) {   // what the colours and the selected track hang on
    std::string s = std::to_string(f.selOp) + std::to_string(f.selLayer);
    for (int o = 0; o < 8; ++o) s += std::to_string(flagOr(g, f.opV[o], false)) + std::to_string(flagOr(g, f.opU[o], false)) + std::to_string(trackOfOp(g, f, o));
    return s;
}

// ---- geometry: lanes, the time axis, the scales --------------------------------------------------

struct Lane {
    Rect r, plot;
    bool freq = true;
    std::vector<int> tracks;   // what it draws, in drawing order
};

struct Geo {
    Rect r;
    std::vector<Lane> lanes;
    int n = 0, gutter = 0, layout = 1, freqScale = 0, levelScale = 0;
    double x0 = 0, x1 = 1;     // frames in view
    double maxWord = 0;        // the highest word in the document, for the Hz scales
    double levelDb = 0.752574;
    int plotX = 0, plotW = 1;
    double X(double f) const { return plotX + (f - x0) * plotW / (x1 - x0); }
    double frameAt(double x) const { return x0 + (x - plotX) * (x1 - x0) / plotW; }
    double ppf() const { return plotW / (x1 - x0); }
};

bool shown(Gui& g, const Fields& f, int t) {
    if (t == kPitch) return flagOr(g, f.showPitch, true);
    if (!flagOr(g, f.showOp[opOf(t)], true)) return false;
    return layerOf(t) ? flagOr(g, f.showU, true) : flagOr(g, f.showV, true);
}

// Every track an edit reaches: what is on screen, both fields of each shown operator, and the pitch.
std::vector<int> editTracks(Gui& g, const Fields& f) {
    std::vector<int> out;
    for (int t = 0; t < kPitch; ++t)
        if (shown(g, f, freqOf(t))) out.push_back(t);
    if (shown(g, f, kPitch)) out.push_back(kPitch);
    return out;
}

Geo geoOf(Gui& g, const Hit& h, const Fields& f, const Shared& st, Rect r) {
    Geo e;
    e.r = r;
    e.n = st.doc.frames();
    e.gutter = g.skin().dp(28);
    e.layout = (int)paramOr(g, f.layout, 1);
    e.freqScale = (int)paramOr(g, f.freqScale, 0);
    e.levelScale = (int)paramOr(g, f.levelScale, 0);
    e.levelDb = f.levelDb;
    e.plotX = r.x + e.gutter;
    e.plotW = std::max(1, r.w - e.gutter);
    const double span = std::max(1, e.n);
    e.x0 = st.x1 > st.x0 ? st.x0 : 0;
    e.x1 = st.x1 > st.x0 ? st.x1 : span;
    e.x0 = std::clamp(e.x0, 0.0, span), e.x1 = std::clamp(e.x1, e.x0 + 1, span);
    for (int i = 0; i < e.n; ++i)
        for (int t = 0; t < kTracks; ++t)
            if (!isLevel(t)) e.maxWord = std::max(e.maxWord, readValue(st.doc.frame(i), t));
    std::vector<int> vf, uf, vl, ul;
    for (int op = 0; op < 8; ++op) {
        if (shown(g, f, op)) vf.push_back(op), vl.push_back(op + 8);
        if (shown(g, f, 16 + op)) uf.push_back(16 + op), ul.push_back(24 + op);
    }
    const bool pitch = shown(g, f, kPitch);
    auto lane = [&](double weight, bool freq, std::vector<int> tracks) {
        Lane l;
        l.freq = freq;
        l.tracks = std::move(tracks);
        l.r.h = (int)std::lround(weight);   // weights first; resolved below
        e.lanes.push_back(std::move(l));
    };
    auto cat = [](std::vector<int> a, const std::vector<int>& b) { a.insert(a.end(), b.begin(), b.end()); return a; };
    if (e.layout == 0) {
        std::vector<int> t = cat(vf, uf);
        if (pitch) t.insert(t.begin(), kPitch);
        lane(100, true, t);
    } else if (e.layout == 2) {
        std::vector<int> t = vf;
        if (pitch) t.insert(t.begin(), kPitch);
        lane(100, true, t);
        lane(100, true, uf);
    } else {
        std::vector<int> t = cat(vf, uf);
        if (pitch) t.insert(t.begin(), kPitch);
        lane(160, true, t);
        lane(100, false, cat(vl, ul));
    }
    const int gap = g.skin().dp(2);
    int total = 0;
    for (auto& l : e.lanes) total += l.r.h;
    int y = r.y, left = r.h - gap * ((int)e.lanes.size() - 1);
    for (size_t i = 0; i < e.lanes.size(); ++i) {
        Lane& l = e.lanes[i];
        const int hh = i + 1 == e.lanes.size() ? left : (int)std::lround((double)left * l.r.h / std::max(1, total));
        total -= l.r.h;
        left -= hh;
        l.r = {r.x, y, r.w, hh};
        l.plot = {e.plotX, y, e.plotW, hh};
        y += hh + gap;
    }
    return e;
}

// A value on a lane's scale, 0 at the bottom to 1 at the top, and back.
double curveOf(const Geo& e, const Lane& l, int t, double v) {
    if (t == kPitch || !isLevel(t)) {
        const double top = std::max(e.maxWord + 1024 * std::log2(1.2), wordOfHz(100));
        switch (e.freqScale) {
        case 1: return hzOf(v) / hzOf(top);
        case 2: return (v - wordOfMidi(0)) / (wordOfMidi(127) - wordOfMidi(0));
        case 3: return v / kMaxWord;
        default: { const double lo = wordOfHz(20); return (v - lo) / (top - lo); }
        }
    }
    return l.freq ? 0 : e.levelScale == 1 ? ampOf(v, e.levelDb) : 1 - v / 127;
}

double valueOf(const Geo& e, int t, double curve) {
    if (t == kPitch || !isLevel(t)) {
        const double top = std::max(e.maxWord + 1024 * std::log2(1.2), wordOfHz(100));
        switch (e.freqScale) {
        case 1: return wordOfHz(std::max(curve, 1e-4) * hzOf(top));
        case 2: return wordOfMidi(0) + curve * (wordOfMidi(127) - wordOfMidi(0));
        case 3: return curve * kMaxWord;
        default: { const double lo = wordOfHz(20); return lo + curve * (top - lo); }
        }
    }
    return e.levelScale == 1 ? levelOfAmp(std::clamp(curve, 0.0, 1.0), e.levelDb) : (1 - curve) * 127;
}

double Y(const Lane& l, double curve) { return l.plot.y + (l.plot.h - 1) * (1 - std::clamp(curve, 0.0, 1.0)); }
double curveAt(const Lane& l, double y) { return 1 - (y - l.plot.y) / std::max(1, l.plot.h - 1); }

const Lane* laneAt(const Geo& e, int y) {
    for (auto& l : e.lanes)
        if (y >= l.r.y && y < l.r.y + l.r.h) return &l;
    return nullptr;
}

// The track of the app's selected operator in a lane, the one Draw, Line, Move and Scale edit: of the
// selected layer where the lane holds both, its frequency or its level as the lane draws; -1 when the lane
// does not show it.
int selectedTrack(Gui& g, const Fields& f, const Lane& l) {
    if (f.selOp < 0) return -1;
    bool v = false, u = false;
    for (int t : l.tracks)
        if (t != kPitch) (layerOf(t) ? u : v) = true;
    const int layer = v && u ? f.selLayer : u ? 1 : 0;
    const int t = (layer ? 16 : 0) + (l.freq ? 0 : 8) + trackOfOp(g, f, f.selOp);
    return std::find(l.tracks.begin(), l.tracks.end(), t) != l.tracks.end() ? t : -1;
}

// The track nearest the pointer in a lane, within a twelfth of the lane's height; the earlier one on a tie.
int trackAt(const Geo& e, const Lane& l, const Doc& d, double f, double c) {
    const int i = std::clamp((int)std::lround(f - 0.5), 0, std::max(0, d.frames() - 1));
    int best = -1;
    double reach = 0.08;
    if (d.frames() == 0) return -1;
    for (int t : l.tracks) {
        const double dist = std::fabs(curveOf(e, l, t, readValue(d.frame(i), t)) - c);
        if (dist < reach) best = t, reach = dist;
    }
    return best;
}

// ---- the edit out, undo, the clipboard -----------------------------------------------------------

void remember(Shared& st, const std::vector<uint8_t>& bytes) {   // what the engine will hold after an edit: its echo is no new document
    st.sent[st.sentAt++ & 7] = hashOf(bytes);
}

void markDirty(Shared& st, int lo, int hi) {
    st.dirtyLo = std::min(st.dirtyLo, lo);
    st.dirtyHi = std::max(st.dirtyHi, hi);
}

// Sends the frames edited since the last flush, with those of an edit the worker has not taken yet, as
// "frames <first> <count> <base64>"; from the document, or from the preview of a drag in progress.
void flush(Gui& g, const Fields& f, Shared& st, const Doc& from) {
    if (f.editKey.empty() || st.dirtyHi < st.dirtyLo) return;
    int lo = st.dirtyLo, hi = st.dirtyHi;
    if (!g.state().data(f.editKey).empty() && st.sentHi >= st.sentLo) lo = std::min(lo, st.sentLo), hi = std::max(hi, st.sentHi);
    lo = std::max(lo, 0), hi = std::min(hi, from.frames() - 1);
    if (hi < lo) { st.dirtyLo = INT_MAX, st.dirtyHi = -1; return; }
    const int count = hi - lo + 1;
    g.state().setData(f.editKey, "frames " + std::to_string(lo) + " " + std::to_string(count) + " " + b64enc(from.frame(lo), (size_t)count * kFrame));
    std::vector<uint8_t> held = st.doc.bytes;   // the engine's copy after this edit
    if (held.size() == from.bytes.size()) std::memcpy(held.data() + kHeader + (size_t)lo * kFrame, from.frame(lo), (size_t)count * kFrame);
    remember(st, held);
    st.sentLo = lo, st.sentHi = hi;
    st.dirtyLo = INT_MAX, st.dirtyHi = -1;
}

void pushUndo(Shared& st, const Doc& before, const std::string& path) {
    const auto now = Clock::now();
    if (path == st.lastPath && now - st.lastPush < std::chrono::milliseconds(400)) return;   // a repeat folds into the last entry
    st.undo.push_back(before.bytes);
    if (st.undo.size() > kUndoDepth) st.undo.erase(st.undo.begin());
    st.redo.clear();
    st.lastPath = path;
    st.lastPush = now;
}

// Swaps the document for a snapshot, marking the frames that differ for the edit out.
void swapDoc(Shared& st, std::vector<uint8_t>& snapshot) {
    std::vector<uint8_t> was = st.doc.bytes;
    st.doc.bytes.swap(snapshot);
    snapshot = was;
    const int n = std::min(st.doc.frames(), (int)((was.size() - std::min(was.size(), (size_t)kHeader)) / kFrame));
    for (int i = 0; i < n; ++i)
        if (std::memcmp(st.doc.frame(i), was.data() + kHeader + (size_t)i * kFrame, kFrame) != 0) markDirty(st, i, i);
    if (st.doc.frames() != n) markDirty(st, 0, st.doc.frames() - 1);
    ++st.docVersion;
}

void selectionOrAll(const Shared& st, int& from, int& to) {
    if (st.selFrom >= 0 && st.selTo >= st.selFrom) from = st.selFrom, to = std::min(st.selTo, st.doc.frames() - 1);
    else from = 0, to = st.doc.frames() - 1;
}

// ---- range operations, on the selection (or the whole sequence) and the visible tracks -----------

void rangeOp(Gui& g, const Fields& f, Shared& st, const std::string& op, double amount) {
    if (st.doc.frames() == 0) return;
    int from, to;
    selectionOrAll(st, from, to);
    if (to < from) return;
    const Doc before = st.doc;
    const std::vector<int> tracks = editTracks(g, f);
    const double levelDb = f.levelDb;
    double pivot = 0;
    if (op == "scale") {   // about the selection's mean pitch
        double sum = 0;
        int k = 0;
        for (int t : tracks)
            if (!isLevel(t))
                for (int i = from; i <= to; ++i) sum += midiOf(readValue(st.doc.frame(i), t)), ++k;
        pivot = k ? sum / k : 69;
    }
    double loudest = 127;
    if (op == "normalize")
        for (int t : tracks)
            if (isLevel(t))
                for (int i = from; i <= to; ++i) loudest = std::min(loudest, readValue(st.doc.frame(i), t));
    for (int t : tracks) {
        const bool level = isLevel(t);
        std::vector<double> v = readTrack(st.doc, t);
        const std::vector<double> src = v;
        const int n = (int)v.size();
        if (op == "reverse") {
            std::reverse(v.begin() + from, v.begin() + to + 1);
        } else if (op == "smooth") {   // a three-tap mean; a silent level is a gap, and a jump is kept as it is
            for (int i = std::max(from, 1); i <= std::min(to, n - 2); ++i) {
                if (level && (src[(size_t)i] > 90 || src[(size_t)i - 1] > 90 || src[(size_t)i + 1] > 90)) continue;
                if (std::fabs(src[(size_t)i + 1] - src[(size_t)i - 1]) > (level ? 30 : 400)) continue;
                v[(size_t)i] = (src[(size_t)i - 1] + 2 * src[(size_t)i] + src[(size_t)i + 1]) / 4;
            }
        } else if (op == "quantize") {
            if (!level)
                for (int i = from; i <= to; ++i) v[(size_t)i] = wordOfMidi(std::round(midiOf(v[(size_t)i])));
        } else if (op == "normalize") {   // one shift so the loudest selected level reaches 0; silence stays silent
            if (level && loudest > 0 && loudest < 127)
                for (int i = from; i <= to; ++i)
                    if (v[(size_t)i] < 127) v[(size_t)i] = std::max(0.0, v[(size_t)i] - loudest);
        } else if (op == "silence") {
            if (level)
                for (int i = from; i <= to; ++i) v[(size_t)i] = 127;
        } else if (op == "fade_in" || op == "fade_out") {   // linear in amplitude
            if (level)
                for (int i = from; i <= to; ++i) {
                    if (v[(size_t)i] >= 127) continue;
                    double k = to > from ? double(i - from) / (to - from) : 1;
                    if (op == "fade_out") k = 1 - k;
                    v[(size_t)i] = levelOfAmp(ampOf(v[(size_t)i], levelDb) * k, levelDb);
                }
        } else if (op == "interpolate") {   // a line between the frames just outside the span
            const double a = src[(size_t)std::max(from - 1, 0)], b = src[(size_t)std::min(to + 1, n - 1)];
            for (int i = from; i <= to; ++i) v[(size_t)i] = a + (b - a) * (i - from + 1) / (to - from + 2);
        } else if (op == "ramp") {
            const double a = src[(size_t)from], b = src[(size_t)to];
            for (int i = from; i <= to; ++i) v[(size_t)i] = to > from ? a + (b - a) * (i - from) / (to - from) : a;
        } else if (op == "hold") {
            for (int i = from; i <= to; ++i) v[(size_t)i] = src[(size_t)from];
        } else if (op == "gain") {   // amount in dB, louder is a lower level; silence stays silent
            if (level)
                for (int i = from; i <= to; ++i)
                    if (v[(size_t)i] < 127) v[(size_t)i] = std::clamp(v[(size_t)i] - amount / levelDb, 0.0, 127.0);
        } else if (op == "shift") {   // amount in semitones
            if (!level)
                for (int i = from; i <= to; ++i) v[(size_t)i] += amount * kWordsPerSemitone;
        } else if (op == "scale") {   // amount a factor on the distance from the mean pitch, in semitones
            if (!level)
                for (int i = from; i <= to; ++i) v[(size_t)i] = wordOfMidi(pivot + (midiOf(v[(size_t)i]) - pivot) * amount);
        }
        writeTrack(st.doc, t, v);
    }
    if (st.doc.bytes == before.bytes) return;
    pushUndo(st, before, op);
    markDirty(st, from, to);
    ++st.docVersion;
    flush(g, f, st, st.doc);
}

void copySelection(Gui& g, const Fields& f, Shared& st) {
    int from, to;
    selectionOrAll(st, from, to);
    if (to < from) return;
    st.clip.frames = to - from + 1;
    st.clip.tracks.clear();
    for (int t : editTracks(g, f)) {
        const std::vector<double> v = readTrack(st.doc, t);
        st.clip.tracks.push_back({t, std::vector<double>(v.begin() + from, v.begin() + to + 1)});
    }
}

void paste(Gui& g, const Fields& f, Shared& st) {
    if (st.clip.frames == 0 || st.doc.frames() == 0) return;
    const int at = st.selFrom >= 0 ? st.selFrom : 0;
    const Doc before = st.doc;
    for (auto& tr : st.clip.tracks) {
        std::vector<double> v = readTrack(st.doc, tr.first);
        for (int i = 0; i < st.clip.frames && at + i < (int)v.size(); ++i) v[(size_t)(at + i)] = tr.second[(size_t)i];
        writeTrack(st.doc, tr.first, v);
    }
    if (st.doc.bytes == before.bytes) return;
    pushUndo(st, before, "paste");
    markDirty(st, at, std::min(at + st.clip.frames - 1, st.doc.frames() - 1));
    ++st.docVersion;
    flush(g, f, st, st.doc);
}

// ---- the view -----------------------------------------------------------------------------------

void setView(Shared& st, double x0, double span) {
    const double n = std::max(1, st.doc.frames());
    span = std::clamp(span, std::min(8.0, n), n);
    x0 = std::clamp(x0, 0.0, n - span);
    if (span >= n) st.x0 = st.x1 = 0;   // the fit
    else st.x0 = x0, st.x1 = x0 + span;
}

void writeSelection(Gui& g, const Fields& f, Shared& st) {
    const int from = st.selFrom >= 0 ? st.selFrom : 0, len = st.selFrom >= 0 ? st.selTo - st.selFrom + 1 : 0;
    if (f.selFrom >= 0 && st.selWritten[0] != from) g.setParam(f.selFrom, from);
    if (f.selLen >= 0 && st.selWritten[1] != len) g.setParam(f.selLen, len);
    st.selWritten[0] = from, st.selWritten[1] = len;
}

// A selection typed into the sel params, when it is not what the widget wrote.
void readSelection(Gui& g, const Fields& f, Shared& st) {
    if (f.selFrom < 0 || f.selLen < 0) return;
    const int from = (int)paramOr(g, f.selFrom, 0), len = (int)paramOr(g, f.selLen, 0);
    if (from == st.selWritten[0] && len == st.selWritten[1]) return;
    st.selWritten[0] = from, st.selWritten[1] = len;
    if (len <= 0 || st.doc.frames() == 0) st.selFrom = st.selTo = -1;
    else st.selFrom = std::clamp(from, 0, st.doc.frames() - 1), st.selTo = std::clamp(from + len - 1, st.selFrom, st.doc.frames() - 1);
}

// ---- the document in ----------------------------------------------------------------------------

// A changed document under the key: the processor's echo of an edit sent is nothing new; one that differs
// in its header alone (an End or a Loop typed) keeps the frames and the history; anything else is another
// Fseq, and the editor starts over on it.
void sync(Gui& g, const Hit& h, Shared& st) {
    const std::string& key = g.inst(h).key;
    if (key.empty()) return;
    const std::string d = g.state().data(key);
    if (d == st.docSeen) return;
    st.docSeen = d;
    std::vector<uint8_t> bytes = b64dec(d);
    if (bytes.size() < kHeader + kFrame || (bytes.size() - kHeader) % kFrame) bytes.clear();
    const uint64_t hash = hashOf(bytes);
    for (uint64_t s : st.sent)
        if (s == hash && !bytes.empty()) return;
    const bool sameFrames = bytes.size() == st.doc.bytes.size() && !bytes.empty() &&
                            std::memcmp(bytes.data() + kHeader, st.doc.bytes.data() + kHeader, bytes.size() - kHeader) == 0;
    if (sameFrames) {
        std::memcpy(st.doc.bytes.data(), bytes.data(), kHeader);
        ++st.docVersion;
        return;
    }
    st.doc.bytes = std::move(bytes);
    st.undo.clear(), st.redo.clear();
    st.gest = Gesture();
    st.previewOn = false;
    st.dirtyLo = INT_MAX, st.dirtyHi = -1, st.sentLo = INT_MAX, st.sentHi = -1;
    const int n = st.doc.frames();
    if (st.selFrom >= n) st.selFrom = st.selTo = -1;
    else if (st.selTo >= n) st.selTo = n - 1;
    if (st.x1 > st.x0) setView(st, st.x0, st.x1 - st.x0);
    ++st.docVersion;
}

// ---- rendering -------------------------------------------------------------------------------------

uint32_t withAlpha(uint32_t col, double a) { return (uint32_t)std::lround((col >> 24) * std::clamp(a, 0.0, 1.0)) << 24 | (col & 0xffffff); }

// A track's points in a lane, one a frame in view, or one pair a column below a pixel a frame, each with
// the opacity its level gives it (1 for a level track and the pitch).
struct Pt { double x, y, a; };

std::vector<Pt> tracePoints(const Geo& e, const Lane& l, const Doc& d, int t, double levelDb) {
    std::vector<Pt> out;
    const int n = d.frames();
    if (n == 0) return out;
    const int lv = levelOf(t);
    const int i0 = std::max(0, (int)std::floor(e.x0)), i1 = std::min(n - 1, (int)std::ceil(e.x1));
    auto alpha = [&](int i) { return lv < 0 ? 1.0 : traceAlpha(readValue(d.frame(i), lv), levelDb); };
    if (e.ppf() >= 1) {
        for (int i = i0; i <= i1; ++i) out.push_back({e.X(i + 0.5), Y(l, curveOf(e, l, t, readValue(d.frame(i), t))), alpha(i)});
        return out;
    }
    for (int i = i0; i <= i1;) {   // a column: its lowest and highest value, as loud as its loudest frame
        const int col = (int)std::floor(e.X(i + 0.5));
        double lo = 1e9, hi = -1e9, a = 0;
        int j = i;
        for (; j <= i1 && (int)std::floor(e.X(j + 0.5)) == col; ++j) {
            const double v = readValue(d.frame(j), t);
            lo = std::min(lo, v), hi = std::max(hi, v), a = std::max(a, alpha(j));
        }
        out.push_back({col + 0.5, Y(l, curveOf(e, l, t, lo)), a});
        if (hi != lo) out.push_back({col + 0.5, Y(l, curveOf(e, l, t, hi)), a});
        i = j;
    }
    return out;
}

// The ribbon: one stroke through the points, laid into a mask and put down once, so it never blends over
// itself where its segments meet. Each pixel takes the most any segment gives it: how much the segment
// covers it, antialiased, times the opacity between the segment's two frames there. While a drag moves
// (fast) a segment is square-cut at its mean opacity instead.
void ribbon(Canvas& c, const std::vector<Pt>& pts, uint32_t col, double width, bool fast) {
    if (pts.size() < 2) return;
    const double half = width / 2;
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (auto& p : pts) x0 = std::min(x0, p.x), y0 = std::min(y0, p.y), x1 = std::max(x1, p.x), y1 = std::max(y1, p.y);
    Rect box{(int)std::floor(x0 - half - 1), (int)std::floor(y0 - half - 1), 0, 0};
    box.w = (int)std::ceil(x1 + half + 1) - box.x + 1, box.h = (int)std::ceil(y1 + half + 1) - box.y + 1;
    box = box & c.clip;
    if (box.empty()) return;
    static thread_local std::vector<float> mask;
    mask.assign((size_t)box.w * box.h, 0.0f);
    auto put = [&](int x, int y, float v) {
        if (x < box.x || y < box.y || x >= box.x + box.w || y >= box.y + box.h) return;
        float& m = mask[(size_t)(y - box.y) * box.w + (x - box.x)];
        m = std::max(m, v);
    };
    const int k = std::max(1, (int)std::lround(width));
    for (size_t s = 0; s + 1 < pts.size(); ++s) {
        const double ax = pts[s].x, ay = pts[s].y, bx = pts[s + 1].x, by = pts[s + 1].y, aa = pts[s].a, ab = pts[s + 1].a;
        if (aa <= 0 && ab <= 0) continue;
        if (fast) {   // Bresenham, k px square
            int px = (int)ax, py = (int)ay;
            const int qx = (int)bx, qy = (int)by, dx = std::abs(qx - px), dy = -std::abs(qy - py), sx = px < qx ? 1 : -1, sy = py < qy ? 1 : -1;
            const float a = (float)(aa + ab) / 2;
            for (int err = dx + dy;;) {
                for (int j = 0; j < k; ++j)
                    for (int i = 0; i < k; ++i) put(px - (k - 1) / 2 + i, py - (k - 1) / 2 + j, a);
                if (px == qx && py == qy) break;
                const int e2 = 2 * err;
                if (e2 >= dy) { err += dy; px += sx; }
                if (e2 <= dx) { err += dx; py += sy; }
            }
            continue;
        }
        const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
        const int sx0 = std::max(box.x, (int)std::floor(std::min(ax, bx) - half - 1)), sx1 = std::min(box.x + box.w - 1, (int)std::ceil(std::max(ax, bx) + half + 1));
        const int sy0 = std::max(box.y, (int)std::floor(std::min(ay, by) - half - 1)), sy1 = std::min(box.y + box.h - 1, (int)std::ceil(std::max(ay, by) + half + 1));
        for (int y = sy0; y <= sy1; ++y)
            for (int x = sx0; x <= sx1; ++x) {
                const double px = x + 0.5 - ax, py = y + 0.5 - ay, t = len2 > 0 ? std::clamp((px * dx + py * dy) / len2, 0.0, 1.0) : 0;
                const double cover = std::clamp(half + 0.5 - std::hypot(px - t * dx, py - t * dy), 0.0, 1.0);
                if (cover > 0) put(x, y, (float)(cover * (aa + (ab - aa) * t)));
            }
    }
    for (int y = 0; y < box.h; ++y)
        for (int x = 0; x < box.w; ++x)
            if (const float m = mask[(size_t)y * box.w + x]; m > 0) fillRect(c, {box.x + x, box.y + y, 1, 1}, withAlpha(col, m));
}

// The fill under a level track: every pixel column from the line, between its frames, down to the lane's
// foot, so the area reads as one shape at any zoom.
void levelFill(Canvas& c, const Lane& l, const std::vector<Pt>& pts, uint32_t col) {
    std::vector<int> top((size_t)l.plot.w, INT_MAX);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const double ax = pts[i].x, ay = pts[i].y, bx = pts[i + 1].x, by = pts[i + 1].y;
        const int from = std::max(0, (int)std::floor(std::min(ax, bx)) - l.plot.x), to = std::min(l.plot.w - 1, (int)std::floor(std::max(ax, bx)) - l.plot.x);
        for (int x = from; x <= to; ++x) {
            const double t = bx != ax ? std::clamp((l.plot.x + x + 0.5 - ax) / (bx - ax), 0.0, 1.0) : 0;
            const int y = (int)std::lround(bx != ax ? ay + (by - ay) * t : std::min(ay, by));
            top[(size_t)x] = std::min(top[(size_t)x], y);
        }
    }
    const int bottom = l.plot.y + l.plot.h;
    for (int x = 0; x < l.plot.w; ++x)
        if (top[(size_t)x] < bottom) fillRect(c, {l.plot.x + x, top[(size_t)x], 1, bottom - top[(size_t)x]}, col);
}

// A track in the colour of the first operator that follows it, as every page colours its operators; in
// `lineOff` when none does.
uint32_t trackColour(Gui& g, const Hit& h, const Fields& f, int t) {
    if (t == kPitch) return f.line2;
    const int o = userOf(g, f, t);
    return o < 0 ? f.lineOff : numberedColour(g, h, layerOf(t) ? "lineU" : "line", o + 1, 0xffffffff);
}

uint32_t tint(uint32_t col, uint32_t by) {   // a colour seen through another, channel by channel; white leaves it
    uint32_t out = col & 0xff000000;
    for (int sh = 0; sh < 24; sh += 8) out |= ((col >> sh & 255) * (by >> sh & 255) + 127) / 255 << sh;
    return out;
}

// A ribbon's width in a lane: a formant's grows with a frequency lane's height, as fs1r-edit's did, the
// unvoiced two thirds of it; the pitch and the levels stay thin.
double ribbonWidth(Gui& g, const Lane& l, int t) {
    const int k = g.skin().lw();
    const double formant = l.freq ? std::clamp(l.plot.h / 60.0, 1.5 * k, 4.0 * k) : 1.5 * k;
    return t == kPitch ? 1.5 * k : layerOf(t) ? formant * 2 / 3 : formant;
}

// A small rgb buffer the bloom is laid into, sx by sy lane pixels to each of its own.
struct Bloom {
    int w, h, sx, sy;
    std::vector<float> px;
    Bloom(int laneW, int laneH, int sx_, int sy_)
        : w(std::max(1, (laneW + sx_ - 1) / sx_)), h(std::max(1, (laneH + sy_ - 1) / sy_)), sx(sx_), sy(sy_), px((size_t)w * h * 3, 0.0f) {}

    void add(int x, int y, uint32_t col, float k) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        float* p = &px[((size_t)y * w + x) * 3];
        p[0] += (col >> 16 & 255) / 255.0f * k, p[1] += (col >> 8 & 255) / 255.0f * k, p[2] += (col & 255) / 255.0f * k;
    }

    // A segment in lane pixels, k to each buffer column a level one crosses whole: its ink goes evenly along
    // time, and in each column over the rows it crosses there, so a jump between frames lights no more than
    // a level stretch and leaves no streak.
    void line(double x0, double y0, double x1, double y1, uint32_t col, float k) {
        if (x1 < x0) std::swap(x0, x1), std::swap(y0, y1);
        if (x1 - x0 < 1e-9) return;
        const double slope = (y1 - y0) / (x1 - x0);
        for (int cx = (int)std::floor(x0 / sx); cx * sx < x1; ++cx) {
            const double a = std::max(x0, (double)cx * sx), b = std::min(x1, (double)(cx + 1) * sx);
            if (b <= a) continue;
            const double ya = y0 + (a - x0) * slope, yb = y0 + (b - x0) * slope;
            const int ra = (int)std::floor(std::min(ya, yb) / sy), rb = (int)std::floor(std::max(ya, yb) / sy);
            const float each = (float)(k * (b - a) / sx / (rb - ra + 1));
            for (int r = ra; r <= rb; ++r) add(cx, r, col, each);
        }
    }

    // One box pass along every row or column, r pixels each side, nothing past the edges: a running sum,
    // a column's kept for a whole row at once so the pass goes down the buffer in order.
    void box(bool vertical, int r, std::vector<float>& src) {
        if (r <= 0) return;
        src = px;
        const float inv = 1.0f / (2 * r + 1);
        const size_t row = (size_t)w * 3;
        if (!vertical) {
            for (int y = 0; y < h; ++y) {
                const float* in = &src[(size_t)y * row];
                float* out = &px[(size_t)y * row];
                float s[3] = {};
                for (int i = 0; i <= std::min(r, w - 1); ++i)
                    for (int ch = 0; ch < 3; ++ch) s[ch] += in[i * 3 + ch];
                for (int x = 0; x < w; ++x)
                    for (int ch = 0; ch < 3; ++ch) {
                        out[x * 3 + ch] = std::max(0.0f, s[ch] * inv);
                        if (x + r + 1 < w) s[ch] += in[(x + r + 1) * 3 + ch];
                        if (x - r >= 0) s[ch] -= in[(x - r) * 3 + ch];
                    }
            }
            return;
        }
        std::vector<float> s(row, 0.0f);
        for (int y = 0; y <= std::min(r, h - 1); ++y)
            for (size_t k = 0; k < row; ++k) s[k] += src[(size_t)y * row + k];
        for (int y = 0; y < h; ++y) {
            float* out = &px[(size_t)y * row];
            for (size_t k = 0; k < row; ++k) out[k] = std::max(0.0f, s[k] * inv);
            if (y + r + 1 < h)
                for (size_t k = 0; k < row; ++k) s[k] += src[(size_t)(y + r + 1) * row + k];
            if (y - r >= 0)
                for (size_t k = 0; k < row; ++k) s[k] -= src[(size_t)(y - r) * row + k];
        }
    }

    // Three box passes each way, near enough a Gaussian of sigma sigX by sigY lane pixels, scaled so a
    // level line keeps its ink as the peak it blurs to.
    void blur(double sigX, double sigY) {
        auto radius = [](double s) { return (int)std::lround((std::sqrt(1 + 4 * s * s) - 1) / 2); };   // three passes of r: sigma^2 = r(r+1)
        const int rx = radius(sigX / sx), ry = radius(sigY / sy);
        std::vector<float> tmp;
        for (int pass = 0; pass < 3; ++pass) box(false, rx, tmp), box(true, ry, tmp);
        if (ry <= 0) return;
        const float peak = (float)std::sqrt(ry * (ry + 1) * 2 * 3.14159265358979);
        for (float& v : px) v *= peak;
    }

    void saturate(float k) {   // each pixel pushed away from its grey by k, so colours that summed stay apart
        for (size_t i = 0; i < px.size(); i += 3) {
            const float m = (px[i] + px[i + 1] + px[i + 2]) / 3;
            for (int ch = 0; ch < 3; ++ch) px[i + ch] = std::max(0.0f, m + (px[i + ch] - m) * k);
        }
    }

    // The pixel either side of a lane pixel's centre along one axis, and the weight of the second.
    static void tap(int i, int scale, int size, int& a, int& b, float& t) {
        const double f = std::clamp((i + 0.5) / scale - 0.5, 0.0, size - 1.0);
        a = (int)f, b = std::min(size - 1, a + 1), t = (float)(f - a);
    }
};

// The bloom under the formant ribbons, after fs1r-edit's: each ribbon laid into two small buffers in its
// own colour, as strong as it is loud. Up and down the light spreads as a formant spreads over its
// bandwidth, tight in one buffer (the ribbon's halo) and tall in the other (the column of light behind a
// sound); in time it only bleeds half a frame, since a formant does not spread over time. Both are
// stretched back over the lane, toned so that a dense patch runs through the colour to white, and
// screened onto it; the tall one is saturated first, or the operators stacked in a column sum to grey.
// `glow` (voiced) and `glowU` (unvoiced) tint the light, white leaving it alone, and their alphas are the
// two layers' strengths.
void bloomLane(Canvas& c, Gui& g, const Hit& h, const Fields& f, const Geo& e, const Lane& l, const Doc& d) {
    constexpr double kHalo = 0.02, kSmear = 0.065, kBleed = 0.5;   // up and down as fractions of the lane's height, in time of a frame
    constexpr float kHaloGain = 0.5f, kSmearGain = 0.35f, kSaturate = 3;
    if (l.plot.h < g.skin().dp(48) || !((f.glow | f.glowU) >> 24)) return;
    Bloom halo(l.plot.w, l.plot.h, 2, 2), smear(l.plot.w, l.plot.h, 2, 4);
    for (int t : l.tracks) {
        if (t == kPitch || isLevel(t)) continue;
        const uint32_t glow = layerOf(t) ? f.glowU : f.glow, light = tint(trackColour(g, h, f, t), glow);
        const float strength = (glow >> 24) / 255.0f;
        if (strength <= 0) continue;
        const std::vector<Pt> pts = tracePoints(e, l, d, t, f.levelDb);
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const float o = (float)(pts[i].a + pts[i + 1].a) / 2, a = o * o * strength;   // as bright as its amplitude
            const double x0 = pts[i].x - l.plot.x, y0 = pts[i].y - l.plot.y, x1 = pts[i + 1].x - l.plot.x, y1 = pts[i + 1].y - l.plot.y;
            halo.line(x0, y0, x1, y1, light, a * kHaloGain);
            smear.line(x0, y0, x1, y1, light, a * kSmearGain);
        }
    }
    const double bleed = std::min(kBleed * e.ppf(), l.plot.h * kHalo);
    halo.blur(bleed, l.plot.h * kHalo);
    smear.blur(bleed, l.plot.h * kSmear);
    smear.saturate(kSaturate);
    // Back over the lane: for each row, the two buffers' rows blended down to it and summed (they share
    // their columns), then each pixel between two columns, toned and screened on.
    static const std::vector<float> tone = [] {   // 1 - e^-A for A in [0, 8], 256 steps a unit
        std::vector<float> v(8 * 256 + 1);
        for (size_t i = 0; i < v.size(); ++i) v[i] = 1 - std::exp(-(float)i / 256);
        return v;
    }();
    const Rect span = c.clip & l.plot;
    if (span.w <= 0 || span.h <= 0) return;
    std::vector<int> col0((size_t)span.w), col1((size_t)span.w);
    std::vector<float> colT((size_t)span.w), mix((size_t)halo.w * 3);
    for (int i = 0; i < span.w; ++i) Bloom::tap(span.x - l.plot.x + i, halo.sx, halo.w, col0[(size_t)i], col1[(size_t)i], colT[(size_t)i]);
    for (int y = span.y; y < span.y + span.h; ++y) {
        int h0, h1, s0, s1;
        float ht, st;
        Bloom::tap(y - l.plot.y, halo.sy, halo.h, h0, h1, ht);
        Bloom::tap(y - l.plot.y, smear.sy, smear.h, s0, s1, st);
        const float *ha = &halo.px[(size_t)h0 * halo.w * 3], *hb = &halo.px[(size_t)h1 * halo.w * 3];
        const float *sa = &smear.px[(size_t)s0 * smear.w * 3], *sb = &smear.px[(size_t)s1 * smear.w * 3];
        for (size_t k = 0; k < mix.size(); ++k) mix[k] = ha[k] + (hb[k] - ha[k]) * ht + sa[k] + (sb[k] - sa[k]) * st;
        uint32_t* row = c.px + (size_t)y * c.w + span.x;
        for (int i = 0; i < span.w; ++i) {
            const float *m0 = &mix[(size_t)col0[(size_t)i] * 3], *m1 = &mix[(size_t)col1[(size_t)i] * 3], t = colT[(size_t)i];
            const float lit[3] = {m0[0] + (m1[0] - m0[0]) * t, m0[1] + (m1[1] - m0[1]) * t, m0[2] + (m1[2] - m0[2]) * t};
            if (lit[0] + lit[1] + lit[2] < 0.004f) continue;
            uint32_t out = 0xff000000;
            for (int ch = 0; ch < 3; ++ch) {
                const int sh = 16 - 8 * ch, d0 = (int)(row[i] >> sh & 255);
                out |= (uint32_t)(d0 + (int)(tone[(size_t)std::min(8.0f * 256, lit[ch] * 256)] * (255 - d0) + 0.5f)) << sh;
            }
            row[i] = out;
        }
    }
}

// The ticks of a scale: the curve of each, with its label, for the gutter and the grid.
struct Tick { double curve; std::string label; bool major; };

std::vector<Tick> ticksOf(const Geo& e, const Lane& l, int t) {
    std::vector<Tick> out;
    auto add = [&](double v, const std::string& s, bool major) {
        const double c = curveOf(e, l, t, v);
        if (c >= 0 && c <= 1) out.push_back({c, s, major});
    };
    if (l.freq) {
        if (e.freqScale == 2) {
            for (int m = 0; m <= 127; m += 12) add(wordOfMidi(m), "C" + std::to_string(m / 12 - 2), true);
        } else if (e.freqScale == 3) {
            for (int w = 0; w <= kMaxWord; w += 4096) add(w, std::to_string(w), w % 8192 == 0);
        } else {
            for (double dec = 10; dec <= 10000; dec *= 10)
                for (int m : {1, 2, 5}) {
                    const double hz = dec * m;
                    char s[16];
                    if (hz >= 1000) std::snprintf(s, sizeof s, "%gk", hz / 1000);
                    else std::snprintf(s, sizeof s, "%g", hz);
                    add(wordOfHz(hz), s, m == 1);
                }
        }
        return out;
    }
    if (e.levelScale == 1) {
        for (int k = 0; k <= 5; ++k) add(levelOfAmp(std::pow(0.5, k), e.levelDb), k == 0 ? "1" : "1/" + std::to_string(1 << k), k % 2 == 0);
    } else {
        const int step = e.levelScale == 2 ? 16 : (int)std::lround(6 / e.levelDb);   // 6 dB
        for (int lv = 0; lv <= 127; lv += step)
            add(lv, e.levelScale == 2 ? std::to_string(lv) : "-" + std::to_string((int)std::lround(lv * e.levelDb)), (lv / step) % 2 == 0);
    }
    return out;
}

void renderLane(Canvas& c, Gui& g, const Hit& h, const Fields& f, const Geo& e, const Lane& l, const Doc& d, Shared& st) {
    const int k = g.skin().lw();
    fillRect(c, l.r, f.screen);
    fillRect(c, {l.r.x, l.r.y, e.gutter, l.r.h}, f.gutter);
    const std::vector<Tick> ticks = ticksOf(e, l, l.freq ? 0 : 8);
    const Font* font = f.font >= 0 ? &g.skin().fonts[(size_t)f.font] : nullptr;
    if (l.freq && e.freqScale != 3) {   // the piano ruler: a key per semitone, C keys marked
        const double per = std::fabs(Y(l, curveOf(e, l, 0, wordOfMidi(61))) - Y(l, curveOf(e, l, 0, wordOfMidi(60))));
        if (per >= 1.5)
            for (int m = 0; m <= 127; ++m) {
                const double y0 = Y(l, curveOf(e, l, 0, wordOfMidi(m + 0.5))), y1 = Y(l, curveOf(e, l, 0, wordOfMidi(m - 0.5)));
                if (y1 < l.plot.y || y0 > l.plot.y + l.plot.h) continue;
                const int top = std::max(l.plot.y, (int)std::floor(y0)), bottom = std::min(l.plot.y + l.plot.h, (int)std::ceil(y1));
                const int semi = m % 12;
                const bool black = semi == 1 || semi == 3 || semi == 6 || semi == 8 || semi == 10;
                const int gw = black ? e.gutter * 7 / 10 : e.gutter - 2 * k;
                fillRect(c, {l.r.x + k, top, gw, std::max(1, bottom - top - (per >= 4 ? 1 : 0))}, semi == 0 ? f.keyC : black ? f.keyBlack : f.keyWhite);
            }
    }
    for (auto& t : ticks) {   // the grid, and the gutter's labels on a level lane or a raw scale
        const int y = (int)std::lround(Y(l, t.curve));
        fillRect(c, {l.plot.x, y, l.plot.w, k}, withAlpha(f.grid, t.major ? 1 : 0.7));
        if (font && (!l.freq || e.freqScale == 3) && l.plot.h > g.skin().dp(34) && t.major)
            drawText(c, *font, t.label, {l.r.x, y - font->height / 2, e.gutter - g.skin().dp(3), font->height}, 2, 0, false);
    }
    if (e.ppf() >= g.skin().dp(4) && e.ppf() >= 2.5 * (double)l.plot.w / std::max(1, e.n)) {   // a frame grid when zoomed in
        const double a = std::clamp((e.ppf() / g.skin().dp(4) - 1) * 0.15, 0.1, 0.45);
        for (int i = (int)std::ceil(e.x0); i <= (int)std::floor(e.x1); ++i)
            fillRect(c, {(int)std::lround(e.X(i)), l.plot.y, k, l.plot.h}, withAlpha(f.grid, a));
    }
    Rect saved = c.clip;
    c.clip = c.clip & l.plot;
    if (!st.fast && l.freq) bloomLane(c, g, h, f, e, l, d);
    const int selected = selectedTrack(g, f, l);
    auto track = [&](int t) {
        const uint32_t col = trackColour(g, h, f, t);
        std::vector<Pt> pts = tracePoints(e, l, d, t, f.levelDb);
        if (!l.freq) levelFill(c, l, pts, withAlpha(col, 0.04));   // faint: sixteen of them stack
        if (t == selected) {   // the selected operator's, last: rimmed, and seen through its quiet frames
            std::vector<std::pair<double, double>> rim;
            for (auto& p : pts) rim.push_back({p.x, p.y}), p.a = std::max(p.a, 0.5);
            drawPolyline(c, rim, withAlpha(f.hover, 0.35), ribbonWidth(g, l, t) + 2 * k);
        }
        ribbon(c, pts, col, ribbonWidth(g, l, t), st.fast);
    };
    for (int t : l.tracks)
        if (t != selected) track(t);
    if (selected >= 0) track(selected);
    c.clip = saved;
}

std::string cacheKeyOf(Gui& g, const Fields& f, const Geo& e, const Shared& st, Rect r) {
    std::string s = std::to_string(r.w) + "x" + std::to_string(r.h) + "@" + std::to_string(g.skin().density) + "/" + std::to_string(st.docVersion) + "/" +
                    std::to_string(st.previewOn ? st.previewVersion : 0) + "/" + std::to_string(e.layout) + std::to_string(e.freqScale) + std::to_string(e.levelScale) +
                    "/" + std::to_string(e.x0) + "-" + std::to_string(e.x1) + "/" + (st.fast ? "f" : "q") + "/";
    for (auto& l : e.lanes)
        for (int t : l.tracks) s += std::to_string(t) + ",";
    return s + "/" + opSig(g, f);
}

// ---- markers and the overlay --------------------------------------------------------------------

struct Marker { int param; uint32_t col; int on, off; };   // a dash pattern, in line widths

std::vector<Marker> markersOf(const Fields& f) {
    return {{f.loopStart, f.colLoopStart, 3, 3}, {f.loopEnd, f.colLoopEnd, 3, 3}, {f.start, f.colStart, 1, 3}, {f.end, f.colEnd, 2, 4}};
}

void dashed(Canvas& c, int x, int y, int h, uint32_t col, int on, int off, int k) {
    for (int yy = y; yy < y + h; yy += (on + off) * k) fillRect(c, {x, yy, k, std::min(on * k, y + h - yy)}, col);
}

double playFrame(Gui& g, const Fields& f, Shared& st) {   // where the playhead is drawn, run on between updates
    if (f.position < 0) return -1;
    const double pos = g.state().get((size_t)f.position);
    const auto now = Clock::now();
    if (pos != st.lastPos) st.lastPos = pos, st.lastPosAt = now;
    long us = 0;
    int dir = 1, running = 0;
    if (!f.rateKey.empty()) std::sscanf(g.state().data(f.rateKey).c_str(), "%ld %d %d", &us, &dir, &running);
    if (!running || us <= 0) return pos;
    const double elapsed = std::min(std::chrono::duration<double>(now - st.lastPosAt).count(), 2.0 / 30);
    double fr = pos + dir * elapsed / (us / 1e6);
    if (f.loopEnd >= 0 && f.loopStart >= 0) {
        const double lo = std::min(g.state().get((size_t)f.loopStart), g.state().get((size_t)f.loopEnd)), hi = std::max(g.state().get((size_t)f.loopStart), g.state().get((size_t)f.loopEnd));
        if (pos >= lo && pos <= hi) fr = std::clamp(fr, lo, hi);
    }
    return fr;
}

void drawOverlay(Canvas& c, Gui& g, const Hit& h, const Fields& f, const Geo& e, Shared& st, const Doc& d) {
    const int k = g.skin().lw();
    const Rect stack = e.lanes.empty() ? e.r : Rect{e.plotX, e.r.y, e.plotW, e.r.h};
    Rect saved = c.clip;
    c.clip = c.clip & stack;
    if (st.selFrom >= 0 && st.selTo >= st.selFrom) {
        const int xa = (int)std::lround(e.X(st.selFrom)), xb = (int)std::lround(e.X(st.selTo + 1));
        fillRect(c, {xa, stack.y, std::max(1, xb - xa), stack.h}, f.selection);
        fillRect(c, {xa, stack.y, k, stack.h}, f.selectionEdge);
        fillRect(c, {xb - k, stack.y, k, stack.h}, f.selectionEdge);
    }
    for (auto& m : markersOf(f)) {
        if (m.param < 0) continue;
        const int x = (int)std::lround(e.X(std::clamp(g.state().get((size_t)m.param), 0.0, (double)std::max(0, e.n - 1))));
        dashed(c, x, stack.y, stack.h, m.col, m.on, m.off, k);
    }
    if (st.hoverTrack >= 0 && st.hoverLane >= 0 && st.hoverLane < (int)e.lanes.size() && !st.gest.active) {
        const Lane& l = e.lanes[(size_t)st.hoverLane];
        Rect was = c.clip;
        c.clip = c.clip & l.plot;
        std::vector<std::pair<double, double>> pts;
        for (auto& p : tracePoints(e, l, d, st.hoverTrack, f.levelDb)) pts.push_back({p.x, p.y});
        drawPolyline(c, pts, withAlpha(f.hover, 0.5), ribbonWidth(g, l, st.hoverTrack) + 1.2 * k);
        c.clip = was;
    }
    const double fr = playFrame(g, f, st);
    if (fr >= 0 && e.n > 0) {
        const int x = (int)std::lround(e.X(std::clamp(fr, 0.0, (double)e.n) + 0.5));
        fillRect(c, {x - k, stack.y, 4 * k, stack.h}, withAlpha(f.screen, 0.7));   // edged in the screen's colour, so it shows over a glow
        fillRect(c, {x, stack.y, 2 * k, stack.h}, f.cursor);
        st.drawnPlayX = x;
    }
    c.clip = saved;
}

// ---- the hooks ----------------------------------------------------------------------------------

Shared& sharedOf(Gui& g, const Hit& h) { return sharedState<Shared>(g, "fseq_edit/" + g.inst(h).key); }

void draw(Gui& g, Canvas& c, const Hit& h, Rect r) {
    Shared& st = sharedOf(g, h);
    sync(g, h, st);
    const Fields f = fieldsOf(g, h);
    const Doc& d = st.previewOn ? st.preview : st.doc;
    const Geo e = geoOf(g, h, f, st, r);
    const std::string key = cacheKeyOf(g, f, e, st, r);
    if (key != st.cacheKey || st.cache.size() != (size_t)r.w * r.h) {
        st.cache.assign((size_t)r.w * r.h, f.screen);
        Canvas cc{st.cache.data(), r.w, r.h, {0, 0, r.w, r.h}};
        Geo local = e;   // the cache is drawn at the origin
        local.r.x -= r.x, local.r.y -= r.y, local.plotX -= r.x;
        for (auto& l : local.lanes) l.r.x -= r.x, l.r.y -= r.y, l.plot.x -= r.x, l.plot.y -= r.y;
        for (auto& l : local.lanes) renderLane(cc, g, h, f, local, l, d, st);
        st.cacheKey = key;
    }
    const Rect clip = c.clip & r;
    for (int y = clip.y; y < clip.y + clip.h; ++y)
        std::memcpy(c.px + (size_t)y * c.w + clip.x, st.cache.data() + (size_t)(y - r.y) * r.w + (clip.x - r.x), (size_t)clip.w * 4);
    drawOverlay(c, g, h, f, e, st, d);
}

void gestureStart(Shared& st) { st.lastGesture = Clock::now(); st.fast = true; }

// Writes the preview of the tool's drag from the snapshot and the pointer; nothing compounds.
void applyTool(Gui& g, const Fields& f, Shared& st, const Geo& e, double fr, double curve, bool shift) {
    Gesture& ge = st.gest;
    st.preview = ge.src;
    Doc& p = st.preview;
    const int n = p.frames();
    auto spanOf = [&](int a, int b) { return std::make_pair(std::clamp(std::min(a, b), 0, n - 1), std::clamp(std::max(a, b), 0, n - 1)); };
    if (ge.tool == Draw || ge.tool == Line) {
        const int t = ge.target;
        std::vector<double> v = readTrack(p, t);
        if (ge.tool == Line) ge.path.resize(1);
        ge.path.push_back({fr, valueOf(e, t, curve)});
        for (size_t i = 0; i + 1 < ge.path.size(); ++i) {   // between two points, a value a frame
            double fa = ge.path[i].first, va = ge.path[i].second, fb = ge.path[i + 1].first, vb = ge.path[i + 1].second;
            if (fb < fa) std::swap(fa, fb), std::swap(va, vb);
            const int ia = std::max(ge.from, (int)std::lround(fa - 0.5)), ib = std::min(ge.to, (int)std::lround(fb - 0.5));
            for (int k = ia; k <= ib; ++k) {
                const double u = fb - fa > 1e-9 ? std::clamp(((k + 0.5) - fa) / (fb - fa), 0.0, 1.0) : 1;
                v[(size_t)k] = isLevel(t) ? std::clamp(va + (vb - va) * u, 0.0, 127.0) : std::clamp(va + (vb - va) * u, 0.0, (double)kMaxWord);
            }
        }
        writeTrack(p, t, v);
        markDirty(st, ge.from, ge.to);
        return;
    }
    if (ge.tool == Move || ge.tool == Scale) {
        const int t = ge.target;
        const double m0 = midiOf(ge.v0), m1 = midiOf(valueOf(e, t, curve));
        std::vector<double> v = readTrack(p, t);
        if (ge.tool == Move) {
            const double semis = std::round((m1 - m0) * (shift ? 0.1 : 1) * 100) / 100;
            for (int i = ge.from; i <= ge.to; ++i) v[(size_t)i] += semis * kWordsPerSemitone;
        } else {
            const double factor = std::clamp(std::pow(2.0, (curve - ge.c0) * 2), 0.25, 4.0);
            for (int i = ge.from; i <= ge.to; ++i) v[(size_t)i] = wordOfMidi(m0 + (midiOf(v[(size_t)i]) - m0) * factor);
        }
        writeTrack(p, t, v);
        markDirty(st, ge.from, ge.to);
        return;
    }
    if (ge.tool == Amplify) {
        const double dB = (curve - ge.c0) * 60 * (shift ? 0.1 : 1);
        for (int t : ge.tracks) {
            std::vector<double> v = readTrack(p, t);
            for (int i = ge.from; i <= ge.to; ++i)
                if (v[(size_t)i] < 127) v[(size_t)i] = std::clamp(v[(size_t)i] - dB / f.levelDb, 0.0, 127.0);
            writeTrack(p, t, v);
        }
        markDirty(st, ge.from, ge.to);
        return;
    }
    if (ge.tool == Silence) {
        auto [a, b] = spanOf((int)std::lround(ge.f0 - 0.5), (int)std::lround(fr - 0.5));
        for (int t : ge.tracks) {
            std::vector<double> v = readTrack(p, t);
            for (int i = a; i <= b; ++i) v[(size_t)i] = 127;
            writeTrack(p, t, v);
        }
        markDirty(st, a, b);
        return;
    }
    if (ge.tool == Warp || ge.tool == Stretch) {
        const double a = ge.from, b = ge.to;
        if (b - a < 1) return;
        const double g0 = std::clamp(ge.f0 - 0.5, a + 0.5, b - 0.5), g1 = std::clamp(fr - 0.5, a + 0.5, b - 0.5);
        const double factor = std::clamp((fr - 0.5 - a) / std::max(1e-9, ge.f0 - 0.5 - a), 0.1, 10.0);
        if (ge.tool == Stretch && std::fabs(ge.f0 - 0.5 - a) < 0.5) return;
        for (int t : ge.tracks) {
            const std::vector<double> src = readTrack(ge.src, t);
            std::vector<double> v = src;
            for (int i = ge.from; i <= ge.to; ++i) {
                double s;
                if (ge.tool == Warp) s = i <= g1 ? a + (i - a) * (g0 - a) / std::max(1e-9, g1 - a) : g0 + (i - g1) * (b - g0) / std::max(1e-9, b - g1);
                else s = std::min(b, a + (i - a) / factor);
                v[(size_t)i] = sampleAt(src, s);
            }
            writeTrack(p, t, v);
        }
        markDirty(st, ge.from, ge.to);
    }
}

bool down(Gui& g, const Hit& h, Rect r, int x, int y, bool) {
    Shared& st = sharedOf(g, h);
    sync(g, h, st);
    const Fields f = fieldsOf(g, h);
    const Geo e = geoOf(g, h, f, st, r);
    const Lane* l = laneAt(e, y);
    if (!l || x < e.plotX || st.doc.frames() == 0) return false;
    Gesture& ge = st.gest;
    ge = Gesture();
    ge.active = true;
    ge.lane = (int)(l - e.lanes.data());
    ge.tool = std::clamp((int)paramOr(g, f.tool, Select), 0, 9);
    ge.f0 = e.frameAt(x);
    ge.c0 = curveAt(*l, y);
    const int n = st.doc.frames(), reach = g.skin().dp(5);
    int from, to;
    selectionOrAll(st, from, to);
    ge.from = from, ge.to = to;
    if (ge.tool == Loop) {
        int k = 0, best = -1;
        for (auto& m : markersOf(f)) {
            if (m.param >= 0 && std::abs(x - (int)std::lround(e.X(std::clamp(g.state().get((size_t)m.param), 0.0, (double)(n - 1))))) <= reach) best = k;
            ++k;
        }
        if (best < 0) { ge.active = false; return false; }
        ge.marker = best;
        std::vector<int> ps;
        for (auto& m : markersOf(f)) ps.push_back(m.param);
        g.beginParams(ps);
        gestureStart(st);
        return true;
    }
    if (ge.tool >= Draw && ge.tool <= Scale) {   // the app's selected operator's track, or the nearest without one
        ge.target = f.selOp >= 0 ? selectedTrack(g, f, *l) : trackAt(e, *l, st.doc, ge.f0, ge.c0);
        if (ge.target < 0 || (ge.tool >= Move && isLevel(ge.target))) ge.tool = Select;
    }
    if (ge.tool == Select) {
        if (st.selFrom >= 0 && std::abs(x - (int)std::lround(e.X(st.selTo + 1))) <= reach) ge.edge = 1, ge.anchor = st.selFrom;
        else if (st.selFrom >= 0 && std::abs(x - (int)std::lround(e.X(st.selFrom))) <= reach) ge.edge = 0, ge.anchor = st.selTo;
        else ge.anchor = std::clamp((int)std::lround(ge.f0 - 0.5), 0, n - 1), st.selFrom = st.selTo = ge.anchor;
        g.invalidate(r);
        return true;
    }
    ge.src = st.doc;
    const int i0 = std::clamp((int)std::lround(ge.f0 - 0.5), 0, n - 1);
    if (ge.target >= 0) ge.v0 = readValue(st.doc.frame(i0), ge.target);
    if (ge.tool == Draw || ge.tool == Line) ge.path.push_back({ge.f0, valueOf(e, ge.target, ge.c0)});
    if (ge.tool == Amplify || ge.tool == Silence) {   // the lane's levels
        for (int t : l->tracks)
            if (t != kPitch) ge.tracks.push_back(isLevel(t) ? t : levelOf(t));
    } else if (ge.tool == Warp || ge.tool == Stretch) {
        ge.tracks = editTracks(g, f);
    }
    st.preview = st.doc;
    st.previewOn = true;
    ++st.previewVersion;
    gestureStart(st);
    g.invalidate(r);
    return true;
}

void drag(Gui& g, const Hit& h, Rect r, int x, int y, bool shift) {
    Shared& st = sharedOf(g, h);
    Gesture& ge = st.gest;
    if (!ge.active) return;
    const Fields f = fieldsOf(g, h);
    const Geo e = geoOf(g, h, f, st, r);
    const int n = st.doc.frames();
    const double fr = e.frameAt(x);
    st.lastGesture = Clock::now();
    if (ge.tool == Select) {
        const int at = std::clamp((int)std::lround(fr - 0.5), 0, n - 1);
        st.selFrom = std::min(ge.anchor, at), st.selTo = std::max(ge.anchor, at);
        ge.moved = true;
        g.invalidate(r);
        return;
    }
    if (ge.tool == Loop) {
        const std::vector<Marker> ms = markersOf(f);
        const int v = std::clamp((int)std::lround(fr), 0, n - 1);
        g.editParam(ms[(size_t)ge.marker].param, v);
        if (ge.marker == 0 && ms[1].param >= 0 && g.state().get((size_t)ms[1].param) < v) g.editParam(ms[1].param, v);   // a loop point pushes the other
        if (ge.marker == 1 && ms[0].param >= 0 && g.state().get((size_t)ms[0].param) > v) g.editParam(ms[0].param, v);
        g.invalidate(r);
        return;
    }
    const Lane& l = e.lanes[(size_t)std::clamp(ge.lane, 0, (int)e.lanes.size() - 1)];
    ge.moved = true;
    applyTool(g, f, st, e, fr, curveAt(l, y), shift);
    ++st.previewVersion;
    g.invalidate(r);
}

void up(Gui& g, const Hit& h, Rect r, int x, int) {
    Shared& st = sharedOf(g, h);
    Gesture& ge = st.gest;
    if (!ge.active) return;
    const Fields f = fieldsOf(g, h);
    ge.active = false;
    st.lastGesture = Clock::now();
    if (ge.tool == Select) {
        if (!ge.moved && ge.edge < 0) st.selFrom = st.selTo = -1;
        writeSelection(g, f, st);
        g.invalidate(r);
        return;
    }
    if (ge.tool == Loop) { g.invalidate(r); return; }
    const bool cancelled = x == INT_MIN / 2;
    if (st.previewOn && ge.moved && !cancelled && st.preview.bytes != st.doc.bytes) {
        pushUndo(st, st.doc, kToolNames[ge.tool]);
        st.doc = st.preview;
        ++st.docVersion;
        flush(g, f, st, st.doc);
    } else if (st.previewOn && st.previewSent) {   // a preview the engine heard, taken back
        markDirty(st, ge.from, ge.to);
        flush(g, f, st, st.doc);
    }
    st.previewOn = false;
    st.previewSent = 0;
    g.invalidate(r);
}

void dbl(Gui& g, const Hit& h, Rect r, int, int) {
    Shared& st = sharedOf(g, h);
    st.x0 = st.x1 = 0;
    g.invalidate(r);
}

void hover(Gui& g, const Hit& h, Rect r, int x, int y) {
    Shared& st = sharedOf(g, h);
    if (st.gest.active) return;
    const Fields f = fieldsOf(g, h);
    const Geo e = geoOf(g, h, f, st, r);
    const Lane* l = x == INT_MIN / 2 ? nullptr : laneAt(e, y);
    const int lane = l ? (int)(l - e.lanes.data()) : -1;
    const int tool = std::clamp((int)paramOr(g, f.tool, Select), 0, 9);
    int track = -1;
    if (l && x >= e.plotX && st.doc.frames())   // what a drag edits: the selected operator's track for a single-track tool
        track = tool >= Draw && tool <= Scale && f.selOp >= 0 ? selectedTrack(g, f, *l) : trackAt(e, *l, st.doc, e.frameAt(x), curveAt(*l, y));
    if (track != st.hoverTrack || lane != st.hoverLane) {
        if (st.hoverLane >= 0 && st.hoverLane < (int)e.lanes.size()) g.invalidate(e.lanes[(size_t)st.hoverLane].r);
        st.hoverTrack = track, st.hoverLane = lane;
        if (l) g.invalidate(l->r);
    }
}

void wheel(Gui& g, const Hit& h, Rect r, int x, int, double notches, bool shift) {
    Shared& st = sharedOf(g, h);
    const Fields f = fieldsOf(g, h);
    const Geo e = geoOf(g, h, f, st, r);
    const double span = e.x1 - e.x0;
    if (shift) setView(st, e.x0 - notches * 0.1 * span, span);
    else {
        const double fr = e.frameAt(x), span2 = span / std::pow(1.25, notches);
        setView(st, fr - (fr - e.x0) * span2 / span, span2);
    }
    gestureStart(st);
    g.invalidate(r);
}

void runCommand(Gui& g, const Hit& h, Rect r, const std::string& cmd) {
    Shared& st = sharedOf(g, h);
    const Fields f = fieldsOf(g, h);
    std::istringstream in(cmd);
    std::string op, arg;
    in >> op >> arg;
    const int n = st.doc.frames();
    if (op == "undo" || op == "redo") {
        std::vector<std::vector<uint8_t>>& from = op == "undo" ? st.undo : st.redo;
        std::vector<std::vector<uint8_t>>& to = op == "undo" ? st.redo : st.undo;
        if (from.empty()) return;
        std::vector<uint8_t> snap = std::move(from.back());
        from.pop_back();
        swapDoc(st, snap);
        to.push_back(std::move(snap));
        st.lastPath.clear();
        flush(g, f, st, st.doc);
    } else if (op == "copy") {
        copySelection(g, f, st);
    } else if (op == "cut") {
        copySelection(g, f, st);
        rangeOp(g, f, st, "silence", 0);
    } else if (op == "paste") {
        paste(g, f, st);
    } else if (op == "delete") {
        rangeOp(g, f, st, "silence", 0);
    } else if (op == "select_all") {
        if (n) st.selFrom = 0, st.selTo = n - 1;
        writeSelection(g, f, st);
    } else if (op == "select_none" || op == "cancel") {
        if (op == "cancel" && st.gest.active) { st.gest.active = false; st.previewOn = false; }
        st.selFrom = st.selTo = -1;
        writeSelection(g, f, st);
    } else if (op == "fit") {
        st.x0 = st.x1 = 0;
    } else if (op == "zoom_in" || op == "zoom_out") {
        const Geo e = geoOf(g, h, f, st, r);
        const double span = e.x1 - e.x0, span2 = op == "zoom_in" ? span / 1.25 : span * 1.25;
        setView(st, e.x0 + (span - span2) / 2, span2);
    } else if (op == "tool") {
        for (int t = 0; t < 10; ++t)
            if (arg == kToolNames[t] && f.tool >= 0) g.setParam(f.tool, t);
    } else if (op == "op") {
        rangeOp(g, f, st, arg, 0);
    } else if (op == "gain" || op == "shift" || op == "scale") {
        rangeOp(g, f, st, op, std::atof(arg.c_str()));
    } else if (op == "solo") {
        const int k = std::atoi(arg.c_str());
        if (k >= 1 && k <= 8) {
            bool alone = true;
            for (int i = 0; i < 8; ++i) alone &= flagOr(g, f.showOp[i], true) == (i == k - 1);
            for (int i = 0; i < 8; ++i)
                if (f.showOp[i] >= 0) g.setParam(f.showOp[i], alone || i == k - 1 ? 1 : 0);
        }
    } else if (op == "show_all") {
        for (int i = 0; i < 8; ++i)
            if (f.showOp[i] >= 0) g.setParam(f.showOp[i], 1);
    }
    g.invalidate(r);
}

void right(Gui& g, const Hit& h, Rect r, int x, int y, bool, bool up) {
    if (!up) return;
    const std::string style = g.wid(h).json["menuStyle"].str("plain");
    static const char* const labels[] = {"Undo", "Redo", "-", "Cut", "Copy", "Paste", "Delete", "Select All", "-", "Reverse", "Smooth", "Quantize", "Normalize",
                                         "Silence", "Fade In", "Fade Out", "Interpolate", "Ramp", "Hold", "-", "+1 dB", "-1 dB", "+1 semitone", "-1 semitone", "-", "Fit"};
    static const char* const cmds[] = {"undo", "redo", "", "cut", "copy", "paste", "delete", "select_all", "", "op reverse", "op smooth", "op quantize", "op normalize",
                                       "op silence", "op fade_in", "op fade_out", "op interpolate", "op ramp", "op hold", "", "gain 1", "gain -1", "shift 1", "shift -1", "", "fit"};
    std::vector<MenuEntry> items;
    for (size_t i = 0; i < sizeof labels / sizeof labels[0]; ++i) {
        MenuEntry m;
        m.label = labels[i];
        m.separator = m.label == "-";
        m.id = (int)i;
        items.push_back(m);
    }
    Hit held = h;
    g.openMenu(items, {x, y, 0, 0}, style, [&g, held, r](int id) {
        if (!g.live(held) || id < 0 || id >= 26 || !*cmds[id]) return;
        runCommand(g, held, r, cmds[id]);
    });
}

void tick(Gui& g, const Hit& h, Rect r) {
    Shared& st = sharedOf(g, h);
    sync(g, h, st);
    const Fields f = fieldsOf(g, h);
    readSelection(g, f, st);
    if (!f.cmdKey.empty())
        if (const std::string cmd = g.state().data(f.cmdKey); !cmd.empty()) {
            g.state().setData(f.cmdKey, "");
            runCommand(g, h, r, cmd);
        }
    if (st.gest.active && st.previewOn && st.previewVersion != st.previewSent) {   // hear the drag: its preview, once a tick
        markDirty(st, st.gest.from, st.gest.to);
        flush(g, f, st, st.preview);
        st.previewSent = st.previewVersion;
    } else if (!st.gest.active && st.dirtyHi >= st.dirtyLo) {
        flush(g, f, st, st.doc);
    } else if (st.dirtyHi < st.dirtyLo && st.sentHi >= st.sentLo && g.state().data(f.editKey).empty()) {
        st.sentLo = INT_MAX, st.sentHi = -1;   // the worker took it
    }
    if (st.fast && !st.gest.active && Clock::now() - st.lastGesture > std::chrono::milliseconds(140)) {   // full quality again
        st.fast = false;
        g.invalidate(r);
    }
    if (f.position >= 0) {   // the playhead: two thin strips, the old column and the new
        const Geo e = geoOf(g, h, f, st, r);
        const double fr = playFrame(g, f, st);
        if (fr >= 0 && e.n > 0) {
            const int x = (int)std::lround(e.X(std::clamp(fr, 0.0, (double)e.n) + 0.5)), k = g.skin().lw();
            if (x != st.drawnPlayX) {
                if (st.drawnPlayX != INT_MIN) g.invalidate({st.drawnPlayX - k - 1, r.y, 4 * k + 2, r.h});
                g.invalidate({x - k - 1, r.y, 4 * k + 2, r.h});
            }
        }
    }
    // The view's params moved under the widget (a toolbar toggle, a scale menu): the cache key sees it at the
    // next paint, so a repaint is asked for whenever one of them changed.
    std::string sig;
    for (int p : {f.tool, f.layout, f.freqScale, f.levelScale, f.showV, f.showU, f.showPitch, f.loopStart, f.loopEnd, f.start, f.end})
        sig += p >= 0 ? std::to_string(g.state().get((size_t)p)) : "-";
    for (int p : f.showOp) sig += p >= 0 ? std::to_string(g.state().get((size_t)p)) : "-";
    sig += std::to_string(st.docVersion) + "/" + std::to_string(st.selFrom) + "/" + std::to_string(st.selTo) + "/" + opSig(g, f);
    if (sig != st.paramSig) {
        st.paramSig = sig;
        g.invalidate(r);
    }
}

} // namespace

const KindOps* fseqEditKind() {
    static const KindOps ops = {draw, down, drag, up, dbl, hover, right, tick, wheel};
    return &ops;
}

} // namespace hollow

// audioToFseq: fseq-flash's analysis (FormantDetector, PitchDetector, SpectralAnalysis) with the frame bytes
// written on the engine's own scales (fs1r::Device::fseqWord, fseqLevel, fseqFrameSeconds).
#include "audio_fseq.h"
#include "fs1r.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace fsvr {
namespace {

using cd = std::complex<double>;
const double kPi = 3.14159265358979323846;

void fft(std::vector<cd>& a, bool inverse) {   // radix 2, in place; size a power of two
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2 * kPi / (double)len * (inverse ? 1 : -1);
        const cd w(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd wk(1);
            for (size_t k = 0; k < len / 2; ++k, wk *= w) {
                const cd u = a[i + k], v = a[i + k + len / 2] * wk;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

struct Frame {
    double pitch = 0, voiced = 0, rms = 0;   // f0 in Hz (0 = none found), periodicity 0..1
    double hz[8] = {}, amp[8] = {};
};

} // namespace

std::vector<uint8_t> audioToFseq(const std::vector<float>& x, double sr, const std::string& name, int& framesOut) {
    const double dur = std::max(x.size() / sr, 1e-3);
    // The fastest frame rate that still fits the sound into the format's 512 frames; past 512 of the
    // slowest the whole sound is spread over 512 (fseq-flash always spreads over 512).
    int speed = 0;
    for (int s = 127; s >= 0; --s)
        if (dur / fs1r::Device::fseqFrameSeconds(s) <= 512) { speed = s; break; }
    const int n = std::clamp((int)std::ceil(dur / fs1r::Device::fseqFrameSeconds(speed)), 1, 512);
    const double step = dur / n;

    const int M = sr > 50000 ? 4096 : 2048;   // fseq-flash's 2048 at 44.1 kHz, the same span at 96
    const int N2 = 2 * M;                     // zero padded: a finer spectrum and an unwrapped autocorrelation
    const double binHz = sr / N2;
    const int h = std::max(1, (int)std::lround(200 / binHz));   // fseq-flash's detect bandwidth, about 10 bins of 21.5 Hz
    const int lo = std::max(1, (int)(80 / binHz)), hi = std::min(M - h - 1, (int)(10000 / binHz));   // IMPORT_HIGHEST_FORMANT_FREQ
    std::vector<double> win(M);
    for (int i = 0; i < M; ++i) win[i] = 0.5 - 0.5 * std::cos(2 * kPi * i / M);
    std::vector<double> winAc(M);   // the window's own autocorrelation, which biases the signal's
    for (int t = 0; t < M; ++t) {
        double s = 0;
        for (int i = 0; i + t < M; ++i) s += win[i] * win[i + t];
        winAc[t] = s;
    }
    const int lagLo = std::max(2, (int)(sr / 1000)), lagHi = std::min(M / 2, (int)(sr / 50));

    std::vector<Frame> fr((size_t)n);
    std::vector<cd> buf((size_t)N2);
    std::vector<double> P((size_t)M + 1), E((size_t)M + 1);
    for (int k = 0; k < n; ++k) {
        Frame& f = fr[(size_t)k];
        const long c = (long)((k + 0.5) * step * sr) - M / 2;
        double e = 0;
        for (int i = 0; i < N2; ++i) {
            const long at = c + i;
            const double s = i < M && at >= 0 && at < (long)x.size() ? x[(size_t)at] * win[i] : 0;
            buf[(size_t)i] = s;
            e += s * s;
        }
        f.rms = std::sqrt(e / M);
        fft(buf, false);
        for (int b = 0; b <= M; ++b) P[(size_t)b] = std::norm(buf[(size_t)b]);
        // Pitch: the autocorrelation, unbiased by the window's, its strongest peak between 50 Hz and 1 kHz;
        // the first peak within 0.9 of it wins, which keeps an octave below off (PitchDetector's comb).
        for (int b = 0; b < N2; ++b) buf[(size_t)b] = std::norm(buf[(size_t)b]);
        fft(buf, true);
        const double r0 = buf[0].real();
        if (r0 > 1e-12) {
            auto r = [&](int t) { return buf[(size_t)t].real() / r0 / (winAc[t] / winAc[0]); };
            double best = 0;
            for (int t = lagLo; t <= lagHi; ++t) best = std::max(best, r(t));
            for (int t = lagLo + 1; t < lagHi; ++t) {
                const double a = r(t - 1), b = r(t), d = r(t + 1);
                if (b >= a && b >= d && b >= 0.9 * best) {
                    const double den = a - 2 * b + d, off = den < 0 ? 0.5 * (a - d) / den : 0;
                    f.pitch = sr / (t + off);
                    f.voiced = std::clamp(b, 0.0, 1.0);
                    break;
                }
            }
        }
        // Formants: the spectrum's log power smoothed over the detect bandwidth, its eight strongest
        // peaks at least a bandwidth apart (FORMANT_DETECT_DISALLOW_NEIGHBORS), each at its power's
        // centre, as FormantDetector's smoothed method does.
        for (int b = 0; b <= M; ++b) {
            double s = 0, w = 0;
            for (int j = -h; j <= h; ++j) {
                const int q = std::clamp(b + j, 0, M);
                const double tw = h + 1 - std::abs(j);
                s += tw * std::log(P[(size_t)q] + 1e-20);
                w += tw;
            }
            E[(size_t)b] = s / w;
        }
        std::vector<bool> ok((size_t)M + 1, false);
        for (int b = lo; b <= hi; ++b) ok[(size_t)b] = true;
        int picks[8], got = 0;
        for (; got < 8; ++got) {
            int best = -1;
            for (int b = lo; b <= hi; ++b)
                if (ok[(size_t)b] && (best < 0 || E[(size_t)b] > E[(size_t)best])) best = b;
            if (best < 0) break;
            picks[got] = best;
            for (int j = std::max(0, best - h); j <= std::min(M, best + h); ++j) ok[(size_t)j] = false;
        }
        std::sort(picks, picks + got);
        for (int i = 0; i < got; ++i) {
            double pw = 0, fw = 0;
            for (int j = std::max(0, picks[i] - h); j <= std::min(M, picks[i] + h); ++j) {
                pw += P[(size_t)j];
                fw += P[(size_t)j] * j * binHz;
            }
            f.hz[i] = pw > 0 ? fw / pw : picks[i] * binHz;
            f.amp[i] = std::sqrt(pw);
        }
        for (int i = got; i < 8; ++i) f.hz[i] = (i + 1) * 1000.0;   // a band with nothing in it stays silent
    }

    // Levels relative to the loudest formant of the whole sound, so the sequence keeps the sound's dynamics.
    double top = 0, loud = 0;
    for (auto& f : fr) {
        loud = std::max(loud, f.rms);
        for (double a : f.amp) top = std::max(top, a);
    }
    std::vector<double> voicedPitch;
    for (auto& f : fr)
        if (f.voiced >= 0.5 && f.pitch > 0) voicedPitch.push_back(f.pitch);
    std::sort(voicedPitch.begin(), voicedPitch.end());
    const double median = voicedPitch.empty() ? 261.63 : voicedPitch[voicedPitch.size() / 2];
    const int note = std::clamp((int)std::lround(69 + 12 * std::log2(median / 440.0)), 0, 127);

    const int total = 128 * ((n + 127) / 128);
    std::vector<uint8_t> d(32 + (size_t)total * 50, 0);
    for (int i = 0; i < 8; ++i) {
        const unsigned char ch = i < (int)name.size() ? (unsigned char)name[(size_t)i] : ' ';
        d[(size_t)i] = ch >= 32 && ch < 127 ? ch : '_';
    }
    auto pair = [&](size_t at, int v) { d[at] = (uint8_t)(v >> 7 & 0x7F); d[at + 1] = (uint8_t)(v & 0x7F); };
    pair(0x10, 0);          // loop start
    pair(0x12, n - 1);      // loop end
    d[0x15] = (uint8_t)speed;
    d[0x18] = (uint8_t)note;   // the key that plays the sound at its own pitch
    d[0x19] = 63;              // tuning centred
    d[0x1B] = (uint8_t)(total / 128 - 1);
    pair(0x1E, n - 1);      // end step
    double pitch = median;
    for (int k = 0; k < total; ++k) {
        const Frame& f = fr[(size_t)std::min(k, n - 1)];
        if (f.voiced >= 0.5 && f.pitch > 0) pitch = f.pitch;   // an unvoiced frame holds the last pitch
        uint8_t* q = d.data() + 32 + (size_t)k * 50;
        const int pw = fs1r::Device::fseqWord(pitch);
        q[0] = (uint8_t)(pw >> 8);
        q[1] = (uint8_t)((pw & 0xFF) >> 1);
        const bool silent = f.rms < loud * 1e-3;   // 60 dB under the loudest frame
        for (int i = 0; i < 8; ++i) {
            const int w = fs1r::Device::fseqWord(f.hz[i]);
            const double a = top > 0 && !silent ? f.amp[i] / top : 0;
            q[2 + i] = q[0x1A + i] = (uint8_t)(w >> 8);
            q[0x0A + i] = q[0x22 + i] = (uint8_t)((w & 0xFF) >> 1);
            q[0x12 + i] = (uint8_t)fs1r::Device::fseqLevel(a * f.voiced);
            q[0x2A + i] = (uint8_t)fs1r::Device::fseqLevel(a * (1 - f.voiced));
        }
    }
    framesOut = n;
    return d;
}

} // namespace fsvr

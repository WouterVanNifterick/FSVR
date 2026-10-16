// dsp_bench - the filters' and effects' cost, and their output as a bit-exact reference.
//
//   build.bat bench                       builds bin\dsp_bench.exe with the vop3host flags
//   cmake --build build --target dsp_bench   the same with CMake (x86-64)
//   dsp_bench [out.raw]                   10 s at 48 kHz, ns per sample
//
// Effects runs Hall 1 and Chorus, Filter an LPF24 voice on four slots, all fed the same fixed noise with
// an impulse every second. out.raw (default build\dsp_bench.raw) holds every sample's effect pair and
// sixteen filter channels as doubles: two builds that write the same bytes compute the same thing.
#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <cstdio>
#include <cstring>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include <memory>
#include <vector>
#include "effects/effects.h"
#include "filters/filter.h"

namespace H = fsvr::vop3host;

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "build\\dsp_bench.raw";
    const int SAMPLES = 480000, OUT = 18;

    auto fx = std::make_unique<Effects>();
    uint8_t c[H::COMMON_BYTES] = {};
    c[0x10] = 127; c[0x11] = 64; c[0x12] = 24;   // volume, pan and the rest as init_perf has them
    c[H::EQ_LOW_GAIN] = c[H::EQ_MID_GAIN] = c[H::EQ_HIGH_GAIN] = 64;
    c[H::REVERB_PAN] = c[H::VARIATION_PAN] = 64; c[H::REVERB_RETURN] = c[H::VARIATION_RETURN] = 100;
    fx->load(c);
    fx->host.setByte(H::REVERB_TYPE, 1);       // Hall 1
    fx->host.setByte(H::VARIATION_TYPE, 1);    // Chorus

    auto flt = std::make_unique<Filter::Bank>();
    { using namespace H::filter;
      uint8_t v[H::FilterHost::VOICE_BYTES] = {}, r[H::FilterHost::PART_BYTES] = {};
      for (int i = 0x18; i <= 0x23; i++) r[i] = 64;
      r[PART_LFO2_SPEED] = r[PART_LFO2_DEPTH] = 64;
      v[FILTER_TYPE] = LPF24; v[CUTOFF] = 64; v[RESONANCE] = 76; v[KEY_SCALE_DEPTH] = 64; v[KEY_SCALE_POINT] = 60;
      v[INPUT_GAIN] = 12; v[EG_DEPTH] = 64;
      for (int i = EG_L4; i <= EG_L3; i++) v[i] = 50;
      flt->sync(0, v, r, 0, 0, 0);
      for (int s : {0, 3, 7, 12}) flt->host.noteOn(s, 0, 48 + s, 100); }

    std::vector<double> out((size_t)SAMPLES * OUT);
    uint32_t seed = 12345;
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t c0 = __rdtsc();
    for (int i = 0; i < SAMPLES; i++) {
        seed = seed * 1664525u + 1013904223u;
        const double x = (int32_t)seed * (0.2 / 2147483648.0) + (i % 48000 == 0 ? 0.9 : 0.0);
        double* o = &out[(size_t)i * OUT];
        const double bus[8] = {x, -x, 0, 0, x, x, x, -x};   // dry, insertion, variation send, reverb send
        fx->run(bus, o[0], o[1]);
        for (int n = 0; n < 16; n++) o[2 + n] = x;
        flt->run(o + 2);
    }
    const uint64_t cycles = __rdtsc() - c0;
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();

    std::printf("%.1f ns/sample  %.0f TSC cycles/sample  %.0f%% of a core at 48 kHz\n", ns / SAMPLES, double(cycles) / SAMPLES, ns / 1e8);
    FILE* f = std::fopen(path, "wb");
    if (!f || std::fwrite(out.data(), sizeof(double), out.size(), f) != out.size()) { std::perror(path); return 1; }
    std::fclose(f);
    uint64_t h = 1469598103934665603ull;
    const unsigned char* b = (const unsigned char*)out.data();
    for (size_t i = 0; i < out.size() * sizeof(double); i++) h = (h ^ b[i]) * 1099511628211ull;
    std::printf("%s  fnv1a %016llx\n", path, (unsigned long long)h);
    return 0;
}

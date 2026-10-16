// the engine's DAC out (f32 stereo, stdout) for a performance with step / constant patches, a 1 kHz sine
// into one bus.  usage: patched <perf.bin> <bus 1=ins 2=var> <secs> [st:w0,w1,w2,w3,w4[:coef]]...  (hex)
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "effects/effects.h"
int main(int argc, char** argv) {
    std::vector<uint8_t> p(400); FILE* f = fopen(argv[1], "rb"); fread(p.data(), 1, 400, f); fclose(f);
    int bus = atoi(argv[2]); double secs = atof(argv[3]);
    Effects fx; fx.load(p.data()); fx.host.runPosted();
    yss236_core& c = fx.chip.core;
    for (int a = 4; a < argc; a++) {
        char* s = argv[a]; int st = strtol(s, &s, 16); s++;
        for (int i = 0; i < 5; i++) { c.prog[st][i] = (uint16_t)strtol(s, &s, 16); if (*s == ',') s++; }
        if (*s == ':') c.coef[st] = (uint16_t)strtol(s + 1, &s, 16);
        c.decode(st);
    }
    for (int n = 0; n < (int)(secs * 48000); n++) {
        double b[8] = {}, l, r, x = 0.25 * std::sin(2 * M_PI * 1000.0 * n / 48000.0);
        b[2 * bus] = b[2 * bus + 1] = x;
        fx.run(b, l, r); float o[2] = {(float)l, (float)r}; fwrite(o, 4, 2, stdout);
    }
}

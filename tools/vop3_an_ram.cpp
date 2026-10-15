// vop3_an_ram - run a PLG150-AN or AN1x VOP3 program through yss236_core and report where its RAM reads land.
//
// Driven by tools/vop3_an_ram.py --probe over stdin:
//   ram <hex word> ...                       the RAM image (unused here beyond its length: the probe fills
//                                            ram[i] = i so the value a read returns is its address)
//   step <st> <r10> <r9> <r8> <r7> <r6> <k> <offset>   one step, hex; offset is the slot's 18-bit RAM offset
//   run <passes>                             run and print "read <st> <address> <ram word>" per transfer
// Built twice by the script: as is (the FS1R's delay ring, address = pointer + offset) and with
// -DYSS236_TABLE_ADDR (the candidate absolute addressing of w2[5] accesses, docs/vop3_an_ram.md).
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#define private public   // the probe steps the core itself to read each pass's transfers before pass() drops them
#include "fs1r/chips/yss236_core.h"
#undef private

// The probe steps the interpreter itself, so it links no compiler.
bool yss236_core::jit_pass() { return false; }
void yss236_core::jit_free::operator()(jit_code *) const {}
const char *const yss236_core::jit_target = nullptr;

int main() {
    yss236_core c;
    c.interpret = true;
    std::vector<unsigned> image;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd >> std::hex;
        if (cmd == "ram") {
            unsigned w;
            while (in >> w) image.push_back(w);
        } else if (cmd == "step") {
            unsigned st, r10, r9, r8, r7, r6, k, off;
            in >> st >> r10 >> r9 >> r8 >> r7 >> r6 >> k >> off;
            uint16_t w[5] = {uint16_t(r10), uint16_t(r9), uint16_t(r8), uint16_t(r7), uint16_t(r6)};
            for (int i = 0; i < 5; i++) c.prog[st][i] = w[i];
            c.coef[st] = uint16_t(k);
            if (r10 & 7) { c.slot[st >> 2][0] = uint16_t(off >> 9); c.slot[st >> 2][1] = uint16_t(off & 0x1ff); }
        } else if (cmd == "run") {
            int passes;
            in >> std::dec >> passes;
            c.decode_all();
            c.reset();
            for (int i = 0; i < yss236_core::RAM_WORDS; i++) c.ram[i] = i;
            for (int p = 0; p < passes; p++) {
                // a slow two-voice ramp on every serial input so the programs compute moving indices
                for (int s = 0; s < 4; s++)
                    for (int q = 0; q < yss236_core::SLOTS_IO; q++) c.in[s][q] = ((p * (q + 1)) % 4800) / 2400.0 - 1.0;
                c.m_skip = 0;
                for (int st = 0; st < yss236_core::STEPS; st++) c.step(st);
                for (int h = 0; h < c.m_nhist; h++) {
                    unsigned a = unsigned(c.m_hist[h].v);
                    std::printf("read %x %x %x\n", c.m_hist[h].st, a, a < image.size() ? image[a] : 0);
                }
                c.m_nhist = 0;
                c.ptr = (c.ptr - 1) & (yss236_core::RAM_WORDS - 1);
            }
        }
    }
    return 0;
}

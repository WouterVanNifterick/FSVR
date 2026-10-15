// fs1r/native/native.h - the VOP3 chips' programs written in C++. Ours.
//
// One function per chip program, ported from the microcode the firmware uploads. Each runs a range of its
// program's steps on the core's own state exactly as yss236_core::step would, bit for bit, so it can stand
// in for the interpreter one range at a time (yss236_core::natives) and be checked against it
// (tools/vop3_native_check). When every program has one, the interpreter and the JIT go.
// docs/vop3_native/README.md is the brief, docs/vop3_native/status.json the tracker.
#pragma once
#include "fs1r/chips/yss236_core.h"

namespace fs1r::native {

using Run = void (*)(yss236_core& core, int from, int to);

// A ported program, by its key in the tracker:
//   flt                       the filter chip's program, all 512 steps
//   base                      the effect chip's base program, outside the reverb's steps
//   rev                       the reverb's steps of the base program
//   aux                       the aux window's program
//   ins-mNN, dly-mNN, var-mNN the insertion, reverb-delay and variation windows' program for type msb NN
struct Program { const char* key; Run run; };
extern const Program PROGRAMS[];                 // ends with {nullptr, nullptr}
const Program* find(const char* key);            // null until the program is ported

// The ported programs, one line each in key order, as `void ins_m03(yss236_core&, int from, int to);`
void flt(yss236_core& c, int from, int to);         // flt: the filter chip's program, all 512 steps

}  // namespace fs1r::native

// fs1r/native/native.h - the VOP3 chips' programs written in C++. Ours.
//
// One function per chip program, ported from the microcode the firmware uploads. Each runs a range of its
// program's steps on the core's own state exactly as yss236_core::step would, bit for bit, so it can stand
// in for the interpreter one range at a time (yss236_core::natives) and be checked against it
// (tools/vop3_native_check).
// docs/vop3_native/README.md is the brief, docs/vop3_native/status.json the tracker.
#pragma once
#include "fs1r/chips/yss236_core.h"

namespace fs1r::native {

using Run = void (*)(yss236_core& core, int from, int to);

// Which of the programs below to run on the effect chip, from the words its registers hold. A window running
// a program we did not port gets no range and runs on the interpreter, which gives the same answer more
// slowly. Call it after anything that writes a program word (Vop3::programDirty says when that was).
// The voice filter is not a chip program here at all: fs1r/native/voice_filter.h.
void install_effect(yss236_core& core);

// A ported program, by its key in the tracker:
//   base                      the effect chip's base program, outside the reverb's steps
//   rev                       the reverb's steps of the base program
//   aux                       the aux window's program
//   ins-mNN, dly-mNN, var-mNN the insertion, reverb-delay and variation windows' program for type msb NN
struct Program { const char* key; Run run; };
extern const Program PROGRAMS[];                 // ends with {nullptr, nullptr}
const Program* find(const char* key);            // null until the program is ported

// The ported programs, one line each in key order, as `void ins_m03(yss236_core&, int from, int to);`
void aux(yss236_core& c, int from, int to);         // aux: the aux window's program, its 40 steps
void base(yss236_core& c, int from, int to);         // base: the effect chip's base program, its 138 steps
void dly_m00(yss236_core& c, int from, int to);     // dly-m00: the reverb delay window's program for type msb 00
void dly_m08(yss236_core& c, int from, int to);     // dly-m08: the reverb delay window's program for type msb 08
void ins_m00(yss236_core& c, int from, int to);     // ins-m00: the insertion window's program for type msb 00
void ins_m01(yss236_core& c, int from, int to);     // ins-m01: the insertion window's program for type msb 01
void ins_m02(yss236_core& c, int from, int to);     // ins-m02: the insertion window's program for type msb 02
void ins_m03(yss236_core& c, int from, int to);     // ins-m03: the insertion window's program for type msb 03
void ins_m04(yss236_core& c, int from, int to);     // ins-m04: the insertion window's program for type msb 04
void ins_m05(yss236_core& c, int from, int to);     // ins-m05: the insertion window's program for type msb 05
void ins_m07(yss236_core& c, int from, int to);     // ins-m07: the insertion window's program for type msb 07
void ins_m10(yss236_core& c, int from, int to);     // ins-m10: the insertion window's program for type msb 10
void ins_m12(yss236_core& c, int from, int to);     // ins-m12: the insertion window's program for type msb 12
void ins_m13(yss236_core& c, int from, int to);     // ins-m13: the insertion window's program for type msb 13
void rev(yss236_core& c, int from, int to);         // rev: the reverb's steps of the base program
void var_m00(yss236_core& c, int from, int to);     // var-m00: the variation window's program for type msb 00
void var_m01(yss236_core& c, int from, int to);     // var-m01: the variation window's program for type msb 01
void var_m02(yss236_core& c, int from, int to);     // var-m02: the variation window's program for type msb 02
void var_m03(yss236_core& c, int from, int to);     // var-m03: the variation window's program for type msb 03
void var_m04(yss236_core& c, int from, int to);     // var-m04: the variation window's program for type msb 04
void var_m05(yss236_core& c, int from, int to);     // var-m05: the variation window's program for type msb 05
void var_m06(yss236_core& c, int from, int to);     // var-m06: the variation window's program for type msb 06
void var_m07(yss236_core& c, int from, int to);     // var-m07: the variation window's program for type msb 07
void var_m08(yss236_core& c, int from, int to);     // var-m08: the variation window's program for type msb 08
void var_m09(yss236_core& c, int from, int to);     // var-m09: the variation window's program for type msb 09
void var_m10(yss236_core& c, int from, int to);     // var-m10: the variation window's program for type msb 10

}  // namespace fs1r::native

// engine/scales.h - the firmware's byte-to-register scales. KNOWN: each cites its FUN_.
#pragma once
#include <algorithm>
#include "fs1r/tables.h"
#include "math/fastmath.h"

static inline int eb86(int v) { return std::min(255, (((v & 0xFF) << 1) * 0xA5) >> 7); }   // 0..99 -> 0..127
static inline int eb70(int v) { return (((v & 0xFF) << 1) * 0xA5) >> 8; }                   // 0..99 -> 0..127 (bandwidth)
// Pan key scaling, MEASURED off 10_envelope2 on 2026-09-19. Every request file leaves the performance's
// PAN SCALING byte at 0, the extreme, so the recording sweeps the pan right across the keyboard and reads
// the law out: the index is 64 - 4 * (note - 60) / 3, hard right at note 12 and hard left from note 108
// up, which matches all ten notes and both ends of the table. That is 64 index steps per 48 semitones at
// byte 0, where the engine had 50, so every pan-scaled patch was 28 % shy of the unit. In the firmware's
// own 0..255 pan domain (docs/ymp706_registers.md, Pan) a byte of 0 is the whole 128 of one side.
static inline int pan_index(int base, int scaling, int note) {
    return base + ((clampi(scaling, 0, 100) - 50) * 64 / 50) * (note - 60) / 48;
}
static inline int keygroup(int n) { return std::max(0, (KEYFACT[clampi(n, 0, 127)] >> 2) - 3); }
// FUN_00010d7a: key tracking of fixed/formant frequencies, notescale 0..99, pm = pitch word - C3
static inline int keytrack(int ns, int pm) { int k = eb70(ns); return k ? ((k + 1) * pm) / 128 : 0; }
// FUN_00013596: frequency velocity sensitivity, b = raw sysex nibble (sens + 7)
static inline int fvs_term(int b, int vel) {
    b &= 0xF;
    int m = (b & 8) ? FVSTAB[1 + (b & 7)] : FVSTAB[7 - (b & 7)];
    int v = (vel - 64) * m; if (!(b & 8)) v = -v;
    return v >> 3;
}
// FUN_00012a32: velocity attenuation, b = raw sysex nibble (sens + 7), 0.375 dB units
static inline int vel_att(int b, int vel) {
    b &= 0xF; int s, t;
    if (b & 8) { s = (b & 7) + 1; t = VELW[vel]; } else { s = 7 - (b & 7); t = VELW[127 - vel]; }
    return std::min(255, (15 - 2 * s) + ((s * 32 * t) >> 8));
}

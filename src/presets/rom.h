// presets/rom.h - the EPROM, its banks, and sysex and parameter loading. KNOWN.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "presets/voice.h"

struct Synth;
struct Rom { std::vector<uint8_t> d; bool ok = false; };
bool load_rom(Rom& R, const char* path);
void rom_voice(const Rom& R, int idx, Voice& V);
int bank_voice_index(int bank, int prog);
void perf_from_bytes(Synth& S, const Rom* R, const uint8_t* d);
bool rom_perf(Synth& S, const Rom& R, int idx);
bool rom_ready(const Rom* R);
bool rom_fseq(Synth& S, const Rom& R, int n);
bool load_sysex(Synth& S, const Rom* R, const uint8_t* d, size_t len, int pick, int part);
bool param_is_wide(int ah, int am, int al);
bool write_param(Synth& S, int ah, int am, int al, int val);
bool apply_param_change_locked(Synth& S, const uint8_t* d, size_t len);
bool apply_param_change(Synth& S, const uint8_t* d, size_t len);

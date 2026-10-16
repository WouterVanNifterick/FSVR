# src/effects: the effects

The FS1R's effect section: reverb, variation, insertion and the mixer, as plain DSP functions over one shared state, plus the firmware's effect host that programs them. Bit for bit what the firmware's effect programs compute.

| File | What |
|---|---|
| `effects.h`, `.cpp` | `Effects`: the section. Runs the host, its LFOs and the five loaded programs once per sample |
| `programs.h` | `State` (registers, cells, delay memory) and the program list. Start here |
| `base.cpp` | the base program: mixer, sends and returns, master EQ, the reverb |
| `aux.cpp` | the send-level smoothers. Hand-written, the style the rest should move towards |
| `dly_mNN.cpp` | the reverb delay window's programs |
| `var_mNN.cpp` | the variation window's programs (`var_m07.cpp` the 3-band EQ, hand-written) |
| `ins_mNN.cpp` | the insertion window's programs |
| `host/` | the firmware's effect host (KNOWN, cites `FUN_`): performance bytes into parameters, LFOs and coefficients. `effect_host.h` is its interface, `fx_*.cpp` one file per effect family |

Each program file's header names the effect types that load it. `NN` is the firmware's program number in its window; the `*_m11`/`*_m14` programs are the clear programs that silence a window.

## Rules
- Bit-exact: built without FMA contraction. `dsp_check` pins the output; ctest runs it. A readability rewrite of a program must leave `dsp_check` and the bench hash unchanged.
- The host talks to the section only through `dsp/sink.h`.

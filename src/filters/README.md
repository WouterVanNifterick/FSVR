# src/filters: the voice filters

The FS1R's per-voice resonant filter: one 4-pole ladder per tone-generator channel with LP24/LP18/LP12, HP, BP and notch taps, plus the firmware's filter host that programs it. Bit for bit what the firmware's filter program computes; the unit has 16 filtered voices, FSVR can run 32 (`docs/Differences.md`).

| File | What |
|---|---|
| `voice_filter.h` | `VoiceFilter`: one voice's ladder, its coefficients and taps. Start here |
| `filter.h`, `.cpp` | `Filter`: 32 voices in two banks of 16, each with its filter host and `Modulators`; turns the host's writes into voice coefficients |
| `filter_host.h`, `.cpp` | the firmware's filter host (KNOWN, cites `FUN_`): the part's filter parameters into register writes |

## Rules
- Bit-exact: built without FMA contraction (`CMakeLists.txt`, the `vop3host` library). `dsp_check` pins the output; ctest runs it.
- The host talks to the section only through `dsp/sink.h`.

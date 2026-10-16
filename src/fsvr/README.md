# src/fsvr: FSVR's own

Everything with no hardware counterpart: the public device, speed, tooling. **Ours**: the hardware has no opinion; a change here must not move a measurement.

| File | What |
|---|---|
| `device.cpp` | `fs1r::Device` (declared in `src/fs1r.h`): host-rate resampling, state as bulk dumps, the display helpers |
| `fastmath.h`, `.cpp` | the per-sample sine, 2^x and dB: LUT (default), CORDIC, hybrid or libm, chosen by `FSVR_MATH`; `clampi` and `db2lin` |
| `tuning.h` | cost knobs: control-rate decimation, queue caps. Changing one must not change the output |
| `selftest.cpp` | `selftest()`, run by `engine_selftest` and ctest: the engine's own invariants and pinned hashes |
| `display.h` | how the FS1R shows values (Hz, ms, names), transcribed from K_Take's editor. Nothing in the engine reads it |
| `egview.h` | an envelope as a drawing, for the editor |
| `smf.h` | Standard MIDI File reading, offline rendering and WAV writing, for the tools and the console |

# src/math: the sample loop's maths

**Ours**: the hardware has no opinion. A backend trades CPU for nothing else, so one that moves a measurement is wrong.

| File | What |
|---|---|
| `fastmath.h` | `fm::sin_turns`, `fm::exp2`, `fm::db2lin`: LUT (default), CORDIC, hybrid or libm, chosen by `FSVR_MATH`; plus `clampi` and the engine's `db2lin` |
| `fastmath.cpp` | the tables, the CORDIC rotations, and `fm::selfcheck`, which holds each backend to libm and the build to no fused multiply-add |
| `floor.h` | `fm::floor_exact`: `std::floor` where the build makes it one instruction, the rounding instruction behind a runtime SSE4.1 test on the x64 baseline, and an exact branchless cut through an integer elsewhere; every floor on the sample path, the DSP's `word.h` included, goes through it |

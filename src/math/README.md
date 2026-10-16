# src/math: the sample loop's maths

**Ours**: the hardware has no opinion. A backend trades CPU for nothing else, so one that moves a measurement is wrong.

| File | What |
|---|---|
| `fastmath.h` | `fm::sin_turns`, `fm::exp2`, `fm::db2lin`: LUT (default), CORDIC, hybrid or libm, chosen by `FSVR_MATH`; plus `clampi` and the engine's `db2lin` |
| `fastmath.cpp` | the tables, the CORDIC rotations, and `fm::selfcheck`, which holds each backend to libm |

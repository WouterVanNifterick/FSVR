# CPU cost, and why A020 Vox Morph is the worst case

A user report: "A020 Vox Morph is very CPU hungry. With 5 to 8 voices it brings my Intel i7 to the limit" (AU build). This is what the engine does, measured, and what would make it cheaper. Nothing here is a fidelity question: the constants in `cal.h` are not involved, and every option below has to leave the output bit-identical or it is not an optimisation.

## What the patch asks for

A020 is two layered parts of PrB 097 `SpacySweep` (algorithm 8) on the performance channel, so every key is two channels. Each channel runs five voiced sine operators (three sit at output level 0) and **all eight unvoiced operators** at levels 39 to 71 with their own EGs. The unvoiced path is a noise generator, two one-poles and a resonance carrier per operator per sample, and no other kind of preset uses eight of them at once. Eight held keys is 16 channels × (8 voiced + 8 unvoiced) = 256 operator paths per sample at 48 kHz. On the hardware this costs nothing extra: the YMP706 runs every slot of every channel whether it sounds or not.

## Measured

`build/perfbench.cpp` (not committed: a twenty-line harness that loads a ROM performance through `Synth`, holds N notes and times `render` over four seconds) on one core of a Ryzen 9 3900X, gcc `-O2`, denormals flushed as the plugin does:

| performance | 8 held notes | channels | load, one core |
|---|---|---|---|
| A001 Zap ! | | 1 | 4 % |
| B014 Full Tines | | 16 | 41 % |
| A014 Homy | | 16 | 25 % |
| A020 Vox Morph | | 16 | 53 % |

A laptop i7 core is one and a half to two times slower than this, and a DAW shares it, so "5 to 8 voices hits the limit" is the engine as built, not the AU wrapper. The plugin does flush denormals (`NoDenormals` around `process` in `plugin/plugin.cpp`, FTZ and DAZ on x86, FZ on ARM); without it Vox Morph reads 120 % on the same core, which is the number anyone benchmarking the console or `render_capture` sees, since those do not set FTZ.

The profile (`-fno-inline -pg`) is flat: `fsin` 14 %, `EG::tick` 10 %, the unvoiced block about 15 %, `db2lin_fast` 5 %, the rest spread across `render_chan`. There is no single hot spot; it is the count of operator paths.

## What would make it cheaper

In order of cost to do, each with the measurement that says what it buys:

1. **Gate at −90 dB, not −100.** `render_chan` skips an operator whose EG-minus-attenuation is under −100 dB. An operator at output level 0 sits at exactly −96 dB (register 255 × 0.376 dB), so it never gates out: Vox Morph renders three silent voiced operators per channel forever. Raising the gate to −90 dB takes 8 notes from 53 % to 46 %. −96 dB is below the 16-bit floor after `OUT_GAIN`, so the render is unchanged, but this needs the capture bench and the demo ledger run before it ships, like any edit in `ymp706.cpp`. One constant.
2. **Skip an unvoiced operator whose EG has finished.** `ueg.done()` is already tracked for the channel's liveness; the per-sample block does not test it. Vox Morph's unvoiced EGs decay to L4 = 0, so after the decay every one of its 128 noise paths is computing silence. Not measured yet; likely the larger win on this patch after the notes have been held a second.
3. **`-O3`.** 128 % → 93 % at `-O2` → `-O3` without FTZ on the bench. The release builds at `-O2` (CMake Release). Free, but check the regression renders hash-identical first, since the compiler may reassociate the EG maths.
4. **Vectorise the eight operators.** The operator loop is written per operator with a scalar `fsin` table read. Eight lanes of phase, level and EG would map onto SSE/NEON, but the algorithm routing (`Cb`, `H`, `S`, feedback) is serial between operators, so only the EG and level path vectorises cleanly. The real change, and the last one worth doing.

`-ffast-math` reads 33 % on the same bench, but that number is mostly FTZ, which the plugin already has, plus reassociation that changes the output; it is not an option.

## What not to do

Do not touch `tuning::CTL_DECIMATION` to chase this: its rule is that changing it must not change the output, and at 16 the control maths is already well under the operator loop in the profile. Do not lower the voiced gate under the 16-bit floor to chase the number further, and do not skip operators by their *level byte*: a level-0 operator can still be a modulator with a control set or an Fseq track driving it.

## The sample loop's maths: table, CORDIC, or both (2026-09-28)

Everything the per-sample path asks of libm (the sine by phase, 2^x for the pitch EGs and the effect sweeps, 10^(dB/20) for the level path, tanh in the enhancer) now goes through `src/fsvr/fastmath.h`, and `FSVR_MATH` picks how it is computed: `0` raw libm, `1` a table with linear interpolation, `2` CORDIC in Q40 fixed point, `3` a hybrid where the table seeds a short CORDIC rotation over the residual. `set FSVR_MATH=n` before `build.bat`, or `-DFSVR_MATH=RAW|LUT|CORDIC|HYBRID` to CMake. The self check prints which one is built and its worst error against libm over a sweep of each function; all four pass the nineteen render cases against a fingerprint of the tree before the change.

Thirty seconds of one note, `bin/fsvr_console.exe -r ROM -P n -n 60 -d 30`, best of three on the desktop, MSVC `/O2`, no FTZ:

| backend | A020 Vox Morph (`-P 19`) | `-P 24`, regress.py's perf-everybody | worst error vs libm |
|---|---|---|---|
| 1 LUT (default) | 1.86 s | 1.75 s | 3e-7 (sin), 7e-6 relative (dB table) |
| 0 raw libm | 2.79 s | 3.18 s | 0 |
| 3 hybrid | 6.77 s | 8.19 s | 1.2e-7 |
| 2 CORDIC | 8.96 s | 10.98 s | 5e-10 |

The table wins by a wide margin and is the default. One trap on the way: the table's 2^x first scaled its result with `ldexp`, which is a CRT call on MSVC and cost more than the `pow` it replaced; it now writes the exponent field of the double directly, which is exact. A CORDIC step is a serial chain of a shift, an xor and two adds on x, y and z, and even branchless and cut to the fifteen steps the residual needs, fifteen of them cost more than the two loads and one multiply of a table read on any core with a fast FPU; the branchy first version was worse again (9.1 s and 11.3 s on the hybrid) because the sign of the residual mispredicts half the time. The hybrid is more accurate than the table and independent of table size, which is what it would be for on a target without that FPU, and it is kept for that. Raw libm is kept as the reference the self check measures against and for A/B renders; it is a sin() per operator per sample and was never what the engine shipped with.

The grain windows (`g_win`) are not part of this. They are a modelled waveform read by phase, not a function of x that another method could compute, and they stay a table on every backend.

## Fat Line: 32 channels alive for a one-note line (2026-09-28)

jameshansen: the Fat Line demo sits at about 50 % of a core. Measured with a counting build of the engine (`__rdtsc` around each section, counters per sample) on the demo's own song, 27.9 s, plain MSVC `/O2` build:

| | before | after |
|---|---|---|
| render time | 12.0 s (43 % of a core) | 6.1 s (22 %) |
| channels rendering per sample | 28.5 of 32 | 28.5, of which 14.8 silent and free |
| voiced operators working per sample | 100.5 | 70.9 |
| operator slots only ticking an EG per sample | 127.9 | 39.1 |
| refresh_ctl share of the render | 25 % | 12 % |

**What the song does.** Parts 1 and 2 are DigiSQ2, whose operators 3 and 4 have a release time of 99, the slowest setting. After every 70 ms note of the bass line they hold near 0 dB as modulators for minutes, so a channel is never finished: the engine never freed one during the song, and 28.5 channels rendered on average for a line that holds one note at a time. Their carriers were done within a second (release times 28 to 43), their unvoiced operators sit at level 0 with the EG held at full, so each channel ran 8 voiced and 8 unvoiced EGs, the control refresh and the operator loop for nothing. Parts 3 and 4 alone render in 0.36 s; the effects are 3 % and the filters 5 %. The unit renders every one of these channels in full and it costs it nothing.

**What changed**, all in `render_chan` and `refresh_ctl`, all ours:

1. **A channel whose carriers and noise are over stops rendering.** Once every carrier's EG is done with no grain in flight, every unvoiced EG is done, and the channel has put out 64 samples under 1e-9 with no damp left, it is silent. Its remaining envelopes are all in their release and falling linearly, so the sample at which the last one passes -120 dB is arithmetic, and the channel costs nothing until the allocator may treat it as free on that sample. The render differs from before in one 16-bit sample by one step on the song (the filter's ring-down below 1e-9). Freeing the channel outright instead was tried and rejected: it changes which channel the firmware's allocator steals, and 132,000 samples moved by up to 193 steps.
2. **The control refresh skips finished operators.** A voiced operator whose EG is done, with no grain in flight and no attenuation under -20 dB (the carrier correction can reach -22.5), never reads its frequency maths again; an unvoiced operator whose EG is done is gated for good. Bit-identical.
3. **The control-rate maths reads the tables.** `word_hz` and `db2lin` go through `fsvr/fastmath.h` like the sample loop does. That moves 0.4 % of the song's 16-bit samples by one step, from the 1/16 dB table's 6.6e-6 relative error.

The nineteen regression cases match a fingerprint of the tree before the change, and the demo ledger reads the same to the digit before and after (`tools/demo_scores.txt`, the two rows of 2026-09-28). Vox Morph's one-note render goes from 1.86 s to 1.54 s on the same rules.

**What is left** is real work: fourteen sounding channels of carrier release tails between -60 and -120 dB, and the unit computes those too. The -90 dB gate above is 5 % on this song (5.3 operators per sample sit between -100 and -90), doubling `CTL_DECIMATION` another 6 %, and beyond that it is the operator loop itself.

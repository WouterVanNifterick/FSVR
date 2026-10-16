# src/engine: the firmware's side of the FS1R

`struct Synth`, the whole machine, and what the FS1R's CPU does every note, tick and MIDI byte. **KNOWN**: rewritten from the disassembly; every claim cites its `FUN_` address. A disagreement with the unit is a bug here, not a calibration.

| File | What |
|---|---|
| `synth.h` | `struct Synth`: the performance, the 32 channels, the filter and effect sections, and every engine method's declaration |
| `scales.h` | the firmware's byte-to-register scales: velocity, key tracking, pan, the 0..99 to 0..127 conversions |
| `notes.cpp` | note on and off, channel allocation, operator setup, pitch, portamento, the pitch EG, LFO1, the 192.3 Hz tick, the filter latch |
| `controllers.cpp` | the eight controller sets, the three scalers and the 48 destinations |
| `fseq.cpp` | formant sequence playback and MIDI clock |
| `midi.cpp` | MIDI in, controllers, RPN/NRPN, sysex replies and dumps |
| `tuning.h` | our cost knobs: control-rate decimation, queue caps. Ours, and changing one must not change the output |
| `render.cpp` | the sample loop: tick, channels (`tonegen/`), filter voices (`filters/`), part buses, effects (`effects/`) |

## Structure
`Synth` owns state and sequencing; the parts it drives are in their own folders and do not know about `Synth`:
`presets/` the data it decodes, `tonegen/` the tone generator (a channel renders from a `Voice`, not from `Synth`), `filters/`, `effects/`. The public API is `src/fs1r.h` (`fs1r::Device`, in `src/device/device.cpp`).

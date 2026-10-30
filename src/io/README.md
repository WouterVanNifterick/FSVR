# src/io: files in and out

**Ours.** No host and no Windows in it.

| File | What |
|---|---|
| `smf.h` | Standard MIDI File reading, offline rendering and WAV writing, shared by `tools/render_capture.cpp` and the console; `to_s16`, the one 16-bit quantiser every render and hash goes through |
| `factory.h` | the factory banks from `plugin/generated` without the EPROM: a performance with its voices and Fseq, a voice, an Fseq, by the engine's index numbers (`-factory` on the renderers, `tools/perfbench.cpp`) |

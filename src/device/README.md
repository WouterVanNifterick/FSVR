# src/device: the public device

`fsvr::Device`, declared in `src/fs1r.h`: the one surface the plugin, the console and the tools use. **Ours.**

| File | What |
|---|---|
| `device.cpp` | `fsvr::Device`: host-rate resampling, state as bulk dumps, MIDI in and sysex out, the Quality/Speed option, display helpers (`op_wave`, frequencies) |

# src/dsp: what the filters and effects share

| File | What |
|---|---|
| `word.h` | the DSP's number format: doubles in register units, saturating or wrapping at +-128, and `word::sum`, the wrapping running sum |
| `sink.h` | `WriteSink`: how a host (filter or effect) talks to its section: register writes, the program a window loads, a slot's type and clear |

# src/display: values as the unit shows them

For editors and displays; the engine never reads these. **Ours.**

| File | What |
|---|---|
| `display.h` | the unit's readouts: fixed frequencies, ratios, every effect type's parameter slots, ranges, defaults and value names. Transcribed from K_Take's FS1R Editor, checked by the selftest; the skin's `fixed_freq`, `op_ratio` and `fx_values` were made from it |
| `egview.h` | an EG curve solved rather than stepped, for the editor's envelope plots. The selftest checks it against the engine's EGs |

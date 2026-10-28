# src/kernel: the lane kernels

**Ours.** The filter bank's voices and, in time, the tone generator's channels computed a few at a time in one register, each lane doing exactly what the scalar code it stands in for does, in the same order, so the result is the scalar code's to the bit. `dsp_check` and `tools/abcheck.py` hold them to that; nothing here is a fact about the FS1R.

| File | What |
|---|---|
| `simd.h` | the lane types, one per instruction set: `vd` (W doubles) and `vm` (a mask), with the dozen operations the kernels use and the rules that keep them exact |
| `dispatch.h`, `.cpp` | `fsvr::Kernels`, the table of kernels a build of them fills, and `fsvr::kernels()`, which picks the best the CPU has once (`FSVR_SIMD=scalar\|sse2\|avx2\|neon` asks for less) |
| `isa/bank_kernel.cpp` | the filter bank's sixteen voices W at a time (`filters/lanes.h`), `VoiceFilter::run` on lanes |
| `isa/table.cpp` | this instruction set's table, and the check that its build fused nothing |

## Rules

- **`isa/` is compiled once per instruction set**, with `FSVR_ISA` 0 (scalar), 1 (sse2), 2 (avx2, under `/arch:AVX2` or `-mavx2`) or 3 (neon), each into `namespace fsvr::kernel::<set>`; `dispatch.cpp` is compiled once and names the tables it may choose from. `build.bat` and `CMakeLists.txt` do this; a file added to `isa/` is picked up by both.
- **A kernel file defines nothing outside its namespace.** An inline function it instantiates from a header, a `std::min` or a `word::sat`, becomes a COMDAT the linker may pick for the whole program, and one compiled under `/arch:AVX2` would then run on a machine without it. So `isa/` includes `simd.h`, the plain-data state headers (`filters/lanes.h`) and the C library only, restates the scalar helpers it needs in its own anonymous namespace, and `tools/check_isa.py` reads the avx2 object's symbols to prove it.
- **Nothing fuses.** The build keeps a multiply and an add apart everywhere (`-ffp-contract=off`, `/fp:precise`); each table carries a check that it did, which the self check runs.
- **Exactness is by construction, proved by hash.** `min`/`max` take the second operand on a tie, `select` is never arithmetic, `+ 0.0` stays; when a scalar path has a branch the lane takes both sides and selects. A lane that leaves the fast path (a wrap past its range) goes the long way alone.

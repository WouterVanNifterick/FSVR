# The VOP3 programs in C++

FSVR runs both of the FS1R's VOP3 chips on an emulated YSS236: the filter chip (VOP3-1) and the effect chip (VOP3-2) execute the firmware's own microcode, compiled at run time by the JIT in `src/fs1r/chips/yss236_jit.cpp` and checked against the interpreter in `src/fs1r/chips/yss236_core.h` by the `vop3_jit` test. This port replaces that microcode, program by program, with C++ that produces the same result bit for bit. When every program is ported, the JIT, the interpreter and AsmJit are retired.

The tracker is `docs/vop3_native/status.json`, one row per chip program the FS1R loads, and the public view of it is https://ai.musica.studio/effects. Work happens on the `vop3-native` branch only.

## What a native program is

A native program is one C++ function per chip program, in `src/fs1r/native/<key>.cpp` (the key with `-` as `_`, so `ins-m03` is `ins_m03.cpp` and the function `ins_m03`):

```cpp
void ins_m03(yss236_core& core, int from, int to);
```

It runs steps `from` to `to - 1` of its program on the core's own state, exactly as `yss236_core::step()` would run them, and leaves every field of the core as `step()` would leave it: the registers, cells, t file, running value, operands, delay RAM, pointer, step counter, compare flag, skip state and the pending writes in `m_rq`, `m_dq`, `m_tq` and the index queue. That is the whole contract, and `tools/vop3_native_check` enforces it after every sample. The core's helpers are public for this: `begin_step()` (what every step does first), `queue_r`, `queue_d`, `queue_t`, `queue_i`, `cur_idx`, `mode`, `accmode`, `clip`, `s16`, `addr`, `captured`, `push_hist`, `t_write` and `adder`.

The constants and delay offsets are not part of the program. The host rewrites them whenever a parameter moves, so a native reads them from `core.coef[st]` and `core.slot[]` every pass, never as numbers in the code. The program words are fixed per row, so a native may fold them: a step whose fields say "class 2, route 1, saturate, write r[67]" becomes that arithmetic written out, with no decode.

`yss236_core::step()` is the definition of what a step does. `tools/vop3_disasm.py` prints an older reading of the instruction set (see `docs/vop3_blockers.md`) and can be wrong where it disagrees with `step()`.

## A row, start to finish

The routine in #effects nudges you with the next row's brief. Each row moves through these stages, and you record each with `python3 tools/vop3_native.py set <key> <stage> --note "..."`:

1. **reading.** Name this thread for the row first, with the `kimaki session title` command at the top of the brief, so #effects shows which program each thread is on. Then `build/vop3_native_check --dump <key> /tmp` prints every step of the program as the core holds it under a type that runs it, decoded, and writes `/tmp/<key>.prog` and `.coef` for `tools/vop3_disasm.py`. Read the old C++ model the brief names under "Start from" for the structure: which steps form a comb, an allpass, a delay line, an LFO sweep, a filter. The old models were fitted to recordings and are no reference for the numbers.
2. **writing.** Write the function. The safe order is a straight transcription first: one block per step, in step order, with `begin_step()` and the step's arithmetic written out for its fixed fields, using the core's helpers so the pipeline timing stays exact. Get that to identical, commit it, and only then make it read like the DSP it is: name the registers and cells, group the steps into the structures you found, comment what each block is. Check again after every change; a tidy native that is not identical is worth less than an ugly one that is.
3. **checking.** Declare the function in `src/fs1r/native/native.h`, add `{"<key>", <function>},` to the table in `src/fs1r/native/native.cpp` in key order, build with `cmake --build build -j12` and run `build/vop3_native_check <key>`. It loads every type that runs the program, each over the last, changes every parameter halfway, and compares the whole core after every sample. The first field that differs is printed with the type and the sample. `--selftest` shows the check catching a native that is a hair off.
4. **done.** `python3 tools/vop3_native.py check <key>` runs the checker and marks the row done only when it reports identical. Then run `ctest --test-dir build` (every test must pass), commit the native, its two registry lines and `status.json` together, and push.

If you are stuck, do not go round in circles. Set the row to `blocked` with a note saying exactly what differs (the field, the type, the step you suspect) and what you tried, push, and stop. A person picks blocked rows up. After eight sessions on one row the brief says the same.

## Rules

- Bit for bit or not done. Nothing is rounded, tuned or "close enough". The checker is the judge, and the tracker only says done when it said identical.
- Never change what the interpreter, the JIT, the hosts or the chip programs compute: `step()`, `yss236_jit.cpp`, `src/fs1r/firmware/`, `src/fs1r/chips/cal.h` and the ROM tables stay as they are, and `vop3_jit` must stay green. The natives adapt to them, never the reverse.
- Never hand-edit generated files (FSVR's `AGENTS.md` lists them).
- Build without FMA contraction: the natives are compiled into `vop3host`, which already has `-ffp-contract=off`. Keep each operation in the order `step()` does it, since doubles are not associative.
- Commit to `vop3-native` only, never `main`, and never force-push. One commit per finished row, plus progress commits while one is in hand, each with a message that says what changed and the check's result. Never add a co-author line for a tool or a model; the container's commit hook adds the people.
- Before a session ends, record where the row stands with `set ... --note`, commit and push, so the next session (which may not be you) starts from the note and the branch, not from memory.
- Keep each session to one row, and report in the thread as the next section says.
- **Always English.** Every message, note, commit and file you write is in English, whatever language your model leans toward. Code identifiers and quoted tool output stay as they are. No other script and no mixed-language words, not even one, not even in a passing note: if a draft has any, rewrite it in English before you send or save it.
- **Evidence on every claim.** Say where a finding comes from: the checker's output, a `--dump` listing, `step()`'s code, the old model, or that it is an inference. The checker beats everything else.

## Reporting in the thread

Everything you write as text goes straight into this session's Discord thread, where the team follows the port; your commands do not show there. So the thread is the only view of your work, and it has to make sense to someone who cannot see your commands.

- **Open** with a short plan once the thread is named: the row, the stage you are picking up from (the note in the brief says where the last session stopped) and what this session will do.
- **Say what you are doing as you go.** Before each new step of the work (not each command), write one plain sentence saying what it does and why: `Dumping the filter program to see which steps form the ladder.`, not `Running the dump.` Before anything that will take minutes, say so and roughly how long.
- **Update at every milestone and at least every 15 minutes.** A milestone is a stage change, the first build of the native, every check run with its result, and every commit. A long stretch of silence reads as a stuck agent.
- **Close** with a summary: what this session did, what the checker said, what was committed and pushed, and what the next session picks up.

### How a message reads

Write every update and summary the same way, so the thread reads at a glance. Discord renders it as Markdown.

- **Lead line.** One or two plain sentences: what happened and what happens now.
- **Blocks under short labels.** After the lead line, group the rest under labels of one to four words ending in a colon, alone on their line (`Check:`, `Found:`, `Committed:`, `Next:`), with a blank line before each label. No `#` headings, no tables, and no code blocks except for a command the reader must run.
- **Bullets, one fact each.** A bullet is a short sentence or a `name: value` pair. Indent one level for the parts of one item. Never a paragraph of more than three sentences.
- **Numbers, ids and paths exactly.** Paths, file names, keys, step numbers, registers and fields in backticks; counts with what they count (`9,600 samples`, `13 types`).
- **Plain and direct.** Say what is, not what might be. No greetings, no thanks, no restating the task, no closing pleasantries; the last block is what happens next.
- Keep each message under 1500 characters. The detail belongs in the files and the commits.

A message shaped this way:

```
The straight transcription of `flt` builds, and the check finds one difference; fixing it is next.

Check:

• `vop3_native_check flt`: differs, `m_rq[2]` after sample 214 under LPF18.
• The 5 types before it matched for 9,600 samples each.

Found:

• Step `0x07c` is a z^-1 copy (`rcopy`), and my version lands it one step early.
   • `step()` queues it with `queue_r`, so it lands 3 steps later.

Next: queue the `0x07c` copy the way `step()` does and run the check again.
```

## Commands

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release    # once; a full build takes seconds
cmake --build build -j12
build/vop3_native_check --dump <key> /tmp                  # the program's steps, decoded
build/vop3_native_check <key>                              # the check
python3 tools/vop3_native.py status                        # every row
python3 tools/vop3_native.py check <key>                   # check and record
ctest --test-dir build                                     # everything, before each push
```

## Retiring the JIT

The `retire-jit` row is ready only when every other row is done. Then, in this order, each a separate commit:

1. Make the natives the default. `Effects` and `Filter` set `chip.core.natives` from what the hosts have loaded (the base, reverb and aux programs always; the insertion, variation and reverb-delay programs by the type msb each window holds), so the chip runs no microcode.
2. Record golden hashes: extend `vop3_native_check` with `--record`, writing a hash of each program's output over its check run per type to `docs/vop3_native/golden.txt`, and a mode that compares against the file, so the check keeps a reference once the interpreter is gone.
3. Show identical renders before and after with `render_capture`, on the demo pair named in `docs/vop3_blockers.md`, and a full `ctest`.
4. Remove `yss236_jit.cpp`, `third_party/asmjit`, `vop3_jit_check` and the interpreter's `step()` path from the build, keeping the core's state and the helpers the natives use.

Then set the row to `checking` and ask the Team to review. James merges `vop3-native` into `main`.

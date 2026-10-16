# The filters and effects as plain DSP

The FS1R runs its voice filters and its effects on two VOP3 chips executing the firmware's own programs. FSVR runs neither chip: `src/filters` and `src/effects` are those programs as plain DSP, bit for bit, and the chip interpreter in `src/fs1r/chips/yss236_core.h` stays only as the reference they are checked against.

- **Filters.** `src/filters/voice_filter.h` is one voice's resonant ladder; `src/filters/filter.h` runs one per channel (32 of them), programmed by the firmware's filter host.
- **Effects.** `src/effects/effects.h` is the effect host, its LFOs and the five programs it has loaded, run on one `State` (`src/effects/programs.h`): the base program (mixer, sends, returns, master EQ, reverb), aux, the reverb delay window, the variation window and the insertion window. Each program is a function `void ins_m03(State&)`, one sample, in `src/effects/<program>.cpp`. The programs talk only through the `State`: the stereo sends and returns in `r[3]`..`r[18]`, a few cells and the delay memory.

The tracker is `docs/vop3_native/status.json`, and the public view of it is https://ai.musica.studio/effects. Work happens on the `vop3-native` branch only.

## Where the code comes from

`tools/fx_lift` writes each effect program's body from the chip's program: it runs the program through a symbolic copy of `yss236_core::step()`, resolves the pipeline, the latches, the delay captures and the conditional skips at lift time, and prints the arithmetic that is left, in the chip's order. `fx_lift --seal` proves that no stage reads a latch or an in-flight write of the stage before it, for every combination of programs the EPROM holds; that proof is what lets a program be a function of the `State` alone.

```
cmake --build build --target fx_lift && build/fx_lift src/effects   # rewrites the bodies, keeps each file's header
build/vop3_native_check                                             # the filter and every effect type, against the chip
```

A lifted body is correct, not yet readable. The work on a program is to make it read as the DSP it is, from there:

- Name what a run of arithmetic is (a one-pole, a biquad, a delay tap with interpolation, an LFO-swept allpass) and give it a small function or a table of its units, as `voice_filter.h` does for the ladder.
- Name the registers and cells by what they hold (`state.r[103]` is the variation's summed input under 3-Band EQ). The file's header comment, from the earlier port, already says most of this.
- Keep every operation and its order, including the `+ 0.0` a sum opens with: `word::sum(a, b, c)` is the chip's running sum in its order, and `word.h` has the number format.
- Check after every change. A tidy program that is not identical is worth less than a lifted one that is.

Re-run `fx_lift` only for a program that has not been hand-edited, or after a change to `step()` or the EPROM tables; it overwrites the body.

## Checking

`build/vop3_native_check <program>` (`flt`, `base`, `aux`, `ins-mNN`, `dly-mNN`, `var-mNN`) loads every effect type, each over the last, randomises every effect parameter halfway, and compares the native section with the chip interpreter after every sample: the outputs, the registers, cells and t registers, and every 256 samples the whole delay memory. The first thing that differs is printed with the type and the sample. With no argument it runs everything; `--selftest` shows it catching a voice or a program a hair off. `build/vop3_bench` prints the cost and an output hash that must not move.

## Rules

- Bit for bit or not done. Nothing is rounded, tuned or "close enough". The checker is the judge, and the tracker only says done when it said identical.
- Never change what the interpreter, the hosts or the chip programs compute: `step()`, `src/fs1r/firmware/`, `src/fs1r/chips/cal.h` and the ROM tables stay as they are. The filters and effects adapt to them, never the reverse.
- Never hand-edit generated files (FSVR's `AGENTS.md` lists them).
- Build without FMA contraction: `src/effects` and `src/filters` are compiled into `vop3host`, which already has `-ffp-contract=off`. Keep each operation in the order the chip does it, since doubles are not associative.
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
- **Keep working until then.** A reply with no tool call in it ends your session, so a progress message must come with the next step's tool call, never on its own. Stop only when the row is done, blocked with a note, or its note is recorded and pushed. If you stop earlier, the routine tells you to carry on in the same thread, so one row stays in one thread; when the context fills, OpenCode compacts it in place.

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

# VOP3 (Yamaha YSS236) instruction set, as measured

Reference for `tools/vop3_disasm.py` and `tools/vop3_interp.py`. Every claim names its evidence:
**FW** = read from the FS1R, PLG150-AN or AN1x firmware; **CHIP** = measured on rgwan's FS1R through the
debug monitor (FS1R.unlock `captures/2026-09-30-9`, `-10`; per-take tables in those READMEs). Anything
not marked is an inference and says so. The disassembler's output over every image we have is in
`docs/vop3_disasm/`: FS1R VOP3-1 (filter, 2 variants), FS1R VOP3-2 (base + 4 reverb + 16 variation +
2x12 insertion + test images 0 and 1), PLG150-AN (3 voice modes + base), AN1x (voice + boot, parameter-labelled).
Each line gives the raw fields, then the step as its measured operation (`y = s + k*(r[31] + r[30])`,
`dram[34] = y`, `d[0b] <- xfer(slot 36, else 37)`, ...); `python tools/vop3_disasm.py --listings` regenerates them.

## One chip

**VOP3-1 and VOP3-2 are the same chip; a result measured on either applies to both** (and to the PLG150-AN's
and AN1x's VOP3). No measurement on one contradicts one on the other. Where this doc gives a field two
descriptions (`path`, `r9[12]`, op table columns "VOP3-1 filter" vs "VOP3-2"), they describe the same
behaviour in two contexts: a step inside the FS1R filter's feedback loop, where a change closes, opens or
runs the loop away, and a step in VOP3-2's feed-forward test chain, where the same change reads out as plain
arithmetic. The one board difference is external: the FS1R's VOP3-1 has no DRAM attached, so its programs
carry no `mem` marks. The disassembler and interpreter decode every image with the one set of meanings.

## 1. The machine

* A program is **512 steps**, run in order once per sample. FW: every uploader (FS1R `FUN_0000BC8C`,
  PLG150-AN `FUN_0007f87c`, AN1x `FUN_00056028`) writes 512 rows.
* A step is **five 16-bit words**, chip registers 10, 9, 8, 7, 6 (`r10 r9 r8 r7 r6` here), plus a
  **constant** in register 0xB and a **tag byte** in register 0xC, all addressed by the step number in
  register 0. FW: all three uploaders. The AN1x's image packs the tag into the `r10` word
  (`r10 | tag<<4`), so it is part of the instruction, not a side table.
* **Register 0 is an address latch shared with everything.** The FS1R's 192 Hz tick writes cutoff
  constants through it with interrupts masked (`FUN_0000B6A4`); a step written from outside that
  critical section lands on whatever step the tick last named. CHIP: two takes lost to exactly this.
  The only safe write is the firmware's own patch path from the CPU shadow (`FUN_0000B600`).
* State the word addresses: a **register file `r[0..0x7f]`** (7-bit addresses in `r9`/`r6`), a
  **write area `w[0..0x3f]`** (6-bit addresses in `r6`/`r7`, class-2 results, read back through `rsrc`),
  **data memory `d[0..0x1ff]`** (9-bit address in `r8`, one entry per step: the constant table seen as
  memory, used for lookup tables), and on chips with DRAM a **delay memory** by slot (`r10[2:0]` marks
  the access, slot = step >> 2, base/length in registers 0x1A/0x1C per slot at upload). FW: AN uploaders,
  FS1R VOP3-2.
* **`r[]` is one 7-bit space laid out per voice, and `w[]` is not its upper half.** FW
  (`tools/vop3_verify.py`): FS1R filter channel c of group g reads exactly the register the same-position
  channel of group 0 reads plus 24g, or a group-shared one (444/444 reads), so group 2 reads `r[3b..4d]`
  where group 0 reads `r[0b..1d]`, straight through 0x40; the AN1x reads `r[1 + 5k + v]` for voice v with one
  step shape per k for k = 0..12 (`r[01..41]`). A "97% of reads at or above 0x40 name a `w[]` slot some
  step writes" count also holds, but it is a coincidence of two dense files and the stride test overrides
  it: `w[3e]` (the cutoff MAC's slot) is not `r[7e]`. Below 0x40 on the AN1x nothing writes `r[01..3f]`
  (no driver path either: the only sub-0x40 targets are class-1 loads to `r[00]` and the boot register
  block `0x24..0x2a`), so they are hardware inputs: the serial ports and internal generators fill them, the
  program reads them. On the FS1R the class-1 control word of channel c (`100`: k=8000 mutes) lands in
  `r[6c]` and nothing reads it: the hardware consumes it. `r[]` is the chip's per-voice I/O window plus the
  class-1 constants; `w[]` is the program's own scratch.
* The FS1R filter runs **four channels interleaved in a 124-step group**: channel c owns steps
  `base+3c..base+3c+2` (gain, mode, cutoff constant) and every step `base+0x0c+c+4n`; four groups from
  0x010 cover sixteen channels. CHIP: clearing any of a channel's 31 steps changes its output,
  clearing any other step of the group does not.
* **The register file is banked per filter channel** (inferred): clearing channel 4's cutoff load
  `08e` closes channel 4 although `012`, `097`, `10a`, `186` load the same `r[61]` with the same word
  in the same pass; the bank must follow the step's position. `tools/vop3_interp.py` models it as
  sixteen banks selected by the interleave.
* A filter type change does not re-upload: the firmware patches steps into the shadow and uploads
  those (`FUN_0000D050` -> `FUN_0000C6C0` -> `FUN_0000B600`), muting the channel around it via
  register 0x2B. CHIP: LPF24 patches two steps per channel (`r7 40d1 -> 50d1`, `r6 low bits`).

## 2. The word

```
r6  [15:14] class   [13:7] rB              [6] -      [5:0] wdst
r9  [15] mod        [14:13] -   [12] ?     [11:8] path   [7] mod   [6:0] rA / load destination
r7  [15:14] -       [13:12] route   [11] ?   [10] sel   [9] mod   [8:6] op   [5] -   [4] rd-en   [3:0] rsrc
r8  [15:7] daddr    [6:0] mmode
r10 [15:3] ?        [2:0] mem
```
`-` = inert on every step it was flipped on (CHIP). `?` = never probed.

| field | role | evidence |
|---|---|---|
| `class` `r6[15:14]` | **1** constant load; **2** compute; **0** no output from this step; **3** loop runs away | CHIP: 1->0 and 2->0 silence the step's contribution, 2->3 runaway; on class 1 every other bit of r6 is inert |
| `rA` `r9[6:0]` | class 2: source register A, **added to `r[rB]`** when rB is set, ignored when rB is 0 (sessions 14/15); class 1: **destination register** of the constant | CHIP: VOP3-1 exact-match on every bit; VOP3-2 `x = r[rA] + r[rB]` (session 15) |
| `rB` `r6[13:7]` | class 2: **source register B**: `x = r[rB]` (+ `r[rA]`); 0 = the running value | CHIP: VOP3-1 exact-match; VOP3-2 sessions 13-15 |
| `path` `r9[11:8]` | exact-match per data path on VOP3-1's feedback loop (1..0xb across a group); inert on a feed-forward step | CHIP: VOP3-1 all 15 other values close the filter; VOP3-2 `0d3` 0..15 gain 1.000 (session 12) |
| `op` `r7[8:6]` | **3-bit operation**, section 3 | CHIP: swept 0..7 on two steps |
| `sel` `r7[10]` | **negates the running value** `s` (op 0 `-s + k*x`, op 4 `-s`; ops 1..3 unaffected) | CHIP: VOP3-2 session 12; VOP3-1's opened filter / constant output are the same sign flip inside the loop |
| `route` `r7[13:12]` | **scale of what the step writes out** (DRAM / bus): 3 = 1, 2 = 1/4, 1 = 1/8, 0 = 1/16; the value handed to the next step is not scaled; op 7 on route 3 gives 0 | CHIP: VOP3-2 sessions 12/14 |
| `rd-en` `r7[4]`, `rsrc` `r7[3:0]` | **with rB set: drops the gain `k`** (`y = x`); `w[rsrc]` never enters the arithmetic; with rB clear: inert | CHIP: VOP3-2 sessions 12/14; VOP3-1 `0f8` (session 9) |
| `wdst` `r6[5:0]` | **write slot** `w[wdst]` of the step's result; `rsrc` reads the same slots | CHIP: exact-match, 55/63 alternatives run away, bit-3 neighbours retune |
| `mod` `r9[7]`, `r9[15]`, `r7[9]` | modifiers: `r7[9]` ignored on the MAC probed; `r9[7]` and `r9[15]` share one signature (partial loss) | CHIP |
| `abs` `r7[11]` | **rectify: `y = abs(y)`** | CHIP: VOP3-2 session 15 (0.9997 x abs(output), residual -51 dB) |
| `r9[12]` | inert on a feed-forward step; one flip opened VOP3-1's filter | CHIP: VOP3-2 session 15 (gain 1.000, -62 dB); VOP3-1 session 8 |
| `daddr` `r8[15:7]` | 9-bit data-memory address. On the FS1R filter: lookup-table pointers into other steps' constants. On VOP3-2: `d[]` cell `n` = bits 6:0; bit 8 set addresses `d[]`: bit 7 = 1 the step writes `d[n]` (its route-scaled output; op 7 reads it), bit 7 = 0 **captures** into `d[n]` the DRAM transfer of slot s - 2 (else s - 1, else it holds) | FW: table indices; CHIP: VOP3-2 session 16 (`d[10a]` / 0e8 captures freeze, 0e0 with slot 36 empty takes slot 37, R == L bit-exact) |
| `mmode` `r8[6:0]` | inert on every step tried (VOP3-2 sessions 12 and 16, the DRAM read and write included) | CHIP |
| `mem` `r10[2:0]` | **delay-memory access, only on steps 3 mod 4, slot = step >> 2:** 1 = write the step's route-scaled output; 2, 3 = read the word written N passes ago (N = this slot's offset - the writing slot's); 4, 6 = one pass older; 0 = none. Offsets (reg 0 = slot, 0xd/0xe = 18-bit offset) index one 2^18-word ring whose pointer steps once per pass | CHIP: VOP3-2 session 16, lag = N to the sample for N = 0..16385, both-offsets shift inert, 2^14..2^16 + 17 no alias, off 0x3fff1 reads the other line 2^17 - 15 back; FW: test image 1 |
| constant (reg 0xB) | class 2: signed 1.15 **gain**; `rd-en` with rB set drops it (and `w[rsrc]` does not enter), with rB clear it stays (session 14). Class 1: the loaded value, 8.8 fixed point (`r = v/256`, session 14) | FW: the firmware patches k into class-2 steps on all three machines (FS1R gain `x120`, mode `x7c`, resonance A/B, type; AN1x mixer levels `x67`, pitch, PW, edge, sync; 69 of 69 AN1x handler targets and 112/112 FS1R targets are class 2 with `rd-en` clear), and a live constant on an `rd-en` step is rare: FS1R 0 of 240, AN1x 20 of 276 (7%, against 86% of the others), PLG150-AN 24% against 43%. So the read replaces the constant on the FS1R and nearly always on the AN; where both are present (the AN's 0x35c3 mixer group) the step has four sources and the MEG's `const * reg + reg` form is the reading. CHIP: session 9's 13 values on `0f8` changed nothing, and `0f8` is an `rd-en` step, so the two agree; the class-1 mute above 0x8000 stands. CHIP (VOP3-2, session 11): on the output step and the DRAM writer the constant is a signed 1.15 **gain** (linear, 0x4000 = half scale, sign follows k); with a zero input every op 0..7 and every k gives exact digital zero, so it is never an addend |
| tag (reg 0xC) | write-group id the firmware uses to patch steps by parameter: 3..7 = voice 0..4 on the AN1x, 1 = global; 0/1 on the FS1R filter | FW |

## 3. Operations (`op` = `r7[8:6]`)

| op | on the cutoff MAC (`098`: rA=62 rB=61 -> w[3e]) | at the output tap (session 10) | MEG counterpart | VOP3-2 `0d3`, measured gain on real audio (session 11 `held`; s = running value, k signed 1.15) |
|---|---|---|---|---|
| 0 | runaway | passes rA | `p =s ... + p` accumulate | **y = s + k*s** (accumulate the product) |
| 1 | closed | floor (~0.02): does not pass rA; 100% carry a read | `p = m` / `p = r` forward move | **y = k*s** |
| 2 | **corner follows rA x rB** | passes rA | `p = c * r` multiply | **y = k*s** |
| 3 | closed | passes rA | `p = (c<<8) + (p>>15)` mul-acc | **y = k*s** |
| 4 | closed | passes rA | mul-acc with memory | **y = s**, k ignored |
| 5 | runaway | floor (~0.02): does not pass rA; lowest read rate | `p = c * r` fresh product (discards accumulator) | s shifted out: output is the sign only ({-2^-17, 0}), k ignored |
| 6 | closed | passes rA; 100% carry a read | accumulate-forward | 0 |
| 7 | closed | passes rA | shifted mul-acc | 0 (op 7 starts a chain; the running value is not its source) |

**Session 12 (VOP3-2, `captures/2026-10-01-0414-s12/README.md`) closes:** the output word is 18 bits
(LSB 2^-17); the pipeline lag is 2 samples; `sel` negates the running-value term (op 0 `-s + g*x`, op 4
`-s`, ops 1..3 unchanged); `route` scales the step's output 3 : 2 : 1 : 0 = 1 : 1/4 : 1/8 : 1/16; op 7
reads the input at any step (session 11's zero at `0d3` was on route 3); `path`, `r8[6:0]`, `r10` and the
DRAM offset registers are inert on a step of a program without a delay line.

**Session 13 (`captures/2026-10-01-0441-s13/README.md`) closes the operand files:** rA/rB address `r[]`,
never `w[]` (w[n] loaded with 0.5 x input reads the same through rB = n or 40|n as with nothing
loaded; `w[]` is reached only through `rd-en`/`rsrc`). A class-1 load lands in `r[]` and op 1 reads it
through rB (`r[30]` = 0x4000 -> a constant, zero-variance output). (Its "live `r[]`" reading is withdrawn by
session 15, below.)

**Session 14 (`captures/2026-10-01-0454-s14/README.md`), class-1 loads as known operands:** a class-1 load
is 8.8 fixed point (`r = v / 256`); the running value / DRAM word clips at +-8 (seen at two output gains);
the running value carries from step to step unscaled by route (op 0 `s + k*x` gives the same 4.000 at
`0d1`, `0d2` or `0d3`); op 1 reading `r[rB]` gives `x + k*x`; `rd-en` with rB set drops `k` and never
brings `w[rsrc]` in (`x` alone, 4.000 for w[0e] live or w[0f] unwritten), with rB clear `k` stays (session
12): VOP3-1's `0f8` (session 9) and VOP3-2 agree. op 0 ignores a loaded `r[rA]` alone.

**Session 15 (`captures/2026-10-01-0509-s15/README.md`) closes the operand and the remaining fields:**
with rA and rB both set the operand is the **sum** `x = r[rA] + r[rB]` (r[30] = 1, r[31] = 0.5/1/2, k = 0.5:
ops 0, 2 store 0.75/1.00/1.50, op 3 1.00, op 1 3.00); op 1 with rB is `x + k*x` (k = 0xc000/0/0x4000 store
0.5/1.0/1.5); op 0 ignores rA alone, loaded or not; `r7[11]` rectifies (`abs(y)`, 0.9997, residual -51 dB);
`r9[12]` is inert. Unloaded `r[]` registers hold constants left by earlier programs (96/127 zero, the rest
fixed values, none carries audio). A step's result reaches the next step in the same pass: there is no
write delay; when the chain head is missing the next step reads the value the previous step in program
order left (the other chain's end), which is what sessions 12/13 saw as a "live" register.

**On a step with no feedback the arithmetic is readable** (last column; VOP3-2's DRAM writer `0d3`, every
fit a pure gain of the same signal, residual equal to the reference's): the constant only multiplies, op 0
adds the product to the running value, ops 1..3 replace it with the product, op 4 passes it, op 5 shifts it
out. With the note off every op and k gives exact zero (take 2), so no op loads or adds k. Whether ops 0..3
multiply the running value or rA/rB is not split yet (on `0d3` they are the same signal).

**On VOP3-1 the op's arithmetic cannot be read from one probed step.**
Session 10 used the output stage's op-1 move as a DC voltmeter on a silenced channel; the tap is
AC-coupled, so `dc` read converter offset (-2e-5) on all 122 segments and the DC plan is dead. What the
level *did* show: the output passes `rA` alone, identically for ops {0,2,3,4,6,7}; ops 1 and 5 drop to a
fixed floor (they start/forward without passing the probed operand); `rB`, the read source, `path` (bar
the reserved slot 3), and every remaining `r7/r9/r10/r8` bit are all inert on a pass step. The op only
manifests through the recursive filter loop, which the FS1R never exposes. The MEG column is the close
(see section 7): the same operation *mix and ordering*, on a chip whose ISA MAME decodes to explicit
arithmetic. The FS1R program uses ops {0, 2, 3, 4, 5, 7} on class-2 steps, the AN1x {0..7}.

## 4. The FS1R filter's cutoff path, channel 0, read off the chip

```
08c  class 2  op 5  -> w[13]  rB=7f                  gain term: cleared, the loop runs away (+83 dB HF)
08d  class 2  op 4  route 1 -> w[05]                 mode: cleared, the resonance peak moves to 80-160 Hz
08e  class 1  r[61] = k                              staged cutoff constant (log frequency, tick-driven)
098  class 2  op 2  path 1  rA=62 rB=61 -> w[3e]     the MAC whose result the corner follows
09c  class 2  op 2  path 1  rA=62 rB=62 -> w[3d]  rd w[1]
0a8  class 2  op 2s        rA=62 rB=61 -> w[3f]      the resonance peak (cleared: -19 dB at 320-1280 Hz only)
0c4..0c7, 0d4..0d7                                   the two biquad sections: a state write (silence when
                                                     cleared), a feedback term (+19/+24 dB blow-up), a runaway
0e8  op 5  rA=1b rB=1b -> w[17] rd w[1]              state write (silence when cleared)
0f0  op 1  sel  rB=1c -> w[23] rd w[1]               output stage, bus A (-16 dB flat when cleared)
0f8  op 1  sel  rB=1d          rd w[1]               output stage, bus B (-16 dB flat when cleared)
100  class 1  r[1e] = k (k=8000)                     control word: a negative value mutes the output stages
```
Channel c adds c to every step address from 0x098 on and 3c to the first three, with its own registers.
The cutoff-byte-to-corner law is `docs/filter.md` (`15_filter`).

## 5. AN1x parameter labels

`docs/an1x_param_map.md` (from the scene-parameter table at `0xCFCEC`) names, for each of 56 knobs,
the steps whose constant it rewrites; `docs/vop3_disasm/an1x_voice.txt` carries them. Cutoff is a
class-1 load followed by a `6000 0004 3040 4000` step (class 1, `r9[14:13]` = 3, route 3, op 1, sel; 20 per
AN1x voice program, 7 on the PLG150-AN, after every sync/cutoff load: a log->linear conversion, as the EX5
MEG program does with an exp table); VCO pitch 4096/octave with a key term; mixer levels `level*0x67`.

## 6. Interpreter

`tools/vop3_interp.py` runs a program with the arithmetic measured on VOP3-2 (sections 3 and 8; the same
chip as VOP3-1). Class 1: `r[rA] = v/256` (8.8). Class 2: gain `g` = `k` (signed 1.15), 0 when `rd-en` and
rB are both set; running value `s` (`-s` with `sel`), one register passed step to step in program order;
operand `x` = `s` when rB = 0, `r[rB]` when only rB is set, `r[rA] + r[rB]` when both are; op 0 `s + g*x`,
op 1 `g*x` (`+ x` when rB is set), ops 2/3 `g*x`, op 4 `s`, op 5 sign of `s`, op 6 zero, op 7 `g x` the
chip's input (0 on route 3); `r7[11]` takes `|y|`; every result clips at +-8. `wdst` writes into `w[]`;
route 0 of an op-1 step goes onto the output bus. Its self-check reproduces session 11's `held` table and
`dc dram` silence, session 12's `sel` gains, session 14's eight stored values, session 15's sum, op-1 and
`r7[11]` results, and session 8's cutoff load (clearing it zeroes the MAC) and per-channel step ownership.
The delay memory is modelled from session 16: `r10` read/write on steps 3 mod 4, slot offsets into one
2^18-word ring stepping once per pass, route-scaled writes, `d[]` captures of slot s - 2; its self-check
reproduces test image 1's lags (N - 1 against image 0 for N = 0, 1, 2, 17), the moved read that plays the
left channel bit-exact, and the other line reached 2^17 - 15 back. Not modelled: the 2-sample output lag and
the 18-bit output word (fixed properties of the DAC path, not of a program).

## 7. Model check against the EX5 MEG (the close for the op arithmetic)

The FS1R VOP3 and the EX5 SWP30 MEG run the *same* AN algorithm; MAME decodes the MEG to explicit
arithmetic (`docs/ex5_meg_an.txt`, the AN program, 254 steps), so it is value-transparent where the
FS1R is not. `tools/vop3_meg_check.py` classifies every step of both into one vocabulary
(multiply / accumulate / load-constant / move / memory / lookup) and compares:

* **Operation mix matches.** MEG: 82% multiply-class, 4% load-constant, 13% memory. VOP3 AN1x voice:
  63% MAC-class, 7% load-constant, 19% memory. Both are MAC-dominated with a small constant-load
  population and a comparable memory fraction.
* **Step count is ~2x** (VOP3 512 vs MEG 254): the FS1R runs the engine at a higher internal rate.
* **The op-1/op-5 pairing reconciles.** Session 10 found ops 1 and 5 do not pass the probed operand at
  the output tap. In the VOP3 program op 1 and op 6 carry a read 100% of the time (operand-forward /
  accumulate, MEG's `p = m`), while op 5 is the largest op with the *lowest* read rate (a fresh product
  `p = c * r` that discards the running accumulator, MEG's most common op). So op 5's "does not pass the
  preloaded a" is exactly a new-product-start, and op 1's is a forward-move: both consistent with the MEG.

This validates the operation field as a genuine ALU-op selector with the MEG's operation semantics,
without needing the FS1R accumulator to be observable. Reproduce: `python tools/vop3_meg_check.py`.

**Dataflow check (`tools/vop3_verify.py`).** The MEG listing passes the trivial test (every `r`/`t` read has
a writer in the program, 260/260); the VOP3 images are tested for what their firmware and layout prove:
the constant is live on class-2 steps and dead on `rd-en` steps, `r[]` has the per-voice stride above
(FS1R 444/444, AN1x 13/13), which is what pins `w[]` as a separate file. An earlier reading of `w[n]` as
`r[0x40 | n]` passed a writer-coverage count at 97% and was wrong; the stride test is the one to trust.

## 8. Open

**The decode does not yet run a shipped program** (full list: `docs/vop3_blockers.md`). Every field above is measured on a feed-forward test chain, and `tools/vop3_gaps.py` runs the shipped programs through the interpreter and asserts three gaps that block a C++ rewrite from the decode:

* **VOP3-1 has no memory.** An impulse on any register filter channel 0 reads and does not load leaves in the same pass with no tail, so the filter as decoded has no pole. Routing class-2 results back into `r[n]` or `r[40|n]`, same pass or next, changes nothing. The state path is undecoded; `path` (exact-match on the feedback loop, inert feed-forward) and class 3 are the candidates.
* **VOP3-2 reaches no output.** Base image plus reverb 0, variation 0 and insertion 0/0, audio on the input port, every constant 0.5: zero non-zero output samples in 4096. Only test image 1 names its output steps (0e9/0eb); which steps of the shipped program feed the DAC is unknown.
* **Class 3 is not decoded** and carries the reverb: 16 of reverb 0's 29 live steps, 27 of the assembled VOP3-2 program, 32 of VOP3-1.

The per-type constants VOP3-2 runs with are not in the images either: `FUN_00039652` writes register 0xB from `DAT_0106842c`, which the effect parameter handlers fill.

Session 16 (`captures/2026-10-01-0535-s16`, test image 1) closed the delay memory: the `mem` and `daddr`
rows of section 2 and the interpreter's DRAM model are its measurements. Recorded but not modelled, because
no shipped program uses them: a capture step at 0dc..0de or 0e5 (R == L at lag 0 / +1 on test image 1) and
0e0 turned into a `d[18b]` step write (0e9 at 0.42 x L).

**Sessions 17-19 (remote rig, 2026-10-01; folders on the rig under `~/FS1R.unlock/captures/2026-10-01-17*`):**

* **op 0 with rA and rB both set, below route 3: rA is a destination, not an operand.** x = r[rB], y = s + k x,
  and r[rA] = y (scaled x16 by route, as the d[]/DRAM writes are: route 0 x1, route 1 x2). With rA = rB this
  is a recursive integrator: r[32] settles to s / (1 - k) at route 0 (s = 0.375 / 0.5, k = 0..0.25: 0.375 /
  0.3999 / 0.4285 / 0.5 and 0.5 / 0.5333 / 0.5714 / 0.6666, all to 1e-4) and 2s / (1 - 2k) at route 1
  (s18: s = 0.25, k = 0.25 / 0.125: 1.0 / 0.667). The
  register keeps its value across passes: this is the chip's state, the comb's damping pole (base 0a5) and the
  missing VOP3-1 filter memory. Route 3 still sums (session 15). Interpreter: in, with its self-check.
* **Class 3 is class 2 at unity gain with rA as destination.** k is ignored; op 0 gives s + r[rB], op 1 2 r[rB]
  (x + x), ops 2/3 r[rB], op 4 s, op 5 r[rB] (not sgn); r[rA] and the running value both take y (s19,
  s = 0.5, r[33] = 0.25, all six exact). Clearing the reverb window's 16 class-3 steps silences the reverb
  (s17 `c3_off`, R exact zero). Interpreter: in.
* **rd-en with rB set: the gain is a 16-entry file G[rsrc], not k and not zero.** Op 2, x = 1.0, k 0.5:
  G = 0.99988, 0.10144, -0.07422, 0, -0.33203, 0, 0.99988, 0, -0.50757, 0, 0, 0, -1, -1, 0, 0 (rsrc 0..f),
  linear in x (x = 0.5 halves it), the same on ops 0/2/3; op 1 adds x (1.10144). A step writing w[02] in the
  same pass does not change G[2], so G is not this pass's w[]. Its writer is open (blocker 4 narrowed):
  the values are what the effect programs left. Interpreter: the measured table, as a constant.
* **Class 1 with rB set adds:** r[rA] = k/256 + r[rB], r[rB] unchanged (2.0 + 0.5 = 2.5, both r7 forms).
* **Comb 1 (Hall1, time 0.8 s) on the unit:** period 4974 samples = read slot 22 - write slot 2a (+1000 on
  either moves it by 1000, slot 28 inert), echo ratio 0.412 = the loop 2 (x3 + x4 z^-1) / (1 - 2 p z^-1) of
  0a3/0a4/0a5's words at its peak (`tools/vop3_comb_check.py`, five segments). Zeroing 0a9's word kills the
  comb (0a9 is the input gain).
* **Off-phase DRAM marks are live:** the write moved to phase 2 still writes; to phases 1 / 0 it changes the
  line (gain 1.1, lag 0); a read on phase 1 also reads. r10 = 5/6/7 on the write and 5/7 on the read are
  not the shipped meanings (uncorrelated / unchanged).
* **Output path (s17 `walk`, Hall1, noise held):** the dry note (L, part panned hard left) drops 25-35 dB
  only when 002, 0e0, 0e7 or 0ea is cleared; the reverb (R) goes to exact zero only when 0d5 or 0d9 is cleared.
  Clearing 12d left R dead for the rest of the take (state latched at zero), so steps after 12d were not
  measured on R, and the note clipped L near 0 dBFS. Needs a re-walk at a lower level with a state reset.
* **EQ frequency:** the dumped coefficients evaluated at 48 kHz fit the measured responses better than at
  44.1 kHz on all three (1.37 / 1.45 / 1.38 dB rms against 1.42 / 1.55 / 1.60), the measured peaks sitting
  at 539 / 1131 / 2150 Hz for words designed at 500 / 1000 / 2000 Hz at 44.1 kHz. Leaning 48 kHz; the
  fit is not decisive.

**Sessions 20-23 (remote rig): VOP3-2's buses, and a reset that needs no power-cycle.**

* **Output:** the left / right DAC words are r[09] / r[0a] after the last step (a class-1 load of 0.25 into
  either at the free step 10f reads 0.5 x 1/16 on that channel only; DAC = r / 8). The base program writes them
  last at 0e7 / 0df (op 0, route 2, rA the destination).
* **Inputs (s22, all 127 registers read into L per bus configuration):** the reverb send pair arrives in
  r[03] / r[04] (and r[57..5c]), the variation send pair in r[05] / r[06] (and r[67..6c]), the insertion pair
  in r[07] / r[08] (and r[43..4e]); each at the same level (-49.5 dB on a -42 dB send). The dry part reaches
  the output already mixed: with the effects off, r[09] / r[0a] and the output-EQ chain r[23..2a] carry it at
  the DAC level, 1-3 samples ahead. So the chip's audio inputs are register writes by the hardware before the
  pass, which is where the "read, never written" registers of the program come from.
* **Reset:** FUN_0003AAC8 (the boot's VOP3-2 sequence: init, every effect handler, programs, unmute) plus
  all-notes-off clears latched state; FUN_0003A064 + FUN_0003A110 alone leave the chip muted.
  FS1R.unlock `tools/vop2_reset.py`.
* **End-to-end (s23):** the effect input r[03] (or r[05]) and the output r[0a] recorded together for reverb
  0/1/9/13/16 and variation 1/9, with the constants, offsets and selectors dumped. The interpreter fed that
  input does not reproduce the output yet: the reverb program saturates (+-8) inside the comb section and
  r[0a] stays 0. That run is the acceptance test the C++ rewrite needs; it is not passed.

**Session 24 (remote rig, rounds 1-4, 80 DC probes; `docs/vop3_probes.json`, `tools/vop3_probe.py check`).** One
rule set reproduces 79 of them and session 11's op table; it replaces the session 18/19 readings above (they were
the special cases rA = rB and route 0 of it):

* x = r[rB]; **rB = 0 names r[0], the chip input**, not the running value (rB 0 on a silent input gives 0 on
  ops 1/2/3; op 0 then passes s).
* y = op(s, k, x), then **y is scaled by the route, x1 / x2 / x4 / x16 (route 0..3)**, and s = y.
* **rA != 0 writes r[rA] = y** on every class-2/3 step, any route (route 3 included: the session 15 "sum"
  was this write plus the next step's read).
* Class 3 is class 2 with k = 1. Class 1 loads r[rA] = k/256 plus r[rB] (rB set) or plus s (rB 0), scaled by
  the route, and s takes the value.
* Open: one probe (`G5_op6_r30_r`, class 1 r7 = 0x1180, the shipped op-6 load form) stores -8 where the rule
  gives +8; and nothing tried writes the rd-en gain file G[] (op-6 loads with wdst 5/7, a plain load and a
  class-2 step with wdst 5: G[5] stays 0).

**End-to-end (`tools/vop3_e2e.py`, session 23's recorded input):** still 0 on r[0a] for var1, rev1, rev9; the
insertion-1 window saturates within one pass (its steps lean on G[12] / G[13] and the op-6 loads, both open).

**Sessions 24-27 (remote rig): number formats, latency, z^-1 registers, the real DAC taps.**
Measured on the unit; every rule is asserted by `tools/vop3_interp.py` (349 probes in `docs/vop3_probes.json`)
or `tools/vop3_ship.py` (shipped program, `docs/vop3_ship/`, 41 of 41).

* Registers hold [-128, 128) (word / 256); the running value s is [-256, 256). r7[15:14] picks the overflow
  rule: 0 wraps, 1 saturates, 2 clamps negatives to 0, 3 takes |y|.
* A register written at step n is readable from step n + 3; steps n + 1, n + 2 see the old value.
* Class 1: r[rA] = k/256 + s (rB does not replace s), times the route.
* Op 5 with rB set (class 2): max(r[rB], 0), k only gates; the comb input steps use it.
* rd-en gain file G[rsrc]: a fixed table (same on a blank image and under the insertion program; no step writes it).
* rA with bit 7 set (word 1 bit 7) is a z^-1 write: the register gets the previous step's x operand (r[rB] of
  the step before), not this step's result; after a step with rB = 0 it gets its own x. The base program's
  biquads (066-074, 0b6-0c4) keep their delay state this way.
* d[] holds at least +-32 (it is not clipped to the DRAM word's +-8).
* The DAC plays d[0x10] (L) and d[0x11] (R), written by base steps 0cf / 0d0 (route 3, x16); unit readout =
  d / 4 in the s25 units (DAC full scale = d 32). Session 22's "r[09] / r[0a] after the last step" was only
  what the base program reads into them. Steps 06a / 0ba are where the output biquads read r09 / r0a.
* Input registers change mid-pass: r0e tapped at 0x010 and at 0x110+ differ by one sample (boundary between
  0x108 and 0x110), r0d by two; the external (VOP3-1 to VOP3-2) bus writes land partway through the pass.
* Still open: probes that break the output biquad loop (06a-06e, 0b9-0bb) settle at +-8 on the unit with a
  sign the model does not reproduce (unstable loops, history-dependent); the end-to-end replay needs a fresh
  paired take using the tap method above and the mid-pass input timing.

**Session 28 (remote rig): step responses on the shipped programs, sample-exact.**
`tools/vop3_step.py`, `tools/vop3_seq.py`; kept-patch probes (s25 runner `keep`), data in FS1R.unlock s28 folders.

* Base program inputs (steps 000-007) re-pointed at a loaded register and stepped 0 -> 1/16: the dry path of
  every shipped program (none, Hall1, variation 1, Hall 9) matches the model to the bit: DAC word 18-bit,
  value floor(d / 4 x 2^14) / 2^14 (readout units), R leaves the chip in the pass the input changes, L one pass
  later (`Interp.dac()`).
* 24 register taps (0cf re-pointed) on Hall1 with the step held: all match the model.
* The reverb tail does NOT appear in the model: on the unit, L (tapped at r77/r78/r1d/r29) starts moving
  1346 / 1473 samples after the step (r37/r3c: 5112 / 5619), with exact first samples 0.000977, 0.002319,
  0.002869, 0.003052. The model's delay memory stays empty: its comb feed is op 7 / op 4 steps with rB = 0
  (0x2a-0x2e, 0xa3-0xb3, 0xd2-0xd3) whose source is not modelled. Probes so far:
  - op 7 / op 4 source is not d[] when daddr bit 8 is clear, not w[r6 slot], not the register x (rounds 13-15,
    all read 0 on test image 0, where no DRAM transfer runs).
  - Hypotheses tried against the 1346-sample onset and rejected: op 7 = k x previous step's x (right sample
    values, but no delay: tail at sample 0); op 7 / op 4 = k x the DRAM transfer of slot s - 2 / s - 1 (no
    tail at all, the memory is never written).
  - Next: a probe on the shipped program that writes a known value into one comb's DRAM write step and reads
    the op 7 chain's result through 0cf, to find which transfer op 7 multiplies.

**Session 29 (remote rig): the shipped Hall1 step by step; op 7's operand; class-1 d[] writes.**
Shipped Hall1 with every DRAM write cut (r10 = 0) and the inputs held at 1/16, each of its 380 live steps tapped
in turn to d[10] (daddr 190, the left DAC): **378 / 380 match the model** (`docs/vop3_ship/29c.*`). Rules added:

* **Op 7's operand is d[r6[5:0]]** (the step's `f6c` field), not daddr: probe D_o7_10 (f6c 0x10) reads the
  left's d[10]; daddr 18d with f6c 0 reads 0 (s24 round 14). Test image 0's own op-7 steps (0d0 f6c 01, 0d4 f6c
  05) read the audio input, so the input arrives in d[1] / d[5] (`INPUT_D`, test-image layout only).
* **Class 1 with r9[12:8] set writes d[n]** for daddr 0x180|n (the 8 Hall1 taps on its `1c7c` / `1d01` / `1c7e`
  loads); plain, op-6 and op-1 loads write nothing there.
* Memory probes need test image 1 (`{"image": 1}` in the s24 runner) or a shipped program; on test image 0 no
  DRAM transfer is observable (every round-16/17 memory probe read 0 on both images, so the transfer path of
  hand-built steps is still not understood; image 1 as shipped (J_ship) does round-trip a value).

Still open, blocking the reverb tail (the model's delay memory stays empty on Hall1, the unit's L tail starts
1346 samples after the step: read slot 0 (offset 29998) minus write slot 0xd (28652), moved 1:1 by slot 0xd's
and slot 0's offsets (346 / 1614 after +-1000), so the tail is the delay line written at slot 0xd (step 0x37,
op 4 rB 0, mem 1) and read at slot 0 (step 0x03)):
* what the comb write steps (op 4 / op 5 / op 7 with rB = 0 and mem 1) carry: tapping them on the unit gives 0
  with memory writes cut, while tapping 0x29 / 0x34 (op 5 rB 34 / 31, k 0) with memory live gives a rising
  non-zero value (0.0065, 0.0156, 0.0191, 0.0208 readout) that the model's op-5 gate (k 0 -> 0) does not produce.

**Session 30 (remote rig): the reverb tail starts in the model, to the sample.**
Hall1 step response, input 0 -> 1/16 (s28 takes; s29a offset-moved takes; s30a: sixteen d[] cells tapped through
an op-7 step at 0cf). Rules added to `tools/vop3_interp.py`, all against unit data:

* **Op 5 with rB: max(r[rB], 0) + k x d[f6c]** (the comb write: feedback plus input gain); k does not gate
  (K_o5_k0 on image 1 writes 0.25 with k 0). **Op 4: s + k x d[f6c]** (the FIR taps).
* **Captures land three steps late** (`CAPLAT = 3`): with it the L tail of every Hall1 take starts on the
  unit's sample (1346 as shipped, 346 with slot 0xd + 1000; taps r77 / r1d / 02b / 02c / 02e / 036 / 037 / 03f
  match their first ~100 tail samples to the LSB).
* **DRAM reads and writes also run on phase 1** (step & 3 == 1; the shipped comb writes 0a9..0b5, 0d1).
* **Op 7 writes d[daddr]** like the other ops, except onto its own source cell.
* **d[] has 64 cells: the cell is daddr & 0x3f** (Hall1's comb outputs at daddr 1e2..1e7 land in d[22..27];
  their onsets 5587 / 4880 / 4200 / 3763 match the model within one sample).

Measured and not yet matched (`docs/vop3_ship/30a.probes.json`, takes in FS1R.unlock s30 folders):
* tail amplitude: d[3e] (op-4 FIR at 04c-04f) is 0.46 x the model from its first sample; the rest of the tail
  follows it, so the late tail (beyond ~1490 samples on L, ~1487 on R) drifts by 1-30 LSB.
* d[26], d[27], d[39], d[3c] stay 0 in the model but carry the tail on the unit (onsets 3066, 5080, 3066, 1487);
  d[17] moves in the model (614) and stays 0 on the unit.
* o0p1000 (slot 0 + 1000): unit 1614, model 1997.

**Session 31: d[] write timing, DRAM regions, the upper d[] bank.** Checked against the s30a d[]-cell taps and
s31a step taps (steps 040, 041, 04c..04f with memory live; `docs/vop3_ship/31a.probes.json`).

* **A step's d[] write lands 3 steps later in the same pass** (`DLAT = 3`), not at pass end. This was the
  0.46 "gain" on d[3e]: with it d[3e] matches the unit to 1 LSB over its first 400 samples (it was 0.46 x).
  Steps 040..04f themselves match to 1 LSB.
* **mmode bits 3:2 select a DRAM region** (`REGION`, size assumed 2^16; Hall1 0x1d3 / 0x177 carry 0x44 / 0x48).
  Without it 0x1d3 overwrote 0xd1's comb line. d[3c] now starts on the unit's sample (1487).
* **Class-0 steps with mem 1 write the running value** (Hall1 0x153, the 1452-sample line read at 0xa3).
* **Steps >= 0x100 use the upper 64 d[] cells** (`DHI`): d[17] (written by 0x11c) stays 0 on the unit; now in
  the model too.
* Regression: 384 / 384 s24 probes, s28a dry takes bit-exact.

Open:
* **daddr bit 6 (0x1e2..0x1e7, the comb outputs):** onsets match (d22..d25 to the sample), level is ~4-5.6 x the
  model and not a plain gain; a lag -1 fit gives y = 3.99 x(read at 0xa3, one step earlier), i.e. close to a x4 copy
  of the DRAM read. Four simple variants (4 x previous operand, 4 x running value, now / pass end) do not fit.
* d[26], d[27], d[39] still silent in the model (unit onsets 3066, 5080, 3066).

**Session 32: daddr bit 6 (the Hall1 comb-output writes 0x1e2..0x1e7).** Probes edit step 0a4 / 0a3 of shipped
Hall1 and tap d[22] (`docs/vop3_ship/32a.probes.json`, `32b.probes.json`). Capture check: the takes carry 18
effective bits, left-justified (low 14 bits zero in all 587k non-zero samples of the s31 take, no dither).

* With bit 6 set and k != 0, the step writes d[n] = the previous step's operand (d[f6c] of 0a3), not its own
  result: d[22] = 3.987 x the op-7 read, lag 1, residue 1 LSB (`base`). The op and the value of k do not
  matter (k 0x0800 / 0x4000 / 0xc000 and op 0 all give the same take). k = 0 gives the plain write
  (`k0`: d[22] = 0a4's result, 1 LSB), as does clearing bit 6 (`b6off`, 1 LSB).
* Now in the model: d[22] matches 1 LSB for its first 9 samples, then drifts.

Measured, not explained:
* Step 0a3 with k = 0: the unit's d[22] is 0, the model's is not. So the written value does depend on
  0a3's k after all. The 3.99 x operand fit is a coincidence of this k, or the value is 0a3's product times
  a fixed gain (0.17834 x 22.36).
* 0a3 k = 0x4000: d[22] alternates +8 / -8 every sample on the unit (a sign-flipping loop through d[38]).
* 0a4 k = 0x7fff: d[22] = -8 constant.
* d[23..27], d[38..3f] drift after 4..40 samples; d[26], d[27], d[39] stay silent in the model.

**Session 33: bit-6 rule settled, Hall1 step taps with memory live.** (`docs/vop3_ship/33a.probes.json`,
`33c.probes.json`; s32a / s33b probes ran with `keep`, so each probe carries the earlier patches: the s32
"0a3 k = 0 silences d[22]" result was 0a4 k = 0 plus 0a3 k = 0, not a dependence on 0a3's k.)

* **daddr bit 6 writes d[n] = the previous step's operand d[f6c]**, one pass later, whatever this step's op,
  k or the previous step's k: 0a3 k = 0x0400 / 0x0b6a / 0x16d4 / 0x2000 / 0xe92c all give the identical d[22]
  take (gain 3.987 on the model's d[38] read, residue 1 LSB). Without bit 6 it is the plain result write.
* **Class-0 steps with mem 1 write nothing** (s31's "running value" rule withdrawn; it zeroed 0xd1's line).
  d[26] and d[39] now start on the unit's sample (3066).
* 28 Hall1 step taps with memory live (01d..08f): 27 match to 1 LSB over 2400 samples; 08f drifts at 2389.
* d[] cells: d16, d17, d22 exact over 6000 samples; the others start on time and match for 1060 samples,
  then miss a negative step (d3c at 2549 = 1487 + 1062: the 0x57 -> 0x33 line, written with 0 in the model).
* Step-response takes (s28a, 8000 samples): Hall1 exact to sample 2088, Hall9 to 1905; tail rms error 1.3x
  (Hall1) and 1.1x (Hall9) the unit's tail rms.

Open: what 0x57 / 0x157 / 0x19f (op 0, k 0, mem 1, rB 78 / 7a) write into DRAM. Writing r[rB] makes the tail
worse (`B13X`, rejected). Swapping the capture source order (`CAPORDER`) breaks 0..613: rejected.


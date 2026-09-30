# VOP3 (Yamaha YSS236) instruction set, as measured

Reference for `tools/vop3_disasm.py` and `tools/vop3_interp.py`. Every claim names its evidence:
**FW** = read from the FS1R, PLG150-AN or AN1x firmware; **CHIP** = measured on rgwan's FS1R through the
debug monitor (FS1R.unlock `captures/2026-09-30-9`, `-10`; per-take tables in those READMEs). Anything
not marked is an inference and says so. The disassembler's output over every image we have is in
`docs/vop3_disasm/`: FS1R VOP3-1 (filter, 2 variants), FS1R VOP3-2 (base + 4 reverb + 16 variation +
2x12 insertion), PLG150-AN (3 voice modes + base), AN1x (voice + boot, parameter-labelled).

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
| `rA` `r9[6:0]` | class 2: **source register A**; class 1: **destination register** of the constant | CHIP: exact-match, every bit: any other rA retunes (smaller operand), any other destination loses the constant |
| `rB` `r6[13:7]` | class 2: **source register B** | CHIP: exact-match |
| `path` `r9[11:8]` | **exact-match per data path**: 1..0xb across a group, channel 0's compute steps carry 1, channel 1's 2 | CHIP: all 15 other values close the filter identically. A bus/accumulator id, not an opcode |
| `op` `r7[8:6]` | **3-bit operation**, section 3 | CHIP: swept 0..7 on two steps |
| `sel` `r7[10]` | modifier: on the cutoff MAC, set alone opens the filter with the peak intact; on the output move, 1 = audio, 0 = a constant | CHIP |
| `route` `r7[13:12]` | **result destination**. Output stage: 0 = the output bus, 1/2 = elsewhere (DC leaks), 3 = constant. Cutoff MAC: 1 shipped, 0 drops the corner an octave, 2/3 runaway | CHIP |
| `rd-en` `r7[4]`, `rsrc` `r7[3:0]` | **read enable + 4-bit read source**: rd-en clear makes all 16 rsrc values inert; enabled, 0..0xb replace an operand with something useless (closed), 0xc..0xf with a scaled copy (retuned); on the output stage the read flattens the response (a subtraction) | CHIP. FW: the field is 0 or 0x11..0x1b on every FS1R/AN1x step |
| `wdst` `r6[5:0]` | **write slot** `w[wdst]` of the step's result; `rsrc` reads the same slots | CHIP: exact-match, 55/63 alternatives run away, bit-3 neighbours retune |
| `mod` `r9[7]`, `r9[15]`, `r7[9]` | modifiers: `r7[9]` ignored on the MAC probed; `r9[7]` and `r9[15]` share one signature (partial loss) | CHIP |
| `r9[12]` | one flip (0x11->0x01) opened the filter; not swept | CHIP |
| `daddr` `r8[15:7]` | 9-bit data-memory address: lookup-table pointers into other steps' constants | FW: the firmware patches table indices here; the four channel copies differ only in these bits |
| `mmode` `r8[6:0]` | delay-memory access mode: 0x40 on VOP3-2 reads, 0x28/0x30/0x68 on the AN; 0 on VOP3-1 (no DRAM) | FW |
| `mem` `r10[2:0]` | non-zero = delay-memory access this step, slot = step >> 2 | FW: the AN uploaders write 0x1A/0x1C for exactly these steps; VOP3-2's slot lists match |
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
through rB (`r[30]` = 0x4000 -> a constant, zero-variance output). op 1 ignores rA. op 0 with rA alone:
rA = 7f gives exactly `s + k*s` (as rA = 0), rA = 10..70 give 1.82..1.88 at coherence 0.97, so there the
operand takes in part of a live register. Unloaded `r[]` registers carry a live
chip-written signal (the input window, section 1).

**Session 14 (`captures/2026-10-01-0454-s14/README.md`), class-1 loads as known operands:** a class-1 load
is 8.8 fixed point (`r = v / 256`); the running value / DRAM word clips at +-8 (seen at two output gains);
the running value carries from step to step unscaled by route (op 0 `s + k*x` gives the same 4.000 at
`0d1`, `0d2` or `0d3`); op 1 reading `r[rB]` gives `x + k*x`; `rd-en` with rB set drops `k` and never
brings `w[rsrc]` in (`x` alone, 4.000 for w[0e] live or w[0f] unwritten), with rB clear `k` stays (session
12): VOP3-1's `0f8` (session 9) and VOP3-2 agree. op 0 ignores a loaded `r[rA]` alone.

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
rB are both set; running value `s` (`-s` with `sel`); operand `x` = `s` when rB = 0, `r[rB]` when only rB
is set, `r[rA] x r[rB]` when both are; op 0 `s + g*x`, op 1 `g*x` (`+ x` when rB is set), ops 2/3 `g*x`,
op 4 `s`, op 5 sign of `s`, op 6 zero, op 7 `g x` the chip's input (0 on route 3); every result clips at
+-8. `wdst` writes into `w[]`; route 0 of an op-1 step goes onto the output bus. Its self-check reproduces
session 11's `held` table and `dc dram` silence, session 12's `sel` gains, session 14's eight stored
values, and session 8's cutoff MAC and per-channel step ownership. Not modelled: route's scale on a
step's DRAM write, the 2-sample output lag, the 18-bit output word, delay memory.

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

* **`r[rA] x r[rB]` with both set.** Session 14's product loads clipped at 8; the scale of the product (and
  whether op 0/2/3 form it, as VOP3-1's cutoff MAC suggests) needs loads near 0x0010.
* **op 1 with rB: `x + k*x`.** Fits session 14's four op-1 points; not split from other forms that give the
  same numbers at k = 0x2000 and ~1 (a k = 0xc000 point would split it).
* **op 0 with an unloaded rA alone** gives 1.82..1.88 (session 13) where a loaded rA is ignored (session 14).
* **Pipeline latency.** VOP3-2's right path trails its left by 2 samples through identical chains
  (sessions 11-13); the register write delay between steps is not measured. On the AN1x every `rd-en`
  read sits 1..14 steps after its writer, so a write delay is at most one step.
* **Which generator feeds which input.** `r[1+5k+v]` is settled as the input window (above; session 13
  sees the same live window from VOP3-2); what k=0..12 carry (audio in, LFO1/2, noise, EG) is inferred
  from the reader shape only (k=0: squared by op 5 at the oscillator start; k=4,5,7,8: the filter
  block's op 2/1/4 trio; k=9: the mixer; k=10,11: the output block). The PLG150-AN reads `r[07..1a]` in
  the same pattern with fewer voices and would pin the stride if its handlers were mapped.
* `r9[12]`, `r7[11]`: unswept. `path` `r9[11:8]`, `r8[6:0]`, `r10`: inert on every step probed (VOP3-1
  pass steps, session 10; VOP3-2 `0d3`/`0e9`, session 12), unswept inside a feedback loop.
* **Delay memory.** `daddr` and the DRAM offset registers are inert on test image 0, which has no delay
  line (session 12); their effect needs a program that has one (an effect program's delay).

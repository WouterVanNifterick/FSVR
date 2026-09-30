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
* State the word addresses: one **register file `r[0..0x7f]`** (7-bit addresses in `r9`/`r6`), whose
  **upper half `r[0x40..0x7f]` is the write area** (the 6-bit `wdst`/`rsrc` fields name `r[0x40 | n]`),
  **data memory `d[0..0x1ff]`** (9-bit address in `r8`, one entry per step: the constant table seen as
  memory, used for lookup tables), and on chips with DRAM a **delay memory** by slot (`r10[2:0]` marks
  the access, slot = step >> 2, base/length in registers 0x1A/0x1C per slot at upload). FW: AN uploaders,
  FS1R VOP3-2. The write-area placement is FW (`tools/vop3_verify.py`): on every image, 97% of `rA`/`rB`
  reads at or above 0x40 name a slot some class-2 step writes (AN1x 321/332, FS1R 421/432; PLG150-AN 89/114 with
  its base image overlaid), against 21%
  for the same reads taken as class-1 destinations; the leftovers are `r[40]`, read and never written on
  all three machines (the zero register, as the MEG wires `r00`/`m00` to 0), and `r[6b]/r[6c]`, `r[63]/
  r[64]` written by a class-3 step. Reads below 0x40 hit class-1 destinations only, and on the AN1x every
  one of `r[01..3f]` is read twice or three times by the voice mixer with no writer in the image: they are
  the CPU-written voice state (pitch, EG, LFO) the driver's handlers keep, not program results.
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
| `wdst` `r6[5:0]` | **write slot** of the step's result, `r[0x40 \| wdst]`; `rsrc` reads the same slots | CHIP: exact-match, 55/63 alternatives run away, bit-3 neighbours retune |
| `mod` `r9[7]`, `r9[15]`, `r7[9]` | modifiers: `r7[9]` ignored on the MAC probed; `r9[7]` and `r9[15]` share one signature (partial loss) | CHIP |
| `r9[12]` | one flip (0x11->0x01) opened the filter; not swept | CHIP |
| `daddr` `r8[15:7]` | 9-bit data-memory address: lookup-table pointers into other steps' constants | FW: the firmware patches table indices here; the four channel copies differ only in these bits |
| `mmode` `r8[6:0]` | delay-memory access mode: 0x40 on VOP3-2 reads, 0x28/0x30/0x68 on the AN; 0 on VOP3-1 (no DRAM) | FW |
| `mem` `r10[2:0]` | non-zero = delay-memory access this step, slot = step >> 2 | FW: the AN uploaders write 0x1A/0x1C for exactly these steps; VOP3-2's slot lists match |
| constant (reg 0xB) | signed 1.15; **the third operand of a class-2 step when `rd-en` is clear** (`rd-en` set swaps `r[0x40 \| rsrc]` in for it), the loaded value on class 1 | FW: the firmware patches k into class-2 steps on all three machines (FS1R gain `x120`, mode `x7c`, resonance A/B, type; AN1x mixer levels `x67`, pitch, PW, edge, sync; 69 of 69 AN1x handler targets and 112/112 FS1R targets are class 2 with `rd-en` clear), and a live constant on an `rd-en` step is rare: FS1R 0 of 240, AN1x 20 of 276 (7%, against 86% of the others), PLG150-AN 24% against 43%. So the read replaces the constant on the FS1R and nearly always on the AN; where both are present (the AN's 0x35c3 mixer group) the step has four sources and the MEG's `const * reg + reg` form is the reading. CHIP: session 9's 13 values on `0f8` changed nothing, and `0f8` is an `rd-en` step, so the two agree; the class-1 mute above 0x8000 stands |
| tag (reg 0xC) | write-group id the firmware uses to patch steps by parameter: 3..7 = voice 0..4 on the AN1x, 1 = global; 0/1 on the FS1R filter | FW |

## 3. Operations (`op` = `r7[8:6]`)

| op | on the cutoff MAC (`098`: rA=62 rB=61 -> r[7e]) | at the output tap (session 10) | MEG counterpart |
|---|---|---|---|
| 0 | runaway | passes rA | `p =s ... + p` accumulate |
| 1 | closed | floor (~0.02): does not pass rA; 100% carry a read | `p = m` / `p = r` forward move |
| 2 | **corner follows rA x rB** | passes rA | `p = c * r` multiply |
| 3 | closed | passes rA | `p = (c<<8) + (p>>15)` mul-acc |
| 4 | closed | passes rA | mul-acc with memory |
| 5 | runaway | floor (~0.02): does not pass rA; lowest read rate | `p = c * r` fresh product (discards accumulator) |
| 6 | closed | passes rA; 100% carry a read | accumulate-forward |
| 7 | closed | passes rA | shifted mul-acc |

**The op field is an ALU-op selector, but its arithmetic cannot be read from one probed step.**
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
08c  class 2  op 5  -> r[53]  rB=7f                  gain term: cleared, the loop runs away (+83 dB HF)
08d  class 2  op 4  route 1 -> r[45]                 mode: cleared, the resonance peak moves to 80-160 Hz
08e  class 1  r[61] = k                              staged cutoff constant (log frequency, tick-driven)
098  class 2  op 2  path 1  rA=62 rB=61 -> r[7e]     the MAC whose result the corner follows
09c  class 2  op 2  path 1  rA=62 rB=62 -> r[7d]  rd r[41]
0a8  class 2  op 2s        rA=62 rB=61 -> r[7f]      the resonance peak (cleared: -19 dB at 320-1280 Hz only)
0c4..0c7, 0d4..0d7                                   the two biquad sections: a state write (silence when
                                                     cleared), a feedback term (+19/+24 dB blow-up), a runaway
0e8  op 5  rA=1b rB=1b -> r[57] rd r[41]              state write (silence when cleared)
0f0  op 1  sel  rB=1c -> r[63] rd r[41]               output stage, bus A (-16 dB flat when cleared)
0f8  op 1  sel  rB=1d          rd r[41]               output stage, bus B (-16 dB flat when cleared)
100  class 1  r[1e] = k (k=8000)                     control word: a negative value mutes the output stages
```
Channel c adds c to every step address from 0x098 on and 3c to the first three, with its own registers.
The cutoff-byte-to-corner law is `docs/filter.md` (`15_filter`).

## 5. AN1x parameter labels

`docs/an1x_param_map.md` (from the scene-parameter table at `0xCFCEC`) names, for each of 56 knobs,
the steps whose constant it rewrites; `docs/vop3_disasm/an1x_voice.txt` carries them. Cutoff is a
class-1 load followed by a `path 6, op 0, class 1` step (a log->linear conversion, as the EX5 MEG program
does with an exp table); VCO pitch 4096/octave with a key term; mixer levels `level*0x67`.

## 6. Interpreter

`tools/vop3_interp.py` runs a program over the measured fields: class-1 loads, class-2 `op 2`/`op 5` =
`rA x rB`, `op 1`/`op 6` = forward-move, `op 0`/`3`/`4`/`7` = `rA x rB + c` (multiply-accumulate) with
`c` the `rd-en` read or, when it is clear, the step's constant; `wdst` writes into `r[0x40 | wdst]`; route 0
of an op-1 step onto the output bus.
The op arithmetic is the MEG mapping of section 7, not FS1R gate-level truth (`Interp.ops` takes
replacements to test an alternative). Its self-check reproduces session 8/9 qualitatively: clearing the cutoff load zeroes the MAC, `sel=0`
on the output stage emits DC, a class-2 constant changes nothing, and the per-channel step ownership.

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

**Dataflow check (`tools/vop3_verify.py`).** The same test the MEG listing passes trivially (every `r`/`t`
read in `ex5_meg_an.txt` has a writer in the program, 203/203 and 57/57) is what settles the VOP3 register
map: with `wdst` taken as `r[0x40 | n]`, 97% of upper-half reads have a writer on all three VOP3 images;
under the old separate-`w[]` reading only `rd-en` reads did (AN1x 276 of 508 operand reads, FS1R 357 of 576),
so the interpreter was running two thirds of the program on zeros. The constant-liveness test (which steps
the firmware patches, which carry a non-zero k) is the second half of the same tool and is what moved the
constant from "class 1 only" to "class-2 operand unless `rd-en`".

## 8. Open (not closable on this hardware)

* **Exact per-op arithmetic** (which shift, signed/unsigned, what the read subtracts) is fixed only up
  to the MEG mapping above; the FS1R exposes no accumulator, so the last bits are inferred from the MEG,
  not measured on the FS1R. Good enough to interpret; not a gate-level truth for the FS1R silicon.
* **Pipeline latency.** The MEG delays register writes by 3 steps; on the AN1x every `rd-en` read sits
  1..14 steps after its writer with 1 and 2 both common, so if the VOP3 has a write delay it is at most
  one step. Not measured.
* **Lower-half registers on the AN1x** (`r[01..3f]`, all read, none written): CPU-side voice state. Which
  handler writes which register is readable from the driver (`FUN_000564a0` writes `base | 0x16` = reg 0xB
  at the step in `er1`); not yet mapped.
* `r9[12]`, `r7[11]`, `r10[15:3]`, `mmode` values, `path-hi`: inert on every pass step probed; unswept
  inside the loop.
* Delay memory (VOP3-2, AN): structure read from the uploaders only.

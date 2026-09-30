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
  **write area `w[0..0x3f]`** (6-bit addresses in `r6`/`r7`), **data memory `d[0..0x1ff]`** (9-bit
  address in `r8`, one entry per step: the constant table seen as memory, used for lookup tables),
  and on chips with DRAM a **delay memory** by slot (`r10[2:0]` marks the access, slot = step >> 2,
  base/length in registers 0x1A/0x1C per slot at upload). FW: AN uploaders, FS1R VOP3-2.
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
| `wdst` `r6[5:0]` | **write slot** of the step's result; takes the values `rsrc` reads | CHIP: exact-match, 55/63 alternatives run away, bit-3 neighbours retune |
| `mod` `r9[7]`, `r9[15]`, `r7[9]` | modifiers: `r7[9]` ignored on the MAC probed; `r9[7]` and `r9[15]` share one signature (partial loss) | CHIP |
| `r9[12]` | one flip (0x11->0x01) opened the filter; not swept | CHIP |
| `daddr` `r8[15:7]` | 9-bit data-memory address: lookup-table pointers into other steps' constants | FW: the firmware patches table indices here; the four channel copies differ only in these bits |
| `mmode` `r8[6:0]` | delay-memory access mode: 0x40 on VOP3-2 reads, 0x28/0x30/0x68 on the AN; 0 on VOP3-1 (no DRAM) | FW |
| `mem` `r10[2:0]` | non-zero = delay-memory access this step, slot = step >> 2 | FW: the AN uploaders write 0x1A/0x1C for exactly these steps; VOP3-2's slot lists match |
| constant (reg 0xB) | **read only by class 1**, signed; a class-2 step ignores it | CHIP: 13 values on a class-2 step, no change; on a class-1 step 0..0x7fff no change, 0x8000..0xffff mute (a control word to its consumer) |
| tag (reg 0xC) | write-group id the firmware uses to patch steps by parameter: 3..7 = voice 0..4 on the AN1x, 1 = global; 0/1 on the FS1R filter | FW |

## 3. Operations (`op` = `r7[8:6]`)

| op | on the cutoff MAC (`098`: rA=62 rB=61 -> w[3e]) | on the output stage (`0f8`: rd w[1], sel=1) |
|---|---|---|
| 0 | runaway | nothing |
| 1 | closed | **output move**: audio to the bus at 0 dB; sel=0 or route 3: a constant (DC) instead |
| 2 | **shipped**: the corner follows rA x rB | nothing |
| 3 | closed | nothing |
| 4 | closed | nothing |
| 5 | runaway | **+10 dB, V-shaped**: the unfiltered input; identical to rd-en cleared |
| 6 | closed | nothing |
| 7 | closed | nothing |

op 2 is multiply-class, op 1 move-class, op 5 on the output stage bypasses the read. The arithmetic of
each op (multiply vs multiply-accumulate, shifts, what the read subtracts) is **not measured**: every
probe so far reads through the filter loop or the output bus, both nonlinear in the step's result.
The FS1R program uses ops {0, 2, 3, 4, 5, 7} on class-2 steps, the AN1x {0, 2..7}.

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
class-1 load followed by a `path 6, op 0, class 1` step (a log->linear conversion, as the EX5 MEG program
does with an exp table); VCO pitch 4096/octave with a key term; mixer levels `level*0x67`.

## 6. Interpreter

`tools/vop3_interp.py` runs a program over exactly the fields above and nothing further: class-1 loads,
class-2 `op 2` = `rA x rB`, `op 1` = move (audio with `sel`, a constant without), `op 5` = pass, the
`rd-en` read as a subtraction, `wdst` writes, route 0 of an op-1 step onto the output bus. Ops 0, 3, 4,
6, 7 return 0 until measured (`Interp.ops` takes replacements to test a hypothesis against the takes).
Its self-check reproduces session 8/9 qualitatively: clearing the cutoff load zeroes the MAC, `sel=0`
on the output stage emits DC, a class-2 constant changes nothing, and the per-channel step ownership.

## 7. Open

* **Arithmetic per op**: feed the output stage's op 1 from a register a class-1 step just loaded, and
  sweep the loader's k and the stage's op; the bus is linear in what op 1 moves.
* `r9[12]`, `r7[11]`, `r10[15:3]`, `mmode` values, `path-hi`: unprobed or single flips.
* Delay memory (VOP3-2, AN): read from the uploaders only.
* The EX5 MEG AN program (`docs/ex5_meg.md`) computes the same algorithm with a decoded ISA and is the
  check for any AN interpretation once the op arithmetic is in.

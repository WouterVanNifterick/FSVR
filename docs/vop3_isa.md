# VOP3 (YSS236) instruction word: what is settled, what is not (2026-09-30)

Companion to `tools/vop3_disasm.py`. Every claim here names its evidence. The programs under
`docs/vop3_disasm/` are the disassembler's output over every VOP3 image we have: FS1R VOP3-1
(filter, 2 variants), FS1R VOP3-2 (base + 4 reverb + 16 variation + 2x12 insertion), PLG150-AN
(3 voice modes + base).

## Per-step upload, both chips, both drivers

A step is five 16-bit words written to registers 10, 9, 8, 7, 6 after register 0 selects the step
(FS1R `FUN_0000B600`/`FUN_0003C9C4`, PLG150-AN `FUN_0007f87c`). Alongside, indexed by the same step
address: register 0xB = 16-bit constant (1.15), register 0xC = a byte, registers 0xD/0xE (VOP3-2)
or 0x1A/0x1C (AN) = a delay-memory address at slot `step >> 2`.

The FS1R's VOP3-2 upload (`FUN_0003C9C4`) walks `step*10 + k*2`, the same interleave as VOP3-1.
`tools/extract_vop3_2.py` had this as word-major and produced a scrambled image; fixed, and it now
also extracts the effect programs that `FUN_00039C8A` uploads (the boot image is a skeleton).

## Fields with a settled meaning

| field | bits | meaning | evidence |
|---|---|---|---|
| `r10[2:0]` | 3 | delay-memory access; slot = `step >> 2` | AN driver writes regs 0x1A/0x1C only when `r10 & 7`; VOP3-2 effect programs have `r10 != 0` exactly on the steps whose `step >> 2` is in the effect's DRAM slot list (`FUN_000397F4` arg) |
| `r8[6:0]` | 7 | memory-access mode; 0x40 on every VOP3-2 read, 0x48/0x4C/0x04/0x08 variants, 0x28/0x30/0x68 on the AN | non-zero on exactly the `r10 != 0` steps in VOP3-2 (plus 2 outliers per effect); always 0 on VOP3-1 |
| `r8[15:7]` | 9 | data address into the 512-entry constant/byte tables | FS1R filter: `r8 >> 7` points at steps whose constant is non-zero (47/147); the 4 per-channel copies of the filter block differ in exactly these bits (block-diff bit map below) |
| `r9[6:0]` | 7 | register address A, per-channel | block diff: only `r9[6:3]` changes between the four channel copies (0x61,0x62,0x63 -> 0x69,0x6a,0x6b ...) |
| `r6[13:7]` | 7 | register address B, per-channel | block diff: `r6[13:10]` changes between channel copies |
| `r6[15:14]` | 2 | step class: 1 = parameter-load step (every firmware-patched constant slot has it), 2 = compute step, 0 = idle/const, 3 = rare (VOP3-2 reverb write-back, FS1R 0x046..0x053) | FS1R type/cutoff/reso/gain tables all land on `r6t=1` or `r6t=2 & byte=1` steps |
| `r7[5:0]`, `r6[6:0]` | 6/7 | share value ranges (0x11..0x1b, 0x25..0x2a, 0x3d..0x3f, 0x61..0x63): a third register/port index | value sets coincide across all three program families |

Block-diff bit map (FS1R filter, channel copies at 0x010/0x08C/0x108/0x184, 124 steps), bits that
ever differ between copies:

```
r10 0000000000000000   r9 0000000001111000   r8 0111111110000000
r7  0000000000000000   r6 0011110001111111
```

Everything in r7, r10, `r9[15:7]`, `r9[2:0]`, `r8[6:0]` and `r6[15:14]` is identical across the
four copies: those are opcode/mode bits. `r6[6:0]` changing per copy means it is an address too
(or an address-like port index), not an opcode.

## Opcode bits, not yet decoded

`r9[15:8]` (values 0x11..0x18 in the filter's biquad section, 0x40/0x41/0x60 on the AN, 0x19/0x1a/0x59/0x5b on
effects), `r7[15:6]` (`4480`, `00c0`, `0140`, `1100`, `8000`, `9280`, `82d1/82d2`, `40c0`, `01c0`,
`6100`, `4100`, `4000`, `a140`, `8040` are the filter's whole vocabulary) and `r9[7]`.

## Hard block

The MEG's per-step semantics (`p = a*b + c` with source/destination selectors) cannot be mapped onto
these bits without a ground truth. Three things would settle it, none available here:

1. **Register captures from the unit** while a filter parameter sweeps: FSVR's `fs1r_uart_probe.py`
   register sessions already log the CPU side; a companion audio capture with cutoff at two known
   settings lets the constant-table entries be tied to biquad coefficients and from there the
   `op9`/`r7` words to MAC operations. This is the FS1R.unlock path.
2. **The AN1x/AN200 firmware**: same chip, and the AN200's service manual (`docs/`) plus a ROM dump
   would give a third driver with its own comments-in-tables.
3. **An emulator diff**: run the EX5 MEG AN program (decoded, `docs/ex5_meg.md`) and the PLG150-AN
   VOP3 program side by side with identical constants once fields 1-6 above are pinned; the MEG
   listing says what each of the 45 memory slots and 230 steps must compute.

Until one of those lands, the disassembler prints fields, not operations.

## AN1x (an1x_v104.bin, H8/3002) — third driver, and the first with parameter labels

`tools/extract_an1x_vop3.py` → `docs/vop3_an1x/{voice,boot}.bin`; listing in `docs/vop3_disasm/an1x_voice.txt`
(annotated). Uploader `FUN_00056028` uses the identical register protocol (reset via reg 1, mode 5 = 0x1004,
regs 0xC/0xB/10..6 per step, 0x1A/0x1C delay slots when `r10&7`). Image row = 6 words: coef, `r10 | byte<<4`,
r9, r8, r7, r6 — so **reg 0xC (the "byte table") is packed into the r10 word by Yamaha's own tooling**, i.e.
it is part of the instruction, not a side table.

The AN1x scene-parameter table at `0xCFCEC` (0x22 bytes/entry, index = scene sysex address, see
`docs/an1x_param_map.md`) names, for every knob, the VOP3 step(s) whose constant it rewrites (`FUN_000564a0`
writes reg 0xB at a step; `FUN_000564f4`/`FUN_00056562` write reg 0xB + reg 0xC). That gives:

| parameter | steps (5 voices) | step shape | what the constant is |
|---|---|---|---|
| VCF Cutoff | 165 16d 174 17c 185 | `r6t=1 op9=40 r7=1.2.00.0`, k | log-frequency, table `0xCADB0`: 85.3 units/step = 1024/octave |
| VCO1/2 Pitch+Fine | 004/009, 0e3/0e7 | 009: `op9=01 rA=4f r7=1.0.02.0` | 341/semitone (4096/octave) + fine 3/cent, + key/PB term `((note&0x7f)+(oct&7)*0x80-0x200)` |
| Mixer VCO1/VCO2/Ring/Noise | 148.. 149.. 147.. 14a.. (stride 5) | `r6t=2 op9=00 r7=0.0.00.0 rB=..`, byte=voice+3 | `level*0x67` (linear gain, 0x7f→0x3339) |
| VCO Edge, PWM depth/src, Sync pitch/depth/src, FM src | see map | `r6t=2` steps in the oscillator blocks | — |
| VCA Feedback | 146 | `op9=01 rB=5c r7=1.2.01.1` | — |
| VCF Mod / VCA Mod depth | 1f2/1f1, 1ee/1ed | — | — |

So: the step following every cutoff constant is `op9=60 r7=0.3.00.1 r6t=1` — a log→linear conversion (the EX5
MEG AN program does the same with an exp table in reverb RAM). The FS1R filter has the same *semantic* (per-channel
cutoff/reso/gain constants patched by the firmware into `r6t=1` steps 0x12/0x15/0x18, 0x8e/0x91/0x94, …) with a
different opcode word (`r7=8000`), consistent with the FS1R computing its coefficients on the H8S and the AN
computing them on the DSP.

**Reg 0xC ("byte")**: on the AN1x it is 3..7 on exactly the 5×23 per-voice parameter-patchable steps and 1 on
the global ones (VCA feedback, pitch mod), 0 elsewhere; on the FS1R filter it is 0/1 per step. It is a per-step
tag the firmware uses to select which steps a patch applies to — a write-group id, not data.

Still open (the hard block for an interpreter): the mapping of `r7[15:6]`, `r9[15:7]` and `r8[6:0]` to the
multiplier/accumulator/table operations. Next lever: the AN1x per-voice step lists give 23 labelled steps per
voice × 5 voices; aligning those labelled steps against the EX5 MEG AN program (`docs/ex5_meg.md`, whose
operations are known) is now a labelled matching problem rather than a blind one.

## Measured on the chip (2026-09-30, FS1R.unlock `captures/2026-09-30-9`)

Four takes on rgwan's unit through the firmware's own patch path (`FUN_0000B600` from the CPU shadow,
which is the only safe write: VOP3 register 0 is an address latch the 192 Hz tick also uses). One
held note through the filter, one program step rewritten at a time, octave bands against the reference.
Full tables in that folder's README; what they settle for the instruction word:

**Program layout.** In a 124-step group, the four filter channels are interleaved: channel c owns steps
`base+3c..base+3c+2` (gain, mode, cutoff constant) and then every step `base+0x0c+c+4n`. Clearing any of
a channel's 31 steps changes its output; clearing any other step of the group does not.

**Field roles, as measured (the disassembler's `fields()` carries the same notes):**

| field | measured role |
|---|---|
| `r6[15:14]` | step class. 1 = constant load: the staged reg-0xB value goes to register `r9[7:0]`, and **no other bit of r6 matters**. 2 = MAC. Flipping the class to 0 silences the step's contribution, to 3 makes the loop run away |
| `r9[7:0]` on class 1 | destination register (0x61 for channel 0's cutoff). Every bit matters |
| `r9[6:0]` on class 2 | source register A. Every bit matters: any other register gives a smaller operand and the corner retunes to ~200 Hz |
| `r9[7]` | a mode bit of its own on the MAC step (partial loss); same signature as `r9[15]` |
| `r9[11:8]` | the operation. All four bits of `0x1` on the cutoff MAC lose the corner |
| `r9[12]` | changes the operation (`0x11 -> 0x01`: +14 dB above 640 Hz, the filter opens). `r9[14:13]` inert |
| `r6[13:7]` on class 2 | source register B (0x61 = where the cutoff constant was loaded). Bits 7-10, 12, 13 all lose the corner; bit 11 is inert on this step |
| `r6[5:0]` | six-bit mode. Bits 0, 1 lose the corner; 2, 4, 5 run away; 3 retunes; **bit 6 inert** |
| `r7[15:14]` | **inert on all three probed steps.** Not an opcode bit the chip acts on here |
| `r7[13:12]` | result destination select. Either bit set on any of the three steps breaks the loop, usually as the gain-step-lost runaway |
| `r7[10]`, `r7[7]`, `r7[6]` | break every step they were probed on |
| `r7[4]`, `r7[8]` | break the MAC step; `r7[4]` also the constant load |
| `r7[3:0]`, `r7[5]`, `r7[9]`, `r7[11]` | inert where probed (all zero on those steps) |

**The cutoff data path, read off the chip:** step `08e` loads the staged cutoff constant into register
0x61; step `098` is `op 1, mode 0x3e: reg 0x62 * reg 0x61` and the corner follows its result; step `08c`
(`r7=0140, mode 0x13`) supplies the term without which the loop runs away, i.e. the unity/gain term of
the ladder. The cutoff byte to corner law itself is `15_filter`'s and is in `docs/filter.md`.

**Next take** (`fs1r_capture_session8.py values`, 2.8 min): every value of `r9[11:8]`, `r7[13:12]` and
`r6[5:0]` on the MAC step, so the operation and mode fields get a table instead of a bit mask.

### Value sweeps (take 5, channel 1's MAC step 0x099)

* `r9[11:8]`: **exact-match**. All 15 other values close the filter identically. Channel 0's MAC steps
  carry 1, channel 1's 2, and the field runs 1..0xb across the group: a per-data-path id (bus,
  accumulator or pipeline slot), not an opcode menu. The disassembler prints it as `path=`.
* `r7[13:12]`: 1 is right; 0 drops the corner an octave and the loop lives; 2, 3 run away. Printed
  as `route=`.
* `r6[5:0]`: **exact-match**; 55 of 63 other values run away, five close, 0x34/0x36 retune. So it is an
  address, and the values it takes on class-2 steps (0x13, 0x15..0x17, 0x1c, 0x20, 0x21, 0x23, 0x3d..0x3f)
  are the same range `r7[5:0]` takes on later steps (0x11..0x13, 0x18..0x1b), with `r7[5:0]` mostly the
  channel's own tag. Read: `r6[5:0]` is the write address of the step's result, `r7[5:0]` the read
  address. Printed as `-> w[..]` and `rd w[..]`.

Still unswept by value: `r7[10:6]` (bits 10, 7, 6 break every step) and `r7[5:0]` itself; that is the
next run (`fs1r_capture_session8.py`, default `r7values`, 3.1 min).

### r7 value sweeps (take 6, channel 2's MAC step 0x09a, `r7=9280`)

* `r7[5:0]`: **bit 4 is a read enable; with it clear all sixteen values of `r7[3:0]` are inert, and
  bit 5 changes nothing either way.** Enabled, `r7[3:0]` = 0..0xb close the filter (an operand replaced
  by a register the corner cannot use) and 0xc..0xf retune it (a register holding a scaled copy).
  Matches the programs: the field is 0 or 0x11..0x1b on every FS1R and AN1x step, never 0x20+.
  Printed as `rd w[n]` only when enabled.
* `r7[10:6]`: 0x02 and 0x0a (the step's own) are interchangeable, 0x10 and 0x18 both open the filter
  with the peak intact, so **`r7[9]` is a modifier this step ignores**. `r7[10]` alone opens the filter.
  Of the remaining codes, 0x00 (and most odd values) run away, the rest close. Read as `r7[10]` a
  selector plus **`r7[8:6]` a 3-bit MAC operation, code 2 on the cutoff MAC**: printed as `op=2`,
  with `s` appended when `r7[10]` is set. The FS1R program uses op codes {0,2,3,4,5,7} and the AN1x
  {0,2..7}; op 0 appears on class-0 and gain-type steps.

What the instruction word now reads as, per class-2 step:
`path=r9[11:8] rA=r9[6:0] rB=r6[13:7] op=r7[8:6] route=r7[13:12] -> w[r6[5:0]] [rd w[r7[3:0]]]`
with `r9[7]`, `r9[15]`, `r7[10]`, `r7[9]` modifiers and `r7[15:14]`, `r9[14:13]`, `r6[6]`, `r7[5]`
inert on the probed steps. Still not measured: what each op code computes (needs a step whose two
operands are both known constants, i.e. a constant-load pair feeding a MAC into an output stage), and
the memory word r8 (nothing in the filter reads it except the d[] table pointers).

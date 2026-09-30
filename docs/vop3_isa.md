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

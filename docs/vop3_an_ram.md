# The VOP3's external RAM on the AN boards, and the AN1x mask ROM

The PLG150-AN hangs a DRAM off its VOP3 and the AN1x hangs a 1 Mbit mask ROM off the same pins. This note reads what the PLG150-AN's H8 writes into its RAM, what the two voice programs read out of it, and rebuilds the AN1x mask ROM from both. `python tools/vop3_an_ram.py` prints every number below and asserts the checks; `--probe` runs both programs through `yss236_core` (`tools/vop3_an_ram.cpp`). Nothing here was measured on hardware.

## What the PLG150-AN uploads

`FUN_00081358`, called once from the boot sequence `FUN_00080a3a` after the microcode is in, writes 0 to VOP3 registers 0x0F and 0x10 (the RAM address, high and low), then for each little-endian word of the 0xC242 bytes at ROM 0xC245C writes 0 to register 0x12 and the word to register 0x11. The address auto-increments, so RAM words 0x0000..0x6120 hold the image (`docs/vop3_an/ram.bin`, as the chip receives it). The H8 drivers name registers by byte offset (base + 2 x reg, as `FUN_0000B5E2` on the FS1R), so the decompilation shows these as `| 0x1e`, `| 0x20`, `| 0x22`, `| 0x24`. No other PLG150-AN function writes registers 0x0F..0x12, and no AN1x function does at all. It is a one-time table load: nothing writes the RAM afterwards, from the CPU or from the program.

The image is nine tables, word offsets:

- `0000..0FFF` exp, `-16384 * 2^(i/4096)`, one octave
- `1000..101F` powers of two, -1 .. -32768, each twice
- `1020..181F` decay, `32767 * 2^(-i/256)` over eight octaves
- `1820..281F` sine, one cycle of 4096
- `2820..381F` the same sine again
- `3820..481F` an odd periodic shape, four periods of 1024, peak 1552
- `4820..5820` a soft clipper, +-18022 over 4097 words, odd about 5020
- `5821..5929` zeros, then 1 .. 16384 in powers of two, then a run of 32767
- `592A..6120` a reciprocal, 29127 falling to 128

## What the programs read

The RAM is addressed per delay slot, as on the FS1R's VOP3-2: slot = step >> 2, and the slot's offset is 9 + 9 bits in registers 0x0D/0x0E. The PLG150-AN uploader `FUN_0007f87c` writes `w1 >> 9` to 0x0D and `w1 & 0x1FF` to 0x0E, so the 18-bit offset is `w1` itself; the AN1x uploader `FUN_00056028` writes its slot pair to 0x0D then 0x0E. Both appear as `| 0x1a` / `| 0x1c` in the decompilations, which is why the docs called them registers 0x1A/0x1C until now: they are the FS1R's 0x0D/0x0E. The same reading settles `docs/vop3_outputs.md`'s open question: the AN drivers' 15-word table at `| 0x2c` is register 0x16, the FS1R's, not a second register. `tools/extract_an1x_vop3.py` paired the AN1x's slot words wrongly until this note (it took one half by step parity); `docs/vop3_an1x/voice.bin` is regenerated with the 18-bit offset in the slot column, in the PLG150-AN's packing.

With the patch lists of `FUN_0007f9e6` applied (0xC0598 / 0xC0C56 / 0xC1314 per mode on steps 105..162, 0xC19D2 on 163..1F6):

- PLG150-AN mode 0: 95 reads, 0 writes, offsets 00000 01010 01020 01C20 02020 05020
- PLG150-AN mode 1: 85 reads, 0 writes, offsets 00000 01010 01C20 02020 04020 05020 05921
- PLG150-AN mode 2: 85 reads, 0 writes, offsets 00000 01010 02020 02420 02576 025BA 02728 02820 02980 05020 058A1
- AN1x voice: 95 reads, 0 writes, offsets 00000 01010 01020 03420 03820 04820

Every offset lands inside one of the tables above, mostly at its centre (02020 is the sine's half-cycle, 05020 the clipper's zero, 01010 the middle of the pow2 pairs), which is where a signed index into a lookup belongs. No AN program has a single `mem 1` write. So on these boards the "delay memory" is a table ROM, which is why the AN1x can wire a mask ROM where the PLG150-AN has DRAM.

**Every RAM access on both AN boards sets `r8` bit 5 (`w2[5]`, decoded as `tbl` in `yss236_core.h`), and no access in any FS1R program does** (FS1R VOP3-2 marks carry `r8` low bits 0x00, 0x01, 0x03, 0x08, 0x40, 0x44, 0x48, 0x4C). That bit is the only structural difference between a table read and a delay-line read, so it is the candidate for "address absolutely, without the ring pointer". It is not measured.

## The AN1x mask ROM

PLG150-AN mode 0 and the AN1x voice are the same voice program with the same step count and the same read pattern: six offsets each, and the readers pair one to one.

- 00000 <-> 00000, 20 / 20 readers (exp)
- 01010 <-> 01010, 20 / 20 (pow2)
- 01020 <-> 01020, 5 / 5 (decay)
- 01C20 <-> 03420, 5 / 5 (sine)
- 02020 <-> 03820, 25 / 25 (sine)
- 05020 <-> 04820, 20 / 20, the same twenty steps (clipper)

Where the offset is the same, the table is at the same place; where it moved, the AN1x layout is shifted. The AN1x CPU ROM settles which: a big-endian copy of the tables sits at **0xE496C**, referenced from code at 0x6CD4C, and against the PLG150-AN image it holds

- exp at +0000, 4096 of 4096 words equal
- pow2 at +1000, the same sixteen powers, each once instead of twice
- decay at +1010, 0x2000 words at `2^(-i/1024)`: the PLG150-AN's table is exactly every fourth word of it (2048 of 2048)
- sine at +3010, 4096 of 4096 equal
- clipper at +4010, 4097 of 4097 equal
- a second clipper at +5011, +-4506, which the PLG150-AN does not carry

Laid in at the offsets the AN1x reads, it fits: the AN1x reads the sine at 3420 and 3820 where the CPU copy's sine starts at 3010 + 0x10 = 3020 (PLG: 1C20 and 2020 against a start of 1820, the same 0x400 and 0x800 in), and the clipper at 4820 where the copy's clipper centre is 4020 + 0x800. So `docs/vop3_an1x/maskrom.bin` (64 Ki words, little-endian, as `ram.bin`) is:

- `0000..0FFF` exp (PLG = CPU copy)
- `1000..101F` pow2 doubled, from the PLG image, read at 1010. The CPU copy has them single, so the mask ROM's own layout here is inferred from the PLG150-AN, and the copy's 0x10 shift of everything after is consistent with the doubled pairs taking 0x20
- `1020..301F` decay at the AN1x's 4x resolution, from the CPU copy
- `3020..401F` sine
- `4020..5020` clipper +-18022
- `5021..6021` clipper +-4506, the next table in the CPU copy. No AN1x step reads it, so its place is inferred
- `6022..FFFF` unknown, zero. The CPU copy runs on into other data after 0xF0990 (word 0x6012), and no step reads past 0x4820 + 0x800

What is solid: the five tables the AN1x voice reads (exp, pow2, decay, sine, clipper) and their offsets. What is inferred: the doubled pow2 (taken from the PLG150-AN), the second clipper's place, and everything past 0x6022. A dump of the AN1x mask ROM, or a read of the chip's RAM bus on a running unit, settles all three.

## Running it through the model

`tools/vop3_an_ram.py --probe` loads each program into `yss236_core`, fills the RAM so a read returns its own address, and runs 4800 passes:

- On the current rule (address = ring pointer + offset, the FS1R's delay line), 70% of reads land inside the table image and the rest wrap past it: the pointer walks the address down one word a pass, so a lookup turns into a slow scan of the whole RAM.
- Built with `-DYSS236_TABLE_ADDR` (a `w2[5]` read addresses offset + index, no pointer), 100% of reads land inside the tables, on the table each offset names.
- Either way the index the program adds (`w2[3]` reads, loaded by `w2[2]` steps, 45 per program) stays 0 in this probe, so every read is the table's base word. The index scale is open in `yss236_core.h` already, and the probe drives the serial inputs rather than the `r[01..3f]` hardware inputs the AN voice reads its pitch and modulators from, so this run says nothing yet about how far the index moves.
- The core transfers only on phases 1 and 3 (s16, s17). PLG150-AN mode 0 puts 57 of its 95 reads on phase 0 and 2 on phase 2; the AN1x 36 on phase 0 and 30 on phase 2. In the probe the clipper reads (05020, 04820) are all on phase 0 and never transfer. Phase-0/2 RAM reads join blocker 7 of `docs/vop3_blockers.md`, with a firmware argument now: these programs ship 66 to 59 table reads on phases the model calls dead, on boards where the table is the synthesis.

`YSS236_TABLE_ADDR` is a compile-time switch in the interpreter only, off by default, and the compiled pass declines to run when it is on, so nothing in the shipped engine changes: `vop3_jit_check` still compares 229,600 passes identical with the switch off.

## What would settle it

- The mask ROM: a dump of the AN1x's VOP3 ROM. Failing that, its address bus during a note.
- `w2[5]`: on the FS1R VOP3-2, a read step with `r8 | 0x20` against the same step without, on a slot whose offset is far from the write slot. If the bit drops the ring pointer, the read becomes a constant at the slot's absolute address.
- Phase-0 reads: the same probe on phase 0.

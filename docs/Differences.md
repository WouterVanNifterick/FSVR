# Where FSVR differs from the FS1R on purpose

Everything else in this repository is an attempt to match rgwan's unit, and a disagreement with it is a
bug (`AGENTS.md`, "Where a claim comes from"). This file is the short list of places where FSVR knowingly
does something the hardware does not, because the hardware's behaviour is an artifact of how its CPU talks
to its chips rather than a musical decision. Each entry says what the unit does, what FSVR does, and where
the firmware says so.

The engine has one entry, and it is a choice you can turn off. The last forced one, a filter switch that reached
notes already sounding, went on 2026-10-14 when the filter moved onto its chip: on the unit a change of part byte
0x07 runs `FUN_0002087E`, which stops every channel before it latches the four switches, and FSVR now does the same.

## 32 filtered voices

**The unit.** One VOP3-1 filters the tone generator's first sixteen channels. While any part has its filter on,
the allocator splits the 32 channels in two (`FUN_000112c0`): the parts with a filter play on channels 0 to 15
(`FUN_0000fa5c`), the others on 16 to 31 (`FUN_0000fcfa`), so filtered parts share 16 voices between them.

**FSVR.** A second VOP3-1, running the same program on the same calibration, filters channels 16 to 31. The
allocator then walks all 32 channels as it does with no filter, and a note is filtered when its part's switch is
on; a channel of a part without a filter goes straight to its bus. This is the default. Options > Special >
32 Filtered Voices off (`Device::setFilterPoly32(false)`, `-flt16` on the console) gives the unit's 16, and renders bit-identically to the single-chip engine: all fifteen demo songs were checked.
Changing it stops every note, as a filter switch does. Nothing about the second chip can be measured, since the
unit has none, so `tools/render_capture`, which renders what a capture is compared against, keeps the unit's
16 unless given `-flt32`.

# What the plug-in adds

The plug-in adds features the unit never had; none of them changes how the engine renders a voice, and each is something you choose to use. [editor.md](editor.md) has the detail.

## User libraries without the unit's memory limits

**The unit.** 128 internal performances, and 128 internal voices with no Fseqs or 64 voices with six, in battery-backed RAM.

**FSVR.** A folder of .syx files, each a bank, as many as you import or save to (Import > Import FS1R SysEx to New Bank names a new bank after the file). A performance's Int voices and Fseq mean its own bank's internal ones when the bank holds them, and your library's numbered ones otherwise. The factory banks are the unit's.

## Import Audio

**The unit.** It plays the 90 preset Fseqs and the six its internal memory can hold, which arrive as bulk dumps; it cannot make one from a sound itself.

**FSVR.** The Fseq page's Import Audio analyses a WAV, AIFF, MP3, Ogg Vorbis or MP4 file into an Fseq of your own, after fseq-flash's method, with the frame bytes on the engine's own scales. The result is an ordinary Fseq: it exports as a bulk dump the unit takes.

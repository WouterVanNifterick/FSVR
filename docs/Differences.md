# Where FSVR differs from the FS1R on purpose

Everything else in this repository is an attempt to match rgwan's unit, and a disagreement with it is a
bug (`AGENTS.md`, "Where a claim comes from"). This file is the short list of places where FSVR knowingly
does something the hardware does not, because the hardware's behaviour is an artifact of how its CPU talks
to its chips rather than a musical decision. Each entry says what the unit does, what FSVR does, and where
the firmware says so.

The engine has no entry left. The last one, a filter switch that reached notes already sounding, went on
2026-10-14 when the filter moved onto its chip: on the unit a change of part byte 0x07 runs `FUN_0002087E`,
which stops every channel before it latches the four switches, and FSVR now does the same.

# What the plug-in adds

The plug-in adds features the unit never had; none of them changes how the engine renders a voice, and each is something you choose to use. [editor.md](editor.md) has the detail.

## User libraries without the unit's memory limits

**The unit.** 128 internal performances, and 128 internal voices with no Fseqs or 64 voices with six, in battery-backed RAM.

**FSVR.** A folder of .syx files, each a bank, as many as you import or save to (Import > Import FS1R SysEx to New Bank names a new bank after the file). A performance's Int voices and Fseq mean its own bank's internal ones when the bank holds them, and your library's numbered ones otherwise. The factory banks are the unit's.

## Import Audio

**The unit.** It plays the 90 preset Fseqs and the six its internal memory can hold, which arrive as bulk dumps; it cannot make one from a sound itself.

**FSVR.** The Fseq page's Import Audio analyses a WAV, AIFF, MP3, Ogg Vorbis or MP4 file into an Fseq of your own, after fseq-flash's method, with the frame bytes on the engine's own scales. The result is an ordinary Fseq: it exports as a bulk dump the unit takes.

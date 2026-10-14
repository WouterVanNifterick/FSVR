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

## The morph square (new)

**The unit.** A part plays one voice. Moving between two sounds means a controller set routed to a few destinations (the 48 in the Data List's list), or loading another voice.

**FSVR.** Each part holds four corner voices, and the Quick Control page's morph square (off for now) blends them into the one voice the part plays: amounts (levels, rates, frequencies, depths, key scaling) bilinearly by the square's position, choices (the algorithm, forms, waves, switches, Fseq tracks, the formant and FM control routing) from the nearest corner. A random amount on each axis moves each note's position from a seed. The processor does it outside the engine, as an editor would: moving the square sends the part one voice bulk, and the engine plays that voice exactly as the unit would play it. With all four corners the same, which is where every part starts, nothing differs from the unit.

**What it costs.** The engine holds one voice per part, as the unit does, so a morph position reaches the notes already sounding along with the next one, rather than each voice moving on its own. A voice exported or stored from a morphed part is the corner the part edits, not the blend.

## User libraries without the unit's memory limits

**The unit.** 128 internal performances, and 128 internal voices with no Fseqs or 64 voices with six, in battery-backed RAM.

**FSVR.** A folder of .syx files, each a bank, as many as you import or save to (Import > Import FS1R SysEx to New Bank names a new bank after the file). A performance's Int voices and Fseq mean its own bank's internal ones when the bank holds them, and your library's numbered ones otherwise. The factory banks are the unit's.

## Import Audio

**The unit.** It plays the 90 preset Fseqs and the six its internal memory can hold, which arrive as bulk dumps; it cannot make one from a sound itself.

**FSVR.** The Fseq page's Import Audio analyses a WAV, AIFF, MP3, Ogg Vorbis or MP4 file into an Fseq of your own, after fseq-flash's method, with the frame bytes on the engine's own scales. The result is an ordinary Fseq: it exports as a bulk dump the unit takes.

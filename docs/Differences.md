# Where FSVR differs from the FS1R on purpose

Everything else in this repository is an attempt to match rgwan's unit, and a disagreement with it is a
bug (`AGENTS.md`, "Where a claim comes from"). This file is the short list of places where FSVR knowingly
does something the hardware does not. Each entry says what the unit does, what FSVR does, where the firmware
says so, and how to get the unit's behaviour back.

## Toggle Filter Effects on Playing Notes
On a real FS1R if the filter or effects is turned off while notes are being played, they remain enabled on those held notes. In FSVR, this toggle is immediately applied to held notes.

## 32 filtered voices

**The unit.** The board has one VOP3-1 for the voice filters, and it is wired to the tone generator's first
sixteen channels only. So while any part has its filter on, the note allocator (`FUN_000112c0`) stops walking
all 32 channels and walks two halves instead, each with its own cursor: parts with their filter on get
channels 0 to 15 (`FUN_0000fa5c`), parts with it off get 16 to 31 (`FUN_0000fcfa`). The filtered parts
share 16 voices between them, and a 17th filtered note steals one even when the other half is idle; the
unfiltered parts are capped at 16 the same way.

**FSVR.** The VOP3 is emulated, so FSVR runs a second VOP3-1, filtering channels 16 to 31, alongside the
unit's on 0 to 15. Channel n is slot `n & 15` on chip `n >> 4`. Both chips run the same filter program, host
and calibration; nothing about the second needed a new constant.

- **Allocation.** With both chips there is nothing to split: the allocator walks all 32 channels with the one
  cursor and the stealing rules it uses when no filter is on.
- **Routing.** A note is filtered when its part's filter switch is on, whichever channel it lands on
  (`Chan::filtered`). A channel of a part with its filter off goes straight to its part's bus, bypassing the
  chip. Note on, note off, the cutoff modulation every tick and all-notes-off go to the slot on the note's own
  chip.
- **Ring-out.** Once its note ends, a filtered channel keeps passing through its slot, so the filter rings out
  as on the unit. When a note from a part with its filter off takes that channel, the slot is silenced first,
  the same way the unit silences a slot before a filtered note takes it.
- **Cost.** The second chip only runs while a part has its filter on, and only in this mode.

It is the default. Options > Special > 32 Filtered Voices switches it off (`Device::setFilterPoly32(false)`,
`-flt16` on the console); the setting is kept in `Settings.json` for every instance. Off is the unit's 16 and
renders bit-identically to the single-chip engine: all fifteen demo songs were checked. Changing the setting
stops every note, as a filter switch does. Nothing about the second chip can be measured, since the unit has
none.

## 14-bit pitch bend

**The unit.** The Pitch Bend handler (0x15B80) keeps `(msb - 64) * 16 + (lsb >> 3)`, and the pitch takes that
>> 3 (`FUN_0002022C`), so the wheel moves in 256 steps: at a range of 12 semitones each is 9.4 cents, which a
slow bend makes audible as a staircase.

**FSVR.** The LSB's low six bits, which the unit drops, land between its steps as a fraction of one, so the
wheel has 16,384 positions and the unit's pitch word comes out exactly wherever those bits are 0. The bend as a
controller source is the unit's own -128..127, unchanged. `Device::setBend14(false)` gives the unit's 256
steps.

## Oscillator initial phase

**The unit.** Voiced operator byte 0x1C is reserved (Data List, Voice Voiced Parameter) and every factory voice
holds 0 there. A key-synced oscillator starts each note at phase 0.

**FSVR.** Byte 0x1C, 0 to 127, is a key-synced operator's initial phase, value / 128 of a turn, so 64 is 180
degrees and 127 is 357.2 (0 and 360 are the same phase). It is added to the operator's phase input on every
form, so the sine and all1/all2 rotate and the grain forms shift their carrier inside each grain. `refresh_regs`
reads it each tick, so an edit moves a sounding note too. With key sync off it does nothing. A voice that leaves
it at 0 sounds as on the unit; one that sets it plays at 0 on the unit.

## User libraries without the unit's memory limits

**The unit.** 128 internal performances, and 128 internal voices with no Fseqs or 64 voices with six, in battery-backed RAM.

**FSVR.** A folder of .syx files, each a bank, as many as you import or save to (Import > Import FS1R SysEx to New Bank names a new bank after the file). A performance's Int voices and Fseq mean its own bank's internal ones when the bank holds them, and your library's numbered ones otherwise. The factory banks are the unit's.

## Import Audio

**The unit.** It plays the 90 preset Fseqs and the six its internal memory can hold, which arrive as bulk dumps; it cannot make one from a sound itself.

**FSVR.** The Fseq page's Import Audio analyses a WAV, AIFF, MP3, Ogg Vorbis or MP4 file into an Fseq of your own, after fseq-flash's method, with the frame bytes on the engine's own scales. The result is an ordinary Fseq: it exports as a bulk dump the unit takes.

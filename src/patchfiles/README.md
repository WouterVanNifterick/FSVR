# src/patchfiles: patch files read into banks

Patch files of any kind FSVR takes, read into a `Bank` of performances, voices and Fseqs, with a `Report` of what was skipped and why. **Ours**: reading a file is not synthesis and the unit has no counterpart. It sits outside the plug-in so the console and the tools can read the same files.

One entry point, `readPatches(bytes, name, report, pending)`. A file is sniffed by its content, never its extension: a container turns it into a sysex stream, then each message goes to the format that takes it, in file order. An item is the sysex the FS1R takes for it, so a DX voice is its ACED (when it has a supplement) and its VCED, and the engine's own conversion (`convert_dx7`) runs each time it loads.

| File | What |
|---|---|
| `patchfiles.h` | `Item`, `Bank`, `Report`, `Pending`; `readPatches`; `vmemRenamed` and `vmemBlanked`, a VMEM voice edited in its slot |
| `reading.h` | what the dispatcher, containers and formats share: `Message`, `Reading`, each container's and format's entry |
| `read.cpp` | `readPatches`: the containers in order (SMF, headerless, sysex), then the formats on each message; why a message nobody took was skipped |
| `dx7.h`, `dx7.cpp` | DX7 voice data: VCED and ACED messages, the VMEM record packed and unpacked, clamping to the DX7's ranges, INIT VOICE, an AMEM slot as an ACED |
| `container_sysex.cpp` | a sysex stream: a `.syx`, and an `.fs1`, which is the same messages |
| `container_smf.cpp` | a Standard MIDI File's sysex events, in order |
| `container_headerless.cpp` | a 4096-byte VMEM with no frame (or one within 256 bytes of header or tail, or several), and bare 155- or 156-byte VCEDs |
| `format_fs1r.cpp` | the FS1R's bulks: performances, voices, Fseqs; voices and an Fseq right after a performance are its own |
| `format_vced.cpp` | a DX7 single voice, with the ACED before it; right after a performance, the voice of its next Int part |
| `format_aced.cpp` | a DX7II, DX7s or TX802 voice's supplement, held for the next VCED |
| `format_vmem.cpp` | a DX7 or DX5 32-voice bank, unpacked to 32 VCEDs, each with its AMEM slot's ACED |
| `format_amem.cpp` | a DX7II, DX7s or TX802 bank's supplements, held for the next VMEM, and the DX7II bank select |
| `format_dx7ii.cpp` | a DX7II performance, PCED or the 32 of PMEM: single, dual or split onto parts 1 and 2 |
| `performance.cpp` | a DX performance as an FS1R performance bulk built on the engine's init performance, its voices found by number at the end of the read |

## Rules
- **Detect by content.** A container says whether a file is its own from the bytes.
- **Never refuse for damage.** A bad checksum or a DX5's `10 00` byte count is a warning; DX voice bytes are clamped to the DX7's ranges after unpacking.
- **Report what is skipped**, with the reason, so the status line can name it.
- **Pairing follows the stream:** ACED then VCED, AMEM then VMEM. Files read together share a `Pending`, so a supplement in one file pairs with the bank in the next.
- **A DX performance names its voices by number**: a VMEM's 32 take the numbers of the bank select before it, else follow the last bank read. They are looked up once the file is read, in it first and then in the files read before it; a part whose voice is in neither is left off, and a performance with none is skipped.
- **Mapping a DX performance onto FS1R parts is ours**, not the hardware's: the firmware has no such path. Each choice is marked where it is made, and none goes in `cal.h`.
- **An AMEM slot becomes an ACED only when it would change the conversion** (`acedMatters`): a DX7 voice's default slot does not, so a DX7II bank and a DX7 bank of the same voices load the same.
- **A file that was not sysex is rewrapped** (`Bank::rewrapped`) into the messages the instrument would have sent; the library keeps those, and the items' byte ranges count in them.
- The engine's `load_sysex` keeps taking only what the unit takes (VCED, ACED, FS1R bulks). VMEM, AMEM and DX performances stop here.
- One source file per container and per format. A new one gets a line in this table and its test in `src/selftest/patchfiles.cpp`.

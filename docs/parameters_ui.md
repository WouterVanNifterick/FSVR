# FS1R parameters in the editor

Every parameter in the FS1R's Data List, and where the editor puts it, or that it has none. Checked on 2026-10-16 against `plugin/skin` as it stood at `15ce2be`, with the Fseq header added since. Every one of the 3,178 sysex-mapped params in `plugin/skin/data/fs1r_sysex.json` was matched against every widget binding in `plugin/skin/views`, with `{part}`, `{op}` and `{layer}` expanded. A param counts as editable when a control edits it: a pot, a number, a menu, a button, a list row, or a handle on an envelope, key range, velocity or key scaling curve. A param that is only read in a `showIf` or a readout does not count. The Data List's own tables (`docs/FS1R_DataList_text.txt`, Tables 1 to 4) were then read against `tools/gen_parameters.py`'s output for anything the generator never turned into a param.

[editor.md](editor.md) says why the pages are laid out as they are; this is the index.

## Not editable in the editor

| What | Params | Why | Deliberate? |
|---|---|---|---|
| Knob Mode, Play 1 to 4 note and velocity, Memory, Dump Interval, LCD Contrast | `sys.knob_mode`, `sys.play1_note` to `sys.play4_velocity`, `sys.memory`, `sys.dump_interval`, `sys.lcd` | Settings with no meaning in software ([editor.md](editor.md), Unit-only settings without controls). Their params and sysex stay, so a system dump round-trips | Yes |
| Reverb and Variation Param 10 | `fx.rev.p10`, `fx.var.p10` | No reverb or variation type in the Data List uses the slot | Yes |
| Rcv Channel Max for parts 3 and 4 | `part.rcv_max.p3`, `part.rcv_max.p4` | The manual gives a receive range to parts 1 and 2 only, though the byte exists for all four | Yes |
| Voice Category | `voice.category.p1` to `.p4` | No control edits the part's voice. Edit Attributes in the Browser changes a *saved* preset's category in its bank (`item.category`), and a voice saved with Save Current Preset keeps whatever the edit buffer held | Not written down until now |
| Fseq header (Table 3, `70 00 ll`): frame format | none | The Fseq's size, not a setting: Import Audio picks the smallest that holds the sound. Every other header setting has a param (Fseq header, below) | Yes |
| Fseq frames: pitch, and eight voiced and eight unvoiced formant frequencies and levels per frame | not params | Data, not settings. Import Audio makes them out of a sound file, and the Fseq page draws them | Yes |
| Names: performance (12 characters), voice (10, one per part), Fseq (8) | not params | Save Current Preset names the performance it saves, and Rename on a row in a user bank renames a performance, voice or Fseq there. The edit buffer's Fseq name is text data `fseq.name`, sent into the Fseq's header; the performance's and the voices' cannot be edited ([editor.md](editor.md), Open) | Open |
| Fseq init command (`00 00 46`) | none | A command, not a setting; the engine ignores it as a parameter change | Yes |

Everything else in the Data List has a control.

## Part parameters: side by side, or the selected part only

The Performance Part table (`3p 00 00` to `2F`) is 48 bytes holding 49 parameters. The Parts page shows four columns of 28 of them, one column per part. The other 21 are on pages that edit the part picked by the top bar's 1 to 4, one part at a time. None is missing apart from Rcv Max for parts 3 and 4.

| Parameter | Parts page (all four) | Selected part only |
|---|---|---|
| Note Reserve | Rsv | |
| Voice bank and number | Voice (opens the Browser on it); the part's lamp turns the bank off | Browser › Voices |
| Rcv Channel Max | to (parts 1 and 2 only) | |
| Rcv Channel | MIDI Ch | top bar LCD |
| Mono/Poly, Mono Priority | Poly, Mono, and the four priority keys | |
| Filter Switch | Filter | Filter page's Part Filter; Quick Control |
| Note Shift | Note Shift triangle over the keys | |
| Detune | Detune | Pitch page; Quick Control |
| V/N Balance | V/N Bal | Quick Control |
| Volume | Volume fader | Quick Control |
| Velocity Sense Depth, Offset | Velocity curve, Depth, Offset | Quick Control |
| Pan, Dry, Variation Send, Reverb Send | Pan, Dry, Variation, Reverb | |
| Note Limit Low, High | the keys, and Keys ... to | |
| Insertion Switch | Insertion | |
| Portamento Switch | Porta | Pitch page's On |
| Pan Scaling, Pan LFO Depth, Expression Low Limit | Pan Scale, Pan LFO, Expr Lo | |
| Velocity Limit Low, High | Velocity curve, Range | |
| Sustain Rcv Switch | Sustain | |
| **LFO1 Rate, Pitch Mod Depth, Delay** | | **Quick Control only** (LFO: Speed, Pitch, Delay) |
| **LFO2 Rate, Depth** | | **Quick Control only** (Filter: LFO2 Speed; LFO: LFO2 Depth) |
| **EG Attack, Decay, Release** | | Quick Control (Amplitude Envelope); Attack and Release also on the top bar's Tone pots |
| **Formant, FM** | | Quick Control (Tone); top bar Tone pots |
| **Filter Cutoff, Resonance, EG Depth** | | Quick Control (Filter); Filter page's Part row |
| **Pitch EG Initial Level, Attack, Release Level, Release Time** | | Quick Control (Pitch Envelope); Pitch page's Part row |
| **Portamento Mode, Time** | | Pitch page only |
| **Pitch Bend Range High, Low** | | Pitch page only |

## Reading the tables

- **Where** is Tab › sub-tab › group › control, as the editor captions them. The tabs are Browser, Parts, Operators (with the sub-tabs Operator, All Operators, Envelopes, Modulation, Key Scaling, Filter and Pitch), Performance, Effects, Fseq, Quick Control and Options. **Op column** is the operator panel at the window's right. **Alg header** is the algorithm matrix's header, which six of the Operators sub-pages carry (all but Modulation).
- **Param** ids are the skin's. `.pN` stands for the four parts `.p1` to `.p4`, `op.K.` for operators 1 to 8, and `ctrl.#.` for controller sets 1 to 8.
- Voice and operator parameters always belong to the part the top bar picks, and operator ones to the operator the Op column picks, unless a page shows all eight at once (All Operators, Envelopes, Key Scaling, the Fseq page's operator row).
- The views behind the tabs: Parts `page_parts`, Operators › Operator `page_operator`, All Operators `page_all_ops`, Envelopes `page_all_envs`, Modulation `page_mod_matrix`, Key Scaling `page_key_scaling`, Filter `page_filter`, Pitch `page_pitch`, Performance `page_master`, Effects `page_fx`, Fseq `page_fseq`, Quick Control `page_quick`, Browser `library_browser`, top bar `topbar`, Op column `opcol`, Alg header `fm_matrix`.

## System (`00 00 ll`)

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 | master tuning | `sys.tune` | Performance › Master › Tune | |
| 06 | master note shift | `sys.note_shift` | Performance › Master › Note Shift | |
| 07 | dump interval | `sys.dump_interval` | none | unit-only |
| 08 | program change mode | `sys.pgm_mode` | Performance › MIDI › Program Mode (Performance, Multi) | |
| 09 | performance channel | `sys.perf_channel` | Performance › MIDI › Performance Channel | |
| 0B | knob control mode | `sys.knob_mode` | none | unit-only |
| 0D | BC curve | `sys.bc_curve` | Performance › Master › BC Curve | |
| 0E | velocity curve | `sys.vel_curve` | Performance › Master › Vel Curve | |
| 10 | Rx Excl. | `sys.rx_sysex` | Performance › MIDI Receive › SysEx | |
| 11 | note event receive sw | `sys.rx_notes` | Performance › MIDI › Receive Notes | |
| 12 | bank select receive sw | `sys.rx_bank` | Performance › MIDI Receive › Bank Select | |
| 13 | program change receive sw | `sys.rx_program` | Performance › MIDI Receive › Program Change | |
| 14 | knob receive sw | `sys.rx_knobs` | Performance › MIDI Receive › Receive KN1-KN4 via MIDI | |
| 15 | knob transmit sw | `sys.tx_knobs` | Performance › MIDI › Transmit KN1-KN4 via MIDI | |
| 16 to 21 | KN1 to KN4, MC1 to MC4, FC, BC, Formant, FM control numbers | `sys.cc_kn1` to `sys.cc_kn4`, `sys.cc_mc1` to `sys.cc_mc4`, `sys.cc_fc`, `sys.cc_bc`, `sys.cc_formant`, `sys.cc_fm` | Performance › Controller Numbers › KN1 ... FM | |
| 22 to 29 | play sound 1 to 4 note, velocity | `sys.play1_note` to `sys.play4_velocity` | none | unit-only |
| 46 | fseq init command | none | none | a command |
| 47 | memory allocation | `sys.memory` | none | unit-only |
| 48 | LCD contrast | `sys.lcd` | none | unit-only |
| 49 | system exclusive device number | `sys.device` | Performance › MIDI › Device ID | |
| 4A | bulk dump protect | `sys.dump_protect` | Performance › MIDI Receive › Bulk Dump Protect | |

## Performance Common (`10 00 ll`)

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 to 0B | NAME | none | Save › Save Current Preset's name; Rename on a user performance | see Names |
| 0E | CATEGORY | `perf.category` | Performance › Performance › Category | also Edit Attributes on a saved one |
| 10 | performance volume | `perf.volume` | Performance › Performance › Volume; top bar Volume | |
| 11 | performance pan | `perf.pan` | Performance › Performance › Pan | |
| 12 | performance note shift | `perf.note_shift` | Performance › Performance › Note Shift | |
| 14 | individual out | `perf.indiv_out` | Performance › Performance › Individual Out | |
| 15 | FSEQ PART | `fseq.part` | Fseq › Formant Sequence › Part | |
| 16 | FSEQ bank | `fseq.bank` | Fseq › Fseq Bank list; Browser › Fseqs | set with the number by a row |
| 17 | FSEQ number | `fseq.number` | Fseq › Fseq Bank list; Browser › Fseqs | |
| 18 | FSEQ Speed Ratio / MIDI Clock | `fseq.speed` | Fseq › Speed and Level › Speed % | |
| 1A | FSEQ start step offset | `fseq.start` | Fseq › Loop › Start | |
| 1C | FSEQ start step of loop point | `fseq.loop_start` | Fseq › Loop › Loop Start; the display's loop markers | |
| 1E | FSEQ end step of loop point | `fseq.loop_end` | Fseq › Loop › Loop End; the display's loop markers | |
| 20 | FSEQ loop mode | `fseq.loop_mode` | Fseq › Loop › Loop Mode (One Way, Round) | |
| 21 | FSEQ play mode | `fseq.play_mode` | Fseq › Formant Sequence › Play Mode (Scratch, Fseq) | |
| 22 | FSEQ velocity sensitivity for tempo | `fseq.speed_vel` | Fseq › Speed and Level › Speed Vel | |
| 23 | FSEQ formant pitch mode | `fseq.pitch_mode` | Fseq › Formant Sequence › Pitch (Fseq, Fixed) | |
| 24 | FSEQ key on trigger | `fseq.trigger` | Fseq › Formant Sequence › Trigger (First, All) | |
| 26 | FSEQ formant sequence delay | `fseq.delay` | Fseq › Speed and Level › Delay | |
| 27 | FSEQ level velocity sensitivity | `fseq.level_vel` | Fseq › Speed and Level › Level Vel | |

### Controller sets (`10 00 28` to `4F`)

Eight sets, VC1 to VC8, one row each on Operators › Modulation › Controller Sets. They are the performance's, not the voice's, so they stay put whichever part is picked.

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 28 to 2F | controller 1 to 8 part switch | `ctrl.#.part1` to `ctrl.#.part4` | Modulation › Controller Sets › P1 to P4 | |
| 30 to 3F | controller 1 to 8 source switch | `ctrl.#.src.kn1` ... `ctrl.#.src.pb` (14 sources: KN1 to KN4, MC1 to MC4, FC, BC, MW, CAT, PAT, PB) | Modulation › Controller Sets › KN1 ... PB | |
| 40 to 47 | controller 1 to 8 destination | `ctrl.#.dest` | Modulation › Controller Sets › Destination | Ins Param 1 to 14 stay numbered (editor.md, Open) |
| 48 to 4F | controller 1 to 8 depth | `ctrl.#.depth` | Modulation › Controller Sets › Depth | |

## Effects (`10 00 50` to `10 01 3E`)

All on the Effects tab, in four rows: Insertion, Variation, Reverb and EQ. The 16 slot params of each effect mean something different in each type, so each type shows only the slots it uses, under the Data List's name for that type (Hall1's Reverb Param 1 reads Reverb Time), in two rows of eight to the right of the row's pots.

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 50 to 00 67 | Reverb parameter 1 to 16 | `fx.rev.p1` to `fx.rev.p9`, `fx.rev.p11` to `fx.rev.p16` | Effects › Reverb › the type's slots | 1 to 8 are two-byte words |
| 00 61 | Reverb parameter 10 | `fx.rev.p10` | none | no type uses it |
| 00 68 to 01 07 | Variation parameter 1 to 16 | `fx.var.p1` to `fx.var.p9`, `fx.var.p11` to `fx.var.p16` | Effects › Variation › the type's slots | all words |
| 00 7A | Variation parameter 10 | `fx.var.p10` | none | no type uses it |
| 01 08 to 01 27 | Insertion parameter 1 to 16 | `fx.ins.p1` to `fx.ins.p16` | Effects › Insertion › the type's slots | all words |
| 01 28 | Reverb type | `fx.rev.type` | Effects › Reverb › type menu | |
| 01 29 | Reverb pan | `fx.rev.pan` | Effects › Reverb › Pan | |
| 01 2A | Reverb return | `fx.rev.return` | Effects › Reverb › Return | |
| 01 2B | Variation type | `fx.var.type` | Effects › Variation › type menu | |
| 01 2C | Variation pan | `fx.var.pan` | Effects › Variation › Pan | |
| 01 2D | Variation return | `fx.var.return` | Effects › Variation › Return | |
| 01 2E | Send Variation to Reverb | `fx.var.to_rev` | Effects › Variation › To Rev | |
| 01 2F | Insertion type | `fx.ins.type` | Effects › Insertion › type menu | |
| 01 30 | Insertion pan | `fx.ins.pan` | Effects › Insertion › Pan | |
| 01 31 | Send insertion to Reverb | `fx.ins.to_rev` | Effects › Insertion › To Rev | |
| 01 32 | Send insertion to Variation | `fx.ins.to_var` | Effects › Insertion › To Var | |
| 01 33 | Insertion level | `fx.ins.level` | Effects › Insertion › Level | |
| 01 34 | EQ low gain | `fx.eq.low_gain` | Effects › EQ › Low › Gain | |
| 01 35 | EQ low frequency | `fx.eq.low_freq` | Effects › EQ › Low › Freq | |
| 01 36 | EQ low Q | `fx.eq.low_q` | Effects › EQ › Low › Q | |
| 01 37 | EQ low shape | `fx.eq.low_shape` | Effects › EQ › Low › Shape | |
| 01 38 | EQ mid gain | `fx.eq.mid_gain` | Effects › EQ › Mid › Gain | |
| 01 39 | EQ mid frequency | `fx.eq.mid_freq` | Effects › EQ › Mid › Freq | |
| 01 3A | EQ mid Q | `fx.eq.mid_q` | Effects › EQ › Mid › Q | |
| 01 3B | EQ high gain | `fx.eq.high_gain` | Effects › EQ › High › Gain | |
| 01 3C | EQ high frequency | `fx.eq.high_freq` | Effects › EQ › High › Freq | |
| 01 3D | EQ high Q | `fx.eq.high_q` | Effects › EQ › High › Q | |
| 01 3E | EQ high shape | `fx.eq.high_shape` | Effects › EQ › High › Shape | |

Options › Special › Turn off all Effects sets the three types to no effect and the three EQ gains to 0 dB in one go.

## Performance Part (`3p 00 ll`, p = part 1 to 4)

"All four" means the Parts page has the control in each part's column; the rest edit the selected part.

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 | NOTE RESERVE | `part.note_reserve.pN` | Parts › Rsv | all four |
| 01 | BANK NUMBER | `part.bank.pN` | Browser › Voices (a row sets bank and number); Parts › the part's lamp turns it off and back | all four via Parts › Voice |
| 02 | PROGRAM NUMBER | `part.program.pN` | Browser › Voices; Parts › Voice opens it on the part | all four via Parts › Voice |
| 03 | Rcv CHANNEL MAX | `part.rcv_max.pN` | Parts › MIDI Ch ... to | parts 1 and 2 only |
| 04 | Rcv CHANNEL | `part.rcv_ch.pN` | Parts › MIDI Ch; top bar LCD | all four |
| 05 | MONO/POLY MODE | `part.poly.pN` | Parts › Poly, Mono | all four |
| 06 | MONO PRIORITY | `part.priority.pN` | Parts › Last, Top, Bottom, First (shown with Mono) | all four |
| 07 | FilterSw | `part.filter_sw.pN` | Parts › Filter; Operators › Filter › Filter › Part Filter; Quick Control › Filter › Filter | all four |
| 08 | NOTE SHIFT | `part.note_shift.pN` | Parts › Note Shift triangle over the keys | all four |
| 09 | DETUNE | `part.detune.pN` | Parts › Detune; Operators › Pitch › Pitch › Detune; Quick Control › Tone › Detune | all four |
| 0A | VOICED/UNVOICED BALANCE | `part.vn_balance.pN` | Parts › V/N Bal; Quick Control › Tone › V/N Bal | all four |
| 0B | VOLUME | `part.volume.pN` | Parts › Volume; Quick Control › Volume | all four |
| 0C | VELOCITY SENSE DEPTH | `part.vel_depth.pN` | Parts › Velocity curve's high end, Depth; Quick Control › Vel Depth | all four |
| 0D | VELOCITY SENSE OFFSET | `part.vel_offset.pN` | Parts › Velocity curve's low end, Offset; Quick Control › Vel Offset | all four |
| 0E | PAN | `part.pan.pN` | Parts › Pan | all four |
| 0F | NOTE LIMIT LOW | `part.note_low.pN` | Parts › the keys, Keys | all four |
| 10 | NOTE LIMIT HIGH | `part.note_high.pN` | Parts › the keys, Keys ... to | all four |
| 11 | DRY LEVEL | `part.dry.pN` | Parts › Dry | all four |
| 12 | VARIATION SEND | `part.var_send.pN` | Parts › Variation | all four |
| 13 | REVERB SEND | `part.rev_send.pN` | Parts › Reverb | all four |
| 14 | INSERTION SW | `part.ins_sw.pN` | Parts › Insertion | all four |
| 15 | LFO1 RATE | `part.lfo1_rate.pN` | Quick Control › LFO › Speed | selected part only |
| 16 | LFO1 PITCH MOD DEPTH | `part.lfo1_pmod.pN` | Quick Control › LFO › Pitch | selected part only |
| 17 | LFO1 DELAY | `part.lfo1_delay.pN` | Quick Control › LFO › Delay | selected part only |
| 18 | FILTER CUTOFF FREQ | `part.cutoff.pN` | Quick Control › Filter › Cutoff; Operators › Filter › Part › Cutoff | selected part only |
| 19 | FILTER RESONANCE | `part.resonance.pN` | Quick Control › Filter › Resonance; Operators › Filter › Part › Reso | selected part only |
| 1A | EG ATTACK TIME | `part.attack.pN` | Quick Control › Amplitude Envelope › Attack; top bar Tone pots | selected part only |
| 1B | EG DECAY TIME | `part.decay.pN` | Quick Control › Amplitude Envelope › Decay | selected part only |
| 1C | EG RELEASE TIME | `part.release.pN` | Quick Control › Amplitude Envelope › Release; top bar Tone pots | selected part only |
| 1D | FORMANT | `part.formant.pN` | Quick Control › Tone › Formant; top bar Tone pots | selected part only |
| 1E | FM | `part.fm.pN` | Quick Control › Tone › FM; top bar Tone pots | selected part only |
| 1F | FILTER EG DEPTH | `part.filter_eg.pN` | Quick Control › Filter › EG Depth; Operators › Filter › Part › EG Depth | selected part only |
| 20 | PITCH EG INITIAL LEVEL | `part.peg_init.pN` | Quick Control › Pitch Envelope › Init; Operators › Pitch › Part › Init | selected part only |
| 21 | PITCH EG ATTACK TIME | `part.peg_attack.pN` | Quick Control › Pitch Envelope › Attack; Operators › Pitch › Part › Attack | selected part only |
| 22 | PITCH EG RELEASE LEVEL | `part.peg_release_level.pN` | Quick Control › Pitch Envelope › Rel Lvl; Operators › Pitch › Part › Rel Lvl | selected part only |
| 23 | PITCH EG RELEASE TIME | `part.peg_release.pN` | Quick Control › Pitch Envelope › Release; Operators › Pitch › Part › Release | selected part only |
| 24 bit 0 | PORTAMENTO SWITCH | `part.porta.pN` | Parts › Porta; Operators › Pitch › Portamento › On | all four |
| 24 bit 1 | PORTAMENTO MODE | `part.porta_mode.pN` | Operators › Pitch › Portamento › Fingered, Fulltime | selected part only |
| 25 | PORTAMENTO TIME | `part.porta_time.pN` | Operators › Pitch › Portamento › Time | selected part only |
| 26 | PITCH BEND RANGE HIGH | `part.bend_up.pN` | Operators › Pitch › Pitch › Bend Up | selected part only |
| 27 | PITCH BEND RANGE LOW | `part.bend_down.pN` | Operators › Pitch › Pitch › Bend Down | selected part only |
| 28 | PAN SCALING | `part.pan_scaling.pN` | Parts › Pan Scale | all four |
| 29 | PAN LFO DEPTH | `part.pan_lfo.pN` | Parts › Pan LFO | all four |
| 2A | VELOCITY LIMIT LOW | `part.vel_low.pN` | Parts › Velocity curve's low end, Range | all four |
| 2B | VELOCITY LIMIT HIGH | `part.vel_high.pN` | Parts › Velocity curve's high end, Range | all four |
| 2C | EXPRESSION LOW LIMIT | `part.expr_low.pN` | Parts › Expr Lo | all four |
| 2D | SUSTAIN Rcv SW | `part.sustain.pN` | Parts › Sustain | all four |
| 2E | LFO2 RATE | `part.lfo2_rate.pN` | Quick Control › Filter › LFO2 Speed | selected part only |
| 2F | LFO2 MOD DEPTH | `part.lfo2_depth.pN` | Quick Control › LFO › LFO2 Depth | selected part only |

## Voice Common (`4p 00 ll`)

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 to 09 | NAME | none | Rename on a voice in a user bank; the Parts page and LCD show it | see Names |
| 0E | CATEGORY | `voice.category.pN` | none | Edit Attributes changes a saved voice's only |
| 10 | LFO1 waveform | `voice.lfo1.wave.pN` | Operators › Modulation › LFO 1 › wave menu | |
| 11 | LFO1 speed | `voice.lfo1.speed.pN` | Operators › Modulation › LFO 1 › Speed | |
| 12 | LFO1 delay | `voice.lfo1.delay.pN` | Operators › Modulation › LFO 1 › Delay | |
| 13 | LFO1 key sync | `voice.lfo1.key_sync.pN` | Operators › Modulation › LFO 1 › Key Sync | |
| 15 | LFO1 pitch modulation depth | `voice.lfo1.pitch.pN` | Operators › Modulation › LFO 1 › Pitch | |
| 16 | LFO1 amplitude modulation depth | `voice.lfo1.amp.pN` | Operators › Modulation › LFO 1 › Amp | |
| 17 | LFO1 frequency modulation depth | `voice.lfo1.freq.pN` | Operators › Modulation › LFO 1 › Freq | |
| 18 | LFO2 waveform | `voice.lfo2.wave.pN` | Operators › Modulation › LFO 2 › wave menu | |
| 19 | LFO2 speed | `voice.lfo2.speed.pN` | Operators › Modulation › LFO 2 › Speed | |
| 1C | LFO2 phase | `voice.lfo2.phase.pN` | Operators › Modulation › LFO 2 › Phase | |
| 1D | LFO2 key sync | `voice.lfo2.key_sync.pN` | Operators › Modulation › LFO 2 › Key Sync | |
| 1E | Note shift | `voice.note_shift.pN` | Operators › Pitch › Pitch › Note Shift | |
| 1F, 20, 21, 3E, 22 | Pitch EG level 0 to 4 | `voice.peg.l0.pN` to `voice.peg.l4.pN` | Operators › Pitch › Env plot; L0 to L4 under it | |
| 23 to 26 | Pitch EG time 1 to 4 | `voice.peg.t1.pN` to `voice.peg.t4.pN` | Operators › Pitch › Env plot; T1 to T4 under it | |
| 27 | Pitch EG velocity sensitivity | `voice.peg.vel.pN` | Operators › Pitch › Pitch EG › Velocity | |
| 28 to 29 | Fseq voiced op switches | `op.K.v.fseq_sw.pN` | Operators › Operator › Oscillator › Fseq; All Operators › Fseq; Fseq › Operators of the selected part › V | one bit per operator |
| 2A to 2B | Fseq unvoiced op switches | `op.K.u.fseq_sw.pN` | the same, with N | one bit per operator |
| 2C | Algorithm preset number | `voice.alg.pN` | Alg header › Alg number, ▼ ▲, Select Algorithm | |
| 2D to 34 | Voiced op 1 to 8 carrier level correction | `op.K.v.carrier_corr.pN` | Operators › Operator › Freq EG row › Carrier | V only |
| 3B | Pitch EG range | `voice.peg.range.pN` | Operators › Pitch › Pitch EG › Range | |
| 3C | Pitch EG time scaling depth | `voice.peg.time_scale.pN` | Operators › Pitch › Pitch EG › Time Scale | |
| 3D | Voiced feedback level | `voice.feedback.pN` | Alg header › Feedback; the feedback box on the matrix | |
| 40 to 44 | Formant Control Destination 1 to 5 (dest, V/N, op) | `voice.formant_ctrl.1.dest.pN`, `voice.formant_ctrl.1.layer.pN`, `voice.formant_ctrl.1.op.pN`, and the same for 2 to 5 | Operators › Modulation › Formant and FM Control › Formant › Dest, V/N, Op | three params per byte |
| 45 to 49 | Formant Control Depth 1 to 5 | `voice.formant_ctrl.1.depth.pN` ... `.5.depth.pN` | the same › Depth | |
| 4A to 4E | FM Control Destination 1 to 5 (dest, V/N, op) | `voice.fm_ctrl.1.dest.pN`, `voice.fm_ctrl.1.layer.pN`, `voice.fm_ctrl.1.op.pN`, and the same for 2 to 5 | Operators › Modulation › Formant and FM Control › FM › Dest, V/N, Op | three params per byte |
| 4F to 53 | FM Control Depth 1 to 5 | `voice.fm_ctrl.1.depth.pN` ... `.5.depth.pN` | the same › Depth | |
| 54 | Filter Type | `voice.filter.type.pN` | Operators › Filter › Filter › type menu | |
| 55 | Filter Resonance | `voice.filter.resonance.pN` | Operators › Filter › Filter › Resonance | |
| 56 | Filter Resonance Vel Sens | `voice.filter.reso_vel.pN` | Operators › Filter › Key and Velocity › Reso Vel | |
| 57 | Filter Cutoff Frequency | `voice.filter.cutoff.pN` | Operators › Filter › Filter › Cutoff | |
| 58 | Filter EG Depth Vel Sens | `voice.filter.eg_vel.pN` | Operators › Filter › Key and Velocity › EG Vel | |
| 59 | Filter Cutoff LFO1 Depth | `voice.filter.lfo1.pN` | Operators › Filter › LFO › LFO1; Operators › Modulation › LFO 1 › Filter | |
| 5A | Filter Cutoff LFO2 Depth | `voice.filter.lfo2.pN` | Operators › Filter › LFO › LFO2; Operators › Modulation › LFO 2 › Filter | |
| 5B | Filter Cutoff Key Scale Depth | `voice.filter.key_depth.pN` | Operators › Filter › Key and Velocity › Key Depth | |
| 5C | Filter Cutoff Key Scale Point | `voice.filter.key_point.pN` | Operators › Filter › Key and Velocity › Key Point | |
| 5D | Filter Input Gain | `voice.filter.gain.pN` | Operators › Filter › Filter › Gain | |
| 64 | Filter EG depth | `voice.filter.eg_depth.pN` | Operators › Filter › Filter EG › Depth | |
| 66, 67, 68, 65 | Filter EG level 1 to 4 | `voice.filter.eg_l1.pN` to `voice.filter.eg_l4.pN` | Operators › Filter › Env plot; L1 to L4 under it | |
| 69 to 6C | Filter EG time 1 to 4 | `voice.filter.eg_t1.pN` to `voice.filter.eg_t4.pN` | Operators › Filter › Env plot; T1 to T4 under it | |
| 6E bits 0-2 | Filter EG time scale | `voice.filter.eg_time_scale.pN` | Operators › Filter › Filter EG › Time Scale | |
| 6E bits 3-5 | Filter EG attack time vel | `voice.filter.eg_attack_vel.pN` | Operators › Filter › Filter EG › Atk Vel | |

## Voiced operator (`6p 0m 00` to `22`, m = operator 1 to 8)

Every row is `op.K.v.<name>.pN`. The Op column, the Operator page, and the All Operators and Envelopes pages show the voiced operators while V is lit.

| Address | Data List | Param | Where |
|---|---|---|---|
| 00 bits 0-5 | oscillator transpose | `op.K.v.transpose.pN` | Operator › Oscillator › Transpose; Op column › Trans (Formant form only) |
| 00 bit 6 | oscillator key sync | `op.K.v.key_sync.pN` | Operator › Oscillator › Key Sync; All Operators › Key Sync |
| 01 | frequency coarse | `op.K.v.coarse.pN` | Op column › Coarse and the frequency box; Operator › Coarse; All Operators › Coarse |
| 02 | frequency fine | `op.K.v.fine.pN` | Op column › Fine; Operator › Fine; All Operators › Fine |
| 03 | frequency note scaling | `op.K.v.note_scaling.pN` | Operator › Sens › Note Scale |
| 04 bits 0-2 | spectral form | `op.K.v.form.pN` | Op column › form bar; Operator › Oscillator › form menu; All Operators › Waveform |
| 04 bits 3-6 | bw bias sense | `op.K.v.bw_bias.pN` | Operator › Sens › BW Bias |
| 05 bits 0-2 | fseq track number | `op.K.v.fseq_track.pN` | Operator › Freq EG row › Fseq Trk; Fseq › Operators of the selected part › Trk |
| 05 bits 3-5 | spectral skirt | `op.K.v.skirt.pN` | Op column › Skirt; Operator › Oscillator › Skirt; All Operators › Skirt |
| 05 bit 6 | oscillator mode | `op.K.v.mode.pN` | Op column › Ratio, Fixed; Operator › Ratio, Fixed; All Operators › Mode |
| 06 | freq. ratio of band spectrum | `op.K.v.band_ratio.pN` | Op column › Width or Reson (named by the form); Operator › Oscillator › Band |
| 07 | detune | `op.K.v.detune.pN` | Op column › Detune; Operator › Oscillator › Detune; All Operators › Detune |
| 08 to 0B | frequency EG initial, attack value, attack time, decay time | `op.K.v.feg_init.pN`, `.feg_attack_level`, `.feg_attack`, `.feg_decay` | Operator › Freq EG row › Init, Atk Lvl, Attack, Decay |
| 0C to 0F | EG level 1 to 4 | `op.K.v.eg_l1.pN` to `op.K.v.eg_l4.pN` | Op column › Amplitude EG; Operator › Env plot, L1 to L4; Envelopes › the operator's plot |
| 10 to 13 | EG time 1 to 4 | `op.K.v.eg_t1.pN` to `op.K.v.eg_t4.pN` | Op column › Amplitude EG; Operator › Env plot, T1 to T4; Envelopes › the operator's plot |
| 14 | EG hold time | `op.K.v.eg_hold.pN` | the same, Hold |
| 15 | EG time scaling | `op.K.v.eg_time_scale.pN` | Operator › Sens › Time Scale |
| 16 | total level | `op.K.v.level.pN` | Op column › Level; Operator › Amplitude › Level; All Operators › Level; Envelopes › Level; the operator's box on the algorithm matrix |
| 17 | level scaling break point | `op.K.v.ls_break.pN` | Key Scaling › Level Scaling › Break, and the curve |
| 18 | level scaling left depth | `op.K.v.ls_left_depth.pN` | Key Scaling › L Depth, and the curve |
| 19 | level scaling right depth | `op.K.v.ls_right_depth.pN` | Key Scaling › R Depth, and the curve |
| 1A | level scaling left curve | `op.K.v.ls_left_curve.pN` | Key Scaling › L Curve |
| 1B | level scaling right curve | `op.K.v.ls_right_curve.pN` | Key Scaling › R Curve |
| 1F bits 0-2 | pitch mod sense | `op.K.v.pitch_mod.pN` | Operator › Sens › Pitch |
| 1F bits 3-6 | freq bias sense | `op.K.v.freq_bias.pN` | Operator › Sens › Freq Bias |
| 20 bits 0-3 | freq velocity sense | `op.K.v.freq_vel.pN` | Operator › Sens › Freq Vel |
| 20 bits 4-6 | freq mod sense | `op.K.v.freq_mod.pN` | Operator › Sens › Freq |
| 21 bits 0-3 | amp velocity sense | `op.K.v.amp_vel.pN` | Operator › Amplitude › Amp Vel |
| 21 bits 4-6 | amp mod sense | `op.K.v.amp_mod.pN` | Operator › Sens › Amp |
| 22 | EG bias sense | `op.K.v.eg_bias.pN` | Operator › Amplitude › EG Bias |

The carrier level correction (Voice Common `2D` to `34`) and the Fseq switches (`28` to `2B`) are per operator too, and are listed under Voice Common where the Data List puts them.

## Unvoiced operator (`6p 0m 23` to `3D`)

Every row is `op.K.u.<name>.pN`. N on the Op column, the Operator page and the All Operators and Envelopes pages shows them.

| Address | Data List | Param | Where |
|---|---|---|---|
| 23 | formant pitch transpose | `op.K.u.transpose.pN` | Operator › Oscillator › Transpose |
| 24 bits 0-4 | formant pitch coarse | `op.K.u.coarse.pN` | Op column › Coarse; Operator › Coarse; All Operators › Coarse |
| 24 bits 5-6 | formant pitch mode | `op.K.u.mode.pN` | Op column › pitch mode menu (Normal, Link FO, Link FF); Operator › Oscillator › mode menu; All Operators › Mode |
| 25 | formant pitch fine | `op.K.u.fine.pN` | Op column › Fine; Operator › Fine; All Operators › Fine |
| 26 | formant pitch note scaling | `op.K.u.note_scaling.pN` | Operator › Sens › Note Scale |
| 27 | formant shape band width | `op.K.u.bandwidth.pN` | Op column › Width; Operator › Oscillator › Bandwidth; All Operators › Bandwidth |
| 28 | formant shape bw bias sense | `op.K.u.bw_bias.pN` | Operator › Sens › BW Bias |
| 29 bits 0-2 | formant skirt | `op.K.u.skirt.pN` | Op column › Skirt; Operator › Oscillator › Skirt; All Operators › Skirt |
| 29 bits 3-5 | formant resonance | `op.K.u.resonance.pN` | Op column › Reson; Operator › Oscillator › Resonance; All Operators › Resonance |
| 2A to 2D | frequency EG initial, attack value, attack time, decay time | `op.K.u.feg_init.pN`, `.feg_attack_level`, `.feg_attack`, `.feg_decay` | Operator › Freq EG row › Init, Atk Lvl, Attack, Decay |
| 2E | level | `op.K.u.level.pN` | Op column › Level; Operator › Amplitude › Level; All Operators › Level; Envelopes › Level |
| 2F | level key scaling | `op.K.u.level_key_scale.pN` | Operator › Freq EG row › Lvl Key; Key Scaling › Noise |
| 30 to 33 | EG level 1 to 4 | `op.K.u.eg_l1.pN` to `op.K.u.eg_l4.pN` | Op column › Amplitude EG; Operator › Env plot, L1 to L4; Envelopes › the operator's plot |
| 34 to 37 | EG time 1 to 4 | `op.K.u.eg_t1.pN` to `op.K.u.eg_t4.pN` | the same, T1 to T4 |
| 38 | EG hold time | `op.K.u.eg_hold.pN` | the same, Hold |
| 39 | EG time scaling | `op.K.u.eg_time_scale.pN` | Operator › Sens › Time Scale |
| 3A | freq bias sense | `op.K.u.freq_bias.pN` | Operator › Sens › Freq Bias |
| 3B bits 0-3 | freq velocity sense | `op.K.u.freq_vel.pN` | Operator › Sens › Freq Vel |
| 3B bits 4-6 | freq mod sense | `op.K.u.freq_mod.pN` | Operator › Sens › Freq |
| 3C bits 0-3 | amp velocity sense | `op.K.u.amp_vel.pN` | Operator › Amplitude › Amp Vel |
| 3C bits 4-6 | amp mod sense | `op.K.u.amp_mod.pN` | Operator › Sens › Amp |
| 3D | EG bias sense | `op.K.u.eg_bias.pN` | Operator › Amplitude › EG Bias |

## Fseq header (`70 00 ll`)

The loaded Fseq's own settings, sent as the Data List's Fseq parameter changes. The player reads the note, the tuning and the speed adjustment (FUN_000127FC, FUN_0001A59E) and the end of valid data; the loop, loop mode, tempo velocity, pitch mode and delay are the defaults choosing the Fseq copies into the performance, which also sets the performance's start offset to 0 (FUN_00032A38, [findings.md](findings.md) 2026-10-20). The params read whatever the loaded Fseq holds; the player reads the performance's copies of the loop mode, tempo velocity, pitch mode and delay (Performance Common, above). All of them are on the Fseq page's In the Fseq tab.

| Address | Data List | Param | Where | Notes |
|---|---|---|---|---|
| 00 to 07 | NAME | text data `fseq.name` | Fseq › In the Fseq › Name; Rename on a user Fseq; Import Audio names it after the file | a name set in `fseq.name` is sent as eight parameter changes; see Names |
| 10 to 11 | start step of loop point | `fseq.hdr.loop_start` | Fseq › In the Fseq › Loop | copied into the performance's Loop Start when the Fseq is chosen |
| 12 to 13 | end step of loop point | `fseq.hdr.loop_end` | Fseq › In the Fseq › Loop ... to | copied into the performance's Loop End |
| 14 | loop mode | `fseq.hdr.loop_mode` | Fseq › In the Fseq › Loop Mode | copied into the performance's Loop Mode, which the player reads |
| 15 | speed adjust | `fseq.hdr.speed` | Fseq › In the Fseq › Speed Adj | the performance's Speed % scales it |
| 16 | velocity sensitivity for tempo | `fseq.hdr.tempo_vel` | Fseq › In the Fseq › Tempo Vel | copied into the performance's Speed Vel, which the player reads |
| 17 | formant pitch mode | `fseq.hdr.pitch_mode` | Fseq › In the Fseq › Pitch Mode | copied into the performance's Pitch, which the player reads |
| 18 | formant note assign | `fseq.hdr.note` | Fseq › In the Fseq › Note | |
| 19 | formant pitch tuning | `fseq.hdr.tuning` | Fseq › In the Fseq › Tune | |
| 1A | formant sequence delay | `fseq.hdr.delay` | Fseq › In the Fseq › Delay | copied into the performance's Delay, which the player reads |
| 1B | frame data format (128 to 512 frames) | none | | the Fseq's size, not a setting; Import Audio picks the smallest that holds the sound |
| 1E to 1F | end step of valid data | `fseq.hdr.end` | Fseq › In the Fseq › End | the Browser's Fseq list shows the length |

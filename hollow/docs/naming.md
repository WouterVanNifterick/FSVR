# Names and the FM8 mapping

> This copy travels with FSVR's `hollow/`, which is Hollow's `framework/` folder. The web editor (`editor/`) and the skin tools (`tools/`) it mentions live in the Hollow project beside this repository.

Hollow uses its own names for everything. No FM8 or NGL identifier (class names such as `FormArp` or `SwitchControl`, resource ids, parameter names) appears in the framework, the skin or the editor. The link back to FM8 lives only in `tools/fm8import/mapping/`, which the importer reads, and in the generated table `docs/mapping.md`.

## Vocabulary

| FM8 / NGL term | Hollow term |
|---|---|
| Form (`FRM` resource) | view |
| page form shown by the navigator | page (a view shown in the `pages` embed) |
| Control | widget |
| `Label` | `plate` |
| `Switch` / `switch.dll` | `button` |
| `ButtonMenu` (+ `PopupMenu`) | `dropdown` (+ `items`) |
| `Selector` / `selector.dll` | `dial` |
| `ValueEdit`, `FM8ValueEdit` | `number` |
| `TextEdit`, `DelayedTextControl` | `textbox` |
| `SubForm`, `StackedSubForm` and FM8's code-driven page switching | `embed` (with `goto` actions) |
| `Scope` | `plot` |
| `LevelMonitor` | `meter` |
| `ShadeArea` | `veil` |
| `List2` and its subclasses | `list` |
| `Tree View` / `BrowserTree` | `tree` |
| `Generic` subclasses `XYHandle`, `XYHandleMorph`, `EffectList`, `FM8Envelope` | `custom` kinds `pad`, `morph_pad`, `fx_chain`, `envelope` |
| `PICTURE` resource | image |
| `PM` picture map (frames, orientation, stretch bands) | `skin.json` `images` (`tiles`, `axis`, `slice`) |
| picture frame | tile |
| `FNT` picture font and its glyph strip | font |
| `PanelItem` | `fill` |
| `TextItem` / `TextPanelItem` | `text` / `text` + `fill` + `pad` |
| `ParameterLink` binding a control id to a parameter tag | the widget's `param` |
| VST parameter index / name | param `id` / `name` in `params.json` |
| Switch `mode` 0/1/2/4 | `press` `down`/`momentary`/`repeat`/`up` |
| Switch `hoverFrames`, `pressedFrames`, `disabledFrame` | `hoverTiles`, `pressedTiles`, `disabledTile` |
| Selector `dragMode` 0/1/2 | `drag` `linear`/`absolute`/`rotary` |

## Naming rules

- Everything is lowercase `snake_case`. Parameter ids are dotted paths of such segments.
- **Views**: `page_*` for navigator pages, `dialog_*` for modal boxes, `fx_*` for effect strips, otherwise a short noun for the region (`topbar`, `sidebar`, `keys`).
- **Images**: `<group>/<name>`, grouped by what the image *is*, never by resource id and never one folder per view. Groups: `backgrounds` (whole-view backdrops), `panels` (frames, boxes, 9-slice plates), `knobs` (rotary strips), `faders` (linear slider strips and their tracks), `buttons` (push/toggle strips), `switches` (multi-state selectors, radio groups), `leds`, `meters`, `displays` (scope, envelope and graph backdrops), `icons`, `keyboard`, `branding` (logos, wordmarks), `scrollbars`, `library` (browser-only art), `dialogs` (setup, licence and about boxes), `misc` (last resort, keep it small). Names say what it looks like or does, with size or variant suffixes when a group has several: `knobs/rotary_large`, `knobs/rotary_small`, `buttons/nav_page`, `leds/on_off_green`.
- **Fonts**: `<colour-or-role>_<size>`, e.g. `caption_grey`, `value_white_small`, `title_bold`.
- **Widgets**: unique in their view. Interactive widgets are named after what they control (`tempo`, `ratio`, `fine_tune`, `nav_arp`); their caption is `<name>_label`, a readout next to a dial is `<name>_value`, a decorative frame is `<section>_frame`, a section heading `<section>_title`. Pure decoration that nobody will look for can stay `deco_<n>`.
- **Params**: `<section>.<name>` or `<section>.<instance>.<name>`: `quick.brightness`, `op.a.ratio`, `fx.reverb.time`, `arp.tempo`, `master.volume`. Display names are short Title Case in our own words.
- **Fixed param segments.** Families that repeat per instance use the instance as its own segment, with identical leaf names across instances, so one view can serve all instances through `{var}` templates. These segment values are fixed:
  - operator: `op.<a|b|c|d|e|f|x|z|pitch>.<leaf>` (the pitch envelope pseudo-operator is `pitch`)
  - morph corner (timbre), always the last segment: `tl`, `tr`, `bl`, `br`, e.g. `op.a.ratio.tl`
  - FM matrix amount: `matrix.<src>.<dst>.<corner>` with `src` in `a..f, x, z, in` (`in` = the external input) and `dst` in the same set plus `out` (audio output) and `pan`
  - modulation: `mod.<source>.<target>`, target an operator segment above, source one of `bend_up`, `bend_down`, `pressure`, `wheel`, `breath`, `cc1`, `cc2`, `input_env`, `lfo1`, `lfo1_pressure`, `lfo1_wheel`, `lfo1_breath`, `lfo1_cc1`, `lfo1_cc2`, `lfo2`, `lfo2_pressure`, `lfo2_wheel`, `lfo2_breath`, `lfo2_cc1`, `lfo2_cc2`
  - effects: `fx.<overdrive|tube|cabinet|shelf_eq|peak_eq|talkbox|phaser|flanger|tremolo|reverb|psych_delay|chorus_delay>.<leaf>`
  - LFOs: `lfo.<1|2>.<leaf>`

## FSVR's params

FSVR (the FS1R editor, `tools/fsvr`) names its params the same way, the part last as `p1`..`p4` where a parameter belongs to a part (its `{part}` var):
- `sys.<leaf>`, `perf.<leaf>`, `fseq.<leaf>`: the system, the performance and its Fseq
- `ctrl.<1..8>.<leaf>`: a controller set, its sources one switch each (`ctrl.3.src.kn1`)
- `fx.<rev|var|ins|eq>.<leaf>`: the effects, their type-dependent words `p1`..`p16`
- `part.<leaf>.<part>`, `voice.<leaf>.<part>`: a part's own settings and its voice's common ones
- `op.<1..8>.<v|u>.<leaf>.<part>`: a voiced (`v`) or unvoiced (`u`) operator; leaves the two share have the same name, so one page serves both through `{layer}`
- `perf.program` picks a factory performance; `browse.*` are the browser's own (not host) params
- `perf.bank` with `perf.user`, `part.user.<part>` and `fseq.user` number the user's own performances, voices and Fseqs (1 to 16,384) where the unit had a fixed internal bank; `knob.1`..`knob.4` are the knobs as KN1 to KN4; `part.morph_<x|y|jitter_x|jitter_y|seed|edit>.<part>` are a part's morph (docs/fsvr-skin.md)

## Mapping files (`tools/fm8import/mapping/`)

The importer (`tools/fm8import/import.py`) builds `plugins/hollow_fm/skin` from the FM8 resources plus these tables. Every key is the FM8 numeric id as a string; every value is an object whose first field is the Hollow name and whose `about` says what it is. Missing entries get automatic names, so the tables can be filled in gradually.

| File | Key | Value |
|---|---|---|
| `views.json` | FRM id | `{ "view": "page_arp", "fm8": "FormArp", "about": "..." }` plus optional view fields (`"flow": "column"`, `"gap"`, `"fill"`...) merged into the view; `null` removes a field the FRM gave it |
| `images.json` | PICTURE id | `{ "image": "knobs/rotary_large", "about": "..." }` |
| `fonts.json` | FNT id | `{ "font": "caption_grey", "about": "..." }`, plus optional `"ttf": [face, px, "#rrggbbaa"]` for a font FM8 turns into TrueType in code (the importer rasterizes TTFD `face` instead of the glyph strip; an id with no FNT resource adds a font) and `"selection"` (text selection colour, else the FNT's first colour) |
| `params.json` | VST2/VST3 parameter index | `{ "param": "arp.tempo", "name": "Arp Tempo", "fm8": "Arpeggiator Tempo", "min": .., "max": .., "steps": .., "labels": [..], "format": ".." }` (range fields optional; the importer fills them from the widgets bound to the param; `default` is the plain value FM8 starts with, read from its own display text, and `defaultNormal` the same as 0..1 where the text is not a number) |
| `widgets/<view>.json` | control id within that FRM | `{ "name": "tempo", "fm8param": 149, "about": "..." }` plus optional overrides: `"param"` (a Hollow param id or template, when the binding is not a single FM8 index, e.g. `"op.{op}.ratio"`), `"type"`, `"kind"`, `"action"` (a skin `action` object), `"items"`, `"hidden"` |

`params_internal.json` (a JSON array in skin `params.json` form) lists GUI-side params FM8 keeps outside its host list (arpeggiator steps, wheels, polyphony, editor flags); the importer appends them after the 1,094 host params and marks each `"host": false`. Entries may carry `fm8` and `about` like the other tables (dropped from the skin, `fm8` shown in `docs/mapping.md`). Widgets bind them with `"param"`.

`skin.json` (optional) holds skin-wide fields copied into the skin's `skin.json`, such as the `menu` and `tooltip` styles. The importer also writes `fonts/<font>.json` (`selection` and `caret` colours for text entry, from the FNT colour pair) and the popup menu check and arrow icons FM8 draws in code (`icons/menu_*`).

A view given `"flow": "column"` has its widgets sorted by `rect` y, so the mapping sets the column order through the rects (the effect rack puts its strips in chain order this way).

Views FM8 lays out in code (the stacked envelope and key scaling rows) are `views.json` entries under a key starting with `+`, e.g. `"+env_block": { "view": "env_block", "size": [452, 133], "about": "..." }`, plus any view fields; their widgets are the `+name` entries of `widgets/<view>.json`. The skin's `data/` tables (waveforms, factory presets and template names, the sound attribute lists, the cabinet setups) are FM8 content, so no mapping file holds them: `tools/fm8import/fm8data.py` extracts them at import time from the user's own FM8 files (`--fm8-macros`, `--fm8-mac`, `--fm8-decomp`; `--no-data` skips them). A widget entry names such a list instead of copying it: `"rowsFrom": "sound_attributes.types"` (list rows) or `"itemsFrom": "arp_templates.names"` with `"itemsColumns": 16` (menu items), `"textFrom"` / `"tileFrom": "cabinets.models.3"` (one entry as the text or the fill tile).

`fm8param` is the FM8 VST parameter index (0..1093; VST2 index = VST3 id) the control edits. The importer turns it into `param` through `params.json`.

Reused views: when one view serves several instances (the operator page for ops A..F, a modulation column for each source, a control bound to the current morph corner), give `fm8param` for one instance and say which segments vary: `"varies": { "op": "a", "timbre": "tl" }` means "this index is the op-A, top-left instance; turn the segment `a` into `{op}` and `tl` into `{timbre}`". The importer then writes `"param": "op.{op}.ratio.{timbre}"`. The embed or `goto` that shows the view supplies `vars` with the same names and segment values (`{ "op": "b" }`), and `skin.json` supplies skin-wide ones (`timbre`). Extra override fields for widget entries: `"vars"` (on an embed), `"varies"`.

Any other field in a widget entry is a skin widget field and is merged into the imported widget: objects merge key by key (so `"text": { "text": "{op:upper}" }` keeps the imported font and alignment), everything else replaces. This is how entries add `showIf`/`enableIf`, `scroll`, `midi`/`spring`, `status` and text data `key`s. Mapping-only fields (`about`, `fm8param`, `varies`) are dropped from the skin.

The importer sets `"editable": true` on number boxes whose FRM `ValueEdit` is editable and on text boxes whose `TextEdit` is not read-only, and takes `tip` from the FRM help text. An editable text box needs a `key`: give one in the entry (dotted lowercase, `{var}` allowed, e.g. `"patch.name"`, shared where two views show the same text), otherwise it gets `<view>.<widget>`. The importer checks every param, var and key a condition or key names (under every set of vars the view can be shown with), and every image a `scroll` or skin style names.

Widgets FM8 creates in code (no FRM record) are added in the same file under a key starting with `+`: `"+fm_matrix": { "type": "embed", "rect": [547, 23, 272, 315], "view": "fm_matrix", "layer": 3 }`. The value is a complete skin widget (skin-format.md) except `name`, which is the key without the `+`; it may use `fm8param`/`varies` like other entries.

## Generated table

`python tools/fm8import/import.py` also writes `docs/mapping.md`: every view, image, font, param and named widget with its FM8 origin, so one can go from either side to the other.

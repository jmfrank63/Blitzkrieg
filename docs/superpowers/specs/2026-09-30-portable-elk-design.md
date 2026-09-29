# Portable ELK (Blitzkrieg Localisation Kit)

A new ELK, written in Zig with a Dear ImGui interface, built by `zig build`
for macOS and Windows. It replaces the MFC ELK in `Sources/src/ELK`. It is the
"ELK" row of the editor set in
`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` and the
"Editors on every platform" item of `docs/PLANNED_FEATURES.md` section 1,
planned as Phase 7 of `.planning/ROADMAP.md`.

## What ELK is, measured from the source

ELK translates the game's texts. It keeps a **text database** on disk, lets a
translator edit one translation at a time, tracks a **state** per text, and
packs the result into a `.pak` the game reads. Measured on the MFC source
(12,547 lines in 37 files) and on the shipped data in `Data/ELK` and
`Data/AmericanELK`:

- **Project file** `elk.xml` (`CELK`): a list of *elements* (projects), each
  with a description (`Name`, `PAK` output name, `UPD` file prefix, `Fonts`
  flag), a path, a version string and `LastUpdateNumber`; plus the last two
  statistics snapshots. `Data/ELK/elk.xml` has one element, "Game Resources".
- **Text database**: per element, a folder `<element>_data_base/` (the code
  says `_DATA_BASE\`; the shipped folder is lower case) holding one file
  triple per text, keyed by a relative path without extension:
  `<key>.elk` (original), `<key>.txt` (translation), `<key>.xml` (state:
  `<base State="0..3" Changed="0|1"/>`), and optional `<key>.dsc`
  (description) or `<folder>/_folder.dsc`, plus `resource.description` (the
  element's description). `Data/ELK/game_data_base` has 2,852 `.elk`, 1,758
  `.txt`, 6,623 `.xml`. `.elk`, `.txt` and `.dsc` are **UTF-16LE with a
  `FF FE` byte-order mark**; trailing CR, LF and NUL are stripped on read and
  write; the files use CRLF and the editing buffer uses LF.
- **States**: `0` Not translated (red), `1` Outdated (yellow, set only by an
  update, never by hand), `2` Translated (green), `3` Approved (blue), plus the
  separate `Changed` flag, which becomes true once a state other than
  "Not translated" was ever set.
- **Updates in**: `.pak` (partial update) and `.upd` (complete update, prunes
  texts no longer in the update) are zip files of `.txt` originals,
  `.dsc` and `.description`. Files are named `<UPD prefix>_<n>.upd`; the
  highest `n` per prefix wins; a changed original marks a translated text
  "Outdated". "Import from game" builds such a `.pak` / `.upd` from the game's
  own data. `Data/ELK/game_*.upd` (22 files, 0.65 to 1.3 MB each) are real
  examples.
- **Out to the game**: "Export to PAK" writes a zip of `<key>.txt` files
  (the translation if its state is not "Not translated" and it is non-empty,
  otherwise the original), optionally filtered, and, if the element's `Fonts`
  flag is set, generated fonts `fonts/{tiny,small,medium,large}/1.tfd` and
  `1_c.dds`, `1_l.dds`, `1_h.dds`. `zip.exe -9 -R -D` did the zipping.
  "Run Game" exports to `<game>/Data/Texts.pak` and starts `game.exe -windowed`.
- **Spreadsheet exchange**: Excel through ODBC/Jet, table `BlitzkriegELK`
  with columns `Path`, `Original`, `Translation`, `State`, `Description`
  (`<element>\<key>` with backslashes); apostrophes are replaced by
  backticks in both directions (an SQL-quoting workaround). Import only
  touches rows with a non-empty translation and marks the state Outdated when
  the sheet's original differs from the database's. `Data/ELK/desc.xls` is a
  real BIFF8 file with sheet `BlitzkriegELK`.
- **UI**: two window layouts, chosen at launch (`-short` in the code;
  the help text says the opposite key `-developer` selects the extended
  mode): a *short* translator mode (preset `elk.xml` beside the program,
  automatic `.upd` update on start, one toolbar, five fixed filters) and the
  *extended* mode (any ELK, project management, custom filters, toolbar
  customisation, recent ELKs). Panels: text tree with state colours,
  original, description, image (`icon.tga` from the game data), translation,
  state radio buttons, next/previous/first/last through the active filter,
  find dialog over original, description and translation, statistics tree,
  dialogs for the imports and exports, filter editor, font chooser, spell
  check (F7), progress dialog, help (`elk.chm`), about.
- **Spell check**: `CSAPI`, Microsoft's proofing engine (`ot711as.dll`,
  `sfl11as.dll`) with a user dictionary `CUSTOM.DIC` (a `#LID 1049` header
  and words). The help text says Word is required. Windows only.
- **Fonts**: `CFontGen` draws a TrueType font with GDI, packs the glyphs into
  a power-of-two atlas, writes it as DDS (DXT3 / ARGB4444 / ARGB8888) and the
  metrics as a `SFontFormat` structure (`.tfd`). Sizes tiny/small/medium/large
  are 8/16/24/48, medium 16 to 24 and large 16 to 48 are user-set. Windows
  only (GDI, `EnumFontFamiliesEx`).

## Why a rewrite

Same reasons as the Map Editor: MFC, 32-bit, Visual Studio only, not built by
`build.zig`, shipped as a checked-in `ELK.exe` with its own copies of
`streamio.dll`, `image.dll`, `bugslay.dll`, `zip.exe` and the Sentry/CSAPI
DLLs. Unlike the Map Editor, ELK needs **no engine**: it edits text files,
zips, spreadsheets and one font atlas. That makes it the cheapest editor to
port and the easiest to test headless.

## Decisions

Numbered as in `.planning/phases/07-elk-portable-localisation-kit/07-CONTEXT.md`.

- **D-01 Separate executable `ELK`.** Not a mode of `MapEditor` or of a shared
  editor app. Translators receive ELK alone (it has always been shipped
  separately, in `Data/ELK`), it needs none of the engine bridge, and its
  release cadence is independent. It is built on the shared editor kit
  (below), not on the map editor's engine code.
- **D-02 Layout.** `Sources/editor/elk/core` (std-only Zig, headless),
  `Sources/editor/elk/app` (SDL3 + ImGui), `Sources/editor/elk/help`
  (Markdown, embedded in the binary), and one small C++ library
  `Sources/src/FontKit` (C ABI) for the game's `.tfd` and DDS writers.
- **D-03 Shared editor kit.** ELK consumes the kit Phase 6 extracts from the
  map editor: ImGui host and frame loop (`host.zig`), the key=value settings
  file and recent-files list (`settings.zig`), file dialogs and the
  thread-safe `PathSlot`, the crash-safe write helper, the `BK_*_AUTO`
  scripted-UI harness (`auto.zig`), the icon decoder (`pictures.zig`'s TGA
  path) and the CRT/assert routing (`crt.zig`). If Phase 6 has not extracted
  the kit when Phase 7 starts, plan 07-06 extracts the minimum it needs into
  `Sources/editor/kit` itself and Phase 6 rebases on it; the extraction is a
  move, never a copy.
- **D-04 Modes.** Extended mode is the default. `--short` (and the legacy
  `-short`) selects the translator layout; `--developer` is accepted as an
  alias for extended. Short mode auto-opens `elk.xml` beside the executable,
  applies the newest `.upd` per prefix on start, keeps the five fixed filters
  and the single toolbar, and resets its layout on every launch, as the
  original did.
- **D-05 Database format unchanged.** The on-disk database (`elk.xml`, the
  `*_data_base` folder, the `.elk`/`.txt`/`.xml`/`.dsc` files) is read and
  written exactly as the MFC ELK does, so `Data/ELK` opens unchanged and the
  old and new ELK can share a database during the transition. Folder names
  resolve case-insensitively. Text keys are lower case on import. The
  unused `_DATA_BASE_RESERVE` folder is not ported. `Path` values are written
  as the bare element name (`game`), which the MFC ELK also accepts (it keeps
  only the part after the last backslash), so no host path leaks into the
  file and it is portable.
- **D-06 Text codec.** Files are UTF-16LE with `FF FE`; reads also tolerate a
  missing BOM and a big-endian BOM; writes always use `FF FE`, CRLF, and strip
  trailing CR/LF/NUL exactly like `CELK::FromWideText`. An untouched text
  written back is byte-identical to what the MFC ELK wrote. The UI works in
  UTF-8 with LF.
- **D-07 No code page.** The MFC ELK converted to an ANSI code page for its
  edit control and for the font's character table. The portable ELK is
  Unicode throughout and the setting is dropped (an old value is read and
  ignored). Fonts carry the Unicode code points in use, which is what the
  game looks up (`FontGpu::TextWidth` indexes glyphs by the UTF-16 unit).
- **D-08 Output the game reads unchanged.** Export writes a zip `.pak`:
  entry names `<key>.txt` with forward slashes, no directory entries, method
  8 (deflate, level 9) or 0, CRC-32 and DOS timestamps, fonts under
  `fonts/<size>/1.tfd|1_c.dds|1_l.dds|1_h.dds`. It must parse with
  `Sources/src/StreamIOZig/zip.zig` (CRC checked) and with `unzip -t`. Zig's
  `std.compress.flate.Compress` replaces `zip.exe`; the zip reader is
  `zip.zig` imported as a module (it is std-only).
- **D-09 Where the output goes.** The game mounts overlays (mods) first,
  then loose files, then archives (`bk_storage_open` order in
  `StreamIOZig/streamio.zig`). A `Data/Texts.pak` is therefore **shadowed**
  by a loose `Data/Textes/...` folder, which the repository's `Data` has.
  So: export defaults to a mod, `mods/<Name>/data/texts.pak`, which always
  wins; "Install to game" and "Run Game" still write `<game>/Data/Texts.pak`
  as the MFC ELK did, but warn when a loose text with the same key exists and
  offer the mod route instead. Run Game launches `Game -windowed
  -mod=<Name>` (or `-mod=None`) in a dedicated `ELKTest` profile, never the
  user's profile, saves or cloud sync (same rule as Map Editor D-02).
- **D-10 Spreadsheet formats.** `.xlsx` (read and write, the new default),
  `.csv` (UTF-8 with BOM, RFC 4180, read and write), and legacy `.xls` BIFF8
  (**read only**, so translators' existing files import). The Excel ODBC route
  cannot be ported; writing BIFF8 buys nothing since Excel opens `.xlsx`.
  Columns, sheet name `BlitzkriegELK` and the backslash path form are kept,
  and import accepts both separators. Import semantics are ported exactly
  (rows with empty translation are skipped, trim of `\t\r\n `, Outdated on an
  original mismatch, Translated otherwise, write only when the bytes
  differ). The backtick-for-apostrophe substitution is applied only when
  reading legacy `.xls`; new exports do not substitute.
- **D-11 Spell check: Hunspell.** Hunspell (tri-licensed MPL/GPL/LGPL) is
  vendored and built by `zig build`; it reads `.aff`/`.dic` dictionaries from
  a dictionary folder the user picks (default `<ELK dir>/dictionaries`), the
  language is chosen in a combo of the dictionaries found, and a user
  dictionary `custom.dic` (UTF-8, one word per line, `#` comments, so the
  legacy `CUSTOM.DIC` header line is ignored) is kept beside the settings.
  Same word and delimiter rules as `CSpellChecker` (`SPELLING_WORD_DELIMITERS`,
  `WORD_DELIMITERS`, `IGNORE_SYMBOLS`). Only dictionaries whose licence
  allows redistribution are committed (with their licence files);
  others are found at the user-chosen path. F7 keeps its meaning. Rejected:
  the platform spell checkers (two code paths, not testable headless,
  no user-chosen dictionaries), Word automation (Windows only, needs Office),
  and dropping spell check.
- **D-12 Fonts: stb_truetype + FontKit.** Rasterise with `imstb_truetype.h`
  (already vendored under `vendor/dcimgui`), lay out the atlas with the same
  packing rule as `CFontGen::MeasureFont` (texture 64..4096 wide, 2:1 or 1:1,
  2 px leading), and write the outputs through `FontKit`, a C++ library that
  calls the engine's own `SFontFormat::operator&` structure saver and
  `SaveImageToDDSImageResource`, so the `.tfd` and DDS bytes are produced by
  the code the game reads back. A pure-Zig writer was rejected as a format
  drift risk. Glyph shapes will not be pixel-identical to GDI; the acceptance
  is measured, not assumed (see Exit criteria E-05). System fonts are found
  by scanning the OS font folders and reading the `name` and `cmap` tables;
  the list offered is the fonts that cover the characters in use, like
  `CFontGen::GetFonts`. Phase 8's FontGen port reuses this library.
- **D-13 UI font.** The ELK window itself loads a system Unicode font stack
  (ImGui 1.92 loads glyphs on demand), with a setting to override it, so
  Cyrillic, Chinese and other translations are readable while editing.
- **D-14 Long operations.** Open, statistics, import, export, find and font
  generation run on a worker thread with a progress modal (message, position,
  range, Cancel). Cancel is safe: outputs are written to a temporary name and
  swapped in; a cancelled import leaves every text file whole.
- **D-15 Saving.** As before: the translation is written when leaving the
  text, on Save, Close and Exit, and now also every 30 seconds while it is
  dirty. Every file write is temp + rename. Ctrl+Z is the edit box's own undo,
  as before.
- **D-16 Settings.** `elk.cfg` in the kit's user-data area replaces the
  registry (`LoadFromRegistry`/`SaveToRegistry`): last ELK, PAK, XLS and
  game folder, the ten recent ELKs, named filters and the current one, find
  options, font name and the two sizes, collapse-item, toolbar visibility,
  spell language and dictionary folder. The legacy registry is not read.
- **D-17 Delete the MFC ELK at parity.** When `07-PARITY.md` shows every row
  done and verified: delete `Sources/src/ELK`, the shipped binaries in
  `Sources/elk` (`ELK.exe`, `bugslay.dll`, `image.dll`, `streamio.dll`,
  `zip.exe`, `elk.chm`, `excel_rows.reg`, `CUSTOM.*`) and the same binaries
  in `Data/ELK` and `Data/AmericanELK` (`ELK.exe`, the DLLs, `zip.exe`,
  `elk.chm`, `excel_rows.reg`, `CUSTOM.*`), keeping the databases and updates.
  `A7.sln` drops the project.
- **D-18 Help.** The `.doc` help (`Sources/elk/Blitzkrieg ELK help-*.doc`,
  ~9,300 words) is converted once to Markdown, corrected for the new UI, and
  shown in an in-app window (F1, Help > Contents) with a table of contents;
  the Russian text is kept as a second language. `elk.chm` is dropped.
- **D-19 Test data policy.** Tests use only tracked data (`Data/ELK`,
  `Data/AmericanELK`, `Data/Fonts`, `Data/Fonts Variants`, the loose
  `Data/Textes`) copied to `zig-out/local-test/elk`; they never read a GOG
  install, the AchtungPanzer2 mod or anything untracked. The win-home GOG
  install is used only for a stdout-only manual check.
- **D-20 Menus and toolbars.** The extended layout keeps every menu command;
  the short layout keeps its reduced menu. "Customize" becomes a window that
  shows or hides each toolbar and toolbar button and resets to defaults,
  persisted in `elk.cfg`. Free dragging of toolbars is replaced by ImGui
  docking (layout persisted by the ImGui ini). Recorded as an intended
  adaptation in `07-PARITY.md`.

## Architecture

Three layers, like the Map Editor, with the engine layer reduced to FontKit.

### 1. FontKit (C++, `Sources/src/FontKit`)

Flat C ABI, no UI, no editing policy:

- `BkFontWrite( folder, glyph_table, glyph_count, atlas_rgba, w, h, metrics,
  kerning, kerning_count, face_name )` writes `1.tfd` (via the engine's
  `SFontFormat` structure saver) and `1_c.dds`, `1_l.dds`, `1_h.dds` (via
  `SaveImageToDDSImageResource( ..., GFXPF_DXT3, GFXPF_ARGB4444,
  GFXPF_ARGB8888 )`), returning a status and a message.
- `BkFontRead( path, out )` reads a `.tfd` back through the game's own reader
  so tests compare metrics, characters and kerning with what was written.
- No exception crosses the boundary; every call is guarded like the
  EditorBridge entry points.

If Phase 999.2 changes which texture variants exist, FontKit follows whatever
`SaveImageToDDSImageResource` writes.

### 2. ELK core (Zig, `Sources/editor/elk/core`)

std-only, headless, no UI. Modules:

- `text_codec`: UTF-16LE with BOM, CRLF and trailing-run rules (D-06).
- `state`: `State`, `TextProperty`, the state-file reader/writer, the rules
  for "Changed".
- `database`: open a project file, the elements and their folders, enumerate
  keys, read and write original/translation/description/state, the folder
  description lookup (`<key>.dsc`, else the folder's `_folder.dsc`), case
  resolution.
- `project_file`: `elk.xml` reader/writer (`Elements`, `Path`, `Statistic`,
  `PreviousStatistic`), the path normalisation of D-05, recent list logic.
- `tree`: the key tree (folders and texts), the lowest-state colour rule,
  next/previous/first/last through a filter, folder state propagation, bulk
  state set on a folder.
- `filter`: `SSimpleFilter::Check` (OR of AND-lists of folder substrings, plus
  a states mask and the "translated" flag), the five built-in filters, the
  named filter store.
- `search`: find over original, description and translation, direction,
  match case, whole word, resume position.
- `stats`: word, word-symbol, symbol and text counts per state per element
  and per tree node (`GetTextCounts` rules), previous-statistic snapshot.
- `packages`: zip read (imports `zip.zig`), zip write, `.pak`/`.upd` import
  (partial and complete, Outdated rule, version string), import from game
  data (loose folders and `.pak`), auto-update from the newest `.upd` per
  prefix, export to `.pak` with only-filled and filter, install target and
  shadow check (D-09).
- `sheet`: `.csv`, `.xlsx` (zip + XML, shared strings, inline strings),
  `.xls` BIFF8 reader (OLE compound file, SST, `CONTINUE`, `LABELSST`,
  `RK`, `NUMBER`), export and import with the D-10 rules.
- `font`: the font-generation model: character set from the texts,
  atlas packing, metrics, kerning pairs, size clamps, font coverage, the
  call into FontKit; TTF parsing goes through `stb_truetype` via a tiny C
  shim.
- `spell`: word splitting, the delimiter tables, Hunspell wrapper, user
  dictionary, suggestions.
- `settings`: `elk.cfg` on the kit's settings format.

### 3. ELK app (Zig, `Sources/editor/elk/app`)

- SDL3 window and ImGui host from the kit; one window, docked panels.
- Panels: menu bar (extended or short), toolbars, tree, original,
  description, image, translation with state selector, status bar (tooltips).
- Windows and dialogs: Open ELK, Recent, Import from Game, Import from PAK,
  Import/Export spreadsheet, Export to PAK (with filter), Create Filter,
  Find, Statistics, Set Font Name and Size, Spell check, Progress, Customize,
  Run Game, Help, About, the unsaved/overwrite/delete prompts.
- Scripted UI: `BK_ELK_AUTO` (the kit's harness) drives every command for
  the app tier.

## Data flow

```
   game data / .pak / .upd                 .xlsx / .csv / .xls
            |  import                                |  import
            v                                        v
   +-------------------------- text database ---------------------------+
   | elk.xml + <element>_data_base/ (*.elk *.txt *.xml *.dsc)            |
   +---------------------------------------------------------------------+
            | export                          |  export           | edit
            v                                 v                   v
   <name>.pak (+ fonts)  --> mods/<Name>/data/   .xlsx/.csv    translation view
            |
            v
        the game (StreamIO: overlays, loose files, archives)
```

## Errors

Every core function returns an error union with a message the app shows in the
status bar or a dialog; nothing asserts on bad input (a missing BOM, a broken
zip, a truncated `.xls`, a text with a lone surrogate are all handled). Partial
failures in a batch operation are collected and reported, never abort the
rest. A database file that cannot be written leaves the previous file intact
(temp + rename). Unknown files in a database folder are left untouched.

## Testing

Tiers, following the Map Editor spec:

| Tier | Needs | Runs on | Gate |
|---|---|---|---|
| **Core** (Zig) | nothing, uses tracked data under `Data/` | all six CI targets | required |
| **FontKit** (C++) | data-only startup, no window | the five CI targets that build the engine C++ | required |
| **Game reads it** | the game and a window | macOS arm64 locally, win-home | required locally |
| **ELK app** | ELK and a window | macOS arm64 and Windows x64 in CI where a GPU device exists, locally otherwise | required where possible; "skipped: no GPU device" is never a pass |

Golden comparisons use `zig-out/local-test/elk` as scratch, never `/tmp`.
Measurements before assumptions (the project's rule): the font atlas and
metrics against `Data/Fonts` and `Data/Fonts Variants` (E-05), the export
against `Data/ELK/texts.pak` (E-02), the `.xls` reader against an independent
decoder's output for `Data/ELK/desc.xls` (E-04), and the open time of the full
database (E-01) are each measured first and recorded in the plan summary.

## Plan split (Phase 7)

| Plan | Content | Depends on |
|---|---|---|
| 07-01 | Core database: codec, state, database, `elk.xml`, tree, filters, search, statistics, settings | Phase 6 kit decision |
| 07-02 | Packages: zip write, `.pak`/`.upd` import, import from game, auto-update, export, install/shadow check | 07-01 |
| 07-03 | Spreadsheet exchange: csv, xlsx, xls reader | 07-01 |
| 07-04 | FontKit and font generation, font enumeration | 07-01 |
| 07-05 | Spell check: Hunspell vendored, dictionaries, user dictionary | 07-01 |
| 07-06 | ELK app shell: window, panels, tree, translation editing, navigation, filters UI, find, statistics window, recents, short mode, help, about, Customize, progress | 07-01, kit |
| 07-07 | ELK app workflows: import/export dialogs, filter editor, fonts dialog, spell UI, run game, delete project, `BK_ELK_AUTO` | 07-02 to 07-06 |
| 07-08 | Parity closure: CI on macOS and Windows, packaging, win-home run, delete the MFC ELK and binaries, docs | all |

Waves: 07-01; then 07-02, 07-03, 07-04, 07-05 in parallel (independent
core modules); then 07-06; then 07-07; then 07-08.

## Risks

- **Font fidelity.** stb_truetype hints and kerns differently from GDI (kern
  table only, no GPOS). Mitigated by measuring against `Data/Fonts Variants`
  and by an on-screen check in the game; text width differences of one pixel
  per glyph are accepted, layout breaks are not.
- **Shadowing.** A loose `Data/Textes` beats `Data/Texts.pak`; D-09 makes the
  mod the default and warns otherwise.
- **`.xls` breadth.** BIFF8 has many record kinds; only what ELK's sheet uses
  is read, and anything else fails with a message, never a crash. A fixture
  with an independent decoding guards it.
- **Hunspell dictionaries.** Licences and language coverage vary; the app
  works without a dictionary (spell check disabled with a reason, like the
  MFC ELK without Word).
- **Kit timing.** Phase 6 may not have extracted the kit; D-03 covers that.
- **Speed.** Enumerating thousands of small files is fast in Zig but the MFC
  ELK's "processed very slowly" FAQ shows it was not; E-01 measures the
  full-database open and sets a budget (target under 2 s on the shipped
  `Data/ELK`).

## Exit criteria for Phase 7

- **E-01** Opening `Data/ELK/elk.xml` (a copy) and saving without edits
  leaves every database file byte-identical and `elk.xml` semantically equal;
  the full open takes under 2 seconds on the dev Mac.
- **E-02** Exporting the unmodified database produces a `.pak` whose entry
  names equal `Data/ELK/texts.pak`'s text entries and whose `.txt` bytes
  are equal, or each difference is listed and explained in the plan summary.
- **E-03** Every import rule (partial, complete, Outdated, version string,
  auto-update pick-highest) passes a table test, and importing
  `game_21.upd` then `game_22.upd` into a fresh database gives the same
  file set as importing `game_22.upd` alone (complete update).
- **E-04** `desc.xls` imports to exactly the rows an independent decoder
  reports; `.xlsx` and `.csv` round-trip every text in `Data/ELK` including
  newlines, quotes, apostrophes and characters beyond the BMP-safe range.
- **E-05** Generated fonts for Arial at the shipped sizes: metrics
  (`nHeight`, `nAscent`, `nDescent`, `nAveCharWidth`, `fSpaceWidth`) within 1
  px of `Data/Fonts Variants/Arial.pak`, atlas sizes equal, `BkFontRead`
  round-trips; the game shows Cyrillic and Latin text with them (F9 capture).
- **E-06** The game started with `-mod=<Name>` on an ELK-exported mod shows an
  edited string from the export (verified by the game's own text lookup and a
  capture), on macOS arm64 and on win-home.
- **E-07** `BK_ELK_AUTO` drives every menu command in both layouts on macOS
  and Windows without an error, with shot comparison.
- **E-08** Spell check flags a known misspelling in an English text with
  suggestions, honours "Add to dictionary" across a restart, and reports a
  clear reason with no dictionary.
- **E-09** CI: core tier on all six targets, FontKit tier on the five,
  app tier on macOS arm64 and Windows x64.
- **E-10** Every row of `07-PARITY.md` is verified; the MFC ELK, its shipped
  binaries and the `A7.sln` entry are gone; `zig build` still passes on all
  targets.

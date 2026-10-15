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
  Recorded in 07-01: the project's own `Path` (which the MFC ELK replaced on
  open with where it opened the file) is written as the bare file name
  (`elk.xml`) for the same reason. The texts are the `.elk` files, as the MFC
  tree was filled from them; a state file without an original (the tracked
  database has 3,777) is left alone. A state outside 0..3 reads as "Not
  translated", the clamp the MFC import applied. The description lookup
  keeps the MFC order (`<key>.dsc`, then `_folder.dsc` from the text's
  parent folder upward, skipping its own folder) but stops at the database
  folder instead of climbing on to the drive root.
  Recorded in 07-02 (imports): an import is `UpdateELK` given one file,
  whose name picks the element (`<title>_<n>`, cut at the last underscore
  even when no number follows), `LastUpdateNumber` and the kind (`.upd`
  complete, anything else partial); a missing element is added and a
  missing `elk.xml` created, which is how the MFC ELK made a new database
  (it had no New command). The MFC state rule is kept as written: a changed
  original makes a Translated or Approved text Outdated and any other text
  Not translated, so an Outdated text whose original changes again drops to
  Not translated. A complete update deletes only the `.elk` of texts it
  lacks. `resource.description` is read over the element's description as
  the data tree did: a field it lacks keeps its value (`game_1.upd`'s copy
  has no `Fonts`; the tracked `elk.xml` kept `Fonts="1"`). Element names
  match the file title without regard to case; a file whose bytes would not
  change is not rewritten, so a repeated import writes nothing; the Version
  names the bare package file and its time is UTC at two-second steps. Import
  from game reads the loose `.txt`/`.dsc`/`.description` files of a data
  folder straight into the database (no intermediate `.pak`), its Version
  time being the newest file's. Re-importing `game_1.upd`, from which
  `Data/ELK` was built, leaves every database file byte-identical.
- **D-06 Text codec.** Files are UTF-16LE with `FF FE`; reads also tolerate a
  missing BOM and a big-endian BOM; writes always use `FF FE`, CRLF, and strip
  trailing CR/LF/NUL exactly like `CELK::FromWideText`. An untouched text
  written back is byte-identical to what the MFC ELK wrote. The UI works in
  UTF-8 with LF.
  Recorded in 07-01: a file without a BOM reads as UTF-16LE; the editor side
  is WTF-8 so a lone surrogate survives; "untouched" means the text encodes
  to what the file already reads as, and then the file is not rewritten,
  which also keeps the 1,361 tracked originals and 3 translations that end in
  a CR/LF/NUL run (never written by `FromWideText`) byte-identical.
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
  Recorded in 07-02 (export): `zip_writer.zig` sorts the entries byte-wise
  by name (so `a-b.txt` precedes `a.txt`) and gives every entry the DOS time
  1980-01-01 00:00:00, where zip.exe took the file system's order and each
  file's time; the engine only passes the stamp on as a stream's time, and
  the fixed one makes two exports of one database byte-identical. An entry
  is stored when deflating would not shrink it, as zip.exe did. No extra
  fields (zip.exe wrote NT security descriptors and Unix times), no Zip64;
  names that differ only in case are refused, since the game would reach
  only one. The pick is `ExportToPAK`'s: the translation's bytes when the
  state is not Not translated and the translation has more than its BOM
  (only-filled, which every MFC caller set), else the original's, both
  copied unchanged. Importing an export back (named as the element's
  update, since the file name picks the element) and exporting again gives
  the same bytes.
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
  Recorded in 07-02 (export): the headless core has both destinations,
  `mods/<Name>/data/texts.pak` and `Data/Texts.pak`, existing folders and
  file found whatever their case, and writes a mod's `data/mod.xml`
  (`MODName` the mod's name, `MODVersion` 1.0, the Resource Editor's form)
  when it has none, because the game mounts no mod folder without one; an
  existing `mod.xml` is left alone. The shadow warning and Run Game belong
  to 07-07.
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
  *App shell slice (2026-10-15):* the kit's `host` starts the engine
  (`BkEditorStart` owns the GPU device and the frame), which ELK does not
  load, so ELK runs on the engine-less counterpart with the same
  start / frame / stop shape. *Kit gaps closed (2026-10-15):* that window
  host is `kit/window.zig` (moved from `elk/app`), and the file dialogs'
  slot is `kit/dialogs.zig`'s `PathSlot(Kind, capacity)` with its SDL
  callback, moved from the map editor's `panels_logic` with the resource
  editor's four hand-rolled slots rebased on it. ELK's File > Open (Ctrl+O)
  uses it; the opened database goes first in elk.cfg's recent list.
  *Tree, states and autosave slice (2026-10-16):* the core's `tree` module
  is the key tree (one root per element, titled `<name>, VERSION: <version>`,
  folders and texts of a folder ordered together without regard to case,
  each folder the lowest state below it, a text's change carried up to the
  root); the app draws it clipped, coloured red / yellow / green / blue, and
  selecting a text opens its folders and scrolls to it. The edit view's
  state buttons write the selected text's state at once; Outdated shows but
  cannot be chosen. A button press is kept over a later edit of that visit
  only: `InputViewWindow` cleared `bManualState` on each text it loaded, so
  an Approved text edited on a later visit is saved Translated. The 30 s
  autosave counts from the moment the edit became dirty, on the session's
  clock (the scripted runs move it with `do=wait`); the smoke kills the frame
  loop (`do=kill`, no save point) with a later edit unsaved and reopens the
  copy with the autosaved text in it. Folder bulk-set and the tree context
  menu remain for their own slice.
  *Navigation and filters slice (2026-10-16):* the core's `filter` module is
  `SSimpleFilter::Check` (the text's folder path in the MFC tree's form,
  `medals\1` in lower case, holding every substring of one condition; the
  states mask; the `Changed` flag) and the five built-in filters; `tree`
  browses First / Previous / Next / Last through a filter in the tree's
  order, Previous and Next reaching nothing with no text selected as in
  the MFC commands. The extended layout's filter bar holds the named
  filters' combo and the four Browse buttons; the Browse menu has the MFC
  order (Last, Next Ctrl+N, Previous Ctrl+P, First), each command enabled
  when its step reaches a text, and a step lands in the translation box as
  the MFC input view took the focus. Choosing a filter leaves the
  selection where it is (`OnChangeFilter`). *Adaptation:* the MFC tree did
  not show the filter; the new one dims the texts it rejects and the
  folders that hold none it keeps (dimmed, not hidden, so the tree and its
  state colours stay whole). Alt+N / Alt+P browse too, except while the
  translation box is active: ImGui keeps Alt with a letter for text input
  there (AltGr, macOS Option). elk.cfg keeps each named filter as one line,
  `filter=<name>|<four 0/1 state flags>|<changed 0/1>|<conditions>`, the
  conditions AND-lists of substrings joined by `,`, the lists by `;`, and
  `current_filter=<name>`; a file with no `filter=` line gets the five
  built-in ones, as the MFC ELK made them when the registry held none. The
  smoke's fourth pass chooses Translated, sees the medal rows dimmed, walks
  Data/ELK's four Translated texts by Alt+N, Ctrl+N, Ctrl+P and the menu,
  stops at both ends, and a second run on the same elk.cfg starts with the
  filter and walks it again. The short layout's single toolbar remains for
  its own slice.
  *Filter editor and collapse-item slice (2026-10-16):* the filter editor
  moved from 07-07 into 07-06, which completes the browse side.
  Browse > Create Filter... (also on the filter bar) is
  `CCreateFilterDialog`: the named filters with their condition counts,
  Add, Rename, Delete, the four state boxes, "Changed only" and the folder
  conditions with Add / Remove Condition. It edits a copy of the settings;
  OK writes elk.cfg, Cancel drops the edit. Names are made unique as
  `GetUniqueFilterName` did (`name`, `name_0`, `name_1`, ...) and lose `|`
  and control characters; Add appends, Rename keeps the position and the
  current filter follows the new name; deleting the current filter falls
  back to All. The first edit writes the five built-in filters into the
  file, and the last filter cannot be deleted, since a file with none would
  bring the five back. *Adaptations:* the name is typed in the dialog's own
  Name box rather than a second dialog, and a condition is typed as folder
  substrings joined by `&` (or `,`), lower-cased with `/` turned into `\`,
  rather than ticked in a list of the database's folders: the check is the
  same substring test, and the list showed exactly those lower-case
  folder paths. *Collapse Item (B5),* read from
  `CELKTreeWindow::DeselectItem`: with the mode on, each Browse step first
  collapses the element (the tree root) holding the text it leaves, every
  folder in it, and then opens the way to the text it reaches, so only
  that path stays open. A click in the tree does not collapse anything, as
  in the MFC tree, where only the Browse steps and Find called
  `DeselectItem`. The mode is the Browse menu's first, checkable item, as
  in the MFC menu, plus a check box on the filter bar (the MFC toolbar's
  check button). It is on by default, as the MFC registry default was, and
  is kept as `collapse_item=0|1`. Forcing it on in the short layout joins
  that layout. While the dialog is open the translation box does not take
  the keyboard, because its focus would close the modal. The smoke's
  filter pass gains two runs: the expanded set with Collapse Item on and
  off; a filter created through the dialog (Outdated, `medals&krest`),
  applied and walked, renamed in place, a delete cancelled, then deleted
  while current (the bar shows All); and a restart that finds the mode and
  a kept filter in elk.cfg.
  *Find and statistics slice (2026-10-16):* the core's `search` module is
  `FindItem`: from where the last hit stopped (the text, the field, one past
  the hit), each text's original, description and translation in that
  order, then the next text in the tree's order (the previous one with Up;
  within a text the fields are still read forwards), the filter playing no
  part; Match case off lower-cases both sides (ASCII, Latin-1 and the basic
  Cyrillic capitals, which keep their byte length, so a hit's offset is one
  into the edit box's text), Whole word wants a spelling delimiter or the
  text's end on either side. *Changes from the MFC search:* it wraps round
  the tree and stops when it is back where it started (the MFC one stopped
  at the end); a hit Whole word rejects no longer ends the field's search
  (the MFC loop moved to the next field, so "cat" in "concatenate the cat"
  was never found); with nothing selected it starts at the first text (the
  MFC one did nothing); a text chosen by hand is searched from its start,
  not from the last hit's field and position. Edit > Find... (Ctrl+F) is the
  Windows Find dialog's layout, modeless as it was: Find what, Find Next
  (Enter), Match whole word only, Match case, Up / Down, Cancel, which sends
  the next search back to the start. The edit is saved before a search, the
  hit's text is selected like a Browse step (Collapse Item included), and
  elk.cfg keeps `find_text=`, `find_down=`, `find_match_case=` and
  `find_whole_word=` (the registry's FindText, FindDown, FindMatchCase,
  FindWholeWord; FindWindow and FindPosition were reset by closing the
  dialog and are not kept). *Adaptation:* the MFC ELK selected the hit's
  characters in its edit box; the status line names the field and offset
  instead, and a description hit has no description view to show it until
  C1. The core's `stats` module is `CreateStatistic` with
  `GetTextCounts` and its quirk (a word starts after a run of spelling
  delimiters but ends only at a blank, a trailing delimiter run counts an
  empty word), counting UTF-16 code units where the MBCS build counted the
  code page's bytes; each text counts under the state the tree shows, in
  both halves. View > ELK Statistic counts the database each time and shows
  `CStatisticDialog`'s tree as a table: Original Text and Translation, each
  with the four states summed over the elements and each element
  (`<name>, VERSION: <version>`) with its four states, columns Items, Words,
  Word Symbols and Symbols with the thousands set apart. *Measured:*
  `Data/ELK/elk.xml`'s snapshot is of an older database (2,968 texts in other
  states, the tracked one has 2,852), so equality is proved on
  `Data/AmericanELK`: with the six files this project edited after the
  snapshot (the credits, four option names) restored from
  `tools/zig/fixtures/elk/american_snapshot`, every count of both halves
  equals the stored `Statistic`. Not yet: storing the counts as the
  snapshot (`Close` moved `statistic` to `previousStatistic` and saved
  `elk.xml`), and D-14's worker thread with progress and Cancel for both
  (a full find that misses reads every text's three files, about a second
  on a Linux debug build). The smoke's fifth pass opens the statistic
  window and asserts Data/ELK's counts, then finds "kuban" in the desc's
  translation twice, the name's original, round to the desc again, misses
  with Match case and with Whole word "Kuba", and goes Up from Tunis to
  Tobruk; a second run finds the text and options in elk.cfg and opens
  Find by Ctrl+F.
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
| 07-06 | ELK app shell: window, panels, tree, translation editing, navigation, filters UI and filter editor, find, statistics window, recents, short mode, help, about, Customize, progress | 07-01, kit |
| 07-07 | ELK app workflows: import/export dialogs, fonts dialog, spell UI, run game, delete project, `BK_ELK_AUTO` | 07-02 to 07-06 |
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

## The Game's language option (scope change 2026-10-15)

07-CONTEXT adds one game-side feature: Options > Settings > Gameplay has a
**Language** row that selects the text source and applies at once, with no
restart. It selects among packs, it is not a new localisation system.

- The choices: `Default`, the texts the game ships (loose `Data/Textes` or
  `Data/Texts.pak`), then every folder under `mods/` (any case) that carries
  `data/texts.pak`, which is what ELK's mod export writes (D-09). The row
  shows the mod's `MODName` from its `data/mod.xml`, or the folder name;
  only `mod.xml` is read, not the mod's paks. The screen scans on every
  opening (`ScanLanguages`, `GameTT/OptionEntryWrapper.cpp`).
- The option is `GamePlay.Language` in `Data/Configs/defconf.cfg`, instant
  apply, main options screen only (flag 1, like Cloud), stored in the
  profile's `config.cfg` like every Gameplay setting. Its action
  `SetLanguage` (StreamIO's option bridge) publishes the value as the global
  `Text.Language`.
- The Game's frame loop compares `Text.Language` with what is mounted just
  before each step and calls `NMain::ApplyLanguage`: the chosen
  `mods\<folder>\data\texts.pak` becomes the `LANGUAGE` overlay of the data
  storage, last added so it wins over the base data and over a loaded MOD
  (`CICChangeMOD` puts it back on top). A storage named by one `.pak` holds
  that archive alone, none of its folder's other paks or loose files, so a
  gameplay mod that also carries `texts.pak` lends only its texts
  (`test-streamio`). Then `ITextManager::Reload` re-reads every text already
  handed out into the same objects; labels keep pointers to them and the GPU
  text compares the string each frame, so they draw the new language in the
  very step that follows. Labels a screen copied out of a text (the settings
  rows and tabs) are set again by the settings screen in that same step.
- At start the option system's `Init` runs every action after the profile's
  config is read, so the chosen language is mounted before the first frame;
  a profile switch runs `Init` again.
- Not covered: the in-mission options screen (a mission's loaded texts
  would switch too, but nobody asked for it there); fonts (a language that
  needs other glyphs needs its fonts as well, which the option does not
  mount); the row's own texts are new and not in the ELK database yet.

## Exit criteria for Phase 7

- **E-01** Opening `Data/ELK/elk.xml` (a copy) and saving without edits
  leaves every database file byte-identical and `elk.xml` semantically equal;
  the full open takes under 2 seconds on the dev Mac.
- **E-02** Exporting the unmodified database produces a `.pak` whose entry
  names equal `Data/ELK/texts.pak`'s text entries and whose `.txt` bytes
  are equal, or each difference is listed and explained in the plan summary.
  Recorded in 07-02: `Data/ELK/texts.pak` is the MFC export of 2002-12-03,
  older than the database built from `game_1.upd`. Of the 2,712 texts both
  carry, 2,213 are byte-identical and 499 differ because the texts were
  re-imported since (a changed original, or a state dropped to Not
  translated so the original now goes out); 227 texts (`movies/`, ...) are
  only in the old pak and 140 (`medals/`, ...) only in the database. Names
  agree, all lower case. `test-elk-core` pins these counts.
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
  Recorded on Linux x64 (`zig build elk-game-reads-it`,
  `tools/zig/elk_game_reads_it.zig`): a copy of `Data/ELK` is exported to
  `mods/ELKTest/data/texts.pak` of the install and the real Game run with
  `-mod=ELKTest -profile=ELKTest`, its working directory, `XDG_DATA_HOME` and
  `BK_USER_ROOT` in a scratch folder, to its own `-reference-scene` capture
  of the main menu at 1024x768; then `textes/strings/imheader-gbutton-exitgame`
  (Exit Game) gets the translation `ELK E-06 MARKER`, the database is
  exported again and the Game captured the same way. Both runs exit 0;
  BK_MOD_TRACE shows the label opened from the MOD and BK_UI_TRACE the
  main menu at scale 1, so the button is at (650,527)-(970,577) as
  `MainMenu.xml` places it; 1,795 pixels differ between the frames, all in
  (732,543)-(890,563), inside the button. This found a Linux bug in the
  game, fixed with it: `CICChangeMOD` lower-cases the mod's name and
  StreamIO looked the `*.pak` folder up by that exact spelling, so a mod
  folder `ELKTest` mounted its `mod.xml` (loose files resolve without regard
  to case) but none of its paks; `loadArchives` now resolves the folder the
  same way (`test-streamio`). macOS and Windows are left to CI and the
  maintainer.
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
- **E-11** The Game's Language option switches to an ELK-exported language
  mod and back at once and keeps the choice across a restart. Recorded on
  Linux x64 (`zig build elk-game-reads-it`, second test of
  `tools/zig/elk_game_reads_it.zig`): the E-06 marker database exported to
  `mods/ELKTest`, the real Game run windowed at the desktop size (1920x1080
  here) in a scratch profile, driven by `BK_AUTO_UI` through Options >
  Settings > Gameplay > Language and V. In the Exit Game button
  ((1152,739)-(1605,813) on that window, from BK_UI_TRACE's layout of the
  main menu), 3,494 pixels change with the mod chosen; with Default chosen
  again the button is pixel-identical to the start; after a restart the
  menu comes up with the marker, pixel-identical to the chosen frame, and
  the log shows the mod mounted before the first scripted frame. Set on the
  open main menu, the switch shows in the first frame drawn after it: the
  frame before still has the marker, the frame after is identical to the
  original (0 pixels). macOS and Windows are left to CI and the maintainer.

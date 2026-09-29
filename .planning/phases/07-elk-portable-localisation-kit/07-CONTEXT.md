# Phase 7: ELK: portable localisation kit - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Port ELK, the Blitzkrieg localisation kit (`Sources/src/ELK`, 12,547 lines of MFC in 37 files), to the portable stack of the new map editor (a Zig app with Dear ImGui, `Sources/editor`) on macOS arm64 and Windows x64. Every ELK feature is ported ("all features implemented"): the text database, import from the game, from PAK/UPD and from a spreadsheet, export to PAK and to a spreadsheet, translation editing with original/description/image/translation views and states, filters, find, statistics, spell check, font generation, the text tree, help, and the short translator mode. The output must be read by the game unchanged. When `07-PARITY.md` shows every feature verified, the MFC ELK and its shipped Windows binaries are deleted.

The design spec is `docs/superpowers/specs/2026-09-30-portable-elk-design.md`; this file records the decisions (D-01 to D-20) that spec and the plans follow. The Resource Editor (Phase 6) is discussed in parallel and is assumed to extract a shared editor kit from the map editor; ELK reuses it. Out of scope: the other small tools (Phase 8, including the FontGen command-line tool, which will reuse ELK's font library), a new localisation system for the game itself (`docs/PLANNED_FEATURES.md` section 2), and Linux.

</domain>

<decisions>
## Implementation Decisions

### Product shape and shared code
- **D-01:** ELK is a separate executable `ELK`, not a mode of `MapEditor` or of a shared editor app. Translators get ELK alone (it has always shipped separately), it needs none of the engine bridge, and its release cadence is independent.
- **D-02:** Layout: `Sources/editor/elk/core` (std-only Zig, headless), `Sources/editor/elk/app` (SDL3 + ImGui), `Sources/editor/elk/help` (Markdown embedded in the binary), plus one small C++ library `Sources/src/FontKit` (C ABI) for the game's `.tfd` and DDS writers.
- **D-03:** ELK consumes the shared editor kit Phase 6 extracts (ImGui host and frame loop, key=value settings file and recent list, file dialogs and `PathSlot`, crash-safe write, the `BK_*_AUTO` harness, the TGA icon decoder, CRT/assert routing). If the kit is not there when Phase 7 starts, plan 07-06 extracts the minimum into `Sources/editor/kit` as a move, never a copy, and Phase 6 rebases on it.
- **D-04:** Modes: extended is the default; `--short` (and legacy `-short`) selects the translator layout; `--developer` is an alias for extended. The code says `-short`, the old help text says `-developer` selects extended: both are accepted. Short mode auto-opens `elk.xml` beside the executable, applies the newest `.upd` per prefix at start, keeps the five fixed filters and one toolbar, and resets its layout each launch.

### Database and game output
- **D-05:** The on-disk database (`elk.xml`, `<element>_data_base/`, `.elk` / `.txt` / `.xml` / `.dsc` / `_folder.dsc` / `resource.description`) is read and written exactly as the MFC ELK does, so `Data/ELK` opens unchanged and old and new ELK can share a database during the transition. Folder names resolve case-insensitively, keys are lower case on import, `Path` is written as the bare element name (the MFC ELK accepts that), the unused `_DATA_BASE_RESERVE` folder is not ported.
- **D-06:** Text files are UTF-16LE with `FF FE`, CRLF on disk and LF in the editor, trailing CR/LF/NUL stripped exactly like `CELK::FromWideText`; reads tolerate a missing or big-endian BOM; an untouched text written back is byte-identical.
- **D-07:** The code-page setting is dropped: Unicode throughout; fonts carry the Unicode code points in use (what the game looks up).
- **D-08:** Export writes a zip `.pak` (`<key>.txt` entries with forward slashes, no directory entries, deflate level 9, CRC and DOS time; fonts under `fonts/<size>/1.tfd|1_c.dds|1_l.dds|1_h.dds`) that the game's `StreamIOZig/zip.zig` and `unzip -t` accept. Zig's `std.compress.flate.Compress` replaces `zip.exe`; the reader is `zip.zig` imported as a module.
- **D-09:** The game mounts mods, then loose files, then archives, so `Data/Texts.pak` is shadowed by a loose `Data/Textes` (the repository has one). Export defaults to a mod (`mods/<Name>/data/texts.pak`); "Install to game" and "Run Game" still write `<game>/Data/Texts.pak` but warn about shadowing and offer the mod route. Run Game launches `Game -windowed -mod=<Name>` in a dedicated `ELKTest` profile, never the user's profile, saves or cloud sync.

### Exchange formats, spell check, fonts
- **D-10:** Spreadsheet: `.xlsx` (read and write, new default), `.csv` (UTF-8 BOM, read and write), legacy `.xls` BIFF8 (read only). Columns `Path`, `Original`, `Translation`, `State`, `Description`, sheet `BlitzkriegELK`, backslash paths, import semantics ported exactly. The backtick-for-apostrophe swap applies only when reading legacy `.xls`.
- **D-11:** Spell check uses Hunspell, vendored and built by `zig build`, with dictionaries from a user-chosen folder (redistributable ones committed with licence files), a language combo, a plain-text `custom.dic` user dictionary (legacy `CUSTOM.DIC` reads fine), and the `CSpellChecker` word and delimiter rules. Without a dictionary the feature is disabled with a reason.
- **D-12:** Fonts: rasterise with the already vendored `stb_truetype`, pack the atlas with the `CFontGen` rule, write `.tfd` and DDS through FontKit (the engine's own writers) so the game reads them back. System fonts are found by scanning OS font folders (name and cmap tables) and filtered by coverage of the characters in use. Not pixel-identical to GDI; acceptance is measured against `Data/Fonts` and `Data/Fonts Variants`.
- **D-13:** The ELK window loads a system Unicode font stack (ImGui 1.92 dynamic glyphs) with an override setting, so any translation is readable while editing.

### Application behaviour
- **D-14:** Long operations run on a worker thread with a progress modal (Cancel is safe: temp names swapped in, a cancelled import leaves every file whole).
- **D-15:** Saving: on leaving a text, Save, Close, Exit, plus every 30 s while dirty; every write is temp + rename; Ctrl+Z is the edit box's own undo.
- **D-16:** Settings live in `elk.cfg` in the kit's user-data area (recent ten ELKs, last paths, named filters and current one, find options, font and sizes, collapse-item, toolbar visibility, spell language and folder); the registry is not read.
- **D-18:** Help: the two `.doc` helps are converted once to Markdown, corrected for the new UI, shown in an in-app window on F1; `elk.chm` is dropped.
- **D-20:** Every extended-mode menu command is kept; "Customize" becomes a window that shows or hides toolbars and buttons and resets defaults; free toolbar dragging is replaced by ImGui docking. Recorded as an intended adaptation.

### Retirement and testing
- **D-17:** At parity, delete `Sources/src/ELK`, the binaries in `Sources/elk` and the same binaries in `Data/ELK` and `Data/AmericanELK` (keep databases and updates), and the `A7.sln` entry.
- **D-19:** Tests use only tracked data (`Data/ELK`, `Data/AmericanELK`, `Data/Fonts`, `Data/Fonts Variants`, loose `Data/Textes`) copied to `zig-out/local-test/elk`; never a GOG install or the AchtungPanzer2 mod; the win-home GOG install is a stdout-only manual check.

### Claude's Discretion
- Internal module boundaries inside `elk/core`, the exact file names, the shape of the C shim for `stb_truetype`, the FontKit struct layout, the progress-modal layout, the icon choices, colours for the four states (keep red / yellow / green / blue), the look of the toolbars, and the wording of messages, within the spec.
- Which dictionaries to commit (subject to licence check in 07-05) and the default spell language.
- Plan order inside a wave.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Sources/src/StreamIOZig/zip.zig`: std-only zip reader with an indexed, case- and separator-insensitive `find` and CRC-checked `extract`; its own test already reads `Data/ELK/texts.pak`. Import as a module.
- `Sources/editor/app/{host,panels,panels_logic,auto,pictures,crt,testlaunch}.zig`, `Sources/editor/core/{settings,autosave,files}.zig`: ImGui host, menu/dialog patterns, `PathSlot`, scripted UI harness (`BK_EDITOR_AUTO`), TGA icon decode, crash-safe save, settings format, test-launch of the game in a test profile (reused for Run Game).
- `vendor/dcimgui/src-docking/imstb_truetype.h` (stb_truetype), Dear ImGui 1.92.9b docking with dynamic fonts.
- `Sources/src/RandomMapGen/Resource_Functions.cpp` `SaveImageToDDSImageResource` and `Sources/src/Formats/fmtFont.h` `SFontFormat`: the game's own font writers, called by FontKit. `GFXGPU/GfxGpuObjectFactory.cpp` `FontGpu::Load` is the game's reader of `1.tfd`.
- Real data: `Data/ELK` (`elk.xml`, `texts.pak` with 2,939 entries, 22 `game_*.upd`, `desc.xls`, `game_data_base` with 2,852 originals), `Data/AmericanELK` (`usa01.pak`, `game_19/20.upd`), `Data/Fonts`, `Data/Fonts Variants/*.pak` (nine prebuilt faces), loose `Data/Textes`.
- Old help text (`Sources/elk/Blitzkrieg ELK help-eng.doc`, converted with `textutil`: 860 lines) describes the behaviour, hot keys, both layouts and the FAQ; it is the checklist for `07-PARITY.md`.

### Established Patterns
- Core stays std-only and headless; SDL/ImGui/C ABI live in the app; the C++ side is a thin guarded C ABI (EditorBridge pattern).
- Three-plus test tiers, CI on the macOS and Windows runners, Windows CRT asserts routed to stderr, test artifacts in `zig-out/local-test`, never `/tmp`.
- Measure before assuming (graphics and format questions have been answered wrongly by code reading).
- Game data storage order: overlays (mods), loose files, archives (`bk_storage_open`); `-mod=Name` mounts `mods/Name/data`.

### Integration Points
- `build.zig`: new `elk`, `elk-core-test`, `fontkit` and `elk-app` steps beside the map editor's; `package-game-editors` gains `ELK`; never `zig fmt` `build.zig`.
- `A7.sln` / `Sources/src/ELK/ELK.vcxproj`: removed at parity.
- Game launch and profile flags: `Game/GameMain.cpp` (`-mod=`, `-profile`, `-windowed`), `Platform/Paths.h`.
- The Map Editor's mod handling and the shared kit from Phase 6.

</code_context>

<specifics>
## Specific Ideas

- "All features implemented": nothing in the MFC ELK is dropped; the two intentional adaptations (toolbar dragging replaced by docking, `.xls` write replaced by `.xlsx`) are listed in `07-PARITY.md`, and the spell checker's engine changes but its behaviour is a superset.
- The output must be readable by the game unchanged: proved by the game's own zip reader and by starting the game on an ELK-exported mod.
- Controls should match the old ELK where it had one (Ctrl+O, Ctrl+S, Ctrl+N, Ctrl+P, Ctrl+F, F1, F7, Ctrl+Z/X/C/V, Del).

</specifics>

<deferred>
## Deferred Ideas

- Localising the game's own menus and mod metadata through a namespaced localisation table (`docs/PLANNED_FEATURES.md` section 2): a later phase; ELK edits the existing text files.
- A FontGen command-line tool: Phase 8, reusing ELK's font library.
- Writing legacy `.xls` files; reading the legacy registry settings; Linux builds.
- Machine-translation assistance, translation memory, glossary checks: new capabilities, not parity.
- Live preview of a translation inside the running game beyond Run Game.

</deferred>

---

*Phase: 07-elk-portable-localisation-kit*
*Context gathered: 2026-09-30*

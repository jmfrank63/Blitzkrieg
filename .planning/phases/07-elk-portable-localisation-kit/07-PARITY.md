# Phase 7: ELK parity list

Every feature of the MFC ELK (`Sources/src/ELK`, help text in `Sources/elk`) mapped to the plan that delivers it and how it is verified. The MFC ELK is deleted (D-17) only when every row is **Verified**. Status starts at "Planned"; a plan's summary flips its rows.

Plans: 07-01 core database, 07-02 packages, 07-03 spreadsheets, 07-04 fonts, 07-05 spell check, 07-06 app shell, 07-07 app workflows, 07-08 closure. Exit criteria E-xx are in the spec.

Legend for "Kind": **same** = identical behaviour, **adapted** = intended change (reason given), **superset** = same plus more.

## A. Data model and files

| # | Feature (MFC source) | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| A1 | Project file `elk.xml`: elements, description (Name, PAK, UPD, Fonts), path, version, `LastUpdateNumber` (`CELK::operator&`, `ELK_Methods.cpp`) | same; `Path` written as bare name | 07-01 | Round trip of `Data/ELK/elk.xml` and `Data/AmericanELK/elk.xml` (E-01) | Planned |
| A2 | Statistics and previous-statistics stored in `elk.xml` | same | 07-01 | Same round trip; values equal the file's | Planned |
| A3 | Database folder `<element>_data_base/` with `.elk` `.txt` `.xml` `.dsc` `_folder.dsc` `resource.description` | same; case-insensitive folder resolution | 07-01 | Open `Data/ELK`, list equals 2,852 originals (E-01) | Planned |
| A4 | UTF-16LE + BOM text codec, CRLF/LF, trailing CR/LF/NUL strip (`ToWideText`, `FromWideText`) | same; tolerant read | 07-01 | Byte-identical rewrite of every text; fuzz of BOM-less and BE files | Planned |
| A5 | Four states plus `Changed` flag, state file read/write, Outdated set only by update (`SELKTextProperty`, `SetState`) | same | 07-01 | Table test of every transition, `SetState` return value | Planned |
| A6 | Original/translation/description getters, description lookup (`<key>.dsc`, else folder `_folder.dsc`) | same | 07-01 | Test on `Data/ELK` folders with and without `.dsc` | Planned |
| A7 | Code page setting and ANSI conversion (`nCodePage`, `ToText`, `FromText`) | adapted: dropped, Unicode native (D-07) | 07-01 | Old value read and ignored; no ANSI paths remain | Planned |
| A8 | Recent ELK list (10), last ELK/PAK/XLS/game paths, current element, `SMainFrameParams` | adapted: `elk.cfg` replaces registry (D-16) | 07-01 (format), 07-06 (UI) | Save/reload test; recent limit 10; missing files greyed | Planned |
| A9 | `_DATA_BASE_RESERVE` folder helpers | dropped: never used by any code path | 07-01 | Documented: grep of the MFC source shows no caller | Planned |

## B. Tree, browsing, filters, find, statistics

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| B1 | Text tree from folder structure; folders vs texts; state colour = lowest state of children (red, yellow, green, blue) (`CELKTreeWindow::FillTree`, `UpdateFolderState`) | same | 07-01 (model), 07-06 (view) | Tree model test on `Data/ELK`; shot of the tree | Planned |
| B2 | Tree context menu: set Translated/Approved on a text or, recursively, a folder (`UpdateSelectedText`, `UpdateSelectedFolder`) | same | 07-01, 07-06 | Folder bulk-set test with propagated parent colours | Planned |
| B3 | Select item shows original, description, translation, state; saves previous text (`OnETNTextSelected`, `OnETNFolderSelected`) | same | 07-06 | `BK_ELK_AUTO` script; file bytes after navigation | Planned |
| B4 | Browse First/Previous/Next/Last through the active filter, enable/disable by availability; Ctrl+N, Ctrl+P (`GetNextItem` ...) | same | 07-01 (logic), 07-06 (UI) | Filter walk test; hot keys in the harness | Planned |
| B5 | Collapse-item mode (tree collapses other branches while browsing); always on in short mode | same | 07-06 | Harness check of expanded set | Planned |
| B6 | Five built-in filters (All, Not translated, Outdated, Translated, Approved) | same | 07-01, 07-06 | Filter Check table test | Planned |
| B7 | Filter editor: named filters, conditions = OR of AND-lists of folder substrings, state mask, "translated" flag, add/rename/delete, unique-name rule (`CCreateFilterDialog`, `SSimpleFilter::Check`) | same | 07-01 (logic), 07-07 (dialog) | Logic tests; dialog driven by the harness; persisted in `elk.cfg` | Planned |
| B8 | Filter combo in the browse toolbar; current filter saved | same | 07-06 | Restart test | Planned |
| B9 | Find: text over original, description, translation, direction, match case, whole word, resume position, cancel, selects tree node and text range (`FindItem`, `SSearchParam`) | same | 07-01 (logic), 07-06 (dialog) | Search tests incl. whole word, wrap; cancel test | Planned |
| B10 | Statistics window: tree of items/words/word symbols/symbols per state, original and translation separately, sums nest, previous snapshot (`CStatisticDialog`, `CreateStatistic`, `GetTextCounts`) | same | 07-01 (logic), 07-06 (window) | Counts equal the values stored in `Data/ELK/elk.xml` for the same database | Planned |
| B11 | Progress dialog with message, range, position (`CProgressDialog`) | same | 07-06 | Worker test; cancel leaves files whole | Planned |

## C. Editing view

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| C1 | Read-only original and description views with select/copy | same | 07-06 | Harness | Planned |
| C2 | Editable translation box: Undo, Cut, Copy, Paste, Delete, Select All, context menu, Unicode input | same (ImGui multiline undo) | 07-06 | Harness clipboard round trip | Planned |
| C3 | State radio buttons Not translated / Translated / Approved; automatic Translated on first edit, back to the initial state when the text returns to the original; manual state wins (`OnChangeTranslateEdit`, `bManualState`) | same | 07-06 | State machine test in core; harness | Planned |
| C4 | Grey/disabled translation for folders; description of a folder shown | same | 07-06 | Shot | Planned |
| C5 | Image next to the description: `icon.tga` of the text's game folder if present (`LoadGameImage`) | superset: also reads from `.pak` archives, not only unpacked data | 07-06 | Shot on a `Data/Units` text; missing image hides the panel | Planned |
| C6 | Save on navigation, Save, Close, Exit (`OnFileSave`, `CloseELK`) | superset: plus 30 s while dirty, atomic writes (D-15) | 07-06 | Kill test: no half-written file | Planned |
| C7 | Status bar with tooltips of the hovered action | same | 07-06 | Shot | Planned |
| C8 | Window title with the selected text path | same | 07-06 | Harness | Planned |

## D. Import and export

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| D1 | Import from PAK: partial update; changed original marks a translated text Outdated; new original Not translated; copies `.dsc`/`.description`; version string `<file>, [dd:mm:yyyy, hh.mm.ss]` (`ImportFromPAK`) | same | 07-02 | Table tests (unchanged, changed, new, absent); real `game_*.upd` files (E-03) | Planned |
| D2 | Import from UPD: complete update, deletes texts not in the update (`bAbsolute`, `CImportFromPAKEraseFile`) | same | 07-02 | `game_21` then `game_22` equals `game_22` alone (E-03) | Planned |
| D3 | Automatic update on start: newest `<prefix>_<n>.upd` per prefix, only if newer than `LastUpdateNumber`, adds new projects, removes elements without an update file (`UpdateELK`) | same | 07-02 | Fixture folder with several numbers; short-mode start | Planned |
| D4 | Import from Game: build a `.pak`/`.upd` from the game's `.txt`, `.dsc`, `.description` files, loose or packed (`CreatePAK`, `CImportFromGameDialog`) | same | 07-02 (logic), 07-07 (dialog) | Build from loose `Data/Textes` and from a `.pak`; parse with `zip.zig` | Planned |
| D5 | Export to PAK for the selected project: translation if state is not Not translated and non-empty, else original; only-filled option (`ExportToPAK`) | same | 07-02 | Compare with `Data/ELK/texts.pak` (E-02) | Planned |
| D6 | Export to PAK by filter (`SSimpleFilter::Check` on each key) | same | 07-02, 07-07 | Filtered export contains exactly the matching keys | Planned |
| D7 | Short mode "Export to PAK": one PAK per project, named from the UPD prefix descriptor, in the ELK folder; diagnostic message listing the names | same | 07-02, 07-07 | Multi-element fixture | Planned |
| D8 | Zipping with `zip.exe -9 -R -D` | adapted: Zig deflate writer (D-08) | 07-02 | `zip.zig` parse with CRC, `unzip -t`, and the game starts on it (E-06) | Planned |
| D9 | Export to Excel (Jet/ODBC `.xls` write) | adapted: `.xlsx` and `.csv` write (D-10) | 07-03 | Round trip of every text in `Data/ELK` (E-04) | Planned |
| D10 | Import from Excel: skip empty translations, trim `\t\r\n `, Outdated on original mismatch, Translated otherwise, write only when bytes differ, version string from the file date (`ImportFromXLS`) | same | 07-03 | Table tests; semantics equal to the MFC code paths | Planned |
| D11 | Import legacy `.xls` files translators already have | same via BIFF8 reader; backtick swap only here | 07-03 | `desc.xls` rows equal an independent decoder (E-04) | Planned |
| D12 | Delete project from ELK (keeps folder and translations; at least one project must remain) (`OnFileDelete`) | same | 07-01, 07-07 | Test: element gone from `elk.xml`, folder untouched, refuses the last one | Planned |
| D13 | Open, Close, Save ELK; reopen after import/delete; last-selected text restored (`nLastELKElement`, `szLastOpenedELKName`) | same | 07-01, 07-06 | Restart test | Planned |
| D14 | Recent ELKs submenu, open last ELK on start (extended) or preset `elk.xml` (short) | same | 07-06 | Harness | Planned |
| D15 | Run Game: pack current ELK to `<game>/Data/Texts.pak` and start `game.exe -windowed`; enabled only when a game is found and not already running (`UpdateGame`, `CheckGameApp`) | adapted: mod route by default, `-mod`, `ELKTest` profile, shadow warning (D-09); game found from setting, sibling of ELK, or Windows registry | 07-02 (logic), 07-07 (UI) | Game shows an edited string (E-06) on macOS and win-home | Planned |

## E. Fonts

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| E1 | Generate game fonts on export when the element's `Fonts` flag is set: tiny/small/medium/large at 8/16/24/48 (medium 16..24, large 16..48 user-set), `1.tfd` + `1_c/_l/_h.dds` (`CFontGen::GenerateFont`) | same output format; glyphs from stb_truetype | 07-04 | Metrics within 1 px of `Data/Fonts Variants/Arial.pak`, `BkFontRead` round trip, in-game capture (E-05) | Planned |
| E2 | Character set from the exported texts plus the default character; atlas packing rule, 2 px leading, power-of-two texture | same | 07-04 | Atlas sizes equal the shipped fonts for the same face and text set | Planned |
| E3 | Kerning pairs and per-glyph ABC widths | adapted: kern table only (no GPOS), ABC from stb metrics | 07-04 | Measured deviation recorded; no layout break in game | Planned |
| E4 | Font name and size dialog: lists fonts covering the characters, default button, fallback to the first covering font when the chosen one is missing (`CChooseFontsDialog`, `GetFonts`) | same; list from a font-folder scan | 07-04 (logic), 07-07 (dialog) | Enumeration test on macOS and Windows; fallback test | Planned |

## F. Spell check

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| F1 | F7 / menu / toolbar: check the translation word by word from the last position, select the misspelt word, report it (`OnToolsSpelling`) | superset: suggestions, Ignore, Ignore All, Add, Change, Change All | 07-05 (logic), 07-07 (dialog) | Known misspelling flagged with suggestions (E-08) | Planned |
| F2 | Word splitting and delimiters, ignore symbols (`SPELLING_WORD_DELIMITERS`, `WORD_DELIMITERS`, `IGNORE_SYMBOLS`, `GetWord`) | same | 07-05 | Table test against the constants | Planned |
| F3 | User dictionary (`CUSTOM.DIC`, `#LID`) | adapted: `custom.dic` plain text, legacy file readable | 07-05 | Add word, restart, still accepted | Planned |
| F4 | Language selection (Russian, US, British) and "unavailable" state disabling the command | adapted: dictionary folder and combo; disabled with a reason without a dictionary | 07-05, 07-07 | E-08 | Planned |
| F5 | Word/character counts used by statistics (`GetTextCounts`) | same | 07-01 | With B10 | Planned |

## G. Application shell

| # | Feature | Kind | Plan | Verification | Status |
|---|---|---|---|---|---|
| G1 | Extended layout: menus File, Edit, Browse, View, Tools, Help with every command listed in the old help | same | 07-06, 07-07 | Harness drives each command (E-07) | Planned |
| G2 | Short layout: reduced menu, single toolbar (Game, Pack, Filter, Prev, Next, Export, Import, Spell, Tree, Stats, Exit), no customisation, layout reset each launch | same | 07-06 | Harness with `--short` | Planned |
| G3 | Launch flags `-short` / `-developer` | same; both `--x` and `-x` accepted | 07-06 | Flag parse test | Planned |
| G4 | Toolbars: Project (File), Text (Edit), Browse, Interface (View), with tooltips | same | 07-06 | Shot | Planned |
| G5 | Customize: show/hide toolbars and buttons, reset; saved on exit in extended mode | adapted: window plus docking (D-20) | 07-06 | Persist test | Planned |
| G6 | View menu: ELK Tree on/off, ELK Statistic, Status Bar on/off | same | 07-06 | Harness | Planned |
| G7 | Hot keys: Ctrl+O, Ctrl+S, Ctrl+N, Ctrl+P, Ctrl+F, F1, F7, Ctrl+Z/X/C/V, Del, Alt+F4 (Cmd equivalents on macOS) | same | 07-06 | Harness | Planned |
| G8 | Full-screen and window rectangle remembered (`bFullScreen`, `rect`) | same via ImGui ini and settings | 07-06 | Restart test | Planned |
| G9 | Help window (F1) with the full help text, English and Russian | adapted: Markdown in-app, `elk.chm` dropped (D-18) | 07-06, 07-08 | Every help section present; hot-key list matches G7 | Planned |
| G10 | About dialog | same | 07-06 | Shot | Planned |
| G11 | Resizable dialogs remember size (`CResizeDialog`) | same via ImGui window settings | 07-07 | Restart test | Planned |
| G12 | Unicode display of any translation (`RecreateUnicodeEditControls`) | superset: system Unicode font stack (D-13) | 07-06 | Shot with Cyrillic, Chinese | Planned |

## H. Packaging and retirement

| # | Item | Plan | Verification | Status |
|---|---|---|---|---|
| H1 | `ELK` built by `zig build` on macOS arm64 and Windows x64 (MSVC), steps `elk`, `elk-core-test`, `fontkit` | 07-06, 07-08 | CI green on both | Planned |
| H2 | ELK packaged with the editors (`package-game-editors`) and as a standalone translator archive | 07-08 | Package listing; launch from the package | Planned |
| H3 | Run on win-home (stdout and capture where allowed; GOG install read-only) | 07-08 | Recorded in the summary | Planned |
| H4 | Delete `Sources/src/ELK`, `Sources/elk` binaries, `Data/ELK` and `Data/AmericanELK` binaries (`ELK.exe`, `bugslay.dll`, `image.dll`, `streamio.dll`, `zip.exe`, `ot711as.dll`, `sfl11as.dll`, `elk.chm`, `excel_rows.reg`, `CUSTOM.*`), the `A7.sln` entry | 07-08 | This list all Verified; `zig build` passes on all targets; no reference left (grep) | Planned |

## Intended adaptations (not gaps)

1. Toolbar dragging becomes docking plus a show/hide Customize window (G5).
2. `.xls` write becomes `.xlsx` and `.csv`; `.xls` read stays (D9, D11).
3. Spell engine changes from Microsoft CSAPI to Hunspell; behaviour is a superset (F1 to F4).
4. `zip.exe` becomes an in-process writer (D8); GDI fonts become stb_truetype (E1, E3).
5. Registry settings become `elk.cfg` (A8); code page dropped (A7); `elk.chm` becomes in-app Markdown help (G9).

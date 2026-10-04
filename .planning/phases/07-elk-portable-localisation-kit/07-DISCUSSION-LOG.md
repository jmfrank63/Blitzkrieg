# Phase 7: ELK: portable localisation kit - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md; this log preserves the alternatives considered.

**Date:** 2026-09-30
**Phase:** 07-elk-portable-localisation-kit
**Mode:** autonomous smart discuss. The user ordered "always choose the recommended answer, never come back with questions", so every grey-area answer below is the recommended one, chosen without asking.
**Areas discussed:** Product shape and shared code, Database and game output, Exchange formats / spell check / fonts, Application behaviour, Retirement and testing

---

## Product shape and shared code

| Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|
| Separate executable or mode of a shared editor app? | Separate `ELK` executable on the shared kit (D-01) | A mode of `MapEditor`: drags the engine bridge and GPU renderer into a text tool and ties the release to the map editor. One combined "editors" launcher app: same coupling, translators must download all editors. |
| Where does the code live? | `Sources/editor/elk/{core,app,help}` plus `Sources/src/FontKit` (D-02) | A top-level `Sources/elk` (holds the shipped Windows binaries, mixes old and new). Putting the font writers in the map editor's bridge (wrong owner). |
| Which mode is the default, and the flag? | Extended default, `--short` selects translator layout, `--developer` alias (D-04) | Copy the old help text (`-developer` for extended, short as default): contradicts the code, which uses `-short`. Only one flag: breaks old launch shortcuts. |
| Kit dependency on Phase 6? | Consume the kit; extract the minimum ourselves as a move if it is missing (D-03) | Copy the map editor's host code into ELK (two diverging copies). Block Phase 7 until Phase 6 finishes (needless serialisation). |

## Database and game output

| Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|
| On-disk database format? | Unchanged, byte-compatible, `Path` written as bare element name (D-05, D-06) | A new single-file store (SQLite/JSON): breaks `Data/ELK` and the 22 update files' workflow and the old ELK during transition. Keep absolute Windows paths in `elk.xml`: leaks host paths and cannot be portable. |
| Code page handling? | Dropped, Unicode throughout (D-07) | Keep the ANSI code page conversion: it only existed for the MFC edit control and GDI, and would reintroduce lossy round trips. |
| How is the `.pak` written? | Zig deflate writer (`std.compress.flate.Compress`), checked by `zip.zig` and `unzip -t` (D-08) | Bundle `zip.exe` (Windows only). Call system `zip` (not on Windows CI). Link zlib (extra dependency when std has it). |
| Where does the game find translated texts? | Default to a mod (`mods/<Name>/data/texts.pak`); "Install to game" writes `Data/Texts.pak` with a shadow warning; Run Game uses `-mod` and an `ELKTest` profile (D-09) | Only write `Data/Texts.pak` like the MFC ELK: silently shadowed by loose `Data/Textes`. Delete or rename loose texts: destructive. Use the user's profile to run the game: touches saves and cloud sync. |

## Exchange formats, spell check, fonts

| Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|
| XLS replacement? | `.xlsx` read/write default, `.csv` read/write, legacy `.xls` BIFF8 read-only (D-10) | CSV only: loses the workflow translators know. Keep `.xls` write too: a BIFF8 writer buys nothing. Excel COM/ODBC: Windows only. Drop `.xls` read: strands existing translator files (`Data/ELK/desc.xls` is one). |
| Backtick-for-apostrophe substitution? | Only when reading legacy `.xls` (D-10) | Keep it in both directions (an SQL-quoting artefact that corrupts real backticks). Drop it everywhere (misreads legacy-exported files). |
| Spell check engine? | Vendored Hunspell with user-chosen dictionaries and a plain-text user dictionary (D-11) | Platform checkers (NSSpellChecker, ISpellChecker): two code paths, not headless-testable, no user-chosen dictionaries. Word automation: Windows and Office only. Drop spell check: violates "all features". A pure-Zig Hunspell clone: large, needless. |
| How are game fonts generated? | stb_truetype rasteriser plus FontKit writing through the engine's own `.tfd` and DDS writers; measured against shipped fonts (D-12) | Pure-Zig `.tfd` and DXT3 writer: format drift risk. FreeType: new dependency when stb_truetype is already vendored. Keep GDI on Windows only: no macOS. |
| UI font for translations? | System Unicode font stack with override, ImGui dynamic glyphs (D-13) | Bundle one large CJK font in the repo (size, licence). Latin only (unreadable for most translations). |

## Application behaviour

| Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|
| Long operations? | Worker thread, progress modal, safe cancel (D-14) | Blocking UI as in MFC ("processed very slowly" is in its FAQ). Cancel without temp files (half-written imports). |
| Saving policy? | Save on leaving text, Save, Close, Exit plus every 30 s while dirty, temp + rename (D-15) | Only on navigation as before (loses work on a crash). Explicit Save only (breaks the "saved automatically" contract of the help). |
| Settings location? | `elk.cfg` in the kit's user-data area; registry not read (D-16) | Migrate the Windows registry on first run (Windows-only code for a one-time convenience). |
| Toolbar customisation? | Show/hide window plus ImGui docking (D-20) | Rebuild the SEC drag-and-drop customiser (large, low value). Remove customisation (violates "all features"). |
| Help? | Convert the `.doc` helps to Markdown, in-app window, keep Russian (D-18) | Keep `elk.chm` (Windows only viewer). Link to online docs (offline translators). |

## Retirement and testing

| Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|
| When is the MFC ELK deleted? | At parity, after every row of `07-PARITY.md` is verified, including the binaries in `Sources/elk`, `Data/ELK`, `Data/AmericanELK` and the `A7.sln` entry (D-17) | Delete when the new app first builds (loses the reference). Leave the binaries "just in case" (violates the requirement). |
| Test data? | Tracked data only, scratch in `zig-out/local-test/elk`; win-home GOG install stdout-only (D-19) | Read a GOG install in tests (never committed, not reproducible). Synthetic data only (misses the real 2,852-text database). |

## Claude's Discretion

- Module boundaries inside `elk/core`, the C shim for stb_truetype, FontKit struct layout, progress modal layout, icons and colours, message wording.
- Which dictionaries to commit (licence check) and the default language.

## Deferred Ideas

- Game-wide localisation table (`docs/PLANNED_FEATURES.md` section 2), FontGen command-line tool (Phase 8), writing legacy `.xls`, reading the legacy registry, Linux, translation memory or machine translation, live in-game preview.

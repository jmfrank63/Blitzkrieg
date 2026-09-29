# Phase 8: Small tools - Discussion Log

**Date:** 2026-09-30
**Mode:** autonomous smart discuss. The user ordered "always choose the recommended answer, never come back with questions", so no AskUserQuestion was issued; each grey-area question was answered with the recommended option and the rejected alternatives are recorded here. Decision ids refer to `08-CONTEXT.md`.

## Grey area 1: Tool set and architecture

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | Keep legacy binary names (`ExcelExporter`, `bzmconvertor`, `spcomp`) or new names? | `bk-<noun>` executables with subcommands (D-02): names say the job, no `.exe` assumption | Keep legacy names (perpetuates 2002 names, `bzmconvertor` also makes meshes and minimaps); one `bk` megatool (one binary that needs the engine for every subcommand, so the std-only tools would drag in the engine) |
| 2 | Language for each tool | By dependency (D-03): std-only Zig for pure transforms, small engine-linked C++ where the engine's saver defines the bytes | Everything in Zig (would re-implement structure-saver formats: drift risk); everything in C++ (Zig std-only tools are simpler, faster to build and already the pattern in `season_textures.zig`) |
| 3 | Scope of the set: only the eight named tools, or everything found | Everything found, decided per tool with a reason (D-01) | Only the roadmap's eight (would leave `spcomp`-adjacent modder needs like `convert_bik_to_ogv.ps1`, `AutoRun`, the sandboxes undecided) |
| 4 | Common conventions | Shared CLI contract, one shared test (D-04) | Per tool ad hoc (inconsistent exit codes, no shared test) |

## Grey area 2: Per-tool behaviour

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | zip/unzip | Replace by `bk-pak` on the release packager's writer, stored entries (D-05, D-06) | Keep shipping Info-ZIP exes (Windows only, unstable ordering); use OS `zip` (not on stock Windows, not deterministic); compressing writer now (deferred to 999.2) |
| 2 | zip2exe | Drop (D-07) | Port an SFX stub (Windows-only, AV false positives, no installer role) |
| 3 | ExcelExporter output | Preserve the legacy tab-text sheet as `.tsv`, patch XML in place on import (D-08, D-09) | Real `.xls`/`.xlsx` writer (needs a spreadsheet library, the legacy tool did not write one either); re-serialise XML on import like MSXML did (destroys comments and formatting of every touched file) |
| 4 | Root node table | Built-in table plus `--root` override (D-10) | Hard-coded (blocks new mod stat types) |
| 5 | FontGen rasteriser | `imstb_truetype.h` already vendored (D-12) | SDL3_ttf/FreeType (new dependency, not built in this tree); OS APIs per platform (CoreText + GDI: two code paths, two looks); keep GDI on Windows only (leaves macOS modders without fonts) |
| 6 | Font parity with GDI output | Structural checks + one measured in-game capture (D-14) | Byte parity (impossible with a different rasteriser); no acceptance beyond "it runs" |
| 7 | Font library shared with ELK | Yes, `FontAtlas` library, 08-02 scheduled first (D-13) | Two generators (duplicates the GDI code ELK already has); make Phase 8 wait for Phase 7 (ELK would then port GDI code that is thrown away) |
| 8 | bzmconvertor modes | Port `to-bzm`, `to-xml`, `obj2mod`, `validate-obj2mod`; drop `-fences` (D-16, D-19) | Port `-fences` too (one-time 2002 migration, no shipped map needs it); drop `obj2mod` (the Blender path depends on it) |
| 9 | imagedefrag | Port only the live minimap creator as `bk-map minimap`; drop packer/terrain/noise/spline (D-18) | Port everything (dead and commented code, nothing includes it); drop it entirely (loses batch minimap generation for maps) |
| 10 | spcomp | Port as `bk-sprite compose`, byte-identical `.san` (D-20) | Drop (3799 shipped `.san` show it is how sprite objects are made); leave to Phase 6 (an exe modders can run without the editor is still wanted) |
| 11 | OffsetRomb | Port as `bk-image offset-romb` (D-21) | Drop (cheap enough to keep, and tile art conversion is modder-relevant) |
| 12 | `convert_bik_to_ogv.ps1` | Port as a thin Zig wrapper `bk-video` (D-22) | Leave the PowerShell script (not on stock macOS); build a video transcoder (out of scope) |

## Grey area 3: Drops and removals

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Obsolete tools (keygen, BuildVersion, AutoRun, Maya/Photoshop plug-ins, WhereIS, PlanePathTest, InterfaceSystemTest) | Drop with written reasons in the inventory and commit messages (D-01, D-23) | Keep as reference under an attic folder (dead weight, git history already archives); port them |
| 2 | When are binaries and sources deleted | Binaries and ported sources in the plan that ships the replacement; dropped ones in 08-06 (D-23..D-25) | Delete everything first (breaks the goldens that must be captured from legacy code); delete everything last (leaves Windows binaries in the tree for the whole phase) |
| 3 | `Sources/src/bin/editor.exe` and `MapEditor.exe` | Out of scope, they leave with Phases 6 and 5 (D-24) | Remove now (would break the Windows `Editors/` staging before the replacements exist) |
| 4 | Build-graph cleanup | Remove `addFontGen`/`addBuildVersion`/`addBetaKeyGen`, update `excluded_utilities`, run the hermeticity test, never `zig fmt build.zig` (D-26) | Leave the dead Windows-only steps (they reference deleted sources) |
| 5 | Where tools ship | `Tools/` directory in the staged and packaged game on both platforms (D-27) | Only in the Windows `Editors/` folder (macOS modders get nothing); separate download (modders need the matching engine version) |

## Grey area 4: Testing, CI and packaging

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Test data | Small synthetic fixtures committed; opt-in real-`Data` tier that skips when absent (D-29) | Real GOG data in CI (never committed, not available in CI); fixtures only (would miss real-format regressions) |
| 2 | Windows verification | Run on `win-home` and in CI as part of the exit gate (D-30) | macOS only (the phase goal is both platforms) |
| 3 | Golden source for `obj2mod` and `.san` | Capture goldens from the legacy code/binaries before deleting them (plan split, 08-04/08-05) | Regenerate expectations from the new code (circular) |
| 4 | Documentation | One `docs/modding-tools.md` with a worked example per tool and an old->new table (D-31) | Per tool READMEs (scattered, drift) |
| 5 | Old CI guard scripts (`check_*.ps1`) | Keep, out of scope, deferred `bk-check` (deferred) | Port all now (40+ scripts, not modder tools, would swamp the phase) |
| 6 | Plan split and order | Six plans, 08-01 first, 08-02/03/04 parallel, 08-05 after 08-04, 08-06 last; 08-02 may be pulled before Phase 7's font plan (CONTEXT "Plan split") | One big plan (no wave parallelism); one plan per legacy tool (twelve tiny plans, repeated boilerplate) |

## Scope creep noted (deferred, see CONTEXT)
`.xlsx` support; a validator set (`docs/PLANNED_FEATURES.md`); `bk-check`; bundled open font; pak compression/Zip64.

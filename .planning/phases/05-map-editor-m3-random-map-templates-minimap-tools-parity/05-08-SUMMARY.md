---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 08
subsystem: map-editor
tags: [map-editor, m3, random-map-generation, seed, progress-hook, determinism, export-lists, parity, zig, cpp]
status: complete

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 01
    provides: the m3-auto scenario step, the new-map/open chain, the token-edit discipline
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 03
    provides: ListRmgFolder, the shared storage scan the dialog's combos use
provides:
  - BkEditorCreateRandomMap and CreateRandomMapInSession - one generation through the engine's own CMapInfo::CreateRandomMap (2100 validated lines, never re-typed), with a real IProgressHook (SBkProgressHook), the seed in and out, names checked against the storage, ranges and a plain map name checked, an output folder built from the user root (or the mod's) and refused inside the data
  - the D-05 fix - the generated map records the active mod's name and version (the MOD.Name / MOD.Version globals), not the template's chapter name
  - BkEditorListStorageFiles and BkEditorRmgTemplateGraphs - the Export lists' reads (folder + extension listing with the extension kept; a template's graphs with their weights)
  - File > Create Random Map (the MFC dialog's fields + a seed + Replace-existing), the 19-step progress modal with no cancel, the result modal (seed used) with Open map, and the generated map opened as a normal document
  - Tools > Export lists (graphs, contexts, patches, maps) in the MFC line format, written to <UserRoot>mapeditor/logs
  - commands rmg_dialog[:open|close|ok|open_map], rmg_set:<field>:<value>, rmg_generate, export_lists:<kind>; predicates rmg_dialog, rmg_phase, rmg_seed, export_file, export_lines
  - tools/zig/rmg_determinism_test.cpp + step test-rmg-determinism (D-04/D-40.5's machinery; backlog 999.1's gate harness) and the shared addEngineHostedTool (random-missions rides it too)
  - PARITY rows F10, T3, T4, T5, T6 closed; T7 (not a feature) re-read
affects: [05-09, 05-10, 05-11]

commits: 5
plan_head_before: 1bb45fd7d8e44d057027bd862dfe0f674049f5e3
actuals:
  tokens: 40000
  tasks: 3
  commits: 5
  files: 19

tech-stack:
  added: []
  patterns:
    - "The seed is a 32-bit number that IS the generator's state: the Zig StreamIO's seed blob keeps it in its first word, so a seed is a blob with that word set and the .seed file the generator writes holds the same word - the seed used is read back from the file, and any generation can be asked for again"
    - "A scripted run drives the dialog's own fields (rmg_set:<field>:<value>) and its own OK (rmg_dialog:ok): the auto grammar's 64-character argument limit rules out ten fields on one line, and a script then exercises the code a person does"
    - "A modal for a blocking call is announced for TWO frames: an auto-sized window is hidden for its first frame, so one announced frame showed nothing before the window froze"
    - "Engine-hosted C++ tools share one builder (addEngineHostedTool): linked, staged beside Game and run on the editor bridge's engine, writing only under zig-out/local-test"

key-files:
  created:
    - tools/zig/rmg_determinism_test.cpp
  modified:
    - Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp
    - Sources/src/EditorBridge/session.h, session_rmg.cpp, bridge.h, bridge.cpp
    - Sources/editor/core/bridge.zig, editor.zig, fake_bridge.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, panels.zig, panels_m3.zig, panels_logic.zig, commands.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md
    - .planning/WINDOWS.md

key-decisions:
  - "Seed model (recommended option): a 32-bit number, blank draws one with std::random_device, the reported seed is read back from the .seed file's first word; the same seed + template + context + setting + level + graph + angle gives a byte-identical map, and with graph or angle left at -1 the seed decides them too"
  - "The output root is the user root (or <UserRoot>mods/<Folder>/ with a mod active) and nothing the caller sends; the Settings window's own 'Maps folder' is not honoured for generation because the generator appends maps\\<name> to a root"
  - "A repeat of a map name is refused unless overwrite is set (the MFC overwrote silently): the dialog has a 'Replace a map of that name' box, a script sets it with rmg_set:overwrite:1"
  - "The context combo lists only the chapters' context.xml files - the MFC's own enumeration (EnumFilesInDataStorage's context.xml extension) - not every chapter file"
  - "The graph field defaults to -1 (the template's weights pick, as the game's briefing does); the MFC's edit took any integer, the bridge refuses one outside the template's range naming the field"
  - "Progress (Open Question 3): the frozen-window fallback. The callback only counts; the modal is on screen at 0 of 19 for two frames, then the generator runs on the main thread with the window waiting, then the result modal - the Update Map model, D-03's main-thread mandate kept"
  - "The result is a modal (the MFC's message box) with the seed used and an Open map button; a scripted run opens the map straight away through the open path, which is guarded by the unsaved-changes prompt like any Open (D-02)"
  - "rmg_set + an argument-less rmg_generate instead of the plan's ten-field rmg_generate=: the auto grammar caps an argument at 64 characters (T-04-03-01, left as it is)"
  - "Export lists need two new reads (BkEditorListStorageFiles, BkEditorRmgTemplateGraphs) rather than extra BkEditorListRmg kinds: the MFC's lists keep the extension, mix .bzm and .xml, and the graphs list needs each template's weights"
  - "D-05's source is the MOD.Name/MOD.Version globals the editor's mod switch and the game's own switch both set; with no mod the name is empty and the version the template's own"
  - "The briefing picture goes beside the map (maps\\<name>_large) - the MFC left it in the working directory"
  - "The m3-auto run redirects XDG_DATA_HOME (macOS and Linux) so its generated map and exported lists land under zig-out/local-test, not in the person's folders"

requirements-completed: [D-01, D-02, D-03, D-04, D-05, D-13, D-37, D-40]

duration: ~2h 45m wall (about 70 minutes of it the random-missions cover sweep and the full bridge tier running)
completed: 2026-10-02T22:20:00Z

coverage:
  - deliverable: "D-01/D-02 Create Random Map through the engine's own CreateRandomMap, written under the user's (or the mod's) maps folder, opened as a normal document"
    verification:
      - kind: test
        ref: "editor_bridge_test.cpp#TestM3CreateRandomMap (editor-bridge: M3 create random map ok): 15 refusal cases each naming their field, 19 progress steps, map/.seed/.lua/pictures under the user folder, the generated map opens at 8x8 patches in the template's season"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: rmg_set:* + rmg_generate + expect=rmg_seed:777 + expect=title:m3_auto_rmg.bzm, and the dialog-driven pass rmg_dialog:ok / open_map + expect=title:m3_auto_rmg_ui.bzm"
        status: pass
    human_judgment: false
  - deliverable: "D-03 progress modal: 19 steps, main thread, no cancel"
    verification:
      - kind: test
        ref: "TestM3CreateRandomMap: the callback ends at 19 of 19, never backwards; editor.zig 'create random map: a generation reports 19 steps'"
        status: pass
      - kind: command
        ref: "m3-auto captures m3-rmg-progress.tga (0 of 19 with the waiting text) and m3-rmg-result.tga (19 of 19, seed used, Open map / Close) - inspected"
        status: pass
    human_judgment: false
  - deliverable: "D-05 szMODName fix, the game's own generation paths undisturbed"
    verification:
      - kind: test
        ref: "TestM3CreateRandomMap: with EditorTestMod active the map records 'Editor Test Mod' / '1.0'; with none, an empty name"
        status: pass
      - kind: command
        ref: "zig build test-random-missions -Drandom-missions-sweep=cover: 208 cases, 0 failed, 3988 s (rc 0)"
        status: pass
    human_judgment: false
  - deliverable: "D-04/D-40.5 determinism through the editor's own path"
    verification:
      - kind: command
        ref: "zig build test-rmg-determinism: 'rmg-determinism: byte identical ok (seed 424242, graph 0, angle 0)' and '...with the graph and angle left open (graph 6, angle 0)'; another seed differs; a blank seed's reported seed regenerates that map"
        status: pass
    human_judgment: false
  - deliverable: "D-13 Tools > Export lists (T3-T6) in the MFC format, to the user's logs folder"
    verification:
      - kind: test
        ref: "panels_logic.zig export lists tests (the CRLF lines, the tab-indented graph lines, the two 3D maps left out, each kind's folder/extensions); editor-bridge storage listing and template graph reads; map-editor-engine: M3 random map reads round trip ok"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: export_lists:graphs|contexts|patches|maps + expect=export_file + export_lines (175, 23, 1696, 57 lines)"
        status: pass
    human_judgment: false
  - deliverable: "PARITY rows F10, T3-T6 closed with evidence (anchors re-verified: TEF:207, MainFrm.cpp:939/990/1031/1072/1138)"
    verification:
      - kind: command
        ref: "05-PARITY.md Evidence cells filled; editor.rc:2132-2135 lists Tools 0-3 only (T7 not a feature)"
        status: pass
    human_judgment: false
  - deliverable: "Windows leg (D-37)"
    verification:
      - kind: command
        ref: "pending-push: the branch carries the commits; the orchestrator pushes and win-home runs the non-GUI tiers plus the determinism step (phase-4 precedent: no GUI over SSH)"
        status: pending
    human_judgment: true
    rationale: "win-home needs the branch on origin, which a plan that does not say 'push' leaves to the orchestrator; every tier ran green on macOS arm64 and the new code is platform-plain C++/Zig (filesystem paths go through std::filesystem and the engine's backslash form)."

# Phase 05 Plan 08: Create Random Map and Export lists - Summary

One-liner: File > Create Random Map runs the engine's own `CMapInfo::CreateRandomMap` from the editor with a 32-bit
seed that the `.seed` file reports back, a real `IProgressHook`, the generated map opened as a normal document and
the active mod stamped into it (D-05); Tools > Export lists writes the MFC's four lists to the user's logs folder;
and a determinism harness proves a fixed seed gives byte-identical maps through the editor's own path.

## Accomplishments

- **Task 1 (`1d17db4d7`):** the engine fix at `RMGeneration.cpp` (the map records the active MOD, not the chapter),
  `CreateRandomMapInSession` and `SBkProgressHook`, the three new C entries (`BkEditorCreateRandomMap`,
  `BkEditorListStorageFiles`, `BkEditorRmgTemplateGraphs`), both vtable implementations (the C adapters and the fake)
  in the one commit, `TestM3CreateRandomMap`, the core's refusal tests, the ABI round trip and the step
  `test-editor-bridge-m3-rmg` for a fast loop.
- **Task 2 (`a690fe0c1`, `8656c81b1`, `7568a44a1`):** the dialog (every MFC field plus the seed and a
  Replace-existing box), Browse over the data, the progress and result modals, Open map, Tools > Export lists and
  the std-only logic behind them (`RmgFields`, seed and map-name parsing, the Browse name mapping, the export
  writers) with tests. The follow-up commit is the two-frame announcement the capture review found necessary and the
  scriptable OK / Open map; the last renames two symbols to the plan's names.
- **Task 3 (`9fcbd9012`):** `rmg_determinism_test.cpp` and `test-rmg-determinism`, the m3-auto frames (the dialog,
  a scripted generation with a fixed seed, a dialog-driven pass with three captures, the four exports), PARITY rows
  F10 and T3-T7, deferred items, the build shared by the two engine-hosted tools.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The C adapter swallowed the storage listing's refusal**
- **Found during:** Task 2's engine round trip (`test-map-editor-engine`)
- **Issue:** `vtableListStorageFiles` copied `vtableListRmg`'s "sizing REFUSED means the buffer was short" reading, so a
  folder or extension the bridge refused as not plain came back as an empty OK list.
- **Fix:** with a count of 0 the sizing pass's own status is returned (a real listing is refused WITH a count above 0).
- **Files modified:** Sources/editor/app/c_bridge.zig
- **Commit:** 1d17db4d7

**2. [Rule 1 - Bug] One announced frame never showed the progress modal**
- **Found during:** Task 3's capture review (`m3-rmg-progress.tga` showed no modal)
- **Issue:** an auto-resizing ImGui window is hidden for its first frame, so announcing the modal for one frame and
  freezing the window on the next showed the old screen for the whole generation.
- **Fix:** the announce phase lasts two frames (`rmg_announce_left`); the capture now shows "0 of 19".
- **Files modified:** panels.zig, panels_m3.zig
- **Commit:** 8656c81b1

**3. [Rule 3 - Blocking] The plan's ten-field `rmg_generate=` does not fit the auto grammar**
- **Found during:** Task 2's first m3-auto run (`bad token ... the argument must be at most 64 characters`)
- **Fix:** the dialog's fields are set one at a time (`rmg_set:<field>:<value>`) and `rmg_generate` takes no argument;
  the 64-character cap (T-04-03-01) is untouched. The scenario thereby drives the fields and the OK a person does.
- **Files modified:** panels_logic.zig, commands.zig, build.zig
- **Commit:** a690fe0c1, 9fcbd9012

**4. [Rule 3 - Blocking] The determinism harness found a path inside the generated map**
- **Found during:** Task 3's first run of the harness (two user folders differed in exactly one byte)
- **Issue:** the engine stores `szScriptFile = <output path>` in the map (as the MFC editor's did), so two folders
  differ in that path and nothing else.
- **Fix:** the harness generates into one folder and copies each map aside before the next; the finding is in
  `deferred-items.md` (it also makes Test in game say a generated map's script "is not a plain name").
- **Files modified:** tools/zig/rmg_determinism_test.cpp
- **Commit:** 9fcbd9012

### Differences from the plan's text (recorded, not bugs)

- **Two reads beyond the plan's list:** `BkEditorListStorageFiles` and `BkEditorRmgTemplateGraphs` (decisions above);
  the plan's "BkEditorListRmg extensions" cover the combos, which kinds 1, 4 and 5 already served.
- **`SBkProgressHook` is `class SBkProgressHook`** with the engine's object macros; the plan's `struct` is a naming
  freedom of C++.
- **A Replace-existing box and the bridge's `overwrite` flag** are additions: the plan did not say what a repeated
  name does, and the MFC overwrote silently.
- **The frozen window is the accepted fallback** (the plan's own Open Question 3 wording); the modal does not animate
  while the generator runs, and the callback never pumps events.
- **Windows:** no win-home run - see the coverage entry.

**Total deviations:** 4 auto-fixed (2 bugs, 2 blocking), the rest recorded. **Impact:** none on the plan's contract.

## Issues Encountered

- A vexing parse (`std::vector<T> v( size_t( n ) )`) and a path-form slip (`OpenFileStream` splits on backslashes only,
  so a host path with slashes "cannot open") cost one build each in the new test; both fixed in place.
- The random-missions cover sweep takes about 67 minutes in Debug (208 cases); the first run hit the background time
  limit after 83 passing cases and was rerun whole.
- `TestM3Fields` did not flake in any gate run this plan made.

## Deferred Issues

See `deferred-items.md` ("From 05-08"): the absolute script path in a generated map, the Settings maps folder not
moving a generated map, the progress modal not animating, the Windows leg.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. T-05-08-01 (generation outside the user root): the root is built bridge-side from the
user root and the active mod's folder and refused inside the data (tested by pointing the user root at `Data`); the
test asserts every file lands under the scratch root. T-05-08-02 (callback re-entrancy): the callback only counts
(`RmgProgress.report`, the test's counter). T-05-08-03 (names): storage-relative names checked with
`IsRelativeDataName` and against the storage, the map name must be one component - 15 refusal cases name their field.
T-05-08-04 (the D-05 fix disturbing the game): the random-missions cover sweep, 208 cases, 0 failed.

## Gate results (macOS arm64)

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run --summary all`: `Build Summary: 32/32 steps succeeded; 692/692 tests passed`.
- `zig build test-editor-bridge ...` (the full tier): `editor-bridge: M3 create random map ok`, `editor-bridge: M3 fields ok`, `editor-bridge: PASS`, rc 0. `zig build test-editor-bridge-m3-rmg ...` alone: `editor-bridge: M3 create random map ok`, `editor-bridge: PASS`.
- `zig build map-editor-m3-auto ...`: rc 0 - M1 `done (13 actions)`, M2 `done (298 actions)`, M3 `done (416 actions)`.
- `zig build test-rmg-determinism ...`: `rmg-determinism: byte identical ok (seed 424242, graph 0, angle 0)`, `rmg-determinism: byte identical ok with the graph and angle left open (graph 6, angle 0)`, `rmg-determinism: PASS`.
- `zig build test-random-missions ... -Drandom-missions-sweep=cover`: `random-missions: 208 cases, 0 failed, 3988 s`, rc 0 (the game's own generation path, after the D-05 fix).
- `zig build test-map-editor-engine map-editor-smoke test-map-files ...`: `map-editor-engine: M3 random map reads round trip ok`, `map-editor-engine: PASS (260 objects)`, `map-editor: smoke PASS (52 steps, 260 objects, ...)`, `map-file: PASS`.
- `zig test tools/zig/build_hermeticity_test.zig`: `All 3 tests passed` (after every build.zig edit).
- The plan names no game-side proof beyond the random-missions tier above.

## Self-Check: PASSED

- Files: tools/zig/rmg_determinism_test.cpp, the bridge entries in Sources/src/EditorBridge/bridge.h, the dialog and modals in
  Sources/editor/app/panels_m3.zig, and the m3-auto captures (m3-rmg-dialog, m3-rmg-progress, m3-rmg-result) exist on disk.
- Acceptance greps: szMODName fix with its comment, `CreateRandomMapInSession|SBkProgressHook` x5 and `IProgressHook`
  in session_rmg.cpp, `BkEditorCreateRandomMap` in bridge.h, `createRandomMap` in the fake, `drawRandomMapDialog|drawRmgProgress`
  x2 in panels_m3.zig, `rmg_generate|export_lists` in commands.zig, the four list names in panels_logic.zig, `byte identical` in the
  harness and `test-rmg-determinism` in build.zig; zero `std::min`/`std::max` in the added code.
- Commits: 1d17db4d7, a690fe0c1, 8656c81b1, 7568a44a1, 9fcbd9012 are in `git log`.

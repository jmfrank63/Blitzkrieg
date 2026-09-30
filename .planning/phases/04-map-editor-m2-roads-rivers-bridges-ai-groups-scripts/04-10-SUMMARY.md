---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 10
subsystem: map-editor
tags: [map-editor, m2, script-file, script-areas, lua, test-launch, record-kinds, mfc-conversion, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords (PutScriptFile, IsBareScriptName, Insert/Replace/Erase/IsAreaNameFree), the byte-identity rule
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: the M2 game-reads-it harness and BK_MAP_TRACE (script, area and Lua Trace lines)
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 09
    provides: reinforcement group 900 holding script ID 4245 and its held unit, the record_add/record_delete history entries and the vtable recordKeys/insertRecord/removeRecord
provides:
  - BkEditorScriptFile/BkEditorSetScriptFile (a bare name, None, or the value the file held when opened) and szScriptFileAtOpen
  - BkEditorScriptAreas/Add/Set/Delete, the pure conversions BkEditorScriptAreaFromVis/Moved/Resized, NMapGeometry::AreaFromVis/MoveArea/ResizeArea (the MFC Vis -> AI truncation, once)
  - records.Kind.script_file and .script_area, Editor.setScriptFile/scriptFileName/addScriptArea/editScriptArea/renameScriptArea/deleteScriptArea/scriptAreas, Editor.scriptAreaFromVis/Moved/Resized
  - core script_file.zig (isBareName, gameScriptName, scriptPathBeside/In, copyForTest, listBeside, pickedName, copyInto, copyAlong, openUrl) and Files.list with real and fake implementations
  - Map -> Script... window (list, Choose other with the overwrite question, Open script, missing-file warning), Save As copy-along question, Test in game copies the script beside the test map
  - tools_ai.ScriptAreas (key 8) with move and resize handles, the Script areas panel, area markers, commands and predicates
  - the game running the copied script, which finds area m2_area at its stored centre and lands the unit of group 900 (proved in game-reads-it M2)
affects: [04-11, 04-12, 04-13]

commits: 5
plan_head_before: 10e3491caac56b3d154053ca929c1d53f0d9d9eb
actuals:
  tokens: 55700
  tasks: 4
  commits: 5

tech-stack:
  added: []
  patterns:
    - "A list kind keyed by index rides the generic record path unchanged: recordKeys 0..count-1, insert at the key, remove at the key, a delete re-inserts the value read before it; a drag is editRecord under one gesture, so its edits are one undo step"
    - "A put exempts what the file held: the value a script file had at open (szScriptFileAtOpen) and a name two areas shared at open (openedAreaNames) may always be put back, so an undo of an edit on odd data never fails as a drift; only what an edit adds is held to the rule"
    - "The MFC conversion lives in one place (NMapGeometry) and the core never converts a length: the bridge answers drag, move and resize as pure functions, the fake mirrors the arithmetic"
    - "The game keeps only the last component of a script path (iMissionInternal.cpp), so every copy, list and URL is built from that one validated component (gameScriptName) plus a fixed folder, never from typed text"
    - "The scenario writes the script fixture beside a stand-in map under zig-out/local-test and copies it with the very call Test in game makes; a shipped folder is never written"

key-files:
  created:
    - Sources/editor/core/script_file.zig
    - Sources/editor/core/tools_ai.zig
    - tools/zig/fixtures/m2_script.lua
  modified:
    - Sources/src/MapFile/MapGeometry.h
    - Sources/src/MapFile/MapGeometry.cpp
    - Sources/src/MapFile/MapOverlay.h
    - Sources/src/MapFile/MapOverlay.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/session_records.cpp
    - Sources/editor/core/records.zig
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/files.zig
    - Sources/editor/core/root.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/commands.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/markers.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/tool_registry.zig
    - Sources/editor/app/game_reads_m2.zig
    - tools/zig/map_file_test.cpp
    - tools/zig/editor_bridge_test.cpp
    - build.zig

key-decisions:
  - "The script file put accepts None, a bare name (NMapRecords::IsBareScriptName) or exactly the value the file held at open, whatever it is: shipped maps hold a folder path ('maps\\BattleOfBulge'), and an undo must be able to put such a value back. Any other odd value is refused as new. A value of 64 characters or more reads as a refusal and saves byte-exact"
  - "The name a copy is made under is gameScriptName, the last component of the value: the game itself keeps only that (GameTT/iMissionInternal.cpp), so a shipped map's 'maps\\BattleOfBulge' copies BattleOfBulge.lua from beside the map. It is one component with no separator that passes isBareName, so the plan's mitigation (fixed folder plus a validated name) holds; empty, '..', 'x.lua' and odd characters still build no path"
  - "A palette-placed object is linked with nothing (nLinkWith 0), not -1: the game lands a reinforcement only when nLinkWith is 0 (CScripts::LandSuspendedReiforcements), so an editor-placed unit in a group waited in the queue for good. SAddObject gets an nLinkWith field defaulting to the old -1, the bridge's palette add sets 0, and the bridge, fence and trench pieces keep -1"
  - "Script area records are stored exactly as given (every field verbatim) and refuse an empty or taken name (case-sensitive), a centre off the map and a negative size; a name the file held twice at open may be put back as often as it held it (openedAreaNames), the analogue of 04-09's group rule; a map with a name of 64 characters or more reads as a refusal and saves byte-exact"
  - "Move and resize are two more pure bridge calls (BkEditorScriptAreaMoved/Resized) beside the plan's BkEditorScriptAreaFromVis: the core holds areas in AI units and must not convert, so the bridge answers what MoveArea and ResizeArea give and the tool edits the record through the generic command"
  - "The Script dialog is a floating window opened from Map -> Script..., like the Groups window; Choose other and the copies are refused for a map inside a game's data folder (read-only), Save As of a shipped map asks after the save, and Test in game copies without asking (the test folder is ours)"
  - "The tool hit-tests handles in map units against Pointer.map_x/y (radius 24, about a third of a tile) and grabs the centre before the edge; AI-per-world for the move's grab offset comes from BkEditorWorldToMap so the fake and the real engine agree"

patterns-established:
  - "The two-pass list read answers REFUSED on the sizing pass whenever there is something to count (a capacity below the total is REFUSED after writing what fits): callers ignore that status on the sizing pass and check the second"
  - "Engine-tier tests index a vector read back only after the check that it holds what the index needs: a hardened libc++ aborts on an index past the end, which hides every later check"
  - "Game-reads-it proves a Lua-side feature from the game's own trace: Init traces 1, then the numbers GetScriptAreaParams gave, then a periodic Report traces the count of a script group until the queued reinforcement lands"

requirements-completed: [D-20, D-21, D-02, D-01, D-06]

coverage:
  - id: D1
    description: "D-20: Map -> Script... sets szScriptFile to a bare name or None as one undo step; a value read from the file is kept verbatim until the user changes it and always accepted back"
    requirement: "D-20"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'script file: set, undo and redo are one step each, and an equal value records nothing', 'a name with a folder or .lua is Refused and changes nothing', 'a value read from the file is kept verbatim, and an undo puts it back'; records.zig 'a script file value compares by its text and never by the padding'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2ScriptFile on Data\\Maps\\Allies\\Ardennes\\battleofbulge.bzm (names 'maps\\BattleOfBulge'): the read equals the file verbatim, m2_script sets, saves as the NMapRecords-built map, the file's own value puts back and the bytes equal the unedited save; '..\\x', 'a/b', 'x.lua', 'x.LUA', '..', '.hidden', 'a b', 'a:b', 'dir\\name' REFUSED, an unterminated name BAD_ARGUMENT, all unchanged; a scratch map with '..\\odd name.lua' reads, takes a bare name and gets the odd value back but refuses another odd name; a 70-character name reads as a refusal and saves byte-exact (editor-bridge: M2 script file ok)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: do=script_file:m2_script, expect=script_file:m2_script, undo_depth 14 (BK_EDITOR_AUTO: done, 186 actions)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-20: the dialog lists the .lua files beside the map and None; Choose other copies a picked file beside the map asking before it overwrites; Open script goes through SDL_OpenURL on a file:// URL built from a validated path; a missing file is a warning; Test in game copies <name>.lua beside the test map; Save As of a shipped map copies the script along after asking"
    requirement: "D-20"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: script_file.zig tests for isBareName, scriptPathBeside/In, copyForTest (copies, overwrites in the test folder, warns when missing, builds no path for x.lua, '..', 'a b', '' and keeps a folder value to its last component, refuses a failing disk), gameScriptName, listBeside, pickedName, copyInto (copied, exists asks, overwrite, bad name), copyAlong, openUrl (file://, percent-encoded folder, nothing else, none for a missing file or a bad name); files.zig 'StdFiles list' on a real directory and 'FakeFiles list'"
        status: pass
      - kind: unit
        ref: "zig build test-map-editor-panels: panels_logic.zig 'copyScriptForTest: the script goes beside the test map, a missing one is a warning, no script says nothing' and 'a launch with a note shows it as a warning, and the next clean launch clears it'"
        status: pass
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: the fixture is written beside a stand-in map under zig-out/local-test and script_file.copyForTest copies it beside the test map; the game reports 'script name=\"m2_script\" loaded=1 init=1'"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-10-m2_areas.png (from the .tga shot), inspected: the Script window shows 'Script: m2_script', the orange warning 'm2_script.lua is not beside the map' (the map is coldwinter, a shipped one), the list None, BattlePaths, CentralConflict, LostVictory, RailroadWars, Uferstrasse, arnheim, barrikady ... and the Choose other... and Open script buttons"
        status: pass
    human_judgment: true
    rationale: "The SDL file dialog, the system's default editor and the two modal questions (replace, copy along) need a person at a desktop; the copy, list, URL and answer logic are unit tested and the window is drawn in the scenario. The scripted Save As of a shipped map finds no script beside it and so asks nothing"
  - id: D3
    description: "D-21: script areas are records in AI units; a new or edited area is converted Vis -> AI once with the MFC rule (truncation, radius through x) and untouched areas stay byte-exact; names are non-empty and unique case-sensitively"
    requirement: "D-21"
    verification:
      - kind: integration
        ref: "zig build test-map-files: tools/zig/map_file_test.cpp#TestM2ScriptAreaConversion - a rectangle dragged (100,200)-(300,260) is centre 283,325 half size 141x42 and equals the truncation arithmetic, a circle (50,60)-(80,100) is centre 71,85 radius 71, 0.35 -> 0 and 0.5 -> 1, MoveArea and ResizeArea, a write and read round trip, and an area inserted in and erased from a shipped map leaves its bytes (map-file: M2 script areas ok)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2ScriptAreas - FromVis/Moved/Resized equal NMapGeometry, a rectangle and a circle add and read back in order, the save equals the expected map, a rename, case-sensitive names, an empty or taken name, an off-map centre and a negative size REFUSED, a bad type, NaN, unterminated name or index BAD_ARGUMENT, all unchanged; add, edit and delete save the unedited bytes; a file's own duplicate name is put back beside its twin but a third is refused; a 70-character name reads as a refusal and saves byte-exact (editor-bridge: M2 script areas ok)"
        status: pass
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'script areas: add appends one step, undo removes, redo puts it back at its index', 'an empty or taken name is Refused with the history unchanged, and names are case-sensitive', 'a move and a resize are one undo step each within a gesture, and back at the start leaves none', 'delete puts the area back at its index on undo', 'a name the file held twice can be put back beside its twin, a third cannot'"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-21: the Script Areas tool (key 8) drags a rectangle or a circle and names it; the list centres the camera on a click and offers Rename and Delete; areas move and resize by handles; one undo step each"
    requirement: "D-21"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig - a rectangle drag adds a named area in AI units as one undo step, a circle drag, a click or zero-size drag is a note, a taken name is Refused and stays for another try, the centre handle moves and the edge handle resizes with one undo step per drag, a refused position mid-drag is skipped, a click selects (the last one drawn) and Delete removes it, Escape abandons a drag; test-map-editor-panels: tool_registry.zig 'the Script Areas tool is key 8'; view.zig 'the registry's shortcuts still switch the tools'"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=script_areas, do=area_name:m2_area, a rectangle drag, expect=area_named:m2_area, area_shape:circle and m2_ring, expect=areas_delta:2, a click and a drag on the first area's centre handle (undo_depth 14) undone by the key (13), do=area_rename:0:m2_zone, expect=area_named:m2_zone, undone (m2_area again), shot=m2_areas (BK_EDITOR_AUTO: done, 186 actions)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-10-m2_areas.png, inspected: the Script areas panel lists m2_area (rectangle) and m2_ring (circle) with the selected area's detail line, rename field, Rename and Delete; the map shows the cyan circle with its name and the selected rectangle in yellow with its name and handle (a thin strip, since the drag ran along the world x axis on screen)"
        status: pass
    human_judgment: true
    rationale: "Whether the handles are easy to grab at other zooms (a fixed 24 map-unit radius) and whether the outlines read well on other terrain is a judgment; the scenario proves the gestures and the undo steps"
  - id: D5
    description: "D-02, D-01: both new kinds ride the generic record command (an index-keyed list with add and delete, a singleton), and add-then-undo or edit-then-inverse writes the unedited file byte for byte"
    requirement: "D-02"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2ScriptFile and TestM2ScriptAreas save the unedited bytes after an edit and its inverse (two session saves compared, not a save against a fresh read, 04-01)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: PASS (260 objects) with the extended vtable through RealBridge"
        status: pass
    human_judgment: false
  - id: D6
    description: "The game reads it: the test-launched M2 map runs its copied script, which finds the named area (its traced centre equals the stored AI values) and lands the reinforcement group (one unit in script group 4245)"
    requirement: "D-20"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'BK_MAP_TRACE script name=m2_script loaded=1 init=1', 'BK_MAP_TRACE area name=m2_area cx=3072 cy=3072 (stored 3072,3072)', 'BK_MAP_TRACE lua: the script found m2_area at 3072,3072', 'script group 4245 held one unit from count 2 of 12 after LandReinforcement(900)'; game reads it M2 PASS (... group 900 held 1, script m2_script ran, area m2_area found)"
        status: pass
    human_judgment: false

duration: 1h02m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 10: Script file and script areas Summary

**A map names its Lua script and lists named areas, both edited with undo (the areas drawn, moved and resized by handles), the script is copied to where the game looks for it, and the real game runs the copied script, which finds the area by name at its stored centre and lands the reinforcement group.**

## Performance

- **Duration:** about 1 h (2026-09-30T05:13Z to 06:15Z), of which most was waiting on the three-to-ten-minute engine-tier and game runs
- **Tasks:** 4 (one tracer, three auto)
- **Files:** 30 changed, 4081 insertions, 19 deletions

## Accomplishments

- **Script file** end to end: `BkEditorScriptFile`/`BkEditorSetScriptFile` (None, a bare name, or the value the file held at open, kept verbatim so an undo can put back a shipped map's folder path), `records.Kind.script_file`, `Editor.setScriptFile`, the `script_file` command and predicate. `script_file.zig` (std only) carries the bare-name rule, the paths, and the copies through `Files`.
- **Where the game looks:** Test in game copies the map's script beside the test copy (a missing one is a status warning, never an error); the Map -> Script... window lists the `.lua` files beside the map (`Files.list`), Choose other copies a picked file beside the map after asking, Open script goes through `SDL_OpenURL` on `file://` plus the resolved folder plus a validated name; Save As of a shipped map asks whether to bring the script along.
- **Script areas** as records in AI units: the MFC Vis -> AI truncation lives once in `NMapGeometry::AreaFromVis/MoveArea/ResizeArea`, the bridge answers drag, move and resize as pure calls and adds, sets and deletes by index with unique non-empty case-sensitive names, and `Kind.script_area` rides the generic record command.
- **The tool:** `tools_ai.ScriptAreas` (key 8) draws, selects by a click, moves by the centre handle and resizes by the corner or edge handle (one undo step per drag), deletes with Delete; the Script areas panel lists, centres the camera, renames and deletes; markers draw every area with its name and the selected one's handles.
- **Evidence:** all tiers, the M2 scenario segment (two areas, a handle drag and its undo, a rename and its undo, the script file, the Script window) and a game that loads the copied script, reads `m2_area` at the stored centre, and holds one unit in script group 4245 once `LandReinforcement( 900 )` lands it.

## Task Commits

1. **Task 1: Script file end to end (tracer)** - `31aeb21d2` (feat)
2. **Task 2: The Script dialog** - `50011c277` (feat)
3. **Task 3: Script areas as records** - `ceaf36082` (feat), and its test fix `d56a37335` (fix)
4. **Task 4: Script Areas tool, panel and handles; the game finds the area** - `7bd631459` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; the engine tier (`editor-bridge: M2 script file ok`, `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)`), the core and panels tests and the game proof (`script name=m2_script loaded=1 init=1`) all passed before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core` | 178 tests pass (30 new since 04-09: script file 3 and script areas 5 in editor.zig, records.zig 2, script_file.zig 11, files.zig 2, tools_ai.zig 7) |
| `test-map-editor-panels`, `test-map-editor-view-events` | 132 and 70 pass |
| `test-map-files` | `map-file: M2 script areas ok`, `map-file: PASS` |
| `test-editor-bridge` | `M2 script file ok`, `M2 script areas ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (186 actions)` |
| `map-editor-game-reads-it-m2` | `game reads it M2 PASS (... script m2_script ran, area m2_area found)` |

Logs are under `zig-out/local-test/04-10-*.log`; the inspected shots (as PNG) are `04-10-m2_areas.png` and `04-10-m2_anchor.png`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] A unit the editor placed could never land as a reinforcement**
- **Found during:** Task 1 (the game proof: the script counted 0 units in script group 4245 on every one of twelve Reports)
- **Issue:** the overlay's `AddObject` gave every new object `nLinkWith = -1`; the game lands a queued reinforcement only when `nLinkWith == 0` (`CScripts::LandSuspendedReiforcements`), and the MFC editor wrote 0 for an object it had not linked. A unit placed for a reinforcement group would have waited in the queue for good.
- **Fix:** `SAddObject` gets an `nLinkWith` field defaulting to the old -1, so the spans, fences and trench pieces the group tools add are unchanged; the bridge's palette add (`AddObjectToSession`) sets 0; the three palette-add tests build their expected map with 0. After it the unit lands on the second Report.
- **Files modified:** `MapOverlay.h`, `MapOverlay.cpp`, `session.cpp`, `tools/zig/editor_bridge_test.cpp`
- **Commit:** `31aeb21d2`

**2. [Rule 1 - Bug] The plan's "numbers only, count in Init" script could not work**
- **Found during:** Task 1
- **Issue:** `LandReinforcement` only queues the unit; the game lands one queued unit at a time, every 200 game ticks, so `GetNUnitsInScriptGroup( 4245 )` in `Init` is 0. The plan foresaw this ("if it lands later, the script traces the count from a later-run function").
- **Fix:** the fixture registers `Report` with `RunScript( "Report", 100, 12 )`; the scenario reads `Init`'s `1`, then the two numbers `GetScriptAreaParams` gave, then Report's counts, and requires one to reach 1 and none above 1. Recorded: the unit lands at count 2 of 12 (about 200 game ticks).
- **Files modified:** `tools/zig/fixtures/m2_script.lua`, `game_reads_m2.zig`
- **Commit:** `31aeb21d2`, `7bd631459`

**3. [Rule 2 - Missing critical functionality] A shipped map's script value is a folder path**
- **Found during:** Task 1 (the engine test's first shipped map with a script, `maps\BattleOfBulge`)
- **Issue:** the plan's bare-name rule alone would refuse to copy, list or open the script of every shipped map, yet the game itself keeps only the last component of a script path (`iMissionInternal.cpp`) and looks for that beside the map.
- **Fix:** `gameScriptName` takes the last component and validates it with `isBareName`; `copyForTest`, `copyAlong` and `openUrl` use it. It is one separator-free component, so the mitigation (fixed folder plus a validated name) is unchanged: empty, `..`, `x.lua`, odd characters and separators still build no path. The put rule likewise accepts exactly the value the file held at open (`szScriptFileAtOpen`), not only bare names.
- **Files modified:** `script_file.zig`, `session_records.cpp`, `fake_bridge.zig`
- **Commit:** `31aeb21d2`

**4. [Rule 2 - Missing critical functionality] The scenario must not write beside a shipped map**
- **Found during:** Task 1
- **Issue:** the plan has game-reads-it copy the fixture "beside the edited map", which is coldwinter inside the game's data folder; shipped folders are never written (04-10's own binding constraint).
- **Fix:** the fixture is written beside a stand-in map path under `zig-out/local-test/game-reads-it-m2-script/` and `script_file.copyForTest`, the call Test in game makes, copies it beside the test map. The same guard covers Choose other (refused for a shipped map).
- **Files modified:** `game_reads_m2.zig`, `panels.zig`
- **Commit:** `31aeb21d2`, `50011c277`

**5. [Rule 3 - Blocking] The core cannot convert a handle drag, so two more pure calls**
- **Found during:** Task 3
- **Issue:** the plan lists `MoveArea` and `ResizeArea` in `MapGeometry` but only `BkEditorScriptAreaFromVis` in the ABI; the tool works in the core, which holds areas in AI units and must not convert.
- **Fix:** `BkEditorScriptAreaMoved` and `BkEditorScriptAreaResized` (pure, like FromVis), with vtable entries, the fake's mirror and tests against `NMapGeometry`.
- **Commit:** `ceaf36082`

**6. [Rule 1 - Bug] The first engine run of TestM2ScriptAreas aborted**
- **Found during:** Task 3 (reported by the orchestrator: SIGABRT from a hardened libc++ bounds check in `std::vector<BkEditorScriptAreaRecord>::operator[]`)
- **Issue:** the test's two-pass helper treated the sizing pass as a failure, but the sizing pass is BK_EDITOR_REFUSED whenever there is something to count (a capacity below the total is, as for groups); the empty vector then made `read[nTwin]` index past the end and the abort hid every later check.
- **Fix:** the helper accepts OK or REFUSED on the sizing pass and checks the second; every index in `TestM2ScriptAreas` is guarded by the size it needs (the twins are indexed only once the read is known to hold them). The bridge itself never wrote past a capacity (`Min( capacity, size )`). The rerun passed twice.
- **Files modified:** `tools/zig/editor_bridge_test.cpp`
- **Commit:** `ceaf36082`, `d56a37335`

### Plan wording resolved

- **`Test in game ... the shared launch helper used by game_reads_m2`:** `panels.zig`'s `requestTestLaunch` goes through `logic.copyScriptForTest` (panels_logic.zig, so its test runs in the panels tier); the game-reads scenario calls `script_file.copyForTest` directly, since it launches the game itself. `main.zig` is not touched.
- **`LaunchAttempt`:** a new `started_with_note` variant carries the missing-script warning, so the launch goes on and the status bar says why.
- **Dialog placement:** the Script dialog is a floating window (Map menu), like the Groups window, not a modal; only the two questions are modals. Map -> Script... is one menu item, `Choose other` and `Open script` are buttons inside it.
- **Name capacity:** the area and script records hold 63 characters and a NUL (`BkEditorScriptAreaRecord::name[64]`), so the "64 characters" refusal of the script file read is a 64-or-more value.
- **Fixture path:** the Lua file reaches the app through a build-time anonymous import (`m2_script_lua`) and `@embedFile`, so the scenario needs no path into the source tree at run time (one line in `mapEditorModule`, hermeticity test run after).
- **Predicates:** `expect=script_file:<name|none>` and `do=script_file:<name|none>` use `none` for None because the argument grammar has no empty value.

### Not a plan change

- **`view.zig` key test:** the old test sent key 8 as a key "nobody owns"; key 8 is now the tool's, so 8 moved to the shortcut test and 9 stays the unknown one.
- **The shipped scripts sit on disk beside their maps** (the Script window in the shot lists BattlePaths, CentralConflict, arnheim and others beside coldwinter), so with `gameScriptName` a Test in game of a shipped map that names one now copies it and the game runs it. Not exercised by a game run here; the copy is unit tested.
- **Commit trailer:** `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, as in 04-01 and 04-09: the model that ran this plan, not the line the sequential-execution message names.

---

**Total deviations:** 6 auto-fixed (two Rule 1, two Rule 2, one Rule 3, plus the test fix) plus wording resolutions
**Impact on plan:** no scope change beyond two extra pure bridge calls; deviation 1 fixes a defect that would have made editor-placed reinforcements inert in the game.

## Issues Encountered

- Each C++ change costs a three-to-ten-minute engine-tier run; the sizing-pass helper bug and the `nLinkWith` mismatch in three older tests were each found by that run.
- `LandReinforcement` landing later than `Init` needed a periodic script to observe it; the proof therefore depends on game time advancing (about 200 ticks), which it does in the unattended game (frame 120 is game time 4237 ms).
- The scripted Save As in the M2 scenario is of a shipped map that names `m2_script`, which is not beside coldwinter, so the copy-along question is (correctly) not asked; the question and its two commands are covered by code and compile, not by a scripted answer.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-11 (start commands, reserve positions) can add its records the same way: an index-keyed list rides `recordKeys/insertRecord/removeRecord/putRecord`, and the tool shape in `tools_ai.zig` (press, drag, release, keys, a `selected` index, handle hit-tests in map units from `Pointer.map_x/y`) is the pattern; `Files.list` and `script_file.zig` are there for anything that copies beside a map.
- Palette-added objects now carry `nLinkWith` 0; M3's link editing (`05-CONTEXT`) reads `> 0` as linked, which both 0 and -1 satisfy, so it is unaffected.
- The `m2_script.lua` fixture and `area_name`/`area_named`/`areas_delta` predicates are what 04-13's final walk-through can reuse.

## Threat surface

No new surface beyond the plan's model: T-04-10-01 - `isBareName`/`gameScriptName` and fixed folders, tested on `..\x`, `a/b`, `x.lua`, `..`, `C:x`, an empty value and odd characters in both the C++ and the Zig rule; T-04-10-02 - `copyInto` reports `.exists` and the dialog asks, only the test folder is overwritten without asking, Choose other and its copies are refused for a shipped map; T-04-10-03 - `openUrl` builds `file://` and the resolved folder (percent-encoded) and one validated component, returns null for a missing file, and is the only thing `SDL_OpenURL` is given; T-04-10-05 - empty and duplicate names refused case-sensitively in the bridge and the fake. T-04-10-04 accepted as planned (the fixture only reads and traces).

## Self-Check: PASSED

- Files found: `script_file.zig`, `tools_ai.zig`, `m2_script.lua`, `MapGeometry.h`, `bridge.h`, `session_records.cpp`, `records.zig`, `panels_m2.zig`, `commands.zig`, `game_reads_m2.zig`, `editor_bridge_test.cpp`, `map_file_test.cpp`, `build.zig`.
- Commits found: `31aeb21d2`, `50011c277`, `ceaf36082`, `d56a37335`, `7bd631459`.
- Acceptance greps: `BkEditorSetScriptFile|BkEditorScriptFile` in bridge.h 3; `copyForTest` in script_file.zig 10 and panels.zig 1; `function Init` in the fixture 1; `list:` in files.zig 3; `copyAlong|copyInto|openUrl|listBeside` in script_file.zig 31; `SDL_OpenURL` in panels_m2.zig 1; `AreaFromVis|MoveArea|ResizeArea` in MapGeometry.h 3; the four area calls in bridge.h 5; `ScriptAreas` in tools_ai.zig 20; `tools_ai.zig` in root.zig 1; `drawScriptAreas` in panels_m2.zig and panels.zig 1 each; `GetScriptAreaParams` in the fixture 1; `std::min|std::max` added 0.
- The executor looked at `m2_areas` (as PNG) and recorded what was seen (D2, D4).

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

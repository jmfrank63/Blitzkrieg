---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 05
subsystem: map-editor
tags: [map-editor, m3, players, unit-creation, check-map, fix-all, railroad-guard, game-reads-it, m3-auto, parity, zig, cpp]

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 01
    provides: the map-editor-m3-auto step, the M3 bridge patterns, the new-map chain
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 04
    provides: the object-level edits (fields, links, selection) Check Map's fixes reuse, the owner edit path
provides:
  - players added before the neutral and deleted, one bridge-logged edit each (diplomacies, unit creation, camera anchors and every re-owned object put back raw), with the Players window's Add/Del/S0/S1 buttons and Insert/Delete/0/1 keys (Task 1)
  - the per-player Unit Creation Info as a record kind, MutableValidate-style refusals naming the field, the choices the combos offer, and the Unit Creation Info window (Task 1)
  - Check Map - checks.zig over what the editor holds, the Check Map window with jump-to and checkmap_log.txt, Fix all as ONE composite undo step, Save only saying that the checks failed (Task 2)
  - the game's railroad loader skipping a railroad with fewer than two control points, proved on the real Game (Task 3)
  - editor-craft-fixtures (--craft), test-editor-bridge-m3-players, map-editor-game-reads-it-m3, m3-auto frames 336-458
  - PARITY rows F4, M4, M5, M7, S1 closed
affects: [05-06, 05-11]

commits: 3
plan_head_before: 39506aa725b592bb60ec3ff560c124907d194452
actuals:
  tokens: 64085
  tasks: 3
  commits: 3
  files: 27

tech-stack:
  added: []
  patterns:
    - "A bridge edit that touches several collections is ONE IEditRecord holding the states before and after (SPlayersEdit); undo and redo put them back raw, in the order that keeps every engine owner inside the table in force"
    - "Fix all records each fix through the editor's own calls and then folds the undo entries recorded since the start into a history `composite` step (undo last first, redo in order); a failure part-way keeps what applied inside that one step"
    - "An owner outside the diplomacy table was never the engine's owner (PlaceOneObject hands it a stand-in): moving a record into the table, or out of it again, is a change of the records alone"
    - "A scenario that needs a map no editor tool can make asks the bridge test for it (`--craft <kind> <file>`), the build runs that before the scenario"
    - "A screenshot reference is re-seeded by a plan that changes the UI, the old one kept as <name>-before-<change>.tga (05-03, 05-04 did the same)"

key-files:
  created:
    - Sources/editor/core/checks.zig
    - Sources/editor/app/game_reads_m3.zig
  modified:
    - Sources/src/AILogic/RailroadGraph.cpp
    - Sources/src/MapFile/MapRecords.h, MapRecords.cpp
    - Sources/src/EditorBridge/bridge.h, bridge.cpp, session.h, session.cpp, session_records.cpp
    - Sources/editor/core/records.zig, bridge.zig, editor.zig, fake_bridge.zig, history.zig, root.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, commands.zig, panels.zig, panels_m3.zig, main.zig
    - tools/zig/map_file_test.cpp, tools/zig/editor_bridge_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md

key-decisions:
  - "Add inserts the new player before the neutral entry (the MFC dialog's own insert) and every owner at or above the old neutral moves up with it; the MFC let a neutral object take the new player's index and let the players above a deleted one change owner - here every owner follows its player, the deleted player's objects go to the neutral, a table keeps two players and the neutral at least and 17 entries at most (full parity's evident intent, recorded in PARITY M4)"
  - "The unit-creation record carries the vector's size (`slot_count`): a put makes the vector exactly that long, so the undo of a put that grew it brings the old size back byte for byte, like an AI side's count; a put whose slot count does not reach the player only sets the size. A bespoke record kind (RESEARCH A6: the generic add/delete path does not fit a vector of per-player slots), edited by the same `record_edit` command"
  - "Appear points are MAP (AI) units, as the file holds them (the MFC's list shows them divided by 64): the plan said world units, the file and the RMG's rotation (MapInfo_StaticMethods_RMGeneration.cpp:1094) say AI units. Points are listed in tiles and added at the view centre; the MFC's dialog has no map click either"
  - "Relax time below 1 is refused: the file reader turns 0 or less into 20 (SUnitCreationInfo::Validate), so a smaller value would not survive a save. Formation size 1..32 and counts 0..255 are the editor's own bounds (the MFC checks none)"
  - "`BkEditorSetUnitCreation` has no out_token (the plan listed one): it rides the generic record_edit command like the camera anchors, and the undo is a put of the before-record. The two player edits do hand out tokens"
  - "Check Map: an invalid link is CLEARED (the MFC deleted the object), a shared link ID is reported and left (the bridge keeps every edit away from such a record), a squad is never a duplicate (the MFC skipped formation members), a duplicate's removal is the MFC's own and is not asked, but removing an unknown-type object or a road with fewer than two control points is asked for"
  - "An object whose type the database does not know can now be DELETED (and restored): the explicit removal that replaces RemoveNonExistingObjects (PARITY F4). Every other edit of one is still refused; TestUnknownObjectIsReadOnly proves both"
  - "A party change does not rename existing flags (the MFC's ResetPlayersForFlags): the editor never edits an object it was not asked to; a flag that changes owner through a player edit does follow its new owner's party"
  - "The owner fix of Check Map reaches an object whose owner is outside the table on the records alone (no engine re-own); the same holds for its undo"
  - "The Players window got Add/Del/S0/S1 on one compact row above the list and its rows became selectable: the M1 and M2 screenshot references (16 files) were re-seeded, the old ones kept as <name>-before-m3-players-panel.tga"
  - "`delete_claimed` is reset before the right-hand panels are drawn, not just before the start-commands panel, so the Players list can claim the Delete key while it is focused"

patterns-established:
  - "Pattern: `--craft <kind> <file>` on the bridge test plus an `editor-craft-fixtures` build step, depended on by the scenarios that open the fixtures"
  - "Pattern: the check is a read, the fix is a separate undoable command built from the same edits a person makes"

requirements-completed: [D-30, D-33, D-37, D-40]

duration: one session (2026-10-02, about 14:10 to 17:00 local, most of it waiting for the engine tiers)
completed: 2026-10-02T10:00:00Z
status: complete

coverage:
  - id: D1
    description: "D-30 players: add (before the neutral, 16 + neutral at most) and delete (objects to the neutral, two players kept), Insert/Delete/0/1 in the Players window, each ONE undo step"
    requirement: "D-30"
    verification:
      - kind: unit
        ref: "map_file_test.cpp#TestM3Players (map-file: M3 players ok): Insert/Erase and their inverse byte-identical, the 17-entry bound refused untouched, a delete's owners against a rule written out in the test"
        status: pass
      - kind: integration
        ref: "editor_bridge_test.cpp#TestM3PlayersAndUnitCreation (editor-bridge: M3 players and unit creation ok): add/delete/redo/16 adds undone are the builder map and the unedited bytes; the neutral, a player past the table and a side of 2 refused; c_bridge_test.zig (map-editor-engine: M3 players and unit creation round trip ok)"
        status: pass
      - kind: unit
        ref: "editor.zig tests 'players: add puts a player before the neutral...', 'players: delete re-owns...', 'players: the neutral, a bad player, a bad side and the 17th entry are refused...', 'players: add and delete shift the camera anchors with their players'"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 336-382 (player_add, unit_creation_set, player_delete, undo/redo, expect=players:/player_is:)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-30 Unit Creation Info per player validated like MutableValidate; a put that grew the vector undoes byte-exact"
    requirement: "D-30"
    verification:
      - kind: integration
        ref: "TestM3PlayersAndUnitCreation: party/aircraft/formation/count/squad/relax/appear-point refusals name the field and change nothing; the put saves as the builder's map; the inverse saves the unedited file byte for byte"
        status: pass
      - kind: unit
        ref: "editor.zig tests 'unit creation: ...' (one undo step, merge by gesture, refusals by field, choices); records.zig UnitCreation tests"
        status: pass
    human_judgment: true
    rationale: "The Unit Creation Info window's layout (combos, int fields, the points list) has been seen only in the scripted captures of the frames, not tried by hand"
  - id: D3
    description: "D-33 Check Map: duplicates, invalid/duplicate links, owner out of range, unknown party, unknown object type, road/river with fewer than two control points; window with jump-to, log, Fix all as one undo step, Save only warns"
    requirement: "D-33"
    verification:
      - kind: unit
        ref: "checks.zig tests (every kind on a table of fixtures) and editor.zig 'check map finds every kind, and fix all fixes them as ONE undo step', 'fix all with nothing to fix records nothing; a refused fix is counted and the rest go on'"
        status: pass
      - kind: integration
        ref: "editor_bridge_test.cpp#TestM3CheckMap (editor-bridge: M3 check map ok): the six fixes' bridge half on a crafted map; the save is the builder map, put back in reverse it is the crafted file byte for byte"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 392-458 on the crafted fixture: six findings by kind, checkmap_log.txt written, Save says 'checks failed' and fixes nothing, Fix all = one step (2 findings left), 'remove' = 0 left, undo back to six"
        status: pass
    human_judgment: false
  - id: D4
    description: "Carried bug: the game's loader skips a railroad with fewer than two control points"
    requirement: "D-33"
    verification:
      - kind: automated_ui
        ref: "zig build map-editor-game-reads-it-m3: the real Game loads coldwinter plus a one-point and a no-point railroad and a regular map, exit 0 each (game reads it M3 PASS); without the guard the same map panics in CSplineEdge"
        status: pass
    human_judgment: false
  - id: D5
    description: "Windows leg (D-37)"
    verification:
      - kind: other
        ref: "pending-push: the orchestrator pushes the branch and the win-home non-GUI tiers run there (no GUI over SSH)"
        status: unknown
    human_judgment: true
    rationale: "The win-home leg needs the branch on origin; every tier ran green on macOS arm64"
---

# Phase 05 Plan 05: Players, Unit Creation Info, Check Map and the railroad guard Summary

**Players are added and deleted as one undoable edit that moves every owner with its player, each player's Unit Creation Info is editable under MutableValidate's rules, Check Map reports six kinds of defect with jump-to and a log and Fix all fixes them as one undo step while Save only says so, and the game's loader no longer crashes on a railroad with fewer than two control points - proved on the real Game, where the unguarded build crashes in the same map.**

## Performance

- **Duration:** one session (2026-10-02), most of it waiting for the engine tiers
- **Tasks:** 3/3
- **Files:** 27 (4818 insertions, 45 deletions)

## Accomplishments

- **Task 1** (`d1f0b9a98`, with Task 2): `NMapRecords::InsertPlayer/ErasePlayer/GetUnitCreation/PutUnitCreation`; `BkEditorAddPlayer/DeletePlayer/UnitCreation/SetUnitCreation/UnitCreationChoices`; the `unit_creation` record kind, `Editor.addPlayer/deletePlayer/unitCreation/editUnitCreation`, the `players` edit scope; the Players window (Add, Del, S0, S1, Insert/Delete/0/1) and the Unit Creation Info window.
- **Task 2** (`d1f0b9a98`): `checks.zig`, `Editor.checkMap/fixAll`, the history `composite` step, the Check Map window, `checkmap_log.txt`, the post-save note; unknown-type objects deletable; owner fix on the records alone.
- **Task 3** (`d1f0b9a98` harness, `1760ae670` guard, `3d5e4faa5` the hermeticity audit's word): the one-line `CRailroadGraphConstructor::Construct` guard, `--craft` and `editor-craft-fixtures`, `map-editor-game-reads-it-m3`, m3-auto frames 336-458, PARITY F4/M4/M5/M7/S1.

The three commits are not one per task: Tasks 1 and 2 share files (editor.zig, commands.zig, panels.zig, fake_bridge.zig, build.zig) that cannot be split without hand-picking hunks, and the game-proof harness (build.zig, main.zig) sits with them so the tree stays consistent; the engine guard is its own commit so it can be reverted alone.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] An out-of-table owner could not be re-owned, and its undo could not put it back**
- **Found during:** Task 2 (TestM3CheckMap, three runs)
- **Issue:** `PlaceObjectInSession` asked the engine to take the new owner and refused: PlaceOneObject hands such an object a stand-in owner, a tree has none at all, and the undo (owner 99) is refused by the engine too.
- **Fix:** an owner change with either end outside the diplomacy table is a change of the records alone; `PutObjectRecordBack` also skips the engine place for an object the engine never took when place and direction stay.
- **Files modified:** Sources/src/EditorBridge/session.cpp
- **Commit:** d1f0b9a98

**2. [Rule 2 - Missing critical functionality] Removing an unknown-type object was refused**
- **Found during:** Task 2 (the plan's "offers unknown-type removal and applies it")
- **Issue:** `DeleteObjectFromSession` kept every edit away from an object the database does not know, so Fix all could not remove one.
- **Fix:** delete (and restore) is allowed - the explicit removal replacing RemoveNonExistingObjects; the engine never held the object, the restore is exact. The old `TestUnknownObjectIsReadOnly` now proves the removal and that every other edit is still refused. The fake bridge follows.
- **Files modified:** Sources/src/EditorBridge/session.cpp, bridge.h, Sources/editor/core/fake_bridge.zig, tools/zig/editor_bridge_test.cpp
- **Commit:** d1f0b9a98

**3. [Rule 3 - Blocking] The fake bridge refused to take back what a file held**
- **Found during:** Task 2 (core test of Fix all's undo)
- **Issue:** undoing the party fix put the file's own party ("Narnia") back and the fake refused it; the real bridge accepts what the map held at open.
- **Fix:** the fake keeps the unit creation as opened (`unit_creation_at_open`) and takes a held name back.
- **Commit:** d1f0b9a98

**4. [Rule 3 - Blocking] A scenario needed maps no editor tool can make**
- **Found during:** Task 3 (the game proof and the m3-auto frames need a one-control-point railroad and a map with six defects)
- **Fix:** `editor_bridge_test` got `--craft <kind> <file>` (it has the engine and the object database the fixtures need), the build an `editor-craft-fixtures` step the scenarios depend on, and a `test-editor-bridge-m3-players` step (`--m3-players-only`) that runs the new engine tests alone in about a minute.
- **Commit:** d1f0b9a98

**5. [Rule 1 - Bug] The M1 and M2 screenshot references no longer fit the Players window**
- **Issue:** the Players window's rows and button row changed its pixels beyond the 1% tolerance in 17 references.
- **Fix:** re-seeded (as 05-03 and 05-04 did), the old ones kept as `<name>-before-m3-players-panel.tga`; the difference was checked against the new seeds (the whole picture differs at the UI text, not in the map). No code change.

### Differences from the plan's text (recorded, not bugs)

- Appear points are map (AI) units, not world units (the file and the RMG say so).
- `BkEditorSetUnitCreation` has no token (it rides `record_edit`); a new `BkEditorUnitCreationChoices` lists the combos' names.
- The unit-creation record is a bespoke kind (A6): the generic add/delete does not fit per-player slots.
- A5 audit: the diplomacy length is read from the table everywhere (`Document.diplomacy`, `SetObjectFields`, the combos); nothing assumed a fixed count except the owner re-map this plan adds.
- RESEARCH said the editor's headless session does not run `CAILogic::Init`: it does (`BkEditorOpenMap` -> `IAIEditor::Init` -> `InitEditor` -> `CommonInit`, AILogicInternal.cpp:821). Without the guard the crafted map panics in `CSplineEdge` in the EDITOR's own open, before the game is ever started - the guard protects both.
- Fix all folds the entries it records into a history `composite` step (a new command variant), not a bridge-level composite.

**Total deviations:** 5 auto-fixed (2 bugs, 1 missing functionality, 2 blocking). **Impact:** each makes the plan's own text work; none widens the scope beyond Check Map, players and the guard.

## Issues Encountered

- A background run of the engine tier was started while the previous one was still running and both wrote the same log; the garbled log (lines missing, FAILs hidden) cost one run. Every later run waited for its predecessor's `rc=` line.
- `map-editor-auto`'s `compare=edited` and M2's 16 compares failed after the Players window changed: re-seeded as above.

## Deferred Issues

See `deferred-items.md`: appear points by map click (the MFC has none), flag renames on a party change, shared link IDs reported not fixed, the Windows leg, and the TestM3Fields byte diagnostic that still prints (window 4).

## Known Stubs

None.

## Threat Flags

None. The new entries validate before they touch the map (T-05-05-03), a refusal changes nothing (map-file and engine tests), Fix all is the individually tested edits folded into one step with a byte-exact undo (T-05-05-02), and the loader guard is the proved mitigation of T-05-05-01.

## Gate results (macOS arm64)

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run --summary all`: `Build Summary: 32/32 steps succeeded; 645/645 tests passed` (editor core, view, panels and the rest of the default tier).
- `zig build test-editor-bridge ...` (the full tier, 6 minutes): `editor-bridge: M3 players and unit creation ok`, `editor-bridge: M3 check map ok`, `editor-bridge: PASS` (the byte diagnostic `(identical? ...)` of TestM3Fields printed again - deferred item, window 4 - and the test passed). `zig build test-editor-bridge-m3-players ...` alone: rc 0.
- `zig build test-map-editor-engine test-map-files ...`: `map-file: M3 players ok`, `map-file: PASS`, `map-editor-engine: M3 players and unit creation round trip ok`, `map-editor-engine: PASS (260 objects)`.
- `zig build map-editor-m3-auto ...`: rc 0 - M1 `done (13 actions)`, M2 `done (298 actions)`, M3 `done (203 actions)` (frames 336-458 are this plan's).
- `zig build map-editor-game-reads-it-m3 ...`: rc 0 - `game reads it M3: short railroad loaded clean - 5 roads loaded, 2 of them with fewer than two control points, exit 0` and `game reads it M3 PASS (short railroad loaded clean: 5 roads incl. 2 short, exit 0; the regular map still loads: 3 roads, exit 0)`. Before the guard went in the same step died: `thread panic: reference binding to null pointer of type 'const value_type' (aka 'const CVec3')` in `CSplineEdge` (RailroadGraph.cpp:55) under `CommonInit` (AILogicInternal.cpp:821), raised by the editor's own `BkEditorOpenMap` on the crafted map.
- `zig build test-random-missions ... -Drandom-missions-sweep=cover`: `random-missions: 208 cases, 0 failed, 5333 s`, rc 0 (the loader's own generation path, the file the guard sits in).
- `zig test tools/zig/build_hermeticity_test.zig`: `All 3 tests passed` (after the build.zig edits; the first run caught a word in a step description, fixed in `3d5e4faa5`).

## Self-Check: PASSED

- Files: Sources/editor/core/checks.zig, Sources/editor/app/game_reads_m3.zig, the guard in Sources/src/AILogic/RailroadGraph.cpp, session_records.cpp and panels_m3.zig exist.
- Commits: d1f0b9a98, 1760ae670 and 3d5e4faa5 are in `git log`.

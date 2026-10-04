---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 05
subsystem: map-editor
tags: [map-editor, m2, roads, rivers, vso, cvsobuilder, edit-log, ai-passability, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords InsertVso/ReplaceVso/EraseVso/NextVsoID, the byte-identity rule
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 03
    provides: tool registry, right button / double click / Insert / Enter routing, commands.zig, markers.zig, the auto verbs
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: BK_MAP_TRACE terrain roads/rivers counts, game_reads_m2.zig, RemoveRoad's true answer
provides:
  - the bridge-side edit log (IEditRecord, edits/appliedEdits/undoneEdits, BkEditorUndoEdit/RedoEdit) that bridges, fences and entrenchments reuse
  - session_vso.cpp - roads and rivers derived once per edit with CVSOBuilder, one PutVso for do/undo/redo, saved-to-engine ID map, river AI order, VsoMatchesEngine, pick
  - C ABI BkEditorVsoDescriptors/VsoCount/Vso/AddVso/DeleteVso/MoveVsoPoints/SetVsoWidth/SetVsoOpacity/InsertVsoPoint/DeleteVsoPoint/PickVso/VsoMatchesEngine
  - core history .edit (tokens, scope vso|objects), Editor add/move/width/opacity/insert/deletePoint/delete/pick/read/count/descriptors, vso_generation
  - tools_vso.RoadsRivers with the MFC add and edit gestures, Pointer.screen_x/screen_y, fake roads and rivers
  - the Roads & Rivers panel (key 4), road and river markers, commands vso_kind/desc/width/opacity, predicates vso_delta/vso_points
  - a road and a river in the M2 game-reads-it and auto scenarios
affects: [04-06, 04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 4
plan_head_before: 83abb75e9b82ea3c159e0bc326a8f8ad3332f221
actuals:
  tokens: 56200
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Bridge-logged edits: one session function derives or compounds the change, stores the records before and after in an IEditRecord, puts them through one put, and hands the core a token; the core's `.edit` command keeps a gesture's tokens and undo/redo walk them (reverse / in order)"
    - "Editor.prepareEdit / commitEdit: everything that can fail (history room, the token list) before the bridge call; a drag's tokens merge into one entry by gesture and scope"
    - "A tool that must follow the pointer without an edit gets a plain `hover` call from view.handleMotion, not an Event, so status handling stays with real edits"
    - "BK_EDITOR_AUTO drags put press, drag(s) and release in the same frame: the view's stale-gesture guard ends a gesture whose button the real mouse does not hold at the frame's end"

key-files:
  created:
    - Sources/src/EditorBridge/session_vso.cpp
    - Sources/editor/core/tools_vso.zig
  modified:
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/history.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/tools.zig
    - Sources/editor/core/root.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/tool_registry.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/markers.zig
    - Sources/editor/app/commands.zig
    - Sources/editor/app/game_reads_m2.zig
    - tools/zig/map_file_test.cpp
    - tools/zig/editor_bridge_test.cpp
    - build.zig

key-decisions:
  - "The saved-to-engine ID map is a vector parallel to the saved list (vsoEngineIDs[kind][i]), not an unordered_map keyed by saved nID: it survives a file with repeated nIDs and is kept in step by insert/erase at the same index. The map-file tier measured 0 of 59 shipped maps repeating an nID; VsoMatchesEngine also fails on two records sharing an engine ID"
  - "An edit of an existing record hands CVSOBuilder::Update the record's own first point's width and opacity, where the MFC tool handed it the panel's current values, so an edit never depends on panel state (only a non-key point's opacity takes that value; widths are smoothed between the kept key points)"
  - "Insert gives the new key point the average width and opacity of the two key points it lies between (MFC: the panel's width and opacity)"
  - "Width mode 'from this point on' sets the grabbed key point and every later one, and for opacity every sampled point from it on; MFC's CW_MULTI width drag in practice changed only the grabbed key point (it re-saves the backup). 'All' sets every point, as MFC"
  - "The opacity edit resamples nothing (the MFC right-drag); right-press on a control point or a width handle of the selected line starts the opacity drag of that key point, anywhere else it cycles through the lines under the pointer (roads first, then rivers)"
  - "A press on empty ground while a line is selected deselects it without starting a new line (MFC's edit state ignores it); the next press starts one. Switching Road/River drops the selection and the unfinished line (MFC had two tools)"
  - "A move putting two neighbouring control points within 2 units is refused, where MFC let it sample a degenerate curve"
  - "The multi-mode 'earlier points' variant (MFC: Ctrl held) is a panel checkbox, because Ctrl+click is the right button in this tool"
  - "The M2 game-reads-it scenario adds the road and the river in the same edited run as the camera anchor (one game run, not two more)"

patterns-established:
  - "Engine-tier expected maps for derived records: the same CVSOBuilder calls on a map read from the file, then AreEquivalent with the saved map; every edit undone saves the unedited bytes"
  - "AI passability probe: IAIEditor::CanAddObject for the first placeable SGVOGT_UNIT at a world point (Vis2AI)"

requirements-completed: [D-07, D-08, D-09, D-03, D-02, D-06]

coverage:
  - id: D1
    description: "A road or river is its control polyline plus width and opacity at key points; the sampled points come from CVSOBuilder::CreateVSO, Update (DEFAULT_STEP) and UpdateZ, deterministic, both control and sampled points saved; a new road with passability 0 is saved as 1"
    requirement: "D-07"
    verification:
      - kind: unit
        ref: "zig build test-map-files: tools/zig/map_file_test.cpp#TestM2VsoBuilder (map-file: M2 vso builder ok)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Roads (editor-bridge: M2 roads ok) - the saved road equals the CVSOBuilder-built expected map"
        status: pass
    human_judgment: false
  - id: D2
    description: "Derived points are computed once per edit in the bridge and kept in its edit log; undo and redo put the stored records back (byte-exact unedited file after undo); the new record's saved nID is NextVsoID, mapped to the engine's random one"
    requirement: "D-03"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Roads and TestM2RoadEdits (every edit undone saves the unedited bytes; 8 edits)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 road round trip ok (BkEditorVsoMatchesEngine after add, undo, redo, undo)"
        status: pass
    human_judgment: false
  - id: D3
    description: "One Roads & Rivers tool (key 4): click adds, right-click removes the last point, double-click/Enter/Space finishes, Esc cancels; on a selected line: control-point drags in single/multi/all modes, width handles, 100 px opacity right-drag, Insert, Delete point (never below 2) or the whole line, right-press cycle; one undo step per finished line, drag and key"
    requirement: "D-08"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_vso.zig (14 tests, one per behaviour line); RED run zig-out/local-test/04-05-t3-red.log (7 failing), GREEN 115/115"
        status: pass
      - kind: unit
        ref: "zig build test-map-editor-view: view.zig 'key 4 is Roads & Rivers ...' and 'Ctrl+left is the right button in Roads & Rivers'"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: road by clicks + dblclick, point drag undone, INSERT (vso_points:road:4) undone, river by clicks + ENTER, opacity right-drag, BK_EDITOR_AUTO: done (59 actions)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The engine redraws every change (Remove + Add); a river change updates AI passability (DeleteRiver with the record as locked, AddRiver with the new one, undo and redo too); roads never touch the AI"
    requirement: "D-09"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Rivers (editor-bridge: M2 rivers and passability ok) - probe 10.5-cm_Flak38 flips on add/undo/redo/delete/undo; a road leaves it; arnheim's river point 4 unblocked by delete, blocked again by undo"
        status: pass
    human_judgment: false
  - id: D5
    description: "Refusals change nothing: a line that samples to fewer than 2 points ('too short'), an unknown type, a point off the map, bad widths/modes/keys, a delete below 2 points"
    requirement: "D-02"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Roads refusal block (saves the unedited bytes) and TestM2RoadEdits bad arguments"
        status: pass
      - kind: unit
        ref: "zig build test-editor-core: 'a too-short road is refused and records nothing ...', 'an unknown type or a point off the map is refused ...'"
        status: pass
    human_judgment: false
  - id: D6
    description: "The game reads it: a map with a new road and a new river test-launches and the game's trace reports one more of each"
    requirement: "D-09"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'game reads it M2 PASS (... roads 3 -> 4, rivers 0 -> 1)'"
        status: pass
    human_judgment: false
  - id: D7
    description: "Road and river markers (centre lines of all; for the selected one its centre line, control squares, width handles; the unfinished line) and the Roads & Rivers panel"
    requirement: "D-06"
    verification:
      - kind: other
        ref: "zig-out/local-test/04-05-m2_roads.png (from map-editor-auto-m2's m2_roads.tga), inspected: the new rail road stripe with its tan centre line, the new river as dark water with its green centre line, three control squares and six width-handle circles joined across it, the existing road's centre line along its tracks, the panel with River selected, width 3, opacity 100, 'river 0: ID 19367, 3 points'"
        status: pass
    human_judgment: true
    rationale: "Whether the markers read well over the terrain is a visual judgment; the markers sit on the z = 0 plane (Pitfall 17), so on sloped ground they sit a little off the drawn stripe"

duration: 53min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 05: Roads and rivers Summary

**Roads and rivers are editable at MFC parity: drawn and edited with the MFC tool's own CVSOBuilder calls, derived once per edit and kept in a new bridge-side edit log, kept in step with the engine and (for rivers) the AI's passability, with a Roads & Rivers panel, markers, and proofs on every tier up to the real game reading one more road and one more river.**

## Performance

- **Duration:** about 53 min (2026-09-29T23:38Z to 2026-09-30T00:32Z)
- **Tasks:** 4 (one tracer, two auto, one TDD auto)
- **Files:** 24 changed (2 created), 4589 insertions, 15 deletions

## Accomplishments

- **The edit log** (`session.h`, `session_vso.cpp`): `IEditRecord` with `Revert`/`Reapply`, `edits`, `appliedEdits`, `undoneEdits`, cleared on open and close; `BkEditorUndoEdit`/`BkEditorRedoEdit` with the paints' order rule. The core's `.edit` command holds a gesture's tokens (`scope` vso or objects; objects reloads the document for the later compound edits).
- **Roads and rivers in the bridge** (`session_vso.cpp`): `BuildVso` (CreateVSO, the two "too short" checks of Pitfall 1, Update with DEFAULT_STEP, UpdateZ, Pitfall 2's passability fix, NextVsoID), one `PutVso` for do/undo/redo (both copies, then river `DeleteRiver(before)`, terrain Remove and Add by the mapped engine ID, river `AddRiver(after)`), edits of existing records through `ReplaceVsoInSession` (move, width, opacity, insert and point delete in the MFC order around the key-point backup), whole-record delete, `PickVsoInSession` (TerrainHitTest, roads then rivers, cycle), descriptors from the data storage, `VsoMatchesEngine`.
- **C ABI**: 14 entries, all `Guarded`, all in `TestEntryPointsBeforeAMap`'s two tables (null session and no map).
- **Core**: `Editor.addVso/deleteVso/moveVsoPoints/setVsoWidth/setVsoOpacity/insertVsoPoint/deleteVsoPoint/pickVso/readVso/vsoCount/vsoDescriptors`, `vso_generation`; the fake models roads and rivers with the same stack and refusal rules; `tools_vso.RoadsRivers` with the add and edit states; `Pointer.screen_x/screen_y` filled by `Editor.resolve`.
- **App**: registry entry `roads_rivers` on key 4 (right button, Ctrl-as-right, double click, roads_rivers markers); the view routes plain motion to the tool's `hover`; the Roads & Rivers panel replaces the object palette while the tool is active; markers; commands and predicates; the M2 auto segment and the game-reads-it edit.

## Task Commits

1. **Task 1: Draw a road end to end (tracer)** - `b633592ff` (feat)
2. **Task 2: Rivers with AI passability, whole delete, the game reads them** - `8972f7e74` (feat)
3. **Task 3: Editing a selected road or river (TDD)** - `fcfdd3a88` (feat)
4. **Task 4: Panel, markers, scripted scenario** - `807be9dd1` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; it was re-run end to end (hermeticity, core, panels, map-file, then bridge, engine and view) and passed before Task 2 began. The one failure on the way was a view test that pressed key 4 expecting no tool; see Deviation 1.

## TDD Gate Compliance (Task 3)

- RED: the seven behaviour tests of the selected-line state were written against a stub `hover` and the Task 2 tool; `zig build test-editor-core` answered 108 pass, 7 fail (`zig-out/local-test/04-05-t3-red.log`).
- GREEN: the edit state implemented; 115/115 (`zig-out/local-test/04-05-t3-green.log`). Two Task 1 tests changed with it (Deviation 2).
- The bridge, fake and adapter calls the tool needs were written before the RED run, since the tests drive the tool through them; they are one commit with the tool (the executor rules ask for one commit per task).

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core`, `-view`, `-panels`, `-testlaunch`, `-auto`, `test-map-files` | 48/48 steps, 364/364 tests; `map-file: M2 vso builder ok`, `map-file: PASS`, sweep 66/66 |
| `test-editor-bridge` | `M2 roads ok`, `M2 rivers and passability ok`, `M2 road edits ok (8 edits)`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 road round trip ok`, `PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto-m2` (runs smoke and M1 auto first) | two `BK_EDITOR_AUTO: done`, no failed expectation; M1 compare 0.52 % of pixels (the new tool button) |
| `map-editor-game-reads-it-m2` (runs M1 first) | M1 PASS; `game reads it M2 PASS (... roads 3 -> 4, rivers 0 -> 1)` |

Logs are under `zig-out/local-test/04-05-*.log`. Windows and CI were not run (the plan does not ask for them).

## Decisions Made

See `key-decisions`. Parity deviations recorded by the plan itself: Insert and Delete act on the hovered control point, else the last grabbed one (research Q7); Esc cancels an unfinished line (an addition).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in an existing test] The view test pressed key 4 expecting no tool switch**
- **Found during:** Task 1 verify (`test-map-editor-view`)
- **Issue:** 04-03's "the right button, a double click and the new keys reach no gesture ..." sent SDLK_4 as a key no tool owned; 4 is now Roads & Rivers (the plan's shortcut).
- **Fix:** the test sends 8 and 9; two new view tests cover key 4, the road gestures through the view and Ctrl+left as the right button.
- **Committed in:** `b633592ff`

**2. [Rule 1 - Test design] Task 1's add tests assumed a finished line leaves nothing selected**
- **Found during:** Task 3 GREEN
- **Issue:** with the edit state, a finished line stays selected (MFC's STATE_EDIT) and a press on empty ground deselects it before the next press starts a line.
- **Fix:** the two tests press once more (or Esc) before drawing the second line; all tools_vso tests now `deinit` the tool (it caches the selected line).
- **Committed in:** `fcfdd3a88`

**3. [Rule 3 - Blocking] BK_EDITOR_AUTO drags across frames were ended by the stale-gesture guard**
- **Found during:** Task 4 (`expect=undo_depth:5 was false` after the scripted point drag)
- **Issue:** a scripted button is only an event; at the frame's end `View.update` reads the real mouse (no button held) and ends the gesture, so later `drag=` entries reached a tool with nothing in hand. (M1's brush drag had the same silent limit.)
- **Fix:** the M2 schedule puts each drag's press, motions and release in one frame (the runner runs every entry of a frame before the events are polled); a comment in build.zig says why.
- **Committed in:** `807be9dd1`

### Plan wording resolved

- The registry label is "Roads & Rivers" (the menu and palette text, like "Select"); `tool=roads_rivers` uses the ToolId name as 04-03 set up.
- `Editor.addVso` returns the index the line landed at (the plan wrote `void`); the tool needs it for its selection.
- `BkEditorVsoInfo.desc` is the full saved name (season folder, Roads3D\ or Rivers\, name); `BkEditorAddVso` takes the bare name the descriptor list gives.
- The auto segment adds `do=vso_width:3`, `do=vso_opacity:100` and `expect=undo_depth` checks, and no key=ESCAPE is needed before the river because switching the kind deselects.

---

**Total deviations:** 3 auto-fixed (2 Rule 1, 1 Rule 3) plus wording resolutions
**Impact on plan:** no scope change.

## Issues Encountered

- Markers are drawn on the z = 0 plane (Pitfall 17): on coldwinter the existing road's centre line runs a little beside its wheel tracks where the ground slopes. Accepted as for M1's objects.
- The panel's width and opacity apply to new lines and to the drags; unlike MFC's `CVSOState::Update` in CW_ALL mode, moving the slider does not re-width the selected line (not in the plan's list; a drag with mode All does it).

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- The edit log is ready for bridges, fences and entrenchments: a session function stores its own `IEditRecord`, `LogEdit` hands out the token, and the core records it with `prepareEdit`/`commitEdit` under the `objects` scope (which reloads the document's objects on undo and redo).
- A scripted drag must stay within one frame (see Deviation 3).

## Threat surface

No new surface beyond the plan's model: fewer than 2 control or sampled points never reach a put (T-04-05-01, map-file and engine refusals); counts, finiteness, ranges and indexes are checked at the ABI before the map is touched, and every refusal is shown to change nothing (T-04-05-02); new roads get passability 1 (T-04-05-03, map-file read-back test); markers draw detail only for the selected line and cap the others at 256 per kind (T-04-05-04).

## Self-Check: PASSED

- Created files found: `session_vso.cpp`, `tools_vso.zig`.
- Commits found: `b633592ff`, `8972f7e74`, `fcfdd3a88`, `807be9dd1`.
- Acceptance greps re-run: CVSOBuilder calls in session_vso.cpp 4 (at least 3); `BkEditorUndoEdit|RedoEdit|AddVso` in bridge.h 7; `tools_vso.zig` in root.zig 1; `std::min|std::max` in session_vso.cpp 0; `DeleteRiver|AddRiver` in session_vso.cpp 5, none in the road branch; the six Task 3 entry points in bridge.h 11; `drawRoadsRivers` in panels_m2.zig 1 and panels.zig 1; `vso_delta` in commands.zig 2.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

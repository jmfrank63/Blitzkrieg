---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 07
subsystem: map-editor
tags: [map-editor, m2, fences, map-geometry, edit-log, compound-edit, ai-tile, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: WhyNotPlacedByPalette (the palette refuses loose fences), SAddObject with nFrameIndex/fHP/nScriptID, the marker layer
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 05
    provides: the edit log (IEditRecord, LogEdit, BkEditorUndoEdit/RedoEdit), the core .edit command with scope objects, reloadObjects
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 06
    provides: NMapGeometry (plain numbers in and out), SGroupEdit with all-or-nothing AddGroup/RemoveGroup, tools_groups.zig, the ghost pattern, held scripted drags
provides:
  - NMapGeometry::RasterizeLine (a_dirLine port) and PlanFences, SFencePlanInput
  - session_groups.cpp - SGroupEdit for a group with no bridges entry; DrawFencesInSession, PlanFencesInSession, FencePlanInputFor, FenceDescriptorsInSession; WorldToAITile
  - C ABI BkEditorWorldToAITile, BkEditorFenceDescriptors, BkEditorPlanFences, BkEditorDrawFences
  - core Editor.drawFences/planFences/fenceDescriptors, tools.Pointer.ctrl, tools_groups.FenceTool (key 6), fake fences on the shared edit log
  - the Fences panel with pictures, the fence ghost, command fence_desc and predicate fence_delta, the M2 scenario segment, the game loading a fence run
affects: [04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 2
plan_head_before: 52b38f9a6f0858e690e5350a0c7f25c878af650c
actuals:
  tokens: 33660
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "A fence run is a group with no bridges entry: SBridgeGroup.bEntry false makes RemoveGroup/AddGroup skip the entry checks and the entry insert and erase, so the run is one SGroupEdit with the same all-or-nothing, descending-place removal and ascending restore as a bridge"
    - "The snapshot record holds the packed type ( 1 << dir ) | FENCE_TYPE_NORMAL; the working copy and the engine hold the stats' seeded first centre segment of that direction (seed 0), the same segment the plan's origin was taken from, so the fitted position and the sprite agree"
    - "A plan's expected map on the engine tier starts its link IDs at the session's floor: an ID an undone edit held is never handed out again, so a later run's expected IDs are not the file's NextLinkID"
    - "A scripted predicate that is false says what it measured (fence_delta is 6, not 5), so a wrong expectation is one run"

key-files:
  created: []
  modified:
    - Sources/src/MapFile/MapGeometry.h
    - Sources/src/MapFile/MapGeometry.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/session_groups.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/tools.zig
    - Sources/editor/core/tools_groups.zig
    - Sources/editor/app/c_bridge.zig
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
  - "The tile mapping is the engine's ITerrainEditor::GetAITileIndex (BkEditorWorldToAITile), as the MFC tool calls it. The plan and research Q6 said it is 'proven equal' to the static CMapInfo::GetAITileIndices; it is not: the engine rounds half a cell (int( d / cell + 0.5 )), the static helper truncates. They agree at tile corners only. The engine tier asserts both facts (five corners equal; 5.7 tiles is 6 on the engine and 5 on the static helper) rather than the plan's blanket equality"
  - "A single click places the ghost's fence (direction 0, 1 with Ctrl), as the plan says. The MFC commit path would place direction 3 for a click (a zero delta is 'horizontal' in GetCurrentDirection, and the first x is not smaller than the last), which is not what its ghost shows; the ghost is what the user sees, so the ghost is the rule"
  - "The axis is the longer AI-tile delta and the last tile is locked to the first's other coordinate (the MFC locked the world point, which gives the same tile); a tile-equal drag is one fence, so a click is passed the press point for both ends and stays on its tile whatever the pixel jitter of the release"
  - "PlanFences refuses a planned position outside the map as well as the two end cells: a rightward run's last fence is moved two tiles on (+2) and an upward one two up, which can leave the map with both ends on it. The MFC's own box only tested the ends (and its commit test was written with a wrong && that refused nothing); the engine would have refused the fence, so the plan refuses the run before it starts"
  - "A fence's per-direction origin is the origin of the stats' first centre segment (seed 0), and the working copy uses that same segment for every fence of the direction: deterministic, and the position the plan fitted is the position of the sprite it shows (the file holds only the type, C6). The game picks its own variants when it loads"
  - "A fence type whose stats have fewer than four directions or a direction with no centre segment is refused before any seeded index helper runs (T-04-07-01); all 21 shipped fence types plan a click"
  - "The fence ghost is drawn as one bar per fence, three AI tiles long along the fence's direction and centred on the planned position, on the z = 0 plane (Pitfall 17), not the segment's real footprint (the origin offset is not in the plan): it reads as the run's line and lands on the sprites' bases, measured on the shots"
  - "Ctrl reaches the tool on the pointer (Pointer.ctrl, stamped in View.dispatch from the modifier state) and the registry entry keeps ctrl_click_is_right off, so in the Fence tool Ctrl+left is a left click with the flag set, never a right click (C13). BK_EDITOR_AUTO has no modifier action, so the Ctrl flip is proven on the core, map-file and engine tiers and not in the scripted segment"
  - "The fake's fence run is capped at 32 fences (its group arrays are the bridge's), with the AI tile as half its 32-unit tile"

patterns-established:
  - "Engine-tier expected maps for a fence run: the run planned with PlanFences on inputs read straight from the stats (dirs[d].centers[0]) and tiles from BkEditorWorldToAITile, then NMapOverlay::AddObject per fence with the packed type and link IDs from the session's floor"
  - "Keeping a game shot: the edited game's autoshot is copied beside the report before the sweep deletes it"

requirements-completed: [D-14, D-03, D-05]

coverage:
  - id: D1
    description: "D-14: the Fence tool (key 6) lists fences with pictures and a ghost; a drag along one axis places one fence every second AI tile with the direction in the frame index ( 1 << dir ) | FENCE_TYPE_NORMAL (right 3 with +2, left 1, up 0 with -2, down 2); a click places one fence (direction 0, 1 with Ctrl); one undo step per drag"
    requirement: "D-14"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_groups.zig 'a horizontal fence drag over 10 tiles places 5 fences as one undo step', 'the four drag directions give the directions 3, 1, 0 and 2', 'a click places one fence, direction 0, and direction 1 with Ctrl'"
        status: pass
      - kind: unit
        ref: "zig build test-map-files: TestM2FencePlan (map-file: M2 fence plan ok) - RasterizeLine by hand, the four directions with their shifts, ties, 5 and 6 fences over 10 and 11 tiles, single fence with and without ctrl, one fence spelled out (400, 656)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Fences (editor-bridge: M2 fences ok) - a 12-fence run over coldwinter equals PlanFences on the stats' inputs and the tiles of BkEditorWorldToAITile; leftward, downward, upward, single and flipped runs each saved as expected and undone to the unedited bytes"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-14: the tile mapping is the engine's ITerrainEditor::GetAITileIndex, exposed as BkEditorWorldToAITile, compared with the static CMapInfo::GetAITileIndices (research Q6)"
    requirement: "D-14"
    verification:
      - kind: integration
        ref: "TestM2Fences: BkEditorWorldToAITile equals CMapInfo::GetAITileIndices at five tile corners (the four map corners and the centre); the engine rounds 5.7 tiles to 6 where the static helper truncates to 5, so the two are NOT equal between corners (the plan's 'proven equal' is corrected); MapGeometry's AI tile size and normal fence type equal SAIConsts::TILE_SIZE and FENCE_TYPE_NORMAL"
        status: pass
    human_judgment: false
  - id: D3
    description: "C13: Ctrl in the Fence tool is a modifier that flips a single fence, never a right click"
    requirement: "D-14"
    verification:
      - kind: unit
        ref: "tool_registry.zig 'the Fence tool is key 6, left button only, and Ctrl is never a right click there'; tools_groups.zig click test with Pointer.ctrl set on press and release places a fence and no right event exists for the tool"
        status: pass
      - kind: integration
        ref: "TestM2Fences: BkEditorPlanFences with ctrl 1 plans type 2 | 0x10000; a single flipped fence is drawn, saved as expected and undone to the unedited bytes"
        status: pass
    human_judgment: false
  - id: D4
    description: "A run with any tile outside the map is refused as a whole and changes nothing"
    requirement: "D-14"
    verification:
      - kind: unit
        ref: "TestM2FencePlan: end past the map, -1, 128 of 128, a rightward run whose last fence is moved past the edge, an upward run moved past the top, an empty extent and a NaN origin are all refused with nothing planned; core 'a run with an end off the map is refused whole and records nothing'"
        status: pass
      - kind: integration
        ref: "TestM2Fences refusals: past the edge ('the fence run leaves the map'), a rightward run ending on the last tile, an unknown type, a bridge type, a non-fence type, NaN, null type; no token, no object added, world matches the map, the save is the unedited bytes"
        status: pass
    human_judgment: false
  - id: D5
    description: "Placed fences are ordinary objects: the Select tool moves and deletes them with M1's commands"
    requirement: "D-14"
    verification:
      - kind: integration
        ref: "TestM2Fences: BkEditorMoveObject moves one new fence (saved where moved, packed type kept) and BkEditorDeleteObject deletes another (gone from the saved map); moving back and BkEditorRestoreObject put the saved map equal to the run again; the run's undo then saves the unedited bytes and its redo the expected map"
        status: pass
      - kind: unit
        ref: "tools_groups.zig 'undo takes the run out, redo puts it back, and a fence is an ordinary object' - Editor.delete of one fence, its undo, then the run's undo and redo"
        status: pass
    human_judgment: false
  - id: D6
    description: "D-05 / T-04-07-01: a fence type with fewer than four directions or a direction with no centre segment is refused before any seeded index helper runs"
    requirement: "D-05"
    verification:
      - kind: integration
        ref: "TestM2Fences: a click of each of the 21 shipped fence types plans one fence (21 planned, 0 refused); FenceCentreIndex and FencePlanInputFor guard the list sizes before GetCenterIndex (no shipped type is empty, so the refusal path is by construction)"
        status: pass
    human_judgment: false
  - id: D7
    description: "The game reads it: the M2 map with a fence run beside the start camera loads and the game exits cleanly, with the roads, rivers and bridges still read"
    requirement: "D-14"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'game reads it M2 PASS (camera at player 0's anchor 2172,2172; ... roads 3 -> 4, rivers 0 -> 1, bridges 0 -> 2, fences +10)'; the edited shot kept as zig-out/local-test/map-editor-game-reads-it-m2.log.edited.rgba"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-07-game-edited.png (a crop of the kept shot), inspected: the brick fence run lies across the snow beside the start camera's units, posts and panels drawn as the game draws a fence"
        status: pass
    human_judgment: true
    rationale: "Whether the placed run reads as a fence is a visual judgment; the trace has no fence count, so the automated part is only that the map loads and the game exits 0"
  - id: D8
    description: "D-06: the Fences panel with pictures, the drag ghost, and the scripted M2 segment with one undo step, undo and redo"
    requirement: "D-14"
    verification:
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=fence, fence_desc:W_FactoryFence, a drag over five frames with the ghost shot held, fence_delta:6, undo_depth:10, undo, fence_delta:0, redo, fence_delta:6, BK_EDITOR_AUTO: done (97 actions)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-07-m2_fences.png (from m2_fences.tga), inspected: six brick fences run along the drag beside the bridge, the Fences panel lists A_Fence, E_Fence_Bush, E_FieldFence and more with pictures, the placing note, the Fence button lit; 04-07-m2_fence_ghost.png shows the green ghost line along the same run while the button is held"
        status: pass
    human_judgment: true
    rationale: "Panel layout and ghost legibility are visual judgments"

duration: 28min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 07: Fences Summary

**Fences are placed as the MFC Fences tab places them - an axis-locked drag becomes one fence every second AI tile with its direction in the packed frame type, as one all-or-nothing undo step through the shared edit log - with the geometry in one plain-number function the bridge and the map-file tier share, and the real game loading a map with a fence run beside its start camera.**

## Performance

- **Duration:** about 28 min (2026-09-30T01:26Z to 01:54Z)
- **Tasks:** 2 (one tracer, one auto)
- **Files:** 23 changed, 2073 insertions, 27 deletions

## Accomplishments

- **MapGeometry** (`Sources/src/MapFile/MapGeometry.{h,cpp}`): `RasterizeLine` (a_dirLine, unchanged), `PlanFences` (axis lock, one fence every second tile, direction and its shift, single fence, whole-run refusal, the commit's own AI2Vis / FitVisOrigin2AIGrid / Vis2AI chain), `SFencePlanInput`. No engine or object-database header.
- **The fence side** (`session_groups.cpp`): `SBridgeGroup.bEntry` lets the one `SGroupEdit` carry a group with no entry, so a run is one edit with the bridge's ordering guarantees; `FencePlanInputFor` reads the four centre origins with the list-size guards; `DrawFencesInSession` builds the snapshot's packed type and the working copy's seeded segment; `WorldToAITile` in `session.cpp`.
- **C ABI**: four entries (`BkEditorWorldToAITile`, `BkEditorFenceDescriptors`, `BkEditorPlanFences`, `BkEditorDrawFences`), all `Guarded`, all in both status tables of `TestEntryPointsBeforeAMap`.
- **Core**: `Editor.drawFences` as one `.edit` entry under scope `objects`, `Editor.planFences`/`fenceDescriptors`, `Pointer.ctrl`, `FenceTool` (press, drag, release; a click is one fence; Escape gives the drag up), the fake's fence model on the one edit log.
- **App**: registry entry `fence` on key 6, the Fences panel, the ghost, `fence_desc` and `fence_delta`, the M2 auto segment and the game-reads-it fence run with its kept shot.

## Task Commits

1. **Task 1: Draw a fence run end to end (tracer)** - `c9d50629c` (feat)
2. **Task 2: Fences panel, ghost, scripted and game scenarios** - `3cd3df50a` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; it was re-run end to end (core, panels, view, view-events, auto parser, map-file tiers: 45/45 steps, 366/366 tests; `map-file: M2 fence plan ok`; the engine tier `editor-bridge: M2 fences ok`, `editor-bridge: PASS`) and passed before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-view-events test-map-editor-auto test-map-files` | 45/45 steps, 366/366 tests; `map-file: M2 fence plan ok`, `map-file: PASS` |
| `test-editor-bridge` | `editor-bridge: M2 fences ok`, `editor-bridge: PASS` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto` (M1, run first by the M2 step) | `BK_EDITOR_AUTO: done (13 actions)`, compare 0.1603 % (see Deviations 5) |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (97 actions)`, no failed expectation |
| `map-editor-game-reads-it-m2` (runs M1 first) | M1 PASS; `game reads it M2 PASS (... roads 3 -> 4, rivers 0 -> 1, bridges 0 -> 2, fences +10)` |

Logs are under `zig-out/local-test/04-07-*.log`. Windows and CI were not run (the plan does not ask for them).

## Deviations from Plan

### Plan wording resolved

**1. [Plan claim not true] The engine's AI tile mapping is not equal to CMapInfo::GetAITileIndices**
- **Found during:** Task 1, reading `TerrainEditor.cpp` against `MapInfo_StaticMethods.cpp` before writing the engine test.
- **Issue:** the plan (must_haves, research Q6) says the mapping is "proven equal to the static CMapInfo::GetAITileIndices on the engine tier". The engine's `GetTileIndexLocal` rounds (`int( d / cell + 0.5 )`); the static helper truncates (and treats negatives differently). They agree only at tile corners.
- **Fix:** the tool uses the engine's mapping, as the MFC tool does. `TestM2Fences` asserts the true statement - equal at the five corner points, different at 5.7 tiles (6 against 5) - so the difference is a measured fact, not a surprise later.
- **Committed in:** `c9d50629c`

**2. [Plan and MFC differ] A click**
- The plan says a click is one fence, direction 0 (1 with Ctrl). The MFC commit path would give direction 3 for a zero-length drag. The plan follows the MFC ghost, which is what the user sees, so it is followed.

### Auto-fixed Issues

**3. [Rule 2 - Missing functionality] A planned fence could leave the map with both ends on it**
- **Found during:** Task 1 (writing PlanFences against the +2 / -2 shifts).
- **Issue:** the MFC refusal box tests the end tiles only, but the last fence of a rightward run is moved two tiles on and the first of an upward run two tiles up, past the edge, where the engine would refuse it and the run would half-exist in a naive implementation.
- **Fix:** `PlanFences` also refuses a planned position outside the map's AI units, before any object is made; both cases are in `TestM2FencePlan` and `TestM2Fences`.
- **Committed in:** `c9d50629c`

**4. [Rule 2 - Diagnostics] A false `fence_delta` said nothing**
- **Found during:** Task 2 (the first scripted drag covered 6 fences, not the guessed 5).
- **Fix:** the predicate sets the status line to `fence_delta is 6, not 5`, which the runner prints with a false predicate, so a wrong expectation costs one run.
- **Committed in:** `3cd3df50a`

### Not a plan change

**5. M1 auto compare 0.1603 %** (was 0.0054 % after 04-06's refresh): the sixth tool button ("Fence") sits on the second palette row beside the Bridge button, a few pixels the reference does not have. It is inside the runner's threshold and the pass stands; the local reference was not refreshed.

**6. Link IDs in the engine test's expected maps**: after the first run is undone, the session's floor keeps its IDs unused, so the later expected runs take their IDs from the floor (`LayFences( ..., nFloor )`), not from the file's next ID. This is the bridge's intended behaviour (M1's delete-restore hazard), not a defect.

**7. The Ctrl flip is not in the scripted segment**: BK_EDITOR_AUTO has no modifier action; the flip is proven on the core, map-file and engine tiers.

---

**Total deviations:** 2 auto-fixed (both Rule 2) plus 2 plan-wording resolutions
**Impact on plan:** no scope change; one plan claim (tile equality) corrected with a measured fact.

## Issues Encountered

- `zig build` reported "file exists in modules 'sdl3' and 'root'" when markers.zig imported `sdl3.zig` as a file; the app's files import the module `sdl3`. Fixed before the first commit that carried it.
- The ghost bars are drawn on the z = 0 plane, so on raised ground they sit a little below the sprites' bases (the bridge outline had the same offset, 04-06).

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- `SBridgeGroup.bEntry` is ready for entrenchments (04-08): a trench is an entry of sections plus pieces, the same `SGroupEdit` with an entry.
- `Pointer.ctrl` is on every tool event for any later modifier gesture.
- The kept shot `zig-out/local-test/map-editor-game-reads-it-m2.log.edited.rgba` (1440x900 RGBA, the game's own frame with the fence run) is for 04-13.

## Threat surface

No new surface beyond the plan's model: a fence type with empty or missing direction lists is refused before any seeded index helper (T-04-07-01); an off-map run, including one whose moved fence would leave it, is refused whole in the plan and again by the engine, both shown to change nothing on the map-file and engine tiers (T-04-07-02); names are length-checked (64), coordinates finite-checked at the ABI.

## Self-Check: PASSED

- Modified files found: `MapGeometry.h`, `MapGeometry.cpp`, `session_groups.cpp`, `bridge.h`, `tools_groups.zig`, `panels_m2.zig`, `commands.zig`, `markers.zig` (all present).
- Commits found: `c9d50629c`, `3cd3df50a`.
- Acceptance greps: `PlanFences|RasterizeLine` in MapGeometry.h 3; `BkEditorWorldToAITile|BkEditorDrawFences|BkEditorPlanFences` in bridge.h 4; `FenceTool` in tools_groups.zig 9 (type and tests); `drawFences` in panels_m2.zig and panels.zig 1 each; `fence_delta` in commands.zig 3; `std::min|std::max` added by the diff 0.
- The executor looked at `04-07-m2_fences.png` and records that the six-fence run is visible.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 02
subsystem: map-editor
tags: [map-editor, m3, heights, update-map, fill, fit-to-grid, instant-update, tile-properties, parity, m3-auto, zig, cpp]

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 01
    provides: the D-19 altitude region primitive and its token discipline, ApplyAltitudesInSession, the map-editor-m3-auto step, the new-map/save chain, the 2x2 default brush
provides:
  - ApplyHeightsStroke/GenerateHeights/SetZeroHeights and the heights stroke machine (session_terrain.cpp) with the four level modes, the profile pattern and the IsValidHeight rollback
  - UpdateMapInSession (the OnButtonUpdate composite as one edit: engine height/terrain updates, full crosses and shades, the objects-Z refresh, the fit pass, progress over the MFC's own count) and SUpdateMapEdit's raw whole-composite undo (altitudes, tiles, moves and VSO z captured)
  - FillEntireMapInSession (one paint of the log; the MFC's update-rect typo not copied) and SetTerrainModesInSession (instant_update/fit_to_grid defaults off/on)
  - BkEditorSnapToGrid - the placement rule answered as a question, so the Placer and the Selector's drag snap before they edit and undo replays raw
  - BkEditorTile.variant_count (tile 0 included; the MFC's > 0 guard not copied) and the palette's tile-properties context menu
  - core vtable updateMap/fillEntireMap/setTerrainModes/snapToGrid (+ fake and c_bridge in the same commits), Editor.updateMap/fillEntireMap/setTerrainModes/snapToGrid, settings keys instant_update/fit_to_grid
  - commands map_update/map_fill/instant_update/fit_grid/tile_info; the Map menu (Update Map Ctrl+U, Fill confirmation, the two checks) and the update report modal
  - TestM3UpdateMapAndFill, TestM3FillRegion, TestM3TileInfo; the m3 scenario's heights/update/fill/toggles/tile-info frames
  - PARITY rows M1, M8, M9, M10, TR2, TR4-TR12 closed; m1/m2 screenshot references re-seeded (the M3 chrome postdates them)
affects: [05-03, 05-04, 05-05, 05-06, 05-07, 05-08, 05-09, 05-10, 05-11]

commits: 3
plan_head_before: 70edd4420
actuals:
  tokens: 92000
  tasks: 3
  commits: 3
  files: 25

tech-stack:
  added: []
  patterns:
    - "A composite update is one edit of the log whose record captures every region, position and VSO list it will touch, before and after; undo/redo put them back raw - nothing is re-derived on the way back"
    - "The fit answers as a question (BkEditorSnapToGrid): the tools snap before they edit, so the map never holds a position the caller did not mean and undo never re-fits"
    - "A selection click's stray motion is told from a drag by asking whether the raw pose equals the object's own before fitting"
    - "Crosses are compared by content (element runs), never memcmp - a patch holds its crosses in std::vectors whose pointers the two builds do not share"

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/session.h, session.cpp, session_terrain.cpp, bridge.h, bridge.cpp
    - Sources/editor/core/bridge.zig, editor.zig, fake_bridge.zig, settings.zig, tools.zig
    - Sources/editor/app/c_bridge.zig, commands.zig, panels.zig, panels_m3.zig
    - tools/zig/editor_bridge_test.cpp, tools/zig/map_file_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md

key-decisions:
  - "A3, measured: IAIEditor IS live in the bridge session, so Update Map calls the MFC's own UpdateAllHeights/UpdateTerrain; only the objects-Z pass is implemented bridge-side over CVSOBuilder::UpdateZ, because roads/rivers/sounds are map data, not engine state"
  - "The fit lives where the questions live: BkEditorSnapToGrid answers for buildings and generic objects only - the frame-less origin the MFC's placement rule asks for is the base stats' own, and the segment-based kinds' overrides read beside it (the MFC got away with it in release; this bridge guards)"
  - "Update Map's undo captures the VSO z whole instead of re-deriving it: the bytes undo owes are the map's own, not what a pure function would produce again"
  - "Fill Entire Map is a paint of the log (the token names it for undoPaint/redoPaint), exactly what a whole-map brush paint is - and the expected value at both tiers"
  - "The m1/m2 screenshot references were re-seeded rather than tuned: the M3 chrome (status lines, title) postdates their capture, the compare tool seeds on first run, and the old references are kept beside them (04-13's own practice)"

requirements-completed: [D-18, D-20, D-22, D-35, D-40]

duration: ~6h wall (including the interrupted predecessor's in-flight work reconciled, the snap-as-a-question refactor, and the stale m1/m2 screenshot re-baseline)
completed: 2026-10-01T15:10:00Z

coverage:
  - deliverable: "Heights strokes raise/lower/level in all four modes with the profile pattern, one undo step per stroke, rollback unless Ctrl (D-18, TR4-TR8)"
    verification:
      - kind: test
        ref: "editor_bridge_test.cpp#TestM3Heights (editor-bridge: M3 heights ok)"
        status: pass
      - kind: test
        ref: "core tools_heights/tools tests (test-editor-core rc=0)"
        status: pass
    human_judgment: false
  - deliverable: "Generate heights (Hills/Rocks/Dunes) and Set Zero (TR10, TR12)"
    verification:
      - kind: test
        ref: "TestM3Heights: the MFC formula's bounds, both ends attained, byte-exact undo"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: do=heights_generate:hills:0.3:-3:3, expect=undo_depth:3"
        status: pass
    human_judgment: false
  - deliverable: "Update Map composite with progress and the fit pass (D-20, M8, M10)"
    verification:
      - kind: test
        ref: "TestM3UpdateMapAndFill: progress heard 7+snapped, the snap exactly FitVisOrigin2AIGrid's own, altitudes untouched, undo byte-exact"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: do=map_update, expect=undo_depth:4"
        status: pass
    human_judgment: false
  - deliverable: "Instant Update and Fit To Grid toggles, settings-backed (D-20, M9/M10)"
    verification:
      - kind: test
        ref: "settings.zig round-trip tests; BkEditorSetTerrainModes NO_SESSION/defaults in TestM3UpdateMapAndFill"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: do=instant_update, do=fit_grid, expect=undo_depth:5 (settings, never undo steps)"
        status: pass
    human_judgment: false
  - deliverable: "Fill Entire Map (D-22, M1)"
    verification:
      - kind: test
        ref: "TestM3FillRegion (map-file: M3 fill ok); TestM3UpdateMapAndFill's fill half"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: do=map_fill:0, expect=undo_depth:5"
        status: pass
    human_judgment: false
  - deliverable: "Tile properties for every index including 0 (D-35, TR2)"
    verification:
      - kind: test
        ref: "TestM3TileInfo (editor-bridge: M3 tile info ok)"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto: do=tile_info:0, expect=status:0"
        status: pass
    human_judgment: false
  - deliverable: "PARITY rows M1, M8, M9, M10, TR2, TR4-TR12 closed with evidence"
    verification:
      - kind: command
        ref: "05-PARITY.md Evidence cells filled (14 rows)"
        status: pass
    human_judgment: false
  - deliverable: "Preservation guard (D-40.8): heights/fill/update edits followed by undo write byte-identical files"
    verification:
      - kind: command
        ref: "aggregate zig build test rc=0; map-file sweep 66/66; byte-exact undo asserted inside all three new tests"
        status: pass
    human_judgment: false
  - deliverable: "Windows leg (D-37)"
    verification:
      - kind: command
        ref: "pending-push: the branch carries this plan's commits; the orchestrator pushes and the win-home non-GUI tiers run there (phase-4 precedent, no GUI over SSH)"
        status: pending
    human_judgment: true
    rationale: "The win-home leg needs the branch on origin, which the orchestrator pushes after the wave; the non-GUI tiers ran green on macOS arm64 and the tier is platform-plain C++/Zig with no macOS-only surface touched."

# Phase 05 Plan 02: Terrain heart — Heights tool, Update Map, Fill, tile properties — Summary

One-liner: the MFC terrain workflow end to end - the Heights tool's stroke machine
(four level modes, profile pattern, invalid-height rollback), the Update Map
composite and Fill Entire Map as single byte-exact-undo commands, Instant Update
and Fit To Grid as settings-backed toggles, and tile properties that answer for
tile 0 too.

## Accomplishments

- **Task 1 (committed by the interrupted run, verified):** `4fd96b3ba` - the
  Heights engine path (stroke machine, generate, set zero), the Heights tool and
  panel. Verified against the plan's acceptance criteria; all greps hold and
  `editor-bridge: M3 heights ok` prints inside the aggregate gate.
- **Task 2:** `9ea31348d` - Update Map as one edit of the log (engine height and
  terrain updates, full crosses and shades on both copies, the objects-Z refresh,
  the fit pass, progress over the MFC's own 7+snapped count, and an undo record
  that captures altitudes, tiles, every moved position and the VSO z whole);
  Fill Entire Map as one paint of the log; the Instant Update/Fit To Grid
  toggles; the Map menu, the fill confirmation and the update report modal; the
  `map_update`/`map_fill`/`instant_update`/`fit_grid` commands; settings keys.
- **Task 3:** `38bef047f` - tile properties (the describe surface's variant
  count, tile 0 included, the palette's context menu, the `tile_info` command,
  TestM3TileInfo); the m3 scenario's frames (heights raise/level drags, generate,
  update, fill, the toggles, tile info - each command one undo step); PARITY rows
  M1, M8, M9, M10, TR2, TR4-TR12 closed with evidence.

## Issues Encountered

None open. The interrupted predecessor's in-flight work was reconciled rather
than redone; everything it had written survived into Task 2's commit, completed.

## Deviations from Plan

- **[Rule 1 - in-flight compile breaks] The engine tier was the first to compile
  the in-flight session code.** Found during: Task 2's verify. Issue: six errors
  (a helper declared only in session.cpp, `SMapObjectInfo::link.nLinkID` written
  as `.nLinkID`, and `NGDB::GetRPGStats` called without its type parameter).
  Fix: the declaration moved to session.h, the member path corrected, the
  MFC's own `pObjectsDB->GetRPGStats(pDesc)` form used. Files: session.h,
  session_terrain.cpp, session.cpp. Verification: the engine tier compiles and
  passes. Commit: 9ea31348d.
- **[Rule 1 - unit bugs in the whole-map passes] `UpdateTerrainCrosses` takes
  PATCH coordinates.** Found during: the first map-file run (a SEGV in
  PreprocessMapSegment, diagnosed under lldb). Issue: the in-flight Update Map
  passed the full TILES rectangle, and the new map-file test passed tiles where
  patches are the contract - over a 96x96 map the wrong-unit rect walks ~9x past
  the tiles. Fix: both call sites pass the patches' full sheet. Files:
  session_terrain.cpp, map_file_test.cpp. Commit: 9ea31348d.
- **[Rule 1 - progress off by one] The composite emitted six fixed steps against
  a total of seven, and a snap the engine refuses skipped its object's step.**
  Found during: TestM3UpdateMapAndFill's progress check. Fix: the seventh (commit)
  step announced, and each fit step announced at the top of its look. Commit:
  9ea31348d.
- **[Rule 1 - undo must not re-fit] A session-side snap in the place route made
  raw undo impossible** (the restore's place re-snapped the captured position -
  the engine tier's byte-compare caught it). Fix: the placement rule became a
  question - BkEditorSnapToGrid - the Placer and the Selector's drag snap before
  they edit, the session's place route is raw again, and the smoke's five edits
  undo clean (the Selector's click-motion no-op needed its exact-pose guard
  restored). Commit: 9ea31348d (the snap) and 38bef047f (the guard).
- **[Rule 2 - recorded] The engine's own `stats[-1]` UB is guarded, not copied.**
  The MFC's placement rule asks `GetOrigin(-1)`, which the segment-based kinds
  answer by reading beside their vector (release builds got away with it). The
  bridge's fit answers only for the base-stats kinds (buildings, generic
  objects). Documented in bridge.h and the SUMMARY.
- **[Rule 3 - stale baselines] The m1/m2 scenario screenshot references
  predate 05-01's final chrome** (captured Sep 30 15:07/15:37, before the
  status-line/title commit at Oct 1 02:25 - the interrupted run never re-ran
  them). Fix: the references re-seeded (the compare tool seeds on first run);
  the stale ones kept beside them as `*-reference-before-m3-chrome`, 04-13's
  own practice. Commit: 38bef047f.
- **[Rule 1 - recorded, pre-existing] `zig build test-map-editor-engine`'s run
  step intermittently stages the test binary into an empty .zig-cache/tmp dir
  and fails the engine start** ("no engine modules loaded") - the same failure
  is visible in the Task 1 verify's own log; run from the staged install root,
  the tier passes (all 2 tests). Not chased further in this plan; noted for the
  win-home/CI leg where the harness controls the cwd.

**Total deviations:** 7 (5 auto-fixed code, 2 recorded context). **Impact:** the
plan's contract holds at every tier; the snap-as-a-question refactor is the one
architectural change, and it is the plan's own "callers snap" instruction made
literal after the session-side variant proved incompatible with raw undo.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None - the plan's threat register landed as designed (T-05-02-01's range
checks and rollback-refusal proven in TestM3Heights; T-05-02-02's raw undo and
byte-exact proofs in TestM3UpdateMapAndFill; T-05-02-04's tile bounds in
TestM3TileInfo and BkEditorFillEntireMap's BAD_ARGUMENT/REFUSED refusals).

## Self-Check: PASSED

All three task commits exist (4fd96b3ba, 9ea31348d, 38bef047f); the aggregate
`zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` is
rc=0 with `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)`,
`map-file: PASS` (66/66 sweep) and all three `BK_EDITOR_AUTO: done` lines;
TestM3Heights/TestM3UpdateMapAndFill/TestM3TileInfo/TestM3FillRegion's ok lines
print; PARITY rows M1, M8, M9, M10, TR2, TR4-TR12 carry evidence; the win-home
leg is pending-push (noted above).

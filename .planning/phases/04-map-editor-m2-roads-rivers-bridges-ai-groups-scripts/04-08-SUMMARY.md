---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 08
subsystem: map-editor
tags: [map-editor, m2, entrenchments, map-geometry, edit-log, compound-edit, property-tests, bk-editor-auto, editor-bridge, ci, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords Insert/EraseEntrenchment, WhyNotPlacedByPalette (the palette refuses loose trench pieces), the BK_MAP_TRACE entrenchments line
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 05
    provides: the edit log (IEditRecord, LogEdit, BkEditorUndoEdit/RedoEdit), the core .edit command with scope objects, reloadObjects
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 06
    provides: SGroupEdit with all-or-nothing RemoveGroup/AddGroup, BkEditorPickGroup kind 2, tools_groups.zig, held scripted drags
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 07
    provides: SBridgeGroup.bEntry, the ghost pattern, game_reads_m2.zig's edit chain
provides:
  - NMapGeometry::TrenchPath, PlanEntrenchment, STrenchPlanInput, STrenchPlan, ETrenchPieceType (the MFC trench builder, plain numbers)
  - session_groups.cpp - SGroupEdit carrying an entrenchments entry; EntrenchmentPlanInputFor, PlanEntrenchmentInSession, DrawEntrenchmentInSession, ReadSessionEntrenchments, DeleteEntrenchmentFromSession; CanTakeOutWhole refuses a group a unit is garrisoned in
  - C ABI BkEditorPlanEntrenchment, BkEditorDrawEntrenchment, BkEditorEntrenchmentInfo, BkEditorEntrenchments, BkEditorDeleteEntrenchment
  - core Editor.drawEntrenchment/planEntrenchment/entrenchments/deleteEntrenchment, entrenchments_generation; tools_groups.EntrenchmentTool (key 7); fake entrenchments on the shared edit log
  - the Entrenchments panel, the live preview and the hovered/selected outlines, commands trench_player/trench_delete, predicate trench_delta, the M2 scenario segment, the game loading a new trench
  - a green six-job CI run on feat/map-editor-m2 with 04-01..04-08 in (run 36663674380)
affects: [04-09, 04-10, 04-11, 04-12, 04-13]

commits: 4
plan_head_before: 845d936b8fdee31a0890a944088ed79b016a7645
actuals:
  tokens: 45600
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "An entrenchment is the bridge's SGroupEdit with bTrench: the entry is an SEntrenchmentInfo at nEntryIndex, erased before the pieces go and inserted after they are back; linkIDs are all pieces in build order, so BuildOneBridge places them unchanged"
    - "Fidelity without goldens: the port is pinned by properties over 500 fixed-seed LCG polylines (terminators, half-turn begin, midpoints, straight vs arc by length, global fireplace/line alternation, sections after arcs, exact cover, determinism) - identical counts on macOS, Linux x86/arm and Windows"
    - "A group delete checks everything that could refuse before it takes anything out (CanTakeOutWhole), so RemoveGroup never stops halfway"
    - "An engine-tier test that needs a shipped map with a property scans Data/Maps in sorted path order, so every platform takes the same map"

key-files:
  created: []
  modified:
    - Sources/src/MapFile/MapGeometry.h
    - Sources/src/MapFile/MapGeometry.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session_groups.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/tools_groups.zig
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
    - .github/workflows/cross-platform.yml

key-decisions:
  - "The builder's input is two lengths read from the \"Entrenchment\" stats (first line and first arc segment, GetVisAABBHalfSize().x * 2: 2 * 52 and 2 * 15 AI units times fAITileXCoeff, checked equal to the map-file tier's literals on the engine tier) plus the map's extent; the MFC read a rand() line index each call, but every shipped line segment is as long"
  - "PlanEntrenchment takes the clicks, not the MFC path: it replays OnLButtonDown per click (the path grows with straight runs cut into line pieces, arcs of 15 then 30 degrees the shorter way round, the extension after an arc), then the double click's commit. The preview plans the clicks plus the pointer as one more click - what a click there and a double click would commit - rather than MFC's own approximate preview"
  - "The two-argument fabs is hypot (plan); the cosine in GetLineAngle is held to [-1, 1] (a NaN guard); an arc that never closes (the MFC loop would spin) and a path over 2048 points are refused"
  - "Pieces are listed and linked in the MFC commit's order: begin terminator, end terminator, then one per path step; the sections are indices into that list"
  - "The engine places trench pieces anywhere, even off the map (CAIEditor::IsObjectInsideOfMap passes every SGVOGT_ENTRENCHMENT), so the plan refuses a piece whose centre the map does not hold ('the trench leaves the map') - an addition the MFC tool never needed because its clicks were on the terrain"
  - "An entrenchment a unit is garrisoned in (nLinkWith names a piece) is refused whole before anything is taken out; moving units out is M3's links (05-PARITY O18/O21). The check is in CanTakeOutWhole, so bridges get it too"
  - "With no polyline started, a press on a piece selects its entrenchment instead of starting a trench (D-11's group rule; the MFC tool had no selection); Delete removes the selected one, else the hovered one; the right button's press clears the path (MFC: its release)"
  - "The delete test's map is the first in sorted path order with an entrenchment that holds no units (Kharkov43, trench 3): battleofbulge, the first map with entrenchments, has all 15 garrisoned and carries the refusal test instead"
  - "The tools panel is 176 pixels high (was 150): the seventh button wraps to a third row and had pushed the brush radius under a scrollbar"

patterns-established:
  - "Engine-tier expected maps for a trench: PlanEntrenchment on inputs read from the stats and the map's extent, then NMapOverlay::AddObject per piece (packed type, player, HP 1, script ID -1) and InsertEntrenchment with the sections' link IDs"
  - "Every section of every entrenchment is non-empty and names an engine object after every step (EveryTrenchLinkIsInTheEngine)"

requirements-completed: [D-13, D-03, D-04, D-25]

coverage:
  - id: D1
    description: "D-13: the Entrenchment tool (key 7) draws a polyline - click adds a point, right-click clears, double-click commits - with a live preview; terminators at both ends (the start turned +pi), fireplace/line alternation, arcs at turns, a new section after an arc; the player from the tool"
    requirement: "D-13"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_groups.zig 'three clicks and a double click draw one entrenchment as one undo step', 'a right click clears the polyline with no history ...', 'a one-point commit is a note, not a command', 'a trench off the map is refused whole ...; the preview plans the clicks and the pointer'; tool_registry.zig 'the Entrenchment tool is key 7 ...'"
        status: pass
      - kind: unit
        ref: "zig build test-map-files: TestM2TrenchOverlay (map-file: M2 trench overlay ok) - the L's terminators, arcs, sections, fireplace-first run, half-turn begin, midpoint in map units; refusals (one point, same point, shorter than a piece, NaN, no width, far off every map, past a bounded extent)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=entrenchment, trench_player:0, two clicks, the pointer moved, shot m2_trench_preview, dblclick, trench_delta:1, undo_depth:11, undo, trench_delta:0, redo, trench_delta:1, shot m2_trench, BK_EDITOR_AUTO: done (113 actions)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-08-m2_trench_preview.png and 04-08-m2_trench.png (from the .tga shots), inspected: the preview shows the red polyline with cyan terminators, an orange fireplace, green lines and yellow arcs with direction ticks; after the double click the brown trench lies on the snow left of the bridge, selected in green, the panel says 'entrenchment 0, player 0: 7 pieces in 2 sections'"
        status: pass
    human_judgment: true
    rationale: "Whether the preview and the trench read well is a visual judgment"
  - id: D2
    description: "D-13: the commit is one entrenchments entry plus its pieces as one undo step, ordinary objects, no engine grouping call; D-03: the geometry is NMapGeometry::PlanEntrenchment shared by the bridge and the map-file tier"
    requirement: "D-03"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Entrenchments (editor-bridge: M2 entrenchments draw ok) - BkEditorPlanEntrenchment equals PlanEntrenchment on the stats' inputs; the save equals the expected map (LayTrench); undo saves the unedited bytes, redo the expected map; world matches the map; every section non-empty and every link an engine object after each step; refusals (one point, off the edge 'the trench leaves the map', NaN, null, 257 points, players 99 and -1) change nothing"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 entrenchment round trip ok (19 pieces, 2 sections)"
        status: pass
      - kind: other
        ref: "grep -c AddNewEntrencment Sources/src/EditorBridge/session_groups.cpp = 0"
        status: pass
    human_judgment: false
  - id: D3
    description: "D-03 without goldens: the builder's fidelity by properties over 500 random polylines"
    requirement: "D-03"
    verification:
      - kind: unit
        ref: "zig build test-map-files: TestM2TrenchProperties - 500 planned, 62987 pieces, 6056 arcs, 1912 sections; every property holds; map-file: M2 trench properties ok (500 polylines); the same counts on CI's Linux x86/arm, macOS x86/arm and Windows"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-13/D-04: clicking a piece selects the whole entrenchment, hovering highlights it, Delete removes it whole; a single piece is still refused; a garrisoned entrenchment is refused whole"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: 'a press on a piece selects its entrenchment; Delete removes it whole and undo puts it back at the same index', 'a trench piece alone is still refused to the object delete (D-04)'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2EntrenchmentDelete (editor-bridge: M2 entrenchment delete ok) - Kharkov43 trench 3 picked at 321,240 (BkEditorObjectAt passes the piece over), deleted whole, the save equals the file's map less the entry and pieces, links resolve, undo saves the unedited bytes, picked again; a piece alone REFUSED; a drawn trench deleted and undone; battleofbulge's garrisoned trench refused whole with the map unchanged"
        status: pass
    human_judgment: false
  - id: D5
    description: "The game reads it: the M2 map with a new entrenchment loads, the trace reports one more entrenchment, the game exits cleanly"
    requirement: "D-13"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'game reads it M2 PASS (... bridges 0 -> 2, fences +10, entrenchments 0 -> 1)'"
        status: pass
    human_judgment: false
  - id: D6
    description: "D-25 mid-phase gate: the branch's six CI jobs are green with 04-01..04-08 in"
    requirement: "D-25"
    verification:
      - kind: integration
        ref: "gh run 36663674380 (Cross-platform validation, feat/map-editor-m2 at 4d8ea0565): linux-platform, linux-arm-platform, macos-platform, macos-intel-platform, windows-platform, windows-mingw-platform all success; the engine tiers ran on macOS arm64 and Windows-MSVC (M2 entrenchments draw ok, delete ok, round trip ok, PASS)"
        status: pass
    human_judgment: false

duration: 2h01m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 08: Entrenchments Summary

**The MFC trench builder is ported into the shared geometry unit (NMapGeometry::PlanEntrenchment, with its integer path, hypot lengths and CAngle wrapping) and pinned by properties over 500 random polylines; entrenchments are drawn with the MFC gestures and a live preview, selected, highlighted and deleted as wholes through one all-or-nothing edit that never leaves a section naming a missing piece; the real game loads a map with a new trench; and the branch is green on all six CI jobs mid-phase.**

## Performance

- **Duration:** about 2 h (2026-09-30T02:03Z to 04:05Z), of which about 65 min was waiting on two CI runs
- **Tasks:** 3 (one tracer, two auto) plus one CI fix commit
- **Files:** 23 changed, 2883 insertions, 32 deletions

## Accomplishments

- **MapGeometry**: `TrenchPath` (OnLButtonDown per click: first point, straight runs of line pieces, arcs the shorter way round with the extension after them) and `PlanEntrenchment` (the double click's commit: terminators, pieces at integer midpoints, straight vs arc by 0.9 line widths, the global fireplace/line switcher, sections, directions `int( angle / 2pi * 65535 )`, Vis2AI), refusals for degenerate input and pieces off the map.
- **Session**: `SGroupEdit` carries an entrenchments entry (`bTrench`); draw, list, delete; `CanTakeOutWhole` now also refuses a group a unit is garrisoned in, before anything is taken out.
- **C ABI**: five entries, all `Guarded`, all in both status tables of `TestEntryPointsBeforeAMap`.
- **Core**: `Editor.drawEntrenchment/planEntrenchment/entrenchments/deleteEntrenchment`, `entrenchments_generation`, `EntrenchmentTool` (points, player, pointer, hovered, selected), the fake's entrenchments on the one edit log.
- **App**: registry entry `entrenchment` on key 7 (right button, Ctrl-as-right, double click), the Entrenchments panel, the preview and outlines, `trench_player`/`trench_delete`/`trench_delta`, the M2 scenario segment, the game-reads-it trench.
- **CI**: run 36663674380 green on all six jobs.

## Task Commits

1. **Task 1: Draw an entrenchment end to end (tracer)** - `738917a8e` (feat)
2. **Task 2: Builder properties; pick, hover and delete whole** - `211469afb` (feat)
3. **Task 3: Panel, markers, scenario, game reads it** - `558edb227` (feat)
4. **Task 3, CI gate fixes** - `4d8ea0565` (fix)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; it was run end to end (`test-editor-core test-map-files` rc 0 with `map-file: M2 trench overlay ok`; `test-editor-bridge test-map-editor-engine` rc 0 with all four named lines) before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after the build.zig edit | 3/3 pass |
| `test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-view-events test-map-editor-auto` | rc 0 |
| `test-map-files` | `M2 trench overlay ok`, `M2 trench properties ok (500 polylines)`, `map-file: PASS` |
| `test-editor-bridge` | `M2 entrenchments draw ok`, `M2 entrenchment delete ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 entrenchment round trip ok (19 pieces, 2 sections)`, `map-editor-engine: PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto` (M1, run first) | compare 0.29-0.37 % against the refreshed reference, done (13 actions) |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (113 actions)` |
| `map-editor-game-reads-it-m2` | `game reads it M2 PASS (... entrenchments 0 -> 1)` |

## CI (D-25 mid-phase gate)

| Run | Head | Result |
|---|---|---|
| 36662216504 | `558edb227` | failed: the map-file tier on linux, linux-arm, macos-intel (TestM2VsoBuilder, 04-05: no road descriptor in the narrow checkout) and windows (compile: `far` is a Windows SDK macro, this plan); macos-platform and windows-mingw green |
| **36663674380** | `4d8ea0565` | **success, all six jobs**, 03:16:44Z to 04:03:43Z (about 47 min; windows-platform the longest). The engine tiers ran on macos-platform and windows-platform, Windows picking the same Kharkov43 trench |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing validation] Trench pieces off the map**
- **Found during:** Task 1 (the engine test's off-edge trench was accepted)
- **Issue:** `CAIEditor::IsObjectInsideOfMap` returns true for every entrenchment, so the engine places a piece anywhere; a click near the edge plus the builder's extension after an arc could put pieces outside the map for the game to load.
- **Fix:** `STrenchPlanInput` carries the map's extent (map units); `PlanEntrenchment` refuses a piece outside it ("the trench leaves the map"); map-file and engine tiers test it.
- **Committed in:** `738917a8e`

**2. [Rule 1 - Bug] A garrisoned trench's delete stopped halfway**
- **Found during:** Task 2 (battleofbulge trench 0)
- **Issue:** a unit whose `nLinkWith` names a piece makes the overlay refuse that piece's delete; `RemoveGroup` had already erased the entry and some pieces, leaving "reopen the map".
- **Fix:** `CanTakeOutWhole` refuses a group any outside object is linked into, before anything is taken out; the engine tier proves the refusal leaves the map byte-identical. Bridges share the check.
- **Committed in:** `211469afb`

**3. [Rule 1 - Bug] The seventh tool button hid the brush radius**
- **Found during:** Task 3 (M1 compare 1.22 %, inspected `04-08-t3-m1-edited-left.png`)
- **Fix:** tools panel 150 -> 176 px. The M1 local reference was refreshed (the old kept as `zig-out/local-test/04-08-t3-reference-edited-before-row3.tga`); the remaining 0.35 % outside the left column is tree sway, inspected on crops.
- **Committed in:** `558edb227`

**4. [Rule 3 - Blocking, CI] Two CI failures**
- `far` is a Windows SDK macro (this plan's test variable): renamed.
- The linux, linux-arm and macos-intel jobs' sparse checkout had no road descriptor XML, which 04-05's `TestM2VsoBuilder` loads; the three lists now take `Data/Terrain/sets/*/Roads3D/*.xml` and `Rivers/*.xml` (160 KB).
- **Committed in:** `4d8ea0565`

### Plan wording resolved

- **Straight-run alternation:** the plan's property says each straight run starts with a fireplace. The MFC switcher is one flag for the whole trench (it is not reset after an arc), so the port - and the property test - alternate fireplace, line over all straight pieces from a fireplace.
- **Begin terminator:** "within one direction unit" is within one and a half: both directions are truncated separately, so the half-turn difference is 32766..32769.
- **Delete map:** "the first found under Data\\Maps" (battleofbulge) holds units in all 15 entrenchments; the delete runs on the first sorted map with an empty one (Kharkov43), battleofbulge proves the refusal.
- **Pick after undo:** the pieces an undo puts back are pickable after a frame; the test re-picks through `PickTrenchAt`, which draws one.

### Not a plan change

- The kept game shot (`04-08-game-edited.png`) does not show the new trench, though the trace reports it and the map loads; the same holds with the trench moved into the camera's view. Not investigated further (hypothesis: the game shows entrenchments under the fog of war only once seen). 04-13 looks at the game shots.
- Two local runs of `map-editor-host-check`/`map-editor-auto-m2` failed with the window losing focus on the shared desktop (a smoke tile click, a scripted bridge drag) and passed unchanged on rerun.

---

**Total deviations:** 4 auto-fixed (Rule 1 x2, Rule 2, Rule 3) plus wording resolutions
**Impact on plan:** no scope change.

## Issues Encountered

- `NStr::Format` returns `const char*`, not a string (two `.c_str()` calls removed before the first commit).

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- The group machinery now carries three entry kinds (bridge, none, entrenchment) and refuses a garrisoned group whole; the script ID and garrison on a trench stay with M3 (05-PARITY O18).
- CI is green mid-phase; the narrow checkouts now carry road and river descriptors.

## Threat surface

No new surface beyond the plan's model: T-04-08-01 (empty section, dangling link) - the property test (exact cover, no empty section), all-or-nothing placement, entry-first removal, the engine link check after every step, and the new up-front garrison refusal; T-04-08-02 (degenerate or huge polylines) - fewer than two points refused, non-finite refused at the ABI and in the plan, 256 clicks at the ABI, a 2048-point path cap, the arc-step cap, and pieces off the map refused; T-04-08-03 - only feat/map-editor-m2 was pushed, never forced.

## Self-Check: PASSED

- Files found: `MapGeometry.h`, `MapGeometry.cpp`, `session_groups.cpp`, `bridge.h`, `tools_groups.zig`, `panels_m2.zig`, `markers.zig`, `commands.zig`, `game_reads_m2.zig`.
- Commits found: `738917a8e`, `211469afb`, `558edb227`, `4d8ea0565`.
- Acceptance greps: `PlanEntrenchment` in MapGeometry.h 1; `hypot` in MapGeometry.cpp 2; `AddNewEntrencment` in session_groups.cpp 0; `EntrenchmentTool` in tools_groups.zig 17; `TestM2TrenchProperties` in map_file_test.cpp 2; `BkEditorDeleteEntrenchment` in bridge.h 1; `drawEntrenchments` in panels_m2.zig and panels.zig 1 each; `trench_delta` in commands.zig 3; `std::min|std::max` added 0.
- The executor looked at `m2_trench.tga` (as `04-08-m2_trench.png`) and records that the entrenchment is visible.
- The latest CI run on feat/map-editor-m2 (36663674380) concluded success with all six jobs.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

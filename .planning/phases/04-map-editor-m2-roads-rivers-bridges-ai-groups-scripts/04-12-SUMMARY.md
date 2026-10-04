---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 12
subsystem: map-editor
tags: [map-editor, m2, ai-general, parcels, reinforce-points, mobile-script-ids, mfc-conversion, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords::GetAIGeneralSide / PutAIGeneralSide (the side and the side count together), the byte-identity rule
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: the M2 game-reads-it harness and BK_MAP_TRACE (general side=N parcels=K, parcel side=N ...)
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 10
    provides: the tool shape in tools_ai.zig, the two-pass read rule, the left-column panel per tool
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 11
    provides: the `hidden` palette flag this plan retires, the generic record path for index-keyed kinds with nested owned arrays
provides:
  - BkEditorAIGeneralSide (two-pass read of one side, the side count in the info) and BkEditorSetAIGeneralSide (a raw put of one whole side with the side count on both copies), with BkEditorAISideInfo, BkEditorAIParcel and BkEditorAIPoint
  - NMapGeometry::ParcelPointFromVis, ParcelPointToVis, DirectionFromArrow, RadiusFromArrow and fParcelMinRadius (256)
  - records.Kind.ai_side with AiSide (owned script IDs, parcels, points; appendParcel/appendPoint/removeParcel/removePoint/appendMobile/removeMobile in place), Parcel, ParcelPoint, ParcelKind
  - Editor.aiSideCount/aiSide/editAiSide/addDefenceParcel(In)/addMobileScriptID/removeMobileScriptID; the fake keeps the sides and the real rules
  - tools_ai.AIGeneral (key 9) with the MFC handle radii, grab order, drag formulas, type switch and deletes, and the public formulas the markers and tests share
  - the AI General panel, the parcel and point markers, the commands ai_side / ai_parcel_here / ai_point_here / ai_toggle_type / ai_select / ai_delete / ai_mobile_add / ai_mobile_remove and the predicates parcels and mobile_has
  - the game's general of side 1 reading the new parcel (general side=1 parcels 0 -> 1, parcel type=1 r=256 dir=0)
  - every tool in the palette: Start Target and Reserve Positions are no longer hidden, and the local M1 reference frame was refreshed
affects: [04-13]

commits: 3
plan_head_before: 09fbda7256245cbc7d7216817aeeb7317b25078d
actuals:
  tokens: 43400
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A record whose value is a whole collection with a count that belongs to the parent (a side with the side count) is put and undone whole: the before-record carries the count, so an undo that puts it back shrinks away the sides a click created, and a test compares two session saves byte for byte"
    - "Nested owned arrays in a record value (parcels holding points) are cloned, compared and freed by the value, and edited in place through helpers on an owned copy; a value built by a caller borrows"
    - "The ABI flattens nested arrays into sibling arrays with ranges (parcels name [first_point, first_point + count) of one points array) so a two-pass read needs three capacities and every range is checked before it is indexed"
    - "A drag's press that creates something grabs it at once with no offset, and its creation and movement merge under one gesture, so a press-drag-release is one undo step"
    - "A tool's geometry lives once in the map-file geometry unit (NMapGeometry) and once, mirrored and tested against the same literals, in the Zig tool; the engine tier builds the expected map with the C++ functions"

key-files:
  created: []
  modified:
    - Sources/src/MapFile/MapGeometry.h
    - Sources/src/MapFile/MapGeometry.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/session_records.cpp
    - Sources/editor/core/records.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/tools_ai.zig
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
  - "A parcel's type is a non-exhaustive enum(i32) (defence = 1, reinforce = 2, _), not the plan's enum(u8): a file's own type 0 (the file's 'unknown') or any other int reads, compares and is put back exactly, which an undo of an edit of such a file needs"
  - "The put judges only what it ADDS (type 1 or 2, radius above 0, centre on the map, finite numbers, script IDs 0..32000 once) and exempts any parcel or script ID the side holds now or held when the file was opened (openedAISides), as groups, start commands and reserve positions do; direction outside 0..65535 and point ranges outside the array are caller bugs (BAD_ARGUMENT), the rest REFUSED naming why"
  - "The keys act on the selection (the last parcel or point a press grabbed or made, or the panel's choice), not on a hovered parcel: the tool gets no move event, and the MFC editor's own keys only worked while its button was held. Escape lets go of the selection"
  - "A point's arrow drag wraps its angle into 0..2 pi like the parcel's arrow (the plan says 'the same angle rule'); the MFC editor cast a negative float to WORD there, which is undefined"
  - "The tool works in AI units with the MFC's world-unit handle sizes divided by fAITileXCoeff (a world cell is 64 AI units): centre 64, arrow 32, point 42.67, point arrow 32, the point's arrow 96 from the point"
  - "A drag's edits skip a place the bridge refuses (a centre dragged off the map) and the drag goes on, as the Selector and the script-area handles do"
  - "Every tool is in the palette (orchestrator decision): start_target and reserve_positions lost `hidden`, nothing uses the flag now, and the tools panel is 228 pixels tall (five rows of buttons) instead of 176, which moved the smoke's tile combo and the left column of the M1 frame"
  - "The fake exempts only the parcels the side holds now, not those the file held at open (it keeps no copy); the real bridge keeps both"
  - "The scenario shoots before its undos (the plan put the undo keys first, which would have left nothing to look at): parcel, point, undo, redo, Enter, a mobile ID, shot, then four undos back to parcels:1:0"

patterns-established:
  - "The AI General panel takes the left column while its tool is in hand (like Script areas and Reserve positions); the parcels marker kind stays on while the tool is in hand and dims the sides it is not editing"
  - "A list row that would not fit the 280-pixel column says less (kind, radius in tiles, direction) and the selected parcel's detail goes on wrapped lines below the list"

requirements-completed: [D-19, D-02, D-01, D-06]

coverage:
  - id: D1
    description: "D-19 with C2: the AI General tool edits one side at a time; a click outside every parcel makes a defence parcel of radius 256 AI units (4 map tiles) and direction 0; a click inside one adds a reinforce point stored relative to the centre and turned by minus the defence direction (MFC store formula); the click is cut as Vis2AI cuts it before the centre is taken away"
    requirement: "D-19"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig 'AI General: a click outside every parcel makes a defence parcel of radius 256, in one undo step', 'AI General: a click inside a parcel adds a reinforce point stored relative to the centre, turned by minus the direction', 'AI General: a point in a parcel of direction 0 is the click less the centre, and the click is cut first', 'AI General: a press at the radius is outside the parcel, one inside it adds a point', 'AI General: the formulas give the MFC editor's literal values'; editor.zig 'an AI parcel: addDefenceParcel is one step, creates the sides below, and undo restores the count exactly'"
        status: pass
      - kind: integration
        ref: "zig build test-map-files: map-file: M2 parcel formulas ok (ParcelPointFromVis at directions 0, 16384 and 8192 by hand from the MFC formula: (100, 49), (48.9976, -100.0012), (105.3585, -36.0637); ParcelPointToVis, DirectionFromArrow over four quadrants, four axes and the wrap at 2 pi, RadiusFromArrow below, at and above 256, PutAIGeneralSide round trip)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2AIGeneral on coldwinter: a defence parcel on side 1 saves as the NMapRecords-built map with type 1, radius 256, direction 0 (editor-bridge: M2 ai general ok)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-19: dragging changes a parcel's centre (its points move with it), its radius (never below 256) and defence direction, a point's position and a point's direction; Enter / Insert / Space switch the parcel type; Delete removes a point or a parcel; parcels are drawn as circles with a direction arrow coloured by type, points as arrows; one undo step per click, drag or key"
    requirement: "D-19"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig 'AI General: dragging the centre handle moves the parcel and its points move with it', '... dragging the arrow handle sets the radius, never below 256, and the direction', '... dragging a point handle moves the point, and its arrow sets its direction', '... Enter, Insert and Space switch the type, one undo step each', '... Delete removes the selected point, else the selected parcel, one undo step each', '... the press that adds a point goes on dragging it, in the same undo step', '... a press on a handle and no movement is no edit', '... a centre dragged off the map is skipped and the drag goes on'; test-map-editor-view-events: view.zig 'key 9 is the AI General tool: a click on open ground makes a parcel, Enter switches it, Delete removes it, each one step'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2AIGeneral gestures block: a point, a moved centre, a new radius (500) and direction, a second point in the turned parcel (stored through ParcelPointFromVis, drawn back within 1.5 world units), a point's direction, a type switch, a point and a parcel deleted, each saved equal to the map the map tier's own put builds, and every step put back in reverse saves the unedited bytes (editor-bridge: M2 ai general edits ok)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-12-m2_ai_general.png (from m2_ai_general.tga), inspected: a large yellow ellipse (a reinforce parcel, yellow) round the parcel centre on the snow beside the bridge, its centre-handle ring, a line out along its direction to the ring of the arrow handle, a reinforce point with a filled disc (selected), its ring and its own arrow; the AI general panel lists 'side 0 / side 1 / new side (2)', the mobile ID 4245 with Remove, '0: reinforce, 4.0 tiles, 0 deg', Switch type and Delete and the wrapped selection lines"
        status: pass
    human_judgment: true
    rationale: "Whether the handles are easy to grab at other zooms, whether the arrow's tail reads as a direction and how the dimmed other side looks are judgments; the tests prove the numbers and the shot shows the drawing"
  - id: D3
    description: "D-19: mobile script IDs of the side are added (no duplicates, 0..32000) and removed in the panel, one undo step each"
    requirement: "D-19"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'mobile script IDs: add and remove are one step each, a duplicate is a note, the range is 0..32000' (also: on a side the map lacks the first ID creates it and the sides below, and undo takes them away)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2AIGeneral refusals (a script ID above 32000, -1, twice: REFUSED saying so, bytes unchanged); zig build map-editor-auto-m2: do=ai_mobile_add:4245, expect=mobile_has:1:4245 (BK_EDITOR_AUTO: done, 260 actions)"
        status: pass
    human_judgment: false
  - id: D4
    description: "C8 / Pitfall 11: editing a side that does not exist creates it and every lower side empty; undo restores the old number of sides exactly (the saved file equals an unedited save)"
    requirement: "D-19"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2AIGeneral on coldwinter (2 sides): a parcel on a side two above the current count makes the side between empty, both saved as the expected map, and putting the sides back with the old count saves the unedited map byte for byte (two session saves compared)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 ai general round trip ok (a parcel on side 1 and on side count + 1 through Editor on the real bridge, raw reads in two passes, undo, redo and undo with the side count and dirty flag exact, a mobile ID in and out, engine and document still agree)"
        status: pass
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig 'AI General: on a side beyond the count the count grows with empty sides below, and undo shrinks it back'"
        status: pass
    human_judgment: false
  - id: D5
    description: "The game reads it: the M2 map's new parcel on side 1 at direction 0 reaches the enemy general (trace general side=1 parcels one above the unedited map, and a parcel line whose centre, radius and direction equal the stored values)"
    requirement: "D-19"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'BK_MAP_TRACE: general side=1 parcels=0 mobile=0' (baseline) then 'general side=1 parcels=1 mobile=0' and 'parcel side=1 idx=0 type=1 cx=1536 cy=1536 r=256 dir=0'; game reads it M2 PASS (... general side 1 parcels 0 -> 1, script m2_script ran, area m2_area found)"
        status: pass
    human_judgment: false
  - id: D6
    description: "D-02, D-01, D-06: the side rides the generic record command, every put and its inverse save the unedited file byte for byte, a parcel's refusals change nothing, and the parcels marker kind draws them"
    requirement: "D-02"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2AIGeneral refusals (type 3 and 0, direction 70000 and -1, NaN centre, centre off the map and far beyond, radius 0, negative and NaN, point range outside the array, negative first point, a NaN point, point direction 70000, script ID 32001, -1, twice, a parcel on a side at the count, a side or count out of range, counts with no arrays) - REFUSED or BAD_ARGUMENT, two session saves equal"
        status: pass
      - kind: integration
        ref: "zig build map-editor-host-check: panel smoke PASS; map-editor-smoke: smoke PASS (52 steps); map-editor-auto: compare=edited 0.01-0.44% of pixels differ (limit 1%) against the refreshed reference; map-editor-auto-m2: BK_EDITOR_AUTO: done (260 actions)"
        status: pass
    human_judgment: false

duration: 0h50m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 12: AI general Summary

**A side of the AI general - mobile script IDs, parcels and reinforce points - is edited on the map with the MFC gestures and in a panel, through the generic record command with the whole side and the side count as the record, so creating side 3 and undoing it gives the unedited file back byte for byte, and the real game's general of side 1 reads the new parcel.**

## Performance

- **Duration:** about 50 min (14:28 to 15:18 local, +07), of which most was the four-minute engine-tier runs and the game-reads run
- **Tasks:** 3 (one tracer, two auto)
- **Files:** 23 changed, 3132 insertions, 35 deletions

## Accomplishments

- **A defence parcel end to end (tracer):** `BkEditorAIGeneralSide` reads one side in two passes (a side at or above the count reads empty with the current count) and `BkEditorSetAIGeneralSide` puts a whole side with the side count on both copies; `Kind.ai_side` rides the generic record path, `Editor.addDefenceParcel` makes a parcel of radius 256 and direction 0, and the AIGeneral tool (key 9) does it with a click. The engine tier saved it equal to the map the map tier builds and undid a side two above the count back to the unedited bytes; the game's general of side 1 read it (`parcels 0 -> 1`, `r=256 dir=0`).
- **The MFC formulas, once in C++ and once in the tool:** `ParcelPointFromVis` / `ParcelPointToVis` / `DirectionFromArrow` / `RadiusFromArrow` are pinned by literal cases computed by hand from StateAIGeneral.cpp; the Zig tool mirrors them and is tested against the same literals, and the engine tier builds its expected maps with the C++ functions.
- **The gestures:** handles are grabbed in the MFC order with the MFC radii (centre, arrow, point, point arrow), a click inside a parcel adds a point that is grabbed at once, drags keep the grab offset and merge into one undo step, the arrow sets the radius (floor 256) and direction, Enter / Insert / Space switch the type, Delete removes the point else the parcel.
- **The panel and markers:** side radios with a "new side", the mobile script IDs with Add and Remove, the parcel list with Switch type and Delete, the parcels drawn as rings with a direction arrow and their points as arrows in the MFC colours, the selection thicker with filled handles, the other sides dimmed.
- **Every tool is in the palette:** Start Target and Reserve Positions are visible again; the tools panel grew to five rows and the local M1 reference frame was refreshed (old one kept).

## Task Commits

1. **Task 1: A defence parcel end to end (tracer)** - `7db4b3a51` (feat)
2. **Task 2: Reinforce points, drags, type switch and deletes with the MFC store formulas** - `231e51b92` (feat)
3. **Task 3: AI General panel, parcel markers, scenario and the engine round trip** - `21cd8bd97` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; the engine tier (`editor-bridge: M2 ai general ok`, `editor-bridge: PASS`) and the game proof (`general side=1 parcels=1`, baseline 0, `parcel ... type=1 r=256 dir=0`) passed before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core` | 225 tests pass |
| `test-map-editor-panels`, `test-map-editor-view-events`, `test-map-editor-view`, `test-map-editor-auto`, `test-map-editor-testlaunch` | 496 tests pass in the last combined run (with core) |
| `test-map-files` | `M2 parcel formulas ok`, `map-file: PASS` |
| `test-editor-bridge` | `M2 ai general edits ok`, `M2 ai general ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 ai general round trip ok`, `PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-smoke` | `smoke PASS (52 steps)` |
| `map-editor-auto` (M1) | `compare=edited` 0.01-0.44% of pixels differ (limit 1%) against the refreshed reference |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (260 actions)` |
| `map-editor-game-reads-it-m2` | `game reads it M2 PASS (... general side 1 parcels 0 -> 1, ...)` |

Logs are under `zig-out/local-test/04-12-*.log`; the inspected shots (as PNG) are `04-12-m2_ai_general.png` and its crop `04-12-m2_ai_general-crop.png`; the reference refresh is documented by `04-12-m1-reference-old.png`, `04-12-m1-new.png` and `04-12-m1-diff.png`.

## The M1 reference refresh (orchestrator decision)

04-11 had hidden its two new tools from the palette only to keep the M1 frame under 1%. This plan shows every tool, so the palette has three more buttons (five rows, `tools_height` 228 instead of 176) and everything under it in the left column moved down. The first M1 run differed by 6.38% from the old reference. Inspected, by region (channel tolerance 24, as the comparator):

| Region | Differing pixels | Share of the frame |
|---|---|---|
| left column, x < 280 (tool palette and the object panel below it) | 60401 | 5.899% |
| menu bar (the Unit menu, there since 04-11) | 301 | 0.029% |
| map viewport | 4635 | 0.453% (rendering noise: trees and shadows; 04-11 measured 0.53-0.65% with the menu) |
| right column and status bar | 0 | 0% |

So the differences are confined to the tool palette and the panel below it, plus the noise that was already there. The old reference is kept as `zig-out/local-test/map-editor-auto/reference/edited.before-04-12.tga`; `reference/edited.tga` is the new capture. Two later M1 runs differ from the new reference by 0.0115% and 0.44%.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The smoke's tile combo moved when every tool became visible**
- **Found during:** Task 3 (first `map-editor-host-check map-editor-auto-m2` run: "a click on the tile combo opens the picker ... the picker is not open")
- **Issue:** the orchestrator's decision adds three palette buttons (11 tools); at 280 pixels they need five rows, not three, and the 176-pixel tools panel clipped the Brush section, so the scripted click on the tile combo landed on nothing.
- **Fix:** `layout.tools_height` 176 -> 228; the smoke passes (52 steps) and the M1 frame's left column moved, which is the refresh above.
- **Files modified:** `panels.zig`
- **Commit:** `21cd8bd97`

**2. [Rule 2 - Missing critical functionality] A file's own odd AI data must come back from an undo**
- **Found during:** Task 1 (threat T-04-12-03, the 04-09/04-10/04-11 pattern)
- **Issue:** the put's rules (type 1 or 2, radius above 0, a centre on the map, script IDs once) would refuse the undo of an edit of a shipped or hand-made map that holds a parcel or an ID the rules forbid.
- **Fix:** `openedAISides` (taken at open) and the current side exempt every parcel and script ID the side holds now or held then, as often as it held them; a new one like it is refused. A parcel's type is a non-exhaustive enum so such a file's own type reads and comes back exactly.
- **Files modified:** `session.h`, `session.cpp`, `session_records.cpp`, `records.zig`
- **Commit:** `7db4b3a51`

**3. [Rule 1 - Bug] The scenario's second click made a second parcel**
- **Found during:** Task 3 (`expect=undo_depth:24` after Enter: "select a parcel first")
- **Issue:** the click meant to land inside the first parcel was 100 screen pixels straight below it, which on the iso ground is about 200 world units, past the 181-unit radius, so it made another parcel and the undo that followed dropped the selection.
- **Fix:** the clicks are 80 pixels apart on the horizontal screen axis, which is not compressed; they are now at `c80x-150` and `c160x-150` so the parcel is in view and clear of the right panel.
- **Files modified:** `build.zig`
- **Commit:** `21cd8bd97`

**4. [Rule 3 - Blocking] bridge.cpp did not include the map records header**
- **Found during:** Task 1 (first engine build: `use of undeclared identifier 'NMapRecords'`)
- **Fix:** `#include "../MapFile/MapRecords.h"` in bridge.cpp for `nMaxAIGeneralSides`.
- **Files modified:** `bridge.cpp`
- **Commit:** `7db4b3a51`

### Plan wording resolved

- **`Parcel { kind: enum(u8) { defence = 1, reinforce = 2 } ... }`:** `enum(i32)` with `_` (see key decisions).
- **`AiSide { side, side_count, mobile_ids, parcels }` and the plan's `Editor.addDefenceParcel(side, map_x, map_y)`:** as written, plus `addDefenceParcelIn(side, x, y, gesture)` so the press that makes a parcel and the drag that places it are one undo step; the plan's "hovered" parcel for the keys is the selection (see key decisions).
- **Scenario order:** the plan listed "the undo key" after the click inside and before Enter, and three undos at the end; the shot has to come before the undos to show anything, so the scenario is click, click, undo, redo, Enter, mobile ID, shot, four undos (the point, the type, the ID and the parcel), `expect=parcels:1:0`.
- **`parcels` (arg `side:N` delta):** `expect=parcels:1:1` is side 1, one parcel more than when the map opened; `mobile_has:1:4245` is side 1 and script ID 4245.
- **Extra commands:** `ai_select:N` (the panel's choice) and `ai_delete` (the Delete button), so every control is reachable from BK_EDITOR_AUTO.
- **The panel's list row** shows kind, radius in tiles and direction; the centre and the point count are on the selection lines below the list (the 280-pixel column cut the longer row).
- **Commit trailer:** `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, as in 04-01 and 04-09..04-11: the model that ran this plan, not the line the sequential-execution message names.

### Not a plan change

- The view test for key 9 replaced the old "9 is nobody's" assertions with key 0; a view test covers the tool by key, click, Enter, Delete and undo.
- Task 1's commit carries a first version of the tool (a click outside every parcel) and the registry change; Task 2's commit carries the full tool.

---

**Total deviations:** 4 auto-fixed (one Rule 1, one Rule 2, two Rule 3) plus wording resolutions and the orchestrator's palette decision
**Impact on plan:** no scope change; deviation 2 is a correctness requirement the plan's threat T-04-12-03 implied, deviation 1 is the price of the orchestrator's palette decision.

## Issues Encountered

- Each C++ change costs a four-minute engine-tier run (the bridge test plays the whole 04-01..04-12 suite); the only C++ build errors were the missing include and a most-vexing-parse in the test.
- The first combined run failed in `map-editor-smoke` with the window unfocused in its state line; it was the palette layout (deviation 1), and the run after the fix passed without a rerun flake.
- `sed -i` without a suffix on macOS ate the file name; edits were made with Python.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-13's walk-through can reuse `ai_side`, `ai_parcel_here`, `ai_point_here`, `ai_toggle_type`, `ai_mobile_add`, `parcels` and `mobile_has`, and the `m2_ai_general` shot.
- The M1 reference frame (`reference/edited.tga`) is the one with every tool in the palette; `edited.before-04-12.tga` is the old one if a comparison is ever wanted.
- The game gives a general to sides 0 and 1 only, and never to the player's own side; a parcel on another side is saved and ignored by the game, which the panel's help says.

## Threat surface

No new surface beyond the plan's model: T-04-12-01 - every parcel's point range is checked against the points array, directions 0..65535 and counts are BAD_ARGUMENT, types, radius, centre, finite numbers and script IDs are REFUSED naming why, all tested with the bytes compared; T-04-12-02 - the reads are two-pass with capacities (a sizing pass is REFUSED and still answers the counts), counts are capped at 65536 and 1024 sides, the markers draw at most 128 parcels a frame and the panel caches 64 sides; T-04-12-03 - the raw put restores the side count, tested by comparing two session saves after creating and undoing a side two above the count.

## Self-Check: PASSED

- Files found: `bridge.h`, `bridge.cpp`, `session.h`, `session.cpp`, `session_records.cpp`, `MapGeometry.h`, `MapGeometry.cpp`, `records.zig`, `editor.zig`, `fake_bridge.zig`, `tools_ai.zig`, `c_bridge.zig`, `c_bridge_test.zig`, `view.zig`, `tool_registry.zig`, `panels.zig`, `panels_m2.zig`, `markers.zig`, `commands.zig`, `game_reads_m2.zig`, `map_file_test.cpp`, `editor_bridge_test.cpp`, `build.zig`.
- Commits found: `7db4b3a51`, `231e51b92`, `21cd8bd97`.
- Acceptance greps: `BkEditorAIGeneralSide|BkEditorSetAIGeneralSide` in bridge.h at least 2; `ai_side` in records.zig at least 1; `AIGeneral` in tools_ai.zig at least 2; `ParcelPointFromVis|DirectionFromArrow|RadiusFromArrow` in MapGeometry.h at least 3; `drawAIGeneral` in panels_m2.zig and panels.zig 2; `mobile_has|ai_parcel_here` in commands.zig at least 2; `std::min|std::max` added 0.
- The executor looked at `m2_ai_general` (as PNG and a crop) and recorded what was seen (D2).

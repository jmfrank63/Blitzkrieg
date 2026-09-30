---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 06
subsystem: map-editor
tags: [map-editor, m2, bridges, map-geometry, edit-log, compound-edit, specular-mark, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords Insert/EraseBridgeEntry and SetObjectHP, SAddObject with nFrameIndex/fHP/nScriptID, WhyNotPlacedByPalette, the marker layer and the selection_outline kind
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: BK_MAP_TRACE bridges line, game_reads_m2.zig
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 05
    provides: the edit log (IEditRecord, LogEdit, BkEditorUndoEdit/RedoEdit), the core .edit command with scope objects, reloadObjects
provides:
  - NMapGeometry (MapFile/MapGeometry.h) - PlanBridge, RotatedBridgeDrag, BridgePartnerName, plain numbers in and out
  - session_groups.cpp - SGroupEdit (RemoveGroup entry-then-spans, AddGroup spans-then-entry, all or nothing), BuildOneBridge, group pick, delete, rotate, toggle, ApplyBridgeMarks
  - C ABI BkEditorBridgeDescriptors/PlanBridge/DrawBridge/Bridges/PickGroup/DeleteBridge/RotateBridge/ToggleBridgeBuild
  - core Editor.drawBridge/deleteBridge/rotateBridge/toggleBridgeBuild/pickGroup/planBridge/bridges/bridgeDescriptors, bridges_generation; fake bridges on one shared edit log
  - tools_groups.BridgeTool (key 5) - drag draws, click selects, Q/E rotate, Enter toggles, Delete removes
  - the Bridges panel, the drag ghost, bridge outlines, commands bridge_desc/rotate/toggle_build/delete and predicates bridge_delta/bridge_built
  - BK_EDITOR_AUTO scripted presses held across frames (View.holdScripted)
affects: [04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 5
plan_head_before: 9e689f6b65a2f0e4f9d7224bc147c5099d842a6f
actuals:
  tokens: 60350
  tasks: 4
  commits: 5

tech-stack:
  added: []
  patterns:
    - "A group edit is one SGroupEdit: the group it takes out and the group it puts in, each its entry, entry index and both copies' span records with their list places; RemoveGroup erases the entry first, then the spans in descending place (capturing their records); AddGroup restores the spans ascending, builds them in the engine in entry order, then inserts the entry - so no step ever leaves an entry naming a missing object"
    - "A new group is built from a plan as pre-made SDeletedObject records (place size_t(-1): RestoreObject appends) and put in through the same AddGroup its redo uses, so the first apply and every redo are one code path"
    - "Engine refusals of a compound put are all or nothing: AddGroup takes back every record and engine object it made; a rotate whose new group is refused puts the old one back"
    - "Visual marks that live on engine objects are re-applied at the end of UpdateSessionWorld (ApplyBridgeMarks), so open, undo, redo and rotate all show them without their own calls"
    - "BK_EDITOR_AUTO press=/rpress= hold the button in View.scripted_buttons until release=/rrelease=, which the stale-gesture guard counts as held"

key-files:
  created:
    - Sources/src/MapFile/MapGeometry.h
    - Sources/src/MapFile/MapGeometry.cpp
    - Sources/src/EditorBridge/session_groups.cpp
    - Sources/editor/core/tools_groups.zig
  modified:
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/root.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/smoke.zig
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
  - "The working copy's sprite of a new span is chosen with the stats' seeded helper: seed 0 for the begin and end spans (so the begin's origin is the one the plan was fitted with) and the span's place for a middle one, so a long bridge is not one repeated plank; the file gets the packed type (C6)"
  - "PlanBridge leaves out the MFC editor's world-unit nudges before Vis2AI (as the plan says): the saved position is the truncated one plus the one map-unit nudge. The two differ only for a begin origin whose fractional part lies in [0.7, 0.842); the shipped WoodenBig_Heavy origins are whole numbers"
  - "A click (a press released within 4 screen pixels) never draws: on a span it selects the whole bridge, on empty ground it drops the selection. The MFC tool drew a two-span bridge on any release; here a bridge is a drag so a click can select (D-11)"
  - "A rotate plans the partner through NMapGeometry::RotatedBridgeDrag - from centre - n L / 2 to that plus (n + 0.5) L - so the span count is kept exactly and the engine tier builds its expected map with the same function; the new bridge's centre is the old one up to the grid fit (measured within one span length)"
  - "A bridge with a span the editor could not put back (a type the database does not know, a span the engine never held, a shared link ID) is refused to delete and rotate: its undo could not rebuild it (preservation invariant)"
  - "Editor.drawBridge returns the entry's index (the plan wrote void), as addVso does: the tool selects what it drew"
  - "Editor.planBridge leaves the status line alone (the ghost asks every frame); the refusal text comes from bridge.lastMessage()"
  - "The built-during-play dashed outline is orange, not the engine mark's blue, which it could not be seen against (measured on the m2_bridges shot)"
  - "The Bridges panel replaces the object palette while the Bridge tool is active, as the Roads & Rivers panel does; the tool palette's buttons now wrap onto a second row"

patterns-established:
  - "Engine-tier expected maps for group edits: PlanBridge on inputs read from the object database's own stats, then NMapOverlay::AddObject per span and InsertBridgeEntry; a rotate erases the drawn entry, deletes its spans and lays the partner's plan with the link IDs after them"
  - "Measured marks: capture the object's screen box before and after, count pixels changed by more than 48 in summed channels; undo must come back within 0.5 %"

requirements-completed: [D-10, D-11, D-12, D-03, D-01, D-06]

coverage:
  - id: D1
    description: "D-10: the Bridge tool (key 5) lists every SGVOGT_BRIDGE descriptor with its picture, shows a ghost that follows the drag, snaps the drag to one axis and commits begin, middle and end spans plus a new bridges entry as one undo step, all or nothing"
    requirement: "D-10"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_groups.zig 'a horizontal drag draws one bridges entry and its spans as one undo step', 'undo removes the spans and the entry, redo puts both back'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Bridges (editor-bridge: M2 bridges draw ok) - the save equals PlanBridge + AddObject/InsertBridgeEntry on the file's map; undo saves the unedited bytes; every link is an engine object"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 bridge round trip ok (5 spans)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-10: a drag along the other axis, an off-map span or an engine refusal is refused with a status note and changes nothing"
    requirement: "D-10"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: 'a vertical drag with a horizontal type is refused and records nothing', 'a drag whose spans would leave the map is refused and records nothing'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Bridges refusal block (wrong axis, off the edge - 'the engine would not place 5 of the bridge's 7 spans', unknown type, not a bridge, NaN, null) saves the unedited bytes"
        status: pass
    human_judgment: false
  - id: D3
    description: "D-03 / C6: span positions from NMapGeometry::PlanBridge (FitVisOrigin2AIGrid, span length, truncation, one nudge in AI units); the snapshot saves the packed type, the working copy and engine a seeded concrete index"
    requirement: "D-03"
    verification:
      - kind: unit
        ref: "zig build test-map-files: TestM2BridgePlan (map-file: M2 bridge plan ok) - x-0.1 / y+0.1 compared as arithmetic, counts, types, n=0, refusals, partner names, overlay write/read/erase byte identity"
        status: pass
      - kind: integration
        ref: "TestM2Bridges: BkEditorPlanBridge equals PlanBridge on the stats' inputs; the stats agree with the map-file tier's literals (L = 6 * fWorldCellSize / 2, origin 320,160)"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-11: clicking a span selects its whole bridge (outlined); Delete removes the group and its entry; undo restores both at the same index"
    requirement: "D-11"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: 'a click on a span selects its whole bridge ...', 'Delete removes the selected bridge whole, and undo puts the spans and the entry back at the same index', 'a span alone is still refused to the object delete ...'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2BridgeDelete (editor-bridge: M2 bridge delete ok) - arnheim bridge 0 picked at 320,231, BkEditorObjectAt there answers no span, the save equals the file's map less the entry and spans, undo saves the unedited bytes"
        status: pass
    human_judgment: false
  - id: D5
    description: "D-11: Q/E (or Rotate) swaps _01/_02 and rebuilds about the same centre along the other axis with the same span count as one undo step; no partner or off-map rotation is refused"
    requirement: "D-11"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: 'Q rotates the selected bridge to its partner ...', 'a bridge with no rotated variant is refused and the history stays as it was'"
        status: pass
      - kind: integration
        ref: "TestM2BridgeRotateToggle: the entry names W_WoodenBig_Heavy_02 spans, same count, centre within one span length; the save equals the RotatedBridgeDrag + PlanBridge expected map; a rotation off the map refused ('3 of the bridge's 9 spans'), the bridge kept"
        status: pass
    human_judgment: false
  - id: D6
    description: "D-12 with C1: Enter (or the checkbox) toggles a WoodenBig_Heavy bridge between intact (fHP 1) and built during play (fHP -1 on every span); others refused; the spans are visibly marked (SetSpecular 0xFF0000FF, re-applied after undo, redo, rotate and reopen)"
    requirement: "D-12"
    verification:
      - kind: integration
        ref: "TestM2BridgeRotateToggle: saved HP -1 on every span and read back equal; the mark changes 24911 of 73777 pixels (33.77 %) of the bridge's box; after the undo 0 of 73777 (0.00 %); a W_WoodenLittle bridge refused"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-06-t3-toggle-marked.png against 04-06-t3-toggle-before.png, inspected: the whole bridge deck and trestles tinted blue"
        status: pass
    human_judgment: true
    rationale: "Whether the mark reads well is a visual judgment; the pixel count only says it is there"
  - id: D7
    description: "The game reads it: the M2 map with one rotated and one built-during-play bridge loads; the trace reports two more bridges"
    requirement: "D-10"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'game reads it M2 PASS (... bridges 0 -> 2)'"
        status: pass
    human_judgment: false
  - id: D8
    description: "D-06: the Bridges panel with pictures, the drag ghost, the selected outline and the dashed built-during-play outline; the scripted M2 segment"
    requirement: "D-06"
    verification:
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=bridge, a drag over four frames, bridge_delta:1, E, ENTER, bridge_built, two undos and two redos, BK_EDITOR_AUTO: done (80 actions)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-06-m2_bridges.png (from m2_bridges.tga), inspected: the rotated W_WoodenBig_Heavy_02 bridge, blue engine mark, dashed orange outline, green selection outline; the Bridges panel with pictures, 'bridge 0: W_WoodenBig_Heavy_02, 4 spans', Built during play checked"
        status: pass
    human_judgment: true
    rationale: "Panel layout and marker legibility are visual judgments"

duration: 50min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 06: Bridges Summary

**Bridges are drawn, picked, rotated, toggled built during play and deleted as whole span groups at MFC parity: the span geometry is one plain-number function (NMapGeometry::PlanBridge) the bridge and the map-file tier share, every edit is one all-or-nothing entry of the edit log that never leaves a bridges link dangling, the built-during-play spans carry the MFC editor's specular mark, and the real game loads a map with two new bridges.**

## Performance

- **Duration:** about 50 min (2026-09-30T00:34Z to 01:24Z)
- **Tasks:** 4 (one tracer, three auto) plus the ordered auto-runner fix
- **Files:** 25 changed (4 created), 4033 insertions, 84 deletions

## Accomplishments

- **MapGeometry** (`Sources/src/MapFile/MapGeometry.{h,cpp}`, MapFile library): `PlanBridge` (axis check and lock, ordering, `FitVisOrigin2AIGrid` start, `int( run / L )` middle spans, `Vis2AI` truncation, one nudge in map units, packed types), `RotatedBridgeDrag`, `BridgePartnerName`. No engine or object-database header.
- **The bridge side** (`session_groups.cpp`): `SGroupEdit` with `RemoveGroup` (entry first, then spans in descending place) and `AddGroup` (spans ascending, the engine in entry order through `BuildOneBridge`, then the entry), all or nothing; `BridgePlanInputFor` (seeded begin index, empty lists refused - T-04-06-01); draw, group pick, delete, rotate, toggle; `ApplyBridgeMarks` at the end of `UpdateSessionWorld`. `BuildOneBridge` was extracted from `BuildBridges`; `PlaceOneObject` is now declared in `session.h`.
- **C ABI**: eight entries, all `Guarded`, all in both status tables of `TestEntryPointsBeforeAMap`.
- **Core**: the `Editor` calls under scope `objects` (the document's objects re-read after do, undo and redo), `bridges_generation`, the fake's bridge model on the one edit log it shares with roads and rivers, `tools_groups.BridgeTool`.
- **App**: registry entry `bridge` on key 5, the Bridges panel, the ghost and outlines, the commands and predicates, the M2 auto segment and the game-reads-it bridges.

## Task Commits

0. **Auto-runner fix (Rule 2, ordered by the orchestrator)** - `5d123ca1b` (fix)
1. **Task 1: Draw a bridge end to end (tracer)** - `1f40b44b6` (feat)
2. **Task 2: Picked and deleted as wholes** - `df77c3baf` (feat)
3. **Task 3: Rotate and built during play, with the mark** - `fa6655519` (feat)
4. **Task 4: Panel, ghost, scripted scenario, game reads it** - `e4cca92ee` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; it was re-run end to end (hermeticity, core, panels, map-file, view, auto parser: 45/45 steps, 352/352 tests; bridge and engine tiers) and passed before Task 2 began.

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core`, `-panels`, `test-map-files`, `-view-events`, `-view`, `-auto`, `test-editor-bridge`, `test-map-editor-engine` (final run) | 172/172 steps, 374/374 tests; `map-file: M2 bridge plan ok`, `map-file: PASS`; `M2 bridges draw ok`, `M2 bridge delete ok`, `M2 bridge rotate and toggle ok`, `editor-bridge: PASS`; `M2 bridge round trip ok (5 spans)`, `map-editor-engine: PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto` (M1) | compare 0.0054 % against the refreshed reference, `BK_EDITOR_AUTO: done (13 actions)` |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (80 actions)`, no failed expectation |
| `map-editor-game-reads-it-m2` (runs M1 first) | M1 PASS; `game reads it M2 PASS (... roads 3 -> 4, rivers 0 -> 1, bridges 0 -> 2)` |

Logs are under `zig-out/local-test/04-06-*.log`. Windows and CI were not run (the plan does not ask for them).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing functionality, ordered] BK_EDITOR_AUTO drags could not span frames**
- **Found during:** 04-05 (its Deviation 3); fixed first here as the orchestrator asked.
- **Issue:** a scripted button is only an SDL event; `View.update`'s stale-gesture guard reads `SDL_GetMouseState` (no button) and ended every scripted drag at the end of the press's frame.
- **Fix:** `View.scripted_buttons` / `holdScripted`: the auto runner's `press=`/`rpress=` hold the button until `release=`/`rrelease=`, and `update` ORs the hold into the real mask. View test "a scripted press stays held across frames until its scripted release ..." (three frames, three cells, one undo step; without the hold a stray stroke still ends). M1 `map-editor-auto`: the brush drag now paints its whole stroke - 2289 pixels of the stroke's region changed against the old shot where the old reference and the old shot differed by 41 (inspected crops `04-06-t0-zoom-{before,after}.png`); the local reference was refreshed (old kept as `04-06-t0-reference-edited-old.tga`). The M2 bridge segment drags over four frames.
- **Committed in:** `5d123ca1b`

**2. [Rule 1 - Bug] The fifth tool button was clipped off the tool palette**
- **Found during:** Task 4 (inspecting `m2_bridges`)
- **Issue:** the palette put every tool on one row; "Bridge" fell past the panel's right edge.
- **Fix:** the buttons wrap onto a new row when the next would not fit. This moved the left column down, so the M1 compare measured 1.57 % (all in x < 300, inspected `04-06-t4-m1-edited-left.png`) and its local reference was refreshed (old kept as `04-06-t4-reference-edited-before-wrap.tga`).
- **Committed in:** `e4cca92ee`

**3. [Rule 2 - Diagnostics] A failed `expect=` said nothing about why**
- **Found during:** Task 4 (the first bridge drag landed on tanks)
- **Fix:** the auto runner prints the view's and the editor's status line with a false predicate.
- **Committed in:** `e4cca92ee`

### Plan wording resolved

- `Editor.drawBridge` returns the entry index (see key-decisions); `BridgePlanInputFor` returns the plan input only (the concrete indexes are chosen per span in `NewGroupFromPlan`, seeded).
- A click selects and never draws (key-decisions); the MFC tool drew a two-span bridge on any release.
- The engine's refusal of a span can be another object, not only the map's edge (the first scripted drag landed on tanks); the message says "off the map, or on another object".
- `TestM2BridgeDelete` uses arnheim's bridge 0 (RailwayBridge_Little_02, 3 spans).

---

**Total deviations:** 3 auto-fixed (1 Rule 1, 2 Rule 2, one of them ordered) plus wording resolutions
**Impact on plan:** no scope change.

## Issues Encountered

- The MFC toggle also set the span visual's HP to minus its maximum; the plan's mark is the specular only, which is what is implemented and measured (33.8 % of the box).
- The markers sit on the z = 0 plane (Pitfall 17): the bridge outline is drawn at ground level, so on the raised deck it appears inside the bridge's sprite rather than round it.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- The group machinery (`SGroupEdit`, `RemoveGroup`/`AddGroup` ordering, `BkEditorPickGroup` kind 2) is ready for entrenchments (04-08): a trench is an entry of sections plus pieces, the same shape.
- Scripted drags may span frames now.

## Threat surface

No new surface beyond the plan's model: empty begin/line/end lists are refused before any index helper (T-04-06-01); every group put is all or nothing and the engine tier checks every bridges link after each step (T-04-06-02); names are length-checked (64), coordinates finite-checked, indexes range-checked at the ABI, and every refusal is shown to change nothing (T-04-06-03).

## Self-Check: PASSED

- Created files found: `MapGeometry.h`, `MapGeometry.cpp`, `session_groups.cpp`, `tools_groups.zig`.
- Commits found: `5d123ca1b`, `1f40b44b6`, `df77c3baf`, `fa6655519`, `e4cca92ee`.
- Acceptance greps: `PlanBridge|BridgePartnerName` in MapGeometry.h 2; engine headers in MapGeometry.cpp 0; `std::min|std::max` in MapGeometry.cpp and session_groups.cpp 0; `BkEditorDrawBridge|PlanBridge|Bridges` in bridge.h 4; `BkEditorPickGroup|DeleteBridge` in bridge.h 2; `SetSpecular` in session.cpp + session_groups.cpp 1; `WoodenBig_Heavy_` in session_groups.cpp 2; `drawBridges` in panels_m2.zig + panels.zig 2; `bridge_delta|bridge_built` in commands.zig 4.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

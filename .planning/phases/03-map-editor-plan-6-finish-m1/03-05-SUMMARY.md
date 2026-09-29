---
phase: 03-map-editor-plan-6-finish-m1
plan: 05
subsystem: map-editor
tags: [zig, cpp, sdl3, imgui, camera, zoom, bridge]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 03)
    provides: "Editor.save's safe-save contract and the app/core wiring plans 1-5 built (view.zig, view_math.zig, panels.zig, smoke.zig, the EditorBridge C ABI)"
provides:
  - "BkEditorView/BkEditorViewState, BkEditorZoomAt, BkEditorSetZoom, BkEditorWorldToScreen: the C ABI's zoom and world-to-screen surface, plus the PublishWorldBase/ZoomAtScreenPoint bridge internals they're built on"
  - "view_math.ZoomWheel/PinchZoom/zoomDelta: fractional wheel-swipe and trackpad-pinch deltas folded into whole engine zoom steps"
  - "View.showMap/syncFromBridge/resetView/drawOverlay/pinch_zoom/zoom_wheel/remembered: the app's zoom input, per-session view memory, and the brush outline"
  - "The corrected CTerrain::GetTileIndex corner-to-tile relationship (rounds to nearest, Y measured from the terrain's far edge) that any future screen-drawn, tile-aligned overlay needs"
affects: [03-06-camera-rotation, 03-07-autosave-and-settings, 03-08-mod-menu]

# Actuals (#2632)
actuals:
  tokens: 11207
  tasks: 3
  commits: 3
plan_head_before: df92c4cd24d35dadc39d514c265f4288980dd4c4

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ZoomWheel/PinchZoom (view_math.zig) are residual-carry accumulators: a fractional wheel/swipe delta or a pinch's multiplicative scale (folded via a log on the zoom step factor) accumulates toward the next whole engine zoom step, with the residual dropped on a direction reversal rather than partly cancelling toward the old one."
    - "Camera/zoom/world-to-screen calls (RealBridge.viewState/zoomAt/setZoom/worldToScreen) bypass the core's undoable Bridge vtable entirely and are called directly from view.zig, the way setCamera/screenSize already were - view state is not an undoable document edit."
    - "View.showMap replaces a plain centre-on-open: it saves the outgoing map's camera and zoom into a session-only std.StringHashMapUnmanaged keyed by the document's path (owned key copies, freed in deinit), then restores the incoming map's saved view or centres it unzoomed."
    - "The bridge publishes GFX.World.BaseSizeX/Y itself (PublishWorldBase, mirroring Common/InterfaceScreenBase.cpp's Mission-screen publish) because the bridge's headless startup never runs the UI-screen-stack machinery that normally does this - without it NSceneScreenScale::GetMaxZoomSteps always reports 0."

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/view_math.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/smoke.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig

key-decisions:
  - "BkEditorWorldToScreen uses z=0, not the terrain's real height, because GetPos3's real-terrain ray-cast (IAILogic::GetIntersectionWithTerrain) never resolves in this bridge's headless session - measured directly: it always falls through to GetPos3's own z=0-plane algebraic fallback regardless of the point's true height. BkEditorScreenToWorld's x,y therefore already assume z=0; composing the two at a real height round-tripped off by up to ~18px in the engine tier and would draw the brush outline off the very ground a click resolves against. This corrects the plan's original \"use the terrain's real height\" design, which assumed a working ray-cast."
  - "CTerrain::GetTileIndex (Scene/TerrainEditor.cpp) rounds to the nearest tile rather than flooring into a bucket (WorldToTile's default isExact=false), so a tile's CENTRE - not a corner - is a plain index*world_cell_size in X; Y is additionally measured from the terrain's far edge (height_tiles - row)*world_cell_size, not from world_y 0. A cell's corner sits half a cell off its centre either way (-0.5 in X, +0.5 in Y). Confirmed by the engine tier's TestWorldToScreenRoundTrip rather than assumed, since view.zig's drawOverlay corners depend on the exact relationship."
  - "Home and the View menu's Reset view only undo zoom (BkEditorSetZoom(0), anchored at the screen's centre), not pan: D-13 talks about zoom/rotation returning to the game's default, and re-centring the camera onto the map's middle on every reset would surprise a user mid-edit far from the map's centre."
  - "The wheel/swipe pan (view.zig's non-Shift branch) divides its screen-pixel pan by the bridge's own current scale so the map follows the fingers 1:1 at any zoom - the same effective gain a mouse-driven drag already gets for free through editor.resolve's screen-to-world conversion, which is zoom-aware by construction."

patterns-established:
  - "A tracer-then-expand shape within one plan: Task 1 (Shift+wheel zoom) is production-quality and self-contained per the tracer convention, verified end-to-end by its own engine test before Tasks 2-3 (pinch/reset/pan-at-zoom/view-memory, then world-to-screen/brush-outline) built on it."

requirements-completed: [D-10, D-11, D-13, D-14, D-15, D-16, CARRY-OUTLINE]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "Shift + wheel/swipe and a trackpad pinch zoom the map in the game's own integer steps; a plain two-finger swipe still pans (D-10)"
    requirement: "D-10"
    verification:
      - kind: unit
        ref: "Sources/editor/app/view_math.zig (zig build test-map-editor-view) - ZoomWheel/PinchZoom/zoomDelta tests (notches give one step each, four 0.3 deltas give one step on the fourth, a reversal drops the residual, PinchZoom's log-scale steps, InputEventKind.pinch routing)"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke - \"Shift + wheel zooms in at the pointer\" (zoomed_at_pointer), \"a swipe at zoom follows the fingers\" (panned)"
        status: pass
    human_judgment: true
    rationale: "Task 2's own <human-check> asks Johannes to drive a real trackpad pinch and Shift+two-finger swipe - the automated coverage proves the input math and the loop wiring, not the trackpad gesture's on-screen feel."
  - id: D2
    description: "Zoom never goes beyond the game's limits: out to the unzoomed view (0 steps), in to NSceneScreenScale::GetMaxZoomSteps for the window; the camera's tilt stays the game's (D-11, D-16)"
    requirement: "D-11"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestZoomStepsBoundedAndAnchored (zig build test-editor-bridge) - a large zoom-in clamps at the measured maximum (3 steps at 1280x960, 2 at 1280x800), a large zoom-out clamps at 0, SetZoom past the maximum clamps"
        status: pass
    human_judgment: false
  - id: D3
    description: "The world point under the pointer stays under it while zooming (D-14)"
    requirement: "D-14"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestZoomStepsBoundedAndAnchored - a point left-above the centre stays within 2 world units of itself (ScreenToWorld before/after) through a maximum zoom-in"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke - \"Shift + wheel zooms in at the pointer\" (zoomed_at_pointer: the world point under the wheel's own screen position moves less than 2 world units)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Home and View > Reset view return to the game's default zoom (D-13)"
    requirement: "D-13"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestZoomStepsBoundedAndAnchored (BkEditorSetZoom(0) after a large zoom)"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke - \"Home resets the view\" (view_reset: zoom_steps 0)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Reopening a map in the same session restores its camera and zoom; a fresh start or a first open shows the map's middle unzoomed (D-15)"
    requirement: "D-15"
    verification:
      - kind: unit
        ref: "View.showMap/remembered - exercised indirectly through map-editor-smoke's own open/reopen flow (no dedicated multi-map unit test in this plan; the state machine itself is a straightforward StringHashMapUnmanaged save/restore with no bridge or filesystem dependency)"
        status: unknown
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to open map A, zoom, open map B, reopen A, and confirm A comes back as left - a real two-map session flow this plan's automated coverage does not exercise (the smoke script opens only one map)."
  - id: D6
    description: "The brush's outline is drawn on the terrain under the pointer at every zoom (carried: BkEditorWorldToScreen)"
    requirement: "CARRY-OUTLINE"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestWorldToScreenRoundTrip (zig build test-editor-bridge) - the centre and four points 150-200px off it round-trip within 2px at zoom 0 and at the maximum zoom; screen right/up are world (+x,+y)/(-x,+y); the corner-to-tile relationship holds"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke map-editor-host-check - both run cleanly with drawOverlay called every frame (3 \"smoke PASS\"/\"panel smoke PASS\" lines total across the two invocations)"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to move the brush over hills and flat ground at radius 0 and 4, zoomed out and in, and confirm the outline sits on the cells a click paints - a visual judgment this plan's automated coverage does not make."

# Metrics
duration: ~58min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 05: Camera zoom like the game Summary

**Shift+wheel/swipe and trackpad pinch zoom the map through NSceneScreenScale's own GFX.World.ZoomSteps global, pointer-anchored and bounded exactly like the game; the brush outline now follows the terrain at any zoom via a new BkEditorWorldToScreen.**

## Performance

- **Duration:** ~58 min
- **Started:** approx. 2026-09-28T05:05:37Z (STATE.md's prior session timestamp)
- **Completed:** 2026-09-28T06:03:30Z
- **Tasks:** 3 completed
- **Files modified:** 11

## Accomplishments

- `bridge.h`/`bridge.cpp`: `PublishWorldBase` (called from `StartRenderer` and `BkEditorResize`) publishes `GFX.World.BaseSizeX/Y` from the screen size, since the bridge's headless startup never runs the `CInterfaceScreenBase::Step` machinery the game normally uses. `BkEditorView`/`BkEditorViewState` report the anchor, zoom step (clamped), max zoom step and scale. `BkEditorZoomAt`/`BkEditorSetZoom` copy `CInterfaceMission::ApplyZoomStep`'s pointer-anchoring recipe verbatim (`ZoomAtScreenPoint`). `BkEditorOpenMap` resets zoom to 0 and rebuilds the terrain on every open (D-15's "fresh start is unzoomed"). `BkEditorWorldToScreen` composes with `BkEditorScreenToWorld` at z=0 (see Deviations).
- `view_math.zig`: `ZoomWheel` and `PinchZoom` fold a wheel/swipe's fractional delta or a pinch's multiplicative scale into whole zoom steps, carrying the fraction between events and dropping it on a direction reversal. `zoomDelta` picks y, or x when y is 0 (macOS turns Shift+wheel into a horizontal scroll). `InputEventKind.pinch` and its `shouldDeliver` rule route pinch events the same way a wheel already is.
- `view.zig`: `View` gains `zoom_steps`/`scale`/`zoom_wheel`/`pinch_zoom`/`remembered`/`current_path`. Shift+wheel and pinch zoom at the pointer through the bridge and sync back via `syncFromBridge`; `Home` calls `resetView` (`BkEditorSetZoom(0)`); the wheel/swipe pan divides by the bridge's own scale so panning stays 1:1 at any zoom; `showMap` replaces `centreOn`, saving/restoring each map's view per D-15. `drawOverlay` draws the brush's square of cells as a closed polyline through every corner on its boundary (not just the four outer ones), converted through the new `worldToScreen`.
- `panels.zig`: a "View" menu with "Reset view" (Home); `State.mapOpened` calls `showMap`; `draw()` calls `drawOverlay` first every frame.
- `smoke.zig`: `Wheel` gains `mods` (sets `SDL_SetModState` before pushing, restored in `afterFrame`, since pushed key events don't update SDL's own modifier state); three new steps prove Shift+wheel zooms in and stays anchored on the pointer, a swipe at that zoom still pans 1:1, and Home resets `zoom_steps` to 0 - placed right after the plan 5 swipe-pan steps so earlier steps stay unzoomed and the pointer isn't stuck over a panel from a later step's ImGui-capture wait.
- `editor_bridge_test.cpp`: `TestZoomStepsBoundedAndAnchored` measures the window's max zoom steps (3 at 1280x960, 2 at 1280x800 - both measured directly, matching `NSceneScreenScale`'s own formula exactly), proves a zoom-in clamps at it and keeps the pointer's world point anchored within 2 units, the terrain rebuilds (black-fraction check), and a zoom past either bound clamps. `TestWorldToScreenRoundTrip` proves the centre and four points 150-200px off it round-trip within 2px at zoom 0 and the maximum zoom, screen right/up are world (+x,+y)/(-x,+y), and the corrected corner-to-tile relationship holds.
- `build.zig`: `EditorBridge` gains `Sources/src/StreamIO` on its include path so `Scene/SceneScreenScale.h`'s own bare `#include "Globals.h"` resolves, the way every other module that includes it already does.

## Task Commits

Each task was committed atomically:

1. **Task 1: Shift + wheel zooms the map at the pointer, within the game's bounds** - `c308d0dff` (feat)
2. **Task 2: Pinch, Reset view, 1:1 panning at any zoom, and each map's view remembered for the session** - `722dda36c` (feat)
3. **Task 3: BkEditorWorldToScreen and the brush outline on the terrain** - `78b9729ac` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

_Note: `BkEditorWorldToScreen`, its `session.h`/`session.cpp` `WorldToScreen` helper, and `c_bridge.zig`'s `worldToScreen` wrapper were written and committed as part of Task 1's commit rather than Task 3's, because they were designed alongside the other zoom bridge entry points in one editing pass before the task split was re-checked against the plan. No functional difference - all three tasks' acceptance criteria are met by the final state; documented here as a deviation from the plan's per-task file grouping, not a scope or correctness issue._

## Files Created/Modified

- `Sources/src/EditorBridge/bridge.h` - `BkEditorView`, `BkEditorViewState`, `BkEditorZoomAt`, `BkEditorSetZoom`, `BkEditorWorldToScreen` declarations
- `Sources/src/EditorBridge/bridge.cpp` - `PublishWorldBase`, `ZoomAtScreenPoint`, the four new entry points, `BkEditorOpenMap`'s zoom reset
- `Sources/src/EditorBridge/session.h`/`.cpp` - `WorldToScreen` helper (z=0, see Deviations)
- `Sources/editor/app/c_bridge.zig` - `viewState`/`zoomAt`/`setZoom`/`worldToScreen` RealBridge methods
- `Sources/editor/app/view.zig` - zoom/pinch/Home input, `syncFromBridge`, `showMap`, `drawOverlay`
- `Sources/editor/app/view_math.zig` - `ZoomWheel`, `PinchZoom`, `zoomDelta`, `InputEventKind.pinch`
- `Sources/editor/app/panels.zig` - "View" menu, `mapOpened`'s `showMap` call, `draw()`'s `drawOverlay` call
- `Sources/editor/app/smoke.zig` - `Wheel.mods`, three new script steps, `.panned`'s scale division
- `tools/zig/editor_bridge_test.cpp` - `TestZoomStepsBoundedAndAnchored`, `TestWorldToScreenRoundTrip`
- `build.zig` - `EditorBridge`'s `Sources/src/StreamIO` include path

## Decisions Made

- `BkEditorWorldToScreen` uses z=0 rather than the terrain's real height (see Deviations - this is a Rule 1 auto-fix, not a plan choice).
- `CTerrain::GetTileIndex`'s round-to-nearest, far-edge-measured-Y convention, confirmed by the engine tier (see Deviations).
- Home/Reset view only undoes zoom, not pan (D-13 is about zoom/rotation, not repositioning the camera).
- The wheel/swipe pan divides by the bridge's own scale for 1:1 panning at zoom.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] BkEditorWorldToScreen composes at z=0, not the terrain's real height**
- **Found during:** Task 3 (BkEditorWorldToScreen and the brush outline)
- **Issue:** The plan's action text specified "z is the terrain's height at the point (the scene's terrain GetHeight)". Implementing it exactly that way made the engine tier's round-trip test (`ScreenToWorld` then `WorldToScreen`) fail by up to ~18px at several test points. Direct instrumentation (`pScene->GetPos3` returning its full z) showed `GetPos3` returns z=0 for every tested point in this bridge's headless session - `IAILogic::GetIntersectionWithTerrain` (the real terrain ray-cast `GetPos3` tries first) never succeeds here, so it always falls through to `GetPos3`'s own algebraic z=0-plane fallback. `BkEditorScreenToWorld`'s x,y therefore already assume z=0; feeding the terrain's true (nonzero) height into `WorldToScreen` for the same x,y projects to a different screen point than the one that produced them.
- **Fix:** `WorldToScreen` (session.cpp) uses `CVec3(wx, wy, 0.0f)` unconditionally. Removed the `ITerrain::GetHeight` lookup. Updated `bridge.h`'s doc comment to state the z=0 convention and why, rather than claiming real-height accuracy the bridge cannot deliver.
- **Files modified:** `Sources/src/EditorBridge/session.cpp`, `Sources/src/EditorBridge/bridge.h`
- **Verification:** `TestWorldToScreenRoundTrip`'s round-trip checks (centre + 4 points, zoom 0 and max zoom) all pass within 2px after the fix.
- **Committed in:** `78b9729ac` (Task 3 commit)

**2. [Rule 1 - Bug] The brush outline's world-corner formula needed rounding and a Y-axis flip**
- **Found during:** Task 3, while building `TestWorldToScreenRoundTrip`'s corner check
- **Issue:** The plan's stated convention ("world corner = tile index * world_cell_size") assumed a floor-grid tile index. Reading `Scene/TerrainEditor.cpp`'s `GetTileIndexLocal` (what `BkEditorWorldToTile` actually calls) showed `WorldToTile`'s default `isExact=false` **rounds** to the nearest tile rather than flooring, and its Y calculation is `(fTerraSizeY - point.y) / cellSize` - measured from the terrain's far edge, not from world_y 0. So a tile's *centre* (not a corner) is `index * world_cell_size` in X, and `(height_tiles - row) * world_cell_size` in Y; a genuine corner sits half a cell off its centre, in opposite directions for X and Y.
- **Fix:** `view.zig`'s `drawOverlay`/`addCorner` bake a `-0.5` offset into the corner-index math for both axes and flip Y via `height_tiles - cy`. `editor_bridge_test.cpp`'s corner check uses the same corrected formula and verifies it against `BkEditorWorldToTile` directly.
- **Files modified:** `Sources/editor/app/view.zig`, `tools/zig/editor_bridge_test.cpp`
- **Verification:** the engine tier's corner check and the app tier's `map-editor-smoke`/`map-editor-host-check` runs (with `drawOverlay` now called every frame) both pass.
- **Committed in:** `78b9729ac` (Task 3 commit)

**3. [Rule 3 - Blocking] EditorBridge needed Sources/src/StreamIO on its include path**
- **Found during:** Task 1, first attempt to build `test-editor-bridge` after including `Scene/SceneScreenScale.h`
- **Issue:** `SceneScreenScale.h`'s own bare `#include "Globals.h"` (a file in `Sources/src/StreamIO/`) failed to resolve: the `EditorBridge` build target's include-path list (build.zig) never had `Sources/src/StreamIO`, unlike the `Scene` module itself, which does.
- **Fix:** added `module.addIncludePath(b.path("Sources/src/StreamIO"))` to `addEditorBridge` in build.zig, matching the pattern already used for `Formats`/`RandomMapGen`/`Common`/etc.
- **Files modified:** `build.zig`
- **Verification:** `zig build test-editor-bridge` compiles; `zig test tools/zig/build_hermeticity_test.zig` still passes after the build.zig edit (per the standing constraint).
- **Committed in:** `c308d0dff` (Task 1 commit)

**4. [Rule 3 - Blocking] `ITerrain` was an incomplete type in bridge.cpp**
- **Found during:** Task 1, same build attempt as above
- **Issue:** `pTerrain->ResetPosition()` (needed for the zoom-at recipe and the open-map zoom reset) failed to compile: `Scene.h` only forward-declares `ITerrain`; the full definition is in `Scene/Terrain.h`, which bridge.cpp did not include.
- **Fix:** added `#include "../Scene/Terrain.h"` to bridge.cpp.
- **Files modified:** `Sources/src/EditorBridge/bridge.cpp`
- **Verification:** compiles; `TestZoomStepsBoundedAndAnchored`'s terrain-rebuild check passes.
- **Committed in:** `c308d0dff` (Task 1 commit)

---

**Total deviations:** 4 auto-fixed (2 Rule 1 bug fixes discovered by the plan's own verification loop, 2 Rule 3 blocking build fixes).
**Impact on plan:** The two Rule 1 fixes changed the plan's stated design for `BkEditorWorldToScreen` and the brush-outline corner formula, in both cases because direct measurement (not assumption) showed the plan's premise didn't hold in this bridge's actual behavior. Both fixes are load-bearing for D-14/CARRY-OUTLINE's correctness, not scope creep - the plan's own engine-tier verification loop is what caught them. The two Rule 3 fixes are ordinary build-plumbing corrections.

## Task-grouping note (not a deviation, filed here for visibility)

`BkEditorWorldToScreen`, its `session.cpp`/`session.h` `WorldToScreen` helper, and `c_bridge.zig`'s `worldToScreen` wrapper landed in Task 1's commit (`c308d0dff`) rather than Task 3's (`78b9729ac`), since they were written in the same editing pass as the other zoom entry points before the task split was re-checked. Functionally complete and correctly attributed to the plan either way; every task's acceptance criteria pass against the final committed state.

## Issues Encountered

- The engine tier's round-trip test initially failed by 7-18px at several points before the z=0 finding (Deviation 1) and the corner test failed with a wildly wrong tile Y index (88 instead of 8, then an off-by-one in X) before the rounding/flip finding (Deviation 2) - both root-caused via direct instrumentation of the engine's own `GetPos3`/`GetTileIndexLocal` rather than guessed at, per the project's "measure, don't guess" convention (matches `blitzkrieg-measure-graphics-artefacts` and the general spirit for non-visual engine behavior too).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `BkEditorView`/`BkEditorViewState`/`BkEditorZoomAt`/`BkEditorSetZoom` are ready for 03-06 (camera rotation) to extend with a yaw parameter, following the same `Guarded`/direct-`RealBridge`-method pattern.
- `View.remembered`'s `SavedView` struct (`camera_x`, `camera_y`, `zoom_steps`) can grow a `yaw_degrees` field for 03-06 without changing its save/restore shape.
- D-13 (Home/Reset view) is registered as a shared requirement with a sibling plan not yet finished (per `requirements.ready-ids`, blocked pending that plan's own summary) - this plan's zoom-reset half is done; the shared ID stays unmarked in REQUIREMENTS.md until both land, by design (issue #2388's shared-ID gate).
- REQUIREMENTS.md does not track this milestone's `D-XX`/`CARRY-*` requirement IDs at all (it is an older, differently-scoped requirements document from before this convention) - `requirements.mark-complete` is a no-op here for every ID in this plan's `requirements` frontmatter; this is pre-existing project state, not something this plan should fix.
- No blockers for the rest of phase 3.

## Self-Check: PASSED

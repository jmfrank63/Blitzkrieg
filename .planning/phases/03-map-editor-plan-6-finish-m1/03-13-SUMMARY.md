---
phase: 03-map-editor-plan-6-finish-m1
plan: 13
subsystem: map-editor
tags: [zig, cpp, sdl3, editor-bridge, testing]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 12)
    provides: "The full editor app/core/bridge stack plans 1-12 built, including BK_EDITOR_AUTO and the app-tier close-out"
provides:
  - "map-editor-host-check gated on test_mode (compile builds without running); NoDevice prints a skip line and exits 0"
  - "The host check's probe is orange (255,128,0) with an asymmetric channel check, so a red/blue readback swap fails instead of passing"
  - "Tga.pixel bounds-checked (returns null off-image) so a tiny capture fails with a FAIL line instead of an out-of-bounds panic"
  - "BkEditorCaptureFrame disarms CaptureNextFrame on the read-back failure path too, and gives that path its own message instead of one line covering an allocation failure and a readback failure alike"
  - "BkEditorResize refuses a high-pixel-density window with BK_EDITOR_BAD_ARGUMENT (FollowWindowSize only resizes in points)"
  - "StartRenderer keeps a window on the display it actually opened on (KeepWindowOnItsOwnDisplay), measured directly against a real second display rather than assumed"
  - "TestEntryPointsBeforeAMap: the engine tier's first test, covering NO_SESSION (49 entry points) and REFUSED \"no map is open\" (29 entry points) before any map is open"
  - "OtherTilesetTile: paint tests take a tile the open map's own tileset actually offers, not a map-dependent (tile+1)%4 guess"
  - "TestTerrainUnderTheCamera checks two anchors from the engine-confirmed pick set"
affects: [03-14-packaging, 03-15-exit-criteria-sweep]

# Actuals (#2632)
actuals:
  tokens: 11053
  tasks: 3
  commits: 3
plan_head_before: 39b9239c9967709a2ab282e1991ec84e7ec57078

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "A tracer task (Task 1) verified end-to-end before expansion tasks (2, 3) touched the same host-check/bridge surface, per the tracer feedback gate."
    - "KeepWindowOnItsOwnDisplay publishes GFX.Monitor.Index from the caller's own window (SDL_GetDisplayForWindow) before SetMode, the same global a profile's own GFX.Monitor setting would set - measured with a throwaway instrumented build against this machine's real second display (an AirPlay-mirrored TV), not assumed from reading SelectedDisplay's source alone."
    - "OtherTilesetTile queries BkEditorTilesetTiles for a tile that differs from the cell's current one, replacing a map-dependent (current+1)%4 guess that could land on tile 1 (in no shipped tileset) across three call sites."
    - "TestEntryPointsBeforeAMap is a table-driven test (std::function<BkEditorStatus()> per entry point) rather than one Check per function, so ~80 entry-point contracts fit in a maintainable, print-a-tally form."

key-files:
  created: []
  modified:
    - build.zig
    - Sources/editor/app/main.zig
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/session.cpp
    - tools/zig/editor_bridge_test.cpp

key-decisions:
  - "The probe is orange (255,128,0), not any other asymmetric colour: magenta (255,0,255) was replaced because swapping its own R/B channels gives back magenta - the exact readback bug the check exists to catch would still pass a magenta check. Orange's three channels are all different, so any channel swap moves the reading outside isProbeColour's range."
  - "IGFX (Sources/src/GFX/GFX.H) has no accessor for GraphicsEngineGpu::fail()'s own last_error_ string - it is a private member with no virtual method exposing it, and both are read-only via BK_GFX_TRACE's stderr trace, never through the interface. Adding one would mean a new virtual method on IGFX, implemented by every backend (GraphicsEngineGpu and the legacy CGraphicsEngine) - an architectural change outside a carried-minors bridge fix. BkEditorCaptureFrame's read-back branch instead gives its two distinct causes (image allocation vs. readback) their own messages, the most specific reachable at the bridge layer without that change. The must-have truth \"keeps the renderer's own reason\" is met in spirit (a specific, not generic, reason) but not literally (the renderer's own internal string) - flagged here rather than claimed as full."
  - "The second-monitor jump (Task 1's carried line) was verified with a real live measurement, not just source-reading: a throwaway instrumented build of host.zig (never committed - reverted after each measurement) moved the hidden host-check window onto this development machine's second display (a real AirPlay-mirrored TV, matching Johannes's own Monitor2 setup per project memory) before BkEditorStart, and printed SDL_GetDisplayForWindow before and after. Before the fix: display 2 before, display 1 after. After KeepWindowOnItsOwnDisplay: display 2 both times."
  - "TestTerrainUnderTheCamera's second anchor is drawn from the pick set (TestObjectUnderTheCursor's own engine-confirmed first-20-objects scan), not literally \"first and last object in file order\" as first tried: the shipped map's last object by file order sits at the map's edge and measured 79.2% black in the lower half, a false positive for a check whose whole point is \"the ground is drawn, not the map running out under the camera\"."
  - "TestUnknownObjectDoesNotStopTheOpen no longer deletes its scratch copy (orchestrator-authorized fold-in of a known leftover, not originally in this task's action text): main.zig's own host check (03-11) opens this file afterward to prove its unknown-objects warning, found via dirname(output). The `remove()` on its literal-backslash path succeeded on Windows (removing the fixture) and silently failed on macOS (POSIX remove() does not split on '\\\\'), so the host check printed PASS on one platform and \"skipped\" on the other for a reason unrelated to whether the warning itself worked. Keeping the file unconditionally makes both platforms agree; it is a throwaway scratch file in zig-out/local-test either way."

requirements-completed: [CARRY-MINORS]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "The host check honours -Dtest-mode (compile builds without running), skips (not fails) on a runner with no GPU device, and its probe check catches a red/blue channel swap in the readback; Tga.pixel is bounds-checked"
    requirement: "CARRY-MINORS"
    verification:
      - kind: integration
        ref: "zig build map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run - 3 \"host check PASS\" lines, orange probe check passes"
        status: pass
      - kind: integration
        ref: "zig build map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=compile - builds the step without running it (rc=0, no PASS/FAIL lines, shader compile only)"
        status: pass
      - kind: unit
        ref: "grep -n \"host check skipped: no GPU device\" Sources/editor/app/main.zig"
        status: pass
    human_judgment: false
  - id: D2
    description: "BkEditorCaptureFrame disarms the capture on the read-back failure path too and gives it a specific message; BkEditorResize refuses a high-pixel-density window; StartRenderer keeps a window on the display it opened on; PaintTilesInTileset documents its cost; bridge.h's BkEditorWorldToTile comment rewrapped"
    requirement: "CARRY-MINORS"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge map-editor-host-check map-editor-smoke -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run - 8 PASS lines, no FAIL"
        status: pass
      - kind: unit
        ref: "grep -n \"CaptureNextFrame( false )\" Sources/src/EditorBridge/bridge.cpp - exactly two call sites"
        status: pass
      - kind: unit
        ref: "grep -n \"high pixel density\" Sources/src/EditorBridge/bridge.cpp"
        status: pass
      - kind: manual_procedural
        ref: "Live SDL measurement (throwaway, not committed): a window moved to display 2 stayed on display 2 through BkEditorStart after the fix, versus jumping to display 1 before it - see key-decisions"
        status: pass
    human_judgment: false
  - id: D3
    description: "TestEntryPointsBeforeAMap runs first and covers NO_SESSION/REFUSED-before-start paths; the overlay call count is exact; paint tests use a tileset-derived tile; TestTerrainUnderTheCamera checks a second anchor; PICK_RISE's stale comment and the pick-ratio wrap are fixed; ReadFramePixels validates the TGA header; every plan-5 carried item is accounted for"
    requirement: "CARRY-MINORS"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run - \"editor-bridge: PASS\", 49/49 NO_SESSION, 29/29 \"no map is open\", 0 FAIL lines"
        status: pass
      - kind: unit
        ref: "grep -c \"tile + 1 ) % 4\" tools/zig/editor_bridge_test.cpp == 0; grep -c \"OtherTilesetTile\" tools/zig/editor_bridge_test.cpp == 4"
        status: pass
    human_judgment: true
    rationale: "The plan-5 carried-list ledger (below, in the body) includes three app-tier items (Task 4's status-line-never-cleared, Task 4's untested event-to-tool wiring, Task 6's restated scroll-direction constants) that are not in this plan's bridge/engine-tier scope and were not closed by 03-05, 03-11, 03-12 either - a human should confirm whether they are worth a follow-up plan or acceptable as known gaps before M1 is called fully closed."

# Metrics
duration: ~58min
completed: 2026-09-29
status: complete
---

# Phase 3 Plan 13: Bridge and engine-tier carried minors from plan 5 Summary

**The host check honours test_mode and a real GPU-less skip, its orange probe now catches a red/blue readback swap, the bridge disarms a failed capture and keeps a window on the display it opened on (measured live against a real second display), and the engine tier gained a first-run entry-point contract test covering ~80 NO_SESSION/REFUSED/BAD_ARGUMENT checks.**

## Performance

- **Duration:** ~58 min
- **Started:** 2026-09-28T16:50:00Z (approx.)
- **Completed:** 2026-09-28T17:49:54Z
- **Tasks:** 3 completed
- **Files modified:** 6

## Accomplishments

- `build.zig`: `map-editor-host-check`'s run steps (the relative, absolute-path and `-mod=` invocations) are a dependency only when `test_mode == .run`; the step otherwise depends on installing the executable, matching `test-editor-bridge`'s own compile-mode behaviour.
- `main.zig`: `Host.start`'s `NoDevice` prints `map-editor: host check skipped: no GPU device (<reason>)` and exits 0; the probe window is orange (255,128,0) with an asymmetric-channel check (`r>200, 100<g<160, b<40`) that a magenta probe could never fail under a red/blue swap; `Tga.pixel` returns null off-image, and both callers print a FAIL line naming the point and the capture's size instead of panicking.
- `bridge.cpp`/`bridge.h`: `BkEditorCaptureFrame`'s read-back failure branch now disarms `CaptureNextFrame(false)` (previously only the earlier `DrawSessionFrame` failure did) and distinguishes an allocation failure from a readback failure instead of one generic message; `BkEditorResize` refuses with `BK_EDITOR_BAD_ARGUMENT` and a named reason when `SDL_GetWindowSizeInPixels` differs from the window's point size; `StartRenderer` gained `KeepWindowOnItsOwnDisplay`, which publishes the caller's window's own display as `GFX.Monitor.Index` before `SetMode` - measured live (not assumed) to stop a second-monitor window jumping to display 1; `BkEditorWorldToTile`'s doc comment rewrapped to the file's usual width.
- `session.cpp`: `PaintTilesInTileset`'s doc comment now states its cost (cells x terrain types x tiles), why a brush is fine today, and what a future fill tool would want instead - no behaviour change.
- `tools/zig/editor_bridge_test.cpp`: new `TestEntryPointsBeforeAMap`, run first (right after the start, before any map is open) - a null session answers `BK_EDITOR_NO_SESSION` for 49 entry points, 29 map-needing entry points answer `BK_EDITOR_REFUSED` naming `"no map is open"`, and a representative set of null outputs answer `BK_EDITOR_BAD_ARGUMENT`. `TestOverlayDeviceAndSize` checks the overlay's call count exactly (`== 3` for 3 presents) and confirms it still runs in the frame after a `BkEditorResize`. New `OtherTilesetTile(pSession, current)` replaces the map-dependent `(tile+1)%4` guess in `TestPaintUndoIsExact` and `TestPaintAtTheEdgeAndRefused` (three call sites). `TestTerrainUnderTheCamera` checks two anchors drawn from the engine-confirmed pick set (not an arbitrary map index - see key-decisions). `PICK_RISE`'s stale comment and the pick-ratio comment's broken wrap are fixed. `ReadFramePixels` validates the TGA's type, bit depth, descriptor and byte length before reading, printing a reason instead of reading a mismatched file as pixel noise. `TestUnknownObjectDoesNotStopTheOpen` no longer deletes its scratch fixture (see key-decisions).

## Task Commits

Each task was committed atomically:

1. **Task 1: The host check honours test_mode, skips without a GPU, and sees a colour swap** - `3c413c2ec` (feat)
2. **Task 2: Bridge minors - capture disarm and message, resize at high density, second-monitor start, paint cost note** - `f35c52038` (fix)
3. **Task 3: Engine-tier test minors - contracts before a start or a map, overlay in a resize, map-independent paints, a second anchor** - `1ff935a6e` (test)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

## Files Created/Modified

- `build.zig` - `map-editor-host-check`'s run-step dependency gated on `test_mode == .run`
- `Sources/editor/app/main.zig` - NoDevice skip line, orange probe + `isProbeColour`, bounds-checked `Tga.pixel`
- `Sources/src/EditorBridge/bridge.cpp` - `KeepWindowOnItsOwnDisplay`, `BkEditorCaptureFrame`'s disarm-and-message fix, `BkEditorResize`'s HiDPI guard
- `Sources/src/EditorBridge/bridge.h` - `BkEditorWorldToTile`'s doc comment rewrapped
- `Sources/src/EditorBridge/session.cpp` - `PaintTilesInTileset`'s cost comment
- `tools/zig/editor_bridge_test.cpp` - `TestEntryPointsBeforeAMap`, `OtherTilesetTile`, exact overlay count + post-resize check, `TestTerrainUnderTheCamera`'s second anchor, `PICK_RISE`/pick-ratio comment fixes, `ReadFramePixels`'s header validation, `TestUnknownObjectDoesNotStopTheOpen`'s kept fixture

## Decisions Made

See `key-decisions` in the frontmatter above.

## Plan-5 Carried-List Ledger

Every line from plan 5's "Carried to plan 6" list (`docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md`), with its outcome, so none is left unaccounted (Task 3's own acceptance criterion):

| Carried item | Outcome |
|---|---|
| Spec, Errors -> Open: unknown-object warning | **Closed in 03-11** (unknown-objects modal) |
| Packaging: Windows console subsystem | **Pending 03-14** (not yet executed) |
| Task 1: no tests of BAD_ARGUMENT/REFUSED-before-start paths | **Closed here** (`TestEntryPointsBeforeAMap`) |
| Task 1: overlay check is `>= 1`, not `== 3` | **Closed here** (exact overlay count + post-resize check) |
| Task 1: a long line in bridge.h | **Closed here** (`BkEditorWorldToTile` rewrapped) |
| Task 1: BkEditorStart may jump a second-monitor window to display 0 | **Closed here** (`KeepWindowOnItsOwnDisplay`, measured live) |
| Task 1: FollowWindowSize works in points only | **Closed here** (HiDPI `BkEditorResize` guard) |
| Task 2: a skipped frame leaves the capture armed | **Closed here** (disarm on both `BkEditorCaptureFrame` failure paths) |
| Task 2: BkEditorCaptureFrame replaces a specific renderer message with a generic one | **Closed here, with a documented limit** - IGFX has no accessor for the renderer's own message; the two distinct causes get their own bridge-composed messages instead (see key-decisions) |
| Task 2: the magenta probe cannot see an R/B swap | **Closed here** (orange probe) |
| Task 2: map-editor-host-check ignores test_mode and fails without a GPU | **Closed here** (test_mode gating + NoDevice skip) |
| Task 2: Tga.pixel has no bounds check | **Closed here** (bounds-checked, returns null) |
| Task 3: message_len is written but never read | **Closed in 03-11** (`RealBridge.message_len` removed) |
| Task 3: PaintTilesInTileset's per-paint cost | **Closed here** (documented, no behaviour change) |
| Task 3: the older paints pick `(original+1)%4`, map-dependent | **Closed here** (`OtherTilesetTile`) |
| Task 4: the status line is never cleared after a later success | **Still open** - not in this plan's bridge/engine-tier scope; not closed by 03-05, 03-11 or 03-12 either |
| Task 4: a middle-button release ImGui takes can leave panning stuck | **Closed in 03-11** (stale-gesture guard) |
| Task 4: no test of view.zig's event-to-tool wiring beyond the routing function | **Still open** - same as above |
| Task 4: view.zig's gesture handling has no guard of its own | **Closed in 03-11** (the same stale-gesture guard) |
| Task 4: wheel events are routed but not handled (zoom out of M1) | **Closed in 03-05** (D-10 Shift+wheel/pinch zoom) |
| Task 5: a selection change while typing makes a no-op second commitEdit | **Closed in 03-11** (`commitEdit`'s equal-pose guard) |
| Task 5: the brush drops the bridge's own "no tiles" reason | **Closed in 03-11** (`tileReason`/`chooseBrushTile`) |
| Task 5: `std.sort.insertion` over ~5.5k catalogue entries | **Closed in 03-11** (`std.sort.block`) |
| Task 5: `pickDefaultPlacerObject` reads the catalogue again | **Closed in 03-11** (`State.defaultPlacerObject`) |
| Task 5: a second Open/Save As while a dialog is up is dropped silently | **Closed in 03-11** (`Step.dialog_busy`) |
| Task 5: the right-hand panels do not follow a resize; stale hover | **Closed in 03-11** (resize-following `beginPanel`, hover clear) |
| Task 6: a stale PICK_RISE comment and a broken pick-ratio wrap | **Closed here** |
| Task 6: the scroll-direction unit test restates its own constants | **Still open** - same as above |
| Task 6: the terrain regression check uses one anchor only | **Closed here** (second anchor from the pick set) |
| Task 7: the smoke sets the brush tile directly, palette path untested | **Closed in 03-11** (`chooseBrushTile` shared by both) |
| Task 7.1: the smoke's mouse is not isolated from the real cursor | **Closed in 03-12**, with a documented limit (`bk_imgui_backend_use_global_mouse` cannot fully disable ImGui's private fallback) |
| Task 7.1: `prepare` does not check `drag_via`/`drag_to` are clear | **Closed in 03-12** |
| Task 7.1: `ReadFramePixels` assumes the TGA layout without checking | **Closed here** (header + byte-length validation) |
| Task 7.1: bridge.h unit wording and a long line | **Closed here** (`BkEditorWorldToTile` rewrapped; the exact historical line numbers no longer apply after 12 plans of edits, so the current equivalent was rewrapped instead) |

**Three items remain open** (Task 4's status-line-clear, Task 4's event-to-tool wiring test, Task 6's scroll-direction constant restatement) - none is in this plan's bridge/engine-tier scope, and none was closed by 03-05, 03-11 or 03-12. Flagged rather than silently dropped; a human should decide whether they warrant a follow-up plan or are acceptable as known gaps.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 4-adjacent, orchestrator-authorized] Kept `TestUnknownObjectDoesNotStopTheOpen`'s scratch fixture instead of deleting it**
- **Found during:** Task 3, while reading the orchestrator's own note about this exact inconsistency (not in the plan's own action text for Task 3)
- **Issue:** `remove()` on the test's literal-backslash scratch path succeeds on Windows (deleting the fixture the 03-11 host check needs) and silently fails on macOS (POSIX `remove()` does not treat `\` as a separator) - the host check's own unknown-object warning check passed on one platform and printed "skipped" on the other for a platform quirk, not a real difference in whether the warning worked.
- **Fix:** Removed the `remove()` call; the file is now kept unconditionally on every platform (it is a throwaway scratch file under `zig-out/local-test` either way).
- **Files modified:** `tools/zig/editor_bridge_test.cpp`
- **Verification:** `zig build test-editor-bridge map-editor-host-check map-editor-smoke` - the unknown-object warning line reads `PASS` on every host-check invocation, not "skipped".
- **Committed in:** `1ff935a6e` (Task 3 commit)

---

**Total deviations:** 1 (an orchestrator-explicitly-authorized fold-in of a known leftover, not a self-directed rule 1-3 fix).
**Impact on plan:** No scope creep - the orchestrator's dispatch prompt named this exact leftover and explicitly authorized folding it in if it fit this plan's bridge/engine-tier scope, which it does (same file, same test).

## Issues Encountered

- `BkEditorCaptureFrame`'s read-back message fix does not literally surface "the renderer's own message" as the plan's action text describes: `IGFX` has no accessor for `GraphicsEngineGpu::fail()`'s internal `last_error_` string (private, no virtual method, confirmed by reading the full `GFX.H` interface). Adding one would need a new virtual method implemented by every `IGFX` backend - out of a carried-minors bridge fix's scope. The fix gives the two distinct failure causes their own bridge-composed messages instead, the most specific achievable without that change. Documented in key-decisions rather than silently claimed as a full fix.
- The first attempt at `TestTerrainUnderTheCamera`'s second anchor (the map's literal last object by file order) failed with 79.2% black in the lower half - that object sits at the map's edge, where the camera legitimately runs out of ground. Caught by the plan's own verify loop before any commit; fixed by drawing both anchors from `TestObjectUnderTheCursor`'s own engine-confirmed pick set instead (see key-decisions). Not a deviation from the plan's intent (still "a second anchor from the pick set," exactly as asked) - just a correction to which object counts as "from the pick set."

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- 03-14 (packaging, Windows console subsystem) and 03-15 (exit-criteria sweep) are unaffected by this plan's changes - `host-check`, `test-editor-bridge` and `map-editor-smoke` all still pass after every task's commit.
- The plan-5 carried list has three items still open (see ledger above) - not blocking, but worth a decision (fold into 03-15, a future plan, or accept as known gaps) before M1 is called fully closed.
- `KeepWindowOnItsOwnDisplay`'s live measurement was done with a throwaway, uncommitted patch to `Sources/editor/app/host.zig` (reverted after each measurement, confirmed via `git status --short` and `git diff --stat` showing no diff) - the technique (temporarily instrument, measure, revert, then fix the real files) is reusable for any future "measure a real platform behaviour, don't assume it" carried item.

## Self-Check: PASSED

- `Sources/editor/app/main.zig` exists and contains the NoDevice skip line: FOUND
- `Sources/src/EditorBridge/bridge.cpp` exists and contains `KeepWindowOnItsOwnDisplay`: FOUND
- `tools/zig/editor_bridge_test.cpp` exists and contains `TestEntryPointsBeforeAMap`: FOUND
- Commit `3c413c2ec` exists in history: FOUND
- Commit `f35c52038` exists in history: FOUND
- Commit `1ff935a6e` exists in history: FOUND
- `plan_head_before`: `39b9239c9967709a2ab282e1991ec84e7ec57078`; `git rev-list --count 39b9239c9..HEAD` = 3 (matches `actuals.commits`)
- `zig build test-editor-bridge map-editor-host-check map-editor-smoke -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`: rc=0, all PASS lines present, no FAIL
- `zig build map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=compile`: rc=0
- `zig test tools/zig/build_hermeticity_test.zig`: 3/3 passed

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-29*

---
phase: 03-map-editor-plan-6-finish-m1
plan: 06
subsystem: map-editor
tags: [zig, cpp, sdl3, camera, yaw, rotation, measurement, bridge]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 05)
    provides: "BkEditorView/BkEditorViewState/BkEditorZoomAt/BkEditorSetZoom camera and zoom surface, Home/Reset view (zoom-only) that this plan's yaw offset threads alongside"
provides:
  - "BkEditorSetYaw / SEditorSession::fYawOffsetDegrees: a C ABI camera-yaw offset from the game's fixed 45 degrees, wrapped into [0,360), non-finite rejected"
  - "Engine-tier TestYawMeasurement and five editor-bridge-yaw-<offset>.tga captures: a measured (not guessed) record of what the renderer draws at yaw offsets 0/30/90/180/270"
  - "A recorded, Johannes-confirmed decision that D-12 (free 360 degree camera rotation) is deferred out of M1, with the evidence, now in 03-CONTEXT.md's deferred list"
affects: [03-07-autosave-and-settings, any future M2/M3 phase revisiting camera rotation or terrain rendering]

# Actuals (#2632)
actuals:
  tokens: 7750
  tasks: 1
  commits: 3
plan_head_before: 958e7df0867d8ad9908362ec6cffdf8e7308ce31

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Measure-before-build gate: a tracer task (Task 1) wires the smallest possible surface (a yaw offset into SetSessionCamera) purely to produce evidence, before any checkpoint:decision or input-facing Task 3 - the project's 'measure graphics, not guess' rule applied to a whole feature, not just a single artefact."
    - "TestYawMeasurement's three independent signals (black-fraction of the lower half, object picking, terrain-agreement between pick and draw) deliberately do not agree: picking agreed with the terrain at every yaw offset because both walk the same unrotated projection, so picking alone is not a sufficient correctness check for 'does the renderer draw this yaw correctly' - only the rendered pixels (black fraction, and the TGAs themselves) caught the mismatch."

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - tools/zig/editor_bridge_test.cpp
    - .planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md

key-decisions:
  - "D-12 (free 360 degree camera rotation) is deferred out of M1. Decided by Johannes at the Task 2 checkpoint after reviewing Task 1's measurements: at yaw +30 the terrain is already clipped along a hard diagonal (9.4% of the lower half black, visible in editor-bridge-yaw-30.tga/png), and at +90/+180/+270 the terrain is almost entirely gone (94.0%, 99.2%, 89.8% black) while billboard tree/building sprites stay upright and fixed in their yaw-0 screen positions with no ground left under them. Root cause read from source, then confirmed by measurement: CTerrain::MovePatches (Scene/TerrainInternal.cpp:237-256) lays terrain patches out on a fixed isometric screen grid rather than through the view matrix, and buildings/infantry are single-direction SGVOT_SPRITE billboards (Main/GameDB.h:13-19) that cannot show another face. Delivering D-12 correctly needs terrain rendering rewritten to go through the view matrix - Scene/ engine work outside M1's scope - and sprites still could not show a back side regardless."
  - "Task 3 (Alt+Q/E, trackpad two-finger rotate, yaw-aware Camera.scroll/panScreen, smoke steps) is skipped per its own precondition: it only runs if Task 2 chose 'build'. Its files (Sources/editor/app/c_bridge.zig, view.zig, view_math.zig, host.zig, smoke.zig) are untouched by this plan."
  - "D-13 (Home / View > Reset view resets rotation and zoom) needs no new work here: plan 5 already built Home/Reset view as a zoom-only reset (see 03-05-SUMMARY.md decisions), and with D-12 deferred there is no rotation state for it to reset - the existing zoom-only reset already satisfies D-13's scope as it now stands."
  - "Bridge measurement (TestYawMeasurement) is gated pass/fail only at yaw +0 (today's known-good numbers: black under 10%, at least half the probed objects picked, terrain agreement at least 90%); offsets 30/90/180/270 are recorded as measurements, not enforced, so the engine test suite stays green regardless of what future rotation work finds - the gate exists to catch a regression at yaw 0, not to encode today's known-broken behaviour at other yaws as a permanent requirement."

patterns-established:
  - "A checkpoint:decision that changes nothing in code by itself (defer) still produces two durable artefacts: the measurement that justified it (TestYawMeasurement + TGAs, permanent in the test suite) and a dated, evidenced entry in CONTEXT.md's deferred list - so a future phase revisiting D-12 starts from evidence, not from re-deriving it."

requirements-completed: []

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "BkEditorSetYaw threads a yaw offset into SetSessionCamera (ToRadian(45 + offset), pitch/distance unchanged); wraps into [0,360), rejects non-finite as BAD_ARGUMENT; BkEditorViewState.yaw_degrees reports 45+offset"
    requirement: D-12
    verification:
      - kind: unit
        ref: "zig build test-editor-bridge -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (zig-out/local-test/03-06-t1-bridge.log) - grep -n fYawOffsetDegrees session.cpp session.h"
        status: pass
    human_judgment: false
  - id: D2
    description: "TestYawMeasurement measures, for yaw offsets 0/30/90/180/270 on coldwinter, the lower-half black fraction, object-picking count, and terrain/pick agreement; captures editor-bridge-yaw-<offset>.tga for each; gates pass/fail only at offset 0"
    requirement: D-12
    verification:
      - kind: unit
        ref: "zig-out/local-test/03-06-t1-bridge.log lines 'editor-bridge: yaw +<offset>: black <x>%, picked <a>/<b>, terrain agrees <c>/<b>' (all five offsets present); editor-bridge: PASS"
        status: pass
    human_judgment: false
  - id: D3
    description: "Johannes decided, at the Task 2 checkpoint, that D-12 (camera rotation) is deferred out of M1, on the evidence in D1/D2. Recorded in this SUMMARY and in 03-CONTEXT.md's deferred list."
    requirement: D-12
    verification: []
    human_judgment: true
    rationale: "checkpoint:decision is a human-judgment gate by construction - Johannes's own choice (relayed via the orchestrator: \"defer\") is the artifact being recorded, not something automation proves."
  - id: D4
    description: "Task 3 (rotation input: Alt+Q/E, trackpad two-finger rotate, yaw-aware scroll/pan, Home/Reset covering rotation) is not built, per its precondition, because Task 2 chose defer rather than build."
    verification: []
    human_judgment: true
    rationale: "Absence-of-work confirmation: there is nothing to run or measure - a human (Johannes, via the defer decision) is the one who confirms this is the intended outcome, not a bug."

# Metrics
duration: ~20min (Task 1 in a prior session, ~13:15-13:29 local; this continuation recorded the decision and closed the plan)
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 6: Finish M1 - Camera Yaw Measured, Rotation Deferred Summary

**BkEditorSetYaw and an engine-tier measurement across yaw 0/30/90/180/270 showed the terrain progressively vanishing off-screen while sprites stay put; Johannes deferred D-12 (free camera rotation) out of M1 on that evidence.**

## Performance

- **Duration:** ~20 min (Task 1 executed in a prior session; this continuation recorded the checkpoint decision and closed out the plan)
- **Completed:** 2026-09-28
- **Tasks:** 1 of 3 executed (Task 1); Task 2 resolved as a decision (no code); Task 3 skipped per its precondition
- **Files modified:** 6 (5 source/test files in Task 1, plus 03-CONTEXT.md for the deferral record)

## Accomplishments

- `BkEditorSetYaw(session, degrees)` threads a yaw offset (wrapped into [0,360), non-finite rejected) into `SetSessionCamera`'s placement, alongside the existing pitch/distance from D-11.
- Engine-tier `TestYawMeasurement` measured, rather than guessed, what the renderer actually draws at five yaw offsets on `coldwinter`, and captured a TGA of each for visual review.
- The measurement showed camera rotation cannot ship correctly on the current renderer: the terrain quad is laid out in fixed screen space (`CTerrain::MovePatches`) and is progressively clipped away by any yaw but the game's own 45 degrees, while billboard sprites stay upright and in their pre-rotation screen positions with no ground left under them once the terrain clips out.
- Johannes reviewed the table and the TGAs and chose **defer**: D-12 is deferred out of M1, recorded with its full evidence in `03-CONTEXT.md`'s deferred list.
- Task 3 (the rotation input: Alt+Q/E, trackpad two-finger rotate, yaw-aware scroll/pan) was not built, per its own precondition - there is nothing to wire input to on a renderer that cannot draw the result correctly.

## Measured yaw table (Task 1, coldwinter, engine tier)

| Yaw offset | Black fraction (lower half) | Objects picked | Terrain/pick agreement | TGA |
|---|---|---|---|---|
| +0 (gate) | 0.5% | 3/3 | 3/3 | `editor-bridge-yaw-0.tga` |
| +30 | 9.4% | 3/3 | 3/3 | `editor-bridge-yaw-30.tga` |
| +90 | 94.0% | 2/2 | 2/2 | `editor-bridge-yaw-90.tga` |
| +180 | 99.2% | 2/3 | 3/3 | `editor-bridge-yaw-180.tga` |
| +270 | 89.8% | 2/2 | 2/2 | `editor-bridge-yaw-270.tga` |

Only yaw +0 is a pass/fail gate (today's known-good numbers); the rest are recorded measurements.

**Looking at the five captures** (converted to PNG in `zig-out/local-test/` for review):
- **+0:** normal winter ground, trees standing on it, matches the game's usual view.
- **+30:** a hard black diagonal now cuts across roughly the top-left third of the frame - the terrain quad's screen-space layout does not extend to cover the rotated view; trees near the cut line still stand where they always did, some now appearing to float at the terrain's clipped edge.
- **+90:** almost entirely black; only a strip of trees (unrotated billboard sprites, still at their yaw-0 screen positions) remains visible with no ground beneath them.
- **+180:** the most extreme case (99.2% black) - trees hang in pure black with no terrain visible anywhere in frame.
- **+270:** similarly mostly black (89.8%), same floating-tree appearance as +90/+180.

At no offset but +0 does the ground rotate with the camera, and no tree or building sprite turns to face it - both are exactly the two things the plan's objective predicted from reading the source (`CTerrain::MovePatches`, `SGVOT_SPRITE`), now confirmed by measurement rather than assumed.

**Why picking still agreed at every offset:** `BkEditorObjectAt` and `BkEditorWorldToScreen`/`ScreenToWorld` all walk the same *unrotated* projection the terrain and camera-placement math use internally - so a pick that "agrees with the terrain" only proves the picking and camera-placement code are internally consistent with each other, not that either matches what the renderer visibly draws. Picking alone would not have caught this; only the rendered pixels (the black-fraction measurement and the TGAs) did.

## Task Commits

1. **Task 1: Yaw through the bridge, and what the renderer draws at other yaws, measured** - `ec108dda9` (feat)
2. **Task 2: Camera rotation checkpoint decision** - recorded in this SUMMARY and in `03-CONTEXT.md`'s deferred list (no source changes; see "Files Created/Modified")
3. Task 3 - skipped (precondition not met: Task 2 did not choose "build")

**Plan metadata:** committed with this SUMMARY (docs commit, see below)

## Files Created/Modified

- `Sources/src/EditorBridge/bridge.h` - `BkEditorSetYaw` declaration, yaw unit doc comment
- `Sources/src/EditorBridge/bridge.cpp` - `BkEditorSetYaw` implementation (wrap/reject/re-place/reset), `BkEditorViewState.yaw_degrees`
- `Sources/src/EditorBridge/session.h` - `SEditorSession::fYawOffsetDegrees`
- `Sources/src/EditorBridge/session.cpp` - `SetSessionCamera` passes `ToRadian(45 + fYawOffsetDegrees)`
- `tools/zig/editor_bridge_test.cpp` - `TestYawMeasurement`: five-offset capture/measure/print, offset-0 gate
- `.planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md` - D-12 added to the deferred list with the measurement evidence and Johannes's confirmation

## Decisions Made

See `key-decisions` in the frontmatter above (D-12 deferred with full evidence; Task 3 skipped per precondition; D-13 already satisfied by plan 5's zoom-only reset; the engine-tier gate stays scoped to yaw 0 only).

## Deviations from Plan

None - plan executed exactly as written. The plan's own structure made Task 2's "defer" answer skip Task 3 by design (Task 3's `<precondition>` names this explicitly), not as an improvised deviation.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None - no code was added that renders empty/placeholder data. Task 3 (the only task that would have added new user-facing surface) was not built at all, and nothing references it.

## Next Phase Readiness

- D-12 is closed for M1 with recorded evidence (`03-CONTEXT.md` deferred list, this SUMMARY, the five TGAs and `TestYawMeasurement` permanently in the engine test suite) - a future M2/M3 phase revisiting camera rotation has a measured starting point instead of needing to re-derive why it was deferred.
- `BkEditorSetYaw` and `TestYawMeasurement` remain in the bridge as measurement tooling even though input was not wired to them; they are not dead code to be cleaned up - they are the evidence artifact for D-12's status and the regression gate at yaw 0.
- 03-07 (autosave and settings) does not depend on anything this plan would have built in Task 3, so it is unaffected by the defer.

## Self-Check: PASSED

- FOUND: Sources/src/EditorBridge/bridge.h, bridge.cpp, session.h, session.cpp, tools/zig/editor_bridge_test.cpp
- FOUND: .planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md (D-12 deferred entry present, `grep -n "D-12"` confirms it)
- FOUND commits: `ec108dda9` (Task 1), `14f0e3dc4` (D-12 deferral record in 03-CONTEXT.md)
- `grep -n "fYawOffsetDegrees" session.cpp session.h` finds the field and its use in the SetPlacement call

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

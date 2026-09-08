---
id: 02
phase: 2
title: Cursor-anchored zoom application and terrain rebuild trigger
wave: 2
depends_on: [01]
files_modified:
  - Sources/src/GameTT/iMissionInternal.cpp
  - Sources/src/GameTT/iMissionInternal.h
  - Sources/src/Scene/SceneDraw.cpp
autonomous: true
requirements: [D-07, D-08, LANDMINE-L3, LANDMINE-L9]
---

# Plan 02 — Cursor-anchored zoom application and terrain rebuild

## Objective

Replace Plan 01's screen-center zoom application with cursor-anchored zoom
(D-07) inside `CInterfaceMission::ZoomStepMission` (renamed
`ApplyZoomStep( int nDelta )`): capture the world point under the cursor with
`CScene::GetPos3` before the step, bump `GFX.World.ZoomSteps`, re-read
`GetPos3` at the new scale, and shift the camera anchor by `P_old - P_new`
via `ICamera::SetAnchor` before the next `CCamera::Update`. Force
`pTerrain->ResetPosition()` on every applied zoom step (LANDMINE-L3) — the
rebuild heuristic in `CScene::Draw` keys on screen size + projection-bool +
anchor-delta and misses screen-center zoom steps — by replacing that static
heuristic with a player-zoom-aware one. This is the "standard RTS feel"
decision D-07; the anchor snapping quantization (Camera.cpp:92-97) is
inherited unchanged (LANDMINE-L9).

## must_haves

- H1: A zoom step applies `SetAnchor( GetAnchor() + (P_old - P_new) )` where
  P_old/P_new are `GetPos3(cursor)` at old/new zoom (plane-solve `bOnZero=true`
  variant, SceneInternal.cpp:935-959). Screen-center zoom is gone (D-07).
- H2: Every applied step calls `pTerrain->ResetPosition()` (TerrainInternal.h:155
  invalidates vOldAnchor → next `ExtractVisiblePatches` rebuilds meshes) —
  including the screen-center case where the anchor does not move
  (LANDMINE-L3).
- H3: The CScene::Draw rebuild heuristic (SceneDraw.cpp:264-274) also fires
  on player-zoom changes, so a resolution+zoom combination change cannot
  leave stale meshes.
- H4: Anchor delta passes through rcBounds clamping (Camera.cpp:87-91) — at
  map edges the cursor point drifts (documented, acceptable).
- H5: Hold-to-repeat from Plan 01 (OS auto-repeat) keeps working: each
  repeat event runs the same anchored path.
- H6: Pause gate inherited: anchored zoom does not run while
  `GetPauseReason() > PAUSE_TYPE_NO_CONTROL`.

## Tasks

### 02-T1: Cursor-anchored zoom in CInterfaceMission (files: Sources/src/GameTT/iMissionInternal.cpp, Sources/src/GameTT/iMissionInternal.h)

<read_first>
- Sources/src/GameTT/iMissionInternal.cpp lines 1745-1780 (ProcessMessageLocal switch + pause guard, where Plan 01's MC_ZOOM_* cases live)
- Sources/src/Scene/SceneInternal.cpp lines 930-960 (GetPos3 signature and bOnZero plane-solve variant)
- Sources/src/Scene/Camera.cpp lines 56-123 (Update snapping, rcBounds clamp, pause behavior) and Camera.h lines 38-42 (SetAnchor contract)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R2 (the two-GetPos3 formula)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-CONTEXT.md (locked decisions D-07, D-08, LANDMINE-L3, LANDMINE-L9)
- Sources/src/GameTT/iMission.h (MC_ZOOM_* values from Plan 01)
- Sources/src/Scene/Camera.cpp lines 73-74 (trace fprintf pattern to reuse for the ApplyZoomStep trace line)
</read_first>

<action>
1. Rename Plan 01's static helper `ZoomStepMission( int )` to
   `ApplyZoomStep( CInterfaceMission *pThis, int nDelta )` (or make it a
   member — the member form is cleaner since it needs pScene/pCamera/pGFX;
   declare in iMissionInternal.h). Update the MC_ZOOM_* dispatch and the
   StepLocal wheel accumulator to call it.
2. Body (after the pause gate, which moves inside the helper):
   a. `CVec3 vUnderCursor;` — read the cursor screen pos from
      `GetSingleton<ICursor>()->GetPos()` (VGUI coordinates; convert with the
      same screen-space convention the minimap hit test / OnMouseWheel path
      uses — verify against `CUIScreen` mouse handling, UIScreen.cpp:526-534).
   b. If the cursor is outside the gameplay screen rect, fall back to the
      screen center point (GetPos3 of the rect center) — zoom still applies.
   c. `pScene->GetPos3( &vPosOld, cursor2, true )` (old zoom, plane-solve
      variant).
   d. Apply the step: `nSteps = Clamp( nSteps + nDelta, 0, GetMaxZoomSteps(...) )`,
      `SetGlobalVar( "GFX.World.ZoomSteps", nSteps )` (unchanged from Plan 01).
    e. `pScene->GetPos3( &vPosNew, cursor2, true )` (new zoom — matrix
       re-derives per call from the global, SceneInternal.cpp:404-420
       (UpdateTransformMatrix) and SceneInternal.cpp:941 (GetPos3); no cache
       invalidation).
   f. `ICamera::SetAnchor( pCamera->GetAnchor() + ( vPosOld - vPosNew ) )`
      — sets both vAnchor and vAnchor1 (Camera.h:41), so the value survives
      until the next Update. If GetPos3 returns VNULL3 / fails both
      branches, skip the anchor shift and still apply the step.
   g. Force the rebuild: `pScene->GetTerrain()->ResetPosition()` — obtain the
      terrain pointer the way SceneDraw.cpp:269 does (`pTerrain` member or
      `GetSingleton<IScene>()` route; if the IScene interface does not expose
      it, expose a narrow `IViewer`/IScene method or route the call through
      CScene — pick the path with the smallest interface change; do NOT
      duplicate terrain internals in GameTT).
   h. Emit a `BK_INPUT_TRACE` (or `BK_GFX_TRACE`) fprintf line inside
      ApplyZoomStep logging the old anchor, the new anchor, and the resulting
      z — event-down binds and camera-anchor traces fire nothing today
      (Camera.cpp:73-74 traces only strafe/fwd deltas), so this line is the
      per-step trace gate (pattern = Camera.cpp:73-74 getenv + fprintf).
3. Keep the reset semantics from Plan 01 (ZoomSteps=0 in Init /
   CMD_LOAD_FINISHED) untouched.
</action>

<acceptance_criteria>
- `grep -n "ApplyZoomStep" Sources/src/GameTT/iMissionInternal.cpp Sources/src/GameTT/iMissionInternal.h` → declaration + definition + 3 call sites (MC_ZOOM_IN/OUT + wheel accumulator).
- `grep -n "SetAnchor" Sources/src/GameTT/iMissionInternal.cpp` → 1 hit inside ApplyZoomStep.
- `grep -n "ResetPosition" Sources/src/GameTT/iMissionInternal.cpp` → 1 hit inside ApplyZoomStep.
- Source assertion: the two GetPos3 calls bracket exactly one `SetGlobalVar( "GFX.World.ZoomSteps", ... )` write.
- Trace assertion (BK_INPUT_TRACE=1, BK_GFX_TRACE=1): hold J at a fixed cursor position over terrain → repeated steps log; each step emits the new ApplyZoomStep trace line (old anchor, new anchor, z); world point under the cursor visually fixed (manual row).
- Build Game (Debug) succeeds.
</acceptance_criteria>

### 02-T2: Zoom-aware terrain rebuild heuristic in CScene::Draw (files: Sources/src/Scene/SceneDraw.cpp)

<read_first>
- Sources/src/Scene/SceneDraw.cpp lines 255-280 (the static heuristic being extended)
- Sources/src/Scene/TerraDraw.cpp lines 228-240 (ExtractVisiblePatches anchor-change trigger)
- Sources/src/Scene/SceneInternal.cpp lines 404-420 (UpdateTransformMatrix)
- Sources/src/Scene/SceneScreenScale.h (post-Plan-01: GetPlayerZoom / GetMaxZoomSteps)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R7 row 3 + §Landmines L3
</read_first>

<action>
1. Extend the existing static-triple heuristic (nLastGameplayScreenWidth /
   nLastGameplayScreenHeight / bLastScaleGameplayProjection,
   SceneDraw.cpp:264-274) with a fourth static: `static float fLastPlayerZoom = 1.0f;`
2. Compute `fZoom = NSceneScreenScale::GetPlayerZoom( rcGameplayScreen )`
   alongside the existing `CreateGameplayProjectionMatrix` call (line ~262).
   Add `fabsf( fZoom - fLastPlayerZoom ) > 0.001f` to the OR-condition that
   triggers `pTerrain->ResetPosition()`, and update `fLastPlayerZoom` when
   the condition fires.
3. This is defense-in-depth for 02-T1's per-step ResetPosition: it catches
   zoom changes made outside ApplyZoomStep (e.g. a future wheel path or
   script). Do not remove the per-step call from 02-T1 — the Draw-path check
   runs once per frame, the ApplyZoomStep call is immediate and authoritative.
4. Do not touch the `fWidth/fHeight` culling extents at TerraDraw.cpp:247-250
   (research R7 row 16: pre-existing over-inclusive culling, safe at any s).
</action>

<acceptance_criteria>
- `grep -n "fLastPlayerZoom" Sources/src/Scene/SceneDraw.cpp` → >= 3 hits (declaration, comparison, update).
- `grep -n "GetPlayerZoom" Sources/src/Scene/SceneDraw.cpp` → 1+ hit in CScene::Draw.
- Source assertion: the original three-condition OR (screen width/height/projection-bool) is preserved verbatim; the zoom term is additive.
- Trace assertion: BK_GFX_TRACE=1, zoom in one step at screen center (anchor delta ~0) → terrain visibly re-scales with no stale-scale seams; scrolling after the step shows no old-scale lattice.
- Build Game (Debug) succeeds.
</acceptance_criteria>

## Verification criteria (plan-level)

1. Build: `Build Game (Debug)` clean.
2. Grep gates: ApplyZoomStep/SetAnchor/ResetPosition wiring as above;
   `grep -rn "ZoomStepMission" Sources/src/` → 0 hits (rename complete).
3. Trace smoke: `BK_INPUT_TRACE=1 BK_GFX_TRACE=1 Game.exe -tutorial.xml` —
   J/K/Shift+wheel each produce anchor-moving steps; terrain rebuild trace
   (vOldAnchor invalidation) fires per step.
4. Manual rows (RESEARCH §Manual): cursor anchor fixed over terrain and near
   map edges (clamped drift documented); J/K hold-repeat; zoom at screen
   center shows no stale terrain; whole-z regression at 1024x768 z=1
   unchanged.

## Artifacts this phase produces (this plan)

- `CInterfaceMission::ApplyZoomStep` (replaces Plan 01's `ZoomStepMission`)
- Cursor-anchored math: two `CScene::GetPos3` calls + `ICamera::SetAnchor`
  delta in the zoom step path
- Terrain rebuild on zoom: explicit `ResetPosition()` in ApplyZoomStep +
  `fLastPlayerZoom` term in the CScene::Draw heuristic
(No new config keys, no new command IDs, no new globals.)

## Notes

- Cursor drift <= ~2 world units per step is inherent to `CCamera::Update`'s
  anchor snapping (Camera.cpp:92-97, LANDMINE-L9) — accepted, do not bypass.
- At rcBounds edges (Camera.cpp:87-91) the anchored point drifts by the
  clamped amount — standard RTS behavior; listed as a documented manual
  verification observation, not a bug.
- If `GetPos3` fails (cursor over sky with no plane solve), zoom still
  applies without anchor correction — a documented fallback, not silent
  breakage.

---
*Phase 2 — Plan 02 (wave 2). Depends on Plan 01 (zoom state + MC_ZOOM_* dispatch). Independent of Plan 03.*
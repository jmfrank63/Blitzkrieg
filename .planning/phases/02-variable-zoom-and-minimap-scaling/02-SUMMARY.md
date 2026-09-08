---
plan: 02
phase: 2
status: complete
timestamp: 2026-09-08
---

# Plan 02 Summary — Cursor-anchored zoom application and terrain rebuild

## What was built

### 02-T1: Cursor-anchored zoom in CInterfaceMission
- Renamed `CInterfaceMission::ZoomStepMission(int)` static helper to member
  `CInterfaceMission::ApplyZoomStep(int)` (declaration iMissionInternal.h:235,
  definition iMissionInternal.cpp:826).
- Cursor-anchored math per research §R2: cursor position from
  `pCursor->GetPos()` (base-class `CPtr<ICursor> pCursor`,
  InterfaceScreenBase.h:68; `ICursor == ISceneCursor`,
  Scene.h:524, `GetPos()` returns `CVec2` — Cursor.h:65). Outside the gameplay
  rect → falls back to rect center. Two `pScene->GetPos3(&pos, vCursor, true)`
  plane-solve calls bracket exactly one `SetGlobalVar("GFX.World.ZoomSteps")`
  write; `pCamera->SetAnchor(anchor + (vPosOld − vPosNew))` shifts the anchor
  (Camera.h:41 SetAnchor contract — sets both vAnchor and vAnchor1).
- Terrain rebuild forced per step: `pScene->GetTerrain()->ResetPosition()`
  (Scene.h:433, Terrain.h:36) — LANDMINE-L3, covers the screen-center case
  where the anchor does not move.
- BK_INPUT_TRACE line logs delta, steps, anchor, old/new world points
  (getenv pattern, Camera.cpp:73-74 precedent).
- Plan-01 semantics preserved: pause gate stays at the call sites (wheel
  accumulator + MC_ZOOM_* dispatch), D-16 resets untouched, 3 call sites
  renamed (`+1`, `-1`, `MC_ZOOM_IN ? +1 : -1`).

### 02-T2: Zoom-aware terrain rebuild heuristic in CScene::Draw
- Added `static float fLastPlayerZoom = 1.0f` to the existing triple
  (SceneDraw.cpp:266-280) and an additive
  `fabsf(fPlayerZoom − fLastPlayerZoom) > 0.001f` term computing
  `GetPlayerZoom(rcGameplayScreen)` each frame. Original three-condition OR
  preserved verbatim; zoom term is additive; update happens inside the
  firing branch. Defense-in-depth for T1's per-step call.

## Files touched
- `Sources/src/GameTT/iMissionInternal.cpp`
- `Sources/src/GameTT/iMissionInternal.h`
- `Sources/src/Scene/SceneDraw.cpp`

## Acceptance criteria met
- [x] `ApplyZoomStep` → 5 hits (declaration + definition + 3 call sites)
- [x] `SetAnchor` in ApplyZoomStep → present
- [x] `ResetPosition` in ApplyZoomStep → present
- [x] Two GetPos3 calls bracket exactly one ZoomSteps write (source audit)
- [x] `grep -rn "ZoomStepMission" Sources/src/` → 0 hits (rename complete)
- [x] `fLastPlayerZoom` → 3 hits (declaration, comparison, update)
- [x] `GetPlayerZoom` → 1 hit in CScene::Draw
- [x] Original heuristic conditions preserved verbatim, zoom additive
- [ ] Trace smoke + manual rows → human verification (in-game)

## Deviations
- `GetPos3` returns `void` (Scene.h:495) — no failure signal exists, so the
  plan's "if GetPos3 fails, skip the anchor shift" branch cannot be
  implemented; the plane-solve variant always yields a point. The fallback
  for out-of-rect cursors (center point) is implemented. Documented instead
  of a dead validity check.
- Build verification: no MSVC/Windows SDK on this macOS host; zig cross-build
  panics without explicit SDK paths (build.zig:894). Verified by symbol-level
  source audit instead (every API used — GetPos3/SetAnchor/GetAnchor/
  GetTerrain/ResetPosition/GetPos/IsInside/Clamp/GetMaxZoomSteps/
  GetPlayerZoom — checked against its declaring header). Full compile lands
  with the Windows CI run.
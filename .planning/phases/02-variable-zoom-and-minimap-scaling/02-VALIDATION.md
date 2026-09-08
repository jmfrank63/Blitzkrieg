---
phase: 2
slug: variable-zoom-and-minimap-scaling
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-08
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from `02-RESEARCH.md` § Validation Architecture. This is a C++
> legacy Windows game — validation is build-gated + trace-gated + manual
> in-game sign-off rows (the phase-4 precedent).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | MSVC Debug build (`Debug | Win32`) + grep static gates + runtime trace logs |
| **Config file** | `Sources/src/A7.sln` (VS Code task "Build Game (Debug)") |
| **Quick run command** | VS Code task `Build Game (Debug)` → produces `Sources/src/Game/Debug/Game.exe` |
| **Full suite command** | Build + grep gates + scripted mission launch with traces (`BK_GFX_TRACE=1 BK_INPUT_TRACE=1 BK_UI_TRACE=1 Game.exe -tutorial.xml`) |
| **Estimated runtime** | ~2–4 min (build) + ~30s smoke |

---

## Sampling Rate

- **After every task commit:** Quick build (must link clean)
- **After every plan wave:** Full suite (build + grep gates + trace smoke)
- **Before `/gsd-verify-work`:** Full suite green + manual sign-off rows started
- **Max feedback latency:** ~4 minutes (build-bound)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| zoom-state-global | 01 | 1 | D-01..D-10, D-15, D-16 | — | — | grep + build | `GetGameplayScale` composes zoom global; `UpdatePresentOffsets` mirror includes it; resets at `CInterfaceMission::Init` + `CMD_LOAD_FINISHED`; `GFX.` prefix (excluded from saves) | build + grep | pending |
| input-binds | 01 | 1 | D-01..D-04, D-08 | — | — | grep + trace | defconf `game_mission` has `zoom_in/zoom_out/zoom_reset` (J/K/L) + `LSHIFT/RSHIFT+MOUSE_AXIS_Z` wheel binds; `missionCommands[]` entries outside `_FINALRELEASE` guard; `MC_ZOOM_*` in `EMissionCommands` | grep | pending |
| pause-guard | 01 | 1 | research L7 | — | — | source assertion | `MC_ZOOM_*` handlers gate on `GetPauseReason() > PAUSE_TYPE_NO_CONTROL` | grep | pending |
| terrain-rebuild | 02 | 2 | research L3 | — | — | source assertion | zoom step path calls `pTerrain->ResetPosition()` | grep | pending |
| cursor-anchor | 02 | 2 | D-07 | — | — | source + trace | anchor delta = `P_old − P_new` via two `GetPos3` calls; `SetAnchor` before next `Update` | build + trace | pending |
| minimap-sizing | 03 | 3 | D-11..D-14 | — | — | trace + manual | `CUIMiniMap::Reposition` override applies pinned 50% arithmetic once per resolution change; `CreateMiniMapTextures` recreated on wndRect change; no new `PointToTextureMiniMap(..., false)` | grep + BK_UI_TRACE | pending |
| fractional-sync | (in 01/02) | 1–2 | research L1 | — | — | trace | `world_zoom_fractional_` reflects `s*zoom` at fractional steps (BK_GFX_TRACE present-offsets lines) | trace | pending |
| manual-matrix | — | — | all D-xx | — | — | manual | research §Manual rows: zoom-bounds at 640/1024/1920/3440; cursor anchor; input matrix; fractional visuals; minimap rule; res-change matrix; persistence; 1024×768 z=1 regression | in-game | pending |

---

## Sampling Rules

- Whole-step regression invariant (research §R1): at 1024×768 Auto with
  `zoom = 1`, rendering must stay bit-for-bit identical — verified manually
  once per wave, not per task.
- Any edit to `UIMiniMap.cpp` must re-verify the pow2 overlay invariant
  (no `isLeftTop=false` writes, `CMarkPixelFunctional` unflipped) — grep gate
  after each such task.
- Any edit to `GraphicsEngineGpu.cpp:1013-1030` must be paired with a zoom
  mirror assertion in the same task's acceptance criteria.
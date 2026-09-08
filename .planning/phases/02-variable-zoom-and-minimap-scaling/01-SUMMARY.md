---
plan: 01
phase: 2
status: complete
timestamp: 2026-09-08
---

# Plan 01 Summary — Zoom state, bounds, input binds, GPU mirror, persistence resets

## What was built

### 01-T1 — Zoom globals + bounds math in NSceneScreenScale (Scene/SceneScreenScale.h)
- `ZOOM_MIN_VIEW_WIDTH = 640.0f`, `ZOOM_MIN_VIEW_HEIGHT = 480.0f` namespace consts.
- `GetZoomStepFactor()` returning `GetGlobalVar( "GFX.World.ZoomFactor", 1.2f )` (D-06).
- `GetMaxZoomSteps( rcScreen )`: derives `legacy_step * fill` from the base globals
  (returns 0 when unset), computes `z_max = min(vis_w/640, vis_h/480)` from the live
  screen rect, returns the largest n ≥ 0 with `factor^n <= z_max` (capped at 8).
- `GetPlayerZoom( rcScreen )`: `min( factor^ZoomSteps, factor^GetMaxZoomSteps )` —
  read-time clamp (D-15); returns exactly 1.0f at 0 steps (`powf(x,0)==1`), keeping
  the pre-phase product bit-identical (H9).
- `GetGameplayScale` mission-path return is now `fLegacyStep * fFill * GetPlayerZoom( rcScreen )`;
  the unset-base whole-step fallback return is byte-identical to pre-edit.

### 01-T2 — GPU shadow-pass mirror sync (GFXGPU/GraphicsEngineGpu.cpp)
- `UpdatePresentOffsets` mirror block now multiplies the same clamped player zoom into
  `fZoom` before the `bFractional` test (LANDMINE-L1): `z = factor^steps` stepped up to
  min(ZoomSteps, 8) and broken off at the 640×480-effective bound; `world_zoom_fractional_`
  is computed from `fBaseZoom * fZ`. Replicated locally (~15 lines) with a comment naming
  SceneScreenScale.h as canonical — `Sources/src/Scene` is not on the GFXGPU module's
  include path (its `Globals.h` include would not resolve), and the plan explicitly
  allowed replication ("Acceptance is behavioral, not structural").

### 01-T3 — Binds, commands, dispatch, wheel polling
- `iMission.h`: `MC_ZOOM_IN = 0x00200074`, `MC_ZOOM_OUT = 0x00200075`, `MC_ZOOM_RESET = 0x00200076`
  after `MC_CANCEL_CREDITS`.
- `iMissionInternal.cpp` `missionCommands[]`: `zoom_in`/`zoom_out`/`zoom_reset` entries
  immediately after the `#endif` of the `_FINALRELEASE` guard (outside it), before `show_avia_buttons`.
- `defconf.cfg` `game_mission`: `zoom_in`(J), `zoom_out`(K), `zoom_reset`(L) as `event down`;
  two `zoom_wheel` `slider minus` binds on {LSHIFT, MOUSE_AXIS_Z} and {RSHIFT, MOUSE_AXIS_Z},
  exact existing XML item shape; bare `mouse_wheel` and bare T (`begin_timeout`) untouched (D-04).
- `iMissionInternal.h`: `CPtr<IInputSlider> pZoomWheelSlider;` member (`IInputSlider` is at
  global scope in Input.h:39, already on GameTT's include path).
- `Init`: `pZoomWheelSlider = GetSingleton<IInput>()->CreateSlider( "zoom_wheel" );`.
- `ProcessMessageLocal`: shared `MC_ZOOM_IN/OUT/RESET` case group with one shared pause gate
  `pTimer->GetPauseReason() > PAUSE_TYPE_NO_CONTROL` → break (covers RESET — LANDMINE-L7);
  RESET → `SetGlobalVar( "GFX.World.ZoomSteps", 0 )`, IN/OUT → `ZoomStepMission( ±1 )`.
- Static helper `CInterfaceMission::ZoomStepMission( int nDelta )`: clamps
  `Clamp( ZoomSteps + nDelta, 0, GetMaxZoomSteps( GetSingleton<IGFX>()->GetScreenRect() ) )`
  and writes the global; emits `BK_INPUT_TRACE: zoom step delta=%d steps=%d` on stderr
  (pattern Camera.cpp:73-74). Plan 02 replaces the body with cursor anchoring.
- `StepLocal`: polls `pZoomWheelSlider->GetDelta()` into a `static float fZoomWheelAccum`,
  applies `ZoomStepMission( +1/-1 )` per ±4.8f quantum (120 delta × Power 40 × 0.001),
  fraction carried over, same pause gate; sign mapping is wheel-up = IN per D-02
  (flip at runtime trace if inverted — noted, no defconf Power change).

### 01-T4 — D-16 resets + D-15 verification
- `CInterfaceMission::Init` (beside missionMsgs.Init): `SetGlobalVar( "GFX.World.ZoomSteps", 0 )`
  and `SetGlobalVar( "GFX.World.ZoomFactor", 1.2f )` — covers mission start AND restart.
- `CMD_LOAD_FINISHED` case body, first statements before `ChangeResolution()`: the same two
  SetGlobalVar calls (load does not re-run Init; live GFX.* globals survive loads).
- No zoom writes in `ChangeResolution` (InterfaceScreenBase.cpp) — 0 `ZoomSteps` hits;
  D-15 satisfied by the read-time clamp in T1.
- Both resets are `GFX.`-prefixed → excluded from savegames (GlobalVars.h:124-155).

## Files touched
- Sources/src/Scene/SceneScreenScale.h
- Sources/src/GFXGPU/GraphicsEngineGpu.cpp
- Sources/src/GameTT/iMission.h
- Sources/src/GameTT/iMissionInternal.h
- Sources/src/GameTT/iMissionInternal.cpp
- Data/Configs/defconf.cfg

## Acceptance criteria met

| Gate | Evidence |
|---|---|
| `GFX.World.ZoomSteps` in GetPlayerZoom | SceneScreenScale.h:55 |
| `GFX.World.ZoomFactor` in GetZoomStepFactor | SceneScreenScale.h:18 |
| ZOOM_MIN_VIEW_WIDTH = 640 | SceneScreenScale.h:13 |
| Mission-path composition present, fallback untouched | SceneScreenScale.h:85 (`return fLegacyStep * fFill * GetPlayerZoom( rcScreen );`), fallback at :82 unchanged |
| z=1 ⇒ exactly 1.0f | `powf(f, 0) == 1` per C99; product bit-identical |
| GPU mirror reads ZoomSteps inside UpdatePresentOffsets | GraphicsEngineGpu.cpp:1034 (block 1022-1044) |
| `world_zoom_fractional_` from `fBaseZoom * fZ` | GraphicsEngineGpu.cpp:1040-1043 |
| MC_ZOOM_* = 0x00200074/75/76 | iMission.h:207-209 (exactly 3 hits) |
| 3 missionCommands entries between `#endif` and `show_avia_buttons` | iMissionInternal.cpp:150-152 |
| zoom_wheel ×2 in game_mission; begin_timeout untouched | defconf.cfg:2472, 2480; begin_timeout at :947 |
| Pause gate covers IN, OUT, RESET | iMissionInternal.cpp:1852-1865 (shared gate before both branches) |
| BK_INPUT_TRACE in dispatch path | iMissionInternal.cpp:824-825 (ZoomStepMission, shared by keys + wheel) |
| ZoomSteps hits in Init + CMD_LOAD_FINISHED + dispatch (≥4) | iMissionInternal.cpp:822, 823, 848, 1861, 1932 |
| 0 ZoomSteps hits in InterfaceScreenBase.cpp (D-15) | grep count 0 |
| `GFX.`-prefixed reset vars | both SetGlobalVar calls use "GFX.World.*" names |
| E_SHIFT_KEY_DOWN absent from GameTT (LANDMINE-L2) | grep -rn → no hits |
| defconf.cfg XML well-formed | python ET.parse → OK |
| CRLF preserved | `file` on all five edited text files → "with CRLF line terminators" |

### Build verification
`zig build -Dtarget=x86_64-windows-msvc` **cannot run on this macOS host**: build.zig
panics at line 894 requiring explicit `-Dmsvc-include/-Dwindows-sdk-include/-Dmsvc-lib/
-Dwindows-sdk-lib` (MSVC/Windows SDK paths exist only on the repo's Windows CI runners;
CI's macOS job never builds the windows target). Per execution rule 5, fell back to
clang syntax verification of every edited TU and each direct consumer, all clean:

- SceneScreenScale.h (with Misc/StdAfx prelude + calls to all four touched functions): PASS
- Sources/src/GFXGPU/GraphicsEngineGpu.cpp (full TU, `-fsyntax-only`): PASS
- Sources/src/GameTT/iMissionInternal.cpp (full TU): PASS (after the pGFX→GetSingleton fix)
- Consumers: UI/UIScreen.cpp, Scene/SceneDraw.cpp, Scene/TerraDraw.cpp,
  Scene/TerrainInternal.cpp, Scene/FrameSelection.cpp, Scene/SpriteVisObj.cpp,
  Common/InterfaceScreenBase.cpp: PASS
- One compile error found and fixed during verification: `ZoomStepMission` is static and
  cannot touch the instance member `pGFX` — switched to `GetSingleton<IGFX>()`
  (commit 5c03cdbee).

## Deviations

1. **GPU mirror replicates the clamp locally instead of including SceneScreenScale.h.**
   The plan preferred the include "if the GFXGPU include path allows". It does not:
   SceneScreenScale.h includes `"Globals.h"`, which only resolves when `Sources/src/StreamIO`
   is an include root — true for the Scene module but not for the GFXGPU module
   (build.zig `addGFXGPU` adds only Sources/src, Misc, Formats, GFX, GFXGPU). The plan
   explicitly authorized local replication with a canonical-math comment; done exactly so.
2. **`ZoomStepMission` uses `GetSingleton<IGFX>()` rather than `pGFX`.** The plan's
   sketch `GetMaxZoomSteps( pGFX->GetScreenRect() )` was written as if the helper had
   instance access; the helper is static per the plan's own instruction. Behavior identical.
3. **Build check is clang `-fsyntax-only` per-TU, not the full zig/msvc link.** Environment
   limitation documented above; static verification is otherwise complete.

None of these deviate from plan semantics; the shared-file ordering note was honored
(all iMissionInternal.cpp edits landed before this summary commit, ready for Plan 03's
executor to re-read the file from disk).
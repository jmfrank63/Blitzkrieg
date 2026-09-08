---
id: 01
phase: 2
title: Zoom state, bounds, input binds, GPU mirror, and persistence resets
wave: 1
depends_on: []
files_modified:
  - Sources/src/Scene/SceneScreenScale.h
  - Sources/src/GFXGPU/GraphicsEngineGpu.cpp
  - Sources/src/GameTT/iMission.h
  - Sources/src/GameTT/iMissionInternal.h
  - Sources/src/GameTT/iMissionInternal.cpp
  - Data/Configs/defconf.cfg
autonomous: true
requirements: [D-01, D-02, D-03, D-04, D-05, D-06, D-08, D-09, D-10, D-15, D-16, LANDMINE-L1, LANDMINE-L2, LANDMINE-L6, LANDMINE-L7, LANDMINE-L8]
---

# Plan 01 — Zoom state, bounds, input binds, GPU mirror, persistence resets

## Objective

Introduce the player zoom state as `GFX.World.ZoomSteps` (int step count) and
`GFX.World.ZoomFactor` (float, 1.2) global vars, compose it inside
`NSceneScreenScale::GetGameplayScale` on the mission path only, clamp it at
read time between z=1 (D-10) and z_max = min(visW/640, visH/480) (D-09), and
mirror it into the GPU shadow-pass fractional flag in
`GraphicsEngineGpu::UpdatePresentOffsets` (LANDMINE-L1). Register J/K/L
`event down` binds and LSHIFT/RSHIFT+MOUSE_AXIS_Z `zoom_wheel` slider binds in
`defconf.cfg` `game_mission`, add `MC_ZOOM_IN/OUT/RESET` (0x00200074/75/76)
with pause gating, poll the wheel slider in `CInterfaceMission::StepLocal`,
and reset the zoom at `Init` and `CMD_LOAD_FINISHED` (D-16). Zoom is
screen-center-scaling after this plan; cursor anchoring and the terrain
rebuild trigger are Plan 02.

## must_haves

- H1: Globals `GFX.World.ZoomSteps` (int) and `GFX.World.ZoomFactor` (float)
  exist. The `GFX.` prefix keeps them out of savegames (GlobalVars.h:124-155
  policy) — LANDMINE-L6 satisfied for the serialization half.
- H2: `GetGameplayScale` returns `legacy_step * fill * z` ONLY on the mission
  path (world base globals set); the unset-base whole-step fallback
  (SceneScreenScale.h:35-36) is unchanged and never composes z.
- H3: z is clamped at READ time: z_eff = clamp(factor^steps, 1.0, z_max),
  z_max = min(vis_w/640, vis_h/480) computed from the live rcScreen argument.
  At a 640x480 cfg z_max == 1.0 (zero range, D-09). z never < 1 (D-10). This
  makes D-15 (resolution change keeps step count, re-clamps) automatic —
  `ChangeResolution` never writes ZoomSteps.
- H4: `UpdatePresentOffsets` multiplies the same clamped z into `fZoom` before
  the fractional test (LANDMINE-L1) — fractional z at a whole-step base sets
  `world_zoom_fractional_` true.
- H5: `MC_ZOOM_IN=0x00200074`, `MC_ZOOM_OUT=0x00200075`,
  `MC_ZOOM_RESET=0x00200076` exist in `EMissionCommands`; entries
  `zoom_in`/`zoom_out`/`zoom_reset` are registered in `missionCommands[]`
  OUTSIDE the `#if !defined(_FINALRELEASE)` guard; dispatched in
  `ProcessMessageLocal` behind a pause gate
  (`pTimer->GetPauseReason() > PAUSE_TYPE_NO_CONTROL` → skip) — LANDMINE-L7.
- H6: `defconf.cfg` `game_mission` has `zoom_in`(J), `zoom_out`(K),
  `zoom_reset`(L) as `event down`, and two `zoom_wheel` `slider minus` binds
  on {LSHIFT, MOUSE_AXIS_Z} and {RSHIFT, MOUSE_AXIS_Z}. Combo-subset
  suppression (InputBinder.cpp:326-409) silences the bare `mouse_wheel` bind
  while Shift+wheel is formed — no interception code, LANDMINE-L2/L8
  (Shift tracking does NOT use m_keyboardState/E_SHIFT_KEY_DOWN — dead in
  missions per research R3b). Bare T `begin_timeout` untouched (D-04).
- H7: Zoom resets to 0 steps at `CInterfaceMission::Init` (iMissionInternal.cpp:813)
  and in the `CMD_LOAD_FINISHED` handler (~1861) — D-16. Not in `Done()`.
- H8: Hold-to-repeat (D-08) comes from OS key auto-repeat re-firing
  `event down` (CControlKey::ChangeState has no same-state guard); no timing
  code added. Wheel notch quantum = 4.8 slider units (120 delta x Power 40
  x 0.001); one step per quantum, fractional remainder accumulates.
- H9: Regression invariant: with ZoomSteps == 0 (z = 1), GetGameplayScale
  returns exactly `fLegacyStep * fFill` — bit-identical to pre-phase; the
  `fScale <= 1.001f` early-outs remain unreachable from zoom (z >= 1).

## Tasks

### 01-T1: Zoom globals + bounds math in NSceneScreenScale (files: Sources/src/Scene/SceneScreenScale.h)

<read_first>
- Sources/src/Scene/SceneScreenScale.h (whole file — the function being edited)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R1 (composition + bounds arithmetic)
- Sources/src/Common/InterfaceScreenBase.cpp lines 540-560 (what publishes GFX.World.BaseSizeX/Y)
- Sources/src/StreamIO/GlobalVars.h lines 124-155 (GFX.* save exclusion)
</read_first>

<action>
1. Add namespace consts: `ZOOM_MIN_VIEW_WIDTH = 640.0f`, `ZOOM_MIN_VIEW_HEIGHT = 480.0f`.
2. Add `inline float GetZoomStepFactor()` returning
   `GetGlobalVar( "GFX.World.ZoomFactor", 1.2f )` (D-06: 1.2 within the
   1.15-1.25 discretion band).
3. Add `inline int GetMaxZoomSteps( const CTRect<float> &rcScreen )`:
   compute `fLegacyStep * fFill` exactly as GetGameplayScale's mission path
   does (base globals; return 0 if base unset), then
   `vis_w = rcScreen.Width() / baseScale`, `vis_h = rcScreen.Height() / baseScale`,
   `z_max = Min( vis_w / ZOOM_MIN_VIEW_WIDTH, vis_h / ZOOM_MIN_VIEW_HEIGHT )`,
   and return the largest n >= 0 with `powf( GetZoomStepFactor(), n ) <= z_max`
   (simple upward loop; n stays <= 8).
4. Add `inline float GetPlayerZoom( const CTRect<float> &rcScreen )`:
   `nSteps = GetGlobalVar( "GFX.World.ZoomSteps", 0 )`;
   `z = powf( GetZoomStepFactor(), nSteps )`; return `Min( z, powf( factor, GetMaxZoomSteps( rcScreen ) ) )`
   so a stale step count re-clamps at read time (D-15).
5. In `GetGameplayScale`: leave the unset-base fallback return (lines 35-36)
   untouched; change the final mission-path return (line 39) to
   `fLegacyStep * fFill * GetPlayerZoom( rcScreen )`.
6. Do NOT touch `ScaleGameplayScreenPoint`, `UnscaleGameplaySpritePoint`,
   `GetGameplayScreenRect`, `CreateGameplayProjectionMatrix` — they all read
   GetGameplayScale and inherit z.
</action>

<acceptance_criteria>
- `grep -n "GFX.World.ZoomSteps" Sources/src/Scene/SceneScreenScale.h` hits inside GetPlayerZoom.
- `grep -n "GFX.World.ZoomFactor" Sources/src/Scene/SceneScreenScale.h` hits inside GetZoomStepFactor.
- `grep -n "640" Sources/src/Scene/SceneScreenScale.h` hits ZOOM_MIN_VIEW_WIDTH.
- The line `return fLegacyStep * fFill * GetPlayerZoom( rcScreen );` (or equivalent composition) is present; the unset-base fallback return at ~line 36 is byte-identical to pre-edit (`Max( 1.0f, floorf( Min( ...`).
- Build Game (Debug) succeeds.
- Source assertion: when `GetGlobalVar("GFX.World.ZoomSteps",0) == 0`, GetPlayerZoom returns exactly 1.0f (powf(x,0)==1) so the z=1 product is bit-identical.
</acceptance_criteria>

### 01-T2: GPU shadow-pass mirror sync (files: Sources/src/GFXGPU/GraphicsEngineGpu.cpp)

<read_first>
- Sources/src/GFXGPU/GraphicsEngineGpu.cpp lines 1010-1035 (the mirror block being edited)
- Sources/src/GFXGPU/GraphicsEngineGpu.h line ~149 (world_zoom_fractional_)
- Sources/src/Scene/SceneScreenScale.h (post-T1)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R1 "Interaction with the GPU mirror"
</read_first>

<action>
In `UpdatePresentOffsets`'s mirror block (the block computing `fZoom =
fLegacyStep * fFill` around GraphicsEngineGpu.cpp:1013-1030): read the player
zoom with the same globals (`GFX.World.ZoomSteps`, `GFX.World.ZoomFactor`)
and multiply it into `fZoom` before the `bFractional` test. Either include
`Scene/SceneScreenScale.h` and call `GetPlayerZoom(...)`/GetMaxZoomSteps with
the scene rect (preferred if the GFXGPU include path allows), or replicate
the ~15-line clamp locally with a comment naming SceneScreenScale.h as the
canonical. Acceptance is behavioral, not structural.
</action>

<acceptance_criteria>
- `grep -n "ZoomSteps\|GetPlayerZoom" Sources/src/GFXGPU/GraphicsEngineGpu.cpp` hits inside the UpdatePresentOffsets mirror block (line range ~1010-1035).
- Trace assertion: with BK_GFX_TRACE=1 at 1024x768 Auto and one zoom step (z=1.2, s=1.2), a shadow-sprite frame uses linear sampling (no grey blotches around trees); with z=1 the present-offsets trace is unchanged from pre-phase.
- Source assertion: `world_zoom_fractional_` is computed from `fLegacyStep * fFill * z`, not from `fLegacyStep * fFill`.
</acceptance_criteria>

### 01-T3: J/K/L + Shift+wheel binds, MC_ZOOM_* commands, dispatch, wheel polling (files: Sources/src/GameTT/iMission.h, Sources/src/GameTT/iMissionInternal.h, Sources/src/GameTT/iMissionInternal.cpp, Data/Configs/defconf.cfg)

<read_first>
- Sources/src/GameTT/iMission.h lines 155-255 (EMissionCommands; MC_CANCEL_CREDITS = 0x00200073 is the last 0x002000xx value)
- Sources/src/GameTT/iMissionInternal.cpp lines 125-175 (missionCommands[]), 810-840 (Init), 1350-1382 (StepLocal), 1745-1775 (ProcessMessageLocal + pause guard)
- Data/Configs/defconf.cfg lines 891-1000 (game_mission section), 288-296 (MOUSE_AXIS_Z Power 40), 371-377 (mouse_wheel slider minus bind)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R3, §R5
- Sources/src/UI/UIScreen.cpp lines 120-125 (CreateSlider pattern)
</read_first>

<action>
1. iMission.h: add to EMissionCommands after MC_CANCEL_CREDITS (line 206):
   `MC_ZOOM_IN = 0x00200074`, `MC_ZOOM_OUT = 0x00200075`, `MC_ZOOM_RESET = 0x00200076`.
2. iMissionInternal.cpp missionCommands[]: add AFTER the `#endif` of the
   `#if !defined(_FINALRELEASE)` guard (i.e. between `save_scene` and
   `show_avia_buttons`), three entries: `{ "zoom_in", MC_ZOOM_IN }`,
   `{ "zoom_out", MC_ZOOM_OUT }`, `{ "zoom_reset", MC_ZOOM_RESET }`.
3. defconf.cfg game_mission section (line 891): append five bind items at the
   end of the section's Commands list: `zoom_in` / `event down` / `J`;
   `zoom_out` / `event down` / `K`; `zoom_reset` / `event down` / `L`;
   `zoom_wheel` / `slider minus` / {LSHIFT, MOUSE_AXIS_Z}; `zoom_wheel` /
   `slider minus` / {RSHIFT, MOUSE_AXIS_Z}. Use the exact XML item shape of
   the existing `begin_timeout`/`stop_timeout` entries. Do not touch the bare
   `mouse_wheel` bind or bare T.
4. iMissionInternal.h: add member `CPtr<NInput::IInputSlider> pZoomWheelSlider;`
   to CInterfaceMission (guard the type with the same include pattern
   UIScreen.cpp uses for IInputSlider).
5. Init (iMissionInternal.cpp ~828, near `missionMsgs.Init`):
   `pZoomWheelSlider = GetSingleton<IInput>()->CreateSlider( "zoom_wheel" );`
6. ProcessMessageLocal switch: add cases `MC_ZOOM_IN` → `ZoomStepMission( +1 )`,
   `MC_ZOOM_OUT` → `ZoomStepMission( -1 )`, `MC_ZOOM_RESET` →
   `SetGlobalVar( "GFX.World.ZoomSteps", 0 )`. Each MC_ZOOM_IN/OUT case first
   checks `pTimer->GetPauseReason() > PAUSE_TYPE_NO_CONTROL` and does nothing
   if paused (LANDMINE-L7). Add a static helper `ZoomStepMission( int nDelta )`
   in iMissionInternal.cpp that (this plan) clamps
   `nSteps = Clamp( nSteps + nDelta, 0, GetMaxZoomSteps( pGFX->GetScreenRect() ) )`
   and `SetGlobalVar( "GFX.World.ZoomSteps", nSteps )` — Plan 02 replaces the
   body with cursor anchoring.
7. StepLocal (~1356): poll the wheel — accumulate `pZoomWheelSlider->GetDelta()`
   into a `static float fZoomWheelAccum`; apply `ZoomStepMission( +1 )` per
   +4.8f accumulated and `ZoomStepMission( -1 )` per -4.8f, subtracting the
   quantum each application, gated by the same pause check. If runtime
   BK_INPUT_TRACE shows the notch sign inverted vs D-02 (wheel up must zoom
   IN), flip the sign mapping — do not change defconf Power.
</action>

<acceptance_criteria>
- `grep -n "MC_ZOOM_IN\|MC_ZOOM_OUT\|MC_ZOOM_RESET" Sources/src/GameTT/iMission.h` → exactly 3 hits with values 0x00200074/75/76.
- `grep -n "zoom_in\|zoom_out\|zoom_reset" Sources/src/GameTT/iMissionInternal.cpp` → 3 missionCommands entries located between the `_FINALRELEASE` `#endif` and `show_avia_buttons`.
- `grep -n "zoom_wheel" Data/Configs/defconf.cfg` → 2 hits inside the game_mission section (LSHIFT and RSHIFT variants); `grep -n "begin_timeout" Data/Configs/defconf.cfg` still shows the untouched bare-T bind (D-04).
- `grep -n "GetPauseReason" Sources/src/GameTT/iMissionInternal.cpp` shows the gate inside the MC_ZOOM_IN/OUT handling.
- Trace assertion (BK_INPUT_TRACE=1): pressing J logs the zoom bind activation; Shift+wheel logs the zoom_wheel combo activating and the bare `mouse_wheel` slider NOT activating during formed notches.
- Build Game (Debug) succeeds.
</acceptance_criteria>

### 01-T4: D-16 resets + D-15 read-time clamp verification (files: Sources/src/GameTT/iMissionInternal.cpp)

<read_first>
- Sources/src/GameTT/iMissionInternal.cpp lines 810-840 (Init) and 1855-1875 (CMD_LOAD_FINISHED)
- Sources/src/Common/InterfaceScreenBase.cpp lines 451-676 (ChangeResolution — must NOT touch zoom)
- Sources/src/StreamIO/GlobalVars.h lines 124-155 (GFX.* save exclusion)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R4
</read_first>

<action>
1. In `CInterfaceMission::Init` (~line 828, beside missionMsgs.Init): `SetGlobalVar( "GFX.World.ZoomSteps", 0 );` and `SetGlobalVar( "GFX.World.ZoomFactor", 1.2f );` (covers mission start AND restart — restart funnels through Init per research R4).
2. In the `CMD_LOAD_FINISHED` handler (~1861), as the first statement of the case body before `ChangeResolution()`: the same two SetGlobalVar calls (load does not re-run Init; live GFX.* globals survive loads by design).
3. Do NOT add any zoom writes to `ChangeResolution` (InterfaceScreenBase.cpp) — D-15 is satisfied by the read-time clamp in 01-T1.
</action>

<acceptance_criteria>
- `grep -n "GFX.World.ZoomSteps" Sources/src/GameTT/iMissionInternal.cpp` → hits at: the T3 dispatch/reset sites, Init, and CMD_LOAD_FINISHED (>= 4 total).
- `grep -n "ZoomSteps" Sources/src/Common/InterfaceScreenBase.cpp` → 0 hits (D-15: resolution change never resets).
- Trace assertion: BK_GFX_TRACE=1 mission launch → zoom steps via J → save → load → effective zoom is 1 (steps reset) while the step-cap logic still works.
- Source assertion: the two SetGlobalVar names are `GFX.`-prefixed (excluded from savegames per GlobalVars.h:124-155).
</acceptance_criteria>

## Verification criteria (plan-level)

1. Build: VS Code task `Build Game (Debug)` (A7.sln, Debug|Win32) links clean → `Sources/src/Game/Debug/Game.exe`.
2. Grep gates: all per-task greps above pass; additionally
   `grep -rn "E_SHIFT_KEY_DOWN" Sources/src/GameTT/` → no hits (LANDMINE-L2 respected).
3. Trace smoke: `BK_GFX_TRACE=1 BK_INPUT_TRACE=1 Game.exe -tutorial.xml` —
   J/K presses log bind activation; Shift+wheel logs zoom_wheel slider
   activation with bare mouse_wheel suppressed; world-base trace lines
   (`GFX.World.BaseSizeX`) unchanged by zooming.
4. Manual row (from RESEARCH §Validation): at 640x480 cfg, J/K produce zero
   zoom range; at 1024x768, zoom-in stops at the 640x480-effective view;
   L resets; zoom resets on restart and load; wheel alone = old behavior.

## Artifacts this phase produces (this plan)

- Global vars: `GFX.World.ZoomSteps` (int), `GFX.World.ZoomFactor` (float)
- NSceneScreenScale: `ZOOM_MIN_VIEW_WIDTH`, `ZOOM_MIN_VIEW_HEIGHT`,
  `GetZoomStepFactor()`, `GetMaxZoomSteps()`, `GetPlayerZoom()`;
  GetGameplayScale mission-path composition
- EMissionCommands: `MC_ZOOM_IN = 0x00200074`, `MC_ZOOM_OUT = 0x00200075`,
  `MC_ZOOM_RESET = 0x00200076`
- missionCommands[] entries: `zoom_in`, `zoom_out`, `zoom_reset`
- defconf.cfg binds: `zoom_in`(J), `zoom_out`(K), `zoom_reset`(L),
  `zoom_wheel` x2 ({LSHIFT, MOUSE_AXIS_Z}, {RSHIFT, MOUSE_AXIS_Z})
- CInterfaceMission: `pZoomWheelSlider` member, `ZoomStepMission( int )`
  helper, MC_ZOOM_* dispatch cases with pause gate, wheel accumulator in
  StepLocal, zoom resets in Init + CMD_LOAD_FINISHED
- GraphicsEngineGpu: player-zoom multiplication in the UpdatePresentOffsets
  fractional mirror
(Plan 02 adds `CInterfaceMission::ApplyZoomStep` anchored application +
SceneDraw scale-delta rebuild; Plan 03 adds `CUIMiniMap::Reposition`.)

## Notes

- **D-06 step-count expectation adjustment:** with factor 1.2, 1024x768 and
  1920x1080 both cap at z_max = 1.6 → only ~3 distinct zoom-in levels
  (1.2, 1.44, 1.6-clamped), not the 5-7 imagined in D-06. The clamp math
  bounds z_max = min(visW/640, visH/480); this is arithmetic, not a defect.
  Factor 1.15 would give ~3-4 steps; 1.2 chosen as the D-06 center value.
- Hold-to-repeat uses OS keyboard auto-repeat (research R5 verdict): zero new
  timing code, satisfies D-08's "keyboard auto-repeat style".
- Known cosmetic side effect (LANDMINE-L8): while a Shift+wheel notch is
  formed, the {LSHIFT} bare combo suppress/unsuppress flaps add_action_on/off
  messages — harmless (no click in flight), do not "fix".
- D-15 is implemented as read-time clamping only (no writes in
  ChangeResolution); the step count survives resolution changes by design.

---
*Phase 2 — Plan 01 (wave 1). Depends on nothing. Plans 02 (wave 2) and 03 (wave 1, parallel) build on this.*
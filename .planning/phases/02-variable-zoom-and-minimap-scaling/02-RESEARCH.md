# Phase 2 Research: Variable zoom and minimap scaling

**Gathered:** 2026-09-08
**Status:** Research complete — all R1–R8 answered, 9 landmines flagged

---

## Executive summary

The zoom knob is confirmed: `NSceneScreenScale::GetGameplayScale` (`Sources/src/Scene/SceneScreenScale.h:14-40`) computes `s = legacy_step(cfg_eff) * fill`, and a player zoom multiplier `z` composes cleanly as `s *= z`. Every consumer — projection, terrain, sprites, picking, war fog, selection frame, minimap camera frame — reads `s` through this one function, so a `z` inserted there propagates everywhere automatically. **One mandatory sync**: the GPU shadow-pass mirror in `GraphicsEngineGpu::UpdatePresentOffsets` (`Sources/src/GFXGPU/GraphicsEngineGpu.cpp:1013-1030`) re-derives the same formula and must multiply by `z` too, or the shadow-pass linear-sampling switch misfires at fractional player zoom.

The single biggest surprise of this research: **`CUIScreen::m_keyboardState` / `E_SHIFT_KEY_DOWN` is dead in missions.** Text messages (which drive `OnChar`) are only produced when the input text mode is not `NOTEXT`, and missions always run in `NOTEXT` (`CInterfaceScreenBase::OnGetFocus`, `InterfaceScreenBase.cpp:690`). The Shift+wheel feature therefore cannot intercept via `m_keyboardState`; it must use the CCombo bind machinery (`LSHIFT+MOUSE_AXIS_Z` / `RSHIFT+MOUSE_AXIS_Z` slider binds), which works and which **automatically suppresses the bare-wheel bind while formed** — exactly the wanted semantics.

The second-biggest finding: **the 50% minimap-rule arithmetic does not close at 1024×768** with the measured legacy geometry (minimap dialog 264px + command rail + status bar 413px = 800px of 1024 = 78%; even minimap diamond 256 + status bar 413 = 669 > 512). The rule holds the panel content fixed and makes the minimap flexible (D-11), which at 1024 would demand a ~99px diamond — smaller than the dialog's own button rail. Planning must pin the exact arithmetic (see R6c) before sizing code is written.

Cursor-anchored zoom (D-07) has a clean formula via two `GetPos3` calls (§R2), but requires forcing a terrain mesh rebuild on zoom steps (§Landmine L3).

---

## R1. Where the player zoom multiplier lives

### Recommendation: (b) a new global composed inside `GetGameplayScale`

**The formula today** (`SceneScreenScale.h:14-40`):

```
s = legacy_step(cfg_eff) * fill
legacy_step = max(1, floor(min(cfg_eff.w/1024, cfg_eff.h/768)))
fill        = max(1, min(drawable.w/cfg_eff.w, drawable.h/cfg_eff.h))
```

with `cfg_eff` = `GFX.World.BaseSizeX/Y` (published only while a Mission screen is active; 0 otherwise → legacy whole-step fallback at lines 33-36).

**Player zoom composes as:** `s = legacy_step * fill * z`, where `z` is clamped to `[1.0, z_max]` (bounds in §Zoom bounds below). Insert the composition at the end of `GetGameplayScale` (after line 39), reading `z` from a global var.

**Why a global var and not the alternatives:**

- **(c) Modifying `BaseSizeX/Y` directly is wrong** — confirmed. `BaseSize` is the settings authority: `CInterfaceScreenBase::ChangeResolution` publishes it from the configured resolution (`InterfaceScreenBase.cpp:543-544, 655-656`) and diffs it per frame (613-619). It feeds the HUD layout scale (`UIScreen.cpp:214-222`), the sound-radius scale (`SoundScene.cpp:1057-1074`), and the sub-800px control rule (`iMissionInternal.cpp:897-898`). Overwriting it with a zoomed value would corrupt all of those and fight the per-frame diff (which would keep overwriting it back).
- **(a) Inside `GetGameplayScale` as a local composition (this is option (a)+(b) combined):** the function is a free function reading globals — it cannot hold state; `z` must live *somewhere* reachable.
- **(b) New global `GFX.World.ZoomSteps` (int) or `GFX.World.ZoomFactor` (float):** chosen because `GetGameplayScale` is called from **four separate modules** (Scene, GameTT, GFXGPU, UI-adjacent code), and a global var is the only state all of them can already read — `GraphicsEngineGpu` (a separate DLL) already does `GetGlobalVar("GFX.World.BaseSizeX", ...)` at `GraphicsEngineGpu.cpp:1020-1021`. Storing the **step count (int)** and deriving `z = factor^steps` keeps the clamp exact and the math deterministic.

**Serialization landmine resolved:** global vars are saved into savegames *except* names prefixed `GFX.` or `Options.` — `CGlobalVars::operator&` (`StreamIO/GlobalVars.h:124-155`) strips `GFX.*`/`Options.*` on save and restores their live values on load. A `GFX.`-prefixed zoom global therefore automatically satisfies the "not persisted in savegames" half of D-16. The "reset at mission start/restart/load" half needs explicit resets in `CInterfaceMission::Init` (`iMissionInternal.cpp:813`) and the `CMD_LOAD_FINISHED` handler (`iMissionInternal.cpp:1861-1870`) — see §R4.

**Interaction with the GPU mirror (mandatory sync):** `UpdatePresentOffsets` re-derives `fZoom = fLegacyStep * fFill` and sets `world_zoom_fractional_` (`GraphicsEngineGpu.cpp:1013-1030`, member at `GraphicsEngineGpu.h:149`). The player `z` **must** be multiplied into `fZoom` there, reading the same global. Without it, a whole-step `s` (e.g. 1.0 at 1024×768 Auto) combined with a fractional `z` (1.2) stays flagged non-fractional and the shadow pass keeps point sampling → grey blotch artifacts around trees return (the 2026-08-13 bug class, `docs/scaling.md` §sprite path).

**s>1 vs s<1 semantics (R1 sub-question):** `fill` and `legacy_step` are both clamped `max(1, ...)` (`SceneScreenScale.h:37-38`), and `cfg_eff ≤ drawable` by construction (`ChangeResolution` line 543-544: `nWorldBaseX = min(cfg, drawable)`), so `fill ≥ 1` mathematically — the clamps are belt-and-braces. **`fill < 1` cannot occur**, `s < 1` cannot occur in a mission. `s = 1` exactly when `cfg_eff ≤ 1024×768` (legacy_step floors to 1) and `cfg_eff == drawable` (fill = 1), e.g. 640×480 Auto or 1024×768 cfg at 1024×768 drawable. D-10 (zoom-out limit = un-zoomed view, `z ≥ 1`) keeps `s*z ≥ 1` always, so the `fScale <= 1.001f` false-return of `CreateGameplayProjectionMatrix` (`SceneScreenScale.h:114-115`) and the early-outs of `ScaleGameplayScreenPoint`/`UnscaleGameplaySpritePoint` (lines 50, 78) remain unreachable-from-zoom — the legacy 1:1 codepath is never entered via player zoom. No new regime.

Note: at 1920×1080 Auto, `s = 1.40625` is *already fractional every day* (fill is fractional) — the fractional-zoom machinery is exercised routinely; player zoom merely makes it user-controlled.

### Zoom bounds (D-09/D-10 arithmetic)

Let `vis_w = drawable.w / s`, `vis_h = drawable.h / s` — the un-zoomed visible world extent (equals `cfg_eff` on the limiting axis, more on the other; this automatically accounts for fill and aspect). Then:

- **Zoom-in limit (D-09):** `z_max = min(vis_w / 640, vis_h / 480)`. At a 640×480 cfg, `z_max = 1` → zero zoom range, exactly D-09. At 1024×768: `z_max = 1.6` (width binds). At 1920×1080: `vis = 1365×768`, `z_max = 1.6` (height binds: 768/480). Compute `z_max` from the live `rcScreen` passed to `GetGameplayScale` — clamping inside `GetGameplayScale` (or at zoom-step time using the same inputs) makes D-15 (resolution change re-clamps, keeps step count) fall out for free.
- **Zoom-out limit (D-10):** `z ≥ 1.0`.
- **Step count reality check:** with step 1.2, 1024×768 gives ~3 levels in (1.2, 1.44, 1.728→clamped 1.6); D-06's "~5–7 levels" overestimates what a 1.6× range yields. Planner should note the real count derives from the bounds; factor 1.15 gives ~3–4 steps. This is within Claude's discretion per D-06 but the expectation gap should be acknowledged in the plan.

---

## R2. Cursor-anchored zoom math

### The mapping

The gameplay projection is an ortho of `(W/s, H/s)` centered at screen center (`GetGameplayScreenRect`, `SceneScreenScale.h:97-109`; `CreateGameplayProjectionMatrix`, 111-121), and `ScaleGameplayScreenPoint` (48-68) confirms the screen mapping is `screen = center + (proj1(P) - center) * s` around the center — where `proj1` is the camera view at s=1 and the camera anchor (`vAnchor1`) is the world point at the screen center (`Camera.cpp:99-101`: `vPos = vAnchor1 - vDir*fRod`, view built from `vPos`).

### The formula (verified shape)

Moving the camera anchor by world-space `Δ` translates every world point's screen position rigidly; a fixed screen ray then hits world points translated by `+Δ`. Therefore, for a zoom step at cursor `C`:

1. `P_old = GetPos3(C)` — world under cursor **before** the zoom (old `s`).
2. Apply the new zoom level (set the global; `GetGameplayScale`/`CreateGameplayProjectionMatrix` recompute from globals on every call — `SceneInternal.cpp:941` and `UpdateTransformMatrix` (404-420) re-derive per call, so no cache invalidation needed).
3. `P_new = GetPos3(C)` — world under cursor **after** the zoom (new `s`), camera not yet moved.
4. `SetAnchor(anchor + (P_old - P_new))` — `ICamera::SetAnchor` (`Camera.h:41`) sets both `vAnchor` and `vAnchor1`, so `GetAnchor()` answers correctly before the next `Update` (the same staleness trap `SetPlacement` documents at `Camera.cpp:42-46`).

Sign conventions need no derivation: the two-`GetPos3` difference self-corrects. Use the `bOnZero=true` (plane-solve) variant (`SceneInternal.cpp:935-959`) — cheap, matches what the minimap frame uses.

### Confirmed interactions

- **Whole-screen anchor snapping** (`Camera.cpp:92-97`): `Update` snaps `vAnchor1` to integer steps along the camera's screen axes (`int(fComponentX)`, `int(fComponentY/2)*2` — Y quantized to 2 world units in the rotated frame). This quantization survives anchor adjustment (it re-derives from the new `vAnchor`) and introduces ≤ ~2 world units (~1px) of cursor drift per step — acceptable; do **not** bypass `Update`'s snapping (everything else depends on it). But be aware the snapped `GetAnchor()` is what the zoom math reads back.
- **`rcBounds` clamp** (`Camera.cpp:87-91`, bounds set at `iMissionInternal.cpp:1248` to the map size): at map edges the anchor delta is clamped and the cursor point drifts — standard RTS behavior, acceptable, note it.
- **Camera slider integration**: `CCamera::Update` (58-123) adds `pFwd`/`pStrafe` deltas to `vAnchor` every frame *inside* `CScene::Draw` (`SceneDraw.cpp:275`). A zoom-step anchor change via `SetAnchor` composes with this (the delta is applied to `vAnchor` before the next `Update`), no conflict.
- **`fRod` is untouched**: zoom is projection-scale, not camera height. `SetMissionCameraPlacement` (`iMissionInternal.cpp:928-933`) keeps `rod = 1024*4 + screenHeight`.
- **The dead `pZoom` slider** (`Camera.cpp:25`, registered `camera_zoom`, never bound): leave unbound; the global-var transport reaches all modules including GFXGPU, which a camera-member would not.
- **Edge case — center-anchored zoom**: if the cursor is at screen center, `P_old == P_new` and the anchor doesn't move → the terrain-rebuild trigger in L3 (below) must not depend on anchor movement.

---

## R3. Wheel + SHIFT plumbing

### The full flow (traced)

1. SDL wheel → `SDL_EVENT_MOUSE_WHEEL`, delta scaled ×120 per notch (`SDLApplication.cpp:432-440` — the 120-unit legacy `WHEEL_DELTA` ABI; trackpad fractional flicks survive the int cast).
2. `ConsumePlatformEvent` → `EmulateInput(DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_Z, event.y, time, 0)` (`InputAPI.cpp:1092-1094`). Note `nParam=0` — the wheel event carries **no modifier info** into the legacy device model.
3. Pump → `EventCame` → `CControlAxis::ChangeState` (`InputAPI.cpp:70-81`): `nOffset = newState - nAbsPos` (accumulates across same-direction notches: 120, 240, …), `NotifyAllCombos(true)`.
4. Combos on `MOUSE_AXIS_Z` fire in descending control-count order (`SCombosCmp`, `InputBinder.cpp:49-55`): a 2-control `{LSHIFT, MOUSE_AXIS_Z}` combo is notified **before** the 1-control `{MOUSE_AXIS_Z}` combo.
5. Slider binds drive `SCommand::ActivateSlider` → `CAccumulator::ActivateAxis` (a plain sum, `InputTypes.h:99-107`) → `CInputSlider::GetDelta` = accumulated delta × power × 0.001 (`InputSlider.cpp:23-30`). Per notch: `120 × 40 (Power, defconf.cfg:292-294) × 0.001 = 4.8`.
6. `CUIScreen::Update` polls `pMouseWheelSlider->GetDelta()` when the screen is the top UI screen (`UIScreen.cpp:526-534`) and forwards to `OnMouseWheel`. `InterfaceOptionsSettings.cpp:77` / `InterfaceCloudCredentials.cpp:1150` create their own `mouse_wheel` sliders for list scrolling.

### Option (a) — CCombo bind `zoom_wheel = slider minus LSHIFT+MOUSE_AXIS_Z`: **FEASIBLE and recommended**

- `AddBindLocal` (`InputBinder.cpp:326-409`) resolves controls by name; `MOUSE_AXIS_Z` and `LSHIFT` both exist as named controls in the emulated device tables (`InputAPI.cpp:489-509` — mouse controls registered unqualified, keyboard bare). Keyboard `LSHIFT` is a `CControlKey` — `IsActive()` is true while physically held; combo formation is purely `IsActive()`-based (`CCombo::NotifyControlStateChanged`, `InputBinder.cpp:139-167`), **independent of the dead UI keyboard-state tracking**.
- **Suppression is automatic and correct**: when the `{LSHIFT, MOUSE_AXIS_Z}` combo is registered, `AddBindLocal` finds the existing `{MOUSE_AXIS_Z}` combo as a SUBSET (`Compare`, `InputBinder.cpp:179-197`) and adds it as suppressive (`382-385`). On each notch the 2-control combo (notified first, per the sort) suppresses the 1-control combo before it can notify → the bare `mouse_wheel` slider receives **nothing** while Shift+wheel is active. Between notches the axis control deactivates (per-pump `Deactivate`, `InputAPI.cpp:1278-1296`) and the subset un-suppresses. This delivers "wheel+SHIFT = zoom, wheel alone = existing behavior" with **zero interception code**.
- Add **both** `LSHIFT+MOUSE_AXIS_Z` and `RSHIFT+MOUSE_AXIS_Z` binds — the config convention pairs left/right modifiers everywhere (`defconf.cfg` e.g. 947-968, 1268-1329), and a single-sided bind leaves right-shift users behind.
- Default-section inheritance guarantees the subset relationship exists in `game_mission`: every non-default section also receives the default section's binds at config-load time (`InputBinder.cpp:547-551, 589-592`).
- **Config migration**: `CInputBinder::Repair` (467-568) merges binds present in defaults but missing from the user's saved config, matched by name+type+controls, per existing section (comment at 521-525) — new `game_mission` binds reach existing installs.
- Poll the new `zoom_wheel` slider in `CInterfaceMission::StepLocal` (`iMissionInternal.cpp:1356`), not `CUIScreen::Update` — `StepLocal` runs every frame regardless of overlay stacking, and the mission screen owns zoom. Accumulate deltas and apply one step per 4.8 (the notch quantum).
- **Known cosmetic side effect**: `{LSHIFT}` (bare) is also a subset → while a Shift+wheel notch is formed, `add_action_on`/`add_action_off` messages flap (`CCombo::Suppress` re-notifies on suppress/unsuppress, `InputBinder.cpp:94-108`). Harmless (no click is in flight during wheel scrolling) but worth a comment in the plan.

### Option (b) — intercept in `CUIScreen::Update` via `m_keyboardState`: **A TRAP**

`E_SHIFT_KEY_DOWN` is set in `CUIScreen::OnChar` (`UIScreen.cpp:297-319`). But `OnChar` is driven by `ProcessTextMessage` (`InterfaceScreenBase.cpp:363-374`) ← `pInput->GetTextMessage` — and text messages are only produced when `eTextMode != INPUT_TEXT_MODE_NOTEXT` (`InputAPI.cpp:1010-1045` event path; `1231-1234` legacy path). Missions always set `INPUT_TEXT_MODE_NOTEXT` (`InterfaceScreenBase.cpp:690`). **In a mission `OnChar` never fires and `m_keyboardState` is permanently `E_KEYBOARD_FREE`** (the `E_KEYBOARD_FREE` passed at `InterfaceScreenBase.cpp:367` is a tell). The `ProcessMessage` handler for `UI_CLEAR_KEYBOARD_STATE` (342-346) likewise never helps. Any Shift-tracking for zoom must use the combo bind (a) or a raw poll (`GetAsyncKeyState(VK_SHIFT)` in `StepLocal` — precedent: the disabled game-speed fallback `iMissionInternal.cpp:1364-1379`, disabled specifically because it double-fired with binds).

### OnChar shift-tracking correctness (R3 sub-question)

Moot in missions (above), but for completeness: `OnChar`'s shift handling is correct *where it runs* — sets on `bPressed` (299-302), XOR-clears on release only if set (303-304) — idempotent and both-edge correct.

---

## R4. Zoom state ownership, resets, and per-mission view state

### Ownership: global var `GFX.World.ZoomSteps` (int step count)

Reasoning (expands §R1): all consumers are free-function global readers across module boundaries (Scene, GameTT, GFXGPU); the established cross-module state channel is global vars (`GFX.World.BaseSizeX/Y` itself, `GFX.Present.Fit`, `GFX.Drawable.SizeX/Y`). Int steps keep clamping/exactness trivial; derive `z = STEP_FACTOR^steps` (store the factor too, or hard-code with the steps). Precedent for float globals: `SetGlobalVar("Perf.FormVisMs", float)` (`SceneDraw.cpp:318`).

There is no other per-mission *view* state stored as globals — the war-fog/show toggles are `SCENE_SHOW_*` flags on the `CScene` object (`pScene->ToggleShow`, `iMissionInternal.cpp:1766-1797`), destroyed with the scene each mission. The zoom global survives between missions (harmless — only mission code reads it) but **must be explicitly reset** because save/load would otherwise inherit the pre-load session value even though it isn't serialized.

### D-16 reset points (all three events)

- **Mission start / restart:** `CInterfaceMission::Init` (`iMissionInternal.cpp:813-839`). Restart funnels through `CICMission::PreCreate/PostCreate` (`iMissionInternal.cpp:208-213`) → fresh `CInterfaceMission` → `Init`. One reset here covers both.
- **Save/load:** the `CMD_LOAD_FINISHED` handler (`iMissionInternal.cpp:1861-1870`) — it already re-runs `ChangeResolution` + `CheckResolution` + `pUIScreen->Reposition` + `SetMissionCameraPlacement`; add the zoom reset there. (Load does not re-run `Init` — it deserializes into the live interface; `CCamera::operator&` at `Camera.cpp:146-168` shows the load path pattern.)
- **Not in `Done()`** — `CInterfaceMission::Done` (`840-874`) already wipes `temp.*` globals (843) but resetting at the *start* of the next mission is the defensive place.

### D-15 (resolution change keeps zoom) — automatic

`ChangeResolution` (`InterfaceScreenBase.cpp:451-676`) is a per-frame diff that never touches the zoom global; the `z_max` clamp is computed from live `cfg_eff`/drawable at read time (§R1) → a mid-mission resolution change keeps the step count and re-clamps the effective viewport for free. Verify the clamp is applied at *read* (or at `ChangeResolution`-diff time), not only at step time.

---

## R5. J/K/L binds and hold-to-repeat

### Bind format (`defconf.cfg` `game_mission` section, line 891+)

Confirmed format (every entry in the section, e.g. lines 893-901, 925-931):

```xml
<item>
    <Name>zoom_in</Name>
    <Type>event down</Type>
    <Controls>
        <item>J</item>
    </Controls>
</item>
```

Same for `zoom_out`/`K` and `zoom_reset`/`L`. Key names `J`/`K`/`L` exist in both backend key tables (`InputCodes.cpp:21`, DIK map `InputAPI.cpp:134-136`). J/K/L verified unreferenced elsewhere in `defconf.cfg` (matches D-03).

### Command registration

- Add three IDs to `EMissionCommands` (`Sources/src/GameTT/iMission.h:161-249`): free values in the `0x002000xx` block — `MC_CANCEL_CREDITS` ends at `0x00200073`; use `0x00200074/75/76`. (The later `0x002004xx` block is in use through `0x00200419`.)
- Register in `missionCommands[]` (`iMissionInternal.cpp:127-172`) — **outside** the `#if !defined(_FINALRELEASE)` guard (131-148) since these are user-facing; the guard is only for dev-only commands like `show_grid`.
- Registration happens in `CInterfaceMission::Init` via `missionMsgs.Init(pInput, missionCommands)` (line 828) with `SetBindSection("game_mission")` at 835.
- Dispatch: message arrives → `CInterfaceScreenBase::ProcessMessage` → `ProcessMessageLocal` switch (`iMissionInternal.cpp:1749`, cases at 1764+). Add `MC_ZOOM_IN/OUT/RESET` cases there.

### Hold-to-repeat (D-08): event-down + OS auto-repeat is sufficient

`CControlKey::ChangeState` (`InputAPI.cpp:22-46`) has **no same-state guard** — it calls `NotifyAllCombos` on every event, and SDL key-repeat events arrive as ordinary `keyDown`s (`ConsumePlatformEvent` never filters `event.repeat`, `InputAPI.cpp:998-1045`; `SDLApplication.cpp:410` carries the repeat flag but nothing consumes it). So a held J re-fires the `event down` command at the OS repeat rate with zero new timing code — which is literally what D-08 asked for ("keyboard auto-repeat style"). If the 150–250ms target must be exact, the alternative is a timestamp accumulator in the `MC_ZOOM_*` handlers — but OS repeat (default ~500ms delay then ~30/s) is within spirit; planner picks. The engine's own `GenerateRepeats` (`InputAPI.cpp:1429-1462`) is text-mode-only — irrelevant in missions (`NOTEXT`).

Slider-plus/minus binds (the `camera_forward` pattern, `defconf.cfg:975-1000`) were considered for hold-to-repeat: they integrate power×time via `CKeyAccumulator` (`InputTypes.h:74-98`), giving *continuous* zoom — wrong for D-05's discrete steps. Event-down is the right bind type.

**Pause interaction (landmine):** `ProcessMessageLocal` blocks only `nEventID < 512` while paused (`iMissionInternal.cpp:1751-1759`); `MC_ZOOM_*` (0x002000xx) passes the guard. Zoom commands must additionally check `GetPauseReason() > PAUSE_TYPE_NO_CONTROL` (the same gate `CCamera::Update` uses, `Camera.cpp:60-66`) so zooming doesn't work while paused.

**Wheel-step equivalence (D-06 discretion):** one notch (4.8 slider units) = one step; accumulate fractional deltas for high-resolution wheels.

---

## R6. Minimap fixed sizing (D-11..D-14)

### Measured legacy geometry (all from `Data/UI/mission.xml`)

The mission HUD is **BOTTOM|LEFT**-anchored (PositionFlag `0x0031` = UIPLACE_LEFT|UIPLACE_BOTTOM; confirmed by the comment at `UIScreen.cpp:84-91`), not bottom-right:

| Element | ID | Pos (legacy) | Size (legacy) | Notes |
|---|---|---|---|---|
| Minimap dialog | 5000 (`mission.xml:8277`, own pos/size at 8631-8632) | 0,0 | 264×155 | bottom-left cluster incl. button rail |
| Minimap element | 20000 (`mission.xml:8418-8421`) | 6,3 | 256×128 | diamond inside dialog; `FittedMiniMapSize` derives `nSize` from its wndRect |
| Command rail (12 buttons) | 100 (`mission.xml:6`, pos/size 1790-1791) | 265,0 | 114×88 | sits between minimap and status bar |
| Status bar (animated) | 40000 (`mission.xml:1939`, pos/size 2580-2581) | 387,0 | 413×85 | `MinPos x=-413` slide-out (`1942`), hidden by default (`VisibleFlag="0"`) |

Cluster total: x 0→800 of 1024 = **78% of width**. On 16:9 drawables the uniform `s_hud` keeps the cluster at a constant 58.6% of width; on 4:3, 78%. So "half the available width" is a real change everywhere, not a no-op.

### (a) Where wndRect comes from and the cleanest override point

`CUIScreen::Reposition` (`UIScreen.cpp:186-263`) computes the uniform layout scale `fScale = min(BaseSize.w/1024, BaseSize.h/768)` for the edge-anchored mission screen (214-222), applies it via `ScaleLayout(vDeltaScale)` (233) — which scales every window's `vPos`/`vSize` (`CSimpleWindow::ScaleLayout`, `UIBasic.cpp:371-408`) — then `CMultipleWindow::Reposition` (`UIBasic.cpp:1706-1733`) recursively computes each child's `wndRect` from `nPositionFlag` + scaled `vPos`/`vSize` (`CSimpleWindow::Reposition`, `UIBasic.cpp:580-673`). The minimap's `nSize` is re-derived from wndRect every frame in `Draw()` (`UIMiniMap.cpp:996`) and every click (`1159`) via `FittedMiniMapSize()` (`UIMiniMap.h:147-152`) — **the diamond follows the element rect automatically**; only the element's `vSize` needs overriding.

**Cleanest override: a `CUIMiniMap::Reposition` override** (currently inherits `CSimpleWindow::Reposition` with no override) that (1) calls the base, (2) adjusts `vSize` (and `vPos` if needed) to the D-11-computed size, (3) recomputes its own `wndRect`, (4) recreates textures (below). Alternative: reposition-time fixup in `CInterfaceMission::CheckResolution`/`CMD_LOAD_FINISHED` after `pUIScreen->Reposition` — but a virtual override keeps it self-contained and catches every reposition path (`CScene::Reposition` at `SceneInternal.cpp:1121-1128` runs on every resolution change; `CMD_LOAD_FINISHED` at `iMissionInternal.cpp:1867`).

### (b) Does resizing one element scale cleanly?

For the **minimap element itself: yes** — it's a childless `CSimpleWindow` (`CUIMiniMap : CSimpleWindow`, `UIMiniMap.h:126`); `nSize` re-derives per frame; the diamond math (`PointToTextureMiniMap`, `GetZeroPoint` with the frame-centering addition at `UIMiniMap.cpp:225-227`) is nSize-driven; hit testing (`TextureMiniMapToPoint`) shares the mapping. The diamond's 2:1 invariant is preserved if the override scales width and height by the same factor (docs/scaling.md closing warning: anisotropic scaling breaks the diamond).

For the **dialog (5000): no** — resizing it does not re-layout its absolutely-positioned children (buttons at fixed 0..263 x-positions; `ScaleLayout` only runs at screen-scale changes, not per-element). Any dialog resize requires manual child re-layout in code — this is the UI-work core of the rule (see (c)).

### (c) Texture recreation sequencing on resolution change — REQUIRED, currently missing

`CreateMiniMapTextures` (`UIMiniMap.cpp:78-140`) sizes `pInstantObjects` from `GetNextPow2(wndRect)` (line 87). It is called only from `SetTerrainSize` (492-498) and savegame deserialize (`operator&`, 178-181) — **never on reposition**. After a mid-mission resolution change the element rect changes but the overlay texture keeps the old pow2 size. UV mapping (`tU = miniMapPoint.x / textureSize`, `UIMiniMap.cpp:1039-1040`) tolerates a *larger* texture, but growth past the old pow2 (shrink→grow sequences) silently breaks. The fix (D-12 already demands it): recreate textures when the reposition changes wndRect — the `Reposition` override from (a) is the single hook. The 2026-07-26 pow2 invariant (`isLeftTop=true` write sites, `CMarkPixelFunctional` no-flip, `UIMiniMap.h:116-122`) is transform-identity-based and survives any texture size — do not reintroduce a flip.

### (d) Status bar (40000) and the 50% rule — THE ARITHMETIC MUST BE PINNED

The rule as literally written (diamond + status bar = exactly 50% of drawable width, panel content fixed, minimap flexible — D-11) does **not close at low resolutions**:

- At 1024×768: 50% = 512. Status bar alone = 413 (its text fields are 385 wide, `mission.xml:1974,1995`). Remaining for minimap+dialog = 99px — smaller than the dialog's button rail (263px wide) with no redesign (forbidden by D-13). The status bar is also *hidden by default* (`VisibleFlag="0"`) and force-hidden below 800px drawable width (`CheckResolution`, `iMissionInternal.cpp:887-914` — which reads the **world base**, not the drawable, at 897-898).
- Candidate interpretations the planner must choose between (or re-confirm with the user):
  1. **Cluster rule**: minimap dialog + rail + status bar together = 50% (today 78%/58.6%) → on 16:9 this means closing the 123px legacy gap (387−264) and modest shrink; diamond ≈ 256 stays near-legacy. This reading is consistent with D-13's "keep the original arrangement" and is the only one that produces sane geometry at 1024.
  2. **Literal rule with hidden-panel exception**: diamond = 50% alone when the panel is hidden; when visible, panel keeps 413·s_hud and the diamond shrinks to the remainder (→ ~99px at 1024; contradiction with the button rail unless the dialog itself shrinks).
  3. **Drawable-base vs cfg-base**: "50% of the drawable width" — but the HUD cluster is sized by `s_hud` (cfg base). At cfg < drawable (low-res cfg on big screen) the two bases diverge; the plan must state which base the 50% is measured against (drawable = literal D-11; cfg_eff = keeps HUD self-consistent).
- **Status bar content tolerance**: its internal widgets are absolute-positioned (`WindowPos` children at `mission.xml:1945-2578`); it has no internal reflow. Resizing it horizontally stretches only its TileRect background; the 385-wide text fields and icon columns would need the same manual re-layout as the dialog. CheckResolution's hide-below-800px rule and the show/hide animation (`MinPos -413`, `CMultipleWindow::Update` animation block `UIBasic.cpp:1581-1676`) must keep working with the new size — the animation offsets derive from `vMinPos/vMaxPos` (scaled by `ScaleLayout`, `UIBasic.cpp:1734-1749`).

**Recommendation to planner:** treat (d) as the first plan task; write the arithmetic for one interpretation, and if interpretation 1 is chosen, verify D-11's "exactly" survives (the gap-closure gets to ~51% at 16:9 — decide whether "exactly" permits pinning the status bar x-position to `dialog_width` rather than 50%).

---

## R7. Fractional-zoom risk audit — every `GetGameplayScale` consumer

Complete caller inventory (grep-verified) and verdict for a stepped ~1.2× multiplier making `s` fractional:

| # | Consumer | Site | Verdict |
|---|---|---|---|
| 1 | `CScene::UpdateTransformMatrix` | `SceneInternal.cpp:404-420` (via `CreateGameplayProjectionMatrix` 411) | ✅ recomputes per call; world↔screen round-trip invariant holds (both `GetPos2`/`GetPos3` use the same matrix) |
| 2 | `CScene::GetPos3` | `SceneInternal.cpp:935-959` (941) | ✅ same matrix; cursor-anchor math (R2) relies on this |
| 3 | `CScene::Draw` projection setup + terrain reset heuristic | `SceneDraw.cpp:257-274` (261) | ⚠️ **L3 landmine** — the reset heuristic (264-274) keys on screen size + projection-*bool* only; a zoom step changes neither |
| 4 | `CDrawVisitor::Init` (`fSpriteScale`) | `DrawVisitor.cpp:24` | ✅ per-frame; sprite culling rects + sizes scale; no LOD system exists to break |
| 5 | `CTerrain::DrawWarFog` | `TerraDraw.cpp:129-159` (140, 148) | ✅ per-frame recompute; vertices re-scaled from cached patch data each frame |
| 6 | `CTerrain::ReBuildMeshes` | `TerrainInternal.cpp:363-404` (390, cached per rebuild 321-327) | ⚠️ same L3 — rebuild is only triggered via `MovePatches` on anchor change (`TerraDraw.cpp:230-235`); zoom must force `pTerrain->ResetPosition()` |
| 7 | `DrawSingleSpritesPack` / `DrawComplexSpritesPack` UV shift ÷ scale | `SceneDraw.cpp:968-1043` (975-1005), `1044-1121` (1051-1070) | ✅ the existing fractional fix divides the half-texel shift by the live `fSpriteScale`; fractional player zoom rides it |
| 8 | Shadow pass (effect 111) fractional switch | `GraphicsEngineGpu.cpp:1013-1030` + `SceneDraw.cpp:984-991, 1061-1065` (`g_bShadowSpritePass` inset) | ⚠️ **L1 landmine** — mirror must include `z` |
| 9 | `CNormalDepthCalculator` (particles) | `SceneDraw.cpp:1127` | ✅ live scale argument |
| 10 | `DrawSingleParticlesPack` | `SceneDraw.cpp:1147-1154+` | ✅ live scale |
| 11 | Sprite pick unscale | `SpriteVisObj.cpp:70` (`UnscaleGameplaySpritePoint`) | ✅ divides by live scale |
| 12 | `FrameSelection` | `FrameSelection.cpp:16-18` | ✅ live scale |
| 13 | `GetGameplayScreenRect` | `SceneScreenScale.h:97-109` | ✅ **no external callers** (grep: definition only) |
| 14 | Minimap camera-frame polygon | `UIMiniMap.cpp:462-490` (`GetClippedScreenFrame` via `GetPos3`) | ✅ follows zoom automatically — this is D-14's "camera frame already follows zoom" |
| 15 | Sound culling | `SoundScene.cpp` — `InitScreenResolutionConsts` (1047-1078) scales *radii* from the world **base** (intentionally cfg-locked, like the HUD); per-frame culling at line 399 uses `GetScreenRect` + the scene transform | ✅/⚠️ radii staying cfg-locked is correct-by-design (zoom shouldn't change hearing); **verify** the per-frame screen-position culling routes through the gameplay (matTransform) projection, not the raw screen projection — flagged for implementation-time check |
| 16 | Terrain patch culling extents | `TerraDraw.cpp:247-250` (`fWidth/fHeight` from raw screen, no ÷s) | ✅ pre-existing over-inclusive culling (safe at any s; draws extra patches, no visual effect) |
| 17 | `ScaleGameplayScreenPoint` pixel snapping | `SceneScreenScale.h:48-68` | ✅ this snapping + the atlas insets **are** the fractional machinery; whole `s·z` products (e.g. z=2 at s=1) remain bit-exact |

No consumer assumes `s ∈ whole steps` beyond the two landmines above. `ScaleGameplayScreenPoint`'s early-out at `fScale <= 1.001f` and `CreateGameplayProjectionMatrix`'s at `s <= 1.001f` are unreachable from zoom (D-10 keeps `z ≥ 1`, `s ≥ 1`; only the degenerate 640×480-auto-at-z=1 case sits exactly at the boundary, which is today's behavior).

---

## R8. Testing / verification surface

### Automated (build + static)

- **Build**: VS Code task **`Build Game (Debug)`** (MSVC `Debug | Win32`, output `Sources/src/Game/Debug/Game.exe`) — the phase-1/4 precedent (`.planning/phase-1/VERIFICATION.md`, `.planning/phase-4/VERIFICATION.md` row "Game Debug build | PASS"). A `zig build -Dtarget=x86_64-windows-msvc` cross-compile exists (ROADMAP phase 6 notes) but the verified runtime path is MSVC Debug.
- Static checks: grep gates (e.g. "zoom global name appears in `GetGameplayScale`, `UpdatePresentOffsets`, `Init`, `CMD_LOAD_FINISHED`"), defconf bind presence, no `PointToTextureMiniMap(..., false)` reintroduction.
- Mission launch is scriptable: direct mission arg `Game.exe -<mission>.xml`, panic capture via stderr redirect, `Get-Process Game` liveness (ROADMAP MCP backlog entry). `BK_GFX_TRACE=1` (present offsets/world base), `BK_INPUT_TRACE=1` (bind/slider activation — the existing traces at `InputBinder.cpp:224-225`, `Camera.cpp:70-74` fire for the new binds), `BK_UI_TRACE=1` (reposition rects — will show the minimap override). PrintWindow capture of the occluded window (docs/scaling.md toolchain §3) can pixel-verify minimap size without OCR.

### Manual (in-game, per `.planning/phase-4/VERIFICATION.md` row style)

Resolutions to cover: **1024×768** (zero-adjacent zoom range sanity at 640×480 too), **640×480** (D-09: zero zoom range), **1920×1080** (already-fractional fill + zoom), **3440×1440 / 3880×1440-class** (large fill + HUD scaling). Scenarios: zoom in/out to both limits (bounds clamp), cursor anchoring over terrain and at map edges (clamp drift), wheel-alone unaffected, Shift+wheel (both shifts), J/K hold-repeat, L reset, mission restart/load reset (D-16), resolution change mid-mission zoomed (D-15), minimap diamond size + status bar interplay at each resolution (D-11/D-13 + the hide-below-800 rule), minimap camera-frame polygon during zoom (D-14), fractional-zoom visuals while scrolled at mixed zoom (terrain seams — the sign-off item docs/scaling.md reserves), shadow sprites at fractional zoom (trees/blotches), sound culling sanity.

---

## Landmines (ordered by severity)

1. **GPU mirror desync** — `UpdatePresentOffsets` (`GraphicsEngineGpu.cpp:1013-1030`) duplicates the zoom formula into `world_zoom_fractional_` (`GraphicsEngineGpu.h:149`). Any player-zoom variable must be multiplied there too, or fractional-zoom shadow artifacts (the 2026-08-13 grey-blotch class) return at whole-step bases. This is the one edit outside Scene/ that is *mandatory*.
2. **`m_keyboardState` is dead in missions** — `OnChar` never fires under `INPUT_TEXT_MODE_NOTEXT` (`InterfaceScreenBase.cpp:690`, `InputAPI.cpp:1010+`). Shift+wheel must go through CCombo binds (or a raw poll). Do not build on `E_SHIFT_KEY_DOWN`.
3. **Terrain mesh staleness on zoom** — the rebuild trigger (`SceneDraw.cpp:264-274` + `TerraDraw.cpp:230-235`) keys on screen-size/projection-bool/anchor-move. A zoom step at screen center (zero anchor delta) leaves stale terrain scaled at the old `s`. Zoom steps must call `pTerrain->ResetPosition()` (the exact mechanism the projection-change path uses).
4. **The 50% minimap rule doesn't close at 1024×768** with legacy geometry (§R6d) — pin the interpretation (cluster vs literal; drawable vs cfg base; hidden-panel case) before writing sizing code. Also `CheckResolution`'s hide-below-800 rule reads the world base, not the drawable (`iMissionInternal.cpp:897-898`) — decide whether that rule stays base-locked.
5. **Minimap overlay textures don't resize on reposition today** — `CreateMiniMapTextures` only runs on `SetTerrainSize`/deserialize (`UIMiniMap.cpp:492-498, 178-181`); D-12's resize needs the new hook or shrink→grow sequences break overlay UVs.
6. **Savegame global-var policy** — zoom must be `GFX.`-prefixed to stay out of saves (`GlobalVars.h:124-155`), *and* explicitly reset at `Init` + `CMD_LOAD_FINISHED` (load keeps live session values by design). A `temp.`-prefixed var would be serialized into saves and restored on load — violating D-16.
7. **Zoom while paused** — the pause guard in `ProcessMessageLocal` only blocks event IDs < 512 (`iMissionInternal.cpp:1751-1759`); `MC_ZOOM_*` sails through. Add an explicit pause gate to match `CCamera::Update`'s behavior.
8. **Suppression side effects** — the `{LSHIFT, MOUSE_AXIS_Z}` combo suppresses *both* `{MOUSE_AXIS_Z}` (wanted) and `{LSHIFT}`→`add_action_on` (benign flap of add-mode messages during Shift+wheel; no click in flight). Also: only a `slider minus` bind gives wheel-down; `slider plus` on the same combo gives wheel-up — mirror the existing `mouse_wheel` single minus bind carefully (the bare bind is minus-only, `defconf.cfg:371-377`, because the axis power is negative per notch direction — actually the notch sign arrives in the value; **one combo bind of either type receives both directions** via `fPower` sign — verify at runtime which of plus/minus fits "wheel up = zoom in" per D-02).
9. **Anchor snapping quantization** — `Camera.cpp:92-97` snaps `vAnchor1` to whole/even steps in the rotated frame; cursor anchoring inherits ≤ ~2-world-unit drift per step. Acceptable; do not bypass. Related: at map-edge clamps (`Camera.cpp:87-91`) the anchored point drifts — expected RTS behavior, document in the plan's verification notes.

---

## Validation Architecture

### Automatable (build/static/trace)

| Check | Method | Command / evidence |
|---|---|---|
| Clean Debug build | VS Code task | `Build Game (Debug)` → `Sources/src/Game/Debug/Game.exe` produced |
| Zoom plumbing present at all sync points | grep gate | `GetGameplayScale` composes `z`; `GraphicsEngineGpu.cpp:1013-1030` mirror includes it; resets at `iMissionInternal.cpp:813` + `CMD_LOAD_FINISHED` |
| Binds + commands registered | grep gate | `defconf.cfg` `game_mission` has `zoom_in/zoom_out/zoom_reset` + wheel combo binds (both shifts); `missionCommands[]` has the three entries outside the `_FINALRELEASE` guard; `EMissionCommands` has `MC_ZOOM_*` |
| No overlay-transform regression | grep gate | no new `PointToTextureMiniMap(..., false)` / no `CMarkPixelFunctional` flip |
| Runtime smoke | scriptable launch | `Game.exe -<mission>.xml` with `BK_GFX_TRACE=1 BK_INPUT_TRACE=1 BK_UI_TRACE=1`; traces show: bind activation for J/K/L/Shift+wheel; world-base unchanged on zoom; present offsets identity; reposition trace shows minimap override applied once per resolution change |
| Zoom math unit-shape (optional) | trace assertion | log `s`, `z`, `z_max` per zoom step; assert `z ∈ [1, z_max]`, `z_max == 1` at 640×480 cfg |

### Manual in-game (required sign-off rows)

| Row | Resolutions | What to verify |
|---|---|---|
| zoom-bounds | 640×480, 1024×768, 1920×1080, 3440×1440 | zoom-in stops exactly at 640×480-effective (D-09); zoom-out never past un-zoomed (D-10); zero range at 640×480 cfg |
| cursor-anchor | all above | world point under cursor fixed during J/K/wheel zoom (D-07), incl. near map edges (clamped drift documented) |
| input matrix | any | wheel alone = old behavior; Shift+wheel (L and R shift) = zoom; bare wheel suppressed during Shift+wheel; J/K hold-to-repeat (D-08); L resets (D-01) |
| fractional-zoom visuals | 1920×1080 (mixed fractional s·z) | no terrain seam lattice while scrolled at zoom; no grey blotches around tree shadows; sprite edges clean |
| minimap rule | all above | diamond + status bar per the pinned D-11 arithmetic; diamond intact 2:1; camera-frame polygon tracks zoom (D-14); status bar hide/show animation and <800px rule still work |
| resolution-change matrix | any → any, mid-mission, zoomed | zoom step count kept, viewport re-clamped (D-15); minimap textures recreated (no overlay offset — the 2026-07-26 bug class) |
| persistence | any | zoom resets at mission start, restart, and save/load (D-16); zoom absent from savegame files |
| regression | 1024×768 Auto, z=1 | bit-for-bit identical rendering to pre-phase (whole-step early-outs intact), tutorial playable end-to-end |

## RESEARCH COMPLETE
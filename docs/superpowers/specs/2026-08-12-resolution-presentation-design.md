# Resolution & presentation rules

Date: 2026-08-12. Agreed with Johannes in-session; supersedes the
"explicit resolution is the real render size everywhere" model of commit
5960e1c8 for how each screen type uses the configured resolution.

## Intent

The configured resolution (`GFX.Mode`) stops being a literal render size.
Each screen type uses it differently:

- **Menus and videos** render at `cfg_eff = min(cfg, drawable)` per axis
  (revised 2026-08-12, second pass): a resolution lower than the screen
  shows 1:1 inside a black frame; a resolution larger than the screen just
  means screen resolution. The present blit is 1:1 in steady state — no
  scaling ever, so nothing is soft and nothing is left for the OS to
  stretch. The shrink-only fit stays only as a transient safety net during
  live resizes.
- **Missions** always render at the drawable size (window or monitor).
  The resolution acts as the *view and HUD base*: the world shows the region
  a configured-resolution screen would show (projection zoom, rendered
  natively — sharp at every factor), and the HUD keeps the size it would
  have at the configured resolution while being anchored to the real
  drawable edges. The HUD is therefore never resampled by the present
  blit, never cropped, and never pushed off screen; its size follows the
  resolution setting alone.
- A resolution larger than the drawable is clamped: missions behave as if
  the resolution were the drawable size. Nothing scales down, nothing clips.

## Definitions

- `drawable` — pixel size of the surface actually presented into: the
  window's pixel size in windowed mode, the display's in fullscreen.
- `cfg` — the configured resolution (`GFX.Mode`); `Auto` resolves to the
  drawable at mode-set time, as today.
- `cfg_eff = min(cfg, drawable)` per axis — the clamp rule.
- `s_hud = min(cfg_eff.w / 1024, cfg_eff.h / 768)` — the legacy design-base
  scale factor, exactly how `NSceneScreenScale`/`ScaleLayout` compute it
  today, but from `cfg_eff` instead of the scene size.
- `f = min(drawable.w / cfg_eff.w, drawable.h / cfg_eff.h)` — the uniform
  fill factor (≥ 1 by construction).

## Behavior per screen type

### Menus, videos, overlays (`szInterfaceType != "Mission"`)

- Scene size: `cfg_eff = min(cfg, drawable)` per axis (revised: was raw
  `cfg`; an oversized resolution now behaves as the screen resolution
  instead of rendering large and shrinking).
- UI layout scale: derived from the scene as today (`ScaleLayout`).
- Present blit: **shrink-only aspect-fit** — scale factor
  `min(1, fit_scale)`, centered, black bars. This replaces today's
  unrestricted aspect-fit, which upscaled low resolutions. Example: at
  800x600 on a 1440x900 display the menu is shown 1:1 in a black frame;
  today it is stretched to fill the screen.
- "Current" overlays keep inheriting the presentation of the screen below
  them, as today.

### Missions (`szInterfaceType == "Mission"`)

- Scene size: `drawable`, re-adopted live when the window is resized.
- World projection (`NSceneScreenScale`): scaled so the visible world
  region a `cfg_eff` screen shows today, rendered into the drawable — net
  ortho scale `s_world = legacy_step(cfg_eff) * f`, where `legacy_step` is
  the existing whole-step base factor (preserves Auto exactly) and `f` is
  the uniform fill. `f` is uniform, so no distortion; the non-limiting axis
  shows slightly more world.
- HUD (`ScaleLayout` + reposition): layout scale `s_hud`; anchors
  reposition against the drawable-sized scene rect. Result: HUD size
  follows the resolution setting ("scales down" at low resolutions on big
  displays), position always inside the visible area.
- Present blit: identity in steady state (scene == drawable). The existing
  centered-1:1 path stays as the degenerate case covering transient frames
  during live resizes.

### Window sizing

- Windowed: the resolution option sets the window size at mode changes,
  clamped to the display's usable bounds (`SDL_GetDisplayUsableBounds`) so
  the window — and with it the drawable — can never extend off screen. The
  window is not user-resizable: its size comes from the applied preset only
  (decided 2026-08-12; removes the drag-vs-preset edge cases).
- Fullscreen: unchanged (including the deferred fullscreen-entry fix of
  2026-08-12). Invariant on every platform: fullscreen never changes the
  display mode — the drawable is always the display's current native
  resolution, and every scale or letterbox decision happens in our present
  blit, never in the OS or monitor scaler. (Concretely: no
  `SDL_SetWindowFullscreenMode` calls; SDL borderless-desktop fullscreen
  only.)

## Plumbing

- The engine already reads the window pixel size every frame in
  `UpdatePresentOffsets`; it additionally publishes it as globals
  (`GFX.Drawable.SizeX/Y`) and raises a change flag mirroring
  `GFX.DisplayChanged` when it changes.
- `CInterfaceScreenBase::ChangeResolution` computes desired scene size per
  the rules above; the active screen re-runs it when the drawable changes
  (same assert-your-mode pattern screens use today).
- The world/HUD split lives in the existing mechanisms: `NSceneScreenScale`
  gains the `cfg_eff`/`f` inputs; `ScaleLayout` reposition for mission
  screens uses `s_hud` with the drawable rect. No new render targets, no
  HUD compositing pass.
- The options screen needs no changes. The resolution option's meaning
  becomes: window size in windowed mode; HUD size and world view base in
  missions; menu/video render size (never upscaled, capped at the screen).
- Changing the resolution in the options screen takes effect immediately:
  the active screen runs its mode diff every frame (`ChangeResolution` is
  already a no-op diff when nothing changed), not only on focus changes.

## Error handling & edge cases

- `Auto` resolution: `cfg = drawable` → `s_hud` as today, `f = 1`,
  menus 1:1 borderless — no visible change from today's Auto behavior.
- Drawable shrunk below `cfg` (display change only): `cfg_eff` re-clamps,
  HUD rescales down with it, world re-zooms; nothing clips.
- Drawable shrunk very small (display change only): `s_hud` can drop below 1
  (HUD smaller than design size) — acceptable; controls stay visible and
  proportional.
- Aspect mismatch (4:3 resolution on 16:10 display) in missions: uniform
  `f` fills the screen; the wider axis shows more world. In menus: bars on
  the wider axis.
- Display switch / DPI change mid-session: drawable change flag fires, the
  active screen re-runs `ChangeResolution`, everything re-derives.

## Testing

- `BK_GFX_TRACE=1`: menu presents must show `scale <= 1` and centered
  offsets; mission presents identity (`offset 0,0 scale 1,1`).
- Numeric world↔screen round-trip stays exact (the `CScene::GetPos3` /
  `UpdateTransformMatrix` invariant from docs/scaling.md) with the new
  `s_world` — re-verify at 1024x768-on-1440x900 and Auto.
- Visual sign-off by Johannes: HUD size at explicit low resolution, menu
  bars, terrain seam check while scrolling at a fractional `fill`. "Live
  window resizing in a mission" is stale as a sign-off item now that the
  window is not user-resizable (decided 2026-08-12, Window sizing above);
  replaced by two scenarios that exercise the same drawable/world-base
  plumbing without manual dragging: a display change while a mission is
  running (unplug/replug, resolution/arrangement change, or moving the
  window to another display in a multi-monitor setup) re-derives `cfg_eff`,
  the world zoom and the HUD base against the new drawable with no clipping;
  and changing the resolution from the in-mission options overlay applies
  instantly (world zoom and HUD both re-scale on the very next frame, no OK
  press or screen close needed).
- Update `docs/scaling.md` with the new mechanism once implemented.

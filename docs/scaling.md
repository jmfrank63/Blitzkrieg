# Scaling: how the port runs a 1024×768 game at higher resolutions

The original Blitzkrieg engine was authored for exactly 1024×768. This fork
runs it at higher resolutions, and at fullscreen/windowed drawable sizes that
differ from the configured resolution, by scaling the world view and the UI.
That works through a small number of mechanisms — and it breaks wherever the
original code quietly assumed the 1024×768 numbers. This document records the
mechanisms, the assumptions we have already hit, and how they were diagnosed
and fixed, so the next scaling problem starts from knowledge instead of
archaeology.

The fullscreen / monitor viewport work this document used to flag as
"planned" landed 2026-08-12: menus and videos now render at a screen-clamped
size, missions render at the drawable with a resolution-derived world zoom
and HUD base, and the window is fixed-size (resolution presets only, no
manual drag-resize). The full design rationale, the formulas below, and the
edge cases they resolve live in
`docs/superpowers/specs/2026-08-12-resolution-presentation-design.md`; this
document explains how that design maps onto the mechanisms already described
here.

## The scaling mechanisms

### Gameplay projection (`NSceneScreenScale`)

The 3D world view is scaled by rendering the gameplay scene through an ortho
projection of `(W/s, H/s)` where `s` is `NSceneScreenScale::GetGameplayScale()`
(`Sources/src/Scene/SceneScreenScale.h`). Originally `s = min(W/1024, H/768)`,
computed straight from the rendered scene size (`W`,`H` = `rcScreen`). Since
the 2026-08-12 resolution/presentation rules, missions always render at the
*drawable* (window or monitor pixel size, adopted 1:1 — see below), and `s` is
instead derived from the *configured* resolution:

```
s = legacy_step(cfg_eff) * fill
legacy_step(cfg_eff) = max(1, floor(min(cfg_eff.w/1024, cfg_eff.h/768)))
fill                 = max(1, min(drawable.w/cfg_eff.w, drawable.h/cfg_eff.h))
```

`cfg_eff` here is `GFX.World.BaseSizeX/Y` — the configured resolution clamped
to the drawable per axis (`min(cfg, drawable)`), published by
`CInterfaceScreenBase::ChangeResolution` only while a Mission screen is
active (0 for every other screen type). `legacy_step` is exactly the old
whole-step formula, now evaluated against `cfg_eff` instead of the raw scene
size, so `Auto` (`cfg_eff == drawable`) reproduces the original behavior
bit-for-bit — `fill` collapses to `1` and `s` is the same whole-step scale as
before. `fill` is what's new: a *fractional*, uniform zoom that closes the
gap between a whole `legacy_step` and the actual drawable, instead of leaving
unrendered margin whenever the drawable isn't an exact multiple of `cfg_eff`.
`fill` is uniform (not per-axis independent), so there is no distortion — the
non-limiting axis just shows a little more world.

This is still a view-only zoom: world coordinates, AI coordinates and cell
sizes are unchanged from the original. `CScene::UpdateTransformMatrix` and
`CScene::GetPos3` both use the same gameplay projection, so world↔screen
round trips are exact (verified numerically to ~1px, see below).

Because `fill` is fractional rather than a whole step, the terrain atlas —
the one part of the renderer that samples texture *tiles* per world cell
instead of stretching a single quad — can land a bilinear sample fraction of
a texel into a neighbour tile's cell; the old whole-step-only rule made this
impossible by construction, and retiring it for `fill` reopened the door.
The fix is a half-texel inward inset of every tile's/cross's atlas UV rect
(`CorrectUVMaps()` in `Sources/src/Scene/TerrainInternal.cpp`, called with
`0.5f / atlasSize` for both the tileset and crosset, *in addition to* — not
replacing — the pre-existing resolution-keyed legacy inset in the same
function). Visual confirmation that the seam is gone (scrolling at a
fractional `fill`, e.g. 1024×768 on a larger display) is a sign-off item, not
something this fix claims to have measured on its own.

### UI layout scaling (`ScaleLayout`)

Legacy UI layouts are authored in 1024×768 coordinates and scaled by the
layout scale vector at reposition time. Window rectangles (`wndRect`) scale
correctly, but *internal pixel metrics* of individual controls do not scale
automatically. Each control type that keeps its own pixel values needs an
explicit `ScaleLayout` override.

* Example already fixed: `CUITimeCounter` (the air-support availability bar)
  kept `fBegin/fEnd/fCurrent` in unscaled pixels, so the bar only filled to
  1/scale of its width. Fixed by adding a `ScaleLayout` override that scales
  those fields.

For *edge-anchored* screens (`bAnchorLayoutToScreenEdges` — the mission HUD,
`ui\mission`), `CUIScreen::Reposition` (`Sources/src/UI/UIScreen.cpp`)
derives the layout scale from `GFX.World.BaseSizeX/Y` — the same `cfg_eff`
world-zoom base described above — instead of from the screen rect it
repositions against: `fScale = min(base.w/1024, base.h/768)`. The screen
still lays out against the *full drawable rect*, so the HUD's anchors touch
the real window/monitor edges and are never cropped or pushed off screen;
only its *size* follows the configured resolution, "scaling down" on a low
resolution stretched across a big display. Canvas (non-anchored) screens —
every menu and video — are unaffected: they still derive their scale from
their own scene rect, which for those screens is `cfg_eff` itself (next
section), so the two paths agree when `cfg_eff == drawable`.

### Menus, videos and other non-Mission screens

Non-Mission screens (`szInterfaceType != "Mission"`) render at
`cfg_eff = min(cfg, drawable)` per axis — the same clamp Mission uses for its
world base, computed by the same `ChangeResolution`
(`Sources/src/Common/InterfaceScreenBase.cpp`). A configured resolution
smaller than the drawable renders 1:1 and is centered in a black frame by the
present blit's shrink-only aspect-fit (`GFX.Present.Fit=1` for these screens;
`GraphicsEngineGpu::UpdatePresentOffsets`, `scale = min(1, fit_scale)`); a
resolution larger than the drawable is capped at the drawable instead of
being rendered oversized and then shrunk. Either way the blit is 1:1 in
steady state — menus are never upsampled by the present blit, so nothing is
soft. Mission screens set `GFX.Present.Fit=0` (identity present, scene ==
drawable); the shrink-only-fit path only actually engages for Mission
transiently, during the one frame a live resize's `SetMode` hasn't caught up
yet.

### Drawable plumbing and the per-frame mode diff

`GraphicsEngineGpu::UpdatePresentOffsets` already reads the window's pixel
size every frame; it additionally publishes it as `GFX.Drawable.SizeX/Y` and
raises `GFX.DrawableChanged` (mirroring the pre-existing `GFX.DisplayChanged`)
whenever it changes. `ChangeResolution` is the single place that turns the
configured resolution plus the drawable into `cfg_eff`, `GFX.World.BaseSizeX/Y`,
the present-fit flag, and — for Mission — the actual `SetMode` request (the
drawable itself, so the scene always tracks the window/monitor 1:1).

`ChangeResolution` runs as a cheap, internally-diffed no-op every frame for
*every* interface screen on the interface stack (`CInterfaceScreenBase::Step`,
called for each stacked screen, not only the focused one, and not only on a
focus/display-change event). This is what makes a resolution change from the
options screen apply immediately, including from the in-mission options
overlay: that overlay is a separate screen instance with its own
`szInterfaceType` ("Current"), so the Mission screen underneath it only ever
notices the edited `GFX.Mode.*` globals on its own next per-frame step, not
via any direct call from the overlay.

### Window sizing

The window is fixed-size by design
(`Sources/src/Platform/SDLApplication.cpp` creates it without
`SDL_WINDOW_RESIZABLE`, decided 2026-08-12): its size comes only from the
applied resolution preset, clamped to the display's usable bounds
(`SDL_GetDisplayUsableBounds`) in windowed mode. A one-shot apply marker
(`GFX.Mode.<type>.AppliedSizeX/Y`, read/written in `ChangeResolution`) still
gates the windowed clamp bypass that lets an explicit resolution grow the
window past its current size — a leftover mechanism from when manual
drag-resizing also existed and had to coexist with preset changes; it is
harmless now that drag-resizing is gone; it simply makes each distinct
configured size apply exactly once. Fullscreen never changes the display
mode on any platform: the drawable is always the display's current native
resolution, and every scale/letterbox decision happens in the present blit,
never in the OS or monitor scaler.

## Coordinate systems cheat sheet

| Space | Units | Notes |
| --- | --- | --- |
| AI coords | 64 units per terrain cell | `TILE_SIZE = 32`, 2 tiles per cell |
| World (vis) coords | `fWorldCellSize = 32·√2 ≈ 45.25` per cell | `AI2Vis`/`Vis2AI` in `fmtTerrain.h` are exact inverses |
| Terrain cells | map is 112×112 on the intro map | `pTerrain->GetSizeX()` |
| Minimap widget | `nSize = wndRect` width (451 at 1804-wide; 256 at 1024×768) | diamond is `nSize × nSize/2` |
| Minimap overlay texture | `GetNextPow2(width) × GetNextPow2(height)` | **pow2 rounding is where the trouble lives** |

Minimap diamond mapping (square maps): `u = nSize·x/(2T)`, `v = nSize·y/(2T)`,
screen `X = u+v`, and Y comes in two variants selected by the `isLeftTop`
parameter of `PointToTextureMiniMap`:

* `isLeftTop = true` (default): `Y = (u−v)/2 + nSize/4`
* `isLeftTop = false`: `Y = (v−u)/2 + nSize/4` — the vertical mirror of the
  first about the diamond midline.

Engine corner mapping: world (0,0) → left corner, (0,T) → top, (T,0) →
bottom, (T,T) → right. The intro map's river runs lower-left → upper-right on
a correctly rendered minimap.

## Case study: the minimap overlay offset (fixed 2026-07-26)

**Symptom.** Every overlay drawn *into* the minimap's instant-objects texture
— aviation waypoint circles, counter-battery estimates, objective rings,
fire-range arcs — appeared ~30px south of its true position, at any spot on
the map, in both the zig and the MSBuild build. Unit dots, markers, the
camera frame and the terrain itself were all correct.

**Root cause.** Two pieces of original code cancel each other only at
1024×768:

1. The overlay write sites called `PointToTextureMiniMap(..., false)` — the
   mirrored-Y variant.
2. `CMarkPixelFunctional` (UIMiniMap.h), the pixel-plot functor used by
   `BresenhamEllipse`/`MakeLine2` for those overlays, flipped its row index:
   `[size.y - 1 - nYPos]`, where `size.y` is the **pow2 texture height**.

Mirror(about diamond midline `nSize/4`) followed by flip(about texture middle
`pow2H/2`) is the identity **iff `pow2H == nSize/2`**. At 1024×768:
`nSize = 256`, diamond height 128, `GetNextPow2(128) = 128` — identity, the
game shipped working. At 1804-wide: `nSize = 451`, diamond height 225.5,
`GetNextPow2(226) = 256` — the pair collapses to a constant southward shift of
`pow2H − nSize/2 ≈ 30px`.

Content written *directly* into the same texture (unit crosses, war fog)
never went through the functor, which is why only the circle/arc overlays were
displaced.

**Fix.** Make the pair identity by construction, resolution-independent:

* all overlay write sites use the default (`isLeftTop = true`) transform;
* `CMarkPixelFunctional` plots `[nYPos][nXPos]` with no flip.

**Beware:** if any new code draws through `CMarkPixelFunctional` or adds
`PointToTextureMiniMap(..., false)` calls, the pairing breaks again. The
`false` variant is still used legitimately by nothing in the overlay path;
`TextureMiniMapToPoint` (minimap click → world) uses the default variant and
matches the draw side.

## How it was diagnosed (reusable toolchain)

The bug resisted a dozen code-reading passes because every layer *looked*
correct in isolation. What finally cracked it was end-to-end numeric
instrumentation:

1. **Trace file mirror.** `NStr::DebugTrace` (Misc/StrProc.cpp) temporarily
   appends every trace line to `bk_trace.log` in the game's working directory
   — full logs, no console truncation.
2. **Chain traces.** One trace per joint: minimap right-click (local px →
   cells → world), the order handler (screen → world → cells), `AddCircle`
   (stored world → computed texture point), and the per-frame write (actual
   plotted point). Comparing adjacent joints isolates the corrupting step.
3. **Occluded-window capture.** `PrintWindow` with flag 2 captures the game
   window even when it is behind other windows; a cyan-pixel cluster scan
   converts the screenshot into widget-local overlay positions. Comparing
   *measured* pixels against *logged* write positions is what exposed the
   flip: written row 145 displayed at row 255−145 = 110, measured 110.8.
4. **Painted probes.** Writing known bands into the overlay texture (rows
   0–7 red, 100–107 green) verifies the texture→screen orientation
   independently of any game logic.
5. **Persistent circles.** Temporarily extending circle lifetime (5s → 60s)
   decouples "click, then measure" from the 5-second pulse window.

All of 1, 2 and 5 are temporary debug aids marked `[opt-diag]` and must be
stripped before committing.

## Known assumptions re-checked for the fullscreen / monitor viewport work

This list used to flag things to re-check *before* the fullscreen/monitor
viewport work started; that work is now done
(`docs/superpowers/specs/2026-08-12-resolution-presentation-design.md`), and
none of these turned out to be broken by it — recorded here as confirmed,
not as open risk:

* Any `GetNextPow2(...)`-sized surface whose content is addressed with
  layout-derived coordinates (the pattern of this bug). Grep for
  `GetNextPow2` before trusting a new resolution.
* Controls with internal pixel metrics lacking `ScaleLayout` overrides
  (the `UITimeCounter` pattern).
* `NSceneScreenScale` consumers: anything that mixes the gameplay projection
  with the raw screen projection (world↔screen conversions must all use the
  same matrix; `GetPos2`/`GetPos3`/`UpdateTransformMatrix` currently agree).
  This still holds with the resolution-derived `s = legacy_step(cfg_eff) *
  fill` formula above — both `UpdateTransformMatrix` and `GetPos3` call the
  same `GetGameplayScale()`.
* Hard-coded `1024`/`768`/`256`/`128` constants in UI code.
* The minimap window is authored 256×128 (exactly 2:1) in `mission.xml`; the
  diamond math assumes width = 2 × height survives scaling. Anisotropic
  scaling (X scale ≠ Y scale) would break it — the new `fill` factor is
  uniform (not per-axis independent) specifically to avoid introducing this.

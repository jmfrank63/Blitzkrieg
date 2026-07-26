# Scaling: how the port runs a 1024×768 game at higher resolutions

The original Blitzkrieg engine was authored for exactly 1024×768. This fork
runs it at higher resolutions (e.g. an 1804-wide window) by scaling the world
view and the UI. That works through a small number of mechanisms — and it
breaks wherever the original code quietly assumed the 1024×768 numbers. This
document records the mechanisms, the assumptions we have already hit, and how
they were diagnosed and fixed, so the next scaling problem (fullscreen /
monitor viewport is planned) starts from knowledge instead of archaeology.

## The scaling mechanisms

### Gameplay projection (`NSceneScreenScale`)

The 3D world view is scaled by rendering the gameplay scene through an ortho
projection of `(W/s, H/s)` where `s = min(W/1024, H/768)`. At 1804×1358 that
gives `s ≈ 1.76`. This is a view-only zoom: world coordinates, AI coordinates
and cell sizes are unchanged from the original. `CScene::UpdateTransformMatrix`
and `CScene::GetPos3` both use the same gameplay projection, so world↔screen
round trips are exact (verified numerically to ~1px, see below).

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

## Known assumptions to re-check for fullscreen / monitor viewport work

* Any `GetNextPow2(...)`-sized surface whose content is addressed with
  layout-derived coordinates (the pattern of this bug). Grep for
  `GetNextPow2` before trusting a new resolution.
* Controls with internal pixel metrics lacking `ScaleLayout` overrides
  (the `UITimeCounter` pattern).
* `NSceneScreenScale` consumers: anything that mixes the gameplay projection
  with the raw screen projection (world↔screen conversions must all use the
  same matrix; `GetPos2`/`GetPos3`/`UpdateTransformMatrix` currently agree).
* Hard-coded `1024`/`768`/`256`/`128` constants in UI code.
* The minimap window is authored 256×128 (exactly 2:1) in `mission.xml`; the
  diamond math assumes width = 2 × height survives scaling. Anisotropic
  scaling (X scale ≠ Y scale) would break it.

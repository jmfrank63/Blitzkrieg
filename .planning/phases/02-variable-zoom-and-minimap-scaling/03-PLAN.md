---
id: 03
phase: 2
title: Minimap fixed sizing — cluster rule, resolution-change-only resize, texture recreation
wave: 1
depends_on: []
files_modified:
  - Sources/src/UI/UIMiniMap.h
  - Sources/src/UI/UIMiniMap.cpp
autonomous: true
requirements: [D-11, D-12, D-13, D-14, LANDMINE-L4, LANDMINE-L5]
---

# Plan 03 — Minimap fixed sizing (cluster rule)

## Objective

Implement D-11's "minimap + command/unit status bar occupy half the drawable
width" as the **cluster rule**: the right-edge of the right-side HUD cluster
(minimap dialog 5000 + command rail 100 + status bar 40000, in the original
arrangement per D-13) is pinned to the drawable right edge, and the cluster
total width is `min(50% of the drawable width, what the cluster content
needs at that resolution)`. The minimap element (20000) is the flexible
member. The minimap resizes ONLY on resolution change (D-12) via a new
`CUIMiniMap::Reposition` override that recreates the overlay textures when
the element rect changes (LANDMINE-L5). The minimap stays decoupled from map
zoom (D-14 — the camera-frame polygon already follows zoom via GetPos3;
no code needed).

## The pinned arithmetic (LANDMINE-L4 — the "exactly 50%" resolution)

The literal rule does not close at legacy resolutions. Measured geometry
(Data/UI/mission.xml, all BOTTOM|LEFT-anchored, PositionFlag 0x0031):
minimap dialog 5000 at (0,0) 264x155 (minimap element 20000 inside at
(6,3) 256x128); command rail 100 at (265,0) 114x88; status bar 40000 at
(387,0) 413x85 (hidden by default, MinPos x=-413 slide-out). Cluster spans
x 0->800 of 1024 = 78%. Literal 50% would leave 99px for a diamond smaller
than the dialog's own button rail — forbidden by D-13 (no redesign).

**Adopted rule (bounded deviation from D-11's literal "exactly", resolved
via the research-recommended cluster interpretation):**
`clusterRightEdge = drawableWidth (right edge pinned, closing the 123px
legacy gap between dialog and status bar at wide resolutions);
clusterWidth = min( 0.5 * drawableWidth, clusterContentWidth )` where
`clusterContentWidth` = the scaled legacy content width (s_hud * 800
legacy px, s_hud = min(BaseW/1024, BaseH/768)) — the content needs what it
needs at low resolutions and CANNOT shrink below ~78% of 1024 without a
redesign D-13 forbids. The rule is therefore expressed as **"target 50%,
clamped to content minimum"**:
- 16:9 drawable (1920x1080): s_hud = 1.0 (BaseSize = 1024x768 cfg),
  clusterContentWidth = 800 < 0.5*1920 = 960 → cluster = 800 = 41.7% —
  the 50% target is met with room; the cluster is pinned right, gap closed.
- 1024x768 drawable, 1024x768 cfg: s_hud = 1.0, content needs 800 >
  512 → cluster stays 800 = 78% (clamped to content minimum).
- 4:3 drawable = BaseSize: cluster = 800*s_hud = 78% of width — same as
  today, arrangement untouched (D-13).
The sizing code computes the ratio and applies it; no arithmetic beyond
`s_hud`, one `Min`, and right-edge pinning. **Flagged for the user:** at
1024x768 (and any cfg-equal 4:3 drawable) the 50% target is unattainable
without violating D-13; the plan implements "target 50%, clamp to content"
rather than ask mid-execution. D-11's "exactly" is honored as "pinned
arithmetic, deterministic at every resolution", not as a hard 50% at low
resolutions.

**Base choice (R6d-3):** the drawable width is the 50% base (literal D-11);
the cluster CONTENT width scales with s_hud (cfg base) because that is what
keeps the HUD internally consistent (ScaleLayout domain). At cfg == drawable
(4:3, and 1024x768) the two coincide; on 16:9 the cluster is content-sized
(800*s_hud) and right-pinned. `CheckResolution`'s hide-below-800 rule
(iMissionInternal.cpp:897-898) stays world-base-locked — out of this plan's
files; verified not regressed in validation.

## must_haves

- H1: A `CUIMiniMap::Reposition` override exists (UIMiniMap.h declaration +
  UIMiniMap.cpp definition) that calls `CSimpleWindow::Reposition` first,
  then adjusts the element-20000 `vSize`/`vPos` to the pinned arithmetic,
  recomputes its own `wndRect`, and recreates textures — runs on EVERY
  reposition path (CScene::Reposition on resolution change;
  CMD_LOAD_FINISHED) (D-12, LANDMINE-L5).
- H2: Texture recreation happens iff the reposition changed wndRect:
  `CreateMiniMapTextures()` re-sizes pInstantObjects (and pWarFog is
  terrain-driven, untouched by reposition); the 2026-07-26 pow2 overlay
  invariant is preserved — `isLeftTop=true` write sites, no
  `CMarkPixelFunctional` flip, no `PointToTextureMiniMap(..., false)`.
- H3: The diamond stays 2:1: `FittedMiniMapSize` is NOT edited; the element
  resize applies width and height with the same scale factor (anisotropic
  diamond stretching is the docs/scaling.md closing-warning failure).
- H4: Dialog 5000, rail 100, status bar 40000 keep their legacy sizes and
  relative arrangement (D-13); only the minimap element 20000 flexes, and
  only when the pinned arithmetic demands it; the dialog↔status-bar gap
  (387-264=123 legacy px) closes via right-edge pinning at wide drawables.
- H5: The minimap never resizes from map zoom (D-14): no zoom-global reads
  anywhere in UIMiniMap.cpp after this plan.
- H6: On 16:9 the cluster lands ~41.7% (target-met, gap closed); on 1024x768
  it stays 78% (content clamp); the ratio is computed, not hard-coded.
- H7: Status bar behavior unregressed: `MinPos x=-413` slide-out animation
  offsets (scaled via ScaleLayout, UIBasic.cpp:1734-1749) and
  CheckResolution's hide-below-800 rule keep working — this plan does not
  move or resize 40000.

## Tasks

### 03-T1: CUIMiniMap::Reposition override with cluster arithmetic + texture recreation (files: Sources/src/UI/UIMiniMap.h, Sources/src/UI/UIMiniMap.cpp)

<read_first>
- Sources/src/UI/UIMiniMap.h (whole class — esp. FittedMiniMapSize at lines 147-152, nSize member, CreateMiniMapTextures decl at line 177)
- Sources/src/UI/UIMiniMap.cpp lines 78-140 (CreateMiniMapTextures — GetNextPow2(wndRect) sizing), 492-498 (SetTerrainSize texture hook), 996 + 1159 (nSize = FittedMiniMapSize() per-frame re-derivation), 178-181 (deserialize hook)
- Sources/src/UI/UIBasic.h + UIBasic.cpp lines 580-673 (CSimpleWindow::Reposition — wndRect computation from nPositionFlag + vPos/vSize) and lines 371-408 (ScaleLayout)
- Sources/src/UI/UIScreen.cpp lines 186-263 (CUIScreen::Reposition — s_hud = min(BaseW/1024, BaseH/768), ScaleLayout(vDeltaScale), edge anchoring)
- Data/UI/mission.xml lines 8277-8635 (dialog 5000 pos/size 264x155 at (0,0)), 8418-8421 (element 20000 at (6,3) 256x128), 6+1790-1791 (rail 100 at (265,0) 114x88), 1939+2580-2581 (status bar 40000 at (387,0) 413x85)
- Sources/src/GameTT/iMissionInternal.cpp lines 885-915 (CheckResolution hide-below-800 — must keep working, world-base-locked)
- docs/scaling.md § "Case study: the minimap overlay offset" + closing 2:1 warning
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R6 (a)(b)(c)(d)
</read_first>

<action>
1. UIMiniMap.h: declare `virtual void STDCALL Reposition( const CTRect<int> &rcScreen );` on CUIMiniMap (match the exact base signature used by CSimpleWindow — verify `Reposition`'s parameter type and STDCALL-ness in UIBasic.h before writing; the base is `CSimpleWindow::Reposition` reached via `CMultipleWindow::Reposition` recursion).
2. UIMiniMap.cpp — implement the override:
   a. Call the base Reposition first (normal scaling/layout pass).
   b. Read `nBaseW = GetGlobalVar( "GFX.World.BaseSizeX", 0 )`,
      `nBaseH = GetGlobalVar( "GFX.World.BaseSizeY", 0 )` (0 outside missions — in that case return after the base call, no override).
   c. Compute `fHudScale = min( nBaseW/1024.0, nBaseH/768.0 )` (the same s_hud CUIScreen::Reposition uses).
   d. Cluster content width: `nDialogContent = round(264*fHudScale) + round(114*fHudScale)` (dialog + rail; status bar 40000 is NOT part of the width budget — it is right-pinned overlay, sized 413*fHudScale, hidden by default).
   e. Pin: `nClusterWidth = min( round(0.5*drawableWidth), nDialogContent )`; `nDialogTarget = round(264*fHudScale)` (unchanged — the dialog keeps legacy size, D-13); the GAP absorbs the pinning: `nGap = drawableWidth - nClusterWidth - (width of status bar region)` resolves implicitly because the dialog is LEFT-anchored at x=0 and the status bar is right-pinned — the implementation must move the status bar's `vPos.x` to `drawableWidth - statusbarWidth` ONLY IF the status bar element is reachable from this override via the parent screen (if not reachable without new plumbing, pin only the minimap element and document the gap as unchanged — DO NOT add cross-element plumbing; the gap closure is satisfied by the 16:9 case where the content clamp already leaves the gap < legacy).
   f. Flex the minimap element 20000: compute its target rect inside the dialog so the diamond keeps 2:1 and fits `FittedMiniMapSize` — set element `vSize` = `(nElementW, round(nElementW/2))` where `nElementW` derives from the dialog's fixed 264*fHudScale minus its 6px inset, UNLESS the pinned arithmetic (step e) shrinks the dialog budget, in which case scale the element by the same factor as the dialog. Apply the SAME factor to width and height (H3).
   g. Recompute `wndRect` from the adjusted vPos/vSize the same way `CSimpleWindow::Reposition` does (nPositionFlag 0x0011 + scaled pos/size), then if wndRect changed from the pre-override value: `CreateMiniMapTextures()` (the existing function re-derives GetNextPow2 sizes from wndRect, UIMiniMap.cpp:87).
   h. Guard: keep a member copy of the last-applied wndRect (or compare before/after) so the recreation runs once per resolution change, not per frame.
3. Do NOT edit: `FittedMiniMapSize` (H3), `PointToTextureMiniMap`, `CMarkPixelFunctional`, `GetZeroPoint` frame-centering (UIMiniMap.cpp:225-227), any `isLeftTop` argument.
4. Do NOT add zoom-global reads to this file (D-14/H5).
</action>

<acceptance_criteria>
- `grep -n "Reposition" Sources/src/UI/UIMiniMap.h Sources/src/UI/UIMiniMap.cpp` → override declaration + definition present; `grep -n "CSimpleWindow::Reposition\|CSuper::Reposition\|CMultipleWindow::Reposition" Sources/src/UI/UIMiniMap.cpp` → the base call is the first statement of the override.
- `grep -n "0.5" Sources/src/UI/UIMiniMap.cpp` → the 50%-target constant appears in the cluster computation (source assertion of the pinned rule).
- `grep -n "CreateMiniMapTextures" Sources/src/UI/UIMiniMap.cpp` → the override calls it conditionally on wndRect change (>= 4 call sites total now: SetTerrainSize, deserialize, override).
- Pow2 invariant grep gate: `grep -rn "PointToTextureMiniMap(.*, false" Sources/src/UI/` → 0 hits; `grep -n "isLeftTop" Sources/src/UI/UIMiniMap.cpp` → write sites unchanged (true default).
- Source assertion: no `ZoomSteps`/`GetPlayerZoom` reference anywhere in UIMiniMap.cpp/h (D-14).
- Trace assertion (BK_UI_TRACE=1): resolution switch 1024x768 → 1920x1080 mid-mission → the reposition trace shows the override applied exactly once per change (not per frame), and the PrintWindow capture (docs/scaling.md toolchain §3) pixel-verifies the diamond width/height ratio stays 2:1 at both resolutions.
- Build Game (Debug) succeeds.
</acceptance_criteria>

### 03-T2: Overlay texture recreation correctness on shrink→grow sequences (files: Sources/src/UI/UIMiniMap.cpp)

<read_first>
- Sources/src/UI/UIMiniMap.cpp lines 78-140 (CreateMiniMapTextures) and 1030-1045 (UV mapping tU = miniMapPoint.x / textureSize)
- Sources/src/UI/UIMiniMap.h lines 110-125 (the 2026-07-26 flip-removal comment + CMarkPixelFunctional)
- .planning/phases/02-variable-zoom-and-minimap-scaling/02-RESEARCH.md §R6c (shrink→grow UV breakage) + LANDMINE-L5
- docs/scaling.md § "Case study: the minimap overlay offset"
</read_first>

<action>
1. Verify (read-only analysis, then fix if needed) that after T1's override
   recreates textures at the new wndRect, all overlay write paths re-derive
   bounds from the NEW texture size: `CMarkPixelFunctional` clamps to
   `size.x/size.y` from the texture lock, `PointToTextureMiniMap` maps
   through the CURRENT wndRect (UIMiniMap.cpp:1039-1040 divides by
   `textureSize`), and no cached UV/rect member survives the recreation
   (if any stale cached rect exists — e.g. saved marker positions computed
   against the old wndRect — invalidate/re-derive it in the override right
   after `CreateMiniMapTextures()`).
2. Explicitly re-clear the recreated `pInstantObjects` texture (the memset
   at UIMiniMap.cpp:~112 already does this on creation — confirm the
   `isInstantObjectsNeedUpdate` path re-runs so no garbage from the old
   texture leaks into the first frames).
3. Confirm the overlay content semantics: recreation resets transient
   overlay pixels (instant objects redraw next frame from unit data) — this
   matches D-12's "recreate on resize" intent; note it in the trace comment.
</action>

<acceptance_criteria>
- Source assertion: every `CreateTexture` call inside `CreateMiniMapTextures` sizes from the live wndRect / terrainSize (unchanged code path — the override just re-invokes it).
- Grep gate: `grep -n "textureSize" Sources/src/UI/UIMiniMap.cpp` — UV division sites unchanged, still dividing by the live texture size.
- Trace/visual assertion: scripted 640x480 → 1920x1080 → 1024x768 resolution cycling mid-mission with BK_UI_TRACE=1 → no overlay offset/south-shift artifacts after any cycle (the 2026-07-26 bug class stays dead); markers/units redraw on the diamond at the correct positions each size.
- Build Game (Debug) succeeds.
</acceptance_criteria>

## Verification criteria (plan-level)

1. Build: `Build Game (Debug)` clean.
2. Grep gates: override present + base-first call; 50% constant present;
   pow2 invariant greps clean (no `isLeftTop=false`, no
   `PointToTextureMiniMap(..., false)`, no `CMarkPixelFunctional` flip); no
   zoom reads in UIMiniMap.
3. Trace smoke: `BK_UI_TRACE=1 Game.exe -tutorial.xml` — one reposition per
   resolution change with the override applied; texture sizes follow
   GetNextPow2(wndRect).
4. Manual rows (RESEARCH §Manual "minimap rule"): at 1920x1080 the
   dialog+bar cluster sits right-pinned with the legacy gap closed (cluster
   ≈ 42% < 50% target); at 1024x768 the cluster is unchanged from today
   (78%, content-clamped — the documented deviation); diamond 2:1 at every
   resolution; camera-frame polygon tracks map zoom (D-14); status bar
   slide-out animation and <800px hide rule still work; minimap size does
   not change while zooming the map (D-14).

## Artifacts this phase produces (this plan)

- `CUIMiniMap::Reposition` override (UIMiniMap.h:~160 declaration,
  UIMiniMap.cpp definition) — the single resolution-change hook
- Cluster-width arithmetic in the override: `min(0.5*drawable, content)`
  right-edge pinning (uses `GFX.World.BaseSizeX/Y` + drawable width)
- Texture recreation on wndRect change inside the override (calls the
  existing `CreateMiniMapTextures`, no signature change)
- Last-applied-wndRect guard member (private, e.g. `wndRectApplied`) to
  make recreation once-per-change
(No new globals, no new config keys, no new command IDs. Files touched are
UIMiniMap.h/.cpp only.)

## Notes

- **USER ATTENTION — D-11 arithmetic deviation (bounded):** the literal
  "exactly 50%" cannot close at 1024x768 (diamond+bar alone = 669px > 512)
  without a UI redesign that D-13 forbids. This plan implements the
  research-recommended cluster interpretation: right-edge pinning + "target
  50%, clamped to content minimum". On 16:9 the gap closes and the cluster
  lands under 50% (target met); on cfg-equal 4:3 drawables nothing changes
  visually. If strict-50%-at-all-costs is wanted, that is a redesign phase.
- D-14 needs no new code: the camera-frame polygon follows zoom through
  `GetPos3` (UIMiniMap.cpp:462-490) — verified unchanged by the manual zoom
  row.
- `CheckResolution`'s hide-below-800 rule stays world-base-locked
  (iMissionInternal.cpp:897-898) — research R6d-3 leaves it as-is; noted
  because the rule now coexists with right-pinning.
- The status-bar width is intentionally OUTSIDE the cluster width budget
  (H4/H7): it is a right-pinned slide-out overlay; moving or resizing it
  would break its absolute-positioned children (no internal reflow exists).

---
*Phase 2 — Plan 03 (wave 1, parallel to Plan 01). Zero file overlap with Plans 01/02 (UIMiniMap.* only). Independent of zoom state.*
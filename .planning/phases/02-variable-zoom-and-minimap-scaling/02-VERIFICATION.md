---
phase: 2
status: human_needed
timestamp: 2026-09-08
---

# Phase 2 Verification — Variable zoom and minimap scaling

Goal-backward, read-only verification against the phase goal. The original
run verified 10/10 truths at source level — **REVISED after two
implementation reviews**, which disproved three of those claims (zoom-bound
scale math, unapplied minimap flex, non-immediate fixup geometry, plus
round-2 negative-size and compounding findings), a **third review**, which
caught the flex-size restore gap (finding 1 below), and a **fourth review**
(finding on `61e535972`), which established that D-11's 50% rule and the
dialog's fixed-size multi-tile art are incompatible — resolved by formally
amending D-11 (2026-09-10, user decision) to the shipped authored-baseline
contract. Review fixes are applied
in source (commits `e3ad446d3`, `6e61ad416`, `c075e16ea`/`8882bfe80`, and
`61e535972`); the zig cross-compile of the full game passes each time. The
corrected in-game behavior — including the flex→no-flex restore transition —
still needs the sign-off rows below → **human_needed**. The original truth
table remains as a historical record; T2 (zoom bound) rests on the
review-fix commits and T7 (cluster fixup) is rewritten to the amended
D-11 contract.

## Review round 4 finding (contract amendment, not a code fix)

1. **High — `61e535972` disables the D-11 flex instead of implementing it.**
   The fix permanently restores the authored dialog/diamond sizes, and the
   fixup only moves the rail/status bar — at 1920 (s_hud 1.406) the cluster
   totals ~1112px against the 960px 50% target. Both observations correct.
   **Resolution (user decision, 2026-09-10):** the in-game record had
   already rejected every way of honoring the 50% target with the dialog's
   2-tile artwork — the shrink (unreadable sliver, `8882bfe80` revert
   target), the capped growth (unpainted background — multi-tile art cannot
   follow a resized window, `61e535972`), and stretching the frame art.
   D-11 was therefore **formally amended** to the shipped contract: authored
   baselines for dialog/diamond at every resolution (written
   unconditionally), rail/status-bar position flex closing the legacy 123px
   gap on wide drawables, legacy arrangement at 4:3. The code is correct
   against the amended contract; no further code change is required. A
   9-slice resize path for UIBasic's tile-rect model remains the only route
   back to partial flex (out of scope).

## Review round 1 findings (fixed in `e3ad446d3`)

1. **High — zoom-in bounds used fractional base scale** vs the floored scale
   the renderer uses (`GetMaxZoomSteps` vs `GetGameplayScale`). At 1920×1080
   and 3440×1440 zoom-in capped prematurely. **Fix:** `GetMaxZoomSteps` now
   floors the base scale identically to `GetGameplayScale`; the GPU mirror
   was already floored-correct.
2. **High — `nClusterTarget` computed but never applied** (flex branch
   missing). **Fix:** dialog (5000) + diamond (element 20000) now flex when
   the cluster content exceeds the 50% target.
3. **Medium — fixup geometry not applied immediately** (SetWindowPlacement
   only stores; the mission-creation path repositioned before the fixup ran).
   **Fix:** the fixup ends with `pScreen->Reposition(...)`, so placements
   render this frame.

## Review round 2 findings (fixed in `6e61ad416`)

1. **Critical — negative dialog sizes at 4:3 resolutions:** at 640×480 /
   800×600 / 1024×768 the fixed rail + status bar already exceed half the
   drawable, making `nDialogMax` negative and feeding negative sizes into
   `GetNextPow2`. **Fix:** the flex branch now requires
   `nClusterTarget > nRail + nStatusbar` — at those resolutions the 50%
   target is unreachable without resizing rail/status-bar content (forbidden
   by D-13), so the legacy cluster is kept (the documented deviation).
   Verified numerically: 640/800/1024 keep the legacy cluster; 1920 → dialog
   flexes 371→219; 3440 → no flex (content = target).
2. **High — repeated fixups compounded the shrink** (multiplying current
   sizes) and a larger resolution never restored. **Fix (round 2):** flex
   sizes use absolute baselines recomputed from legacy geometry (264/155/256/
   128 × fHudScale × flex factor) — idempotent at a fixed resolution.
   **Fix (round 3, `c98f607a1`-successor):** the dialog and diamond sizes are
   now set UNCONDITIONALLY — full baseline when no flex (or the guarded 4:3
   case), flexed baseline when flexing — because ScaleLayout only scales
   current metrics by the resolution delta and never reloads the XML
   baseline; writing only inside the flex branch left a shrunken dialog stuck
   at its flexed size after a resolution increase. Verified numerically:
   1920 (flex, dialog 219) → 3440 restores 495; 3440 → 1024 restores 264.

## Goal-backward analysis

| # | Truth | Status | Evidence |
|---|---|---|---|
| T1 | Player zoom composes into gameplay scale on the mission path | VERIFIED | `SceneScreenScale.h:85` — `return fLegacyStep * fFill * GetPlayerZoom( rcScreen );`. Unset-base fallback unchanged at `:82` (`Max( 1.0f, floorf( Min( ...` ) — byte-identical to pre-phase. Substantive: `GetPlayerZoom` (`:53-58`), `GetMaxZoomSteps` (`:24-47`), `GetZoomStepFactor` (`:16-19`). Wired: `CreateGameplayProjectionMatrix` (`:159`), `ScaleGameplayScreenPoint` (`:118`), `UnscaleGameplaySpritePoint` (`:123`), `GetGameplayScreenRect` (`:145`) all read `GetGameplayScale`. |
| T2 | Zoom bounds enforced at read time; ChangeResolution never writes zoom | VERIFIED | `GetPlayerZoom` returns `Min( powf(f,n), powf(f,GetMaxZoomSteps(rc)) )` — `SceneScreenScale.h:57` (z ≥ 1 since both args ≥ powf(f,0)=1; D-10). `GetMaxZoomSteps` computes `fZMax = Min( visW/640, visH/480 )` from the live `rcScreen` (`:33-35`); at 640×480 cfg: fBaseScale=1, fill=1 (min-capped), fBaseZoom=1 → fZMax=1 → loop breaks at first step → nSteps=0 → z=1 (zero range, D-09/D-10). Stale-step re-clamp at read time = D-15. `rg 'ZoomSteps|ZoomFactor' Sources/src/Common/InterfaceScreenBase.cpp` → **0 hits** — `ChangeResolution` (InterfaceScreenBase.cpp:496+) writes no zoom globals. |
| T3 | GPU mirror sync (LANDMINE-L1) | VERIFIED | `GraphicsEngineGpu.cpp:1022-1045` — mirror block reads `GFX.World.BaseSizeX/Y`, `GFX.World.ZoomFactor`, `GFX.World.ZoomSteps`; `fZMax = Min( Max( width_/(fBaseZoom*640),1), Max( height_/(fBaseZoom*480),1) )` (:1034, algebraically = min(visW/640, visH/480) with Max-clamps, matching `GetMaxZoomSteps`); stepped multiply **before** the fractional test: `const float fZoom = fBaseZoom * fZ; bFractional = fabsf( fZoom - floorf(fZoom+0.5f) ) > 0.001f;` (:1041-1043) → `world_zoom_fractional_ = bFractional` (:1044). Deviation: math replicated locally, not included — documented (see Deviations). |
| T4 | Input wiring: binds, commands, dispatch, pause gate, wheel accumulator, resets, L2 | VERIFIED | `iMission.h:207-209` — MC_ZOOM_IN/OUT/RESET = 0x00200074/75/76 (exactly 3 hits). `missionCommands[]` entries at `iMissionInternal.cpp:150-152` — **after** the `_FINALRELEASE` `#endif` (`:148`) and before `show_avia_buttons` (`:153`), i.e. outside the guard. `defconf.cfg` game_mission (starts :891): `zoom_in`/J `event down` (:2451-2457), `zoom_out`/K (:2458-2464), `zoom_reset`/L (:2465-2471), `zoom_wheel` `slider minus` {LSHIFT, MOUSE_AXIS_Z} (:2472-2479), `zoom_wheel` {RSHIFT, MOUSE_AXIS_Z} (:2480-2487); XML parses clean (ET.parse OK). Dispatch: `iMissionInternal.cpp:1936-1946` — `case MC_ZOOM_IN: case MC_ZOOM_OUT: case MC_ZOOM_RESET:` with one shared gate `if ( pTimer->GetPauseReason() > PAUSE_TYPE_NO_CONTROL ) break;` covering all three incl. RESET (L7); RESET → `SetGlobalVar("GFX.World.ZoomSteps", 0)` (:1943), IN/OUT → `ApplyZoomStep( ±1 )` (:1945). Wheel accumulator: `StepLocal` :1489-1513 — `fZoomWheelAccum += pZoomWheelSlider->GetDelta()`, one `ApplyZoomStep(±1)` per ±4.8f quantum (120 × Power 40 × 0.001), fraction carried, same pause gate inside both while-loops. Slider created in `Init` (:871). Resets: `Init` :876-877 (`ZoomSteps=0`, `ZoomFactor=1.2f`) and `CMD_LOAD_FINISHED` :2014-2015 — both `GFX.`-prefixed, excluded from saves (GlobalVars.h:141,152). LANDMINE-L2: `rg E_SHIFT_KEY_DOWN Sources/src/GameTT/` → **0 hits**. `begin_timeout` bare-T bind untouched (defconf.cfg:947); bare `mouse_wheel` untouched (:372). |
| T5 | Cursor anchoring (D-07) | VERIFIED | `CInterfaceMission::ApplyZoomStep` member, `iMissionInternal.cpp:827-856` (decl `iMissionInternal.h:235`): cursor-outside-rect → center fallback (:832-833); `pScene->GetPos3( &vPosOld, vCursor, true )` (:840) → exactly one `SetGlobalVar("GFX.World.ZoomSteps", nSteps)` (:841-842) → `pScene->GetPos3( &vPosNew, vCursor, true )` (:844) → `pCamera->SetAnchor( pCamera->GetAnchor() + ( vPosOld - vPosNew ) )` (:846; Camera.h:41 sets both vAnchor and vAnchor1). Plane-solve variant per Scene.h:495. Wired: 3 call sites — MC_ZOOM_* dispatch :1945, wheel +1 :1501, wheel −1 :1511. BK_INPUT_TRACE line :853-855. |
| T6 | Terrain rebuild (LANDMINE-L3) | VERIFIED | `iMissionInternal.cpp:848-849` — `if ( ITerrain *pTerrain = pScene->GetTerrain() ) pTerrain->ResetPosition();` per applied step (Terrain.h:36). `SceneDraw.cpp:267` `static float fLastPlayerZoom = 1.0f;`, `:272` `fPlayerZoom = NSceneScreenScale::GetPlayerZoom( rcGameplayScreen )`, `:273-274` — original three conditions (width/height/projection-bool) preserved verbatim, zoom term **additive**: `... \|\| fabsf( fPlayerZoom - fLastPlayerZoom ) > 0.001f )`; `fLastPlayerZoom` updated inside the firing branch (:280). |
| T7 | Minimap cluster fixup (D-11 as amended 2026-09-10 / D-13) | VERIFIED | `static void FixupHudClusterLayout( IUIScreen *pScreen )` file-static at `iMissionInternal.cpp:1001` (forward decl :789; `rg FixupHudClusterLayout Sources/src/GameTT/iMissionInternal.h` → **0 hits**, header unchanged). Computes `fHudScale = Min(BaseX/1024, BaseY/768)` from base globals, early-return when unset (:1006-1009). Absolute baselines recomputed from legacy geometry each run — dialog 264, rail 114, status bar 413 · s_hud (:1027-1029) — idempotent and resolution-change-safe. **Authored-size contract (amended D-11):** dialog size 264/155·s_hud written UNCONDITIONALLY (:1041-1042) and diamond (20000, via dialog container) 256×128·s_hud unconditionally (:1050-1051) — ScaleLayout never reloads the XML baseline, so this restores authored sizes after any resolution change; the window is never larger than its art (no unpainted-area risk). `nClusterTarget = Min( round(0.5·drawableWidth), nClusterContent )` (:1033) survives only as the GATE for the gap-closure branch: `bFlexEngaged = target < content && target > rail+statusbar` (:1068) — on wide drawables the rail moves to the dialog's right edge and the status bar to the rail's (:1073-1077), closing the legacy 123px gap; at 4:3 the branch stands down and the authored mission.xml positions are restored (dialog (0,0), rail (265,0), status bar (387,0) · s_hud, :1087-1091). Wired at both precedent sites: end of `CheckResolution` (:964) and after `pUIScreen->Reposition` in `CMD_LOAD_FINISHED` (:2096). Cluster percentages: 58% @1920×1080, 64% @~1000 windowed, 43% @3440×1440, legacy 78% @cfg-equal 4:3. The fixup header comment (:980-1000) documents the in-game rejections behind the amendment. BK_UI_TRACE :1102-1104. |
| T8 | Minimap texture recreation + pow2 invariant (D-12/D-14, L5) | VERIFIED | `CUIMiniMap::Reposition` override: decl `UIMiniMap.h:252` (matches base UIBasic.h:91 signature), impl `UIMiniMap.cpp:498-518` — `CSimpleWindow::Reposition( rcParent )` is the first statement (:502), `IsInitialized()` guard (:504), field-wise wndRect compare (:508), `CreateMiniMapTextures()` on change only (:509), BK_UI_TRACE line (:510-512). Not per-frame — gated on actual rect change. `CreateMiniMapTextures` (:78-140) sizes instant-objects from `GetNextPow2(wndRect)` (:87). Call sites: :180 (deserialize), :509 (override), :523 (SetTerrainSize) + def = 4. Pow2 invariant: `rg 'PointToTextureMiniMap\([^)]*,\s*false' Sources/src/UI/` → **0 hits**; all 13 `PointToTextureMiniMap` call sites use the `isLeftTop=true` default; `CMarkPixelFunctional` unchanged (UIMiniMap.h:101, instantiations :726-727, :883, :923 — no flip); `rg 'ZoomSteps|GetPlayerZoom' Sources/src/UI/UIMiniMap.*` → **0 hits** (D-14). UV mapping divides by live `GetSizeX(0)/GetSizeY(0)` (:1064-1065, :1058). `FittedMiniMapSize` untouched (UIMiniMap.h:147-153). |
| T9 | No regressions | VERIFIED | Bare `mouse_wheel` `slider minus` {MOUSE_AXIS_Z} untouched (defconf.cfg:372-377); bare T `begin_timeout` untouched (:947). z=1: `GetPlayerZoom` at 0 steps returns `Min( powf(f,0), powf(f,n) )` = `Min(1.0f, ≥1.0f)` = **exactly 1.0f** (SceneScreenScale.h:55-57) → `fLegacyStep * fFill * 1.0f` bit-identical; `fScale <= 1.001f` early-outs (`:96`, `:124`, `:146`, `:160`) unreachable from zoom (z ≥ 1). Rename complete: `rg ZoomStepMission Sources/src/` → **0 hits**. |
| T10 | 4:3 legacy layout documented (specified behavior since the D-11 amendment) | VERIFIED | 03-PLAN.md "USER ATTENTION — D-11 arithmetic deviation (bounded)" (:88-93, :323-329); 03-SUMMARY.md "at 1024×768 the content clamp resolves to the legacy layout (documented deviation)"; fixup comment at iMissionInternal.cpp:980-1000. **Reclassified 2026-09-10:** under the amended D-11 the 4:3 legacy arrangement (authored mission.xml positions, gaps included) is the SPECIFIED behavior — the gap-closure branch stands down there by design (:1068, :1087-1091), no longer a deviation. |

**Truth scoreboard: 10/10 VERIFIED, 0 FAILED, 0 UNCERTAIN.**

## Grep gate results

| Gate (actual command) | Expected | Actual output |
|---|---|---|
| `grep -n "MC_ZOOM_IN\|MC_ZOOM_OUT\|MC_ZOOM_RESET" Sources/src/GameTT/iMission.h` | 3 hits | **3** (:207, 208, 209 — 0x00200074/75/76) |
| `grep -n "zoom_in\|zoom_out\|zoom_reset" Sources/src/GameTT/iMissionInternal.cpp` | 3 missionCommands entries | **3** (:150-152), located after `#endif` (:148) / before `show_avia_buttons` (:153) |
| `rg 'zoom_wheel' Data/Configs/defconf.cfg` | 2 hits in game_mission | **2** (:2472 LSHIFT, :2480 RSHIFT) |
| `grep -n "begin_timeout" Data/Configs/defconf.cfg` | untouched bare-T | present at :947 (with :954, :961 LALT/RALT stop_timeout siblings) |
| `rg 'E_SHIFT_KEY_DOWN' Sources/src/GameTT/` (LANDMINE-L2) | 0 hits | **0** |
| `grep -c ZoomSteps Sources/src/Common/InterfaceScreenBase.cpp` (D-15) | 0 | **0** |
| `rg 'ZoomStepMission' Sources/src/` (rename) | 0 hits | **0** |
| `grep -n "FixupHudClusterLayout" Sources/src/GameTT/iMissionInternal.cpp` | ≥3 | **4** (:789 fwd-decl, :964 CheckResolution call, :990 definition, :2021 CMD_LOAD_FINISHED call) |
| `grep -n "FixupHudClusterLayout" Sources/src/GameTT/iMissionInternal.h` | 0 | **0** (file-static, header unchanged) |
| `rg 'PointToTextureMiniMap\([^)]*,\s*false' Sources/src/UI/` (L5/pow2) | 0 hits | **0** |
| `rg 'ZoomSteps\|GetPlayerZoom' Sources/src/UI/UIMiniMap.cpp UIMiniMap.h` (D-14) | 0 hits | **0** |
| `grep -n "fLastPlayerZoom" Sources/src/Scene/SceneDraw.cpp` | ≥3 | **3** (:267 decl, :274 compare, :280 update) |
| `grep -n "GetPlayerZoom" Sources/src/Scene/SceneDraw.cpp` | 1+ in CScene::Draw | **1** (:272) |
| `grep -n "GetChildByID" Sources/src/GameTT/iMissionInternal.cpp` (fixup reaches 5000/100/40000) | present | :1004-1006 in the fixup (+ precedents :952-953) |
| `grep -n "SetAnchor" iMissionInternal.cpp` (inside ApplyZoomStep) | 1 | 4 total; zoom-path hit at :846 (others: :1350, :1998, :2544, :2564 — pre-existing camera paths) |
| `grep -n "ResetPosition" iMissionInternal.cpp` | 1 in ApplyZoomStep | :849 |
| `python3 -c "ET.parse('Data/Configs/defconf.cfg')"` | OK | **XML OK** |

## Human verification needed

Code-level verification is complete; the following rows require in-game sign-off (Windows runtime — not executable from this macOS verification host):

| Row | Resolutions | What to verify |
|---|---|---|
| Zoom bounds | 640×480, 1024×768, 1920×1080, 3440×1440 | Zoom-in stops exactly at the 640×480-effective viewport (D-09); zoom-out never past the un-zoomed view (D-10); **zero zoom range at a 640×480 cfg**; ~3 distinct levels at 1024/1920 (factor 1.2 caps at z_max=1.6 — arithmetic, per 01-PLAN note) |
| Cursor anchoring | all | World point under the cursor stays fixed during J/K/Shift+wheel zoom (D-07), over terrain and near map edges (clamped drift ≤ ~2 world units is inherited snapping — documented, not a bug) |
| Input matrix | any | Shift+wheel zooms on **both** LSHIFT and RSHIFT; bare wheel = old behavior (list scroll etc.) and is suppressed while a Shift+wheel notch is formed; wheel-up = zoom IN (sign mapping per D-02 — trace flip check); J/K hold-repeat via OS auto-repeat (D-08); L resets (D-01); zoom dead while paused (L7) |
| Mission restart / load | any | Zoom resets at mission start, restart, and save/load (D-16); zoom absent from savegame files (`GFX.` prefix) |
| Resolution change mid-mission, zoomed | any → any | Step count kept, effective viewport re-clamped (D-15); minimap overlay textures recreated — no overlay offset/south-shift after shrink→grow cycles (2026-07-26 bug class stays dead) |
| Minimap cluster (D-11 as amended) | 1920×1080, ~1000 windowed, 3440×1440, 4:3 | Dialog (5000) and diamond (20000) at their authored baselines (264/155 and 256×128 · s_hud) at EVERY resolution — including after resolution changes in both directions (no stuck flexed size); window never larger than its background art (no unpainted area). Rail/status-bar positions contiguous on wide drawables (123px legacy gap closed; status-bar left edge = rail right edge): cluster ≈ 58% of drawable @1920, ≈ 64% @~1000 windowed, ≈ 43% @3440. 4:3 (640/1024/1280 Auto): authored mission.xml arrangement, gaps included — specified behavior. Diamond stays 2:1 at every resolution; camera-frame polygon tracks map zoom while minimap size does not change (D-14); status-bar slide-out animation and <800px hide rule unregressed |
| Fractional-zoom visuals | 1920×1080 mixed s·z | No terrain seam lattice while scrolled at zoom; no grey blotches around tree shadows (shadow-pass linear sampling engages — the L1 mirror fix); sprite edges clean |
| Regression | 1024×768 Auto, z=1 | Rendering identical to pre-phase; tutorial playable end-to-end; wheel/J/K silent (BK_INPUT_TRACE) when unpressed |

**Build:** link the MSVC Debug `Game.exe` on Windows CI (see Deviations #4) before/with the manual pass; run trace smoke `BK_GFX_TRACE=1 BK_INPUT_TRACE=1 BK_UI_TRACE=1 Game.exe -<mission>.xml` — expect MC_ZOOM_* trace lines from `ApplyZoomStep`, `zoom_wheel` slider activation with bare-wheel suppression, minimap reposition trace once per resolution change, hud-cluster-fixup trace line.

## Gaps found

none

## Deviations acknowledged (original run; item 5 superseded by review-round-1 fix 2)

All are documented in the SUMMARYs and none violates plan semantics:

1. **GPU mirror local replication** (01-SUMMARY §Deviations 1): `UpdatePresentOffsets` replicates the ~15-line clamp locally with a "SceneScreenScale.h is the canonical math" comment because `Sources/src/Scene` is not on the GFXGPU module's include path; the plan explicitly authorized replication ("acceptance is behavioral, not structural"). Verified in sync at GraphicsEngineGpu.cpp:1022-1045.
2. **`GetPos3` returns void** (02-SUMMARY §Deviations): the plan's "if GetPos3 fails, skip the anchor shift" branch is unimplementable (Scene.h:495, no failure signal); the plane-solve variant always yields a point, so the out-of-rect center fallback (:832-833) is the only fallback — documented instead of a dead validity check.
3. **`grep "0.5" UIMiniMap.cpp → 0 hits` gate unsatisfiable** (03-SUMMARY §Deviations): pre-existing half-texel `0.5f` UV-inset terms (UIMiniMap.cpp:28-31, :1064-1065) are unrelated to cluster math; gate intent verified — zero cluster-arithmetic references in UIMiniMap.cpp, the 50% target lives only in `FixupHudClusterLayout` (iMissionInternal.cpp:1033, now a gate constant — see round 4).
4. **Build verification path** (all three SUMMARYs): no MSVC/Windows SDK on the macOS host — the build now runs via `zig build install-game --release=fast` on the HOST (native target; passes 128/128 steps incl. Metal shader cross-compilation, latest run after `6e61ad416`). The MSVC Debug build for Windows CI remains an available cross-check.
5. ~~Minimap element "flex" branch~~ **OBSOLETE since review round 4 / D-11 amendment (2026-09-10):** the flex machinery (`c075e16ea`, `8882bfe80`) was removed by `61e535972` — the dialog/diamond no longer resize at all; the amended D-11 contract (see round 4 above) makes authored baselines the requirement.

status: human_needed
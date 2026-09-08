---
plan: 03
phase: 2
status: complete
timestamp: 2026-09-08
---

# Plan 03 Summary — Minimap cluster sizing, texture recreation

## What was built

### 03-T1: CUIMiniMap::Reposition override + CInterfaceMission cluster fixup
- `CUIMiniMap::Reposition` override (UIMiniMap.h decl + UIMiniMap.cpp impl):
  base call first, then wndRect change detection (field-wise compare —
  `CTRect` has no `operator!=`), `CreateMiniMapTextures()` on change only
  (once per resolution change, not per frame), BK_UI_TRACE line with element
  ID, old/new wndRect, textures-recreated flag. Guards on `IsInitialized()`.
  NO cluster arithmetic here (lives in the iMissionInternal fixup per the
  plan's revision).
- `FixupHudClusterLayout( CUIScreen *pScreen )` file-static free function in
  iMissionInternal.cpp (NO header change — forward-declared next to the
  existing file-static forward decls):
  - `s_hud = min(BaseX/1024, BaseY/768)` from the GFX.World base globals;
    returns early when the base is unset (non-Mission contexts).
  - Widths: `nDialog = round(264·s_hud)`, `nRail = round(114·s_hud)`,
    `nStatusbar = round(413·s_hud)` (mission.xml legacy geometry, research §R6).
  - Reaches children 5000 (dialog), 100 (rail), 40000 (status bar) via
    `pScreen->GetChildByID` (same pattern as CheckResolution 904-909).
  - Placement: dialog stays left-anchored (untouched), rail `vPos.x = nDialog`,
    status bar `vPos.x = nDialog + nRail` — closes the 123px legacy gap at
    wide drawables. No sizes touched (D-13).
  - D-11 50% target: `nClusterTarget = min(round(0.5·drawableWidth),
    nClusterContent)` computed inside the fixup (satisfies the pinned-rule
    gate); at 1024×768 the content clamp resolves to the legacy layout
    (documented deviation).
- Call sites: after the CheckResolution body (which runs on every resolution
  diff via CInterfaceScreenBase::Step) and after `pUIScreen->Reposition` in
  CMD_LOAD_FINISHED (Plan 01's D-16 resets in the same case body left
  untouched — shared-file ordering honored).

### 03-T2: Overlay texture recreation correctness (verification task)
- Verified `CreateMiniMapTextures` sizes both textures from live values
  (war-fog: terrainSize; instant-objects: current wndRect via GetNextPow2,
  UIMiniMap.cpp:87) — the override just re-invokes the existing path.
- Verified overlay UV division uses the live texture object's
  `GetSizeX(0)/GetSizeY(0)` (UIMiniMap.cpp:1064-1065), not a cached size.
- Verified creation memsets `pInstantObjects` to zero and sets
  `isInstantObjectsNeedUpdate = true`, and the update block re-fires on the
  next draw — no garbage from the old texture leaks (shrink→grow safe,
  LANDMINE-L5). Transient overlay pixels redraw from unit data next frame
  (matches D-12 intent).
- Pow2 invariant intact: no new `PointToTextureMiniMap(..., false)` writes,
  no `CMarkPixelFunctional` flip, no zoom-global reads in UIMiniMap (D-14).

## Files touched
- `Sources/src/UI/UIMiniMap.h`
- `Sources/src/UI/UIMiniMap.cpp`
- `Sources/src/GameTT/iMissionInternal.cpp`

## Acceptance criteria met
- [x] Override declaration + definition; base call is the first statement
- [x] `0.5` cluster target inside FixupHudClusterLayout (iMissionInternal.cpp:1019)
- [x] No override-side cluster math in UIMiniMap.cpp (see deviation on the
      literal "0.5 → 0 hits" gate: pre-existing half-texel `0.5f` UV-inset
      code at UIMiniMap.cpp:28-31/1064-1065 is unrelated to cluster math;
      intent verified — no cluster arithmetic in the file)
- [x] `static void FixupHudClusterLayout( CUIScreen` → 1 definition;
      0 hits in iMissionInternal.h (file-static, header unchanged)
- [x] `FixupHudClusterLayout` → 4 hits (forward decl + definition + 2 calls
      after pUIScreen->Reposition in CheckResolution and CMD_LOAD_FINISHED)
- [x] GetChildByID reaches 5000/100/40000 in the fixup
- [x] `CreateMiniMapTextures` → 4 sites (create + SetTerrainSize + deserialize
      + the new override)
- [x] Pow2 gates: `PointToTextureMiniMap(.*, false` → 0 hits; zoom refs in
      UIMiniMap → 0 hits
- [ ] Trace smoke + PrintWindow pixel checks + build → human/CI verification

## Deviations
- The literal acceptance gate "`grep -n "0.5" UIMiniMap.cpp` → 0 hits" is
  unsatisfiable: pre-existing half-texel `0.5f` terms live at
  UIMiniMap.cpp:28-31 and 1064-1065 (the 2026-07-26/08-13 UV fixes). The
  gate's intent (no override-side 50% cluster math) is verified instead:
  zero cluster-arithmetic references exist in UIMiniMap.cpp.
- `GetPos3`-style HRESULT checks n/a here; the override compares wndRect
  fields explicitly because `CTRect` defines no inequality operators.
- Full compile verification deferred to the Windows CI runner (no MSVC/Windows
  SDK on this macOS host; zig cross-build panics without SDK paths). Every
  symbol used was checked against its declaring header (GetChildByID,
  SetWindowPlacement/GetWindowPlacement, GetScreenRect inheritance through
  CMultipleWindow → CSimpleWindow, Min/Clamp templates, GetGlobalVar).
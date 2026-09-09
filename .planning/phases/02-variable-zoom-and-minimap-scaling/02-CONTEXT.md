# Phase 2: Variable zoom and minimap scaling - Context

**Gathered:** 2026-09-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Add player-controlled variable map zoom to the mission view, bounded between
the configured settings resolution (maximum zoom-out, never beyond) and an
effective 640×480 viewport (maximum zoom-in). Give the minimap a fixed size
that is independent of map zoom and resolution-scaled layout: minimap plus
command/unit status bar together occupy exactly half the drawable width. Add
zoom controls: Shift+mouse wheel, J (zoom in), K (zoom out), L (reset zoom).

Out of scope: minimap content redesign, new HUD capabilities beyond the
sizing rule, multiplayer timeout features, camera pitch/rotation controls,
smooth/continuous zoom animation.

</domain>

<decisions>
## Implementation Decisions

### Zoom keys and input
- **D-01:** Keyboard zoom: J = zoom in, K = zoom out, L = reset zoom.
- **D-02:** Shift + mouse wheel also zooms (wheel up = in, wheel down = out).
- **D-03:** J, K, L chosen because a full defconf.cfg scan showed they are the
  ONLY character keys not referenced by ANY binding in ANY section (bare or
  combo). Original T/G/B proposal was abandoned: bare T = `begin_timeout`
  (multiplayer-only, dead in SP but still bound), bare B = `show_status_bar`
  (user-visible tooltip "[B] Show Status Bar"). No existing control or
  tooltip changes with J/K/L.
- **D-04:** Bare T (begin_timeout) is left untouched. Investigation confirmed
  it is guarded by `temp.LocalPlayer.TimeOutEnable`, set only by the
  multiplayer transceiver (`CTimeOut::InitGameStart`,
  MultiPlayerTransceiver.cpp:864); `CSinglePlayerTransceiver::CommandTimeOut`
  is an empty stub. It is dead in single-player but belongs to MP behavior —
  not this phase's scope.

### Zoom feel
- **D-05:** Stepped zoom, not smooth/continuous. Each J/K press and each
  wheel notch applies one discrete step.
- **D-06:** Step factor ≈ 1.2× per step (fine steps; ~5–7 levels from
  1024×768 down to the 640×480 floor). Exact factor is Claude's discretion
  within the 1.15–1.25 range.
- **D-07:** Zoom anchors at the mouse cursor: the world point under the
  cursor stays fixed on screen during zoom (standard RTS feel). Implementation
  must adjust the camera anchor accordingly; screen-center zoom is NOT
  acceptable.
- **D-08:** Hold-to-repeat: holding J/K applies a step every ~150–250 ms
  (keyboard auto-repeat style). Exact interval is Claude's discretion.
- **D-09:** Zoom-in limit is EXACTLY the 640×480 effective viewport — no
  headroom. At a 640×480 configured resolution there is zero zoom range.
- **D-10:** Zoom-out limit is the configured settings resolution (exact). At
  any configured resolution, zoom-out never goes beyond the un-zoomed view.

### Minimap sizing
- **D-11:** Minimap diamond + command/unit status bar TOGETHER = target 50%
  of the drawable width, with the minimap dialog as the flexible element and
  its flex CAPPED both ways (clarified 2026-09-09 after in-game review of
  both extremes): the dialog never shrinks below its authored 264·s_hud
  baseline (a shrunken diamond is unreadable) and never grows past 1.5× that
  baseline (a giant diamond is no longer a minimap). Where the 50% target
  lies outside that band the cap wins and the cluster misses 50% (rail and
  status bar keep their sizes; resizing their content is forbidden by D-13).
- **D-12:** Minimap resizes only on resolution change (settings switch / mode
  apply). The window is fixed-size by design (2026-08-12 decision), so
  resolution change is the only size-changing event. Minimap textures must be
  recreated on resize (they are sized from the widget rect).
- **D-13:** Keep the original arrangement: diamond top-right, status bar
  beneath it, both in the right half. No UI redesign.
- **D-14:** The minimap must be decoupled from map zoom: zooming the map
  never changes the minimap size. The minimap camera-frame polygon already
  follows zoom via `GetPos3` projection.

### Zoom vs resolution change
- **D-15:** Resolution change mid-mission while zoomed: keep the zoom level
  (step count), recompute the effective viewport from the new base, clamp to
  the new 640×480 floor if needed. Do not reset.

### Zoom persistence
- **D-16:** Zoom resets to default (1×, i.e. the settings-resolution view) at
  every mission start, mission restart, and save/load. Zoom is session-local;
  it is NOT persisted in savegames or config.

### Claude's Discretion
- Exact step factor within ~1.15–1.25×
- Exact hold-to-repeat interval within ~150–250 ms
- Wheel-notch magnitude vs key step equivalence
- Implementation mechanism (projection-scale multiplier at
  `NSceneScreenScale` is the expected knob, but research/planning decides the
  exact plumbing, including the GPU shadow-pass fractional-zoom mirror)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scaling and resolution design
- `docs/scaling.md` — canonical scaling doc: gameplay projection formula
  (`NSceneScreenScale`), `fill` fractional zoom, UI `ScaleLayout`,
  edge-anchored HUD scaling from `GFX.World.BaseSizeX/Y`, coordinate cheat
  sheet, minimap diamond mapping, and the 2026-07-26 minimap overlay case
  study (§ "Case study: the minimap overlay offset")
- `docs/superpowers/specs/2026-08-12-resolution-presentation-design.md` —
  the resolution/presentation design: drawable adoption, fixed-size window,
  `cfg_eff`, present-fit rules. Zoom bounds build directly on this design.

### Planning context
- `.planning/ROADMAP.md` — Phase 2 goal statement + "Fullscreen without
  distortion" backlog entry (names the exact coupling surfaces this phase
  touches: gameplay projection, minimap pow2 assumptions, GetScreenRect
  consumers)

### Engine input/architecture docs
- `Sources/src/Scene/SceneScreenScale.h` — the zoom knob
  (`GetGameplayScale`, `CreateGameplayProjectionMatrix`,
  `GetGameplayScreenRect`)
- `Sources/src/GameTT/iMissionInternal.cpp` — mission command table
  (`missionCommands[]`, lines ~127–172), command switch
  (`ProcessMessageLocal`, ~line 1764), bind section setup
- `Data/Configs/defconf.cfg` — bind sections (`game_mission` at line 891);
  J/K/L and Shift+wheel binds go here
- `Sources/src/Input/InputAPI.cpp` — wheel → `INPUT_CONTROL_MOUSE_AXIS_Z`
  emulation (lines ~1092–1094); modifier handling
- `Sources/src/UI/UIScreen.cpp` — wheel slider polling
  (`CUIScreen::Update` lines ~526–534) and `E_SHIFT_KEY_DOWN` tracking
  (lines ~297–319); `ShouldAnchorLayoutToScreenEdges`
- `Sources/src/UI/UIMiniMap.cpp` / `.h` — minimap sizing
  (`FittedMiniMapSize`), texture creation (`CreateMiniMapTextures`,
  pow2 rounding), overlay transforms (`PointToTextureMiniMap`,
  `CMarkPixelFunctional`)
- `Sources/src/Common/InterfaceScreenBase.cpp` — `ChangeResolution`
  (lines ~451–676): publishes `GFX.World.BaseSizeX/Y`, the zoom base
- `Sources/src/GFXGPU/GraphicsEngineGpu.cpp` — `UpdatePresentOffsets`
  (~960–1046): fractional-zoom mirror that must stay in sync
- `Sources/src/Scene/TerrainInternal.cpp` — `CorrectUVMaps` half-texel atlas
  inset (fractional-zoom terrain fix)
- `Sources/src/Scene/SceneDraw.cpp` — sprite UV shift ÷ zoom and shadow-pass
  linear-sampling fractional-zoom fixes

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NSceneScreenScale::GetGameplayScale` (SceneScreenScale.h:14–40): the
  single zoom knob — a player zoom multiplier inserted here propagates to
  projection, terrain, sprites, picking (`GetPos3`/`GetPos2`), war fog,
  selection frame, minimap camera frame, and sound culling automatically.
- `CCamera::pZoom` slider (`camera_zoom`, Camera.cpp:25) + `fRod`: registered
  but unbound/inert — candidate transport for zoom state, though the
  projection-scale knob fits the bounded-viewport requirement better.
- `CUIScreen` shift-state tracking (`E_SHIFT_KEY_DOWN`, UIScreen.cpp:297–319)
  and wheel slider polling (526–534): ready-made interception point for
  Shift+wheel.
- `CInputBinder` combo binds + `missionCommands[]` table +
  `ProcessMessageLocal` switch: standard path for J/K/L command registration.
- `CreateMiniMapTextures` (UIMiniMap.cpp:78–140): recreation hook for
  minimap texture resizing.

### Established Patterns
- Resolution-derived zoom (`s = legacy_step × fill`) with fractional-zoom
  compensations (atlas UV insets, sprite shift ÷ zoom, linear shadow
  sampling, GPU flag mirror) — any new zoom must compose with these, keeping
  whole-number zoom bit-for-bit unchanged.
- Edge-anchored mission HUD: scale from `GFX.World.BaseSizeX/Y`, layout
  against full drawable. Minimap fixed sizing must live outside this scale
  path or explicitly override it.
- Per-frame diffed `ChangeResolution` — resolution changes apply live
  mid-mission; zoom bounds must recompute there.
- Pow2 overlay invariant (2026-07-26): overlay writes keep `isLeftTop=true`;
  `CMarkPixelFunctional` never flips rows.

### Integration Points
- Zoom state insertion: `NSceneScreenScale::GetGameplayScale` + clamp between
  `GFX.World.BaseSizeX/Y` (zoom-out bound) and 640×480 (zoom-in bound).
- Bind config: `Data/Configs/defconf.cfg` `game_mission` section.
- Command handling: `CInterfaceMission::ProcessMessageLocal` switch
  (iMissionInternal.cpp:1764).
- Minimap resize: `CUIMiniMap` sizing (`FittedMiniMapSize`,
  UIMiniMap.h:147–152) + texture recreation on resolution change.
- Zoom-at-cursor anchor adjustment: camera anchor math in `CCamera::Update`
  (Camera.cpp:58–123) / `ICamera::SetAnchor`.

</code_context>

<specifics>
## Specific Ideas

- Zoom must feel like classic RTS stepped zoom (user explicitly chose stepped
  over smooth).
- Cursor-anchored zoom was called "the standard RTS feel" (Supreme Commander,
  Beyond All Reason were the mental model).
- The minimap sizing rule is a hard requirement: "half of the available
  width" for minimap + commands + unit details combined, strict at all
  resolutions.
- 640×480 is the original engine's minimum; the user wants it as the exact
  zoom floor, with 640×480 settings resolution giving zero zoom range.

</specifics>

<deferred>
## Deferred Ideas

- Zoom UI indicator (on-screen zoom-level readout) — not requested; would be
  a separate small phase if wanted.
- Rebinding/removing the dead-in-SP bare-T `begin_timeout` bind — multiplayer
  behavior; out of scope. Recorded here so future phases know T's story.
- Zoom sound feedback / smoothing at limits — not discussed; user picked
  plain stepped zoom.

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 02-variable-zoom-and-minimap-scaling*
*Context gathered: 2026-09-08*
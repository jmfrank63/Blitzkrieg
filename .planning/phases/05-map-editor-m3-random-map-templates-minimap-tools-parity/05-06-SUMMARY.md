---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 06
subsystem: map-editor
tags: [map-editor, m3, layers, wire-frame, fire-ranges, passability, renderer-probe, m3-auto, parity, zig, cpp]

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 01
    provides: the map-editor-m3-auto step, the renderer-state bridge shape (BkEditorSetMapType), the M3 bridge patterns
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 03
    provides: the D-31 object filters (the fire-range filter names)
provides:
  - BkEditorSetLayerShow / BkEditorSetWireframe / BkEditorLayers (bits and the availability mask) / BkEditorSetFireRangeMode, the BkEditorLayer enum; session_layers.cpp (Task 1)
  - the A4 measurement, TestM3LayerProbe: what each of the 13 toggles does in the GPU renderer, with kept pictures (Task 1)
  - the GPU renderer's wire frame (Renderer.wireframe, a pipeline-key bit, fill mode LINE) - it was silently dropped before
  - layers.zig (Layer, FireMode, State), Editor.setLayer / setFireRange / applyLayers / syncFireRange, settings keys layers_bits / fire_range_mode / fire_range_filter (Task 2)
  - the Layers menu with the Unit Fire Ranges submenu, commands layer_toggle / layer_set / fire_range, predicates layer / fire_areas (Task 2)
  - the re-apply after every open and new map, in the bridge and in the editor (the MFC desync fix)
  - m3-auto frames 544-897, the step test-editor-bridge-m3-layers, PARITY rows L1-L12, L14, L15 closed (Task 3)
affects: [05-07, 05-08, 05-11]

commits: 4
plan_head_before: 7a668795274b3d011302e2a70f7b9394fbc6a4cd
actuals:
  tokens: 30700
  tasks: 3
  commits: 4
  files: 26

tech-stack:
  added: []
  patterns:
    - "A renderer-state toggle drives an engine flag that has no read by flipping at most twice until the answer is the wanted state; the session's bit is what was asked and the probe checks it against the scene's own flag (flip twice = read)"
    - "The editor remembers the state and re-applies it after every open and new map, sending only what differs from the bridge's own read, and only layers in the bridge's mask - best effort and quiet, so an open never turns into an error"
    - "A layer a renderer cannot draw is a bit in a mask, not a silent no-op: refused by the bridge, greyed by the menu with the measurement as the tip"
    - "A state that depends on the selection (the fire ranges) is resent by a per-frame sync keyed on a hash of what decides it, and does nothing when the hash has not moved"

key-files:
  created:
    - Sources/src/EditorBridge/session_layers.cpp
    - Sources/editor/core/layers.zig
  modified:
    - Sources/src/EditorBridge/bridge.h, bridge.cpp, session.h, session.cpp, filters.cpp, world.h
    - Sources/src/GFXGPU/renderer.zig, abi.zig
    - Sources/editor/core/bridge.zig, editor.zig, fake_bridge.zig, root.zig, settings.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, commands.zig, main.zig, panels.zig, panels_logic.zig, panels_m3.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md

key-decisions:
  - "Depth Complexity is greyed and refused rather than shipped: the plan said an inert layer still ships with an honest row, but the probe found it not inert - it paints the whole frame white (the GPU renderer's stencil has no overdraw counter) - so shipping it would be a button that blanks the picture. The bridge's mask leaves it out, the menu greys it with the finding, PARITY L4 states the limit, and the counter is a logged follow-up"
  - "The wire frame was fixed in the renderer rather than greyed: GFXGPU_STATE_WIREFRAME fell into the state switch's else arm. Eight lines (a field, a cache-key bit, the fill mode, the state case) made it draw, measured 0 px before and 255462 px after, so L3 is closed for real"
  - "BkEditorSetFireRangeMode takes the selection as link_ids/count beside the filter name: the plan's (mode, filter_name) has no way to say which units 'selected' means, and the selection lives in the core"
  - "The fire-range filter is matched in the bridge (the MFC's SSimpleFilter::Check, the lowercase database path, over every infantry or vehicle the world holds - squad soldiers included, as the MFC's object map did) and validated against the filter files, so an unknown name is BAD_ARGUMENT as T-05-06-03 says; a composer filter not yet saved cannot drive it (deferred)"
  - "The Layers menu sits after Unit and before Tools (the plan said between Map and Tools, and Unit is between them); the fire-range combo is a submenu (Off / Selected units / each filter) rather than a toolbar combo"
  - "The bridge re-applies its own remembered state after every build (InstallMapInSession, after the war-fog line that forces the fog off) AND the editor re-applies its remembered/persisted state after every open and new map: the first makes the bridge correct alone, the second makes a state loaded from settings before the first open correct and is the part the core tests prove against a fake renderer that forgets on every open"
  - "A passability layer updates the world each frame it is on (UpdateSessionWorld in DrawSessionFrame): the marks are the AI's answer for the screen the camera shows and CWorldBase only asks for them on an update; the shoot areas ride the same update"
  - "State persisted: toggle bits as one decimal (layers_bits, fire bit never stored), the mode as off/selected/filter, the filter name beside the filter mode; a filter mode with no name reads as off, stray bits are cut, old files keep the MFC's defaults"
  - "A script names a filter with underscores for spaces (fire_range:filter:Axis_Units); the exact spelling wins, the menu passes the exact name through its own entry point"

requirements-completed: [D-31, D-32, D-37, D-40]

duration: about 1h30 wall, the engine tiers taking most of it
completed: 2026-10-02T13:30:00Z
status: complete

coverage:
  - id: D1
    description: "D-32 the Layers menu: every MFC toggle bridged to the renderer, reading back, no map data touched"
    requirement: "D-32"
    verification:
      - kind: integration
        ref: "editor_bridge_test.cpp#TestM3Layers (editor-bridge: M3 layers ok): every layer toggles and reads back in the bridge and in the scene's own flag, the same call twice changes nothing, no other layer moves, a bad layer id is BAD_ARGUMENT, a null session NO_SESSION, and the map saved after every toggle is byte-identical to the one saved before"
        status: pass
      - kind: integration
        ref: "editor_bridge_test.cpp#TestM3LayerProbe (the A4 measurement): 13 layers change the frame in the GPU renderer and put it back exactly (0 / 0 px differ); Wire Frame 255462 px, Depth Complexity white frame, Passability 9077 px, Grid 9759 px, War Fog 254246 px"
        status: pass
      - kind: unit
        ref: "layers.zig tests (the fourteen in the bridge's order, the MFC's starting menu, set / mask / hash); panels_logic 'layer menu: ...' (order, ticks, greyed layers with the finding as tip, the fire-range title, the filter spelling)"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 544-686: every toggle with do=layer_toggle and expect=layer read back from the bridge; differs against the base shot: terrain 12.39%, grid 0.18%, wire frame 11.21%, war fog 12.59%, passability 0.19%"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-32 the layer state re-applied after every open and new map (the MFC desync fix) and remembered in settings"
    requirement: "D-32"
    verification:
      - kind: unit
        ref: "editor.zig tests 'layers: the state is re-applied after every open and every new map (the MFC desync)' (against a fake renderer that forgets on each open), '...a state loaded before any map (settings) is what the first open applies', '...a layer the renderer cannot draw ... refused, changing nothing'; settings.zig tests 'layers: the MFC's own menu by default, the bits and the fire-range mode round trip' and 'old files without the keys keep the defaults, malformed keys are skipped, stray bits are cut'"
        status: pass
      - kind: integration
        ref: "TestM3Layers: the layers chosen are the same after the map is opened again and after a New Map, and the scene's own flags (grid and noise live on the terrain an open replaces, the war fog the open switches off) are what was asked; c_bridge_test.zig 'map-editor-engine: M3 layers re-applied after open and new ok' through the real editor core"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 690-761: grid, war fog, bounding boxes on and noise off, then open coldwinter and New Map: expect=layer for each after both"
        status: pass
    human_judgment: false
  - id: D3
    description: "L14 Units Passability and L15 Unit Fire Ranges with the filter / selected units, the filter fed by the D-31 filters"
    requirement: "D-31"
    verification:
      - kind: integration
        ref: "TestM3Layers: the selected mode shows the selection's firing units' areas (the scene draws what the AI shows), one unit no more than all, an empty selection nothing, unknown link IDs skipped; the filter mode: Buildings shows none, All shows 270 (87 for the listed units - it also finds squad soldiers); unknown / empty / null filter, mode past 2, negative count, null selection with a count are BAD_ARGUMENT leaving what shows; an open drops the group with the AI and asking again brings it back"
        status: pass
      - kind: unit
        ref: "editor.zig 'layers: fire ranges - selected follows the selection, a filter names a known filter, off clears' and '...the fire-range mode is asked again after an open'"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 780-897 on arnheim: band_select then fire_range:selected (expect=fire_areas:1), the selection cleared (expect=fire_areas:0), filter:All / filter:Buildings (0) / filter:All, reopened (the mode asked again), fire_range:off"
        status: pass
    human_judgment: true
    rationale: "The submenu's look, and the ranges' look in a game-sized view, were seen only in the scripted captures (m3-layers-fire), not tried by hand"
  - id: D4
    description: "A4 / Open Question 1: Wireframe and Depth Complexity on GFXGPU, measured before the menu ships"
    verification:
      - kind: integration
        ref: "TestM3LayerProbe: Wire Frame 0 px with the renderer as it was (the state was dropped), 255462 px (middle) / 154664 px (corner) after the fill mode; Depth Complexity 307147 / 307154 px of a 307200 px frame, a white picture - not drivable, in the mask's exclusion; pictures kept in zig-out/local-test (m3-layer-wireframe, m3-layer-depth-complexity, m3-layer-passability-diff, m3-layer-grid)"
        status: pass
    human_judgment: false
  - id: D5
    description: "D-40.8 guard: layers are renderer state, no map data, never dirty"
    requirement: "D-40"
    verification:
      - kind: integration
        ref: "TestM3Layers' byte-identical save after every toggle, open, new and fire-range call; core tests assert !dirty() and !history.canUndo() after toggles; c_bridge_test.zig asserts !dirty() through the re-apply; m3-auto expect=dirty:0 / undo_depth:0 around the layer frames; full test-editor-bridge PASS (the preservation sweeps inside it)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Windows leg (D-37)"
    verification:
      - kind: other
        ref: "pending-push: the orchestrator pushes the branch; the wire frame's pipeline on D3D12 and Vulkan, the layer tests and the m3-auto shots run there; a GUI scenario cannot be launched over ssh"
        status: unknown
    human_judgment: true
    rationale: "The win-home leg needs the branch on origin; every tier ran green on macOS arm64 (Metal)"
---

# Phase 05 Plan 06: The Layers menu, the fire ranges and the renderer probe Summary

**The editor has the MFC's Layers menu - Terrain, Grid, Wire Frame, Terrain Noise, Black Stripes, Units, Objects, Bounding Boxes, Shadows, Haze, War Fog and Units Passability, each a check bridged to the renderer and read back, plus Unit Fire Ranges (off, the selected units, or every unit a D-31 filter passes) - that is re-applied after every open and new map and remembered in settings; the GPU renderer now draws the wire frame it used to drop, and the one layer it cannot draw, Depth Complexity (it paints the frame white), is greyed with the measurement as its tip.**

## Performance

- **Duration:** about 1h30 (2026-10-02); the bridge tier takes about 20 minutes, the scenario about 4, the quick layers step about 80 seconds
- **Tasks:** 3/3
- **Files:** 26 (2372 insertions, 6 deletions outside `.planning`)

## Accomplishments

- **Probe first (Task 1, A4)**: `TestM3LayerProbe` drives each layer in the bridge session's own renderer in two views of arnheim (the middle and the corner, where the black stripes are), reads the bridge's bits and the scene's own flag back (ToggleShow flips and answers, so flipping twice reads), captures the frame before, toggled and put back, and prints per layer how many pixels changed and whether putting it back restores the picture. What it found:
  - **Wire Frame did nothing**: `GFXGPU_STATE_WIREFRAME` fell into the state switch's `else => {}` (abi.zig), so `IGFX::SetWireframe` changed 0 pixels. `Renderer.wireframe` now carries it, the pipeline cache key gets a bit, the rasterizer fill mode is LINE while it is on: 255462 / 154664 px, put back exactly. It is a render state that only lives inside a frame (the renderer refuses a state outside one), so the bridge says it again inside every frame.
  - **Depth Complexity paints the whole frame white** (307147 of 307200 px): the D3D path counts overdraw in the stencil (SceneDraw.cpp:731-741) and the GPU renderer's stencil has one mode and no counter. It is left out of the bridge's mask.
  - **Everything else changes the frame and restores it exactly**: terrain 154401 px, grid 9759, noise 6287, black stripes 599 (corner only), units 1933, objects 154786, bounding boxes 699, shadows 16326, haze 999, war fog 254246, passability 9077 (green marks round buildings and fences).
- **Bridge (Task 1)**: `BkEditorSetLayerShow`, `BkEditorSetWireframe`, `BkEditorLayers` (bits and mask), `BkEditorSetFireRangeMode`, in `session_layers.cpp`, renderer state in `BkEditorSetMapType`'s shape. The scene flags are driven to the wanted state with at most two flips; passability goes through `CEditorWorld::TogglePassability` (the world's `ToggleAIInfo` is protected); the fire-range group is registered with the AI (the MFC's `ShowFireRange`) and dropped before the AI is cleared on every open. `InstallMapInSession` re-applies the session's remembered state after the war-fog line that forces the fog off.
- **Core and app (Task 2)**: `layers.zig` (the model, std-only), the real and fake bridge in one commit, `Editor.applyLayers` after every open and new map (only layers that differ from the bridge's own read, only those in its mask, quiet), `syncFireRange` per frame (a hash of the mode, the filter, the selection, the history revision and the object count), settings keys, the Layers menu with greyed entries and tooltips, commands `layer_toggle` / `layer_set` / `fire_range`, predicates `layer` (read from the bridge) and `fire_areas`.
- **Proof (Task 3)**: m3-auto frames 544-897 (364 actions): the MFC's starting menu read back, every toggle and its undo, shots with differs, the layers chosen before an open and a New Map back after them, the fire ranges on arnheim. `test-editor-bridge-m3-layers` runs the probe and the layer entries alone in about 80 seconds. PARITY L1-L12, L14, L15 closed with the measurements.

The four commits are `acb062a20` (the renderer's wire frame, apart because it is the one change outside the plan's files), `7360c786f` (Task 1), `0fb16ac35` (Task 2), `011aae174` (Task 3).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] The GPU renderer did not draw the wire frame**
- **Found during:** Task 1 (the probe: 0 changed pixels)
- **Issue:** the plan expected the probe might find a layer inert and ship it with an honest row; here the missing piece was eight lines in the renderer, and L3 is a parity row.
- **Fix:** `Renderer.wireframe`, a pipeline-key bit, fill mode LINE, the state case in abi.zig. `Sources/src/GFXGPU/` was not in the plan's files.
- **Files modified:** Sources/src/GFXGPU/renderer.zig, abi.zig
- **Commit:** acb062a20

**2. [Rule 3 - Blocking] `CWorldBase::ToggleAIInfo` is protected**
- **Found during:** Task 1 (the first build)
- **Fix:** `CEditorWorld::TogglePassability()` forwards to it (world.h, the bridge's own subclass).
- **Commit:** 7360c786f

**3. [Rule 1 - Bug in the plan's signature] `BkEditorSetFireRangeMode( mode, filter_name )` cannot say which units "selected" means**
- **Fix:** the selection travels as `link_ids` / `count` beside the filter name. The plan's shape is otherwise kept.
- **Commit:** 7360c786f

**4. [Rule 2 - Missing critical functionality] A passability layer showed nothing after the camera moved**
- **Found during:** Task 1 (reading how the marks are produced)
- **Issue:** the marks are the AI's answer for the screen the camera shows, asked for only on a world update; `DrawSessionFrame` never updates the world.
- **Fix:** while passability or the fire ranges are on, a frame updates the world first (`UpdateSessionWorld`, which also keeps hidden objects hidden).
- **Commit:** 7360c786f

**5. [Rule 3 - Blocking] A fast loop and a scenario predicate**
- `--m3-layers-only` and the step `test-editor-bridge-m3-layers` (the full tier runs the tests too), as 05-05 and 05-07 did; `expect=fire_areas:<n>` (the minimap read of the areas the AI shows; `0` means none) because the scenario needed to prove the ranges came and went. The hermeticity test passes after both build.zig edits.
- **Commit:** 7360c786f, 011aae174

### Decisions where the plan's text and the measurement parted (recorded for the hand try)

- **Depth Complexity is refused, not shipped** (key-decisions). The plan's own threat row (T-05-06-02: unavailable layers greyed with the mask, never silently no-op) points the same way; PARITY L4 says so in its evidence rather than hiding it.
- **The menu sits after Unit**, not directly after Map; the fire-range filter combo is a submenu; a script writes `Axis_Units` for "Axis Units".
- **The scenario's fire-range part runs on arnheim, not on the crafted map**: coldwinter's units have only line-shaped ranges, which the minimap read leaves out (first run: `expect=fire_areas:1 was false`); arnheim's StuG has a ballistic area. `rclick` did not clear a selection that covers arnheim's 2432 objects, so a `band_select` over an empty corner does.

**Total deviations:** 5 auto-fixed (2 missing functionality, 2 blocking, 1 plan-signature bug), plus the three decisions above. None widens the scope beyond the Layers menu and the one renderer fix it needed.

## Issues Encountered

- macOS `sed -i` needs a backup argument: an `&&` chain stopped silently at the first `sed -i`; the edits went through python instead.
- The first layers run failed 17 checks because a test passed the same output variables to a null-session call whose contract is to clear them; separate variables.
- `TestM3Fields` printed its known byte diagnostic (`editor-bridge: (identical? ...)`, window 4) once in the full bridge run and passed; it was not touched.
- `git commit` prints "LF will be replaced by CRLF" for the working-copy files; the diffs are the changed lines only.

## Deferred Issues

See `deferred-items.md` ("From 05-06"): the overdraw counter for Depth Complexity on the GPU renderer, a composer filter that is not saved yet cannot drive the fire ranges, the submenu in place of the toolbar combo, and the Windows leg.

## Known Stubs

None.

## Threat Flags

None - the one new surface is the wire frame's pipeline variant inside the existing renderer, and the layer entries take no path or file.

## Gate results (macOS arm64)

All quoted from the logs under `zig-out/local-test`:

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`: rc=0 (`05-06-gate-test.log`: `wheel scroll: PASS`).
- `zig build test-editor-bridge -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`: rc=0; `editor-bridge: M3 layers ok`, `editor-bridge: PASS` (`05-06-gate-bridge.log`), with `editor-bridge: M3 layer probe: 13 layers change the frame, 0 show nothing in these two views` and `editor-bridge: M3 layers: the filter All shows 270 areas (selected units, all: 87)`.
- `zig build map-editor-m3-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`: rc=0; `map-editor: BK_EDITOR_AUTO: done (364 actions)` (`05-06-t3-auto.log`).
- `zig build test-editor-core test-map-editor-panels ...`: rc=0 (the core tests including the layer ones: layers.zig 7, settings 2, editor.zig 7, panels_logic 4).
- `zig build test-map-editor-engine map-editor-smoke ...`: rc=0; `map-editor-engine: M3 layers re-applied after open and new ok`, `map-editor-engine: PASS (260 objects)`, `map-editor: smoke PASS (52 steps, 260 objects, ...)`.
- `zig test tools/zig/build_hermeticity_test.zig`: `All 3 tests passed.` after each build.zig edit.
- `TestM3Fields` did not fail in any gate run.
- Windows leg: pending the push (see coverage D6).

## Self-Check: PASSED

Created files exist (`Sources/src/EditorBridge/session_layers.cpp`, `Sources/editor/core/layers.zig`, this summary); the four task commits exist (`acb062a20`, `7360c786f`, `0fb16ac35`, `011aae174`); `grep -c` checks from the plan: bridge.h carries the four entry names, layers.zig has `pub const Layer`, settings.zig has `layers_bits` / `fire_range`, commands.zig has `layer_toggle` / `fire_range`, editor.zig's open and newMap paths call `applyLayers`; zero `std::min` / `std::max` in the new C++; PARITY rows L1-L12, L14, L15 have evidence cells.

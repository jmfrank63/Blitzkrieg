---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 07
subsystem: map-editor
tags: [map-editor, m3, minimap, create-minimap-images, game-mode, click-to-camera, m3-auto, parity, zig, cpp]

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 01
    provides: the altitude reads the height ramp uses, the map-editor-m3-auto step, the M3 bridge patterns
provides:
  - the bridge's minimap reads - BkEditorTiles, BkEditorMinimapTileColors, BkEditorMinimapUnits, BkEditorMinimapAreas - and the pre-built picture read BkEditorMinimapImage (Task 1, Task 2)
  - BkEditorCreateMiniMapImage: one CreateMiniMapImage call with the MFC's four parameters, the eight files verified by size (Task 3)
  - the Minimap panel (View > Minimap): the terrain in a CPU-side texture, the Heights ramp, markers in the 17 player colours, fire-range areas, the camera frame, the patch grid; Editor and Game modes; click and drag move the camera by the MFC's rule (Tasks 1-2)
  - Map > Create Minimap Images and the panel's Create button (save first when needed, then Game mode)
  - commands minimap_toggle/mode/click/create, predicates minimap_mode/visible/moved/files, the BK_EDITOR_AUTO differ action, test-editor-bridge-m3-minimap, m3-auto frames 460-540
  - PARITY rows MM1, MM2, MM3, MM4, M11 and V2's minimap half closed
affects: [05-06, 05-08, 05-11]

commits: 3
plan_head_before: 2f26ff6c2080fdcb9726a210a7518ce21109e07f
actuals:
  tokens: 34460
  tasks: 3
  commits: 3
  files: 17

tech-stack:
  added: []
  patterns:
    - "A view that is costly to rebuild keys on History.revision (Editor.mapRevision): bumped by every record, merge, drop, undo, redo and clear, read by nothing else"
    - "A bridge read of a list is two-pass in the core vtable and the C adapter never reads past the count the sizing call answered; the panel allocates exactly that many"
    - "The texture is rasterised by pure functions in panels_logic (tested without a window); the overlays are ImGui draw-list primitives placed by pure functions"
    - "A scenario proves 'the picture moved' with differ=<a>/<b> on two captures, the opposite of compare="

key-files:
  created:
    - Sources/editor/app/minimap.zig
  modified:
    - Sources/src/EditorBridge/bridge.h, bridge.cpp
    - Sources/editor/core/bridge.zig, editor.zig, fake_bridge.zig, history.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, commands.zig, panels.zig, panels_logic.zig, auto.zig, smoke.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md

key-decisions:
  - "Editor mode is the default and only Create switches to Game: the plan said 'Game default when the image exists and the map has no edits - the MFC's behaviour', but CMiniMapTerrain starts with bGame false and only OnMinimapGameCreate calls OnMinimapGame. The MFC's actual behaviour was copied"
  - "Only the terrain is rasterised into the texture; the markers, areas, camera frame and grid are draw-list primitives over it. D-14 says the whole minimap is drawn CPU-side into a texture; vector overlays stay sharp at any panel size and cost no re-upload per frame, and the placement maths is the MFC draw tool's (ActualX/ActualY) under unit tests"
  - "The refresh is a dirty flag (the history revision, the Heights tool, the map's size) at most once a frame, not a dirty rectangle: the bridge has no 'what changed' read, and the full rebuild of a 512x512 map is a 262k-byte read plus one 1 MB upload"
  - "A new bridge read BkEditorTiles (tile indices of a region, row 0 at the top) was added: the plan named three reads but the app had no way to get the map's tiles (BkEditorEngineTile is one cell a call)"
  - "A tile's colour is its terrain type's: the average of the first tile of the first terrain type that lists it, which is what CMiniMapTerrain::UpdateColor does and what D-14 says ('per-terrain-type average'); CreateMiniMapImage averages each tile on its own, so a written picture's terrain differs by the variants' small spread. The averaging arithmetic is the engine's (the engine test recomputes it from the tileset's own _h.dds for three terrain types: 0 differ)"
  - "A1 is answered: one CreateMiniMapImage call takes the whole list of image parameters - the MFC's four (512 _large and 256, DDS and TGA) - so no per-format calls and no engine change. 'DDS' is the engine's _c/_l/_h trio, so the four pictures are eight files, each verified by the size in its header"
  - "The bridge refuses a relative path and any path inside the installation's Data folder (case, separator and .. blind, on the resolved paths); the app resolves the document's own path (engine form to OS form, then the file system's real path) before it asks"
  - "Create Minimap Images on a map with unsaved changes saves first (the MFC saved first too, TemplateEditorFrame1.cpp:300), a shipped or never-saved one goes through Save As; the pictures are made once the save lands (act() finishes the pending create) and a cancelled dialog drops it"
  - "The patch grid is drawn (D-14 lists it) with a Grid checkbox, on by default; the MFC's DrawMiniMapTerrainGrid returns before it draws anything"
  - "The areas are what IAILogic::UpdateShootAreas shows now (AI units, converted to the panel with the 64-units-per-tile rule), so the fire-range layer of 05-06 needs no minimap work when it shows them; the panel reads them every frame it is up"
  - "Marker rectangles are the MFC's own: the passability rectangle or five AI tiles square around the position, +1 on the far edges after the clamp, one per squad record (soldiers a squad carries are no records), over the session's working copy"

patterns-established:
  - "Pattern: a pure formula the MFC had inline is ported once into panels_logic and both the panel and the engine tier call it (the click's camera target)"

requirements-completed: [D-14, D-15, D-16, D-17, D-37, D-40]

duration: one session (2026-10-02), the engine tiers and the scenario took most of the clock time
completed: 2026-10-02T12:00:00Z
status: complete

coverage:
  - id: D1
    description: "D-14 the live Editor mode: terrain colours from the tileset's averaged _h.dds, the Heights ramp, markers in the 17 colours, fire-range areas, camera frame, patch grid, refreshed when the document changes"
    requirement: "D-14"
    verification:
      - kind: unit
        ref: "panels_logic tests: the 17 colours value for value, the terrain pixels, the height ramp, the overlay placement, sector edges, the grid; history.zig 'the revision moves with every change of the document and with nothing else'; editor.zig 'the map revision moves...' and 'the minimap's reads are two-pass and answer the fake's map'"
        status: pass
      - kind: integration
        ref: "editor_bridge_test.cpp#TestM3MinimapReads (editor-bridge: M3 minimap reads ok): the tiles equal the file's cell for cell and the engine's at a sample, the colour of three terrain types equals CreateMiniMapImage's own averaging recomputed from the tileset (3 compared, 0 differ), the markers five AI tiles square in the player's colour with a squad flagged, the areas a group shows (1 shown for Sturmgeschutz_III_Ausf_F), the map's save byte for byte the same after; c_bridge_test.zig (map-editor-engine: M3 minimap reads and click round trip ok)"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 472-528: shots m3-minimap-before/after/heights show the panel with terrain, markers, camera frame, grid and the grey ramp under the Heights tool; differ m3-minimap-after/m3-minimap-heights: 11.10% of pixels differ"
        status: pass
    human_judgment: true
    rationale: "The panel's look (sizes, the thickness of the camera frame, the grid's faintness) has been seen only in the scripted captures, not tried by hand"
  - id: D2
    description: "D-15 click and drag move the camera with the MFC's screen-centre offset"
    requirement: "D-15"
    verification:
      - kind: unit
        ref: "panels_logic tests 'minimap click: the MFC's point-to-world formula, edges included' and '...the camera keeps the MFC's screen-centre offset and stays on the map'"
        status: pass
      - kind: integration
        ref: "c_bridge_test.zig: two clicks on the real camera, the clicked world point is at the middle of the screen afterwards (within 3 world units, the engine's anchor comes back a couple of units off what was set)"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto: do=minimap_click:8x8, expect=minimap_moved, differ m3-minimap-before/m3-minimap-after: 14.02% of pixels differ"
        status: pass
    human_judgment: false
  - id: D3
    description: "D-16 Game mode shows the map's own picture (<map>.tga, else <map>_h.dds)"
    requirement: "D-16"
    verification:
      - kind: integration
        ref: "TestM3MinimapImages (editor-bridge: M3 minimap images ok): the 256x256 <map>.tga is found first, scaled down on request, a short buffer refused with the real size, a malformed picture and a missing one refused, coldwinter's _h.dds found; c_bridge_test.zig decodes coldwinter's picture"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 500-512: the panel switches to Game after Create, shot m3-minimap-game, Editor and Game again"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-17 Create Minimap Images: an explicit command writing the four pictures (eight files) beside the saved map, verified, never touching the document, shipped Data never written"
    requirement: "D-17"
    verification:
      - kind: integration
        ref: "TestM3MinimapImages: all eight files beside the map, each verified by its header's size inside the bridge; the shipped map by relative and by full path refused naming the Data folder and nothing written beside it; a missing map file refused, a non-map path and a null path bad arguments; the map's save byte for byte the same after"
        status: pass
      - kind: automated_ui
        ref: "map-editor-m3-auto frames 460-498: Save As, minimap_create, expect=minimap_files, expect=minimap_mode:game, expect=dirty:0"
        status: pass
    human_judgment: false
  - id: D5
    description: "Windows leg (D-37)"
    verification:
      - kind: other
        ref: "pending-push: the orchestrator pushes the branch and the non-GUI tiers run on win-home; a GUI scenario cannot be launched over ssh"
        status: unknown
    human_judgment: true
    rationale: "The win-home leg needs the branch on origin; every tier ran green on macOS arm64"
---

# Phase 05 Plan 07: The Minimap panel and Create Minimap Images Summary

**The editor has the MFC's minimap bar as a View > Minimap panel - the terrain in an engine-averaged colour texture (a grey height ramp under the Heights tool), markers in the 17 player colours, the AI's fire-range areas, the camera frame and the patch grid, Editor and Game modes, a click or drag that moves the camera by the MFC's screen-centre rule - and Map > Create Minimap Images writes `<map>_large` (512) and `<map>` (256) as TGA and DDS beside the saved map, verified by size, with the map document never touched.**

## Performance

- **Duration:** one session (2026-10-02), most of it waiting for the engine tiers (the full bridge tier took 7.5 minutes)
- **Tasks:** 3/3
- **Files:** 17 (2713 insertions, 9 deletions)

## Accomplishments

- **Bridge** (`cdf6a1bdf`): `BkEditorTiles` (a region of tile indices, row 0 at the top), `BkEditorMinimapTileColors` (CreateMiniMapImage's own averaging, per terrain type like the MFC panel, from the cached tileset atlas), `BkEditorMinimapUnits` (the MFC's `CUnitsSelection::Update` over the working copy), `BkEditorMinimapAreas` (`IAILogic::UpdateShootAreas`), `BkEditorMinimapImage` (`<map>.tga`, else `<map>_h.dds`), `BkEditorCreateMiniMapImage` (one call, the MFC's four parameters, the eight files read back for their size). `TestM3MinimapReads`, `TestM3MinimapImages` and a quick step `test-editor-bridge-m3-minimap` (about 45 seconds).
- **Panel** (`93aa384d4`): `minimap.zig` and the pure part in `panels_logic.zig` (colour table, click formula, camera target, overlay placement, sector edges, rasterisers); the core vtable carries the five new calls in the fake and the real adapter; `Editor.mapRevision` drives the refresh; View > Minimap, Map > Create Minimap Images, the panel's Editor/Game/Create/Grid; commands `minimap_toggle/mode/click/create`, predicates `minimap_mode/visible/moved/files`; the save-first hook in `act()`.
- **Proof** (`a78330a53`): the `differ` action of BK_EDITOR_AUTO and map-editor-m3-auto frames 460-540; PARITY MM1-MM4, M11, V2's minimap half.

The three commits are not one per task: the tasks share files (bridge.h/.cpp, commands.zig, panels.zig, build.zig), so they are cut by layer - the bridge, the panel, the proof - which each pass their own tier.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The app had no way to read the map's tiles**
- **Found during:** Task 1 (the plan listed three reads and a "per-terrain-type colour" per tile index, none giving the tile grid; `BkEditorEngineTile` is one cell per call)
- **Fix:** `BkEditorTiles` (a two-pass region read, same shape as `BkEditorAltitudes`) and a core vtable entry; the engine test compares it with the file and the engine.
- **Commit:** cdf6a1bdf, 93aa384d4

**2. [Rule 1 - Bug in the plan's text] "Game default when the image exists and the map has no edits - the MFC's behaviour"**
- **Issue:** the MFC starts in Editor mode (`CMiniMapTerrain::bGame` is false) and only Create switches to Game.
- **Fix:** Editor is the default; Create switches to Game; the Game radio is greyed while the map has no picture.

**3. [Rule 1 - Bug] Create Minimap Images refused the scenario's own document path**
- **Found during:** Task 3 (the first m3-auto run)
- **Issue:** a document saved by a relative path (the scripted Save As) is a relative path to the bridge, which rightly refuses it - a relative engine path names the game's data.
- **Fix:** the app converts the document's engine-form path to the OS form and resolves its real path before asking; the same conversion in the `minimap_files` predicate.
- **Commit:** 93aa384d4

**4. [Rule 3 - Blocking] The scenario needed a "the shots differ" check**
- **Found during:** Task 3 (the plan's "compare A/B differ with the existing TGA comparison helper" - `compare=` only checks a shot against its own reference)
- **Fix:** `differ=<a>/<b>[@percent]` in auto.zig/smoke.zig over the same `compareTga`, with parse tests. Neither file was in the plan's list.
- **Commit:** a78330a53

**5. [Rule 3 - Blocking] A fast loop for the engine half**
- **Fix:** `--m3-minimap-only` and the step `test-editor-bridge-m3-minimap` (the full tier runs the two tests too), as 05-05 did for the players; the hermeticity test passes after the build.zig edit.

### Differences from the plan's text (recorded, not bugs)

- **Overlays are vector draws**, not CPU-side texels (D-14's wording); decision above.
- **The texture refreshes by dirty flag, not dirty rectangle**; decision above.
- **Eight files, not four**: the engine's DDS writer makes the `_c/_l/_h` trio; the plan's "four-file contract (2 sizes x 2 formats)" holds as four pictures.
- **A1 answered**: no per-format calls needed.
- **The patch grid** is drawn although the MFC's own function returns before it draws anything.
- **The engine's anchor is not exactly what was set** (it came back about 0.7 and 2.1 world units off): the camera test checks the property that matters - the clicked point is at the middle of the screen - not the anchor's equality.
- **The fire-range layer is not here** (plan 05-06 is not executed yet): the panel draws what the AI shows; the engine test registers a group itself. See deferred-items.md for the object-filter and invalid-height gaps.

**Total deviations:** 5 auto-fixed (3 blocking, 2 bugs), none widening the scope beyond the minimap.

## Issues Encountered

- A bare `find /` in a read-only look for the ImGui header ran past its timeout and sat in the background; it did nothing and was not waited for.
- macOS `sed -i` needs a backup argument; two edits went through python instead.
- The first bridge build failed on a path-case check (`aiconsts.h` is lower case on disk) and on `long long( x )` casts, which C++ does not take; fixed in the same task.

## Deferred Issues

See `deferred-items.md` ("From 05-07"): the object palette's filter on the markers, the red for invalid heights, the fire-range layer waiting for 05-06, the whole-texture rebuild, and the Windows leg.

## Known Stubs

None.

## Threat Flags

None. T-05-07-01 (a malformed picture): the decode runs behind `Guarded` and its own `catch`, and `TestM3MinimapImages` feeds a garbage `.tga` and gets a refusal. T-05-07-02 (writing into shipped Data): the bridge refuses a relative path and any resolved path under the installation's Data folder, and the test proves nothing is written beside the shipped map. T-05-07-03 (the minimap dirtying the map): the reads touch no snapshot; the engine tests save the map before and after and compare bytes, the scenario expects `dirty:0` after Create.

## Gate results (macOS arm64)

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run --summary all`: `Build Summary: 32/32 steps succeeded; 659/659 tests passed` (the core's 315 tests, panels 165, among them the minimap's).
- `zig build test-editor-bridge ...` (7 min 27 s): `editor-bridge: M3 minimap reads ok`, `editor-bridge: M3 minimap colour spot check: 3 tiles compared, 0 differ`, `editor-bridge: M3 minimap images ok`, `editor-bridge: PASS`, rc 0. `zig build test-editor-bridge-m3-minimap ...` alone: rc 0.
- `zig build test-map-editor-engine map-editor-smoke test-map-files ...`: `map-editor-engine: M3 minimap reads and click round trip ok`, `map-editor-engine: PASS (260 objects)`, `map-editor: smoke PASS (52 steps, 260 objects, ...)`, `map-file: PASS`, `157/157 steps succeeded; 181/181 tests passed`.
- `zig build map-editor-m3-auto ...`: rc 0 - M1 `done (13 actions)`, M2 `done (298 actions)`, M3 `done (227 actions)`; `differ m3-minimap-before/m3-minimap-after: 14.0233% of pixels differ`, `differ m3-minimap-after/m3-minimap-heights: 11.1033% of pixels differ`. The M1 and M2 reference compares (0.0000%-0.4462%) are unchanged: the panel is closed by default, so no reference was re-seeded.
- `zig test tools/zig/build_hermeticity_test.zig`: `All 3 tests passed` (after the build.zig edits).
- No game-side proof is named by this plan.

## Self-Check: PASSED

- Files: Sources/editor/app/minimap.zig, the bridge entries in Sources/src/EditorBridge/bridge.h and bridge.cpp, and the eight pictures the scenario wrote beside zig-out/local-test/map-editor-m3-auto/m3-minimap.bzm exist.
- Commits: cdf6a1bdf, 93aa384d4 and a78330a53 are in `git log`.

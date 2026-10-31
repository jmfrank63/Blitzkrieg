# Remastered Compare View + Load-Time Mesh Shadows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Side-by-side 2003-vs-Remastered compare in the Resource Editor's Buildings editor, a per-object mirror flag in the views generator, and building shadows flattened from the 3D model at mission load instead of pre-rendered shadow packs.

**Architecture:** Three phases on one branch (`feat/remastered-compare`), each independently green. Phase 1 adds `compare_logic.zig` — pure logic against the fake bridge, the editor's established pattern — plus thin drawing in the image frame. Phase 2 teaches bk_views per-state rendering and a mirror list. Phase 3 moves building shadows into the engine: meshes ship in the mod, MOBuilding holds static flattened geometry, re-flattened on state change; the generator stops writing shadow packs last, so the Game never runs without shadows.

**Tech Stack:** Zig 0.17 (`zig build`), C++17 engine (MSVC/clang), Dear ImGui Resource Editor, EditorBridge C ABI, tracked fixtures in `tools/zig/fixtures/`.

**Spec:** `docs/superpowers/specs/2026-10-31-remastered-compare-design.md`

**Rules that hold for every task:** one `zig build` at a time (AGENTS.md); new files CRLF; tests use only tracked data copied to `zig-out/local-test/...`, never the user's mod or profile; conventional commit prefixes; never push (the maintainer merges — for this feature the maintainer has asked for the merge into main when green, which happens in the final task from the primary checkout).

---

## File structure

- Create: `Sources/editor/resource_app/compare_logic.zig` — pure compare logic: pack discovery, per-state/step pairs, flip/rotate transforms, anchor mirroring. No ImGui, no window; tested against the fake bridge like `image_logic.zig`.
- Modify: `Sources/editor/resource_app/docks.zig` — the image frame gains the "Compare Remastered" toggle, split drawing, thumbnail strip; only draws and forwards (its established role).
- Modify: `Sources/editor/resource_app/scenario.zig` — the pixel-measured check of the compare frame.
- Create: `tools/zig/fixtures/remastered_compare/` — a tiny fixture building with a two-state fixture pack (tracked data only).
- Modify: `tools/zig/bk_views.zig` — per-state rendering, mirror list, (last) stop writing shadow packs.
- Modify: `tools/zig/bk_views_roundtrip.zig`, `tools/zig/camera_turn_full3d_game_reads_it.zig` — expectations without shadow packs; byte-exact mirror expectation.
- Modify: `Sources/src/Common/MOBuilding.cpp/.h`, `Sources/src/Common/MOObject.cpp` — static flattened shadow from the shipped mesh; remove baked-pack special cases.
- Modify: `Sources/src/Main/CameraTurn.h` — `ShadowModelPath` retires with the packs.
- Modify: `build.zig` — stage meshes into the mod; the dense-map load/frame budget tier.
- Modify: `AGENTS.md` — tier list if a tier is added or renamed.

---

## Phase 1 — The compare view (what the maintainer opens)

### Task 1: `compare_logic.zig` — pack discovery and the compare plan

**Files:**
- Create: `Sources/editor/resource_app/compare_logic.zig`
- Modify: `build.zig` (only if the logic test target enumerates files explicitly — check line ~5387, `resource_app_logic_step`; if it uses the module, nothing to do)

- [ ] **Step 1: Read the pattern to copy**

Read `Sources/editor/resource_app/image_logic.zig` (whole file, 350 lines) and the `test` blocks at its bottom. Note: `Kind` maps `bridge_mod.Kind` to the image frame's kinds; `sourcePath(buffer, doc, kind, project_folder)` resolves where a kind's pictures live; tests build a `Document` and assert on paths/rects with no bridge I/O.

- [ ] **Step 2: Write the failing tests**

Create `compare_logic.zig` with the module doc comment ("The Remastered compare frame's logic ...") and this core, plus tests:

```zig
//! The Remastered compare frame's logic, with no window and no ImGui: which
//! pictures the two panes show for the selected building, state and camera
//! step, where the Remastered pack lives beside the editor's own install,
//! what the thumbnail strip lists, and the flip/rotate display transforms
//! with their anchor crosses. Everything that decides something is tested
//! here; the image frame only draws and forwards.

pub const Pack = struct {
    /// Path of a picture inside the pack, forward slashes, no drive.
    path: []const u8,
    /// Width and height the engine reader reports, for the shared zoom.
    width: i32,
    height: i32,
    /// Anchor cross inside the picture, image coordinates.
    anchor: Point2,
};

/// `<remastered root>/data/<desc path>/<step><lit?>/<state>` is the layout
/// camera-turn-mod writes and NCameraTurn reads. The compare needs one
/// picture per state at step 0, and the strip lists every other view.
pub fn packRoot(buffer: []u8, editor_exe_dir: []const u8) ?[]const u8;

pub const Side = enum { original_2003, remastered };

pub const Pair = struct {
    original: ?[]const u8,   // resolved path or null when the 2003 side lacks the state
    remastered: ?[]const u8, // null when the pack lacks it
};

pub fn pairFor(allocator: std.mem.Allocator, mod_data: []const u8, desc_path: []const u8, state: []const u8, step: u2) !Pair;

pub const StripEntry = struct { label: []const u8, path: []const u8 };

/// The Remastered-only views: camera steps 1-3 and the lit variants
/// (1l1, 2l2, 3l3) of the selected state. Steps order before lit, both
/// ascending, as camera-turn-mod writes them.
pub fn stripFor(allocator: std.mem.Allocator, mod_data: []const u8, desc_path: []const u8, state: []const u8) ![]StripEntry;

/// Display-only transform of one image-space point. Mirror: x -> w-1-x.
/// Rotate 180: (x, y) -> (w-1-x, h-1-y). The anchor cross moves with the
/// pixels; the left pane never transforms.
pub fn transformPoint(p: Point2, width: i32, height: i32, mode: Mode) Point2;

pub const Mode = enum { upright, mirrored, rotated_180 };
```

Tests assert: the pack root sits at `<editor exe dir>/remastered/mods/remastered_views/data`; path joins are case-insensitive the way the Map Editor's `DataFile` helper resolves Data (reuse that helper via the bridge if exposed, else compare lowercased); `pairFor` joins `mod_data + desc_path + ...` for state "1" step 0 on the remastered side — before writing the test, look at the staged mod's real layout (`ls zig-out/remastered/mods/remastered_views/data`) and make the join match what the generator writes today, not a guess; `stripFor` returns steps 1,2,3 then the lit variants, skipping folders that do not exist in the fixture tree; `transformPoint` mirrors and rotates a known point in a known size. Use a fixture directory tree created by the test into a temp dir under `zig-out/local-test/compare-logic/` (delete-tree first, as `camera_turn_missions_game_reads_it.zig` does).

- [ ] **Step 3: Run the logic tests**

Run: `zig build test-resource-app-logic`
Expected: PASS (new tests included; if the target does not pick up the new file, add it where `image_logic` is listed in `build.zig` near line 5387).

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/resource_app/compare_logic.zig
git commit -m "feat(resource-editor): compare frame logic — pack discovery, strip, flip/rotate transforms"
```

### Task 2: The toggle and the split drawing in the image frame

**Files:**
- Modify: `Sources/editor/resource_app/docks.zig` (the ImageFrm draw + input forwarding)

- [ ] **Step 1: Anchor the image frame**

Read the ImageFrm section of `docks.zig`: find where it draws the active picture (search `image_logic.View`, `toScreen`) and where its toolbar/toggles live. Note how the frame keeps one `image_logic.View` (zoom/pan).

- [ ] **Step 2: Implement the split**

With the toggle on: two child viewports sharing one `View`; left draws the 2003 picture of the selected state via the existing image draw call, right draws the compare side from `compare_logic.pairFor`, its pixels passed through `compare_logic.transformPoint` when the display mode is not `upright`. Both anchor crosses draw at the transformed image coordinates. The toggle renders in the frame's toolbar row, enabled only when `pairFor(...).remastered != null` for the current object; when disabled it shows the one-line reason (no Remastered pack for `<desc path>`). All decisions come from `compare_logic.zig`; this file only draws and forwards, as its header demands.

- [ ] **Step 3: Build and run the editor's checks**

Run: `zig build test-resource-app-logic && zig build resource-editor-host-check`
Expected: both PASS.

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/resource_app/docks.zig Sources/editor/resource_app/compare_logic.zig
git commit -m "feat(resource-editor): Compare Remastered toggle splits the image frame"
```

### Task 3: Thumbnail strip and right-pane selection

**Files:**
- Modify: `Sources/editor/resource_app/docks.zig`

- [ ] **Step 1: Draw the strip**

Under the panes, one row of thumbnails from `compare_logic.stripFor` (steps 1-3, then lit variants, ascending), each rendered through the existing thumbnail path of the tree; click sets the right pane's picture. The strip reflects the selected state; changing the state re-runs `stripFor`.

- [ ] **Step 2: Flip / rotate controls**

Two toolbar buttons next to the toggle: mirror, rotate-180. They set `compare_logic.Mode` for the right pane only; the anchor cross moves with `transformPoint`. Keyboard: none (YAGNI).

- [ ] **Step 3: Build and run**

Run: `zig build test-resource-app-logic && zig build resource-editor-host-check`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/resource_app/docks.zig
git commit -m "feat(resource-editor): compare strip, flip and rotate for the Remastered pane"
```

### Task 4: The pixel-measured scenario check

**Files:**
- Create: `tools/zig/fixtures/remastered_compare/` (fixture building: an original picture with a known anchor; a remastered pack with state 1 intact and state 2 damaged, steps 1-3 and one lit variant)
- Modify: `Sources/editor/resource_app/scenario.zig`

- [ ] **Step 1: The fixture**

Copy the smallest tracked building pictures into the fixture layout camera-turn-mod writes (state folders `1`, `2`; step folders `1`, `2`, `3`, `1l1`; no shadow folders — they go away in Phase 3). Keep every file small (downscale, keep the palette format the reader expects).

- [ ] **Step 2: The check**

In `scenario.zig`, following the existing shot-based checks (see `shot_mark_cross` for the measuring pattern): open the fixture project, select the building, enable the toggle, capture the frame, and assert (a) both halves drew (pixel counts per half above their thresholds), (b) the two anchor crosses sit at the same image coordinates, (c) with mirror on, the right half's pixel count equals the upright right half's (a mirror preserves pixels) and the right cross lands at the mirrored position, (d) the strip shows five thumbnails for the intact state. Assert nothing that depends on font rendering.

- [ ] **Step 3: Register and run**

Add the check to the editor auto tier the scenario file feeds (see how `resource-editor-auto-bld` chains its checks in `build.zig`).

Run: `zig build test-resource-editor-scenario && zig build resource-editor-auto-bld`
Expected: PASS (auto-bld is the tier the UI change must keep green; generous timeout, single build).

- [ ] **Step 4: Commit**

```bash
git add tools/zig/fixtures/remastered_compare Sources/editor/resource_app/scenario.zig build.zig
git commit -m "test(resource-editor): the compare frame measured on a two-state fixture"
```

---

## Phase 2 — Generator: per-state rendering and the mirror flag

### Task 5: Inventory — what bk_views renders today, and which model states exist

**Files:** none (read-only investigation, findings recorded in this plan's checkboxes)

- [ ] **Step 1: States in the generator**

Read `tools/zig/bk_views.zig` until you can answer: which states does it render per object today (search the loop that builds `<step>` folders), and where the state number enters the written path. Record the function and line here.

- [ ] **Step 2: Model variants on disk**

Run: `zig build full3d-summer 2>&1 | tail -3 ; ls zig-out/remastered/models` (adjust to the actual model staging the full3d steps use; `bk_views_model.zig` names its model root).
Record: for each of the full-3D objects (e_wb/01, e_wb/02, e_house02_2, e_house01_1, a_h01_1, rick, dzot/01, fence_sandbag/01) which states the Blender export carries (intact only, or damaged/destroyed too).

- [ ] **Step 3: Record the declared states**

Run a one-off against the objects DB (the game reads `<path>` states from `UpdateModelWithHP`'s `nNewState`): the number of declared states per full-3D object. This is the gap list: declared minus exported.

- [ ] **Step 4: Commit the findings**

Append the three findings to this file under Task 5 (checkboxes below), then:

```bash
git add docs/superpowers/plans/2026-10-31-remastered-compare.md
git commit -m "docs(plans): the per-state inventory for the Remastered generator"
```

### Task 6: Render every declared state that has a model

**Files:**
- Modify: `tools/zig/bk_views.zig`

- [ ] **Step 1: Extend the render loop**

For each object: loop the declared states; render only the states Task 5 found models for; write each into the pack layout the engine reads (state number in the path — copy exactly what `UpdateModelWithHP` builds). States without a model are skipped and traced (`BK_VIEWS: <path> state <n> has no model, 2003 pictures stay`).

- [ ] **Step 2: Expectations**

Extend the bk-views test expectation files to the new pack set; the engine reader check (`camera_turn_full3d_game_reads_it`) must read every written pack.

Run: `zig build test-bk-views && zig build camera-turn-full3d-game-reads-it -Dtest-mode=run`
Expected: PASS (the second needs the staged Game; generous timeout, single build).

- [ ] **Step 3: Commit**

```bash
git add tools/zig/bk_views.zig tools/zig/bk_views_roundtrip.zig
git commit -m "feat(bk-views): render every declared state its model has"
```

### Task 7: The mirror flag

**Files:**
- Create: `tools/zig/views-mirror.txt` (tracked list; one object per line: `<desc path>[ steps]`, empty steps = all; `#` comments)
- Modify: `tools/zig/bk_views.zig`, `tools/zig/bk_views_roundtrip.zig`

- [ ] **Step 1: Parse and apply**

Read the list before rendering. A mirrored step writes its picture flipped horizontally and mirrors the anchor: `new_x = width - 1 - old_x`. Lit variants of a mirrored step follow. The flip happens on write only — the render and the expectations of non-listed objects are byte-identical to today's.

- [ ] **Step 2: Byte-exact expectation**

Add a fixture object to the mirror list in the test; `bk_views_roundtrip` asserts the written picture equals the reference flipped byte for byte and the anchor file carries the mirrored x.

Run: `zig build test-bk-views`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tools/zig/views-mirror.txt tools/zig/bk_views.zig tools/zig/bk_views_roundtrip.zig
git commit -m "feat(bk-views): per-object, per-step mirror flag with mirrored anchors"
```

---

## Phase 3 — Engine: shadows flattened from the mesh at load

### Task 8: Inventory — the vehicle shadow path and MOBuilding's shadow today

**Files:** none (read-only; findings recorded here)

- [ ] **Step 1: The vehicle flatten**

Read `Sources/src/Scene/MeshVisObj.cpp` `CMeshVisObj::DrawShadow` and its caller in `SceneDraw.cpp`. Record: the flatten matrix (sun direction source), the density constant, the draw call, and what the mesh must be loaded through to be drawable this way.

- [ ] **Step 2: Building shadow today**

Read `Sources/src/Common/MOBuilding.cpp` `GetShadowPath`/`UpdateModelWithHP`/`PreloadCameraTurn` (as of 8facff281). Record where `pShadow` gets its sprite and every baked-pack special case (step 0 2003 shadow, `bAsItLies`).

- [ ] **Step 3: Mesh loading at runtime**

Find how a runtime mesh loads for vehicles (StreamIO mesh reader used by `MESH_VISOBJ`); record the loader entry point and what a mesh file must sit beside (the staged install's layout).

- [ ] **Step 4: Commit the findings** (as Task 5 did)

### Task 9: Ship the meshes; MOBuilding flattens at load

**Files:**
- Modify: `build.zig` (camera-turn-mod staging: copy the model files per full-3D object into `remastered/mods/remastered_views/data/<desc path>/model/...`)
- Modify: `Sources/src/Common/MOBuilding.cpp/.h`

- [ ] **Step 1: Staging**

The Remastered mod carries each full-3D object's mesh(es) beside its packs; a Game without the mod sees today's behaviour unchanged.

- [ ] **Step 2: Load and flatten at mission start**

MOBuilding, on first draw (or on `PreloadCameraTurn`'s existing hook), loads its state's mesh from the mod through the vehicle mesh loader, flattens it along the world sun with the vehicle matrix, and keeps the result as static geometry at 50% density. It draws every frame like a shadow sprite did. State change (damage step, destruction, bridge repair complete, pontoon placed): flatten once more for the new state; nothing else is touched.

- [ ] **Step 3: Retire the baked-pack cases**

`GetShadowPath`, `bAsItLies`, and the step-0 2003-shadow special case go; non-3D objects keep the 2003 painted shadow exactly as today (`pDesc->szPath` sprite path, no packs involved).

- [ ] **Step 4: Commit**

```bash
git add build.zig Sources/src/Common/MOBuilding.cpp Sources/src/Common/MOBuilding.h Sources/src/Main/CameraTurn.h
git commit -m "feat(game): full-3D buildings cast flattened-mesh shadows computed at load"
```

### Task 10: bk_views stops writing shadow packs

**Files:**
- Modify: `tools/zig/bk_views.zig`, `tools/zig/bk_views_roundtrip.zig`, `build.zig`

- [ ] **Step 1: Remove the shadow pass**

The `<variant>_r<k>_s<s>` writes and their staging disappear; `ShadowModelPath` in `CameraTurn.h` retires with them (deleted, not stubbed). Expectation files drop the shadow entries. install-all gets faster — note the measured delta in the commit body.

- [ ] **Step 2: Tests**

Run: `zig build test-bk-views && zig build test-resource-app-logic`
Expected: PASS (compare_logic never listed shadow packs).

- [ ] **Step 3: Commit**

```bash
git add tools/zig/bk_views.zig tools/zig/bk_views_roundtrip.zig build.zig Sources/src/Main/CameraTurn.h
git commit -m "feat(bk-views): shadow packs retire; the engine flattens from the mesh"
```

### Task 11: The shadow tiers

**Files:**
- Modify: `tools/zig/camera_turn_full3d_game_reads_it.zig` (or the shadow tier it feeds)
- Modify: `build.zig` (the dense-map budget tier)

- [ ] **Step 1: Shadow correctness at all four steps**

The existing shadow tier's expectation changes from pack comparison to captured frames: a full-3D building at each camera step casts the flattened mesh shadow against its footprint along the sun, on the Game's own frames. On destruction the rubble's shadow replaces it (drive one destruction in the mission script if the tier's map has a destructible; the tutorial 6 bridge qualifies).

- [ ] **Step 2: The dense-map budget**

A tier loads a dense tracked map (the camera-turn campaign set already stages Poland's village; Tula's 2631 objects is the named dense case) with Remastered on: assert mission-load time grows by at most the measured precompute budget recorded as a constant in the tier, and frame time stays level with Remastered off (within the noise band the tier already uses for its frame reports).

- [ ] **Step 3: Run the Game tiers**

Run: `zig build camera-turn-tutorials-game-reads-it -Dtest-mode=run && zig build camera-turn-campaign-game-reads-it -Dtest-mode=run`
Expected: PASS (single builds, generous timeouts; never two builds at once).

- [ ] **Step 4: Commit**

```bash
git add tools/zig/camera_turn_full3d_game_reads_it.zig build.zig
git commit -m "test(game): mesh shadows measured at four steps and on a dense map"
```

### Task 12: Green sweep, docs, merge

- [ ] **Step 1: Every tier this plan touched**

Run: `zig build test-resource-app-logic && zig build resource-editor-auto-bld && zig build test-bk-views && zig build test-editor-bridge`
Expected: PASS.

- [ ] **Step 2: AGENTS.md tier list**

If Task 11 added or renamed a tier, update `AGENTS.md`'s tier enumeration in the same wording style.

- [ ] **Step 3: Merge into main (the maintainer asked for this merge)**

```bash
git -C C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg merge feat/remastered-compare
git -C C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg push
```

Report the merge commit and remind: `zig build install-all --release=fast` re-renders (pictures only now) and the Resource Editor opens the compare from the Buildings editor.

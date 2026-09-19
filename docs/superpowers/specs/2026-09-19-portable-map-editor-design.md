# Portable Map Editor

A new Map Editor, written in Zig with a Dear ImGui interface, built by
`zig build` for macOS, Linux and Windows. It replaces the MFC Map Editor in
`Sources/src/MapEditor`. It is the first sub-project of
`docs/PLANNED_FEATURES.md` §1, "Editors on every platform".

## Why a rewrite

The shipped editors are MFC applications: 32-bit, Visual Studio only, not
built by `build.zig`, packaged from checked-in executables. Their UI code is
tangled with the editing logic (MapEditor ~50k lines, 54 dialogs, SEC* grid
classes through `Common/LegacyUiCompat.h`). They pass an `HWND` to
`NMain::Initialize`, which only the legacy D3D renderer accepts; the portable
GPU renderer expects an `SDL_Window*`. Porting MFC is not realistic.
Rebuilding the UI on top of the engine's existing, already portable editing
interfaces is.

## The editor set, and the order

| Sub-project | Content | Status |
|---|---|---|
| **Map Editor M1** | core editing loop, macOS first | this document |
| Map Editor M2 | roads and rivers, AI and unit groups, scripts and areas | later |
| Map Editor M3 | random map templates, minimap tools, parity; delete the MFC editor | later |
| Resource Editor | `Sources/src/editor`, ~64k lines, 20+ sub-editors | own spec |
| ELK | localisation kit, ~12k lines | own spec |
| Small tools | converters and validators | own spec |

The MFC Map Editor stays in the tree, unbuilt, as a reference until M3 reaches
parity, then it is deleted.

## M1 scope

In:

- Open and save `.bzm` maps, in the format the game and the MFC editor read.
- View the map with the game's own renderer; camera scroll, rotate and zoom.
- Paint terrain tiles with a brush.
- Place, select, move, rotate and delete objects and units, from a palette
  built from the object database.
- Edit players and diplomacy (side of each player, attacking side).
- Undo and redo for every edit.
- Test-launch the map in the game.
- Load a mod's data (`-mod<dir>`), like the game.
- macOS arm64 builds and runs; the other five CI targets build and run the
  headless tests.

Out (later milestones): roads, rivers, fences and bridges drawing; AI groups,
reinforcements and scripts; areas and script areas; random map template
dialogs; minimap tools; mission objectives; height editing.

## Architecture

Three layers.

### 1. Engine bridge (C++, `Sources/src/EditorBridge`)

The only new C++. A flat C ABI (`extern "C"`, plain structs, integer handles,
status codes) over the portable engine. No UI, no editing policy, thin
wrappers:

- **Startup:** load the engine modules, open storage (base data, then the
  mod's), read `consts.xml`, create the object database, start the engine on
  the editor's `SDL_Window*` (`NMain::Initialize`, `IGFX::SetMode`,
  `LoadDB`). This is the game's startup without `CMainLoop` and its menu
  screens. A windowless variant exists for tests.
- **Object catalogue:** `IObjectsDB::GetAllDescs()` as plain records (name,
  kind, path, icon).
- **Map load and save:** through `CMapInfo : SLoadMapInfo`
  (`Formats/fmtMap.h`), `CWorldBase` and `IAIEditor`. This reimplements the
  MFC-free parts of `CTemplateEditorFrame::OnFileLoadMap`
  (`TemplateEditorFrame1.cpp:1357-2114`) and `SaveMap` (`2934-3291`); the
  MFC code is the reference for what is read and written, and in what order.
- **Editing:** tiles (`ITerrainEditor::GetTileIndex`, `SetTile`, `Update`),
  objects (`IAIEditor::AddNewObject`, `MoveObject`, `TurnObject`,
  `DeleteObject`), players and diplomacy (`diplomacies`, `nType`,
  `nAttackingSide`).
- **Picking and camera:** screen point to world point and object under the
  cursor (`IScene`), `ICamera::SetPlacement` and `SetAnchor`.
- **Frame:** update the world and scene, draw into the current frame.

Every call returns a status and, on failure, a message the caller can show.
No C++ exception crosses the boundary.

### 2. Editor core (Zig, `Sources/editor/core`)

No UI dependency, runs headless.

- **Document:** the map summary returned by the bridge (size, season,
  players, diplomacy, objects with id, type, position, direction, player) plus
  the dirty flag and the file path. The source of truth for panels and undo.
- **Commands:** paint tiles, add object, move, rotate, delete, set player,
  set diplomacy. Each has *do* and *undo*; both call the bridge and update the
  document. Paint records the old tile per brush cell; delete records the
  whole object and remaps the engine id when undo re-adds it. An undo stack
  with redo, and merging of one brush drag into one command.
- **Tools:** tile brush, object placer, select/move/rotate/delete, and the
  diplomacy editor's model. Tools take plain input events (world position,
  object under the cursor, buttons, modifiers, keys) and emit commands.
- **Bridge interface:** the core talks to the bridge through a Zig interface
  so tests can substitute a fake that records calls.

### 3. Editor app (Zig, `Sources/editor/app`)

- The SDL3 window, event loop and frame loop.
- Dear ImGui through the `dcimgui` C bindings (docking branch), compiled by
  `zig build` from `vendor/`, with its SDL3 and SDL-GPU backends.
- Panels: menu bar, tool palette, object palette with icons and filter,
  properties of the selection, players and diplomacy, status bar. The map view
  is the window's background; input over it goes to the active tool, input
  over a panel goes to ImGui.
- Settings and recent files in the user's profile folder, via
  `Platform/Paths`.

### Drawing ImGui on top of the engine

Each frame the engine draws the map, ImGui draws its panels into the same
frame, then the frame is presented. The GPU renderer has no such hook today
(`GraphicsEngineGpu.h`; GFXGPU exports only `gfxgpu_get_api` and
`gfxgpu_readback`). M1 adds one: an overlay callback that runs after the
engine's last pass and before present, and receives the frame's SDL GPU
command buffer and swapchain texture. ImGui's SDL-GPU backend renders there.

This is the riskiest piece and is the plan's first task, as a spike. If the
callback cannot share the render pass cleanly, the fallback is to let the
callback open its own pass on the swapchain texture with `LOAD` so it keeps
the engine's pixels.

### Build and packaging

- New step `zig build install-map-editor` producing `MapEditor` next to the
  engine dylibs it loads, in the same layout as the game install.
- `zig build test` runs the core's tests on every target and the bridge test
  where the platform can start the engine without a window.
- Packaged separately from the game; a release may ship both.

## Data flow

**Startup.** Open the window, start the engine through the bridge on it, fill
the palette from the object catalogue once.

**Open.** File → Open hands the path to the bridge. It reads the map into
`CMapInfo`, builds the terrain, places every object through `IAIEditor`, and
returns the summary. The core replaces its document and clears undo.

**Edit.** Every change is a command. The engine sees it at once, so the view
is always current, and the document and undo stack stay in step.

**Pick.** A mouse position over the map view goes to the bridge, which
returns the world point and the object under it. The active tool decides.

**Save.** The bridge builds a `CMapInfo` from the engine's state (terrain,
objects, players, diplomacy) and writes the original format.

**Test launch.** Save to a temporary map in the profile's cache (never into
Data), start the Game executable with that map, `-mod<dir>` when a mod is
loaded, and `-windowed`. The editor stays open.

## Errors

- **Startup:** if a step fails (data folder, object database, GPU), one
  dialog names the step, then the editor quits. It never runs half-started.
- **Open:** a broken map is rejected as a whole with a message naming the
  failing part; the previous map stays open. Objects whose type is unknown
  (common with mod maps) are listed in a warning and kept in the document so
  a save writes them back unchanged.
- **Edit:** a command whose bridge call fails is not recorded and leaves the
  document unchanged. Edits outside the map are refused. A placement the
  engine rejects is a status bar note, not an error.
- **Save:** write to a temporary file, swap it in only on success, then read
  it back to verify. On failure the original stays untouched and the map stays
  dirty. Close and Open ask first when there are unsaved changes.
- **Test launch:** a missing game or an immediate exit shows the exit code
  and the game log's path.
- **Crashes:** an autosave into the profile's cache every few minutes,
  offered for restore at the next start.

## Testing

- **Core, headless, all six CI targets:** do/undo/redo round trip per
  command; tools under scripted input against the fake bridge (a brush drag
  paints the expected cells, drag-move moves, delete and undo restores the
  same object and player, diplomacy edits).
- **Bridge against the real engine:** a windowless test binary opens real
  maps from Data (one small, one large) and saves them unchanged. **Open then
  save must not change the map**: same terrain, objects, players and
  diplomacy. Then edit (a tile, an added unit, a moved unit, a diplomacy
  entry), save, reopen and compare. macOS and Linux in CI, Windows if the
  windowless start works there.
- **The game reads what the editor writes:** an editor-saved map is loaded by
  the game under `BK_AUTO_UI`, shot and exited cleanly; the placed unit is
  there.
- **Editor app:** an automation variable like the game's (`BK_EDITOR_AUTO`:
  clicks, keys, shots, exit), shots into `zig-out/local-test`. A smoke script
  opens a map, paints, places a unit, saves, test-launches and quits; shots
  are compared by hand for now.
- **Overlay spike:** a readback check that the frame holds both the map's
  pixels and an ImGui panel's pixels, measured from the image.

## Risks

- **Overlay hook:** see above; settled first by the spike.
- **Map save fidelity:** `CMapInfo::Load` is declared but never defined and
  the MFC editor's save path is long; the unchanged round trip test is the
  guard.
- **Windowless engine start:** the bridge tests need the engine without a
  real swapchain; if GFXGPU cannot start windowless on a CI runner, those
  tests run on the developer's machine only and CI keeps the core tests.
- **Undo across engine ids:** the engine assigns object ids; the core maps
  them so undo and redo never reference a stale id.

## Exit criteria for M1

- `zig build install-map-editor` builds on all six CI targets; the headless
  tests pass there.
- On macOS arm64 the editor opens a shipped map, paints tiles, places, moves,
  rotates and deletes units, edits diplomacy, undoes and redoes all of it,
  saves, and the game plays the saved map.
- Opening and saving a shipped map unchanged gives an equivalent map.

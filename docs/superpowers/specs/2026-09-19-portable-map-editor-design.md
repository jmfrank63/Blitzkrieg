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

**Preservation invariant.** Every part of a map that M1 does not edit is
written back exactly as it was read, including the parts M1 cannot show or
edit (roads, rivers, bridges, entrenchments, scripts, script areas,
reinforcements, start commands, reserve positions, unit creation, sounds, AI
general data, camera anchors, mod name and version) and objects whose type
the object database does not know. M1 never drops, reorders or recomputes
data it did not change. See "Saving: the snapshot and the overlay".

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
  the editor's window (see "Startup contract"), `IGFX::SetMode`, `LoadDB`.
  This is the game's startup (`Game/GameMain.cpp`) without `CMainLoop` and
  its menu screens. A data-only variant (storage, constants, object database,
  no renderer, no window) exists for the map file tests.
- **Object catalogue:** `IObjectsDB::GetAllDescs()` as plain records (name,
  kind, path, icon).
- **Map load and save:** see "Map files", "Building the engine state" and
  "Saving: the snapshot and the overlay" below.
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
  document. Paint records the tiles and crosses of its whole affected region
  (see "Terrain edits"); delete records the
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

### Startup contract

`NMain::Initialize( HWND hWnd3D, HWND hWndInput, HWND hWndSound, bool bGame )`
(`Main/Initialization.cpp:65`) uses only `hWnd3D`, and only to hand it to
`IGFX::Init`, which already takes a portable `GFXNativeWindow` (a `void *`,
`GFX/GFXPlatform.h`). The other two handles and `bGame` are unused. The game
gets there by casting its `SDL_Window*` to `HWND` (`Game/GameMain.cpp:625`).

M1 **extends the API** rather than adding another cast:

- New `bool NMain::InitializeWithWindow( GFXNativeWindow window )` holds the
  current body.
- `NMain::Initialize( HWND, HWND, HWND, bool )` stays for source compatibility
  and forwards `hWnd3D` to it.
- The game switches to the new call and drops its cast. The bridge calls it
  with the editor's `SDL_Window*`.

GFXGPU cannot start without a window: `Renderer.attachWindow` returns
`NullWindow` for a null handle (`GFXGPU/renderer.zig:476`). So every
engine-backed test uses a real, hidden SDL window and needs a GPU device.
Tests that need no renderer use the data-only startup instead.

### Map files

There is no `CMapInfo::Load`: it is declared (`RandomMapGen/MapInfo_Types.h`)
and never defined, and M1 does not define it. M1 adds a small, GFX-free C++
unit, `Formats/MapFile.{h,cpp}`, with a reader and a writer for both formats.
It is lifted from the two places that already do this correctly:

- **Read** (the game's reader, `GameTT/iMissionInternal.cpp:1362-1404`):
  - `.xml`: `CreateDataTreeSaver( stream, IDataTree::READ )` then
    `AddTypedSuper( &mapinfo )`.
  - `.bzm`: `CreateStructureSaver( stream, IStructureSaver::READ )` then
    chunk `1` into `mapinfo`.
  - An explicit file path chooses the format by its extension. A map name
    without an extension picks the newer of the two files, as the game does.
  - `CMapInfo::IsValid()` must hold, otherwise the load fails.
  - Unlike the MFC editor (`TemplateEditorFrame1.cpp:1644`), it does **not**
    call `RemoveNonExistingObjects`.
- **Write** (the MFC editor's writer, `TemplateEditorFrame1.cpp:3238-3258`):
  - `.bzm`: chunk `1` is the map, chunk `RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER`
    (2) is an `SQuickLoadMapInfo` filled from it.
  - `.xml`: `AddTypedSuper` for the map, plus the quick info under
    `RMGC_QUICK_LOAD_MAP_INFO_NAME`.
  - Frame indices are written in their packed form.

The writer is used by the bridge and by the map file tests, which do not need
the renderer.

### Building the engine state

After a successful read, the bridge keeps two copies:

- The **snapshot**: the map exactly as read, frame indices still packed.
- A **working copy** with `UnpackFrameIndices()` applied. Unpacking picks a
  random visual variant per type, so it is never written back for objects
  that were not edited.

It then builds the engine state from the working copy, in the MFC editor's
order (`TemplateEditorFrame1.cpp:1657-1770`; the plan lists every call):

1. `IAIEditor::SetDiplomacies`, then `IAIEditor::Init( terrain )`.
2. `CreateTerrain()`, `ITerrain::Load( name, terrain )`,
   `IScene::SetTerrain`.
3. Every object and scenario object placed through `AddObjectByAI` with its
   link ID. Bridges are placed as their linked spans.
4. `CWorldBase::Update`, then the camera placed at `vCameraAnchor`.

Every engine object is recorded against the snapshot object it came from:
its list (`objects` or `scenarioObjects`) and index, keyed by the object's
**link ID**. Objects whose type is unknown are not placed in the engine. They
are still kept in the snapshot, and the document lists them as unknown.

### Saving: the snapshot and the overlay

The MFC editor saves by rebuilding the whole map from its UI and the engine,
then recomputing shades (`TemplateEditorFrame1.cpp:2934-3291`). M1 does not.
It saves the **snapshot with the M1 edits laid over it**:

- **Terrain:** the bridge's own terrain copy (see "Terrain edits" below).
  Only `tiles` and the crosses of `patches` can differ from the snapshot, and
  only inside the affected regions of paint commands. `altitudes` and their
  shades, patch heights, `rivers`, `roads3`, and the tileset, crosset and
  noise names are always the snapshot's.
- **Objects:**
  - An untouched object is written as its snapshot record, byte for byte
    (packed frame index, HP, script ID, link).
  - A moved, rotated or re-owned object keeps its snapshot record; only
    `vPos`, `nDir` or `nPlayer` change.
  - An added object gets a new record with a fresh link ID (above every link
    ID in use, `GetUsedLinkIDs`), is appended at the end of its list, and has
    its frame index packed.
  - A deleted object's record is removed.
  - Unknown objects are always written back unchanged.
- **Diplomacy and players:** `diplomacies`, `nType`, `nAttackingSide` and
  the per-player `unitCreation` entries that M1 edits. Nothing else in
  `unitCreation` changes.
- **Everything else** is written from the snapshot unchanged.

### Terrain edits

A painted tile has derived effects the engine computes, and the saved map
must match what the engine shows. So the rule is: **derived terrain changes
are allowed, but only as the deterministic function below, inside the
affected region, and the expected value in tests is built with the same
function.** It is not "the edited tile only".

The function, for one paint command with the set of painted cells `C`:

1. **Affected region `R`:** every patch that contains a cell of `C` or a
   cell next to one (8-neighbourhood). A painted cell on a patch border so
   includes the neighbouring patch, whose crosses read the border cells.
2. Set the tiles of `C` (tile index and noise flag).
3. `CTerrainBuilder::PreprocessMapSegment` over the tile rectangle of `R`.
   This pass removes one-cell-thin strips of lower-priority terrain, so it
   can change tiles in `R` outside `C`. That is intended; it is what the
   engine does.
4. Regenerate the crosses (base, layer, noise) of every patch in `R` with
   `MapSegmentGenerateCrosses` and `CopyCrosses`.

Steps 3 and 4 are `CMapInfo::UpdateTerrainCrosses( terrain, R, tileset,
crosset )` (`RandomMapGen/MapInfo_StaticMethods.cpp:420`). It needs no
renderer, contains no randomness, and runs the same `CTerrainBuilder` code
as the engine's `CTerrain::Update` (`Scene/TerrainEditor.cpp:103`; the two
`TerrainBuilder.cpp` copies are identical). It does **not** run the rivers,
roads or shades updates that `CMapInfo::UpdateTerrain` adds, and neither
does the engine's update.

The bridge applies the function to its own terrain copy, starting from the
snapshot's terrain, and then pushes the region's tiles and crosses into the
engine. Save writes that copy. The engine is never the source of the saved
terrain. The engine tier checks that the engine's terrain
(`ITerrainEditor::GetTerrainInfo`) equals the copy in `tiles` and patch
crosses after every paint.

The pass in step 3 makes the order of commands matter. So:
- the function runs once per paint command, never batched at save;
- a paint command records the tiles and patch crosses of all of `R` before
  it runs;
- undo restores exactly those, in the copy and in the engine.

**References to deleted objects.** Other records can refer to an object's
link ID: `bridges`, `startCommandsList.unitLinkIDs`, `reinforcements`,
`link.nLinkWith` of objects inside it, and groups. Deleting such an object
in M1 is **refused**, with a status bar message naming what refers to it,
until M2 can edit those references. Deleting an object that nothing refers
to always works. The only exception is a passenger whose `nLinkWith` points
at a deleted vehicle: the vehicle's delete is refused too.

### Drawing ImGui on top of the engine

Each frame the engine draws the map, ImGui draws its panels into the same
frame, then the frame is presented. The GPU renderer has no such hook today
(`GraphicsEngineGpu.h`; GFXGPU exports only `gfxgpu_get_api` and
`gfxgpu_readback`). M1 adds one: an overlay callback that runs after the
engine's last pass and before present, and receives the frame's SDL GPU
command buffer and colour target (the swapchain texture normally, or the
capture texture while a capture is in progress). ImGui's SDL-GPU backend
renders there.

Settled by the overlay spike (plan
`docs/superpowers/plans/2026-09-19-map-editor-01-overlay-spike.md`):

- The overlay callback runs in `endFrame` after the scene blit and before
  submit. It owns its own `LOAD` render pass on the target, because ImGui's
  SDL GPU backend uploads in a copy pass that may not overlap a render pass.
- ImGui draws at drawable resolution, independent of the scene size.
- A capture composes the frame into a readable texture and copies it to the
  swapchain, so tests read back exactly what was presented.
- Measured on macOS arm64 (`metal`, swapchain format `12`, frame size
  `320x240`): the panel pixel was `(255,0,255)`, the scene pixel
  `(0,255,0)`.
- Hidden window: `PASS`.

### Build and packaging

- New step `zig build install-map-editor` producing `MapEditor` next to the
  engine dylibs it loads, in the same layout as the game install.
- `zig build test` runs the core and map file tiers on every target. A
  separate step, `zig build test-map-editor-engine`, runs the engine tier.
- Packaged separately from the game; a release may ship both.

## Data flow

**Startup.** Open the window, start the engine through the bridge on it, fill
the palette from the object catalogue once.

**Open.** File → Open hands the path to the bridge. It reads the file with
`MapFile`, keeps the snapshot, builds the engine state from the working copy
(see above), and returns the summary. The core replaces its document and
clears undo.

**Edit.** Every change is a command. The engine sees it at once, so the view
is always current, and the document and undo stack stay in step.

**Pick.** A mouse position over the map view goes to the bridge, which
returns the world point and the object under it. The active tool decides.

**Save.** The bridge lays the M1 edits over the snapshot (see "Saving: the
snapshot and the overlay") and writes it with `MapFile` in the file's own
format. The written map becomes the new snapshot.

**Test launch.** Save to a temporary map in the profile's cache (never into
Data), start the Game executable with that map, `-mod<dir>` when a mod is
loaded, and `-windowed`. The editor stays open.

## Errors

- **Startup:** if a step fails (data folder, object database, GPU), one
  dialog names the step, then the editor quits. It never runs half-started.
- **Open:** a broken map is rejected as a whole with a message naming the
  failing part; the previous map stays open. Objects whose type is unknown
  (common with mod maps) are listed in a warning, are not shown, and are
  written back unchanged from the snapshot.
- **Edit:** a command whose bridge call fails is not recorded and leaves the
  document unchanged. Edits outside the map are refused. A placement the
  engine rejects is a status bar note, not an error. So is a refused delete
  of a referenced object.
- **Save:** write to a temporary file, swap it in only on success, then read
  it back to verify. On failure the original stays untouched and the map stays
  dirty. Close and Open ask first when there are unsaved changes.
- **Test launch:** a missing game or an immediate exit shows the exit code
  and the game log's path.
- **Crashes:** an autosave into the profile's cache every few minutes,
  offered for restore at the next start.

## Testing

### What "equivalent" means

Two maps are **equivalent** when a field-by-field comparison of the two
`SLoadMapInfo` values, as `MapFile` reads them, finds no difference in any of:

- **terrain:** `szTilesetDesc`, `szCrossetDesc`, `szNoise`, every `tiles`
  cell, every `patches` entry (with base, layer and noise crosses, min/max
  and sub-patch heights), every `altitudes` vertex (height and shade),
  `rivers`, `roads3`;
- **objects and scenario objects**, in order: `szName`, `vPos`, `nDir`,
  `nPlayer`, `nScriptID`, `fHP`, packed `nFrameIndex`, and `link` (`nLinkID`,
  `bIntention`, `nLinkWith`);
- `entrenchments`, `bridges`, `reinforcements`, `szScriptFile`,
  `scriptAreas`, `vCameraAnchor`, `playersCameraAnchors`, `nSeason`,
  `szSeasonFolder`, `diplomacies`, `unitCreation`, `startCommandsList`,
  `reservePositionsList`, `soundsList`, `szForestCircleSounds`,
  `szForestAmbientSounds`, `szChapterName`, `nMissionIndex`, `nType`,
  `nAttackingSide`, `sounds`, `aiGeneralMapInfo`, `szMODName`,
  `szMODVersion`, and every field of `SLoadMapInfo` added later.

Floats are compared exactly, because nothing is recomputed. The comparator
is written once, in C++ next to `MapFile`, and fails on any field it does
not know, so a new field cannot slip past it.

For an edited map, the expected value is built by an **expected-value
builder**. It starts from the original map as read and replays the edit
sequence:
- a paint command runs the terrain function of "Terrain edits" (the same C++
  code the bridge uses);
- object and diplomacy edits apply the overlay rules of "Saving".

The saved map must then be equivalent to that value. So, for terrain:
- outside the affected regions, every tile and every patch equals the
  original;
- inside them, `tiles` and patch crosses equal the builder's result;
- `altitudes`, patch heights, `rivers` and `roads3` equal the original
  everywhere.

Two further checks:
- **Idempotent save:** saving, reading and saving again gives byte-identical
  files.
- **Quick info:** the quick info chunk matches `SQuickLoadMapInfo::FillFromMapInfo`
  of the saved map.

### Test tiers and the M1 CI gate

| Tier | Needs | Runs on | Gate |
|---|---|---|---|
| **Core** (Zig) | nothing | all six CI targets | required |
| **Map file** (C++) | data-only startup | all six CI targets | required |
| **Engine** (C++) | hidden SDL window and a GPU device | macOS arm64 locally, CI where the runner has a GPU device | required locally on macOS arm64; in CI it reports "skipped: no GPU device", never a pass |
| **Game reads it** | the game and a window | macOS arm64 locally | required locally |
| **Editor app** | the editor and a window | macOS arm64 locally | required locally |

- **Core:**
  - Each command gets a do, undo, redo round trip.
  - Tools run under scripted input against the fake bridge: a brush drag
    paints the expected cells, drag-move moves, delete then undo restores the
    same object, player and link ID.
  - Diplomacy edits are checked the same way.
  - A refused delete leaves the document and history unchanged.
- **Map file:**
  - Read shipped maps and write them without edits: the result is
    equivalent, and the idempotent save holds. In CI this covers the 59 maps
    in `Data/Maps` plus a fixed sample from `Data/Scenarios`. A local step,
    `zig build test-map-files-all`, sweeps all 1,755 shipped `.bzm` files and
    the `.xml` maps.
  - Apply the overlay for scripted edits without the engine (tiles inside
    one patch, tiles on a patch border, a thin strip the preprocessing pass
    removes, paint then undo restoring the region exactly, an added object, a moved object, a deleted object with no
    references, a refused delete of a referenced object, a diplomacy change)
    and compare with the expected map.
  - A map with an unknown object keeps it through a save.
  - This tier guards the preservation invariant on every platform.
- **Engine:**
  - Open two shipped maps (a small and a large one) and save them without
    edits: equivalent.
  - Paint a tile, add, move, rotate and delete a unit, and change diplomacy
    through the real `ITerrainEditor` and `IAIEditor`; save, read back and
    compare with the expected map.
- **Game reads it:** a map saved by the editor is loaded by the game under
  `BK_AUTO_UI`, shot and exited cleanly, and the placed unit is there.
- **Editor app:**
  - An automation variable like the game's (`BK_EDITOR_AUTO`: clicks, keys,
    shots, exit), with shots in `zig-out/local-test`.
  - A smoke script opens a map, paints, places a unit, saves, test-launches
    and quits. For now I compare the shots by hand.
- **Overlay spike:** a readback check that the frame holds both the map's
  pixels and an ImGui panel's pixels, measured from the image.

## Risks

- **Overlay hook:** see above; settled first by the spike.
- **Map save fidelity:** the snapshot overlay and the equivalence tests over
  every shipped map are the guard. The remaining risk is terrain, where the
  engine's result could differ from the terrain function (for example
  through a different update rectangle). The engine tier compares the two
  after every paint.
- **No GPU on CI runners:** engine tests cannot run there. The preservation
  invariant is therefore enforced by the data-only map file tier, which runs
  everywhere; the engine tier adds the real editing APIs on macOS.
- **Undo across engine ids:** the engine assigns object ids; the core maps
  them so undo and redo never reference a stale id.

## Exit criteria for M1

- `zig build install-map-editor` builds on all six CI targets, and the core
  and map file tiers pass there.
- The engine, game and editor app tiers pass locally on macOS arm64.
- On macOS arm64 the editor opens a shipped map, paints tiles, places, moves,
  rotates and deletes units, edits diplomacy, undoes and redoes all of it,
  saves, and the game plays the saved map.
- Opening and saving any shipped map unchanged (the full local sweep) gives an equivalent map, as
  defined under Testing.

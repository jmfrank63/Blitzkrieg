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
| **Map Editor M2** | roads and rivers, bridges (with rotate and built during play), entrenchments, fences, script IDs and reinforcement groups, start commands, reserve positions, AI general, script file and script areas, camera anchors; see "M2 scope" | done (phase 4, 2026-09-30; "Exit criteria for M2") |
| Map Editor M3 | random map templates, minimap tools, parity; delete the MFC editor | phase 5 (in progress) |
| Resource Editor | `Sources/src/editor`, ~64k lines, 20+ sub-editors | own spec |
| ELK | localisation kit, ~12k lines | own spec |
| Small tools | converters and validators | own spec |

The MFC Map Editor stays in the tree, unbuilt, as a reference until M3 reaches
parity, then it is deleted.

## M1 scope

In:

- Open and save `.bzm` maps, in the format the game and the MFC editor read.
- View the map with the game's own renderer; camera scroll and zoom, pointer
  anchored, in the game's own steps. **Changed from the spec, by decision on
  2026-09-28 (03-06):** free camera rotation (D-12) is deferred out of M1 —
  measured, not assumed. An engine-tier measurement at yaw offsets
  +0/+30/+90/+180/+270 on `coldwinter` showed the lower half of the frame
  going from 0.5% black at the game's own 45° to 9.4%, 94.0%, 99.2% and 89.8%
  black at the other offsets: the terrain quad is laid out in fixed screen
  space (`CTerrain::MovePatches`, `Scene/TerrainInternal.cpp:237-256`) and is
  progressively clipped away by any yaw but the game's own, while billboard
  sprites (`SGVOT_SPRITE`) stay upright and fixed in their pre-rotation
  screen positions with no ground left under them. Delivering it correctly
  needs terrain rendering through the view matrix, engine/renderer work
  outside M1's scope. `BkEditorSetYaw` and the measurement stay in the
  bridge as evidence; no input is wired to them. **Closed for the map
  editor on 2026-09-30 (phase 4, D-23):** not "a future M2/M3 revisit" - see
  "M2 scope" below for the reason.
- Paint terrain tiles with a brush.
- Place, select, move, rotate and delete objects and units, from a palette
  built from the object database, each shown with its own picture (see
  "Object palette pictures" below).
- Edit players and diplomacy (side of each player, attacking side).
- Edit the map's sound list: add (at the view's centre), edit and delete,
  each marked on the map and one undo step (see "Editor: sound list" below).
- Undo and redo for every edit.
- Test-launch the map in the game.
- Load a mod's data (`-mod=Name`, `-mod=None`), like the game; File → Mod
  switches it live, closing the open map and reloading the object palette
  (D-26, revised 2026-09-29 in the hand try). **Changed from the spec,
  by decision during 03-08:** the spec originally said `-mod<dir>`; the game
  now takes `-mod=Name` (confirmed by plan 5's own spec-correction note).
- Editor settings, recent files and autosave with crash recovery, all
  independent of game profiles (see "User data: maps, settings and mods"
  below).
- macOS arm64 and Windows x64 (MSVC) build and run the editor app.
  **Changed from the spec, by decision on 2026-09-24 (plan 5):** not macOS
  only. The other four CI targets build and run the core and map file tiers
  only.

**Preservation invariant.** Every part of a map that M1 does not edit is
written back exactly as it was read, including the parts M1 cannot show or
edit (roads, rivers, bridges, entrenchments, scripts, script areas,
reinforcements, start commands, reserve positions, unit creation, AI general
data, camera anchors, mod name and version) and objects whose type the object
database does not know. **Changed from the spec, by decision during 03-10:**
the map's sound list (`CMapInfo::sounds.sounds`, not `soundsList` — see
"Editor: sound list" below) is edited by M1, with its own undo/redo; M1 never
drops, reorders or recomputes any of the rest of the data it did not change.
See "Saving: the snapshot and the overlay". M2 (phase 4) edits the collections
named above record by record and keeps the same invariant for every record it
does not touch; see "M2 scope".

Out of M1: roads, rivers, fences and bridges drawing; AI groups,
reinforcements and scripts; areas and script areas; random map template
dialogs; minimap tools; mission objectives; height editing. M2 builds the
first three of these (see "M2 scope"); the rest stays with M3.

## M2 scope

Phase 4 builds, for each of these, the MFC editor's editing at parity, on the
M1 chain (tool, `Editor`, bridge vtable, C ABI, session, snapshot and working
copy edited together):

- **Roads and rivers** drawn as control-point polylines, with width and
  opacity, sampled by `CVSOBuilder` and stored with the bridge's own `nID`.
- **Bridges** drawn as whole span groups, rotated, deleted, and toggled
  between intact and built during play; **entrenchments** (trenches) drawn as
  a whole; **fences** drawn as runs. A bridge span, a trench piece and a fence
  are no longer placed one by one from the palette (D-05, `WhyNotPlacedByPalette`),
  while objects of those types in a loaded map still load, draw and move.
- **Script IDs and reinforcement groups**, the **script file** beside the map
  and **script areas** (named, unique, in AI units).
- **Start commands**, **reserve positions** and the **AI general** (its sides
  and parcels).
- **Camera anchors**, one neutral and one per player.

Deleting an object that other records refer to cascades instead of refusing
(see "References to deleted objects"). Every collection is saved record by
record (see "Saving: the snapshot and the overlay").

**Built during play (correction of the parity table's wording).** The
05-PARITY row VO3 says "destroyed/intact". What the MFC editor does
(`RoadDrawState.cpp:1244-1272`) is toggle a bridge between intact (`fHP` 1)
and *built during play* (`fHP` -1 on every span), and only for
`WoodenBig_Heavy_*` bridges. M2 does exactly that; the row itself is edited in
04-13.

**Free camera rotation is closed for the map editor** (D-23, 2026-09-30). It
is not deferred to M3 and not waiting for a later milestone. The 03-06
measurement, on `coldwinter`, showed the lower half of the frame going from
0.5 % black at the game's own 45 degrees to 9.4 / 94.0 / 99.2 / 89.8 % black
at +30 / +90 / +180 / +270 degrees: `CTerrain::MovePatches`
(`Scene/TerrainInternal.cpp:237-256`) lays the terrain out on a fixed
isometric screen grid, and any other yaw clips it away. Rewriting the terrain
through the view matrix would not deliver what rotation was for, either:
every building, tree and infantry sprite and the tile art are pre-rendered
for one angle with the lighting baked in, so rotating the view cannot show the
back of a building. The need to see behind buildings is met instead by
markers drawn above the sprites in M2 and by M3's Units/Objects layer
toggles. `BkEditorSetYaw` and the engine tier's `TestYawMeasurement` stay in
the bridge as evidence, with no input bound to them. Reopening the question
needs a renderer phase for the terrain and new multi-angle art, not an editor
change.

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
  its menu screens. A data-only variant (storage and constants only: no
  object database, no renderer, no window) exists for the map file tests
  (`tools/zig/data_only_startup.cpp`; the object database is created after the
  renderer in the bridge, because `LoadDB` reads textures). Because the map
  file tier has no object database, geometry the bridge and the tests must
  agree on (bridge span plans, fence runs, trench pieces, area conversion) is
  plain-number C++ shared by the bridge and the map-file tier, taking span
  lengths, directions and index lists rather than `SBridgeRPGStats` and the
  like.
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
- Settings and recent files in the editor's own file
  (`<UserRoot>mapeditor/mapeditor.cfg`, via `Platform/Paths`; `BK_EDITOR_SETTINGS`
  is a test seam), independent of game profiles and never cloud-synced with
  game saves. **Changed from the spec, by decision during 03-07:** not the
  game's own profile folder.

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
library, `Sources/src/MapFile`, with a reader and a writer for both formats,
an equivalence comparator, and the snapshot overlay. It is its own library
rather than part of `Formats` because it calls `CMapInfo::IsValid`,
`PackFrameIndices` and `UpdateTerrainCrosses`, which live in `RandomMapGen` -
and `RandomMapGen` already includes `Formats/fmtMap.h`, so putting these files
in `Formats` would make it depend on the library that depends on it.
The reader and the writer are lifted from the two places that already do this
correctly:

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

**Frame indices and unknown types.** `CMapInfo::PackFrameIndices` and
`UnpackFrameIndices` (`RandomMapGen/MapInfo_StaticMethods.cpp:74-118`, loops at
606-638) look each object's type up with `pGDB->GetDesc( name )` and touch
`nFrameIndex` only for FENCE, ENTRENCHMENT and BRIDGE objects. For a name the
object database does not know, `GetDesc` returns null; the `NI_ASSERT_T` that
guards the next line compiles away in release (`Misc/ModernAssert.h:57-58`) and
the dereference crashes. Both functions iterate every object with no other
guard. So the editor never repacks a whole map: it packs only the objects it
added or edited, and only when their type is known. An unknown object's
`nFrameIndex` is written back exactly as it was read.

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

**M2: record by record (D-01).** Each M2 collection - the script file, camera
anchors, script areas, reinforcement groups, start commands, reserve
positions, the AI general's sides, roads, rivers, bridge entries, entrenchment
entries, and an object's script ID and HP - is laid over the snapshot the same
way:

- an untouched collection is written from the snapshot byte for byte;
- in an edited collection an untouched record stays byte-identical, an edited
  record is replaced in place, a new record is appended, a deleted record is
  removed, and nothing is renumbered or rebuilt;
- list order is kept (`startCommandsList` and `reservePositionsList` are
  `std::list`; `reinforcements.groups` is written sorted by group ID, whatever
  order the groups were put in).

The bridge keeps its own copy of every collection, in the snapshot and in the
working copy, as it does for terrain; the engine is never the source of the
saved data. The record-level operations are `NMapRecords` (`Sources/src/MapFile`),
which needs neither the engine nor the object database: the bridge applies
them to its copies and the map-file tier builds its expected maps with them.
An edit followed by its inverse writes the unedited file byte for byte, and
the tests prove it for every collection.

The MFC editor's save-time rewrites are **not copied**. There are five:

1. `HandOutLinks` link renumbering;
2. the silent `CheckMap` fixes;
3. the full shade recompute;
4. the `playersCameraAnchors[0]` overwrite;
5. the camera-anchor resize on open: `MakeCamera()` resizes
   `playersCameraAnchors` to `diplomacies.size() - 1`, filled with `VNULL3`, on
   every open (`TemplateEditorFrame1.cpp:3421-3435`), so an MFC save rewrites
   the vector's size. The editor never resizes it on open. "Set camera for
   player N" pads the vector with `VNULL3` up to N + 1 and never shrinks it
   (`VNULL3` is "not set": the game falls back to the neutral anchor), and
   undo restores the exact old size.

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

**Altitudes (amended 2026-10-01, phase 5 D-19).** The same rule covers
terrain altitudes (heights and shades), editable from M3: an altitude edit
is a deterministic function over the affected region — set the heights;
run `CMapInfo::UpdateTerrainShades` over the region grown by one vertex on
each side (the shade kernel, the MFC `DrawShadeState.cpp` update rect);
push the region into the engine — with the expected value in tests built by
the same function. Undo restores the recorded region raw. Outside edited
regions altitudes stay byte-identical. The MFC editor's whole-map shade
recompute at save is **not** copied: shades are recomputed per edit, and a
full recompute belongs to the explicit Update Map command (D-20).

**References to deleted objects.** Other records can refer to an object: by
link ID (`bridges`, entrenchment sections, `startCommandsList` units and
target, `reservePositionsList` artillery and truck, `link.nLinkWith` of objects
inside it) and by script ID (reinforcement groups, the AI general's
`mobileScriptIDs`). Link ID 0 means "no link" and is never a reference.

In M1, deleting an object that something refers to was **refused** with a
status bar message naming what refers to it. From M2 the delete **cascades**
(D-04, as amended by research correction C3), as one undo step that puts every
edited record back at its index:

- a reserve position is erased when its artillery **or** its truck is the
  deleted object (what the MFC editor does; a towed gun without its truck is a
  position the editor cannot create);
- a start command loses the deleted unit, and is erased when no unit is left;
  a start command whose target is the deleted object gets target link 0;
- script-ID references are left alone: reinforcement groups and
  `mobileScriptIDs` name script IDs, which other objects may share and Lua
  scripts may use. The status bar notes that a script ID is still referenced
  when the last object carrying it is deleted.

A refusal stays only for a bridge span (it belongs to its bridge; delete the
bridge), an entrenchment piece (delete the trench), and an object carrying a
passenger, whose `nLinkWith` points at the deleted vehicle. Deleting an object
that nothing refers to always works.

### Object palette pictures

**Changed from the spec, by decision on 2026-09-28 (03-09, D-29):** the
palette shows each object's own picture rather than a per-type symbol,
rendered by the engine on demand and cached per session. The shipped data
already carries one per object — `<szPath>\icon.tga`, the same file the MFC
editor's own palette decoded — for 1073 of 1167 placeable objects (92%);
Johannes chose to decode and cache those shipped pictures rather than build a
new off-screen render path. A single soldier with no icon of its own borrows
its squad's (built from every squad's own `<Members>` list, the
alphabetically first squad winning a tie), raising coverage to 1126 of 1167
(96%). The remaining 41 objects show a neutral frame with the object's own
name inside — the same frame every row shows first, unconditionally, before
its picture (if any) is decoded and uploaded, so a row is never blank while a
GPU upload catches up.

### Editor: sound list

The map's sound list is `CMapInfo::sounds.sounds` (`SSoundInfo::sounds`,
`std::vector<SMapSoundInfo>`, saved under tag 17, `"MapSounds"`) — **not**
`CMapInfo::soundsList` as an earlier draft of this spec and plan 6's own
objective for it named. **Changed from the spec, by decision during 03-10:**
`soundsList`'s element type (`CMapSoundInfo`, `Formats/fmtSound.h`) carries
only a name and a position; `CMapInfo::operator&` (both the binary and the
XML tree form) never serialises it, and nothing in this codebase populates it
from a loaded file — the MFC editor's own sound tab was already non-functional
here. Binding the editor's Sounds panel to `soundsList` would have made every
add, edit and delete a no-op the instant the map was saved and reopened.

The Sounds panel (under Players) lists the open map's sounds; a selected
row's fields (a filtered, known-sound combo; position; repeat and random
repeat in seconds; mute in combat; min/max radius) commit on deactivation as
one undo step. "Add at view centre" and "Delete" are the other two edits.
Positions are world (scene) units. Every sound is marked on the map (a small
diamond and its name), the selected one highlighted, regardless of the active
tool. Placing a sound from the object palette stays refused, as plan 5 built
— the Sounds panel is where sounds are edited.

A pre-existing binary-format bug surfaced while proving a save-and-reload
round trip: `SMapSoundInfo::operator&`'s binary form wrote `nMaxRadius` and
`bMuteDuringCombat` under the same tag (6); `bMuteDuringCombat` now has its
own tag (7). No shipped map has a non-empty sound list today, so the change
displaces nothing on disk.

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
- Hidden window: `PASS`, with the same driver, format, size and pixels as
  the visible run. Re-measured 2026-09-21 after the capture texture moved to
  `sdl.createCaptureTexture`.
- Direct3D and Vulkan are still unmeasured, because CI builds the spike on
  every platform and runs it on none. The device probe shows this is now
  fixable for Direct3D: both Windows jobs get a `direct3d12` device and can
  claim a window. Vulkan stays out of reach until the Linux jobs have a
  video device.

### Build and packaging

- New step `zig build install-map-editor` producing `MapEditor` next to the
  engine dylibs it loads, in the same layout as the game install.
- `zig build test` runs the core and map file tiers on every target. A
  separate step, `zig build test-map-editor-engine`, runs the engine tier.
- **Changed from the spec, by decision on 2026-09-28 (03-14):** packaged
  *beside* the game, not separately. `zig build package-game` and
  `package-game-editors` stage `MapEditor`(`.exe`) next to `Game`(`.exe`) via
  `stage.zig --map-editor` (the package's build-graph node depends on the
  exact build, not whatever happens to be on disk); no package ever carries a
  `mods/` folder. On Windows the packaged `MapEditor.exe` is a GUI-subsystem
  program — no console window on a normal double-click — while
  `--check`/`--smoke`/`--game-reads-it`/`BK_EDITOR_AUTO` still print, through
  `crt.attachParentConsole()` (joins the parent's console and reopens
  `CONOUT$` via `SetStdHandle`, since Zig's own stdio reads the process
  parameters block live rather than caching a handle `AttachConsole` alone
  would update). `map-editor-engine-test` (the CI test executable) stays
  console-subsystem.
- **Added during the hand try (2026-09-29, 03-15):** every staging and
  package carries `SeasonData/SeasonTextures.pak`, the winter and Africa unit
  textures the shipped Data lacks, generated from the summer ones by a cached
  build step (`tools/zig/season_textures.zig`) and never written into Data.
  The game and the editor mount it over the base Data, below any mod
  (`Sources/src/StreamIO/SeasonData.h`).

### User data: maps, settings and mods

- **New and saved user maps** default to `<UserRoot>maps` (created if
  missing); with a mod active, to `<UserRoot>mods/<Folder>/maps` — never the
  installed, read-only `<install>/mods/<Folder>/data`. A shipped map (from
  `Data`) is read-only: Save becomes Save As. A saved map with a mod active
  records the mod's name and version in `szMODName`/`szMODVersion`, left
  untouched with no mod active — the preservation invariant every other
  untouched field keeps.
- **Editor settings and recent files** live in `mapeditor.cfg` (see
  "Architecture" above) — a lenient `key=value` format (unknown keys ignored,
  malformed lines skipped, out-of-range numbers clamped, whole-line `#`
  comments only). It holds scroll/swipe speed, autosave on/off and interval
  (default 2 minutes, on by default, ticking only while there are unsaved
  changes), the default maps folder, and the last 10 opened maps (a missing
  file shown greyed with Remove).
- **Autosave** writes into the map file itself once it has a real path;
  before its first Save As, a never-saved map (new, or a shipped map not yet
  redirected) autosaves to a recovery copy in the user data area with a
  sidecar instead. The next start offers every recovery copy found back
  (Open / Discard / Later); a real Save, Save As, or a clean/Don't-save Quit
  deletes that document's own recovery copy, and so does a mod switch that
  closes it.
- **A mod** is chosen from File → Mod (`None` or an installed mod) or
  `-mod=Name`/`-mod=None` on the command line — mirrors `CICChangeMOD::Exec`
  without a main loop (closes the open map, swaps the mod's data storage,
  re-inspects it, clears the shared managers other than `IGFX`, reloads the
  object database) and never calls `IUserProfile::SetMOD`, since the editor
  has no game profile of its own. Switching asks first when there are
  unsaved changes (Save / Don't save / Cancel; Save on a new, shipped or
  read-only map goes through Save As, and cancelling that cancels the
  switch), then closes the map, switches the mod and reloads the object
  palette. The editor is left with no map open (title and status bar say
  so, the undo history is cleared, autosave is idle); the Open dialog then
  starts in the new mod's maps folder. Choosing the mod that is already
  active does nothing. **Revised 2026-09-29 in the hand try (Johannes's
  decision):** the switch used to reopen the current map under the new mod,
  which mixed object databases - a map saved under AchtungPanzer2, switched
  to None, showed 1359 unknown objects. A running test game is its own
  process on its own copy and keeps running.

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

**Test launch.** **Changed from the spec, by decision during 03-01/03-02:**
the game loads a map only from its own data storage, so "the profile's
cache" is the `MapEditorTest` profile's generated-data root, and the game
needs its own dedicated command-line switch. `BkEditorTestMapPath` writes a
copy of the current state into that root
(`NProfile::GeneratedDirectory` + `NGeneratedData::ModKey` for the editor's
active mod, lower-cased, then `maps`, then `mapeditor_test.bzm` — never into
`Data`), removing a stale same-stem `.xml` sibling first, since the game
loads the newer of a same-stem `.xml`/`.bzm` pair. The editor then spawns
`Game -editor-test -profile=MapEditorTest -mod=<Folder>|-mod=None -windowed
[-monitor<n>] mapeditor_test.bzm` as an argv array — never a shell string, so
a mod folder with a space stays one argument. `-editor-test` is the game's own
test-launch switch: a session-only profile (`active.cfg` is never read or
written, no legacy save/screenshot migration), every cloud-sync gate closed,
the first-visit help skipped, and windowed forced order-independently of any
later `-fullscreen` on the same command line — so a test launch can never
touch Johannes's own profile, saves, settings or cloud state. The editor
stays open, keeps drawing, and polls the game's lifetime without blocking a
frame; quitting the game returns to the editor with the map and its undo
history intact. Pressing Test again while one still runs offers Restart
(close and relaunch with the current state) or Keep (leave the running one
alone).

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
  of a bridge span, an entrenchment piece or an object carrying a passenger;
  any other delete cascades (see "References to deleted objects").
- **Save:** write to a temporary file beside the real one (the map's own
  extension, `<stem>.~save<ext>`), have `SaveSessionMap` read it back and
  compare it to what was meant to be written — `BK_EDITOR_FAILED` naming
  where they differ, or that the write did not read back at all — before
  swapping it over the real path, and keep one `<name><ext>.bak` per file per
  session (taken once, on the session's first write to a path that already
  exists, holding the version from when the map was opened, so an autosave
  can never overwrite the last good version). On failure the original stays
  untouched, the temporary file is removed, and the map stays dirty. A
  shipped map is read-only: Save becomes Save As, defaulting to the user maps
  folder. Close, Open and Quit ask first (Save / Don't save / Cancel) when
  there are unsaved changes; Save through that prompt goes through Save As
  when the map needs it.
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
- object and diplomacy edits apply the overlay rules of "Saving";
- every M2 edit is built with `NMapRecords` (and, from 04-06, `NMapGeometry`,
  the plain-number geometry unit) on the original map: the same functions the
  bridge applies to its own copies, so the expected value never depends on the
  engine;
- an altitude edit (from M3, D-19) runs the altitude function of "Terrain
  edits" — set the heights, `UpdateTerrainShades` over the region grown by one
  vertex per side — with the same C++ code the bridge uses.

The saved map must then be equivalent to that value. So, for terrain:
- outside the affected regions, every tile and every patch equals the
  original;
- inside them, `tiles` and patch crosses equal the builder's result;
- `altitudes` equal the original everywhere except inside altitude-edited
  regions, where they equal the altitude function's result, and an
  altitude edit followed by undo leaves them byte-identical to the original;
  patch heights, `rivers` and `roads3` equal the original everywhere.

Two further checks:
- **Idempotent save:** saving, reading and saving again gives byte-identical
  files.
- **Quick info:** the quick info chunk matches `SQuickLoadMapInfo::FillFromMapInfo`
  of the saved map.

### Test tiers and the M1 CI gate

| Tier | Needs | Runs on | Gate |
|---|---|---|---|
| **Core** (Zig) | nothing | all six CI targets | required |
| **Map file** (C++) | data-only startup | the five CI targets that build the engine C++ | required |
| **Engine** (C++) | hidden SDL window and a GPU device | macOS arm64 and Windows-MSVC in CI, plus macOS arm64 locally | required on the runners that have a device; the rest report "skipped: no GPU device", never a pass |
| **Game reads it** | the game and a window | macOS arm64 locally | required locally |
| **Editor app** | the editor and a window | macOS arm64 locally | required locally |

**Which runners have a GPU device.** Measured 2026-09-21 by
`zig build gpu-device-probe`, a non-gating step in every job:

| Runner | Result |
|---|---|
| `macos-14` (arm64) | `driver=metal`, window claimed, swapchain format 12 |
| `windows-latest` (MSVC) | `driver=direct3d12`, window claimed, swapchain format 12 |
| `windows-latest` (MinGW) | `driver=direct3d12`, window claimed - but MinGW cannot build the engine C++ |
| `macos-15-intel` | video yes, **no device**: "does not meet the hardware requirements for SDL_GPU Metal" |
| `ubuntu-24.04`, `ubuntu-24.04-arm` | **no video device at all** - Vulkan is never reached, so a Mesa package alone would not help; these would need a virtual display first |

So the engine tier runs in CI on macOS arm64 and Windows-MSVC, which is
more than "macOS arm64 locally" assumed, and the skip path is real rather
than theoretical: three of the six take it.

- **Core:**
  - Each command gets a do, undo, redo round trip.
  - Tools run under scripted input against the fake bridge: a brush drag
    paints the expected cells, drag-move moves, delete then undo restores the
    same object, player and link ID.
  - Diplomacy edits are checked the same way.
  - A refused delete leaves the document and history unchanged.
- **Map file:** Windows-MinGW is the one CI target this tier cannot run on.
  `Platform/LegacyVariant.h` includes MSVC's `comutil.h`, so `Formats`,
  `Misc` and `RandomMapGen` do not compile for `x86_64-windows-gnu` at all -
  which is why that job runs only the Zig and platform tiers today. The
  other five targets, including Windows-MSVC, run it.
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
  - **Changed from the spec, by decision during 03-12:** `BK_EDITOR_AUTO`
    ships as `"frame:action,frame:action,..."`, generalising the game's own
    `BK_AUTO_UI` and the fixed `--smoke` table into one scripting mechanism:
    `key=`, `press=`/`drag=`/`release=`/`click=` (a point is absolute pixels
    or `c<dx>x<dy>` from the screen's own centre), `wheel=`, `open=`, `save`,
    `saveas=`, `test`, `waitgame=<seconds>`, `shot=<name>` and
    `compare=<name>[@<percent>]`, and `exit`; a malformed token is rejected,
    naming the offending entry.
  - **Changed from the spec:** shot comparison is automated, not by hand —
    `compare=` runs a hand-rolled, uncompressed-32-bit-TGA reader and a
    pixel-tolerance comparison (no new image-diff dependency, matching the
    project's existing TGA-by-hand convention), seeding a local,
    never-committed reference on its first run and comparing against it
    thereafter.
  - `zig build map-editor-auto` runs the spec's whole editor-app scenario as
    one local command: paint, place, save as, shot, compare, test-launch,
    wait for the game's clean exit, quit — shots live in `zig-out/local-test`.
  - A fixed `--smoke` table (the same synthetic-event machinery
    `BK_EDITOR_AUTO` uses) opens a map, paints, places a unit, saves,
    test-launches and quits.
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
  and map file tiers pass there. **Met** — CI run 36464351662 (03-14): all
  six jobs green; re-confirmed by this plan's own CI run (03-15-SUMMARY.md)
  and locally with `zig build test -Dtarget=aarch64-macos -Dcopy-data=false`.
- The engine, game and editor app tiers pass locally on macOS arm64. **Met**
  — `zig build test-editor-bridge test-map-editor-engine map-editor-host-check
  map-editor-smoke map-editor-game-reads-it map-editor-auto
  -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`, 2026-09-29: 11
  PASS/done lines, rc=0 (03-15-SUMMARY.md).
- On macOS arm64 the editor opens a shipped map, paints tiles, places, moves,
  rotates and deletes units, edits diplomacy, undoes and redoes all of it,
  saves, and the game plays the saved map ("rotates ... units" is object
  rotation with Q/E, not camera rotation — see the M1 scope note on D-12
  above, deferred separately). Automated evidence: `map-editor-game-reads-it`,
  `map-editor-auto` and `map-editor-smoke` (03-12, 03-15). Human
  confirmation: **Met** — Johannes's hand try on the release build,
  approved 2026-09-29 after the gap fixes it found (03-15-SUMMARY.md).
- Opening and saving any shipped map unchanged (the full local sweep) gives an
  equivalent map, as defined under Testing. **Met** — `zig build
  test-map-files-all -Dtarget=aarch64-macos -Dcopy-data=false
  -Dtest-mode=run`, 2026-09-29: 1,755 of 1,755 maps round-tripped, 0 FAIL
  (03-15-SUMMARY.md).

## Exit criteria for M2

Phase 4's D-25. Every command below ran on 2026-09-30 on
`feat/map-editor-m2`, on macOS arm64 with `-Dtarget=aarch64-macos
-Dcopy-data=false -Dtest-mode=run`. The CI run ids are recorded in
04-13-SUMMARY.md, not here (as decided in 03-15). The row-by-row evidence is in
the phase's `04-PARITY.md`, copied into `05-PARITY.md`.

1. **Core tier.** Every M2 command has a do/undo/redo round trip against the
   fake bridge. The cascade delete restores every reference on undo. A refused
   edit changes neither the document nor the history. **Met.**
   - Command: `zig build test-editor-core`, 226 tests.
   - It runs on all six CI targets.
2. **Map-file tier.** For each M2 collection, overlay edits equal the
   expected-value builder's map. An untouched collection stays byte-identical.
   The cascade matches the builder. **Met.**
   - Command: `zig build test-map-files` prints `map-file: M2 record ops ok
     (87 cases)`, the other `M2 ... ok` lines and `map-file: PASS`.
   - It runs on the five CI targets that build the engine C++.
3. **Engine tier.** Every area is edited, saved, read back and compared with
   the expected map through the real `ITerrainEditor` / `IAIEditor`, and the
   engine state matches. That covers roads, rivers and their passability;
   bridges drawn, rotated, toggled and deleted; entrenchments; fences; script
   IDs and groups; start commands; reserve positions; AI parcels; script areas;
   camera anchors. **Met.**
   - Command: `zig build test-editor-bridge test-map-editor-engine`, which
     prints `editor-bridge: PASS` and `map-editor-engine: PASS (260 objects)`
     with every `M2 ... ok` line.
   - It passes on macOS arm64 and Windows-MSVC. The other CI jobs do not
     schedule it, so it never reports a pass it did not run.
4. **Preservation.** **Met.**
   - `zig build test-map-files-all`: 1,755 of 1,755 maps round-trip unchanged.
   - `zig build test-map-files-m2-sweep`: one record edit of each M2
     collection on every `Data/Maps` map, all undone, writes the unedited
     bytes. It prints `map-file: M2 sweep 59 maps, 460 edits, all restored
     byte-exact`.
   - `zig build test-editor-bridge-m2-sweep` does the same through the
     engine: bridges, fences and entrenchments drawn, shipped bridges and
     entrenchments deleted, cascades, all undone. It prints `editor-bridge:
     M2 sweep 57 maps, 238 edits, all restored byte-exact`.
5. **Game reads it.** One map is edited through the editor. It gets a new
   road, a river, two bridges (one rotated, one built during play), an
   entrenchment, fences, group 900 holding a unit with script ID 4245, a
   start command, a reserve position, a parcel on side 1, the area `m2_area`,
   player 0's anchor and `m2_script` beside it. The game loads it under
   `BK_AUTO_UI`, shoots and exits 0. **Met.**
   - Command: `zig build map-editor-game-reads-it-m2`.
   - `BK_MAP_TRACE` shows the camera at the anchor, the script loaded and
     `Init` run, the area found at its stored centre by Lua, and the group's
     unit landed.
   - The shot shows the road and the rotated bridge.
6. **Editor app.** `zig build map-editor-auto-m2` draws one of each, undoes
   and redoes, saves, and compares all 16 shots against local references,
   each under 0.03 %. It chooses the script beside the saved map and brings
   it along a Save As. Test in game then runs that script, and the test game's
   own trace says so. The scenario quits: `BK_EDITOR_AUTO: done (298
   actions)`. **Met.**
7. **CI.** All six jobs of "Cross-platform validation" are green on the
   branch. The engine tier runs and passes on macos-platform and
   windows-platform (`editor-bridge: PASS`, `map-editor-engine: PASS`). The
   other four jobs do not schedule it, so none of them reports a pass it did
   not run. **Met.**
8. **Parity.** Every M2 row of `05-PARITY.md` is closed with evidence: M2, M6,
   U1–U3, O16 references, O18 trench drawing, VO1–VO7, MT2, G1, AI1 and S4
   bridge spans. VO3 now reads "built during play". **Met.**
9. **Hand try, as amended.** Johannes ordered the phase to run without
   questions, so an agent-run walk-through replaced his hand try (recorded in
   04-01). **Met.**
   - `zig build map-editor-auto-m2 map-editor-game-reads-it-m2 --release=fast`
     passed on the release stage `zig-out/game/macos/arm64/release`.
   - The executor looked at every M2 shot and the game's shot; the table is
     in 04-13-SUMMARY.md.
   - On win-home (Windows-MSVC) the non-GUI tiers pass, MapEditor and the
     game build, and so does the release package build.
   - An agent cannot observe three things: trackpad feel, the OS opening a
     `.lua`, and the GPU look on Windows. They are listed in 04-13-SUMMARY.md
     with the automated evidence that covers each.

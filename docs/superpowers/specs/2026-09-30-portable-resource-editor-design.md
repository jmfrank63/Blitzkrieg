# Portable Resource Editor

A new Resource Editor, written in Zig with a Dear ImGui interface over a
portable C++ resource model, built by `zig build` for macOS arm64 and
Windows x64 (MSVC). It replaces the MFC "Blitzkrieg Resource Editor"
(`Sources/src/editor`, `editor.exe`). It is the fourth sub-project of
`docs/PLANNED_FEATURES.md` §1, "Editors on every platform", after the three
map editor milestones (`2026-09-19-portable-map-editor-design.md`).

GSD phase: `.planning/phases/06-resource-editor-portable-port` (decisions in
`06-CONTEXT.md`, the feature-by-feature checklist in `06-PARITY.md`).

## Why a port, and why this shape

The MFC editor is 64,028 lines (135 `.cpp`, 125 `.h`, `editor.rc` 1,128
lines). It is not built by `build.zig` or CI; packaging copies a checked-in
`Sources/src/bin/editor.exe` (10 MB) into `Editors/` for Windows packages
only. It passes an `HWND` to the engine, relies on the Stingray shim
`Common/LegacyUiCompat.h`, stores its settings in the Windows registry and
shells out to `zip.exe`.

Unlike the map editor, most of the resource editor's value is **not** UI: it
is the knowledge of how each resource is described (the tree items and their
property lists) and how a project is turned into game data (the export and
composition code: RPG stats, sprite packs, `.san` animations, DDS triplets,
icons, passability). That code is correct by definition — it produced the
shipped `Data/`. So the port **keeps the model and export code in C++**,
strips MFC out of it, and rebuilds only the UI.

## The MFC editor, in one table

20 active sub-editors plus one switched off. Each is a `C<X>Frame` (base
`CParentFrame`, some via `CGridFrame` or `CImageFrame`), a tree
(`C<X>TreeRootItem` + items registered in `TreeItemFactory.cpp`), a view
(`C<X>View`) and an optional toolbar. The complete feature list per
sub-editor is `06-PARITY.md`.

| Sub-editor | Project | Export folder | Game type written |
|---|---|---|---|
| Unit (vehicles; `MeshFrm`) | `.msh` | `units\technics\` | `SMechUnitRPGStats` |
| Infantry (`AnimationFrm`) | `.unt` | `units\humans\` | `SInfantryRPGStats` |
| Squad | `.scp` | `squads\` | `SSquadRPGStats` |
| Weapon | `.wpn` | `weapons\` | `SWeaponRPGStats` |
| Mine | `.mcp` | `objects\simpleobjects\common\summer\mine\` | `SMineRPGStats` |
| Particle | `.pcp` | `effects\particles\` | `SParticleSourceData` / `SSmokinParticleSourceData` |
| Sprite | `.spt` | `effects\sprites\` | `.san` + DDS |
| Effect | `.eff` | `effects\effects\` | `SEffectDesc` |
| Building | `.bld` | `buildings\` | `SBuildingRPGStats` |
| Object | `.obt` | `objects\` | `SObjectRPGStats` |
| Fence | `.fnc` | `fences\` | `SFenceRPGStats` |
| Bridge | `.bdg` | `bridges\` | `SBridgeRPGStats` |
| Trench | `.trc` | `units\technics\common\entrenchment\` | `SEntrenchmentRPGStats` |
| Mission | `.mip` | `scenarios\` | `SMissionStats` |
| Chapter | `.chc` | `scenarios\` | `SChapterStats` |
| Campaign | `.cgc` | `scenarios\campaigns\` | `SCampaignStats` |
| Medal | `.mdc` | `medals\` | `SMedalStats` |
| Terrain (tileset) | `.til` | `terrain\sets\` | `STilesetDesc`, crosset |
| 3D Road | `.3rd` | `terrain\sets\` | `SVectorStripeObjectDesc` |
| 3D River | `.3rv` | `terrain\sets\` | `SVectorStripeObjectDesc` |
| GUI (switched off in MFC) | `.gui` | `ui\` | UI screen XML |

Shared by every sub-editor (`CParentFrame`): New/Open/Close/Save/Save As,
Recent files, MOD settings, Export, Compress MOD to PAK, picture options
(brightness/contrast/gamma, `gamma.cfg`), background colour, show/hide
panels, expand/collapse, Set directories, Batch mode (dialog and command
line), Run game (F7), Help, insert/delete tree items, the reference pickers
(`RefDlg`, 20 lists) and the multi-select dialog.

The MFC editor has **no undo** (only the disabled GUI editor has one).

## Architecture

The map editor's three layers, plus one new C++ library, plus a shared Zig
kit.

```
ResourceEditor (Zig app, Sources/editor/resource_app)    MapEditor (Sources/editor/app)
        |                                                        |
        +------------- editor kit (Zig, Sources/editor/kit) -----+
        |                                                        |
resource core (Zig, Sources/editor/resource_core)        map core (Sources/editor/core)
        |                                                        |
        +------- EditorBridge (C ABI, Sources/src/EditorBridge) -+
                 bridge.h (map)      resource_bridge.h (resources)
                        |                     |
                        |          ResourceModel (C++, Sources/src/ResourceModel)
                        |                     |
                        +------- engine (Main, Scene, Anim, GFX/GFXGPU, Image, StreamIO) -------+
```

### 1. Editor kit (Zig, `Sources/editor/kit`)

Extracted, behaviour-unchanged, from the map editor. The map editor moves on
to it first and all of its tiers stay green.

- `history`: undo/redo stacks, gesture merging, `clean_depth` dirty flag,
  generic over the command type.
- `files`: the `Files` vtable (`StdFiles`, `FakeFiles`), `tempPathFor`,
  `backupPathFor`, `osPathFromEngine` — the safe-save plumbing.
- `settings`: the lenient `key=value` format and the recent list; the file
  name and keys come from the caller.
- `autosave`: schedule, write-in-place vs recovery copy.
- `shipped`: read-only detection of `Data/` and `mods/<N>/data/` paths.
- App side: `host` (SDL window, engine start, ImGui init), `imgui` backend,
  `crt` (Windows CRT/console), `auto` (the `BK_EDITOR_AUTO` parser, TGA
  reader, `compareTga`), the `AutoRunner`/`Driver` synthetic-event machinery,
  `pictures` (RGBA → SDL-GPU texture cache), file dialogs (`PathSlot`),
  `testlaunch` (spawning `Game` with the test profile).

### 2. Resource model (C++, `Sources/src/ResourceModel`) — new

The MFC editor's tree items and export code, with no MFC, no Windows API and
no UI. Built by `build.zig` on every target that builds the engine C++.

- **Values:** a portable `CVariant` replacement and `SProp` with the same
  domain types (`DT_DEC`, `DT_STR`, `DT_BOOL`, `DT_FLOAT`, `DT_COMBO`,
  `DT_BROWSE`, `DT_COLOR`, and the ~20 `DT_*_REF` reference types). `CString`
  → `std::string`; registry → settings passed in; message boxes → returned
  diagnostics.
- **Tree framework:** `CTreeItem` (props, children, `ClassTypeID` from
  `0x11000000`), the item factory, and every item class of every sub-editor,
  with their `operator&( IDataTree & )` serialisation unchanged.
- **Project files:** load and save the MFC project XML exactly
  (`<X>_Composer_Project`, `ClassTypeID`, `expand`, the `RPG`/`desc` stats
  copy, editor-only blocks such as `LockedTiles`/`Transparences`/
  `TransLines`/bridge `Begin/End/Front/Back`, the `History` list of at most
  100 entries).
- **Export:** each sub-editor's export moved out of its `*Frm.cpp`
  (`FillRPGStats`, `SaveRPGStats`, `SpriteCompose`, `BuildCompose`,
  `BuildAnimations`, `BuildSpritesPack`, `SaveCompressedTexture`,
  `SaveTexture8888`, `SaveIconFile`, `ComposeImageToTexture`,
  `MinimapCreation`, the map `.xml`→`.bzm` conversion) into
  `Export<X>( const CProject &, const SExportContext & ) → SExportReport`.
  Texture compression uses the portable `IImageProcessor` (`NDxt`) the game
  already builds.
- **References:** the reference lists (`RefDlg`'s 20 `EReferenceType`s,
  `LoadAIClassCombo`, `FillVectorOfSides`) built from the storage, not
  from Windows directory listings.
- **Batch export:** `ExportSingleFile` with the up-to-date check
  (`FindMaximalSourceTime` / `FindMinimalExportFileTime`) and the `-f`/`-os`
  flags.

The model runs in the **data-only startup** (storage, `consts.xml`, object
database, image processor; no window) so the resource-file test tier and
batch export need no GPU.

### 3. Resource bridge (C ABI, `Sources/src/EditorBridge/resource_bridge.h`)

Same library, same rules as the map bridge: `extern "C"`, plain structs with
fixed-size fields, integer handles, `BkEditorStatus` plus
`BkEditorLastMessage`, every entry point in `Guarded`. It shares the map
bridge's lifecycle (`BkEditorStart`/`Stop`, data-only start, `Mods`/`SetMod`,
`Paths`, `SetOverlay`, `CaptureFrame`, `GpuDevice`, `Resize`).

New groups, prefix `BkRes`:

- **Projects:** `New(kind)`, `Open(path)`, `Save(path)` (temp, read back,
  compare, rename — the map editor's safe save), `Close`, `Kind`, `Lock` /
  `LockOwner` (`locked_<user>`).
- **Tree:** `Nodes` (id, parent, class type, display name, expand, child
  count), `Props(node)` (id, names, domain type, value as text/int/float,
  combo strings), `SetProp`, `InsertNode(parent, class, index)`,
  `DeleteNode` (returns the serialised subtree for undo),
  `RestoreNode(serialised, parent, index)`, `MoveNode`.
- **References:** `RefList(type)` for every reference domain type.
- **Geometry edits** (the editor-only data): passability cells, locked /
  unlocked tiles, transparency, zero point, entrance, shoot / fire / smoke /
  directed-explosion points with angle and cone, one-way transparency lines,
  formation member positions, bridge span marks, mission objectives and
  chapter / campaign crosses, particle and effect keyframes. Each has *set*
  and *get* so undo is exact.
- **Export:** `Export(project, flags)`, `ExportStatsOnly`, `Batch(kind|all,
  src, dst, flags)`, each returning an `SExportReport` (written files,
  skipped as up to date, warnings such as a missing `gamma.cfg`).
- **MOD:** `ModSettings` get/set (`mod.xml`, seeding `modobjects.xml`),
  `PackMod` (the PAK written natively, see "MOD and PAK").
- **Preview:** see "Previews".
- **Import:** `ImportFromGame(path)` — a project built from an existing
  runtime resource (see "Import").

#### Where geometry is stored (D014 item 2)

Each geometry channel in the table is saved where MFC keeps it, so MFC
reads what the port writes; none is written as a private `_bk_geometry`
element. The
resource bridge's `HomeOf` holds this table in code. A get or set on a node
that has no home for the channel returns `BK_EDITOR_REFUSED`. Building,
object and bridge geometry sits in the project's frame chunks (`own_data`,
`desc`), next to the tree, because `CBuildingFrame`, `CObjectFrame` and
`CBridgeFrame` keep it in the frame and write it in `OnFileSave`
(`SaveFrameOwnData`, `SaveRPGStats`).
No tree item owns these chunks, so the bridge session acts as the frame:

- It keeps each edit.
- It reads a channel nobody has edited from the document as loaded.
- On save it writes only the edited channels, and leaves every other field
  of the chunk as it was read.

The other homes are fields of S03 items, and a save, delete or restore
carries them like any other field. Values are MFC's stored values with no
transform: `desc` positions relative to the zero point, `krest_pos` in view
units, angles in degrees. They are written with `%g`, so six significant
digits survive a save.

| Channel | Owner node | MFC home (writer) |
| --- | --- | --- |
| passability_cells | building / object root | `desc/passability`: `<item size_x size_y/>`, then one `<item>` of upper-case hex per row (`SObjectBaseRPGStats::operator&`, `Do2DArrayData`, `CDataTreeXML::RawData`) |
| locked_tiles | fence segment (`CFencePropsItem`), bridge part (`CBridgePartsItem`) | the item's `LockedTiles` list, one `<item x y val/>` per set tile (`CFenceFrame` / `CBridgeFrame::SaveMyData`). Grid cell (x, y) is tile (x, y). MFC stores only set tiles, so a read grid ends at the furthest set tile. |
| transparency_lines | object root | `own_data/TransLines`, `<item><Point1/><Point2/></item>` per line (`CObjectFrame::SaveFrameOwnData`, `STransLine::operator&`). The ABI list is point pairs, and an odd count is refused. |
| zero_point | building / object root | `own_data/krest_pos` (`m_zeroPos`, CVec3; z kept) |
| zero_point | squad formation (`CSquadFormationPropsItem`) | the item's `ZeroPos` (`vZeroPos`; z kept) |
| entrance | building root | `desc/Entrances/item[0]/Position` (`SBuildingRPGStats::SEntrance`) |
| shoot_points | building root | `desc/FireSlots` items: `Position`, `Direction` = angle, `Angle` = cone (`SSlot`) |
| fire_points | building root | `desc/FirePoints` items: `Position`, `Direction` = angle, `VerticalAngle` = cone (`SFirePoint`) |
| smoke_points | building root | `desc/SmokePoints` items, the same shape as fire points |
| directed_explosion_points | building root | `desc/DirExplosions` items: `Position`, `Direction`, `VerticalAngle` = cone (`SDirectionExplosion`) |
| formation_positions | squad formation (`CSquadFormationPropsItem`) | the item's `units` list, `Pos` x and y of each `SUnit` (`SUnit::operator&`). A slot keeps its z and `Dir`; a new one gets `AddUnit`'s z = 0, `Dir` = 0. |
| bridge_span_marks | bridge root | `own_data` `Begin`, `End` (CVec3; z kept) and the `Front` / `Back` attributes (`CBridgeFrame::SaveFrameOwnData`). The ABI list is always three points: Begin, End, (Front, Back). |
| mission_objectives | mission Objectives node | each child's `Objective position X` / `Y` value (`CMissionObjectivePropsItem::Get/SetObjectivePosition`), and `RPG/Objectives/item[i]/PosOnMap` when the project has an RPG chunk (`SMissionStats`) |
| chapter_crosses | chapter Missions node, Place holders node | each child's `Mission position X` / `Y` or `Place holder position X` / `Y` value (`CChapterMissionPropsItem`, `CChapterPlacePropsItem`), and `RPG/Missions/item[i]/PosOnMap` or `RPG/PlaceHolders/item[i]/Position` (`SChapterStats`) |
| campaign_crosses | campaign Chapters node | each child's `Chapter position X` / `Y` value (`CCampaignChapterPropsItem`), and `RPG/AllChapters/item[i]/PosOnMap` (`SCampaignStats`) |
| particle_keyframes | any particle track (`CKeyFrameTreeItem`) | the item's `Key_frames`, `<item first second/>` per key (`CKeyFrameTreeItem::operator&`): x = time, y = value. z has no home and must be 0. |
| effect_keyframes | effect Animations, Meshes, Function Particles or Maya Particles node | each child's `X position`, `Y position`, `Z position` value (`CEffect*PropsItem::GetPosition`), DT_DEC whole numbers |

The following combinations have no MFC home and are refused:

- **locked_tiles on a building or object.** These frames keep `lockedTiles`
  only in memory and save it as `desc/passability`, so use
  passability_cells.
- **passability_cells on a fence or bridge.** There is no grid; their tiles
  are lists.
- **The zero point anywhere else.** Only the building and object frames and
  the squad formation item save one.
- **Entrance and aimed points outside a building.** Only
  `SBuildingRPGStats` has them.
- **Formation positions anywhere but a formation**, and **bridge span marks
  anywhere but the bridge root.** The spans nodes hold no anchor:
  `vBeginPos` and `vEndPos` are the frame's, and the centre cross
  `vCenterKrest` is a constant in `BridgeFrm.cpp`, so it is not stored.
- **Crosses and effect places on a node that is not one of the listed
  containers**, including the children themselves: one container carries
  the list of its children.
- **Particle keyframes on a node that is not a track.**
- **Any of these channels on the other kinds**, for example weapon or
  sprite.

Two channel names do not match what MFC stores, and the rows above say what
they carry:

- **effect_keyframes.** An MFC effect has no keyframes. Its only stored
  per-part geometry is the place of each sprite, mesh or particle part, a
  3D vector, so that is what the channel carries.
- **The z of particle_keyframes.** A key is a (time, value) pair, so there
  is nothing to store z in; a non-zero z is refused rather than dropped.

Limits, recorded because MFC's reader imposes them:

- **One entrance per building.** The ABI carries one point per node, so it
  addresses `Entrances[0]`. Further entrances are kept as read.
- **Angles are whole degrees.** MFC stores floats, so a read rounds to the
  nearest degree.
- **List lengths must match the tree.** MFC's `LoadRPGStats` walks the
  `Slots`, `FirePoints`, `Smokes` and `DirExplosions` children and indexes
  the `desc` list by position. The tool must keep each aimed list as long
  as its props children, or MFC reads past the end.
- **A new entry has the struct's constructor defaults.** Weapon, picture
  and world positions keep those defaults until the building export (its
  `SaveRPGStats` port) fills them.
- **Other `desc` fields stay as read.** That includes `origin`,
  `visibility` and the Basic Info copy. A `desc` the port creates carries
  only the geometry fields. MFC's `LoadRPGStats` copies the Basic Info
  fields from `desc` into the tree, so the building and object export slice
  must write them too.
- **Bridge `UnLockedTiles` and fence / bridge `Transparences`** have no ABI
  channel yet. They round-trip as item fields.
- **A formation needs a slot per squad member.**
  `CSquadTreeRootItem::CallMeAfterSerialize` walks the members and steps
  through `units` for each one, so the tool must not make the list shorter
  than the member count.
- **One cross or place per child.** The list follows the container's
  children, so a set with another count is refused. Insert or delete the
  child to change the count.
- **The RPG copy of the crosses.** MFC's `LoadRPGStats` (`GetRPGStats`)
  copies `RPG` entry i over child i, so while a list is unedited the bridge
  reads it from `RPG`. Once it is edited, the bridge reads the children,
  and a save writes their positions into the existing `RPG` entries. Other
  `RPG` fields, and children with no entry, are left to the mission, chapter
  and campaign export slice (the `FillRPGStats` port). A project with no
  `RPG` chunk, such as the fixtures, gets none. MFC writes the edited value
  as a float, but `CreateDefaultChilds` turns it back into the default's int
  on reopen, so without `RPG` only the whole part survives, as in MFC.
- **Effect places are whole numbers.** The position values are DT_DEC, so
  a fraction is refused.
- **Frame copies MFC rebuilds from the tree.** The particle `KeyData` and
  the effect `effect` chunks are written by `SaveRPGStats` from the tree on
  every MFC save and never read back into the tree. The port leaves them as
  read; the export slices write them.

Proof (`test-resource-bridge`):

- A building and an object project with every channel set are saved and
  read back with the engine's own `CDataTreeXML` and `SBuildingRPGStats` /
  `SObjectRPGStats`, the way `LoadRPGStats` and `LoadFrameOwnData` read
  them. The squad and fence item homes are read back through
  `NResourceModel::Load` and the S03 items.
- The list channels are set on the scp, bdg, mip, chc, cgc, pcp and eff
  fixtures, on nodes below the root except for the bridge's own data. They
  are read back the same way: the bridge's `own_data` through
  `CDataTreeXML`, the items through `NResourceModel::Load`. The crosses are
  also run on a copy of the fixture that carries an RPG chunk written by
  the engine's `CDataTreeXML`, and read back through `SMissionStats`,
  `SChapterStats` and `SCampaignStats`.
- Every fixture is saved after every channel its nodes support has been
  set.
- Every saved file is checked to contain no private geometry element.
- Each project is reopened and its geometry compared.
- A re-save is byte-identical.
- Delete followed by restore leaves the saved file byte-identical.

### 4. Resource core (Zig, `Sources/editor/resource_core`)

No UI dependency; runs headless against a fake resource bridge.

- **Document:** per open project, a mirror of the tree and props returned by
  the bridge, plus the dirty flag, path, kind and lock state. One open
  project per sub-editor kind, as in MFC; switching sub-editors keeps each
  one's project, selection and history.
  (S05 T08: the bridge session holds one project at a time, so the app parks
  a project by its path when another sub-editor is chosen and reopens it on
  the way back; the unsaved-changes prompt guards the switch, a reopened
  project starts a fresh history, and an untitled project is closed.
  `resource_app/panels_logic.zig`, `Lifecycle.switchEditor`.)
- **Commands** (all undoable; undo/redo from the kit): `SetProp`,
  `InsertNode`, `DeleteNode` (subtree snapshot, restored at the same index),
  `MoveNode`, and one command per geometry edit, recording before/after
  state (never re-running an engine function on undo, as the map editor's
  terrain paint). A drag or a paint stroke is one gesture.
- **Tools:** passability brush, point placer/mover with angle and cone
  handles, zero-point and entrance setters, transparency-line tool,
  formation drag, cross placer on images, the keyframe curve model. Tools
  take plain input events and emit commands.

### 5. Resource app (Zig, `Sources/editor/resource_app`, executable `ResourceEditor`)

- One window, docking layout: project tree (left), property inspector
  (right), preview (the window background, like the map view), bottom dock
  for the thumbnail list and the keyframe curve editor, sub-editor toolbar
  under the menu, status bar.
- **Editors** menu and a combo switch between the sub-editors in MFC's
  order (editor.rc, POPUP "Editors"): 20 entries, because MFC's
  `CMainFrame::OnCreate` has `CreateGUIFrame` commented out, so the GUI frame
  has none; New and opening a `.gui` file still reach it. Opening a file
  switches by extension (`ActivateFrameByExtension`). The last active
  sub-editor is remembered (default: Infantry, as MFC's
  `LoadLastActiveModuleID`).
- **Property inspector:** one generic ImGui widget for every domain type,
  including reference pickers (searchable lists), browse (file dialog
  relative to the source root), colour, and the multi-select dialog.
- Settings in `<UserRoot>resourceeditor/resourceeditor.cfg` (test seam
  `BK_RESOURCE_EDITOR_SETTINGS`): source root, export root (the mod), game
  arguments, picture options default, background colour, recent files (the
  MFC editor kept 7; this keeps the kit's 10), autosave, last sub-editor,
  panel layout.

## Previews

The MFC editor drew every preview into one `CGameWnd` (800×600 windowed
`IGFX`). The portable editor draws the preview as the window background
through the same `IGFX::SetOverlay` path the map editor uses, ImGui on top.

- **Preview scene:** a new bridge path, `BkResPreviewBegin(kind)`, builds an
  empty `IScene` with a camera like the game's and no terrain (roads and
  rivers load their preview terrain, `maps\road3d` / `maps\river3d`, as
  MFC). `BkResPreviewShow(project)` exports the project **into a preview
  storage** (an editor temp folder mounted over the data storage, like
  MFC's `editor\temp\`) and builds the object through `IVisObjBuilder`,
  exactly as the game would. Edits re-run it, debounced. This makes the
  preview a test of the export.
- **Playback:** Run/Stop (F5) drives the game timer for infantry, sprite,
  effect, particle and river (MFC's `OnIdle` set); camera switch for
  particle/effect; the season / model switches (combat, install,
  transportable) for units.
- **Overlays:** grid, passability, points, cones, locators, bounding box,
  transparency lines, formation positions are drawn with ImGui draw lists
  from `BkEditorWorldToScreen`-style projections, not with the engine's
  `DrawRects` (the map editor's brush outline pattern). Picking runs in the
  core.
- **2D editors** (mission, chapter, campaign, medal) show their images as
  ImGui textures (`pictures`), with the cross / objective markers as
  overlays; no engine scene.
- **Picture options** (brightness, contrast, gamma) apply live to the
  preview and are written to `gamma.cfg` as MFC did.
- **Background colour** is a preview setting.

**Measured first.** The map editor found that anything but its own camera
breaks the terrain draw (the D-12 measurement). The preview scene is
therefore spiked in plan 06-01 on one mesh unit, one sprite object and one
particle source before any sub-editor depends on it, and measured from
captured frames (`BkEditorCaptureFrame`), not assumed.

## Import

MFC's "Import XML file" (Ctrl+I) has a menu entry and no handler. Most
shipped resources have no project file (only `WinSniper.unt`, 13 old
infantry `.unt` and one `current.msh` are in the repository; others live in
GOG mods). The port implements Import: it builds a project of the right
kind from a runtime resource folder (`1.xml` and its companions) through the
same `operator&` code, so every shipped resource can be opened, edited and
re-exported. Graphics-source fields stay empty unless the source files are
found; an export with no graphics sources writes the stats and leaves the
existing exported graphics untouched ("Export RPG stats only", which MFC
wired only for infantry, is offered for every sub-editor).

S04 T10 status: the bridge's `BkResImportFromGame` imports infantry (`unt`) through the engine's
`operator&` and a line-for-line port of `CAnimationFrame::GetRPGStats`. Sprite (`spt`) is refused:
its export only composes `.san` packs (`CSpriteTreeRootItem::ComposeAnimations`) and MFC has no
reverse path, so a sprite import would need a `.san` decoder that MFC never had. The other kinds
are refused, naming the kind, until their sub-editor slice ports theirs; likewise every exporter
registers itself (`NResourceModel::RegisterExporter`) in its sub-editor slice, and until then
`BkResExport` refuses the kind instead of writing partial game data. `BkResModSettings` carries
MFC's dialog fields (export dir, name, version, description), not invented bake knobs.

## Saving and exporting

- **Project save:** the map editor's safe save — temp file, read back and
  compare, rename; one `<name>.bak` per session holding the version from
  when the project was opened (replacing MFC's `backup.tmp`). Autosave and
  crash recovery as the map editor (D-19..D-22 of phase 3).
- **Locking:** MFC's `locked_<user>` file is kept: the editor writes its own
  lock and warns (read-only open or take over) when another user's lock is
  present.
- **Export:** into the export root (a mod folder, default
  `<BaseRoot>mods/<Mod>/data/` — MFC defaulted to `mods\mymod\`), same
  sub-folders and file names as MFC. All files of one export are written
  into a staging folder first and moved into place only when the whole
  export succeeded, so a failed export never leaves a half-written resource.
  The move into place is all or nothing too (D014): every live file it
  replaces is backed up first (`.bk-export-backup` beside `data/`), and if
  any move fails the moved files are taken back out, the backups restored,
  and the message names the failing file and says the export was rolled back.
  Export never writes into shipped `Data/`.
- **MOD and PAK:** MOD settings write `mod.xml` and seed `modobjects.xml`
  from `editor\modobjects.xml`. "Compress MOD to PAK" writes the archive with
  a zip writer in the build (no `zip.exe`), in the form `StreamIOZig/zip.zig`
  reads, and the storage mounts it in a test.
- **Run game (F7):** the map editor's test launch — the `Game` beside the
  editor, the editor test profile, windowed, `-mod=<export mod>`.

## Batch mode

`ResourceEditor --batch <ext|all> <sourceDir> <destDir> [-f] [-os]` runs with
the data-only startup, no window: `-f` exports even when up to date, `-os`
only opens and re-saves projects. `all` (new) runs every extension in the
MFC editor's frame order. Tools → Batch mode offers the same with a
progress panel, and lists failures and projects missing `gamma.cfg`.

## What "the game reads it unchanged" means

For every export, compared with the MFC editor's export of the same project
(the **golden**):

- **Stats XML:** read by the engine's own reader (`GameDB::ReadRPGStats`,
  `GetAddStats`, `GetGameStats`, `ParticleSourceData`, `fmtEffect`,
  `fmtTerrain`, `fmtVSO`) on both sides; every field equal (floats exactly).
  A comparator per stats type, written once in C++, that fails on any field
  it does not know.
  Implemented in `Sources/src/ResourceModel/comparator.*` (S03 T06), proved
  by `test-resource-model-comparator`, where every shipped stats file in `Data/`
  compares equal to itself. What that proof taught it, so a file node is not
  an unknown field:
  - each kind opens its own document element (`base`, `effect` for effects,
    as `CVisObjBuilder` opens them);
  - a container the reader enters and never indexes was skipped by the reader
    (`CTreeAccessor::Do2DArray` reads nothing of an empty 2D array);
  - a name repeated under one parent is read as often as the struct asks for
    it (`SBuildingRPGStats` writes `AmbientSound` twice);
  - a short, reviewed table (`kStaleFields`) of nodes older exporters wrote and
    no current reader reads: object `EffectExplosion`/`EffectDeath` and effect
    `sound` written as structs, particle `Position` and `GenerateSpinRand`,
    inline unit `Acks`, and two fields of one 2002 mission. Stale nodes are
    counted and reported, and a stale node on one side only is a difference.
  Golden comparison waits on the goldens: until `tools/zig/win-home/export-goldens.ps1`
  has filled `tools/zig/fixtures/resource_editor/<ext>/golden/` on win-home,
  the tier reports `pending: golden missing` for each extension, never PASS.
- **Binary animations (`.san`) and sprite packs:** byte-identical.
- **DDS:** same size, format, mip count; decoded pixels within a stated
  tolerance, because the checked-in `editor.exe` used a different DXT encoder
  than the portable `NDxt` (the tolerance is measured in 06-01, not
  guessed). `_h.dds` (ARGB8888) byte-identical.
- **Icons (`icon.tga`), copied files (`.mod`, `.txt`, `.lua`):**
  byte-identical.
- **Idempotent project save:** open, save, open, save gives byte-identical
  project files; an MFC-written project re-saved by the port is equivalent
  tree-for-tree.
- **Game reads it:** the game started with the exported mod loads the
  resource (units placed through a test map, UI through `BK_AUTO_UI`), shot
  and exited cleanly.

**Goldens** come from the checked-in MFC `editor.exe` run in batch mode on
the Windows machine (`win-home`) — the only place it runs — from:
the in-repo projects; GOG mod projects referenced by path (never copied or
committed); and repo-owned fixtures, one or more per extension, with small
generated source art. Only goldens of repo-owned inputs are committed
(`tools/zig/fixtures/resource_editor/`). The goldens stay after the MFC
editor is deleted.

## Test tiers and CI

| Tier | Needs | Runs on | Gate |
|---|---|---|---|
| **Kit + core** (Zig) | nothing | all six CI targets | required |
| **Resource file** (C++) | data-only startup | the five targets that build the engine C++ | required |
| **Engine / preview** (C++) | hidden SDL window + GPU | macOS arm64 and Windows-MSVC in CI, macOS locally | required where a device exists, "skipped: no GPU device" elsewhere |
| **Game reads it** | the game and a window | macOS arm64 locally | required locally |
| **Editor app** | the editor and a window | macOS arm64 locally; `--check`/`--smoke` in CI on macOS and Windows | required |
| **Parity oracle** | MFC `editor.exe` | win-home, by hand, once per sub-editor plan | produces the goldens |

- Resource-file tier: every project fixture opens and re-saves
  idempotently; every repo-owned fixture exports equal to its golden; import
  of a sample of shipped resources per kind then stats-only export
  reproduces the shipped `1.xml` field-for-field; a local
  `test-resources-all` sweeps every shipped resource through import and
  stats-only export.
- Editor app tier: `BK_EDITOR_AUTO` gains `editor=<kind>`, `select=<node
  path>`, `set=<prop>=<value>`, `export`, `batch=` and `undo`/`redo`
  actions; `zig build resource-editor-auto` runs open, edit, undo, save,
  export, shot, compare, test-launch, quit.
- Build steps mirror the map editor's: `install-resource-editor`,
  `test-resource-core`, `test-resource-model`, `test-resource-bridge`,
  `resource-editor-host-check`, `resource-editor-smoke`,
  `resource-editor-auto`, `resource-editor-game-reads-it`,
  `test-resources-all`.
- Packaging: `stage.zig` gains `--resource-editor <bin>` like
  `--map-editor`; `ResourceEditor` is staged beside `Game` and `MapEditor`.

## Platforms

macOS arm64 and Windows x64 MSVC build and run the app (the map editor's
`map_editor_platform` gate). The model, bridge and core tiers build and run
wherever the engine C++ builds. Windows builds are GUI-subsystem with
`crt.attachParentConsole()` for the automated modes; CRT asserts go to
stderr in every host.

## Errors

Every bridge call returns a status and a message; export and batch return a
report. A failed save leaves the file and the dirty flag as they were; a
failed export leaves the export folder as it was. The editor never
asserts on bad data; a project that references an unknown class type keeps
the unknown node through a save (the map editor's preservation invariant).

## Risks

- **Model extraction:** the `*Frm.cpp` files mix UI and export (BuildFrm
  2,625 lines, BridgeFrm 2,592, MeshFrm 2,320). Guard: goldens per
  sub-editor, compared by the engine's own readers, before the UI is built.
- **Missing source art:** few projects and sources exist outside GOG. Guard:
  Import, repo-owned fixtures with generated art, GOG projects on win-home
  by path.
- **DXT differences** between the old encoder and `NDxt`: measured tolerance,
  `_h.dds` byte-identical.
- **Preview scene** without terrain may hit the same renderer limits as the
  map editor's yaw: spiked and measured first (06-01).
- **The GUI editor** was switched off in MFC and the game's UI XML has grown
  since (Lua screens, scaling). Guard: it is the last sub-editor plan and
  edits the current format, proven by the game loading an edited screen.
- **Size:** 21 sub-editors in ~16 plans; the shared parts (kit, model
  framework, bridge, core, app shell) land first so sub-editor plans are
  model + export + preview + tools only.

## Exit criteria

- `zig build install-resource-editor` builds on macOS arm64 and Windows
  MSVC; the kit/core tier passes on all six CI targets and the
  resource-file tier on the five engine targets.
- Every row of `06-PARITY.md` is marked done with its evidence (test name,
  golden, or shot), or "no behaviour in MFC" with the reason.
- Every repo-owned fixture exports equal to its MFC golden under "the game
  reads it unchanged"; every GOG project on win-home does too.
- `test-resources-all` passes: every shipped resource imports and its
  stats-only export equals the shipped stats.
- The game loads a mod exported by the editor (one resource per sub-editor
  kind) and plays it cleanly (`resource-editor-game-reads-it`).
- Every edit in every sub-editor undoes and redoes (core tier, one test per
  command).
- Johannes's hand try on the release build, macOS and Windows, approves.
- Then the MFC editor is deleted: `Sources/src/editor`,
  `Sources/src/bin/editor.exe`, `Sources/src/editor/bin/editor2.exe`, its
  `A7.sln` entry and its `stage.zig` `copyEditors` entry; packages ship
  `ResourceEditor` instead.

## Amendments (M001 planning, 2026-10-05)

These entries override the earlier spec where they conflict and extend it
otherwise. Each is dated and references the memories and decisions that
drove it, per `AGENTS.md` ("where this file or a milestone context changes
one, the newer instruction wins, and record the change in the spec").

- **Linux x64 is a first-class target** for `ResourceEditor` and the
  shared editor kit. This supersedes the earlier "Linux deferred" posture:
  the editor is built, tested, and shipped on Linux x64 alongside macOS
  arm64/x64 and Windows x64 (MSVC). The Map Editor's Linux solutions are
  reused as-is for the Resource Editor: executables that load engine
  modules link with `rdynamic` and a `$ORIGIN` rpath; worker threads keep
  the default 16 MiB stack (glibc carves static TLS out of the stack);
  hidden test windows use `SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE`;
  `ISFX` is stopped before engine modules unload; `Data` paths are
  resolved case-insensitively via the `DataFile` helper in
  `tools/zig/editor_bridge_test.cpp`. See MEM001, MEM002, D001.
- **User content lives under `<UserRoot>resourceeditor/`.** All user
  content the Resource Editor writes on behalf of the person using it -
  projects, settings, recent-files lists, window layout, per-tool
  preferences - is rooted at `<UserRoot>resourceeditor/`, resolved
  per-platform the same way the game resolves `<UserRoot>` (XDG on
  Linux, `Application Support` on macOS, `%LOCALAPPDATA%` on Windows,
  with the `BK_*_SETTINGS` seams for tests). Paths stored *inside*
  project files are relative wherever the project format allows, so a
  project saved on one machine opens on another without rewriting
  absolute paths. This extends D-10 (which pinned the location of
  `editor.ini`/`editor2.ini` equivalents) to **all** user content, not
  just settings. See D002.
- **Project file layout (D-07), S03 T05, 2026-10-05.** The port writes a
  project the way `CDataTreeXML` saves it through MSXML: `<?xml
  version="1.0"?>`, CRLF, the whole tree on one line with no whitespace
  between nodes, CRLF. Elements and attributes come in the order the item's
  `operator&` writes them. An empty string chunk is `<a></a>`, because
  `StringData` always appends a text node, even an empty one. An empty
  container or an element with only attributes is `<a/>`. The reader keeps
  that difference. A project opened and saved without edits is
  byte-identical to the MFC file; an edited one keeps the same layout. The
  proof is both MFC projects in `Data/Editor/TestProjects`
  (`WinSniper.unt`, `current.msh`): `roundtrip-bytes` in
  `test-resource-model-fidelity` and the MFC check in
  `test-resource-xml-roundtrip`. A new project is written in the same
  layout. The repo-owned fixtures under `tools/zig/fixtures/resource_editor/`
  were written tab-indented by the port so their diffs stay readable. The
  reader detects that layout (whitespace between elements) and the writer
  keeps it, so they re-save byte-identically too. MSXML ignores whitespace
  between elements, so MFC reads either layout. The cases below cannot be
  proved byte for byte. Each one is equal in content, and no tracked
  project contains any of them, so `roundtrip-bytes` is asserted for every
  tracked project with no exception list:
  - **Escaped characters.** No MFC project in the repo contains `&`, `<`,
    `>`, `"` or a control character, so MSXML's exact escaping is not
    measured. The port writes `&amp;`, `&lt;`, `&gt;` in text, the same
    plus `&quot;` in attributes, and tab, LF and CR in attributes as
    `&#9;`, `&#10;`, `&#13;`. An MFC file that writes them in another form
    reads back to the same values.
  - **Text beside child elements** is stored trimmed. No `IDataTree` chunk
    writes mixed content, so only a hand-edited file has it.
  - **Values written before `int64low`/`int64high`.** The TestProjects
    values have no int64 slots. They are kept exactly as read, stale slots
    included. A value the port creates carries the slots, as the current
    `DTHelper.h` writes it. Floats it writes use `%g` with the MSVC
    three-digit exponent (`MfcFloat`), as `DataChunk( double )` does.
  - **MFC reading a port-written project.** The port keeps MFC's element
    names, `ClassTypeID`, `expand` and the `item`/`childs`/`values` shape;
    `roundtrip-typed` checks this. Opening a port-written project in the
    MFC `editor.exe` runs only on `win-home` and is pending there (S03
    UAT).
- **Geometry is stored in MFC layout (D014 item 2), S05 T03, 2026-10-05.**
  The cells, point and aimed channels moved out of the private
  `_bk_geometry` elements into the MFC homes listed in "Where geometry is
  stored" (section 3). On a node without such a home the channel is now
  refused, where before any node was accepted. An older port file's
  `_bk_geometry` copy of these channels is dropped on read; no released
  build wrote one.
- **The list channels are stored in MFC layout too (D014 item 2), S05 T04,
  2026-10-05.** Formation positions, bridge span marks, the map crosses and
  the particle and effect keyframes moved to the homes in the same table,
  and the `_bk_geometry` reader and writer were removed. A file an earlier
  port build wrote with such elements keeps them as unknown fields of the
  item; no released build wrote one. The span marks are now the bridge
  root's three-point own data, not lists on the spans nodes. The crosses
  and effect places are one entry per child of their container, not free
  lists. A particle key's z must be 0.

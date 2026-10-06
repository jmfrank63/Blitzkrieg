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
| passability_cells | building / object root | (S10: the building holds it in the tile frame while edited, like the object, and the bridge crops it on save with the zero point's origin) `desc/passability`: `<item size_x size_y/>`, then one `<item>` of upper-case hex per row (`SObjectBaseRPGStats::operator&`, `Do2DArrayData`, `CDataTreeXML::RawData`) |
| locked_tiles | fence segment (`CFencePropsItem`), bridge part (`CBridgePartsItem`) | the item's `LockedTiles` list, one `<item x y val/>` per set tile (`CFenceFrame` / `CBridgeFrame::SaveMyData`). Grid cell (x, y) is tile (x, y). MFC stores only set tiles, so a read grid ends at the furthest set tile. |
| transparency_cells | object root, building root | the object's desc visibility grid without the one-way tiles (S09 T04, channel 17): a bytes grid of tile cells, 0..7, rewritten together with passability and the one-way tiles from the trans-lines (`RewriteObjectGrids`), cropped with the origin of the zero point the object ends with. A value above 7 is refused; the channel on another kind returns `BK_EDITOR_REFUSED` naming the channel and the kind. |
| fence_transparences | fence segment (`CFencePropsItem`) | the segment's `Transparences` list (S09 T04, channel 18): a bytes grid in tile coordinates, 0..7. Covered by the get/set/save/reopen round trip; the exported origin is not asserted against an expected value. |
| sprite_pos | object root, fence segment | the object's `own_data/sprite_pos`, a fence segment's `SpritePos` (S09 T04, channel 19), a Point2 in grid pixels; the Move tool drags it and Centre on tile sets it to a tile's centre (`GridProjection`). |
| transparency_lines | object root | `own_data/TransLines`, `<item><Point1/><Point2/></item>` per line (`CObjectFrame::SaveFrameOwnData`, `STransLine::operator&`). The ABI list is point pairs, and an odd count is refused. |
| zero_point | building / object root, bridge root | `own_data/krest_pos` (`m_zeroPos`, CVec3; z kept). S11: the bridge root homes it as frame data too (`CBridgeFrame` Set zero). |
| zero_point | squad formation (`CSquadFormationPropsItem`) | the item's `ZeroPos` (`vZeroPos`; z kept) |
| entrance | building root | `desc/Entrances/item[0]/Position` (`SBuildingRPGStats::SEntrance`) |
| shoot_points | building root | `desc/FireSlots` items: `Position`, `Direction` = angle, `Angle` = cone (`SSlot`) |
| fire_points | building root, bridge root | `desc/FirePoints` items: `Position`, `Direction` = angle, `VerticalAngle` = cone (`SFirePoint`). S11: a bridge keeps its points in its RPG chunk the same way. |
| smoke_points | building root, bridge root | `desc/SmokePoints` items, the same shape as fire points (S11: a bridge smoke point has no `FireEffect`) |
| directed_explosion_points | building root, bridge root | `desc/DirExplosions` items: `Position`, `Direction`, `VerticalAngle` = cone (`SDirectionExplosion`). S11: a bridge has the same five fixed points, and no shoot points. |
| formation_positions | squad formation (`CSquadFormationPropsItem`) | the item's `units` list, `Pos` x and y of each `SUnit` (`SUnit::operator&`). A slot keeps its z and `Dir`; a new one gets `AddUnit`'s z = 0, `Dir` = 0. |
| formation_direction | squad formation (`CSquadFormationPropsItem`) | the item's `FormationDir` attribute (`fFormationDir`, radians). Added in S06 T04 for SquadFrm's direction arrow: the ABI carries it as a Point2, x the angle, y unused (reads 0). A set writes only the angle; the arrow's tool also turns the slots about the zero point (`CalculateNewPositions`) through formation_positions in the same undo step. A slot's own `Dir` (the arrow in drag mode) has no channel yet. |
| bridge_span_marks | bridge root | `own_data` `Begin`, `End` (CVec3; z kept) and the `Front` / `Back` attributes (`CBridgeFrame::SaveFrameOwnData`). The ABI list is always three points: Begin, End, (Front, Back). Nothing stored reads back empty and an empty write is refused, so the undo of the first edit restores the frame's default marks (S11 T05). |
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
- **Formation positions or direction anywhere but a formation**, and **bridge span marks
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
  The scenario is split per editor (S13 T10), because one run passed 380 s and a foreground command
  is capped at 10 minutes: `resource-editor-auto-core`, `-wpn` (Weapon, Mine, Trench, Squad), `-unt`, `-spt`,
  `-msh`, `-obt`, `-fnc`, `-bld`, `-bdg`, `-pcp`, `-eff`, plus `-til`, `-3rd`, `-3rv`. Each has its
  own scratch folder and runs alone; `resource-editor-auto` chains them in order. A concatenation check in
  build.zig keeps the per-editor schedules equal to the old whole.
- S13 additions. Road (.3rd) and River (.3rv) previews load `maps\road3d` / `maps\river3d` as the scene terrain and
  draw the exported description read back; `BkResPreviewWireframe` toggles wireframe; the river animates on Run.
  Tileset ABI: `BkResTileSetImport` (Import terrains / crossets), `BkResTileSetAddTile` (thumbnail double-click);
  the tileset has no game preview (GameWnd hidden in MFC) and its preview is the thumbnail lists, crosset mode
  following the tree selection. `.3rd` / `.3rv` import is a single file (a shipped Roads3D / Rivers xml); `.til`
  import from game data is refused with the reason. T01-T03: `BkResGetParticleInfo`; particle source mode
  (`BkResParticleSourceMode` / `BkResParticleSetSourceMode`) with one `IsComplexSource` derivation shared with the
  exporter; effect direction is view state (`BkResEffectSetDirection`) and child positions are whole numbers.
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
- **Settings, the File menu's lifecycle, autosave and recovery, S05 T09,
  2026-10-05.** `resourceeditor.cfg` is at
  `<UserRoot>resourceeditor/resourceeditor.cfg`; the environment variable
  `BK_RESOURCE_EDITOR_SETTINGS` names another file instead. It is the test
  seam, like the map editor's `BK_EDITOR_SETTINGS`, and only the interactive
  mode reads it. ImGui's `layout.ini` is in the same folder. A `--hidden`
  run reads and writes none of these, and no recovery copy either. The file
  shares the kit's editor-neutral keys (`scroll_speed`, `autosave`,
  `autosave_minutes`, `game_parameters`, `hidden_panels`, `recent`). The kit
  now exposes them as `applySharedKey`, `writeSharedHeadKeys` and
  `writeSharedTailKeys`, and builds its map-editor functions from them, so
  `mapeditor.cfg` keeps its byte order (a kit test asserts the bytes). It
  adds `last_editor` (the Editors menu's last choice, MFC's "Active Frame")
  and `projects_folder` (where Open and Save As start). It does not write the
  map-only `default_format` and `maps_folder`. Other settings named in
  section 5 (source and export roots, picture options, background colour)
  come with the tasks that add their dialogs. Recovery copies are
  `<UserRoot>resourceeditor/recovery/<stem>.<ext>`, named by the kit's
  `recoveryName` with the kind's own extension, each with a `<copy>.txt`
  sidecar (original path, Unix time). They are offered back at start-up
  (Open, Discard, Later). A copy that is opened back stays on Save As until
  it is saved outside the recovery folder. Autosave writes into the
  project's own file when it has a writable one; otherwise (untitled,
  locked by another user, shipped data, a reopened recovery copy) it writes
  a recovery copy. The recovery copy is written with `BkResSave`, so the
  bridge's own notion of the project path moves to the copy. The app's path
  stays the source of truth for Save. Export (T11) must therefore refuse an
  untitled project by the app's path, not the bridge's. `BkResSave` copies the
  file it replaces to `.bak` on every save. The app keeps the phase 3 rule
  of one `.bak` per file per session: from the second save of a path on, it
  moves the `.bak` aside around the save and puts it back, and a file the
  session created keeps none. Save As into shipped data is refused, and Save
  on a shipped project becomes Save As. Save As adds the kind's extension
  when the chosen name lacks it. A recent entry that no longer opens is
  dropped from the list. The lock prompt offers Read-only or Take over
  (`BkResLockTakeOver`). The app passes the lock user name in the engine's
  order (`BK_RESOURCE_EDITOR_USER`, then the login).
- **Tree, inspector and pickers, S05 T10, 2026-10-05.** Three bridge
  entries were added for the tree panel and the inspector: `BkResSetNodeName`
  (MFC's `ChangeItemName`: the displayed name only, written as the item's
  `display_name`; refused rather than cut when it would not fit the 64-byte
  record), `BkResSetNodeExpand` (the item's `expand` attribute, which MFC's
  `SaveTree` took from the tree control; `BkResNodes` now answers it instead
  of always 0) and `BkResPropStrings` (a property's `szStrings`: a combo's or
  bool's choices, a browse's source folder and filter, which the record only
  counted). `BkResRefList` type 5 (`E_ACTIONS_REF`) now answers
  `CMultySelDialog`'s list, the action types of `Data/Editor/actions.ini` with
  each action's id as the token, because MFC's reference dialog never filled
  that list and the multi-select dialog keyed its mask by the id. Every edit
  is a `ResourceCommand`; a rename is the new `rename_node` command. Expand
  is view state: kept and saved, but not an undo step and not an unsaved
  change, as in MFC. One gesture is one undo step (the frames of a colour
  drag merge; an edit box commits once when it loses the keyboard), and a
  value brought back to where its step began drops the step and, if that was
  the saved state, the unsaved mark. An inspector edit with several nodes
  selected writes every selected node of the primary's class that has the
  property, as one step; a delete of several nodes is one step. Insert offers
  the class a container already holds (MFC's per-container insert handlers);
  an empty container offers nothing until its sub-editor slice names the
  class. A tree drag moves a node only before an item of its own class or
  into a container that holds its class. Browse writes the chosen path
  relative to the property's source folder, else to the project's folder,
  lower case with backslashes, as MFC's sub-editors stored them.
- **Docks, preview background, Import, Help and About, S05 T12, 2026-10-05.**
  The preview scene is the main window's background: the app begins it
  (`BkResPreviewBegin`) for the open project's kind every time the kind
  changes, stops it when no project is open and at exit, and leaves the
  bridge to say which kinds have one (a refusal is shown, not hidden). Run
  (F5, MFC's Run button) is `BkResPreviewShow` plus the playback; until a
  sub-editor slice registers its exporter, Show refuses and the app shows the
  bridge's reason on a background-less line above the status line. The docks
  are floating ImGui windows placed around the screen's middle, never over
  it: the thumbnail list (View > Thumbnails, `CThumbList`), the direction
  button (Ctrl+D, `CDirectionButton`: angle, needle, degrees text and
  `GetQuadrant` ported as written) and the function window's frame (Ctrl+F;
  the keyframe editing is the Particle and Effect editors'). The thumbnail
  list reads `*.tga` of a chosen folder, else the project's folder, decoded
  by the engine's image decoders through `BkEditorMinimapImage` (which reads
  `<base>.tga` for a `<base>.xml` path) into the kit's picture cache. Its
  cells are 64 pixels, not MFC's 100, because the kit cache decodes at most
  64 on a side; the picture is fitted and centred on black as
  `LoadImageToImageList` did. Import from game data (Ctrl+I, MFC's
  `ID_IMPORT_XML_FILE` had no handler) is a window at the top of the File
  menu: a kind and the folder holding its `1.xml`; it is a guarded action
  (the unsaved-changes prompt asks first) and every kind goes to the bridge,
  so the refusal of a kind not ported yet names it. Help (F1) lists the
  shortcuts and links this spec, because `reshelp.chm` is not in the
  repository; About shows `IDD_ABOUTBOX`'s lines and the port's.
  `resource-editor-host-check` measures a second capture,
  `resource-editor-check-docks.tga`: the thumbnail of the tracked
  `spt/sprite-1frame.tga` has the picture's colour, and the screen's middle,
  where the preview scene is drawn, is the scene's frame in every sample.
- **Tools, Batch Mode, Run Blitzkrieg and the Editors menu, S05 T11,
  2026-10-05.** MOD Settings, Export Result and Compress current MOD to PAK
  sit in the File menu before Exit, Set Picture Options in Edit, and Set
  Directories, Export Stats Only, Batch Mode and Run Blitzkrieg in Tools,
  with MFC's accelerators (Ctrl+M, E, T, R, B and F7); each answers in one
  report window. Export is refused by the app's own project path, not the
  bridge's: an untitled project and an autosave recovery copy (whose write
  moved the bridge's path into the recovery folder) cannot export. The
  report lists written, skipped and the warnings, and a missing `gamma.cfg`.
  Picture options are app-side: `gamma.cfg` is written in the engine's tree
  layout (`<base Brightness=".." Contrast=".." Gamma=".."/>`), searched from
  the project's folder upward as `ReadConfigFile` did, and written beside the
  project ("current project only") or into the sub-editor's folder under the
  source folder (MFC's `szAddDir`); the dialog's before/after preview runs
  the engine's `CreateGammaCorrection` over a grey ramp. No exporter applies
  the values yet; each sub-editor slice that converts pictures reads them.
  Set Directories keeps MFC's three fields in `resourceeditor.cfg`:
  `source_folder`, `game_folder` (empty: the Game installed beside the
  editor, D-08) and the shared `game_parameters`. The export folder stays
  MOD Settings' own; it is not persisted, because re-applying it at start
  through `BkResModSettingsSet` would rewrite `mod.xml`: it starts as the
  bridge's default, `<BaseRoot>mods/<the active mod>/`, so `-mod=<Folder>`
  picks it. Batch Mode, the dialog and `ResourceEditor --batch <kind|*.ext|all>
  <src> <dst> [-f] [-os]` alike, lists the projects the way `BkResBatch` finds
  them, runs it, and reports the projects exported (or re-saved), each failed
  project with its reason (a project `BkResBatch` names in a warning, MFC's
  error list), other warnings and the projects with no `gamma.cfg` above
  them (export only). It runs in one frame, without MFC's live progress,
  because `BkResBatch` has no progress callback. The command line starts the
  engine on a hidden, never focused window, since `BkEditorStart` needs a
  window and a device: it is headless in that nothing is shown and no
  setting is read or written, but a host with no GPU device cannot run it
  (exit 3). Its exit is 0 with no failed project, 1 otherwise, 2 for a bad
  command line; unlike MFC an unknown flag is refused. `resource-editor-batch`
  runs `--batch-check` over copies of the 21 tracked fixtures. Run Blitzkrieg
  starts Game through `kit/testlaunch` with `-editor-test`, the profile
  `ResourceEditorTest` (testlaunch's `Options.profile`, which defaults to the
  Map Editor's) and no map (`Options.map_name = null`), plus `-mod=` with the
  export folder's name; an export folder outside `<BaseRoot>mods/` is refused,
  as the game could not load it by name. The Editors menu and a combo in the
  menu bar list the twenty entries; the active kind is remembered whichever
  way it became active (Editors, New, Open), the GUI frame excepted.
  SaveMapObjects is not ported: MFC's `OnSaveObjects` had no menu and no
  message-map entry, so it had no behaviour a user could reach.
- **Stats exporters for wpn, mcp, trc and scp, S06 T01, 2026-10-05.** The
  four exporters port SaveRPGStats line for line and write the stats with the
  engine's own `CTreeAccessor` (`ResourceModel/items/stats_export.h`). They
  need the engine's headers and StreamIO, so they are built into the
  EditorBridge archive and not into `resource_model_sources` (the
  engine-free model tests); `exporter.cpp` starts its table with them, since a
  static initialiser in an unreferenced archive member would be dropped by
  the linker. What MFC's frames took from the running editor comes through
  `SExportContext` (D015): `findUnitKey` (the squad member's key from
  `IObjectsDB`) and `meshFirePlaces` (a trench segment model's locators from
  `IVisObjBuilder`); the bridge fills both when the engine runs them, and an
  export that needs a missing one fails, naming the member or model. The
  stats file goes where MFC's Export put it: the project's
  `own_data/export_file_name` below the kind's folder, else that folder plus
  the project's folder name (weapons: `weapons\<folder>.xml`, as the shipped
  weapons are flat files; the others `<folder>\1.xml`), because the port has
  no source root to take MFC's relative path from. MFC's `<History>` node
  (user, dates) is not written: no reader reads it and the comparator skips
  it. The squad's zero point shift (`zeroShiftX/Y`, screen pixels) is turned
  into the world with the squad frame's camera in closed form, checked
  against the engine's view matrix in the comparator tier.
- **Unit (msh) locators and source art (D019), S08 T06, 2026-10-05.** The
  locators of a unit are derived data: the editor rebuilds the Locators
  children from the combat `.mod` as MFC did, and a bridge open and save
  writes them. That is an exception to the byte-identical rule at bridge
  level only; the exporter reads the `.mod` files through the engine's
  structure loader. The fixture's source art (models, six season textures,
  icon, name and description texts) is generated by
  `tools/zig/resource_editor_fixtures.zig`, so tests need no shipped art,
  and the msh golden line reads pending until a win-home run.

- **Grid frame projection and passability origin (D020), S09, 2026-10-06.** One C++ helper,
  `NResourceModel::GridProjection`, owns the grid-frame tile/world maths (MFC's origin -622/296,
  the `asin(1/sqrt5)` tile maths and the editor camera in closed form); the Zig overlay
  (`grid_logic.zig`) mirrors it and a test pins the two together. Object passability and
  transparency are held in the tile frame while edited and cropped on save with the origin of
  the zero point the object ends with, so moving the zero point never shifts a locked tile on
  the map. The grid tools (`grid_tools.zig`) are kind-agnostic: node and channel are
  parameters, so Building and Bridge (S10, S11) reuse them. The golden lines for obt and fnc
  read pending until a win-home run. Verified by `resource-editor-auto` shots whose overlay
  colours (0xff0000 locked, 0x606000 and 0x808000 transparency) are counted by code and which
  must disappear after undo.

- **Negative tiles (D021), S10, 2026-10-06.** Measured: all 590 shipped objects under `Data/Objects`
  import and read their passability and transparency cells with no tile left of or above tile
  (0, 0) (`NEGTILES objects checked=590 negative=0 import_failed=0`). The grid channels keep
  refusing such a tile and need no origin in the blob. `S09Object::NegativeTiles` in
  `resource_bridge_test.cpp` stays as the guard and fails if a shipped object ever needs one.
  Buildings (S10 T04): every shipped folder under `Data/Buildings` with a `1.xml` is imported and its
  tile-frame passability and transparency cells read (`S10Building::NegativeTiles`, line
  `NEGTILES buildings checked=194 folders=194 negative=0 unimportable=0`, `NEGTILES bridges checked=21 folders=21 negative=0 unimportable=0`. D023 (S11 T07): the earlier building scan had silently skipped 12 europe/summer folders (e_ctownhouse01_1, 01_2, 01_4, 02_1, 02_4, 03_2, 03_4, e_house07_1, e_house09_1, e_townhouse04_3, 04_4, e_townrailwaystation_4). The T06 reason, that their 1.xml holds no bld stats, was wrong: each has a full desc and an empty `<KeyName></KeyName>`, and the importer used an empty name as its "no stats" sentinel. The importer now labels the import with the folder name and keeps the stats' own empty name, so all 12 import, their grids are read (no negative tiles) and the building allow-list is empty. Named round trip: `ROUNDTRIP bld europe/summer/e_house07_1: 585 fields compared, 0 differences`.
  Bridges (S11 T02): every shipped folder under `Data/Bridges` is imported and each part's passability read
  (`S11Bridge::NegativeTiles`): `NEGTILES bridges checked=21 negative=0 failed=0 parts=226`, strict as D022. The same 21
  folders round-trip field-equal (`BRIDGES checked=21 folders=21 failed=0 fields=19083 differences=0`). A stats file keeps a
  slab's grid origin but not the span marks it was computed from, so the importer recovers the Begin, Center and End marks from
  the origins (own_data `Center` is new, read back by the exporter) and the shipped comparison allows 1e-3 on `Origin` fields,
  the float cancellation of two world positions of about 700.

- **Building point tools, S10 T02, T05 and T06.** The five point families (shoot, fire, smoke, directed explosion, entrance) are
  undoable commands in `resource_core/point_tools.zig`: place, move, delete, angle, cone, entrance and generate. A point and its tree
  child are one undo step, so the list channel and the tree never disagree: placing adds the child to the container and the point
  to the list, deleting removes both, and the directed explosions have five fixed children and never gain or lose one. The Building
  registers in `grid_logic.zig` with the grid tools plus these modes and a Generate points button (smoke and explosion modes). The
  `BK_EDITOR_AUTO` verbs `entrance`, `point`, `point_select`, `point_move`, `point_angle`, `point_cone` and `generate_points`
  go through the same editor calls as the mouse. The auto block counts overlay colours in captured shots (locked red, transparency
  0x606000, fire 0xff8000, direction handle 0xffff00) and requires them to vanish after undo. The repo bld fixture has no
  `DirExplosions` entries in its desc, so the generate step in the auto block is smoke; directed-explosion generation is proved by
  the `point_tools` tests. The GOG `brandenburgertor` golden is a named test that runs on win-home only.

- Squad overlay screen Y (D018, M001/S08): MFC projects the formation through
  `IScene::GetPos2`. The camera looks toward +Y (`Scene/Camera.cpp:10,76-79`,
  pitch -135 degrees) and the viewport matrix negates Y
  (`GFX/GraphicsEngine.cpp:895`), so world +Y runs up the screen. The port's
  `View.toScreen`/`toWorld` and the canvas origin in `panels.zig` now flip Y
  to match. The stored angle convention (D016) is unchanged; input and
  drawing share the View, so the drawn arrow still matches the stored angle.

- **Particle and Effect sub-editors (D026, M001/S12).** The bridge ABI gains `BkResGetKeyframeKnobs` (range, step and resize
  mode per key-frame node, as CKeyFrameEditor's constructor arguments) and `BkResPreviewCameraMode` (MFC's horizontal against
  default camera), mirrored in resource_core and the app's c_bridge. Effect import is refused: MFC's effect editor has no reverse
  path (EffectFrm.cpp has no GetRPGStats/LoadRPGStats), so the error names that. Curve quirks: key 0 is protected from delete; MFC's
  zoom handlers (OnKeyframeZoomin/outx/y) are commented out, so the port zooms the view only (pixels per step through
  {5,10,20,25,50,100}, not an undo step) and a curve that resizes to fit ignores Zoom X; each add, move, delete and Reset all is one
  geometry command. Interpolate Vector Items has no behaviour in MFC. `BK_EDITOR_AUTO` verbs `curve`, `keyframe`, `camera`,
  `import_file`, `import_refused`, `preview_refused` and predicates `keys`, `key`, `zoom`, `camera` drive and read them; the
  auto block measures Run, Stop and Camera shots by pixel difference. Not done in S12: Get particle info, the simple/complex
  toggle UI, and the effect direction arrow and angle/position editing. Goldens pending win-home.

- **Mission, Chapter, Campaign and Medal sub-editors (D030, M001/S14).** The exporters are ports of MissionFrm, ChapterFrm,
  CampaignFrm and MedalFrm, with `ComposeImageToTexture`/`GetImageSize` in `NImageExport`. Where the MFC Mission export wrote the
  converted map and the minimap into the installation, the port never writes into shipped Data: `BkResMissionMinimap` writes
  `map_{h,c,l}.dds` beside the open `.mip` (a project that lives in shipped Data is refused, naming the path), and the map `.xml`
  to `.bzm` conversion lands in the export root's `maps\<FinalMap>.bzm` (`SExportContext::convertMapToBzm`; a missing callback
  fails naming it). The exporter copies the `.bzm` only when no maps folder has it. The ported validation quirks are kept: the last
  failing message wins, and both music checks read the combat list, so the exploration message is the one shown. The image frame
  (`image_logic.zig`, the Image window in `docks.zig`) shows the picture at real size and refuses one of 2048 pixels or more on
  a side, whose positions would not be real pixels. One gesture is one undo step: a click that places, and a Show crosses drag from
  press to release, each commit one geometry command on release; Escape or a mode switch restores the position with no history
  entry. Property ids count from 1 (Chapter map image 4, Campaign 3, Medal 3), unlike the exporters' value indexes. `BK_EDITOR_AUTO`
  verbs `image_open`, `image_select`, `image_click`, `image_crosses`, `image_drag_cross`, the `{data}` path token and predicates
  `cross`, `shot_marker`, `shot_picture` drive and measure it with real pointer events; one `resource-editor-auto-<kind>` step per
  kind. Goldens and the GOG `INTEX2 ardennen40/current.mip` row stay pending win-home.

- D031 (S15, GUI screen editor): the open screen is the document. `CUiScreen` keeps the screen's text and edits only the touched
  attributes, so an unedited save is byte-identical; it opens a `<base>` root or MFC's `GUI_Composer_Project` root and refuses an
  unknown root, an unbalanced element or a missing `WindowPos`, naming the file, line and reason. The exporter writes the screen as a
  `<base>` file to `<mod>/data/ui/<Screen>.xml` and refuses shipped Data. The canvas outlines the windows at the engine's
  `CSimpleWindow::Reposition` geometry instead of rendering through the engine; the Game frame is the measured truth. Proof is local
  on Linux (this machine; the roadmap's "macOS local" becomes "Linux local"); macOS and Windows are proved by CI only.
  `resource-editor-auto-gui` runs a baseline Game frame, place, move, resize, align, undo, redo, save, reopen, export and a second
  Game frame, and requires the frames to differ at the moved label's old and new rects and the placed button, and nowhere else.
- D032 (S15 T01, Mission image frame): as `CMissionFrame` does, the frame always shows the project's `map_h.dds` when the Final map
  exists, and creates the file first when missing (`BkResMissionMinimap`; a refused minimap shows its message).
- D031 follow-up (S16 T01, 2026-10-06, unique ElementIDs on paste): MFC has no GUI copy and paste that renumbers, so the port
  defines it. The game tells controls apart by ElementID (`UI_NOTIFY_WINDOW_CLICKED` carries it), so a user's paste keeps a
  window's ElementID only while no window of the screen and no earlier window of the same paste has it; otherwise the window,
  nested ones included, gets the next free id above the old one (hex notation kept). ElementID -1 (the engine's "none") and an
  absent attribute never change. Ids the shipped screens already repeat stay as they are. `BkResGuiPaste` takes `unique_ids` and
  returns each change (window, old, new); the app says `paste: ElementID <old> -> <new> (<window name>)` in the canvas status
  line and on stderr, and `BK_DEBUG_LOG=1` traces it in the bridge. The undo of an earlier delete and the redo of a paste paste
  with `unique_ids` 0, so they restore the same bytes; undoing a paste deletes it and restores the screen byte for byte.

## Amendment (S16, 2026-10-06): early MFC deletion

Johannes approved deleting the MFC editor on 2026-10-06, before the
win-home hand try and before the MFC goldens exist, because the MFC editor
stays in git history and upstream. This overrides the gate in the section
above ("Then the MFC editor is deleted"). The deletion runs as S16/T07 after
T01-T06 and removes the same files. The MFC goldens remain pending: they are
generated on win-home from the commit just before the deletion commit, so the
golden comparison can still run later. T07 writes that commit's hash here:
`PRE_DELETE = <filled by S16/T07>`. The hand try (macOS, Windows) and the GOG
goldens also stay open. Tests and fixtures that do not need the MFC sources
stay, and the Linux build stays green. Decision D037.

## Amendment (S16 T08, 2026-10-06): the MFC golden comparison

`test-resource-model-comparator` compares the port's export of each fixture with the golden made by the shipped
`reseditor.exe` in batch mode (commit 734245b31). Two things changed in the comparison itself. The scratch copy of the fixture
gets `<own_data><export_file_name>1.xml`, as `export-goldens.ps1` does, so the port names its result folders as MFC did (the
medal's `medals\name`, not `medals\mdc\name`). `CompareGoldenFolder` takes the folder that holds the port's main stats file
as the port root, compares files by their path below it, and maps the main stats file (`wpn.xml` for a weapon) to the golden's
`1.xml`; layout alone is no difference. A difference is then a failure, unless it is listed with a reason: a stats path in
`kGoldenDifferences` (`Sources/src/ResourceModel/comparator.cpp`), a float the golden holds rounded to six digits, or a file rule
of the kind in `Goldens` (the test). Every explained difference is written with its reason to
`zig-out/local-test/resource_model/comparator/golden/<ext>/accepted.txt` and counted in `GOLDEN_SUMMARY`. A kind without a usable
golden is pending with its reason, and the reason is checked against the golden, so a regenerated golden is compared again.

One port fix came out of it: the mesh frame chooses the DDS format of every texture and of the icon by the picture
(`ChooseBestFormat`, `SpriteCompose.cpp:522`: DXT1 and ARGB0565 for an opaque picture, DXT5 and ARGB4444 for one with alpha). The
port wrote DXT5 and ARGB1555 always. `SaveCompressedTextureBestFormat` does what MFC does, and all 12 DDS of the msh golden
(`_c`, `_l`, `_h`, the icon) now match.

**Correction (hard steer 2026-10-06T06:23, D041, done in S16/T05).** Only differences proven from MFC source and the golden's bytes are accepted: the six-digit floats and the trc DXT5 solid-block encoding. The stats-reload class (wpn, trc, msh, obt, bld) is a fixture artefact: `export-goldens.ps1` now lets MFC open and save each scratch fixture (`-os`) before exporting, so MFC writes its own cached `<RPG>`/`<desc>`; these kinds are *pending regeneration* on win-home. The scene-camera values (fnc origins, bld explosion noise, obt grids and packs) are not proven and are *pending*, not accepted. Where the two bullets below call them accepted, read pending.

Verified reasons for the remaining differences (each from the MFC source and the golden's own bytes, none from a guess):

- **Stats reloaded from a missing element (wpn, trc, msh, obt, bld).** `CParentFrame::ExportSingleFile` calls `LoadRPGStats`, which
  reads the project's own `<RPG>` (`<desc>` for objects and buildings) into a default-constructed struct and writes the struct into
  the tree. MFC's own save writes that element (`CParentFrame::OnFileSave` calls `SaveRPGStats`); the fixtures do not hold it.
  The weapon golden's values are `SWeaponRPGStats`'s constructor defaults (RangeMax 30, AimingTime 1, DeltaAngle 0), not the
  fixture's (100, 100, 10). The mine and infantry kinds fill the struct from the tree first
  (`FillRPGStats` before `tree.Add`), which is why `mcp` and `unt` compare equal. The port exports the tree, as MFC does for a
  project it saved. Open: the port keeps a project's `<RPG>` unchanged on save (`project.cpp`), so MFC's batch export or its
  `LoadRPGStats` would read stale values from a project the port edited. Tracked in 06-PARITY.md; fixing it means writing the
  stats element on save for these kinds, and regenerating the goldens from fixtures that carry it.
- **Six digits (pcp, msh).** MFC's XML writer formats a double with `%lg` (`StreamIOLib/DataTreeXML.cpp:326`), so a float such as
  1.04719758 is stored as `1.0472`. The comparator accepts a float field whose golden value is the port's value printed that way
  and read back, and nothing else; the negative test `golden negative digits` pins both sides.
- **Scene camera (fnc origins, bld explosion positions, obt grids, origins and sprite packs).** MFC asks the live scene
  (`IScene::GetPos2`, `GetPos3`) for these. The port has no engine camera in this tier (`SExportContext::groundCamera` is never
  set) and uses `DefaultEditorCamera`. The fence golden's origins are not symmetric in x and y where the port's are, so the real
  camera differs from the default. Open: whether the bridge can hand the export the engine's camera. Until then the export of
  fences, objects and buildings carries this difference; the parity row is marked partial.
- **DXT5 solid blocks (trc `_c.dds`).** All blocks of the 16 x 16 picture are one colour, and the shipped editor writes them with
  both endpoints moved (colour 0xaf7e and 0xb79e, alpha ff and 00 for the source colour 0xb2f1f8), where NDxt writes one
  endpoint twice and `NLegacyDxt` (the MFC-era source) a third block. The decoded colour differs by 3 to 6. The gate of this kind
  is 6; the shared DXT5 gate stays 2.
- **Sprite (spt).** MFC's `1.xml` holds only the batch `<History>`; the sprite exports graphics only, and its golden has no graphics
  because the fixture's frames folder is empty.

Pending, with the reason the comparator logs:

- `bdg 3rd 3rv mip chc cgc`: the MFC editor crashed (0xC0000005) making the golden; regenerate on win-home from `PRE_DELETE`.
- `scp`: the golden holds only `<History>`. `CSquadFrame::SaveRPGStats` stops at `MakeName` when a member such as `USSR\Mosin` is
  not in the installed objects database.
- `til`: the golden holds only `<History>`; the tileset export needs `editor\terrain\tilemask.tga`, which the installed data lacks
  (the port refuses for the same reason).
- `eff`: MFC skipped the function particle `particle-2key` (no stream, an error box, continue), so the golden's particles are
  empty; the port stops with an error for a missing particle source.

| Kind | Files compared | Result |
|---|---|---|
| wpn | 1 | pending regeneration (struct defaults, fixture artefact) |
| mcp | 2 | pass |
| trc | 10 | accepted: 6 DXT5 solid blocks; 4 stats differences pending regeneration |
| scp | 0 | pending: golden holds only History |
| spt | 1 | accepted: History only |
| unt | 1 | pass |
| msh | 29 | pending regeneration: 121 missing-element differences; 12 DDS equal after the format fix |
| obt | 26 | pending: 16 regeneration, 24 camera and grid |
| fnc | 10 | pending: 18 camera |
| bld | 50 | pending: 4 regeneration, 20 camera |
| pcp | 1 | accepted: 2 six-digit floats |
| eff | 0 | pending: particle source missing in the golden's run |
| til | 0 | pending: golden holds only History |
| mdc | 6 | pass |
| bdg 3rd 3rv mip chc cgc | 0 | pending: the MFC editor crashed |

## Amendment (S16 T09, 2026-10-06): saving in MFC's form (D042)

`CParentFrame::SaveFrame` writes two elements beside the tree, before `<History>`, and `CParentFrame::LoadComposerFile` reads both
unguarded: `<own_data>` (`SaveFrameOwnData`: `export_dir`/`export_file_name` and the frame's own data, such as a bridge's
`Front`, `Back`, `Begin`, `End` or a building's `sprite_pos` and `krest_pos`) and the cached stats block `SaveRPGStats` writes.
A project without them crashes the shipped editor, which is why the port-written bdg, 3rd, 3rv, mip, chc and cgc fixtures
could not be opened by it. The port now writes both on every save of every kind:

- `BkResSave` and the batch open-and-save run the kind's exporter stats-only into a scratch folder and lift the block out of
  the stats file it wrote (`RPG`; `desc` for objects; `KeyData` for particles; `effect`; `VSODescription` for roads and rivers;
  none for buildings, fences, sprites, tile sets and screens). The block is refreshed from the tree each time, not copied from
  the loaded text. `NResourceModel::CachedBlockName`, `EnsureOwnData`, `PutCachedBlock` and `FindStatsBlock` in `project.h` do
  the XML work; the bridge C ABI is unchanged.
- The engine's stats writer prints floats with every digit and an empty string as `<a/>`. MFC's writer prints six significant
  digits (`%lg`) and an empty string as `<a></a>`. The block takes MFC's float form, and an empty element takes the form the
  block already in the project has at the same path. A path the project does not hold keeps `<a/>`; MFC reads both.
- The save runs with `SExportContext::bSaveCache`: MFC's checks belong to `ExportFrameData` (a mission without header texts is
  refused) and to the export (an effect particle without a source is skipped, `EffectFrm.cpp:215`), and `SaveFrame` runs none
  of them, so a save never fails or loses its block for them. A project whose exporter still cannot run keeps the block it
  had and the status line says why.
- Sources (images, a mesh's combat model) are found where the project was opened from, so a Save As refreshes the block as a
  save in place does. The cross lists of a mission, chapter and campaign are written into the block from the tree's
  children, as before: the exporter reads a position as an int and would lose its fraction.
- A project with no `<own_data>` gets one with the values an absent element has always read as: zero positions for buildings and
  objects (as the exporters read them), the frame's constructor value for a bridge's `Begin` and `End` (`BridgeFrm.cpp:91`),
  an empty `export_file_name`. A kind with no frame data (gui) gets none.

All 21 fixtures were re-saved through the port and now hold both. `export-goldens.ps1` and the comparator treat an empty
`export_file_name` as absent (they inject `1.xml`), so the goldens stay comparable.

**Round trip of the shipped editor's own projects.** `tools/zig/fixtures/resource_editor/mfc-new/` (one empty project per editor,
made by the shipped editor) is opened and saved unedited through the bridge by `test-resource-bridge`. Sixteen come back byte
for byte. Five differ in the cached block only, and the test checks that every byte outside the block is equal: MFC's
`SaveRPGStats` writes the frame's struct as it stood while `bNewProjectJustCreated` is set (`GetRPGStats`, not `FillRPGStats`;
`MissionFrm.cpp:140`), which is not the tree.

| File | In the block MFC wrote | The tree gives |
|---|---|---|
| minetest.mcp | `Weapon` = `generic` (the struct's default) | an empty `Weapon` |
| medaltest.mdc | uninitialised `ImageRect` floats (1.42724e-030 ...), no localisation keys | zeros and `medals\<name>\name`, `desc`, `medal` |
| missiontest.mip | uninitialised `MapImageRect` floats (4.37419e-030, a NaN), no scenario prefix | zeros and `scenarios\<folder>\` |
| infantrytest.unt | uninitialised `UninstallRotate` (1.4013e-045), `Commands` size 128, empty `AnimDescs` and `AcksRefs` | the filled values |
| particletest.pcp | uninitialised `Gravity` (2.8026e-045), `LifeTime` 2, one key per curve | the tree's own defaults |

No tolerance is applied to any other file or to any byte outside those blocks. The first edit of such a project in MFC rewrites
the block from the tree as well.

**Left to the maintainer.** The goldens of the D041 'pending regeneration' kinds (wpn, trc, msh, obt, bld) and of the kinds the
MFC editor crashed on (bdg, 3rd, 3rv, mip, chc, cgc) can be regenerated on win-home with `tools/zig/win-home/export-goldens.ps1`
from the re-saved fixtures; MFC can now open all of them. The full resource sweep is also left to the maintainer.

## Amendment (S16 T06, 2026-10-06): M001 closing entry

Linux-first-class and the user-data root were recorded in S01; this entry records what S16 added.

**`resource-editor-game-reads-it`.** One exported resource per project kind is shown to be read by the game. Ten kinds are
proved by the real Game (wpn, unt, msh, obt, fnc, bld, scp, mcp, bdg, gui): the Game runs with the new `BK_MOD_TRACE`, which the
Zig StreamIO overlay lookup (the Game's real data path) logs, and the log must show each exported file opened from the mod. The
other eleven are proved by the engine's own reader (`BkResReadBack`), each with a stated reason the Game cannot be driven to the
file (spt, pcp, eff, trc, til, 3rd, 3rv, mip, chc, cgc, mdc). `zig-out/local-test/resource-editor-game-reads-it/result.log` has
one `KIND` line per kind: the proof (`game` or `reader`), the file read and PASS or FAIL. 06-PARITY.md section D holds the table.
The step runs on the Windows and macOS GPU jobs of CI, in `run-resource-sweep.sh` and in AGENTS.md's tier list.

**Paste ElementID rule (T01).** A pasted window keeps its ElementID while no window of the screen has it; otherwise it gets the
next free id above the old one, and the app reports each change in the status line and the log (see D031 above). ElementID -1
and an absent attribute never change.

**A-13, A-16, A-17 (T02).** View menu toggles for Toolbar, Status Bar, Project Tree and Object Inspector, Set Background
Colour, and Expand/Collapse all (Ctrl+C), all kept in `resourceeditor.cfg`. Proved by `test-resource-app-logic` and
`resource-editor-auto-core`.

**`mfc-item-inventory.json` is the frozen source of truth.** Once `Sources/src/editor` is deleted, `tools/zig/mfc_item_inventory.py`
can only run against git history. `test-resource-model-fidelity` reads the JSON (`tools/zig/resource_model_test.cpp`, line 69),
so it keeps working.

**Deletion scope (D-26).** The scope is `Sources/src/editor/` (with `bin/editor2.exe`), `Sources/src/bin/editor.exe`, the `A7.sln`
entry and the `copyEditors` row in `tools/zig/stage.zig`. Johannes approved it (D037). It is held by D043 (S16/T07) and is
waiting for the golden comparison: the maintainer regenerates the MFC goldens on win-home from the re-saved fixtures
(44094bf22), and any difference is traced against `Sources/src/editor` first. It is not waiting on approval. `PRE_DELETE` in the
early-deletion amendment is still unfilled.

**Verified on Linux (T05).** Every tier ran in the foreground and exited 0: the three installs, `test-resources-all`,
the logic tiers, `test-editor-bridge`, `resource-editor-host-check`, `resource-editor-smoke`, the 19
`resource-editor-auto-<ext>` tiers, `resource-editor-game-reads-it`, `map-editor-smoke`, `map-editor-auto` and
`map-editor-game-reads-it-m3`. The golden comparator reports pass=3 accepted=2 fail=0 pending=15 (see the T08 amendment). No macOS or
Windows result has been seen.

**Open.**

- Johannes's hand try on macOS and Windows release builds.
- win-home: all 20 MFC goldens regenerated from the re-saved fixtures (44094bf22), then compared again; the GOG goldens
  B-09.14 (`INTEX2 brandenburgertor/current.bld`) and B-14.5 (`INTEX2 ardennen40/current.mip`).
- The MFC deletion (T07), held by D043.
- CI results of macOS and Windows after the maintainer pushes.
- The full resource sweep (`tools/zig/run-resource-sweep.sh`), left to the maintainer.
- Whether the bridge can hand the export the engine's camera (fence, object and building exports differ from MFC there; T08).

## Amendment (S16 T10, 2026-10-06): the goldens compared for real, fixtures fixed, the editor's camera reproduced

Hard steer 2026-10-06T08:50, D044. The goldens of commit 3e98ebc19 were made by the shipped editor from the re-saved fixtures
(17 of 20 kinds). Until then 13 of the 20 kinds were listed pending, most of them on rules that were stale (struct defaults, 'MFC
crashed'). This entry removes those rules and records, per kind, what the comparison says now.

**Result:** `GOLDEN_SUMMARY extensions=20 pass=4 accepted=7 fail=0 pending=9`. No stale pending rule remains: a pending reason is
checked against the golden it names and falls away when the golden stops showing it (a regenerated golden is compared at once).

| Kind | Result | Why |
|---|---|---|
| wpn, mcp, unt, mdc | pass | equal to MFC's export; the stats-reload rule for wpn is gone, MFC's own save wrote the cached block |
| msh | accepted | 29 files compared for real, the 38 stats differences are six-digit floats (`%lg`, `DataTreeXML.cpp:326`) |
| pcp | accepted | 2 six-digit floats |
| trc | accepted | stats equal, 6 DXT5 solid-colour blocks: the shipped editor's block is neither NDxt's nor NLegacyDxt's (proven from the golden's bytes, gate 6) |
| spt | accepted | a sprite's export is graphics only, MFC's 1.xml is the batch history |
| obt | accepted | 26 files; the `.san` and DDS packs are byte-equal and the grids equal; 4 origin floats are float noise (see camera) |
| fnc | accepted | 10 files; 15 origin floats are float noise (measured up to 5.5e-4) |
| bld | accepted | 50 files; 4 stats equal MFC's struct defaults because MFC's own save of a building writes no `desc` block (`BuildFrm.cpp:560` returns while no sprite is loaded, `mfc-new/buildingtest.bld` has none, so `LoadRPGStats` (`BuildFrm.cpp:922`) reads defaults: an empty KeyName, no rest or medical slots, MaxHP below 1 reset to 100 by `SHPObjectRPGStats::operator&`, `RPGStats.cpp:215`); 15 explosion positions are float noise (1.5e-5 against the port's 0) |
| scp | pending | the golden holds only History (see below) |
| bdg, 3rd, 3rv | pending | no golden: the editor crashed on the old fixtures; fixed (see below) |
| eff, til | pending | the golden shows the missing particle source and tileset mask; the script supplies them now |
| mip, chc, cgc | pending regeneration | the goldens were made from fixtures that are fixed now |

**The editor's camera (fnc origins, bld explosion noise, obt grids and packs).** The goldens were made with the editor's own scene,
and the port exported with `DefaultEditorCamera`, a camera of half the scale. `MfcEditorCamera( anchor )` (grid_projection.cpp)
is the ground part of that scene's `matTransform`, from the engine's code: `SetDefaultCamera` (distance 700, pitch -120 degrees, yaw
45 degrees), the orthographic projection `MainFrm.cpp:1714` builds from the 800 x 600 game window (`Specific.h`), one world unit per
pixel, and the anchor snapped as `CCamera::Update` snaps it; `GetPos2` maps a ground point to
`(400 + X.(p - a), 300 + Y.(p - a))` with X, Y the camera's axes. With the object frame's anchor of 16 world cells
(`ObjectFrm.cpp:221`) the translation is (-624, 300). Three facts prove it, none of them fitted: (1) the fence golden's origins differ
from the port's by exactly the screen offset (2, -4) a camera translated by (-624, 300) predicts, and agree with it to float rounding
(an anchor of 8 cells moves them by 362 and fails, which a negative test pins); (2) the object golden's origin (11.6562, -5.96064)
is reproduced from the fixture's hand-made origin (2, 1) by what every MFC batch export does, `LoadRPGStats` then `SaveRPGStats`: the
grid becomes tiles placed through the camera and the origin is cut anew from the corner of the tile that holds its first cell
(`SExportContext::bRequantiseGrids`, off for ordinary exports, where the desc is copied); (3) with that camera the object's
`.san` and DDS packs come out byte-equal to the golden's, which depend on the camera's scale and on the origin of the grid, so the
scale and the tile placement are right too. What stays is float rounding of `GetPos2` and `GetPos3`: the ray `GetPos3` casts
runs between the projection's near and far planes (-600, 1800), where a float step is up to 2.4e-4, so an origin (one such result
minus another) is exact to about 1e-3. The comparator accepts a difference of that kind only when the two floats are no further
apart than 1e-3 (`kEnginePositionNoise`), whatever else it says; the measured ones are 1.6e-4 to 5.5e-4 for origins and 1.5e-5 for
explosion points. A tile or a cell is 1 to 45 units apart, so a value off by more is a different value and fails.

**The chapter, campaign and mission block (found by the comparison).** MFC saves with the frame's prefix empty (`szPrefix` is set
only inside `ExportFrameData`), so the cached `RPG` block holds the tree's own paths. The port's refresh (T09) wrote the
export's prefixed paths (`scenarios\chc\header`); MFC's batch export loads the block into the tree (`LoadRPGStats`) and prefixes
again, so the golden says `scenarios\scenarios\chc\header` and a zero `MapImageRect`, because the picture was looked up under
the doubled name. `chc`, `cgc` and `mip` exporters write the tree's own paths into the block now (`bSaveCache`), and the three
fixtures are re-saved.

**Fixtures fixed.**

- `mip`: not a valid mission (no template or final map, no setting, an empty music file, an objective without a header), so
  MFC refused it at its export validation and the golden holds only History. It names a template map, a setting, one music and an
  objective header now. Two bridge tests that used the plain fixture as the refused one build the refused case from it.
- `bdg`: the crash on opening. The cached block listed no fire or smoke point while the tree has one of each, and
  `CBridgeFrame::GetRPGStats` (`BridgeFrm.cpp:1149`) indexes the block's lists by the tree's children (`NI_ASSERT` only). The bridge
  exporter gives each fire and smoke child its own entry now (at the origin when the block has none), and the fixture is re-saved
  with them. The span parts (three items per span: back girder, front girder, slab, in that order) and their pictures were
  already complete. `resource-editor-auto-bdg` counts the fixture's own point in its expectations.
- `3rd`: texture `terrain\sets\1\roads3d\road_asphalt_city`. `3rv`: texture `water\a_bottom`. The installed data lacks the old names.
- `eff`: its function particle `particle-2key` is in the editor's own data folder only, `<editor folder>\data\Effects\particles\`
  (`EffectFrm.cpp:215`), which the installed game does not have. The fixture's reference stays (the bridge tests build their cases on
  it); `export-goldens.ps1` puts the shipped `Data/Effects/Particles/aa_smoke1_of_expground.xml` there as `particle-2key.xml`, the file
  the port's tests give it, and removes it again. The port's comparison gives its export the same folder.
- `til`: the mask `editor\terrain\tilemask.tga` is read from `<editor folder>\data\` (`TileSetFrm.cpp:610`). The script puts the tracked
  `Data/Editor/Terrain/tilemask.tga` there and removes it again; the port's comparison does the same in its scratch folder.
- `scp`: not changed. Its member `USSR\Mosin` is a unit of the tracked `Data/objects.xml` (`USSR_Mosin`, sprite, unit, path
  `units\Humans\USSR\Mosin`), and `MakeName` compares the lower-cased path with the objects database's, so on win-home the editor's
  database does not have it. Another member would not help if the database is empty.

**For the maintainer: regenerate on win-home** (`powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1
-Extensions bdg,eff,til,3rd,3rv,mip,chc,cgc,scp`) from this commit. bdg, 3rd and 3rv crashed MFC: if one still does, the
editor's message or the crash offset says what the port's fixture still lacks. eff and til need the script's data files (it places
them). mip, chc and cgc are for the changed fixtures; their old goldens no longer match. scp: first look at the objects database the
editor loads (does it hold `USSR_Mosin` as a sprite unit? `Allies_Bren` is another unit of the tracked data to try as the member);
if scp still holds only History, the message box of the batch run names the member. wpn, mcp, trc, spt, unt, msh, obt, fnc, bld, pcp
and mdc need no new goldens.

**Not claimed.** No macOS or Windows result. The full resource sweep is left to the maintainer. T07 (the MFC deletion) still waits
for the regenerated goldens of the nine kinds above.

## Amendment (S16 T11, 2026-10-06): golden round 3, the five fixture problems

Commit `ae04873da` holds MFC goldens for 18 kinds made from the T10 fixtures. `test-resource-model-comparator` compares them for real:
pass=8 accepted=7 fail=0 pending=5 (wpn mcp unt mdc eff mip chc cgc pass or are accepted as before; bdg moved from pending to accepted).
The pending five (spt, scp, til, 3rd, 3rv) wait for goldens the maintainer regenerates from this commit; each carries its reason.

**bdg, now compared.** The bridge golden agrees with the editor camera anchored at 24 world cells, not 16 as the fence and object
goldens do. At 24 cells the camera's X component is exactly 1536 (`24 * 16 * 4`), the integer `CCamera::Update` cuts it to
(`Camera.cpp:94`), and MFC's quaternion-built axes land one ulp low, so its camera sits at 1535: one step, 0.7071 on each ground axis,
from the port's. The comparison gives the bridge the anchor one ulp below 24 cells; the remaining differences are the engine's
`GetPos2`/`GetPos3` round trip (at most 5e-4 on segment origins near 227, 1e-4 around 0 for the fire and smoke points), accepted under
the existing 1e-3 noise bound with their `BridgeFrm.cpp:1090`, `:1115`, `:1155` reasons. 105 differences, all accepted, none pending.
`MfcEditorCamera` now takes the component of its X axis in double, as the anchor's cut is on an integer boundary.

**eff.** MFC's batch export opens every kind's tree as `<base>`, an effect included, while its File menu export and the shipped effect
data use `<effect>`; the comparator reads an effect golden under the root it has when that `<base>` holds an `<effect>`. A weapon read
as an effect is still refused (negative test).

**The five fixture problems MFC showed on win-home:**

1. `spt`: `CSpriteFrame` joins the directory value `_.` (a prefix, not a folder) to the frame name (`SpriteTreeItem.cpp:89`, `:220`), so it
   wants `_.sprite-1frame.tga` beside the project. The fixture carries that file (and keeps `sprite-1frame.tga`, which the scenarios and
   the squad fixture use); the generator writes both. The port's export now composes the frame, and the bridge test expects files.
2. `art-16x16.tga` (til): MFC's "Some of files can not be opened" comes from the tile export (`TileTreeItem.cpp:232`, `:364`); the til
   golden's `crosset.xml` has no tiles, so the 24-bit art was not read. A 24-bit map image of chc, cgc and mdc is read, and 32-bit fence
   and building art is read, and the loader's code (`ImageTGA.cpp`) accepts both, so the cause is **not proven**: the tileset's art is a
   32-bit targa with 8 alpha bits now (generator kind `picture32`), the format MFC is known to read. The golden of til shows whether it
   was the cause. fnc's top-level art is unchanged (its golden passes).
3. `3rd` and `3rv`: `export-goldens.ps1` places the tracked `Data/Maps/road3d.xml`, `river3d.xml` and every tracked file of
   `Data/Terrain/sets/1` (tileset, crosset, roadset, noise, level files, minimap, the Roads3D and Rivers textures) in the editor's data
   folder, and removes the files again; the script's comment lists them.
4. `mip`: the cause of the crash on open-and-save could not be found from the open path (`MissionFrm.cpp` `LoadRPGStats`, `FillRPGStats`;
   the tree, the values and `GetImageSize` of `map.tga` match the working chc and cgc fixtures). The batch runner enumerates projects
   recursively (`CParentFrame::RunBatchExporter`), so it also opens `mip/final-map/project.mip`, a hand-seeded mission naming a final map
   whose minimap `LoadRPGStats` creates. That is the one difference left; the script's scratch copy leaves `final-map` out. **A
   hypothesis, to be confirmed by the next run.**
5. `scp`: the member is named by key, `USSR_Mosin`, which `MakeName` (`SquadFrm.cpp:198`) returns as it is, so the export needs no
   objects database; the path form `USSR\Mosin` is what the editor inserts by default and what the win-home database did not resolve.
   The GOG paks are not on this machine, so no unit could be checked against them. The comparator keeps the path form covered on a copy
   of the project (resolution, no database, unknown member).

**For the maintainer: regenerate on win-home** (`export-goldens.ps1 -Extensions spt,scp,til,3rd,3rv`, and mip if it crashed again) from
this commit. If 3rd or 3rv still crash, the crash offset says what is still missing; if til's crosset still has no tiles, the art was
not the cause. **Not claimed:** no macOS or Windows result; the full resource sweep is left to the maintainer. T07 still waits.

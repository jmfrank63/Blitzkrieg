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

### 4. Resource core (Zig, `Sources/editor/resource_core`)

No UI dependency; runs headless against a fake resource bridge.

- **Document:** per open project, a mirror of the tree and props returned by
  the bridge, plus the dirty flag, path, kind and lock state. One open
  project per sub-editor kind, as in MFC; switching sub-editors keeps each
  one's project, selection and history.
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
- **Editors** menu and a combo switch between the 21 sub-editors; opening a
  file switches by extension (`ActivateFrameByExtension`). The last active
  sub-editor is remembered.
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

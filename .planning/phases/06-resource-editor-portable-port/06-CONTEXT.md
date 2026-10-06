# Phase 6: Resource Editor: portable port of editor.exe - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Port the MFC "Blitzkrieg Resource Editor" (`Sources/src/editor`, `editor.exe`, 64,028 lines: 135 `.cpp`, 125 `.h`) to the portable editor stack on macOS arm64 and Windows x64 MSVC, as a new `ResourceEditor` executable built by `zig build`. All 21 sub-editors are in scope: the 20 active ones (Unit, Infantry, Squad, Weapon, Mine, Particle, Sprite, Effect, Building, Object, Fence, Bridge, Trench, Mission, Chapter, Campaign, Medal, Terrain, 3D Road, 3D River) and the GUI editor, which is switched off in MFC. The shared features are in scope too: projects, export, MOD settings, PAK, picture options, directories, batch mode, run game, reference pickers and the property inspector. New relative to MFC: undo/redo for every edit, safe save, autosave and recovery, and Import from game data.

The design spec is `docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md`. The feature-by-feature checklist, with the target plan for every feature, is `06-PARITY.md`. The phase ends when every parity row is done with evidence and the MFC editor has been deleted.

Out of scope: new resource kinds and new editing features beyond MFC parity, apart from the ones this phase adds on purpose (undo, safe save, recovery, Import, and "export stats only" for every kind). The MFC map editor belongs to phase 5 and ELK to phase 7.

</domain>

<decisions>
## Implementation Decisions

### Architecture and reuse
- **D-01:** A separate executable `ResourceEditor`, staged beside `Game` and `MapEditor`. It is not a mode of `MapEditor`.
- **D-02:** The generic parts of the map editor move into a shared Zig "editor kit" (`Sources/editor/kit`). That covers:
  - history/undo;
  - `Files` and the safe-save paths;
  - settings;
  - autosave and recovery;
  - shipped-path detection;
  - SDL host;
  - ImGui backend;
  - Windows CRT and console;
  - the `BK_EDITOR_AUTO` parser, driver and TGA compare;
  - the picture cache;
  - file dialogs;
  - test launch.

  `MapEditor` moves onto the kit first, with no behaviour change, and all of its tiers must stay green.

  The extraction is a move, never a copy. It is taken from the map editor as phase 5 leaves it, because phase 5 extends `settings.zig` and `history.zig`. ELK consumes this kit too (phase 7 D-03), so the kit also exports the TGA icon decoder and has no dependency on the engine bridge: ELK needs none of the bridge.
- **D-03:** The resource bridge lives in the same `EditorBridge` library, with a new header `resource_bridge.h` and the prefix `BkRes*`. It shares `BkEditorStart`/`Stop`, the data-only start, mods, paths, overlay, capture, `Guarded`, and the status-plus-message rules.
- **D-04:** The MFC tree items and export/compose code are ported to a new portable C++ library, `Sources/src/ResourceModel`. It has no MFC, no Windows API and no UI:
  - `CVariant`, `CString` and the registry are replaced with std types and settings passed in;
  - message boxes are replaced with returned diagnostics;
  - `operator&( IDataTree & )` serialisation stays unchanged.

  The model is the source of truth for the project format and the export. Zig never re-implements it.
- **D-05:** The Zig resource core (`Sources/editor/resource_core`) mirrors the tree and props from the bridge. It has one generic command set: `SetProp`, `InsertNode`, `DeleteNode` (a subtree snapshot restored at the same index), and `MoveNode`. It also has one command per geometry edit: passability, points, zero point, entrance, lines, formation, crosses and keyframes. Each geometry command records its before and after state.

  Every edit is undoable. MFC had no undo outside the GUI editor.
- **D-06:** One open project per sub-editor kind, as in MFC. Switching sub-editors keeps each project, its selection and its history. Opening a file switches the sub-editor by its extension. The last active sub-editor is remembered.

### Projects, export and data
- **D-07:** Read and write the MFC project XML unchanged for all 21 extensions. That includes `<X>_Composer_Project`, `ClassTypeID`, `expand`, the stats copy, the editor-only blocks and the `History` list of at most 100 entries. So MFC projects open in the port, and the port's projects open in MFC during the transition. Unknown nodes are kept through a save.
- **D-08:** Project save uses the map editor's safe save: temp file, read back, compare, rename. One `.bak` is kept per session, replacing MFC's `backup.tmp`. Autosave and crash recovery work as in the map editor (phase 3 D-19..D-22). MFC's `locked_<user>` file is kept: when another user's lock is present, the editor warns and offers to open read-only or take the lock over.
- **D-09:** Export writes into the export root (a mod folder, default `<BaseRoot>mods/<Mod>/data/`) with the same sub-folders and file names as MFC. Everything is written to a staging folder first and moved into place only when the whole export succeeded. Export never writes into shipped `Data/`.
- **D-10:** Settings live in `<UserRoot>resourceeditor/resourceeditor.cfg`, with the test seam `BK_RESOURCE_EDITOR_SETTINGS`. They hold the source root, export root, game arguments, picture defaults, background colour, recent files (the kit's 10), autosave, last sub-editor and layout. The editor does not read the Windows registry.
- **D-11:** "The game reads it unchanged" means the following, compared with the MFC export of the same project (the golden):
  - stats are read by the engine's own reader and are equal field by field, with floats compared exactly;
  - `.san` files and sprite packs are byte-identical;
  - DDS files have the same size, format and mip count, and their decoded pixels are within a tolerance measured in 06-01;
  - `_h.dds`, icons and copied files are byte-identical;
  - project saves are idempotent.

  The per-type stats comparator is written once in C++ and fails on any field it does not know.
- **D-12:** The goldens come from the checked-in MFC `editor.exe` in batch mode on win-home. The inputs are:
  - the in-repo projects;
  - GOG mod projects, referenced by path only, never copied or committed;
  - repo-owned fixtures, at least one per extension, with small generated source art.

  Only the goldens of repo-owned inputs are committed. They stay after the MFC editor is deleted.
- **D-13:** Implement MFC's dead "Import XML file" (Ctrl+I) as Import from game data: build a project of the right kind from a runtime resource folder, through the same `operator&` code. "Export RPG stats only", which MFC wired for Infantry only, is offered for every kind. When the graphics sources are absent, export writes the stats and leaves the existing exported graphics untouched.
- **D-14:** MOD settings (`mod.xml`, and seeding `modobjects.xml`) are ported. "Compress MOD to PAK" uses a native zip writer instead of `zip.exe`, and the storage must mount the result in a test. Run game (F7) reuses the map editor's test launch: `Game` beside the editor, the editor test profile, windowed, with `-mod=<export mod>` (phase 3 D-02/D-05/D-09).
- **D-15:** Batch mode keeps MFC's command line and adds an `all` mode:
  - command line: `ResourceEditor --batch <ext|all> <src> <dst> [-f] [-os]`;
  - `all` (new) runs every extension in the MFC frame order;
  - it uses the data-only startup, so it needs no window or GPU and can run in CI;
  - the Tools → Batch mode panel gives the same options, with progress, a list of failures and the missing-`gamma.cfg` report.

### Previews and editing views
- **D-16:** The preview is the window background, drawn through the map editor's `IGFX::SetOverlay` path, with ImGui docked on top. A new preview scene (`BkResPreview*`) has an empty `IScene`, a game-like camera and no terrain; Road and River load `maps\road3d` / `maps\river3d` as MFC did. The preview is built by exporting into a temp preview storage mounted over the data and building the object through `IVisObjBuilder`, so the preview also tests the export. Rebuilds after edits are debounced.
- **D-17:** Plan 06-01 spikes and measures the preview scene before any sub-editor depends on it. It uses one mesh unit, one sprite object and one particle source, and measures captured frames rather than assuming. This follows the lesson of phase 3's D-12 yaw measurement.
- **D-18:** Overlays are drawn with ImGui draw lists from world-to-screen projections, as the map editor's brush outline is, not with the engine's `DrawRects`. The overlays are grid, passability, points, cones, locators, bounding box, transparency lines, formation positions and crosses. Picking happens in the core.
- **D-19:** Playback: Run/Stop (F5) with the game timer for Infantry, Sprite, Effect, Particle and River. There is a camera switch for Particle and Effect, and model and season switches for Unit. Picture options (brightness, contrast, gamma) apply live and are written to `gamma.cfg`. The background colour is a preview setting.
- **D-20:** The 2D editors (Mission, Chapter, Campaign, Medal) show their images as ImGui textures from the kit's picture cache, with markers as overlays. They use no engine scene.
- **D-21:** The layout is:
  - project tree on the left, property inspector on the right, preview in the centre;
  - a bottom dock for the thumbnail list and the keyframe curve editor;
  - the sub-editor toolbar under the menu, and a status bar.

  One generic ImGui property inspector handles every `DT_*` domain type, including the ~20 reference pickers (searchable lists), browse, colour and multi-select. The Direction button, keyframe editor and thumbnail list are ImGui widgets.

### Scope, verification and delivery
- **D-22:** Every sub-editor is ported, including the GUI editor that MFC switched off. The GUI editor comes last (06-15). It edits the game's current UI screen XML, and it is proven when the game loads a screen the editor edited.
- **D-23:** Some MFC commands have no working handler: SaveMapObjects, Ack Import/Export, Effect "Interpolate Vector Items" and `ID_EDIT_CROSSETS`. The owning plan recovers each one's intent from the code. If the intent is clear, the command is implemented; if not, `06-PARITY.md` marks it "no behaviour in MFC" and gives the reason. `reshelp.chm` is not in the repository, so Help shows the shortcut list and links the spec.
- **D-24:** Platforms and CI tiers mirror the map editor:

  | Tier | Where it runs |
  |---|---|
  | Kit/core (Zig) | all six CI targets |
  | Resource file (C++, data-only; project round-trips, golden exports, import samples) | the five engine targets |
  | Engine/preview | macOS arm64 and Windows-MSVC GPU runners, with "skipped: no GPU device" elsewhere |
  | App `--check`/`--smoke` | CI on macOS and Windows |
  | Game-reads-it and `resource-editor-auto` | macOS locally |
  | Parity oracle | win-home |

  A local `test-resources-all` sweeps every shipped resource through import and stats-only export.
- **D-25:** Packaging: `stage.zig` gets `--resource-editor <bin>`, like `--map-editor`. `ResourceEditor` is staged beside `Game` on macOS and Windows.
- **D-26:** The MFC editor is deleted only in 06-16, and only after three things: every `06-PARITY.md` row is done with evidence; every golden and `test-resources-all` pass; and Johannes approves a hand try of the release build on macOS and Windows. The deletion covers `Sources/src/editor`, `Sources/src/bin/editor.exe`, `Sources/src/editor/bin/editor2.exe`, the `A7.sln` entry and the `stage.zig` `copyEditors` entry. The goldens stay. Johannes approved the deletion early on 2026-10-06 (S16 D037, released D050); see the spec's S16 amendment and `PRE_DELETE b8aa895bb00efb2fdac28096656dcf26984e3f87`.

### Plan split and waves
- **D-27:** The phase has 16 plans in 6 waves. The shared foundation comes first, so each sub-editor plan is only model, export, preview and tools.

  | Wave | Plan | Content |
  |---|---|---|
  | 1 | 06-01 | Parity oracle (goldens on win-home, DXT tolerance, fixtures), preview-scene spike, project-XML round-trip spike |
  | 2 | 06-02 | Editor kit extraction; MapEditor on the kit |
  | 2 | 06-03 | ResourceModel framework, every item class, project load/save, reference lists, resource-file tier in CI |
  | 3 | 06-04 | Resource bridge and resource core (document, commands, undo, fake bridge) |
  | 4 | 06-05 | `ResourceEditor` app shell with all shared features (parity section A), packaging, CI, `BK_EDITOR_AUTO` extensions |
  | 5 | 06-06 | Weapon, Mine, Trench, Squad |
  | 5 | 06-07 | Sprite, Infantry |
  | 5 | 06-08 | Unit (mesh) |
  | 5 | 06-09 | Object, Fence |
  | 5 | 06-10 | Building |
  | 5 | 06-11 | Bridge |
  | 5 | 06-12 | Particle, Effect |
  | 5 | 06-13 | Terrain, 3D Road, 3D River |
  | 5 | 06-14 | Mission, Chapter, Campaign, Medal |
  | 5 | 06-15 | GUI |
  | 6 | 06-16 | Full sweep, game-reads-it with one resource per kind, hand try, delete MFC |

  The wave-5 plans register their sub-editor through per-editor files, so they can run in any order or in parallel.
- **D-28:** Each sub-editor plan finishes only when every one of its `06-PARITY.md` rows is done. For each of its sub-editors it needs:
  - a golden comparison;
  - an import-and-export round trip of shipped samples;
  - core undo/redo tests for every tool;
  - a preview shot measured from the captured frame;
  - a `BK_EDITOR_AUTO` scenario.

### Exit criteria (testable)
- **D-29:** The phase is done when all of the following hold:
  - `zig build install-resource-editor` succeeds on macOS arm64 and Windows MSVC;
  - the kit/core tier passes on all six CI targets and the resource-file tier on the five engine targets;
  - the engine/preview tier passes on both GPU runners;
  - `test-resources-all` passes locally with 0 FAIL;
  - every repo-owned golden compares equal in CI, and every GOG golden compares equal on win-home;
  - `resource-editor-game-reads-it` passes: the game loads a mod exported by the editor with one resource per kind and exits cleanly;
  - `resource-editor-auto` passes: open, edit, undo, save, export, shot, compare, test-launch, quit;
  - every `06-PARITY.md` row is done;
  - Johannes's hand try is approved;
  - the MFC editor is gone from the tree and from packages.

### Claude's Discretion
- The exact bridge struct layouts and field sizes, and the exact names of the kit modules.
- How the debounced preview rebuild is scheduled.
- The DXT pixel tolerance value, which 06-01 measures.
- The fixture art generator, provided the fixtures are repo-owned and small.
- The ImGui layout details and toolbar icons.
- The recovery-copy naming for projects.
- Whether 06-06..06-15 run in parallel worktrees or one after another.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Sources/editor/core/history.zig`, `files.zig`, `settings.zig`, `autosave.zig`, `shipped.zig`, `fake_bridge.zig` (the pattern): these move to the kit (D-02).
- `Sources/editor/app/host.zig`, `crt.zig`, `auto.zig`, `smoke.zig` (`AutoRunner`/`Driver`), `pictures.zig`, `testlaunch.zig`, `panels.zig:1105` (SDL file dialogs, `PathSlot`), and `Sources/editor/imgui/*` (the `bk_imgui_backend_*` shim and `overlayCallback`).
- `Sources/src/EditorBridge/bridge.cpp`:
  - `BkEditorStart`: the `Paths::SetRoots` → `EnsureGlobalHooks` → `LoadAllModules` → storage → consts → `CreateObjectsDB` → renderer → `LoadDB` sequence;
  - `Guarded`;
  - `ListInstalledMods`;
  - the mod switch that clears the managers;
  - `BkEditorSetOverlay`;
  - `BkEditorCaptureFrame`;
  - `BkEditorObjectPicture` (the `icon.tga` → RGBA Lanczos scaling).
- `tools/zig/data_only_startup.cpp`: the no-window startup for the resource-file tier and for batch mode.
- `Sources/src/Image/ImageProcessor.cpp`: the portable `Compress` / `CompressDXTN` (`NDxt`), `SaveImageAsDDS` and `CreateGammaCorrection`, which the MFC export calls through `IImageProcessor`.
- `Sources/src/StreamIOZig/zip.zig`: the archive reader, which is the format reference for the PAK writer.
- `Sources/src/RandomMapGen/IB_Types.h`: `CSpritesPackBuilder`, used by the sprite-pack export.
- MFC sources to port into ResourceModel:
  - `TreeItem.*`, `TreeItemFactory.cpp`, every `*TreeItem.cpp`;
  - the export halves of the `*Frm.cpp` files (BuildFrm 2,625 lines, BridgeFrm 2,592, MeshFrm 2,320, ParentFrame 2,055);
  - `SpriteCompose.*`, `BuildCompose.*`, `MinimapCreation.*`, `common.*`, `Reference.cpp`, `UnitSide.cpp`, `localization.*`, `Track.cpp`, `ParticleSourceData.cpp`, `SmokinParticleSourceData.cpp`.
- Fixtures:
  - `Data/Editor/TestProjects/` has one folder per editor and `manifest.json`;
  - `WinSniper.unt` and the 13 `.unt` files in `Data/Old`;
  - `Data/Units/Technics/German/Artillery/8_cm_GrWr34/current.msh`;
  - the GOG `INTEX2` `.bld` and `.mip` on win-home, referenced by path only.

### Established Patterns
- The bridge is a flat C ABI with plain structs and fixed `char[64]` fields. Every call returns `BkEditorStatus` and a `BkEditorLastMessage`, and no C++ exception crosses the boundary. `NO_DEVICE` is kept separate from `BAD_ARGUMENT`.
- The core is std-only and runs headless against a fake bridge. SDL, ImGui and the C ABI live only in the app.
- Undo of engine-computed changes puts back the recorded before/after state and never re-runs the engine function.
- Safe save: `<stem>.~save<ext>` → read back and compare → rename. One `.bak` per session. On failure the original is untouched and the document stays dirty.
- Tests: three tiers plus local tiers, `gpu-device-probe`, "skipped: no GPU device" instead of a pass, and Windows CRT asserts routed to stderr in every host.
- `map_editor_platform` gates the app targets to macOS aarch64 and Windows MSVC. `stage.zig` takes an emitted binary through `addFileArg`.
- C++ rules: never `std::min`/`std::max` (use `Min`/`Max` from `Misc/Tools.h`); CRLF and the existing formatting conventions.
- `build.zig` rules: never `zig fmt` it; run `zig test tools/zig/build_hermeticity_test.zig` after editing it.
- Test artifacts go in `zig-out/local-test`. GOG files and the AchtungPanzer2 mod are never committed.

### Integration Points
- `build.zig` needs these new steps: `install-resource-editor`, `test-resource-core`, `test-resource-model`, `test-resource-bridge`, `resource-editor-host-check`, `resource-editor-smoke`, `resource-editor-auto`, `resource-editor-game-reads-it`, `test-resources-all`.
- `.github/workflows/cross-platform.yml` gets the resource tiers next to the map editor's steps in the `macos-platform` and `windows-platform` jobs. The kit/core tier runs in all six jobs.
- `tools/zig/stage.zig`: add `--resource-editor`, and in 06-16 remove the `editor.exe` from `copyEditors`.
- Engine readers that the goldens are compared through:
  - `Main/GameDB.cpp` (`ReadRPGStats`, `GetAddStats`, `GetGameStats`);
  - `Main/GameStats.h`;
  - `Scene/ParticleSourceData.cpp`, `Scene/SmokinParticleSourceData.cpp`, `Scene/VisObjBuilder.cpp`, `Scene/TerrainInternal.cpp`;
  - `Formats/fmtEffect.h`, `fmtTerrain.h`, `fmtVSO.cpp`;
  - `Anim/AnimationManager`.
- Canonical references for planners:
  - `docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md` (this phase's spec);
  - `.planning/phases/06-resource-editor-portable-port/06-PARITY.md`;
  - `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`;
  - `.planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md`.

</code_context>

<specifics>
## Specific Ideas

- Johannes: "make sure all editors are ported and all features are implemented". This is why the GUI editor is included (D-22), why dead commands are investigated rather than silently dropped (D-23), and why deletion is gated on `06-PARITY.md` (D-26).
- The previews should show what the game would show. The preview is built through the game's own `IVisObjBuilder` from a real export (D-16).
- Measure; don't guess (phase 3's D-12 lesson). That means the preview spike and the DXT tolerance are measured in 06-01.
- win-home: the GOG game on `D:\GOG\Blitzkrieg` is read-only and used for goldens only. Its projects are referenced by path and never copied.

</specifics>

<deferred>
## Deferred Ideas

- New resource kinds or editing features beyond MFC parity, such as a new unit-stats balancing view or a diff between mods. These belong in a later phase.
- Linux build of the app. The model and core tiers run there; the app waits until a Linux GPU runner path exists, as for the map editor.
- Batch export straight into a PAK (export + pack in one step). This is a possible later convenience; for now the two steps stay separate as in MFC.
- Removing `Common/LegacyUiCompat.h`. This happens when its last users are gone: ELK in phase 7, after the MFC map editor in phase 5 and this editor in 06-16.

</deferred>

---

*Phase: 06-resource-editor-portable-port*
*Context gathered: 2026-09-30*

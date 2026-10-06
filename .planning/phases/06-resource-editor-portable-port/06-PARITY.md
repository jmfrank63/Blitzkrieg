# Phase 6: Resource Editor parity checklist

Every sub-editor and every user-visible feature of the MFC resource editor
(`Sources/src/editor`, `editor.exe`), with the plan that ports it. The MFC
editor is deleted (06-16) only when every row is **done** with evidence, or
marked **no behaviour in MFC** with a reason.

> **Update 2026-10-06 (D037):** Johannes approved deleting the MFC editor at S16/T07 before the
> win-home hand try and MFC goldens, since it stays in git history and upstream. Rows marked
> "golden pending win-home" stay pending: the goldens are generated on win-home from the commit
> before the deletion, `PRE_DELETE = <filled by S16/T07>`, so the golden comparison can still run later.

Status values: `todo` → `done (<evidence>)`: a test name, a golden
comparison, or a shot in `zig-out/local-test`. The evidence column is filled
by the executing plan.

Sources for this inventory: `MainFrm.cpp`, `ParentFrame.cpp`, `frames.h`,
`TreeItemFactory.cpp`, every `*Frm.cpp` / `*TreeItem.cpp` / `*View.cpp`,
`editor.rc` (menus `IDR_EDITORTYPE`, `IDR_INSERT_TREE_ITEM_MENU`,
`IDR_DELETE_TREE_ITEM`, `IDR_INTERPOLATE_TREE_ITEM_MENU`,
`IDR_KEYFRAME_ZOOM_MENU`, `IDR_ACK_MENU`, `IDR_ALIGN_MENU`,
`IDR_TEMPLATE_MENU`; 14 toolbars; 11 dialogs; accelerators).

## Plan map

| Plan | Wave | Content |
|---|---|---|
| 06-01 | 1 | Parity oracle on win-home (MFC batch export → goldens), DXT tolerance measured, repo-owned fixture per extension, preview-scene spike (one mesh unit, one sprite object, one particle source), project-XML round-trip spike |
| 06-02 | 2 | Editor kit extracted from the map editor (`Sources/editor/kit`); MapEditor on it, all its tiers green |
| 06-03 | 2 | `Sources/src/ResourceModel`: portable variant/`SProp`/`CTreeItem`/factory, every item class of every sub-editor, project load/save, references, data-only resource-file tier in CI |
| 06-04 | 3 | Resource bridge (`resource_bridge.h`, `BkRes*`) + resource core (document, generic commands, undo, fake bridge) |
| 06-05 | 4 | `ResourceEditor` app shell: shared features below, packaging, CI, `BK_EDITOR_AUTO` extensions |
| 06-06 | 5 | Weapon, Mine, Trench, Squad |
| 06-07 | 5 | Sprite, Infantry |
| 06-08 | 5 | Unit (mesh) |
| 06-09 | 5 | Object, Fence (the `CGridFrame` tools) |
| 06-10 | 5 | Building |
| 06-11 | 5 | Bridge |
| 06-12 | 5 | Particle, Effect (keyframe curve editor) |
| 06-13 | 5 | Terrain (tileset/crosset), 3D Road, 3D River |
| 06-14 | 5 | Mission, Chapter, Campaign, Medal (the `CImageFrame` editors) |
| 06-15 | 5 | GUI editor (switched off in MFC; ported against the current UI XML) |
| 06-16 | 6 | Full sweep: all goldens, `test-resources-all`, game reads a mod with one resource per kind, hand try macOS + Windows, delete the MFC editor |

Oracle runbook for the win-home MFC batch export (plan 06-01): `06-ORACLE-RUNBOOK.md`.

Each sub-editor plan (06-06..06-15) delivers, per sub-editor: the model
export port in `ResourceModel`, goldens compared under "the game reads it
unchanged" (spec), Import from game data, its preview and overlays, its
tools as undoable commands, its toolbar, and one `BK_EDITOR_AUTO` scenario.

## A. Shared features (every sub-editor)

| # | MFC feature | MFC source | Plan | Status |
|---|---|---|---|---|
| A-01 | File → New Project (Ctrl+N), per sub-editor kind, with New Dir dialog | `ParentFrame.cpp`, `NewDirDialog` | 06-05 | done (S05 T09: lifecycle_ui File > New offers all 21 kinds; resource-editor-auto frame 1 do=new:wpn, kind and untitled asserted; resource-editor-host-check) |
| A-02 | File → Open Project (Ctrl+O), switches sub-editor by extension | `ActivateFrameByExtension` | 06-05 | done (S05 T09/T13: Open by extension switches the kind; resource-editor-auto opens a copy of the tracked .unt and asserts kind:unt and its node count) |
| A-03 | File → Close Project | `ParentFrame.cpp` | 06-05 | done (S05 T09: Close with the unsaved-changes prompt; test-resource-app-logic) |
| A-04 | File → Save (Ctrl+S) / Save As; project XML format unchanged; `History` list (≤100) | `CTreeDockWnd::SaveTrees` | 06-03 (format), 06-05 (UI) | done (S05 T09/T13: Save and Save As, format unchanged through the bridge; resource-editor-smoke --smoke-edit edits, saves, undoes, saves again and compares the original 54385 bytes) |
| A-05 | Save keeps a backup (MFC `backup.tmp`) → safe save + `.bak` | `ParentFrame.cpp` | 06-05 | done (S05 T09/T13: safe save, one .bak per session; test-resource-bridge asserts bytes, .bak and no .tmp; resource-editor-smoke) |
| A-06 | Project lock `locked_<username>` on open | `ParentFrame.cpp` | 06-05 | done (S05 T09: locked_<user> prompt with take-over; test-resource-app-logic) |
| A-07 | Recent files (MFC 7 entries, registry) | `MainFrm.cpp` | 06-05 | done (S05 T09: Open Recent from resourceeditor.cfg, kept in the settings file, not a registry; test-resource-app-logic) |
| A-08 | File → MOD Settings (Ctrl+M): `mod.xml` name/version/description, seed `modobjects.xml` | `MODDialog` | 06-05 | done (S05 T11: tools_ui MOD Settings modal over BkResModSettingsGet/Set, which writes data/mod.xml and seeds modobjects.xml; export folder follows -mod= at start, a change lasts the session) |
| A-09 | File → Export Result (Ctrl+E) | `OnFileExportFiles` | 06-05 (flow), each sub-editor plan (content) | done (S05 T11 flow: tools_logic.runExport refuses by the app's own path (no project, untitled, recovery copy), report = written/skipped/warnings + missing gamma.cfg; test-resource-app-logic. Content per sub-editor) |
| A-10 | File → Compress current MOD to PAK (MFC: `zip.exe -9 -R -D`) → native writer | `ParentFrame.cpp` | 06-05 | done (S05 T11: save dialog -> BkResPackMod, .pak added; bridge zip writer read back through the engine) |
| A-11 | File → Exit (Ctrl+X) with unsaved-changes prompt | `MainFrm.cpp` | 06-05 | done (S05 T09: Quit guarded by the unsaved-changes prompt; test-resource-app-logic) |
| A-12 | Edit → Set Picture Options (brightness/contrast/gamma, `gamma.cfg` searched upward, preview `SingleIcon`) | `PictureOptions` | 06-05 | done (S05 T11: app-side gamma.cfg in the engine tree format, searched upward like ReadConfigFile, written to the project folder or <source>/<szAddDir>; engine's CreateGammaCorrection ported for the before/after ramps; test-resource-app-logic) |
| A-13 | View → Toolbar, Status Bar, Project Tree, Object Inspector toggles | `MainFrm.cpp` | 06-05 | done (S16 T02: View menu toggles for Toolbar, Status Bar, Project Tree and Object Inspector through view_logic.zig, kept in resourceeditor.cfg; test-resource-app-logic covers state and settings round trip, resource-editor-auto-core drives do=view and measures the frame change) |
| A-14 | View → Direction Button (Ctrl+D) dock | `DirectionButton*` | 06-05 (widget), 06-06/06-08/06-12 (use) | done for the widget (S05 T12: docks.zig Direction window, Ctrl+D; angle from the drag, squashed needle, degrees text and GetQuadrant ported as written; resource-editor-host-check docks half (resource-editor-check-docks.tga measured by code), test-resource-app-logic (docks_logic.zig)). The sub-editors that read the angle use it in their slices |
| A-15 | View → Function Window (Ctrl+F) keyframe dock | `KeyFrame*` | 06-12 | dock frame done (S05 T12: docks.zig Function window, Ctrl+F, an empty graph frame; resource-editor-host-check docks half (resource-editor-check-docks.tga measured by code), test-resource-app-logic (docks_logic.zig)). The keyframe editing is 06-12; S12 T04-T06: key-frame curve widget (grid, polyline, keys, add/move/delete, zoom and Reset all) in the Function window, proved by auto `do=keyframe:` (frames 484-504) S13 T04: pointer-driven: add/drag/delete through the displayed widget (resource-editor-auto frames 525-532: Ctrl+F opens the Function window, SDL pointer and Delete key events on the real event queue, keys read back after each gesture, undo and redo; pcp_fn_add and pcp_fn_drag handle pixels measured at the drawn place). |
| A-16 | View → Set Background Colour | `ParentFrame.cpp` | 06-05 | done (S16 T02: View > Set Background Colour picker paints behind the panels and is kept in resourceeditor.cfg; resource-editor-auto-core counts the #336699 pixels of a captured frame, none before and 809200 after) |
| A-17 | View → Expand/Collapse all (Ctrl+C) | `ETreeCtrl` | 06-05 | done (S16 T02: View > Expand/Collapse all and Ctrl+C through edit_logic.setExpandAll, MFC's flag flips and the first use collapses, no undo step as in MFC; test-resource-app-logic over the fake bridge, resource-editor-auto-core reads the open items back from the tree) |
| A-18 | Tools → Set Directories (Ctrl+T): source, export, game exe, game args (registry) → settings file | `SetDirDialog` | 06-05 | done (S05 T11: source folder, game folder (empty = Game beside the editor) and game arguments in resourceeditor.cfg; the export folder is MOD Settings', as in MFC's SetDirDialog) |
| A-19 | Tools → Export RPG Stats (Ctrl+R) — MFC wired for Infantry only → every sub-editor | `AnimationFrm.cpp` | 06-05 (flow), sub-editor plans | done (S05 T11 flow: Tools > Export Stats Only, Ctrl+R, for every kind through BkResExportStatsOnly; content per sub-editor) |
| A-20 | Tools → Batch Mode (Ctrl+B) dialog: src, dst, mask, `-f`, `-os`, progress, failure list, missing `gamma.cfg` list | `BatchModeDialog`, `ProgressDialog` | 06-05 | done (S05 T11: Batch Mode modal (kind or all, src, dst, -f, -os) -> report of projects exported, failed with reasons, warnings, missing gamma.cfg; runs in-frame, no live progress: BkResBatch has no callback) |
| A-21 | Command-line batch `editor.exe <*.ext> <src> <dst> [-f] [-os]`, up-to-date check | `CEditorApp::RunBatchMode`, `ExportSingleFile` | 06-05 | done (S05 T11: `ResourceEditor --batch <kind> <src> <dst> [-f] [-os]` (kind: an extension, `*.ext` or `all`), hidden window, exit 0/1/2; resource-editor-batch re-saves the 21 fixtures byte for byte and reopens them with the engine's reader) |
| A-22 | Tools → Run Blitzkrieg (F7) with args and `-mod=` → test launch | `ParentFrame.cpp` | 06-05 | done (S05 T11: F7 via kit/testlaunch with profile ResourceEditorTest, -editor-test, -mod=<export folder> when it sits in <base>mods/, no map; test-map-editor-testlaunch, test-resource-app-logic) |
| A-23 | Tools → SaveMapObjects | `OnSaveObjects` (no message-map entry) | 06-05 | no behaviour in MFC (S05 T11: OnSaveObjects copied the data folders of every object the source maps named into the export root, but had neither a menu nor a message-map entry, so no user could run it; not ported) |
| A-24 | Editors menu: 20 sub-editors in MFC order; last active remembered | `CFrameManager`, `SwitchActiveFrame` | 06-05 | done (S05 T11: Editors menu and menu-bar combo from panels_logic.editors_menu, choice through the session's switch_editor; active kind remembered however it became active; test-resource-app-logic) |
| A-25 | Help → About | `IDD_ABOUTBOX` | 06-05 | done (S05 T12: Help > About shows IDD_ABOUTBOX's lines, then the port, spec, source and licence; docks_logic.zig about_* tested in test-resource-app-logic) |
| A-26 | Help → Help (F1) `reshelp.chm` | `ID_HELP` | 06-05 | done (S05 T12: the `.chm` is not in the repository; Help (F1) shows the shortcut list and the spec's path, opens it in the browser or copies the link; test-resource-app-logic). Shortcuts T11 adds belong in `docks_logic.shortcuts` too |
| A-27 | Project tree: select, rename display name, expand state saved | `CTreeDockWnd`, `CETreeCtrl` | 06-05 | done (S05 T10: panels.zig tree; select/multi-select, rename via BkResSetNodeName, expand via BkResSetNodeExpand kept in `expand` and read back after save+reopen; test-resource-app-logic (edit_logic.zig), test-resource-bridge (T10 tree block)) |
| A-28 | Tree context: Insert item (per-kind child classes), Delete item | `IDR_INSERT_TREE_ITEM_MENU`, `IDR_DELETE_TREE_ITEM` | 06-04 (commands), 06-05 (UI) | done (S05 T10: Insert offers the class the container already holds, Delete of a multi-selection is one undo step, Move up/down and drag; an empty container offers nothing until its sub-editor slice names the class; test-resource-app-logic (edit_logic.zig), test-resource-bridge (T10 tree block)) |
| A-29 | Object inspector: every domain type (`DT_DEC/STR/BOOL/FLOAT/COMBO/BROWSE/COLOR`) | `COI/*` | 06-05 | done (S05 T10: edit_logic.widgetFor covers every DT_* incl. HEX and BROWSEDIR; values checked before BkResSetProp; combo/bool strings via BkResPropStrings; test-resource-app-logic) |
| A-30 | Reference pickers: all 20 `EReferenceType` lists | `RefDlg` | 06-03 (lists), 06-05 (UI) | done (S05 T10: refTypeFor maps all 20 DT_*_REF domains to the 20 lists, searchable picker over BkResRefList; test-resource-app-logic) |
| A-31 | Multi-select dialog | `MultySelDialog` | 06-05 | done (S05 T10: actions picker = CMultySelDialog; BkResRefList type 5 answers actions.ini with id tokens, mask written in the variant's text; test-resource-bridge, test-resource-app-logic) |
| A-32 | Browse dialog (source-relative paths) | `BrowseDialog`, `MyOpenFileDialog` | 06-05 | done (S05 T10: SDL file/folder dialog, path made relative to the prop's source folder else the project folder, lower case, backslashes; test-resource-app-logic) |
| A-33 | AI class combo, player sides combo | `Reference.cpp`, `UnitSide.cpp` | 06-03 | done (S05 T10: the S03 combo lists reach the inspector's combo through BkResPropStrings; test-resource-bridge) |
| A-34 | Localisation items (name/desc/stats `.txt`) | `localization.*` | 06-03 | done (S05 T10: localisation items are ordinary tree items edited in the inspector; the .txt export is S03's) |
| A-35 | Thumbnail list dock | `ThumbList*` | 06-05 (widget) | done for the widget (S05 T12: View > Thumbnails lists a folder's `*.tga` (else the project's folder), decoded by the engine through BkEditorMinimapImage into kit pictures_cache, fitted on black in 64-pixel cells, select and double-click; resource-editor-host-check docks half (resource-editor-check-docks.tga measured by code), test-resource-app-logic (docks_logic.zig): the fixture picture's colour measured in the capture) |
| A-36 | Import XML file (Ctrl+I) — no handler in MFC → Import from game data | `ID_IMPORT_XML_FILE` | 06-04 (bridge), 06-05 (UI), sub-editor plans (per kind) | UI done (S05 T12: File > Import from game data (Ctrl+I), a guarded action through the unsaved prompt; unt imports, other kinds show the bridge's refusal naming the kind; test-resource-app-logic (lifecycle.zig), host check imports the shipped Gunner). Per-kind imports come with the sub-editor plans |
| A-37 | Engine preview window (`CGameWnd`, storage + MOD, `consts.xml`, objects DB) | `GameWnd.*`, `GlobalsLoader.cpp` | 06-01 (spike), 06-04 | done (S05 T07/T12: the engine preview scene behind the windows; resource-editor-host-check measures the capture by code, resource-editor-auto asserts shot_lit and differ on captured frames) |
| A-38 | Undo/redo for every edit (new; MFC had none outside GUI) | — | 06-04 + every sub-editor plan | done for the shell (S05 T10: every tree/inspector edit is a ResourceCommand; gestures collapse to one step; a value back at its start drops the step and the unsaved mark; Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z; test-resource-app-logic). Sub-editor slices add their own edits |
| A-39 | Autosave and crash recovery (new, as the map editor) | — | 06-05 | done (S05 T09: autosave and recovery under <UserRoot>resourceeditor/recovery; test-resource-app-logic) |

## B. Sub-editors

### B-01 Unit editor (`CMeshFrame`, `.msh`, `units\technics\`) — 06-08

| # | Feature | Status |
|---|---|---|
| B-01.1 | Tree: Common, Defences, Graphics, Platforms (Guns), Joggings, Locators, Avia, Effects, Sound, Death craters, Track | done: mesh import and tree tests in resource_bridge_test (S08 T02); auto frames 192-195 (open msh, nodes_min:20) |
| B-01.2 | 3D mesh preview (`IMeshVisObj`, `AddMeshPair`) | done: resource_bridge_test "mesh preview" captures per variant (S08 T04); auto frames 203-213 shot_lit unit_combat/unit_install/unit_transportable |
| B-01.3 | Show locators / show bounding box (toolbar) | done: mesh_logic toolbar test "the toolbar sets the variant and the toggles through the bridge"; auto frames 216-218 (locators on, differ unit_locators). Bounding boxes: toolbar toggle through BkResPreviewShowLocators |
| B-01.4 | Pick a locator with right-click | done: mesh_logic tests "pickLocator takes the nearest marker...", "a pick selects the locator's tree node and records nothing"; auto frames 219-222 (pick_locator, expect=selected) |
| B-01.5 | Gun point, gun part, carriage, platform dropdowns | done: mesh_logic tests "the locator combos follow the combat model's skeleton" and the undo/redo tests for a gun point, part and carriage locator reference and a platform's locator choice |
| B-01.6 | Combat / install / transportable model switch | done: mesh_logic test "undo and redo: a model switch rebuilds the locator children both ways"; auto frames 205-213 (mesh_variant 0, 1, 2) |
| B-01.7 | Direction arrow dock | done: mesh_logic test "the direction dock's angle turns the previewed unit by the degrees it shows" |
| B-01.8 | Export: `1.xml` `SMechUnitRPGStats`, copied `*.mod`, `1/1w/1a/2/2w/2a/1p*` DDS, `icon.tga` 64 px, `icon` DDS 128 px, `icon512`, `name/desc.txt`; auto DXT format choice | done except golden: msh exporter tests in resource_bridge_test (S08 T01, T03); auto frames 226-228 (export, 1.xml, 1.mod); golden pending win-home |
| B-01.9 | Import from game data | done: six shipped units import, save, reopen and export stats-only field-equal to their 1.xml (S08 T02) |

### B-02 Infantry editor (`CAnimationFrame`, `.unt`, `units\humans\`) — 06-07

| # | Feature | Status |
|---|---|---|
| B-02.1 | Tree: Common, Localization, AI, Weapon, Grenade, Season directories, Animations/Frames, Actions, Exposures, Acks/Ack types | done: resource_core sub_editor_tools tests; auto frames 170-174 (open unt, set_prop) |
| B-02.2 | Animation preview Run/Stop (F5) | done: previewPlayback via ResBridge vtable; test-resource-bridge Run/Stop shots; auto `preview_run`/`preview_stop` + `shot_lit` (frames 178-182) |
| B-02.3 | Frame thumbnail list | done: infantryAddFrame/delete undoable (sub_editor_tools tests); auto `frame:`/`delete_frame` |
| B-02.4 | Ack Import/Export (`IDR_ACK_MENU`; MFC has enable handlers only) | no behaviour in MFC: IDR_ACK_MENU items ID_IMPORT_ACK_FILE and ID_EXPORT_ACK_FILE have only always-Enable ON_UPDATE_COMMAND_UI handlers in AnimationFrm.cpp and MeshFrm.cpp, DisplayAcksMenu has no caller, CUnitAckTypesItem and CUnitAckTypePropsItem are empty; the Ack set references are already pickers |
| B-02.5 | Export RPG Stats only (Ctrl+R) | done: unt exporter stats-only path; test-resource-bridge |
| B-02.6 | Export: `1.xml` `SInfantryRPGStats`, `1[b][w\|a].san` + DDS per season/blood variant, `name/desc/stats.txt` | done: unt exporter (1.xml, 1[b][w|a].san + DDS, localisation copies) with comparator and bridge tests, auto `do=export` + `expect=exported` (frame 177); golden parity pending win-home (export-goldens.ps1 -Extensions spt,unt) |
| B-02.7 | Opens every `.unt` in `Data/Old` and `WinSniper.unt` | done: 15-file .unt round-trip loop over Data/Old + WinSniper.unt (test-resource-core, test-resource-bridge) |
| B-02.8 | Import from game data | done: shipped-human import comparison (test-resource-bridge) |

### B-03 Squad editor (`CSquadFrame`, `.scp`, `squads\`) — 06-06

| # | Feature | Status |
|---|---|---|
| B-03.1 | Tree: Common (picture, type), Members, Formations | done: tree in panels; resource-editor-auto S06 block (scp) |
| B-03.2 | Formation layout: drag members | done: FormationDrag one undo step; sub_editor_tools tests (fake + real bridge), squad_logic tests, auto `squad_drag` + `expect=slot` |
| B-03.3 | Set zero point (toolbar) | done: setZeroPoint undoable; sub_editor_tools tests, auto `squad_zero` |
| B-03.4 | Direction arrow dock | done: formation_direction arrow, one composite undo step; sub_editor_tools tests, auto `squad_dir` + `expect=direction`; MFC angle convention atan2(-dx, dy): T01-T03 tests print expected/actual angle and vector, auto `squad_arrow` + `expect=squad_dir` |
| B-03.5 | Export: `SSquadRPGStats` + copied icon | done: scp exporter (stats + icon copy) read back by engine; test-resource-bridge, auto `do=export`; golden parity pending win-home (export-goldens.ps1 -Extensions wpn,mcp,trc,scp) |
| B-03.6 | Import from game data | done: import-then-export round trip german_rifle_45 in test-resource-bridge |

### B-04 Weapon editor (`CWeaponFrame`, `.wpn`, `weapons\`) — 06-06

| # | Feature | Status |
|---|---|---|
| B-04.1 | Tree: Common, Shoot types, Damage, Sound, Effect, Flash, Craters, Effects | done: shoot/damage/sound/effect/flash/craters tree; sub_editor_tools weapon tests, auto `tree:add_shoot_type` (undo/redo/save/export); preview: no behaviour in MFC (D015) |
| B-04.2 | Export: `SWeaponRPGStats` to `weapons\<name>.xml` | done: wpn exporter read back field-equal by CTreeAccessor; test-resource-bridge, auto `do=export`; golden parity pending win-home (export-goldens.ps1 -Extensions wpn,mcp,trc,scp) |
| B-04.3 | Import from game data | done: round trip mg_37t in test-resource-bridge |

### B-05 Mine editor (`CMineFrame`, `.mcp`) — 06-06

| # | Feature | Status |
|---|---|---|
| B-05.1 | Tree: Common (name, weight) | done: name/weight tree; auto `set_prop:Weight` undo/redo (mcp) |
| B-05.2 | Export: `SMineRPGStats`, `ComposeSingleObject` from `1.tga`/`1s.tga` | done: mine exporter with ComposeSingleObject (_c/_l/_h.dds + .san); test-resource-bridge, auto `do=export` + shot=mine; golden parity pending win-home (export-goldens.ps1 -Extensions wpn,mcp,trc,scp) |
| B-05.3 | Import from game data | done: round trip mine_at in test-resource-bridge |

### B-06 Particle editor (`CParticleFrame`, `.pcp`, `effects\particles\`) — 06-12

| # | Feature | Status |
|---|---|---|
| B-06.1 | Tree: Common, Source generate (spin, area, angle, opacity, speed, life, density, random spin), Particle curves (spin, weight, speed, size, opacity, texture frame), Complex source, Random life/speed | done: pcp tree opens and exports (Opacity and Life curves read); auto frames 480-484; all 291 shipped sources round-trip (PARTICLES checked=291 unimportable=0, S12 T01) |
| B-06.2 | Run/Stop preview, Camera switch | done: auto `preview_run`/`preview_stop`/`camera` with measured shots (running frames differ 0.41%, stopped frames equal, horizontal camera differs 0.17%); BkResPreviewCameraMode in test-resource-bridge; frames 510-521 |
| B-06.3 | Get particle info | done (S13 T01): BkResGetParticleInfo runs the real pcp exporter, builds the effect and reads IParticleSourceWithInfo::GetInfo of the last source; resource-bridge-test `particle info` checks (fixture Max particles 5, Size 0.01687, Average size 0.0130261, Average count 4.48826; density x4 gives Max particles 20; a .wpn project is refused naming .pcp); resource-editor-auto pcp frame 505 `do=particle_info` + `expect=particle_info:present` prints the four values; app status bar shows MFC's four panes (docks_logic ParticleStatus tests) |
| B-06.4 | Simple / complex source toggle | done (S13 T02): BkResParticleSourceMode / BkResParticleSetSourceMode, one undo step per switch (complex needs a non-empty complex particle name, simple clears it), one IsComplexSource derivation shared with ExportParticle; toolbar checkbox (docks_logic.SourceToggle); resource-bridge-test, test-resource-app-logic and resource-editor-auto-pcp `do=source_mode:` with the exported flag checked (export_complex / export_simple); golden pending win-home |
| B-06.5 | Keyframe curve editor: add/move/delete node, Reset all, Zoom in/out X and Y (`IDR_KEYFRAME_ZOOM_MENU`) | done: keyframe_logic.zig (17 tests) and the Function window widget; auto `do=keyframe:` add/move/delete/reset each undone and redone with keys read back, zoom steps (frames 484-504). Deviation: MFC zoom handlers are commented out, the port zooms the view only, not an undo step (D026) S13 T04: pointer-driven: add/drag/delete through the displayed widget (resource-editor-auto frames 525-532: Ctrl+F opens the Function window, SDL pointer and Delete key events on the real event queue, keys read back after each gesture, undo and redo; pcp_fn_add and pcp_fn_drag handle pixels measured at the drawn place). |
| B-06.6 | Export: `KeyData` (`SParticleSourceData` / `SSmokinParticleSourceData`) | done: pcp exporter, simple and complex; auto `do=export` (frame 502, 529); golden parity pending win-home (export-goldens.ps1 -Extensions pcp) |
| B-06.7 | Import from game data | done: import round trip of all 291 shipped sources, 0 differences; auto `do=import_file:pcp` (frame 526) |

### B-07 Sprite editor (`CSpriteFrame`, `.spt`, `effects\sprites\`) — 06-07

| # | Feature | Status |
|---|---|---|
| B-07.1 | Tree: Sprites, Sprite properties | done: sub_editor_tools tests; auto frames 143-145 |
| B-07.2 | Run/Stop preview; thumbnail list | done: measured Run/Stop shots in test-resource-bridge; auto `preview_run`, `shot_lit`, `differ`, `shot_same` (frames 151-159) |
| B-07.3 | Export: `1.san` + DDS (`BuildAnimations`, `SSpriteAnimationFormat`) | done: spt exporter (1.san + 1_c/1_l/1_h DDS), .san byte-identical to shipped Mp43 1.san; test-resource-bridge, auto `do=export` (frame 150); golden parity pending win-home (export-goldens.ps1 -Extensions spt,unt) |
| B-07.4 | Import from game data | no behaviour in MFC (no reverse path for sprites) |

### B-08 Effect editor (`CEffectFrame`, `.eff`, `effects\effects\`) — 06-12

| # | Feature | Status |
|---|---|---|
| B-08.1 | Tree: Common, Animations (sprites), Meshes, Function particles, Maya particles, Lights | done: eff opens, exports and its tree is measured (nodes_min:5); auto frames 532-535 |
| B-08.2 | Run/Stop, Camera switch | done in the shared preview: Run names the effect's missing function-particle source (auto `preview_refused`, frame 536); Run/Stop/Camera measured on pcp (B-06.2). Run of an eff with a present source not measured |
| B-08.3 | Direction arrow dock | done (S13 T03): the Direction dock drives .eff projects (BkResEffectSetDirection/GetDirection, view state, 45 degrees on open, UpdateEffectAngle's matrix through the running effect's SetEffectDirection); child X/Y/Z positions are whole numbers (a fractional text keeps its integer part), one undo step each, exported unchanged into vPos. Evidence: test-resource-bridge (matrix for 0/45/90 degrees and the 2 pi wrap, vPos read back through operator&, undo/redo, running preview frames differ by 3.2%, though a same-angle pair differs by 3.3% from the particles' own motion, so the frame measure alone does not prove the turn; the matrix test does), test-resource-core, test-resource-app-logic, resource-editor-auto (do=effect_direction, expect=effect_angle, X_position edit, undo, redo, export) |
| B-08.4 | Interpolate Vector Items (`IDR_INTERPOLATE_TREE_ITEM_MENU`; MFC has the enable handler only) | no behaviour in MFC (`grep -rn -i interpolate Sources/src/editor/*.cpp` shows only `ON_UPDATE_COMMAND_UI` at EffectFrm.cpp:34 and its enable handler at :395; D026) |
| B-08.5 | Export: root `"effect"` = `SEffectDesc` | done: eff exporter (sprites, plain and smokin function particles, root effect); auto `do=export` (frame 535); golden parity pending win-home (export-goldens.ps1 -Extensions eff) |
| B-08.6 | Import from game data | no behaviour in MFC: import refused naming the missing reverse path (auto `import_refused:eff`, frame 530; D026) |

### B-09 Building editor (`CBuildingFrame`, `.bld`, `buildings\`) — 06-10

| # | Feature | Status |
|---|---|---|
| B-09.1 | Tree: Common, Entrances, Slots, Graphics 1–3 summer/winter, Defences, Passes, Fire points, Directed explosions, Smokes | done: bridge tree of the fixture (nodes_min:2 at auto frame 343); test-resource-bridge S10Building::Fixture |
| B-09.2 | Move object | done: Move tool on the building sprite (grid_logic tests "entrance: ... set zero and Move reach the building too"); sprite_pos home in the bridge |
| B-09.3 | Draw passability grid (locked/unlocked tiles) | done: tile-frame passability, grid_logic colour test; auto `bld` block (frames 340-399, `resource-editor-auto`) measured locked red 12 to 397 pixels, back to 12 after undo |
| B-09.4 | Transparency dropdown and transparency cells | done: transparency_cells on the building root; auto `bld` block (frames 340-399, `resource-editor-auto`) measured 0x606000 0 to 183 pixels, back to 0 after undo |
| B-09.5 | Set entrance | done: grid_logic test "entrance: the click's world point, one undo step"; auto `do=entrance` and `entrance_tile` |
| B-09.6 | Set zero | done: grid_logic test "set zero: the click's world point, one undo step"; auto `do=grid_zero` and `zero_tile` |
| B-09.7 | Shoot-point mode (slots) | done: grid_logic test "shoot mode: ..., undo and redo"; auto `do=point:shoot`, measured shot of the active point colour |
| B-09.8 | Fire-point mode | done: point_tools fire placement test; auto `do=point:fire`, measured 0xff8000 0 to 51 pixels, back to 0 after undo |
| B-09.9 | Directed-explosion mode | done: point_tools tests "directed explosions have no place and no delete" and "generate directed explosions ..."; the repo fixture holds no DirExplosions entries, so the auto block drives smoke generation and the explosion generate stays covered by the unit tier |
| B-09.10 | Smoke-point mode | done: point_tools smoke test; auto `do=point:smoke` and `do=generate_points:smoke` (2 generated points, undone with the rest) |
| B-09.11 | Move point, set horizontal position, set angle / cone | done: grid_logic test "move point and horizontal position: one undo step per drag ..."; point_tools drag and one-shot direction tests; auto `point_move`, `point_angle`, `point_cone` with measured 0xffff00 handle 0 to 33 pixels |
| B-09.12 | Generate points | done: grid_logic test "generate points: enabled in smoke and explosion modes only, one undo step each"; point_tools generate tests; auto `do=generate_points:smoke` |
| B-09.13 | Export: `desc` = `SBuildingRPGStats`, sprite + shadow packs with passability, `icon.tga` | done: building exporter (ExportBuilding, BuildingStatsToTree); test-resource-bridge S10Building::Fixture export, determinism, missing-picture and round-trip checks; golden parity pending win-home (export-goldens.ps1 -Extensions bld) |
| B-09.14 | GOG `INTEX2 brandenburgertor/current.bld` exports equal to its golden (win-home) | pending win-home: bld-gog-brandenburgertor (S10Building::GogBrandenburgertor prints `GOLDEN bld-gog-brandenburgertor pending` without BK_GOG_ROOT and BK_GOG_GOLDEN; the GOG files are never committed) |
| B-09.15 | Import from game data | done: S10Building::Shipped imports every shipped Data/Buildings folder with a field-equal round trip; D021 guard S10Building::NegativeTiles (182 checked, 0 negative) |

### B-10 Object editor (`CObjectFrame`, `.obt`, `objects\`) — 06-09

| # | Feature | Status |
|---|---|---|
| B-10.1 | Tree: Common, Graphics (sprite/shadow, summer/winter/Africa), Particles, Passes, Effects | done (S05 tree/inspector; resource-editor-auto obt block expect=nodes_min:2) |
| B-10.2 | Move, Draw grid, transparency dropdown, Set zero | done (S09 T04-T06: grid_tools tests, grid_logic tests, resource-editor-auto obt block draws, undoes, redoes and measures 0xff0000 and 0x606000 shots) |
| B-10.3 | One-way transparency lines (`TransLines`) | done (S09: trans-line core tests; resource-editor-auto obt trans_line, lines:2 -> 3 -> 2 -> 3) |
| B-10.4 | Export: `desc` = `SObjectRPGStats`, `1/1s/1w/1ws` `.san` + DDS, icon, `name.txt` | done on repo fixtures (S09 T02: bridge export tests; auto obt export checks 1.xml and 1_c.dds); golden pending win-home |
| B-10.5 | Import from game data | done (S09 T02: obt importer and its bridge tests; the docks check no longer expects .obt to be refused) |

### B-11 Fence editor (`CFenceFrame`, `.fnc`, `fences\`) — 06-09

| # | Feature | Status |
|---|---|---|
| B-11.1 | Tree: Common, Directions, Insert, per-segment properties | done (S05 tree/inspector; resource-editor-auto fnc block expect=nodes_min:2) |
| B-11.2 | Move, Draw grid, transparency dropdown, Centre fence on tile | done (S09 T04-T06: grid_tools and grid_logic tests; resource-editor-auto fnc block draws, centres, undoes, redoes and measures 0xff0000 and 0x808000 shots) |
| B-11.3 | Thumbnail list | done (S05 thumbnail work and S09 T05 fence lists in panels.zig) |
| B-11.4 | Export: `SFenceRPGStats`, `ComposeFences` sprites, icon | done on repo fixtures (S09 T03: bridge export tests, index-hole refusal; auto fnc export checks 1.xml and 1_c.dds); golden pending win-home |
| B-11.5 | Import from game data | done (S09 T03: importer and bridge test) |

### B-12 Bridge editor (`CBridgeFrame`, `.bdg`, `bridges\`) — 06-11

| # | Feature | Status |
|---|---|---|
| B-12.1 | Tree: Common, Defences, Begin/Center/End spans, Parts, Stages (damage states), Fire points, Directed explosions, Smokes | done (S11 T01-T02: the importer builds the tree from stats for all 21 shipped Data/Bridges folders, `BRIDGES checked=21 differences=0`; the app's tree and the resource-editor-auto bdg block open the fixture with expect=nodes_min:2) |
| B-12.2 | Draw grid, draw bridge passability, transparency dropdown, Set zero | done for Draw grid on the active span part's locked tiles and Set zero (S11 T03-T05: undo/redo tests in point_tools and grid_logic; auto bdg block brushes two tiles, measures the 0xff0000 shot 263 -> 629 pixels, undoes back to 263). Not ported: Draw pass (unlocked tiles) and the transparency dropdown, because the C++ bridge homes only the locked tiles on a bridge part (S11 T04 decision) |
| B-12.3 | Span marks `Begin/End/Front/Back` | done (S11 T03-T05: `setSpanMark` undo/redo tests; auto bdg block moves all four marks, measures the cyan 0x00ffff crosses 179 -> 377 pixels, undoes to the home marks and redoes. The auto run exposed that undoing the first mark wrote an empty list, which the channel refuses: undo now restores the frame's defaults) |
| B-12.4 | Fire points, smoke points, move point, horizontal position, angle, generate points | done (S11 T03-T05: bridge fire, smoke and directed-explosion channels with undo/redo tests; auto bdg block places a fire point (0xff8000, 51 pixels, 0 after undo) and a smoke point, undoes and redoes both). Shoot points do not exist on a bridge, as in CBridgeFrame |
| B-12.5 | Export: `SBridgeRPGStats` (segments/spans/states), sprite + shadow packs, `icon.tga` | done on repo fixtures (S11 T01: export tests over the 54-picture fixture; auto bdg block exports and checks data/bridges/bdg/1.xml and 1_c.dds); golden pending win-home |
| B-12.6 | Import from game data | done (S11 T02: every shipped Data/Bridges folder (21) imports and exports stats-only field-equal, `BRIDGES checked=21 folders=21 failed=0 differences=0`; `NEGTILES bridges checked=21 negative=0`) |

### B-13 Trench editor (`CTrenchFrame`, `.trc`) — 06-06

| # | Feature | Status |
|---|---|---|
| B-13.1 | Tree: Common, Sources (`.mod` models), Defences | done: sources tree; sub_editor_tools trench tests, auto `tree:add_source` (undo/redo) |
| B-13.2 | Preview of the entrenchment models | done: preview measured by captured frame; auto `shot=trench` + `expect=shot_lit` + `differ` |
| B-13.3 | Export: `SEntrenchmentRPGStats`, copied `.mod`, `1/1w/1a` DDS | done: trc exporter (stats, .mod copies, 1/1w/1a DDS); test-resource-bridge, auto `do=export`; golden parity pending win-home (export-goldens.ps1 -Extensions wpn,mcp,trc,scp) |
| B-13.4 | Import from game data | done: round trip Entrenchment in test-resource-bridge |

### B-14 Mission editor (`CMissionFrame`, `.mip`, `scenarios\`) — 06-14

| # | Feature | Status |
|---|---|---|
| B-14.1 | Tree: Common, Objectives, Musics | done (S14 T04: mission model and tree, bridge tests `S14MissionExport` and the ussr/finland import; `resource-editor-auto-mip` opens mip/final-map with expect=kind:mip and selects the first objective) |
| B-14.2 | Generate map image (`MinimapCreation`) | done (S14 T03 `BkResMissionMinimap`, test `MISSION MINIMAP map_c/l/h.dds 512x512: 1`; D032 S15 T01: the Image frame always shows map_h.dds and makes it when missing, as CMissionFrame does; tests `image: a mission with a Final map shows map_h.dds and makes it when missing (D032)`, `image: a refused minimap is reported, not shown as a picture` and bridge `MISSION MINIMAP map_h.dds ... differing bytes`; `resource-editor-auto-mip` starts with a map.tga and no map_h.dds, expects the file after selection and `shot_minimap` measures the shot against map_h.dds and map.tga; earlier S14: the Image window shows a 512x512 picture, centre R192 G220 B65 against background R14 G14 B14) |
| B-14.3 | Place objectives by clicking on the image | done (S14 T05 image_logic.zig tests: click places, undo/redo, edge clamp, one undo step per gesture; `resource-editor-auto-mip` clicks picture 200/150 with pointer events, the cross colour is measured at the pixel: absent R177 G203 B68, placed R255 G255 B0 (9 of 9 pixels), undone absent, redone present) |
| B-14.4 | Export: `SMissionStats`, copied `.txt`, map image via `ComposeImageToTexture`, map DDS, map `.xml` → `.bzm` | done on repo fixtures (S14 T03-T04: `S14MissionExport` reads the stats with the engine, checks the texts, map_{h,c,l}.dds, `MISSION BZM` sizes equal in chunk 1 and the quick-load chunk, the four validation refusals; auto mip exports 12 files and expects data/maps/road3d.bzm and data/scenarios/mip/map_h.dds in the export root, never in shipped Data); golden pending win-home. Ported MFC quirks kept: the last failing validation message wins, and both music checks read the combat list, so the exploration message is the one shown |
| B-14.5 | GOG `INTEX2 ardennen40/current.mip` exports equal to its golden (win-home) | pending win-home (`S14Mission::GogArdennen40` prints `pending: win-home only`; mip/golden holds the README only) |
| B-14.6 | Import from game data | done (S14 T04: BkResImportFromGame reads shipped ScenarioMissions 1.xml; ussr and finland import then export field-equal, MODName/MODVersion/ImageRect drift tolerated; MFC left objective headers empty, the port fills the header slot) |

### B-15 Chapter editor (`CChapterFrame`, `.chc`) — 06-14

| # | Feature | Status |
|---|---|---|
| B-15.1 | Tree: Common, Missions, Placeholders | done (S14 T02: chapter tree and bridge tests; `resource-editor-auto-chc` opens the fixture with expect=kind:chc and selects the first mission) |
| B-15.2 | Show crosses mode: place mission markers on the map image | done (S14 T05 image_logic.zig tests, hit box and cross drag as one step; `resource-editor-auto-chc` clicks picture 10/6 (cross R255 G255 B0, 5 of 9 pixels, absent after undo), ticks the Show crosses checkbox with a pointer click, drags the cross by 5/3 to 15/9 (marker there, gone at 10/6), one undo returns it to 10/6, redo to 15/9) |
| B-15.3 | Export: `SChapterStats`, copied `.txt` and `.lua`, image | done on repo fixtures (S14 T02: bridge export tests read the stats with the engine, `CHAPTER rect ...` line; auto chc exports 9 files); golden pending win-home |
| B-15.4 | Import from game data | done (S14 T02: shipped chapters round trip field-equal, German/Kharkov42 46 fields, 0 differences; ImageRect excluded because the shipped DDS has no source picture) |

### B-16 Campaign editor (`CCampaignFrame`, `.cgc`) — 06-14

| # | Feature | Status |
|---|---|---|
| B-16.1 | Tree: Common, Chapters, Templates | done (S14 T02: campaign tree and bridge tests; `resource-editor-auto-cgc` opens the fixture with expect=kind:cgc and selects the first chapter) |
| B-16.2 | Position chapters by clicking on the map image | done (S14 T05 image_logic.zig tests; `resource-editor-auto-cgc` clicks picture 10/6 (R255 G255 B0, 5 of 9 pixels, absent after undo, present after redo) and drags the cross in Show crosses mode as one undo step) |
| B-16.3 | Export: `SCampaignStats`, copied `.txt`, image | done on repo fixtures (S14 T02: `CAMPAIGN rect ... chapters` bridge test; auto cgc exports); golden pending win-home |
| B-16.4 | Import from game data | done (S14 T02: shipped campaign German round trips field-equal, 134 fields, 0 differences) |

### B-17 Medal editor (`CMedalFrame`, `.mdc`, `medals\`) — 06-14

| # | Feature | Status |
|---|---|---|
| B-17.1 | Tree: Common, Picture, Text | done (S14 T01: medal tree and bridge tests; `resource-editor-auto-mdc` opens the fixture with expect=kind:mdc) |
| B-17.2 | Image preview | done (S14 T05 Image window; `resource-editor-auto-mdc` shows the 20x12 picture and measures its centre R107 G73 B156 against background R14 G15 B15, contrast 292). The auto run found that the picture's property id was read 0-based (it showed the description); ids count from 1 (chapter 4, campaign 3, medal 3) and image_logic.zig now uses them |
| B-17.3 | Export: `SMedalStats`, copied `.txt`, image | done on repo fixtures (S14 T01: `MEDAL rect ...` and `MEDAL _h ...` pixel checks; auto mdc exports 6 files); golden pending win-home |
| B-17.4 | Import from game data | done (S14 T01: shipped medal round trip, the 6-digit ImageRect tolerated through NearFloat, a supplied 4.tga because Data holds only DDS) |

### B-18 Terrain editor (`CTileSetFrame`, `.til`, `terrain\sets\`) — 06-13

| # | Feature | Status |
|---|---|---|
| B-18.1 | Tree: Common, Terrains/Tiles, Crossets/Tiles, Ambient sounds, Looped sounds | done (S13 T07-T09): the tileset model and tree, tile index pools on insert/delete; resource-editor-auto-til opens the fixture, adds a tile (nodes 21 -> 22), undo, redo; golden pending win-home |
| B-18.2 | Import terrains, Import crossets (toolbar) | done (S13 T08, T09): BkResTileSetImport, fixtures til/import/terrains.xml + crossets.xml; auto `do=tile_import:terrains/` adds 4 nodes (22 -> 26); bridge tests; golden pending win-home |
| B-18.3 | Thumbnail list of tiles | done (S13 T09): terrain_logic.zig lists, crosset mode driven by tree selection (SwitchToEditCrossetsMode), double-click adds a tile (BkResTileSetAddTile); resource-editor-auto-til `do=tile_add:`; the tileset has no game preview (GameWnd hidden in MFC); golden pending win-home |
| B-18.4 | Crosset edit mode (`ID_EDIT_CROSSETS` appears only in the toolbar map) | no behaviour in MFC: `grep -rn ID_EDIT_CROSSETS Sources/src/editor` shows only MainFrm.cpp:184 toolbar map, editor.rc and resource.h, no ON_COMMAND; the mode itself is tree-selection driven (SwitchToEditCrossetsMode), ported under B-18.3 |
| B-18.5 | Export: `<name>.xml` `"tileset"` = `STilesetDesc` + tileset DDS; `crosset.xml` + DDS | done (S13 T07): tileset_export.cpp ported from ComposeTiles; resource-editor-auto-til exports and expects 1.xml, 1_c/_h/_l.dds, crosset.xml, crosset_c.dds, mod.xml; golden pending win-home |
| B-18.6 | Import from game data | no behaviour in MFC (LoadRPGStats only rebuilds the index pools); the port refuses .til import from game data with the reason (S13 T07) |

### B-19 3D Road editor (`C3DRoadFrame`, `.3rd`) — 06-13

| # | Feature | Status |
|---|---|---|
| B-19.1 | Tree: Common, Layer | done (S13 T05, T11): road3d model and tree; resource-editor-auto-3rd set_prop, undo, redo; golden pending win-home |
| B-19.2 | Preview on `maps\road3d` terrain; wireframe toggle | done (S13 T06, T11): BkResPreviewWireframe; auto-3rd shots road_solid 93.8% drawn, road_wire 30.7% drawn, solid vs wire differ 65.55%, wire vs solid again 65.55%; golden pending win-home |
| B-19.3 | Export: `VSODescription` = `SVectorStripeObjectDesc` | done (S13 T05): road3d_export.cpp through the engine's serializer; auto-3rd export; golden pending win-home |
| B-19.4 | Import from game data | done (S13 T05, T11): single-file import; shipped scan VSO checked=40 files=40 unimportable=0; auto-3rd imports Roads3D/rail_road_grass.xml, saves and exports |

### B-20 3D River editor (`C3DRiverFrame`, `.3rv`) — 06-13

| # | Feature | Status |
|---|---|---|
| B-20.1 | Tree: Bottom layer, Layers | done (S13 T05, T11): river3d model and tree; resource-editor-auto-3rv opens, exports; golden pending win-home |
| B-20.2 | Animated preview on `maps\river3d` terrain; wireframe toggle | done (S13 T06, T11): auto-3rv river_a 93.8% drawn, river_a vs river_b (1.5 s apart) differ (animated, 0.001% of the frame, threshold 0.0005%), river_c = river_d after Stop, wireframe differs 64.50%; golden pending win-home |
| B-20.3 | Export: `VSODescription` = `SVectorStripeObjectDesc` | done (S13 T05): river3d_export.cpp; auto-3rv export; golden pending win-home |
| B-20.4 | Import from game data | done (S13 T05, T11): single-file import; shipped scan VSO checked=40 files=40 unimportable=0; auto-3rv imports Rivers/water.xml, saves and exports |

### B-21 GUI editor (`CGUIFrame`, `GUIFrame2.cpp`, `.gui`; switched off in MFC) — 06-15

| # | Feature | Status |
|---|---|---|
| B-21.1 | Tree/palette: Statics, Buttons, Sliders, Scrollbars, Status bars, Lists, Dialogs (templates from `Data/Editor/UI`) | done (S15 T02, T05, T06): CUiScreen lists the Data/Editor/UI templates; the palette dock shows them by folder; auto-gui drags Buttons/Button00 onto MainMenu |
| B-21.2 | Place, move, resize controls on the screen preview | done (S15 T04-T06): anchored PositionFlag move and resize; auto-gui places at 200,400, moves +40,+30, resizes right-bottom +30,+20 and prints each measured rect |
| B-21.3 | Copy / Cut / Paste | done (S15 T02, T05): CopyText/Paste and Cut in the clipboard dock; gui_logic and gui_tools tests against the fake bridge. S16 T01 (2026-10-06): a paste gives every pasted window whose ElementID the screen or the paste already uses the next free one and reports `paste: ElementID <old> -> <new> (<window name>)`; ui_screen_test pastes a MainMenu.xml subtree twice and the engine reads the originals unchanged and the 12 pasted ids unique; undo restores the bytes, redo keeps the new ids |
| B-21.4 | Undo (`GUIundo.h` `CSaveAllUndo`) | done (S15 T04, T06): each edit is one ResourceCommand.gui; auto-gui undoes four steps back to the placed state and redoes them to the final state, both compared by rects |
| B-21.5 | Template tree (`TemplateTree`, `IDR_TEMPLATE_MENU`) | done (S15 T05): template palette tree by folder; "Create new template" writes only to the settings templates folder |
| B-21.6 | Align menu (`IDR_ALIGN_MENU`) | done (S15 T04-T06): align menu with equal size in gui_geometry; auto-gui aligns left. MFC has no handler for Set equal size or the align items (no behaviour in MFC); the port defines them as aligning to the first selected window and copying its size. "Create radio button from selected" has no behaviour in MFC and is not ported |
| B-21.7 | Property tree for controls (`MTree ctrl/`, `PropertyDockBar`) | done (S15 T05): inspector dock edits PositionFlag and attributes through BkResGuiSetAttribute, one undo step each |
| B-21.8 | Opens and saves the game's current UI screen XML (`Data/UI/*.xml`); the game loads an edited screen | done (S15 T02, T03, T06): unedited save is byte-identical (checked on every shipped screen); the gui exporter writes <mod>/data/ui/<Screen>.xml as <base>; auto-gui exports, run_game loads it with a clean log and the frames are measured. MFC Run and Test-button live mode is replaced by F7 Run Blitzkrieg with the exported mod. Linux local; macOS and Windows by CI only |

## C. MFC internals not ported as such

| MFC piece | Replacement |
|---|---|
| `LegacyUiCompat.h` Stingray shim, `SECWorkbook` etc. | ImGui docking (the header stays while the MFC map editor, until phase 5 deletes it, and MFC ELK, until phase 7, include it) |
| Registry `HKCU\Software\Nival Interactive\...` | `resourceeditor.cfg` |
| `zip.exe` | native PAK writer |
| `KeyBasedData.cpp`, `RoadEditorWnd.cpp`, `COI/OIDlg.cpp` (not compiled) | nothing — dead code |
| `Sources/src/bin/editor.exe`, `Sources/src/editor/bin/editor2.exe` | deleted in 06-16 after the goldens exist |

## D. Game reads it unchanged (S16 T03, T04, 2026-10-06, Linux)

`zig build resource-editor-game-reads-it` exports one resource per kind into a mod and writes `zig-out/local-test/resource-editor-game-reads-it/result.log`, one `KIND=` line per kind. PROOF=game: the real Game, run with `BK_MOD_TRACE`, opened the exported file from the mod. PROOF=reader: the engine's own reader (`BkResReadBack`) found the expected chunk in the exported file, and the reason the Game is not driven to load it is recorded. Linux only; macOS and Windows are left to CI (GPU runners).

| Kind | Proof | File | Reason (reader only) |
|---|---|---|---|
| wpn | game | `weapons/generic.xml` | |
| unt | game | `units/humans/ussr/mosin/1.xml` | |
| msh | game | `units/technics/german/tanks/pz_v_panther_ausf_g/1.xml` | |
| obt | game | `objects/simpleobjects/common/summer/roadpost/1.xml` | |
| fnc | game | `fences/ussr/winter/w_villagefence/1.xml` | |
| bld | game | `buildings/africa/summer/a_h01_1/1.xml` | |
| scp | game | `squads/german_gunner/1.xml` | |
| mcp | game | `objects/simpleobjects/common/summer/mine/mine_at/1.xml` | |
| bdg | game | `bridges/asphaltbridge_special/02/1.xml` | |
| gui | game | `ui/MainMenu.xml` | |
| spt | reader | `effects/sprites/gri_spt/1.san` | no shipped map loads it without a battle |
| pcp | reader | `effects/particles/particle-2key.xml` | no shipped map loads it without a battle |
| eff | reader | `effects/effects/gri_eff.xml` | no shipped map loads it without a battle |
| trc | reader | `units/technics/common/entrenchment/gri_trc/1.xml` | the Game aborts on the fixture trench, which has no segments |
| til | reader | `terrain/sets/gri_til/1.xml` | a replaced season tileset would break the terrain of the shipped maps |
| 3rd | reader | `terrain/sets/gri_3rd/1.xml` | a replaced road would break the shipped maps that use it |
| 3rv | reader | `terrain/sets/gri_3rv/1.xml` | a replaced river would break the shipped maps that use it |
| mip | reader | `scenarios/gri_mip/1.xml` | the Game starts no mission from a mod without a player |
| chc | reader | `scenarios/gri_chc/1.xml` | the Game starts no chapter from a mod without a player |
| cgc | reader | `scenarios/campaigns/gri_cgc/1.xml` | the Game starts no campaign from a mod without a player |
| mdc | reader | `medals/gri_mdc/1.xml` | the Game shows a medal only after a campaign is won |

## MFC golden comparison status (S16 T10, 2026-10-06)

`zig build test-resource-model-comparator -Dtest-mode=run`: `GOLDEN_SUMMARY extensions=20 pass=4 accepted=7 fail=0 pending=9`.
The goldens of commit 3e98ebc19 (made by the shipped editor from the re-saved fixtures) are compared for real now: the old
'pending regeneration' rules (struct defaults, D041) and the 'MFC crashed' rules for mip, chc and cgc are gone. Each
difference is either fixed in the port or a fixture, or accepted with a proof from MFC's source and the golden's bytes, or
pending with the reason that keeps it open. The per-kind table and the maintainer's list are in the spec's amendment
"S16 T10".

| Row | Status |
|---|---|
| golden wpn, mcp, unt, mdc | equal to MFC's export (pass) |
| golden msh | accepted: all 29 files, the 38 stats differences are six-digit floats (`%lg`, `DataTreeXML.cpp:326`) |
| golden pcp | accepted: 2 six-digit floats |
| golden trc | accepted: 6 DXT5 solid blocks, proven from the golden's bytes (the shipped editor's solid-block encoding is neither NDxt nor NLegacyDxt); stats equal |
| golden spt | accepted: a sprite's export is graphics only, MFC's 1.xml is the batch history |
| golden obt | accepted: all 26 files; `.san` and DDS packs equal byte for byte through `MfcEditorCamera` and the origin re-cut from tiles (`bRequantiseGrids`); 4 origin floats within the engine's float noise (1e-3, measured 1.6e-4) |
| golden fnc | accepted: all 10 files; 15 origin floats within the engine's float noise (measured up to 5.5e-4), the wrong anchor fails (negative test) |
| golden bld | accepted: all 50 files; 4 stats are MFC's struct defaults because MFC's own save of a building writes no `desc` block (`BuildFrm.cpp:560`; `mfc-new/buildingtest.bld` has none), 15 explosion points are float noise (1.5e-5) |
| camera of the goldens | proven: `MfcEditorCamera` (orthographic, 800 x 600 window, default placement, anchor 16 world cells) reproduces the fence and object goldens; 8 cells fails (T10) |
| save writes MFC's frame data (D-07, D042) | done (S16 T09), and in T10 the cached `RPG` block of chc, cgc, mip holds the tree's own paths (MFC saves with an empty prefix; the old block held the export's prefixed paths, MFC loaded them into the tree and prefixed them again) and the bridge block lists one fire and one smoke point per tree child (MFC's `GetRPGStats` indexes the block's lists by the children, the crash on opening `.bdg`) |
| golden mip, chc, cgc | pending regeneration: the goldens were made from the old fixtures (mip: an invalid mission MFC refused; chc, cgc: a block with prefixed paths); the fixtures are fixed and re-saved in T10, the golden folders no longer match them |
| golden bdg, 3rd, 3rv | pending: no golden, MFC crashed on the old fixtures; fixed in T10 (bdg: fire and smoke entries; 3rd: texture `road_asphalt_city`; 3rv: texture `water\a_bottom`) |
| golden eff, til | pending: the editor's data folder lacked the particle source and the tileset mask; `export-goldens.ps1` puts them there now (T10) |
| golden scp | pending: MFC wrote only History at `MakeName`; `USSR_Mosin` is in the tracked `Data/objects.xml` (unit, sprite), so the editor's objects database on win-home lacks it: see the spec's amendment "S16 T10" |
| msh DDS format | fixed in T08: formats chosen by the picture as `SaveCompressedTexture` does for the mesh frame |
| port keeps `<RPG>` unchanged on save | fixed in T09: the block is rewritten from the tree on every save |
| T11 golden round 3 | `test-resource-model-comparator`: pass=8 accepted=7 fail=0 pending=5 (spt, scp, til, 3rd, 3rv); see the spec's amendment "S16 T11" |
| golden bdg | accepted (T11): 26 files; the camera anchor of the golden's maker is 24 world cells with its X component on the integer 1536, one step low; 105 float-noise differences accepted (bound 1e-3) |
| golden eff | passes (T11): MFC's batch export roots an effect at `<base>`; read under it when it holds an `<effect>` |
| golden mip, chc, cgc | compared (T11): the T10 regeneration holds; pass |
| golden spt | pending (T11): MFC found no frame (`_.sprite-1frame.tga`); the fixture carries it now, golden to regenerate |
| golden scp | pending (T11): the member is named by key (`USSR_Mosin`), no objects database needed; golden to regenerate |
| golden til | pending (T11): tile art is a 32-bit targa now (cause unproven); golden to regenerate |
| golden 3rd, 3rv | pending (T11): `export-goldens.ps1` places `maps\road3d.xml`, `river3d.xml` and the terrain set in the editor's data folder; golden to regenerate |
| mip open-and-save crash | hypothesis (T11): the nested `mip/final-map` project, left out of the script's scratch copy; to confirm on win-home |
| T12 golden round 4 | `test-resource-model-comparator`: pass=9 accepted=8 fail=0 pending=3 (3rd, til, scp); see the spec's amendment "S16 T12" |
| golden 3rd, 3rv exported by hand | the two goldens were exported by hand in MFC's GUI: MFC's batch export of 3D roads and rivers crashes even for projects MFC made itself (T12) |
| golden 3rv | passes (T12) |
| golden 3rd | pending (T12): `SoilParams` is 0 in the golden, 16 in the port; MFC's source gives 16 (`3dRoadFrm.cpp:187`, `:231`); cause not proven, re-export by hand and note the two soil items |
| golden spt | accepted (T12): `1.san` and the three DDS equal; the golden's `1.xml` is the batch history |
| golden til | pending (T12): the tile art (16 x 16) was smaller than the mask (64 x 32), so MFC read past it (`TileTreeItem.cpp:189-192`); fixture art is 64 x 32 now; regenerate on win-home |
| golden scp | pending (T12): MFC's batch export never calls `CallMeAfterSerialize`, so a formation unit's `pMemberProps` is null at `SquadFrm.cpp:275`; the scratch copy lists no formation units; regenerate on win-home |

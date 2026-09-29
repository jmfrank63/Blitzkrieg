# Phase 5: Map editor M3: random map templates, minimap tools, full parity - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Milestone M3 of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`. The portable Map Editor (`Sources/editor`: Zig app, Dear ImGui, C++ bridge `Sources/src/EditorBridge`) reaches **full parity** with the MFC Map Editor (`Sources/src/MapEditor`). Then the MFC editor is deleted from the tree.

M3 delivers:
- Random map generation from the editor.
- The five RMG composers: Containers, Graphs, Fields, Templates and Filters.
- The minimap panel and minimap image creation.
- Every other MFC feature not owned by M1 (phase 3, done) or M2 (phase 4, planned in parallel).

Every MFC feature is listed in `05-PARITY.md`, with its owner (M1, M2 or M3), its M3 plan, and the evidence that closes it.

**Ownership:**
- **M1 (done):** terrain tile painting; object place, move, rotate and delete; players' sides and attacking side; the map sound list; save; test in game; mods; settings; recent files; autosave.
- **M2 (phase 4):** roads; rivers; bridges, including the destroyed/intact toggle; entrenchments; fences; reinforcement groups; start commands; reserve (artillery) positions; AI general; script file; script areas; player camera anchors; the free camera rotation revisit.
- **M3 (this phase):** everything else the MFC editor can do.

Deleting the MFC editor is the last plan of this phase. It is gated on every row of `05-PARITY.md` being closed, the M2 rows included.

**Not features.** Some things are dead or unreachable in the MFC editor itself, so the MFC editor cannot do them. They are listed in `05-PARITY.md` as "not a feature", with their evidence, and are not re-implemented:
- undo, cut, copy and paste;
- the storage-coverage overlay;
- the Sounds, Forests and Random-map-generator tabs;
- the Templates Composer's "Check!" button;
- `CPESelectStringsDialog`;
- `ID_TOOL_4`;
- the multi/heterogenous noise radios.

The portable editor already has real undo, which exceeds the MFC editor.

</domain>

<decisions>
## Implementation Decisions

### Random map generation from the editor
- **D-01:** File → Create Random Map… opens a dialog with the MFC dialog's fields:
  - template (combo plus browse);
  - context (the chapter `context.xml` files);
  - graph index;
  - setting (plus `<any setting>`);
  - direction N/E/S/W;
  - difficulty level 1–3;
  - Save as BZM or XML;
  - write DDS images;
  - map name.

  It adds one field: an optional **seed** (blank = random). The seed used is shown afterwards and stored in the `.seed` file the generator already writes, so every generation can be reproduced.
- **D-02:** Generation calls the existing `CMapInfo::CreateRandomMap` through one new bridge entry point. It writes into the user maps folder, or the mod's maps folder when a mod is active (spec D-17), passing `rszOutputRoot`. It never writes into `Data`. When it finishes, the editor opens the generated map as a normal document, with a snapshot and preservation rules as for any opened map.
- **D-03:** Generation runs on the main thread. An `IProgressHook` implementation draws a progress modal each step (19 steps), so the window stays alive. There is no mid-generation cancel; the MFC editor has none. The engine's generation touches global state (object database, storages, `rand`), so a worker thread is rejected.
- **D-04:** The polygon-fill cost (backlog 999.1) stays in 999.1 and is not pulled in. M3 adds a determinism test instead: a fixed seed generates byte-identical maps twice. 999.1's own gate then has a ready harness.
- **D-05:** The `szMODName` bug in `CreateRandomMap` (`RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp:971` stores the chapter name) is fixed in M3: it records the active mod's name and version, like the M1 save. A test covers it. The game's own generation path is re-checked by the random-missions tier.

### RMG composers (Containers, Graphs, Fields, Templates, Filters)
- **D-06:** Each composer is its own dockable ImGui window under a **Tools** menu, with New/Open/Save/Save As (the MFC per-composer File menu). Lists become tables with the MFC columns; the right-click popups become context menus with the same actions.
- **D-07:** The composers read and write the existing formats unchanged: `SRMContainer` ("Container"), `SRMGraph` ("Graph"), `SRMFieldSet` ("FieldSet"), `SRMTemplate` ("Template", plus the `QuickLoadMapInfo` entry), and `filter.xml`/`filterSetup.xml`. They use the `RandomMapGen` serialisers and their XML labels. Files the portable editor writes must load in the game's own `CreateRandomMap`, and every shipped file must load, save and reload equal. The shipped files are 43 templates, 102 graphs, 404 containers and 27 field sets.
- **D-08:** Composer lists are built by **scanning** the storage folders: `Scenarios/Containers`, `Graphs`, `FieldSets`, `Templates` and `Settings`. The MFC `Editor\Default*.xml` list files are not used: they are not shipped, so the MFC composers opened empty. Reversibility: cheap.
- **D-09:** Shipped RMG files are read-only, like shipped maps (spec D-18): Save becomes Save As. User-authored RMG files go to `<UserRoot>rmg/`, or `<UserRoot>mods/<Folder>/rmg/` with a mod active, mirroring the `Data` layout (`Scenarios/Templates/…`). That root is mounted as a storage layer over `Data` and the mod, for the editor only. Anything a user file references resolves through storage exactly like shipped data: patches, containers, graphs, field sets and profiles.
  - **Reversibility: costly.** The folder becomes where user templates accumulate; moving it later needs a migration.
- **D-10:** A container's patch must be reachable through the mounted storages. When the user picks a map outside them, the editor offers to copy it into `<rmg root>/Scenarios/Patches/<season>/` instead of refusing. This replaces the MFC refusal (`RMG_CreateContainerDialog.cpp:207`).
- **D-11:** The Graphs Composer's canvas is an ImGui draw-list canvas with the MFC interactions:
  - left-drag on empty space adds a node (at least 1 patch, no overlap);
  - dragging a node moves it, dragging its edge resizes it, and an overlap reverts;
  - Ctrl+drag from one node to another links them;
  - right-click opens Delete/Properties, double-click opens Properties;
  - the 1–32 patch zoom slider stays.

  Pan and zoom follow the map view (wheel/trackpad pan, Shift+wheel zoom).
- **D-12:** Every composer's **Check!** runs the MFC validation:
  - containers: re-load patches; season, folder and script ID/area consistency;
  - graphs: every node's container is valid; `nParts ≥ 8`;
  - fields: ranges, tile indices, object names against the catalogue.

  Findings appear in a results list. Unlike the MFC editor, fixes that remove entries are an explicit, undoable action, never silent. The Templates "Check!" is dead in the MFC editor; M3 makes it check the template's graphs, fields and VSOs with the same rules, and records this as an addition.
- **D-13:** Tools 0–3 become Tools → Export lists, writing `graphs_list.txt`, `contexts_list.txt`, `patches_list.txt` and `maps_list.txt` in the MFC format. They go to `<UserRoot>mapeditor/logs/`, not the data folder. `ID_TOOL_4` (re-save every shipped patch into `Data`) is unreachable in the MFC editor and would write `Data`, so it is not a feature.

### Minimap
- **D-14:** A dockable Minimap panel, toggled from View, with the MFC editor's live "Editor" mode:
  - terrain as the per-terrain-type average colour sampled from `<tileset>_h.dds`;
  - a height gradient while the Heights tool is active;
  - units in the 17 player colours;
  - fire-range areas when that layer is on;
  - the camera frame;
  - the patch grid.

  It is drawn CPU-side into a texture and refreshed incrementally after edits.
- **D-15:** Clicking or dragging on the minimap moves the camera there, keeping the MFC screen-centre offset. This also delivers the whole-map overview phase 3 deferred to "M3's minimap tools". Dragging the viewport rectangle itself is not added: it is commented out in the MFC editor.
- **D-16:** A "Game" mode shows the map's pre-built minimap image (`<map>.tga`, or the `_h.dds`), when one exists, as the MFC editor does.
- **D-17:** Map → Create Minimap Images is an explicit command, not part of Save: the preservation invariant holds, and the MFC editor did not do it on save either. It calls `CMapInfo::CreateMiniMapImage` on the saved map and writes `<map>_large` (512) and `<map>` (256) in both DDS and TGA, next to the map file. A shipped or never-saved map goes through Save As first. Afterwards the panel switches to Game mode.

### Terrain and map parity (heights, fields, fill, new map, update)
- **D-18:** **Heights tool** with the full MFC behaviour:
  - brush 2–16 using the `editor\profile.tga` profile;
  - left-drag raises, right-drag lowers;
  - middle-drag, or left+right held together, levels;
  - four level-to modes: Zero, Click Tile, Instant Average (default), Click Average;
  - Height Speed and Level ratio %;
  - an invalid-height stroke is rolled back unless Ctrl is held;
  - Generate heights: Hills `TG_FBM`, Rocks `TG_HYBRID`, Dunes `TG_RIDGED`, with granularity and min/max Z, after a confirmation;
  - Set Zero, after a confirmation.

  Every stroke and every generate is one undo step. On a trackpad, "middle-drag" is Alt+drag.
- **D-19:** **Preservation invariant, extended (spec change).** `altitudes` (heights and shades) become editable, under the same rule as tiles: a deterministic function over the affected region, with the expected value in tests built by the same function. The function is:
  1. set the heights;
  2. run `CMapInfo::UpdateTerrainShades` over the region grown by the shade kernel;
  3. push the region into the engine.

  Undo restores the recorded region. Outside edited regions, altitudes stay byte-identical. The MFC editor's whole-map shade recompute at save is **not** copied. The spec's "Terrain edits" and "What equivalent means" sections are updated in the first plan.
- **D-20:** **Update Map** (Ctrl+U) is one explicit, undoable command, run with a progress modal:
  - `UpdateAllHeights`;
  - full `UpdateTerrain`;
  - full `UpdateTerrainShades`;
  - object Z, roads and rivers Z;
  - snap-to-grid when Fit Objects To Grid is on.

  **Instant Update Map Mode** is a toggle that runs `ApplyPattern` and the object Z update on every height stroke. **Fit Objects To Grid** is a toggle, on by default like the MFC editor, that snaps non-unit objects on place and move with `FitVisOrigin2AIGrid`.
- **D-21:** **Fields tool** (Terrain › Fields):
  - the field set is chosen from the scanned `Scenarios/FieldSets` plus user field sets;
  - polygon Add / Select / Edit with the MFC keys: click adds a vertex, right-click removes the last, double-click or Enter closes, Insert/Delete on a dragged vertex, Esc clears;
  - Randomize Polygon (min length, width, disturbance);
  - Fill Terrain, Place Objects, Modify Heights, Check Passability, Update Map afterwards;
  - a season-mismatch confirmation.

  One application is one undo step. It records the tiles, crosses, altitudes and objects it changed.
- **D-22:** **Fill Entire Map** (Map menu): confirm, set every tile to the brush tile, one undo step. The MFC update-rect typo (`TemplateEditorFrame1.cpp:4951`) is not copied. The brush size becomes 1×1 to 16×16, even sizes included, to match the MFC toolbar combo; M1's slider stops at 9×9.
- **D-23:** **New Map** (File → New, Ctrl+N):
  - size X/Y 1–32 patches, with a Square lock;
  - season Summer/Winter/Africa/Spring;
  - name;
  - mod (installed / none / current).

  It is created in memory through the bridge (`CMapInfo::Create`, fill with the season's most common tile, zero altitudes, shades), and opens as a never-saved document (recovery-copy autosave, spec D-22). There is no tileset selector, as in the MFC editor.
- **D-24:** **Save format.** File → Save as XML / Save as BZM (Ctrl+X / Ctrl+B in the MFC editor; the portable editor uses Ctrl+Shift+X / Ctrl+Shift+B, because Ctrl+X is Cut in ImGui text fields). It converts the current map's format for this and later saves. The settings gain a default format for new maps, the MFC Options field.

### Objects, properties and players parity
- **D-25:** **Multi-selection:**
  - Ctrl+click adds to the selection;
  - a rubber band on empty ground selects by screen rectangle;
  - Ctrl+rubber band selects by tile rectangle, excluding entrenchments and bridges;
  - squads select whole;
  - dragging moves the whole selection, squads keeping their offsets;
  - Delete removes all;
  - right-click with the left button held cycles overlapping objects;
  - right-click alone deselects.

  Each is one undo step. The Selector layer draws the MFC double circles.
- **D-26:** **Properties panel** (replaces `CPropertieDialog`), showing per kind the fields `SEditorMApObject` exposes:
  - building: garrison units, Script ID, player, health %;
  - trench: units, Script ID;
  - unit: units, angle, Script ID, scenario unit (drawn blue), player (for a flag, the `Flag_<party>` swap), health, formation;
  - multi-selection: angle, Script ID, player, behaviour.

  Edits commit on deactivation as one undo step, like the M1 sound panel. Script ID and HP are overlay fields of the snapshot record (`nScriptID`, `fHP`), so preservation holds.
- **D-27:** **Links:**
  - dropping infantry onto a building, trench or vehicle garrisons it (slot and passenger limits);
  - dropping a tractor on artillery tows it;
  - dropping a train car on another couples them;
  - the cursor shows valid targets;
  - "unlink" is in the properties' units list.

  All of this uses `CheckForInserting`'s rules and edits `link.nLinkWith`. It lifts M1's refused delete for passengers: deleting a host unlinks and deletes its passengers as one undo step.
- **D-28:** **Placement direction wheel** in the object palette. It sets the angle new objects are placed at, and turns the ghost and the selection like the MFC `CDirectionButton`. Q/E stay.
- **D-29:** **Damage tool** (Map Tools): the percentage to add, default 10%. Left-click damages, right-click heals, middle-click (Alt+click) repairs fully; HP is clamped at ≥1% for units. The MFC null-pointer bug is not copied.
- **D-30:** **Players:** add a player (up to 16 plus neutral) and delete a player, with Insert/Delete and the 0/1 side keys in the list, in the M1 Diplomacy panel. Deleting re-maps that player's objects to neutral, one undo step. **Unit Creation Info** panel, per player:
  - party (from `partys.xml`);
  - aviation: five aircraft slots, each with name, formation size and count;
  - paratroop squad and count;
  - relax time;
  - appear points, edited on the map or in a points list.

  Validated like `MutableValidate`.
- **D-31:** **Object filters.** The palette gets the nine quick-toggle filter buttons (Ctrl+click assigns), the filter combo, and New/Delete Filter. The **Filters Composer** window edits filters (conditions of folder words) with Add/Delete/Rename. Filters are read from the shipped `Data/Editor/filter.xml` / `filterSetup.xml`; user filters are saved to `<UserRoot>mapeditor/filter.xml`. Filters feed the palette, the Fields Composer's objects tab and the fire-range filter.

### Layers, checks and the app shell
- **D-32:** **Layers** menu with every MFC toggle, bridged to `IScene::ToggleShow`/`IGFX::SetWireframe`:
  - Terrain, Grid, Wire Frame, Depth Complexity, Terrain Noise, Black Stripes;
  - Units, Objects, Bounding Boxes, Shadows, Haze;
  - War Fog (reachable only in code in the MFC editor, but it works there);
  - Selector;
  - Units Passability (`ToggleAIInfo`);
  - Unit Fire Ranges with the filter combo or "selected units".

  The state is re-applied after every open and new map, fixing the MFC desync. It is remembered in settings.
- **D-33:** **Check Map** (Map menu) runs the MFC checks:
  - duplicate objects;
  - invalid or duplicate links;
  - player index out of range;
  - unknown unit-creation party.

  It adds unknown object types (the editor's own M1 warning, which replaces the MFC `RemoveNonExistingObjects`). Results go to a panel where clicking an entry jumps to it, and to `<UserRoot>mapeditor/logs/checkmap_log.txt`. "Fix all" is one undoable command. Save never fixes silently, unlike the MFC `CheckMap(false)`: it only puts a status-bar note when checks fail.
- **D-34:** **App shell parity:**
  - Tools → Options: extra game command-line parameters for Test in game (the MFC `szGameParameters`), and the default save format.
  - View menu: show/hide every panel, the status bar, and a "Reset layout". ImGui docking replaces MFC toolbar customisation.
  - Status bar: VIS and SCRIPT coordinates; object name, Script ID, position and box, or "N objects selected".
  - Title: name, extension, `*` when modified, size in patches, mod key.
  - Drag and drop a map file onto the window opens it.
  - A map path on the command line opens at startup.
  - **Single instance:** a second launch hands its path to the running editor through a per-user local socket or pipe, then exits.
  - Help: a Help → Keys and tools window (the `.chm` is not shipped and is Windows-only), and About.
  - Arrow keys pan: M1 already does.
- **D-35:** **Tile properties:** read-only name and variant count in the tile palette's context menu, including tile index 0 (the MFC `> 0` bug is not copied).

### Parity proof and deleting the MFC editor
- **D-36:** `05-PARITY.md` is the checklist of record. Every MFC menu item, toolbar button, workspace tab, tool state, dialog, key binding and on-load/on-save behaviour is one row, with its owner (M1/M2/M3), its plan, and its **evidence**: a test name, an automated `BK_EDITOR_AUTO` scenario, or a hand-try note. A row closes only with evidence. "Not a feature" rows need the file:line proving the MFC code is dead or unreachable.
- **D-37:** Every M3 feature gets:
  - a core-tier test (fake bridge);
  - where it touches map data, a map-file-tier overlay test with the expected-value builder;
  - where it touches the engine, an engine-tier test;
  - for each composer and each tool, at least one `BK_EDITOR_AUTO` step in a new `zig build map-editor-m3-auto` scenario that runs locally (macOS arm64 and Windows on win-home).

  The CI gate stays as for M1: core and map-file on all targets, engine on macOS arm64 and Windows-MSVC.
- **D-38:** The deletion plan runs last and only after phase 4's rows are closed. It removes:
  - `Sources/src/MapEditor/`;
  - the checked-in `Sources/src/bin/MapEditor.exe`;
  - the `MapEditor` project in `Sources/src/A7.sln`;
  - the `.vscode/tasks.json` "Build MapEditor (Debug)" task;
  - the `Editors/MapEditor.exe` entry in `tools/zig/stage.zig` and `tools/zig/game_install.ps1`;
  - the MapEditor assertion in `tools/openspy/check_no_gamespy_runtime.ps1`.

  It keeps `Sources/src/RandomMapGen` (the game uses it) and `Data/Editor` (the portable editor reads its filters and markers). Code comments that cite `MapEditor/…` file:line are rewritten to name the portable code, or to cite the last commit that had the file. The spec's editor-set table marks M3 done.
- **D-39:** Plan split, 11 plans:
  1. Foundations: spec changes for D-19; bridge region-edit primitive for altitudes; New Map; save format; brush 1–16; status bar and title; parity checklist verified against the MFC source.
  2. Heights tool, Update Map, Instant Update, Fit To Grid, Fill Entire Map, tile properties.
  3. Object filters, Filters Composer, then the Fields tool.
  4. Multi-selection, properties panel, links, direction wheel, damage tool.
  5. Players add/delete, Unit Creation Info, Check Map.
  6. Layers and fire ranges.
  7. Minimap panel and minimap images.
  8. Create Random Map (bridge, dialog, progress, seed, D-05 fix, determinism test) and Export lists.
  9. Containers Composer and Graphs Composer, plus the user RMG storage root (D-09/D-10).
  10. Fields Composer and Templates Composer.
  11. App shell (Options, View, drag-drop, single instance, Help/About), full parity verification, hand try, deleting the MFC editor.

  Waves:
  - Plans 2–8 depend only on plan 1.
  - Plan 9 depends on plan 8.
  - Plan 10 depends on plans 3 and 9.
  - Plan 11 depends on all of them and on phase 4.

### Exit criteria for M3 (testable)
- **D-40:** M3 is done when all of these hold:
  1. Every row of `05-PARITY.md` is closed with evidence. "Not a feature" rows cite the dead or unreachable code.
  2. `zig build test` (core and map-file tiers) passes on all six CI targets.
  3. The engine tier passes on the macOS arm64 and Windows-MSVC CI runners. It includes new tests for heights, fields, fill, Update Map, multi-select move and delete, properties, links, damage, players, unit creation and New Map, each saved, read back and compared with the expected-value builder.
  4. The composer round trip passes: every shipped template, graph, container and field set loads, saves and reloads equal through the portable composer code. The data-only tier runs this on all five engine-C++ targets.
  5. A template, graph, container and field set authored in the portable composers, with a seed fixed, generates a map through the editor's Create Random Map. Generating twice gives byte-identical `.bzm` files. The game loads that map under `BK_AUTO_UI` (the game-reads-it tier) and exits cleanly.
  6. `CreateMiniMapImage` from the editor writes the four images next to a saved user map. The Minimap panel's click moves the camera. This is checked by a `BK_EDITOR_AUTO` shot comparison.
  7. `zig build map-editor-m3-auto` passes locally on macOS arm64 and on win-home. It covers one scripted step per tool, composer and menu command.
  8. The full sweep `test-map-files-all` (1,755 maps) still round-trips with 0 FAIL: M3 did not weaken preservation.
  9. `Sources/src/MapEditor/`, `Sources/src/bin/MapEditor.exe` and every build, packaging, CI and VS Code reference to them are gone. `git grep -n 'src/MapEditor\|MapEditor.vcxproj\|Editors/MapEditor'` finds only history in `.planning`/`docs/superpowers/plans`. The game, the editor and every CI job still build.
  10. Johannes's hand try on the release build approves M3.

### Claude's Discretion
- Exact ImGui layouts, window names and shortcut keys where the MFC editor has none, and the tick rate of the minimap refresh.
- The single-instance IPC mechanism per platform (Unix domain socket or named pipe).
- The internal form of the region-edit primitive for altitudes, as long as it is deterministic and tested with the same function (D-19).
- Whether a composer shares one generic "list + properties" widget.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Map editor design and history
- `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` — the spec: architecture, preservation invariant, terrain-edit function, saving (snapshot and overlay), user data folders, test tiers. M3 changes "Terrain edits" (D-19) and the editor-set table (D-38).
- `.planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md` — M1 decisions D-01..D-29 (test launch, save rules, user maps folder, recovery copies, mods).
- `.planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/` — phase 4's context and plans, once written. They own the M2 rows of `05-PARITY.md`.
- `.planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md` — the feature-by-feature parity checklist (D-36).
- `.planning/phases/999.1-random-map-generation-fast-polygon-fill/999.1-NOTES.md` — the RMG fill cost and its determinism gate (D-04).
- `docs/superpowers/specs/2026-09-25-revive-random-missions-design.md` — how the game generates random missions and the random-missions test tier.

### MFC editor (the reference until deleted)
- `Sources/src/MapEditor/editor.rc` — menus (`IDR_EDITORTYPE`, composer popups), dialogs and their controls, accelerators (2386).
- `Sources/src/MapEditor/TemplateEditorFrame1.cpp` — command handlers, load (1357–2113), save (2934–3291), Check Map (5952–6390), Update Map (5138), Create Random Map (207–298), CreateMiniMap (300).
- `Sources/src/MapEditor/MainFrm.cpp` — the real toolbars (33–101), brush combo, Tools 0–4 (939–1138), single instance, drag and drop.
- `Sources/src/MapEditor/MapEditorBarWnd.cpp` — workspace panes and tabs. `DrawShadeState.cpp`, `TabTerrainAltitudesDialog.cpp` (heights), `StateTerrainFields.cpp`, `TabTerrainFieldsDialog.cpp` (fields), `ObjectPlacerState.cpp` (selection, links, keys), `SEditorMApObject.cpp` (property sets), `MapToolState.cpp` (damage), `UnitCreation.cpp`, `CreateFilterDialog.cpp`, `MiniMapDialog.cpp`, `MiniMapTypes.cpp`, `RMG_*.cpp`, `CreateRandomMapDialog.cpp`.

### Engine
- `Sources/src/RandomMapGen/RMG_Types.h`, `RMG_Methods.cpp`, `RMG_Consts.cpp` — RMG structs, serialisers, XML labels.
- `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp` — `CreateRandomMap` (793), `FillTileSet`/`FillObjectSet`/`FillProfilePattern`, the `szMODName` bug (971).
- `Sources/src/RandomMapGen/MapInfo_StaticMethods_MiniMapCreation.cpp` — `CreateMiniMapImage` (52), `Data/Terrain/sets/*/minimap.xml`.
- `Sources/src/RandomMapGen/MapInfo_StaticMethods.cpp` — `UpdateTerrainShades`, `UpdateTerrainCrosses`.
- `Sources/src/GameTT/Mission.cpp:169`, `Sources/src/Main/RandomMapHelper.cpp:75`, `tools/zig/random_missions_test.cpp` — the existing callers of `CreateRandomMap`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Sources/src/EditorBridge` (`bridge.h`, 722 lines of C ABI; `session.cpp`) provides the session snapshot/working copy, the paint region record and undo (`SPaintRecord`), tombstones, `BkEditorCaptureFrame`, `BkEditorWorldToScreen`, `BkEditorSetOverlay` and the object catalogue. M3 adds entry points beside these, all through `Guarded`.
- `Sources/src/MapFile` holds the reader, writer, comparator and `MapOverlay` (the paint undo record). It gets the altitude region record for D-19.
- `Sources/editor/core`: `history.zig` (undo stack, drag merging), `tools.zig` (brush), `document.zig`, `settings.zig` (lenient `key=value`: add the layers, default format, game parameters and filters path), `files.zig`/`shipped.zig` (read-only shipped data, user folders), and `fake_bridge.zig` for core tests.
- `Sources/editor/app`:
  - `panels.zig`/`panels_logic.zig`: the menu bar with File/Edit/Tools/View/Test, `PathSlot` file dialogs, the Sounds panel's commit-on-deactivate pattern for D-26;
  - `view.zig`/`view_math.zig`: input routing and the camera;
  - `pictures.zig`: GPU texture upload for icons, reused by the minimap texture and tile/object thumbnails;
  - `auto.zig`: `BK_EDITOR_AUTO` verbs;
  - `testlaunch.zig`: the Test in game path, which takes the Options game parameters.
- `Sources/src/RandomMapGen` is already a portable static library linked into the editor (`build.zig` `addMapEditor`, 2198). `addRandomMissionsTest` (2200) already drives `CreateRandomMap` headless through the bridge's data-only startup: it is the model for the determinism test.
- `Data/Editor/filter.xml`, `filterSetup.xml` and the marker sprites (`Data/Editor/Fire`, `Krest`, `Locator`) are shipped data the portable editor can read.

### Established Patterns
- Bridge calls are guarded, status-returning, and document their units (world vs map vs VIS tiles).
- The core is std-only; SDL, ImGui and the C ABI live in the app.
- Every edit is a command with do/undo; the engine is updated immediately.
- Shipped data is read-only (Save → Save As); user data lives under `<UserRoot>`, mod-specific under `<UserRoot>mods/<Folder>/`.
- Tests run in tiers (core, map file, engine, app); test artefacts go to `zig-out/local-test`.
- C++: never `std::min`/`std::max`; use `Min`/`Max` from `Misc/Tools.h`.
- Never `zig fmt build.zig`. GOG files and the AchtungPanzer2 mod are never committed.

### Integration Points
- New menus: File (New, Create Random Map, Save as XML/BZM), Map (Fill, Update, Instant Update, Fit To Grid, Check Map, Unit Creation, Create Minimap Images), Layers, Tools (the composers, Export lists, Options), View (panels), Help.
- Tools palette: Heights, Fields, Damage, beside Select/Brush/Place.
- Storage: the user RMG root (D-09) is mounted in `BkEditorStart`/`BkEditorSetMod` next to the mod storage.
- Windows: every plan's tests also run on win-home (`ssh win-home`, the repo at `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg`).

</code_context>

<specifics>
## Specific Ideas

- Johannes: "all features implemented". Nothing the MFC editor can do is deferred. A row may be "not a feature" only when the MFC code is dead or unreachable, with file:line evidence.
- Where the MFC editor has a bug, the portable editor does the evident intent, and the parity row notes it:
  - the fill-rect typo;
  - tile 0's properties;
  - the damage-tool null pointer;
  - layer desync after load;
  - `szMODName` in `CreateRandomMap`;
  - the templates "Check!" doing nothing.
- The minimap is also the whole-map overview phase 3 deferred.

</specifics>

<deferred>
## Deferred Ideas

- The game's Custom Mission list offering user-generated random maps or user templates. This is carried over from phase 3; it is a game feature, not an editor one.
- 999.1's polygon-fill speed-up (a backlog phase of its own; M3 only supplies the determinism harness, D-04).
- Free camera rotation (D-12 of phase 3) is phase 4's revisit, not M3's.
- Features that are dead or unreachable in the MFC editor are listed in `05-PARITY.md` as "not a feature", not deferred. They are: undo, cut, copy and paste; storage coverage; the Sounds, Forests and RMG tabs; `ID_TOOL_4`; `CPESelectStringsDialog`; the hidden noise radios.

</deferred>

---

*Phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity*
*Context gathered: 2026-09-30*

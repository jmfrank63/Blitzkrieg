# Phase 4: Map editor M2: roads, rivers, bridges, AI groups, scripts - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Milestone M2 of the portable map editor (`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`, "The editor set"). M1 (phase 3) keeps these parts of a map unchanged on save but cannot edit them. M2 makes all of them editable, at full parity with the MFC map editor (`Sources/src/MapEditor`):

- roads (`roads3`) and rivers (`rivers`), including river passability in the AI;
- bridges, drawn as whole span groups. This includes "rotating" a bridge, which was deferred from M1, and the MFC "built during play" toggle;
- entrenchments (`entrenchments`: arcs, terminators, fireplaces);
- fences (drawn in a line along one axis);
- script IDs and reinforcement groups (`nScriptID`, `reinforcements`);
- start commands (`startCommandsList`) and reserve (artillery) positions (`reservePositionsList`);
- AI general data (`aiGeneralMapInfo`: mobile script IDs, parcels, reinforce points);
- the map's script file (`szScriptFile`) and script areas (`scriptAreas`);
- player camera anchors (`playersCameraAnchors`, `vCameraAnchor`).

M2 also:
- makes deleting an object update the references to it, as the MFC editor does. This fixes `NMapOverlay::FindReferences`: reinforcement groups hold script IDs, not link IDs, and entrenchments, reserve positions and mobile script IDs are not checked today;
- stops the object palette from placing loose bridge spans, entrenchment segments and fences;
- closes the free-camera-rotation question (phase 3's D-12) with a written reason.

All of this keeps M1's standards: one undo step per edit, the snapshot-plus-overlay save with read-back verification, test in game, and the core / map file / engine / game / app test tiers and the CI gate.

The M2/M3 boundary follows `05-PARITY.md` (phase 5's checklist of record). M3 owns:
- unit creation info (aviation, paratroopers, appear points);
- garrison / tow / couple links;
- adding and deleting players;
- multi-selection and the full properties panel;
- the damage tool;
- heights, shades and Update Map;
- terrain fields and object filters;
- layers and the minimap;
- the RMG composers and random map generation;
- Check Map;
- New Map.

This phase owns the M2 rows of `05-PARITY.md` and closes them with evidence. `04-PARITY.md` lists them per plan.

</domain>

<decisions>
## Implementation Decisions

### Saving, undo and references (applies to every M2 area)
- **D-01:** **The preservation invariant extends record by record.** Each M2 collection is laid over the snapshot the way objects are today:
  - an untouched collection is written from the snapshot byte for byte;
  - in an edited collection, untouched records stay byte-identical, edited records are replaced, new records are appended, and deleted records are removed;
  - list order is kept (`startCommandsList` and `reservePositionsList` are `std::list`; `reinforcements.groups` is written sorted by key).

  The bridge keeps its own copy of each collection, as it does for terrain. The engine is never the source of the saved data. The MFC editor's save-time rewrites are not copied:
  - `HandOutLinks` link renumbering;
  - the silent `CheckMap` fixes;
  - the full shade recompute;
  - the `playersCameraAnchors[0]` overwrite.

  The spec's "Saving" and "What equivalent means" sections are updated to say so.
- **D-02:** **Commands.** Every M2 edit is one command through the M1 chain: tool → `Editor` (history.reserve, bridge call, record) → bridge VTable → C ABI (`Guarded`) → session. Each command records the whole before-record and after-record of what it changed. Undo writes the before-record back through the same bridge call, so the bridge stays thin. A drag (moving a point, dragging a width or a radius) merges into one step by gesture, like the brush.
- **D-03:** **Derived data is computed once, when the edit happens, and stored in the command.** This covers:
  - a road's or river's sampled `points`;
  - a bridge's span positions and frame indices;
  - an entrenchment's pieces.

  Undo and redo restore the stored records and never recompute them. Engine calls that use `rand()` are kept off the saved data:
  - `ITerrainEditor::AddRoad` / `AddRiver` pick a random `nID` (`Scene/TerrainEditor.cpp:242-318`). The bridge assigns the saved `nID` itself (above every `nID` in use) and keeps a map from saved `nID` to engine `nID`, the way it maps link IDs.
  - Bridge frame indices are chosen deterministically. The MFC editor picks begin, line and end variants at random (`RoadDrawState.cpp:650-660`).
- **D-04:** **Deleting an object updates what refers to it, as the MFC editor does** (`ObjectPlacerState.cpp:627-743`). The M1 refusal is replaced by one compound undo step that:
  - removes the object's link ID from start commands (`linkID` and `unitLinkIDs`), dropping a command left with no units, as the MFC save does;
  - removes reserve positions that name it as artillery, and clears the truck field of positions that name it as the truck (`RMGC_INVALID`, as the MFC save does);
  - leaves script-ID references alone: reinforcement groups and `mobileScriptIDs` refer to script IDs, which other objects may share and Lua scripts may use. The status bar notes that the script ID is still referenced when the last object carrying it is deleted.

  Undo restores the object and every reference. Two refusals stay:
  - an object with a passenger (`nLinkWith`) still cannot be deleted; links are M3's D-27;
  - bridge spans and entrenchment pieces are never deleted singly (they are not pickable singly; see D-11).

  `FindReferences` is fixed:
  - reinforcement groups are matched by script ID;
  - entrenchment `sections`, reserve positions and `mobileScriptIDs` are added.

  It becomes the source of the status-bar summary of what a delete changed.
- **D-05:** **The object palette stops placing loose spans, segments and fences.** Bridge (6) and entrenchment (4) types come out of the palette (`panels_logic.zig:81-86`, `catalogue.cpp:113`). So do fences. Each is placed only by its own tool (D-10, D-13, D-14), which writes the grouping records the game needs (`LoadBridges` and `LoadEntrenchments` assert every link, `AILogicInternal.cpp:607-646`).
- **D-06:** **M2 markers are drawn by the app over the engine frame**, through ImGui's background draw list and `BkEditorWorldToScreen`, the same pattern as the M1 sound markers and brush outline (`view.zig` overlay). This covers:
  - road and river control points, width handles and centre lines;
  - script areas;
  - parcels and reinforce points;
  - start command lines;
  - reserve positions;
  - camera anchors;
  - the selected bridge, entrenchment or fence outline.

  View → Markers switches each kind on or off. Markers for the active tool are always on. Scene layer toggles (units, objects, grid and so on) stay with M3's layers (05-PARITY L1–L15).

### Roads and rivers (vector stripe objects)
- **D-07:** **Editing model as in the MFC editor.** The user edits the control polyline (`controlpoints`), plus width and opacity at key points. The sampled `points` come from the same code the MFC editor runs, taken from `RandomMapGen/VSO_*.cpp` and `ITerrainEditor`:
  - `SampleCurve` (B-spline, mirrored ends) and `SliceSpline` every 30 Vis, one key point per control point;
  - `SmoothCurveWidth` between key points;
  - `UpdateZ` from the altitudes.

  Both control points and sampled points are saved, as the MFC editor saves them. Only an edited road or river is resampled; nothing is resampled at save (the MFC editor doesn't either, `TemplateEditorFrame1.cpp:3189-3196`).
- **D-08:** **One Roads & Rivers tool, with a Road / River kind switch.** It has:
  - a type list, built from the season's `Roads3D\` and `Rivers\` descriptors (`TemplateEditorFrame1.cpp:1719-1731`);
  - width 1..16 (default 3, `fWidth = w * fWorldCellSize / 2`);
  - opacity 0..100 %;
  - a width mode: single point / from this point on / all (`CW_SINGLE/MULTI/ALL`).

  Gestures follow `VectorStripeObjectsState.cpp`:
  - click adds a point; right-click removes the last point; double-click or Enter finishes; Esc cancels;
  - on a selected road: drag a control point; drag a width handle; right-drag vertically sets opacity (100 px = 1.0);
  - Insert adds a midpoint; Delete removes a point (minimum 2) or the whole road.

  One undo step per finished road, per drag and per key.
- **D-09:** **The engine is kept in step, as in the MFC editor.**
  - `AddRoad` / `UpdateRoad` / `RemoveRoad` and `AddRiver` / `UpdateRiver` / `RemoveRiver` redraw the stripe.
  - Rivers also update AI passability (`IAIEditor::DeleteRiver` when a drag starts, `AddRiver` when it ends).
  - Roads never update the AI, which matches the MFC editor. The game computes road passability when it loads the map (`AIStaticMap.cpp:213-236`).
  - M2 never changes altitudes or shades; that is M3's D-19/D-20. `UpdateZ` only fits the road to the heights already there.

  The 04-01 capture measures that GFXGPU draws roads and rivers, including the animated river layer, before the tool is built (memory: measure, don't guess).

### Bridges, entrenchments, fences
- **D-10:** **The Bridge tool** is the MFC Bridges tab:
  - a list of every `SGVOGT_BRIDGE` descriptor with its picture;
  - a ghost that follows the pointer;
  - a drag snapped to one axis, which produces begin, middle and end spans (`GetPointsForBridge`, `RoadDrawState.cpp:71-104`, with the ±0.1 nudges and `FitVisOrigin2AIGrid`);
  - an all-or-nothing commit: new span objects (`nDir` 0, fresh link IDs, packed frame index) plus a new `bridges[i]` entry, as one undo step.

  The drag axis must match the chosen variant's direction; any other drag is refused with a status note.
- **D-11:** **Bridges are selected, rotated and deleted as wholes.**
  - Clicking a span selects its whole `bridges[i]` group and outlines it (`ObjectAt` stops skipping bridges and returns the group).
  - Delete removes the whole group and its entry.
  - "Rotate" (Q/E, or the tool's Rotate button) swaps the variant for its `_01` / `_02` partner: every family ships both, `_01` horizontal and `_02` vertical (`RPGStats.h:1313-1314`). The bridge is rebuilt about the same centre, along the other axis, with the same number of spans, as one undo step.
  - A bridge with no partner variant, or whose rotated spans would leave the map, is refused with a status note.
  - Moving a bridge means drawing it again, as in the MFC editor.
- **D-12:** **"Built during play".** Enter, or a checkbox in the properties panel, toggles a selected bridge between intact (`fHP` 1) and built during play (`fHP` −1 on every span). As in the MFC editor (`RoadDrawState.cpp:1244-1272`), this is allowed only for `WoodenBig_Heavy_*` bridges. The engine shows such a bridge intact and marked, as M1 already does (`futureBuildLinkIDs`). 05-PARITY row VO3's wording ("destroyed/intact") is corrected to this.
- **D-13:** **The Entrenchment tool** ports the MFC trench builder (`RoadDrawState.cpp:247-465, 1135-1216, 1384-1585`) into the bridge:
  - the user clicks a polyline; right-click cancels; double-click commits;
  - terminators go at both ends (the start one turned +π);
  - straight runs alternate fireplace and line pieces;
  - arcs are placed at turns;
  - a new section starts after an arc;
  - the player comes from the tool's player.

  The commit is one `entrenchments[i]` entry plus its piece objects, as one undo step. The preview is live while drawing. Clicking a piece selects the whole entrenchment; hovering highlights it; Delete removes it whole. In the engine, pieces are placed as today (`PlaceOneObject`). Script ID and garrisoned units on a trench are M3's properties and links (05-PARITY O18).
- **D-14:** **The Fence tool** is the MFC Fences tab:
  - a fence list and a ghost;
  - a drag along one axis places one fence every second AI tile;
  - the direction is encoded in the frame index (`GetCenterIndex`: 1/3 horizontal, 0/2 vertical, by drag direction, with the ±2-tile offset; Ctrl flips a single fence);
  - one undo step per drag.

  Once placed, fences are ordinary objects: select, move, delete (`RoadDrawState.cpp:519-622, 841-898, 1034-1096`).

### AI and unit logic
- **D-15:** **Script IDs.** M2 adds a Script ID field (with its command) to the existing single-selection Properties panel. Reinforcement groups cannot be used without it. M3's D-26 extends the panel (multi-selection, health, formation) and reuses the same command. An object's script ID is −1 (none) or 0..32000.
- **D-16:** **The Groups panel** is the MFC Group Manager (`GroupManagerDialog.cpp`, `GetGroupID.cpp`, `EnterScriptIDDialog.cpp`):
  - a list of reinforcement groups by group ID; New (the first unused ID from 0 up); Delete;
  - each group's script IDs: add (duplicates skipped) and remove;
  - "Hide checked": objects whose script ID belongs to a checked group are hidden in the view, to match the game, which does not place them but holds them for the group (`AILogicInternal.cpp:463-486`);
  - "Select objects": selects the map objects that carry a group's script ID, for inspection.

  Each change is one undo step.
- **D-17:** **Start commands** (Unit menu → Add start command, and the Start Commands panel; `AIStartCommand.cpp`, `AIStartCommandsDialog.cpp`, `ObjectPlacerState.cpp:753-771`).
  - Adding a command takes the selected unit and the command type from `Data/Editor/actions.ini` (default STOP).
  - The target is set by clicking the map (`vPos`, converted Vis → AI as the MFC editor does) or a unit (`linkID`).
  - `fNumber` is a numeric field.
  - A soldier stands for his squad's link ID, deduplicated.
  - The panel lists commands, edits them and deletes them (Delete key).
  - Red lines on the map run from the units to the target.
  - A command left with no units is removed (D-04).
  - `fromExplosion` is kept from the file and not editable, as in the MFC editor.

  Adding more than one unit at a time needs M3's multi-selection. Until then, units are added one by one with "Add selected unit".
- **D-18:** **Reserve positions** (Unit menu → Artillery positions mode; `ObjectPlacerState.cpp:525-617`, `TemplateEditorFrame1.cpp:4122-4175, 2958-2990`):
  - click an artillery unit, then optionally a truck (the MFC stats and towing-force checks), then the ground for `vPos`;
  - Enter commits;
  - positions are listed, drawn (a line from gun to truck to spot) and deletable;
  - a squad gives link ID 0 and a missing truck gives `RMGC_INVALID`, as the MFC editor writes them.

  One undo step per position.
- **D-19:** **The AI General panel and tool** (`StateAIGeneral.cpp`, `TabAIGeneralDialog.cpp`) edits one side at a time (side radios):
  - **Mobile script IDs:** add and remove.
  - **Parcels on the map:** clicking outside every parcel makes a Defence parcel with radius 4 AI cells and direction 0. Clicking inside one adds a reinforce point, stored relative to the parcel centre and rotated by minus the defence direction.
  - **Dragging** changes the centre, the radius (with the MFC minimum), the defence direction, a point's position and a point's `wDir`.
  - **Keys:** Enter / Insert switch the parcel type; Delete removes a parcel or point.
  - **Drawing:** parcels are circles with a direction arrow, coloured by type; points are arrows.

  One undo step per click, drag or key. The game rotates reinforce points the same way (`GeneralIntendant.cpp:480-484`); the engine test places a parcel and checks it through the game reader.

### Scripts, script areas, camera anchors
- **D-20:** **Script file** (Map → Script…).
  - The game loads `<map directory>\<script file basename>.lua` (`iMissionInternal.cpp:1436-1444`, `Scripts.cpp:111-139`), so the script lives beside the map.
  - The dialog lists the `.lua` files beside the map and offers None. "Choose other…" copies a chosen file beside the map, asking before it overwrites.
  - `szScriptFile` is written in the form the MFC editor writes, taken from `MapOptionsDialog.cpp` and the shipped maps in research. A value read from the file is kept exactly until the user changes it.
  - "Open script" opens it in the system's default editor (`SDL_OpenURL`).
  - A missing file is a warning, never an error.
  - A shipped read-only map gets its script copied along when it is saved elsewhere (Save As), after asking.
  - **Test in game copies the script beside `mapeditor_test.bzm`** in the test generated-data root. Without that, a tested map would run without its script.
- **D-21:** **The Script Areas tool and panel** (`MapToolState.cpp:188-274`, `TabToolsDialog.cpp`, `AreaNameDialog.cpp`):
  - drag a rectangle (centre plus half-size) or a circle (centre plus radius), then name it;
  - names must be non-empty and unique, since Lua refers to areas by name;
  - the list centres the camera on a click; Rename; Delete;
  - areas can be moved and resized by handles on the map.

  One undo step each. The file holds AI units (the game uses them raw, `Scripts.cpp:106-110`), and the bridge's copy stays in AI units. Areas are drawn through AI → Vis. A new or edited area is converted Vis → AI once, with the MFC rule (truncation, radius through x), so a map written by the MFC editor would match. Untouched areas stay byte-exact.
- **D-22:** **Camera anchors** (Map → Player camera, `TemplateEditorFrame1.cpp:5035-5051`):
  - "Set camera for player N" and "Set neutral camera" store the view centre (Vis) into `playersCameraAnchors[N]` or `vCameraAnchor`;
  - a list shows each anchor, with Go to and Clear;
  - markers are drawn on the map;
  - one undo step each.

  The game uses `playersCameraAnchors[user]`, then `vCameraAnchor`, then the centre of the units (`iMissionInternal.cpp:1484-1498`). Test in game therefore starts at player 0's anchor.

### Camera rotation (phase 3's D-12, revisited)
- **D-23:** **Free camera rotation is closed as not planned for the map editor; the reason is recorded in the spec.** The 03-06 measurement showed the lower half of the frame going from 0.5 % black at the game's own 45° to 9.4 / 94.0 / 99.2 / 89.8 % black at +30/+90/+180/+270, because `CTerrain::MovePatches` (`Scene/TerrainInternal.cpp:237-256`) lays the terrain out on a fixed isometric screen grid.
  - Rewriting that through the view matrix would still not deliver the feature. The tile art, and every building, tree and infantry sprite, is pre-rendered from one isometric angle with baked light and shadow (`SGVOT_SPRITE`, `Main/GameDB.h:14`). A rotated view would show the same face of each building, lit from the wrong side. The purpose of rotation, seeing behind buildings, needs new art, not engine work.
  - That purpose is met without rotation. M2's markers (D-06) are drawn on top of every sprite, so roads, areas and parcels behind buildings stay visible and editable. M3's Units/Objects layer toggles (05-PARITY L7/L8) hide the sprites in the way.
  - `BkEditorSetYaw` and `TestYawMeasurement` stay in the bridge as the evidence. No input is bound to them.
  - Any wish to reopen this goes to a renderer/art phase, not to the editor.

### Plan split and exit criteria
- **D-24:** **Plan split: 8 plans in 3 waves.** Wave 2's plans are independent in logic, but they share `bridge.h`, `history.zig`, `panels.zig` and `auto.zig`. Execute them one after another in the shared worktree, in the order listed.
  - **Wave 1**
    - **04-01 Foundations:**
      - spec changes (M2 scope, D-01 overlay rules, D-23 D-12 closure, the D-12 wording fix for VO3);
      - the record-level overlay and expected-value builder for every M2 collection in `MapFile`;
      - the `FindReferences` fix and the compound cascade delete (D-04);
      - the generic record command in the core and the fake bridge;
      - the app's marker layer and tool registry (D-06);
      - `BK_EDITOR_AUTO` verbs for tools, keys and panels (e.g. `tool=`, `panel=`, `rclick=`, `dblclick=`, `text=`);
      - the palette filter (D-05);
      - a capture proving roads and rivers render on GFXGPU.
  - **Wave 2**
    - **04-02 Roads and rivers** (D-07–D-09).
    - **04-03 Bridges and fences** (D-10–D-12, D-14).
    - **04-04 Entrenchments** (D-13).
    - **04-05 Script IDs, reinforcement groups, script file, script areas, camera anchors** (D-15, D-16, D-20–D-22).
    - **04-06 Start commands and reserve positions** (D-17, D-18).
    - **04-07 AI general** (D-19).

    Each plan brings its own core, map-file, engine and app-scenario tests.
  - **Wave 3**
    - **04-08 Integration and exit:**
      - the `map-editor-game-reads-it` M2 scenario;
      - the `map-editor-auto` M2 scenario with shot comparison;
      - the edit/undo sweeps over shipped maps;
      - the CI gate (six jobs), with a Windows run on win-home;
      - evidence for the M2 rows of `05-PARITY.md`;
      - Johannes's hand try on the release build (a checkpoint).
- **D-25:** **M2 exit criteria.** Each must be shown with evidence:
  1. **Core tier:** every M2 command has a do/undo/redo round trip against the fake bridge. The cascade delete (D-04) restores every reference on undo. A refused edit (wrong bridge axis, no partner variant, duplicate area name, off-map) leaves the document and history unchanged. Runs on all six CI targets.
  2. **Map-file tier:** for each M2 collection, scripted overlay edits (add, edit, delete) equal the expected-value builder's map. An untouched collection stays byte-identical. The cascade delete matches the builder. Runs on the five CI targets that build the engine C++.
  3. **Engine tier:** through the real `ITerrainEditor` / `IAIEditor`, each area is edited, saved, read back and compared with the expected map, and the engine state matches:
     - add, edit and delete a road and a river; the river's passability appears and disappears;
     - draw, rotate, toggle and delete a bridge;
     - draw and delete an entrenchment;
     - draw fences;
     - set script IDs and groups;
     - add a start command and a reserve position;
     - add an AI parcel and a point;
     - add a script area and a camera anchor.

     Runs on macOS arm64 and Windows-MSVC.
  4. **Preservation:** `test-map-files-all` still round-trips 1,755 / 1,755 shipped maps unchanged. A new local sweep opens every shipped map in `Data/Maps` that has roads, rivers, bridges, entrenchments, groups, start commands, reserve positions, AI parcels or script areas, applies one edit of each kind present, undoes it all, and saves: the result must be byte-identical to an unedited save.
  5. **Game reads it:** a map edited by the editor is loaded by the game under `BK_AUTO_UI`, shot, and exits cleanly. The map has a new road, a new river, a new bridge (one rotated, one built during play), an entrenchment, fences, a reinforcement group with a script ID, a start command, a reserve position, an AI parcel, a named script area, a camera anchor, and a script file beside it that refers to the group and the area. The checks are:
     - the game's log or trace shows the script ran and found the area and the group;
     - the camera starts at the anchor;
     - the shot shows the road and the bridge.
  6. **Editor app:** a `map-editor-auto` M2 scenario draws one of each, undoes and redoes, saves, compares shots, runs Test in game with the script copied along, and quits.
  7. **CI:** all six jobs are green. The engine tier passes on macOS arm64 and Windows-MSVC; the rest report skipped, never passed.
  8. **Parity:** every M2 row of `05-PARITY.md` (M2, M6, U1–U3, O16 references, O18 trench drawing, VO1–VO7, MT2, G1, AI1, S4 bridge spans) is closed with evidence.
  9. **Hand try:** Johannes approves a hand try on the release build (`--release=fast`).

### Claude's Discretion
- The C ABI shape per collection: record get/set/insert/delete functions, or one function per edit, as long as every call is `Guarded` and undo is "write the before-record back".
- Marker colours, handle sizes and the exact panel layouts, following the M1 panels' style.
- Key bindings not fixed by the MFC editor (e.g. the tool shortcut keys), as long as nothing collides with M1 keys or the phase 5 plans' published keys.
- How the trench builder's geometry code is shared between the bridge and any test (lifted from `RoadDrawState.cpp` into a GFX-free C++ unit is preferred, so the map-file tier can build expected values with the same code, the "same function" rule).
- The deterministic choice of bridge frame indices (D-03).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Sources/src/MapFile/MapOverlay.cpp`:
  - `AddObject` (fresh link ID = max + 1), `MoveObject`, `DeleteObject`;
  - `FindReferences` (81-126). It has the script-ID bug and the missing kinds of D-04.

  It is the place for the per-collection overlay records.
- `Sources/src/MapFile/MapEquivalence.cpp`: already compares every M2 field (it refuses unknown fields). The expected-value builder extends from here.
- `Sources/src/EditorBridge/session.cpp`:
  - `PlaceObjects` / `BuildBridges` (110-179): bridge spans are placed as linked groups; `fHP<0` becomes `futureBuildLinkIDs`;
  - `PlaceOneObject` (72-99): entrenchment pieces and fences;
  - `ObjectAt` (967-1010): skips `SGVOGT_BRIDGE`/`SGVOGT_ENTRENCHMENT` at 993; a soldier maps to his squad;
  - `SaveSessionMap` (332-375): the read-back verification.
- Engine APIs, all compiled in the portable build (every file in each library's `.vcxproj` is built, `build.zig:1861/1875`):
  - `ITerrainEditor` (`Scene/Terrain.h:68-97`, from `ITerrain::GetEditor()`): `SampleCurve`, `SmoothCurveWidth`, `Add/Update/RemoveRoad`, `Add/Update/RemoveRiver`;
  - `IAIEditor` (`AILogic/AILogic.h:8-70`): `AddRiver` / `DeleteRiver` (river passability), `AddNewObject`, `GetObjectScriptID`, `LinkToAI` / `AIToLink`;
  - `RandomMapGen/VSO_StaticMethods.cpp` / `VSO_Methods.cpp` (`build.zig:604-605`): the VSO sampling code.
- Rendering: `CTerrain::DrawVectorObjects` (`TerraDraw.cpp:84-103`) uses effects 8/303/304, which GFXGPU implements (`GraphicsEngineGpu.cpp:1579-1586`, `effects.zig`). Nobody has looked at a capture yet, so 04-01 measures it.
- Core:
  - `history.zig` Command union (paint, add, place, delete, diplomacy, map_type, attacking_side, sound_*);
  - `editor.zig` replay (~470-535);
  - `tools.zig` Brush / Placer / Selector;
  - `fake_bridge.zig`.

  M2 adds a record command and tools in the same pattern.
- App:
  - `panels.zig`: Tools, Object palette, Properties (name, link, x, y, direction, player), Players, Sounds (with the commit-on-deactivate pattern), Status bar, menu bar (File/Edit/Tools/View/Test);
  - `view.zig`: the Tool enum {select, brush, place}; the overlay draws sound markers and the brush outline;
  - `auto.zig`: the `BK_EDITOR_AUTO` action union.
- MFC reference code to port (not to link):
  - `VectorStripeObjectsState.cpp`, `TabVOVSODialog.cpp`;
  - `RoadDrawState.cpp` (bridges 71-104, 626-667, 963-1031, 1241-1384; fences 519-622, 841-898, 1034-1096; trenches 247-465, 1135-1216, 1384-1585);
  - `GroupManagerDialog.cpp`, `AIStartCommand.cpp`, `ObjectPlacerState.cpp:525-771`, `StateAIGeneral.cpp`, `MapToolState.cpp`;
  - `TemplateEditorFrame1.cpp` (save 2934-3291, anchors 5035-5051).

### Established Patterns
- Every bridge entry point goes through `Guarded`, guards with `if`s (never asserts) and documents its units (AI/map vs Vis/world: `Vis2AI` truncates with +0.3, `fmtTerrain.h:29-37`).
- The core stays std-only. SDL, ImGui and the C ABI live in the app.
- Save is snapshot plus overlay, then read back and compared (`BK_EDITOR_FAILED` naming the difference). The engine is never the source of saved data.
- One command per gesture; refused edits are status-bar notes and leave the history unchanged.
- C++: never `std::min` / `std::max`, use `Min` / `Max` from `Misc/Tools.h`.
- Build: never `zig fmt build.zig`; after editing it, run `zig test tools/zig/build_hermeticity_test.zig`.
- Test artefacts go in `zig-out/local-test`.
- Don't build packages on the Mac (disk is tight). Windows runs happen on win-home.
- A failed `expect` before `destroy` hangs worker tests; defer destroy.
- Measure graphics (F9, captures, TGA) rather than guess.

### Integration Points
- Game readers the edits must satisfy:
  - `iMissionInternal.cpp:1391-1504`: `IsValid`; the script path is the map directory plus the basename; the camera anchor order;
  - `AILogicInternal.cpp`: `LoadUnits` holds script-ID groups back (463-486); `LoadEntrenchments` / `LoadBridges` assert links (607-646); `InitStartCommands` (662-676); `InitReservePositions` (705-723, `checked_cast`);
  - `SupremeBeing.cpp:48-49`, `GeneralInternal.cpp:451-476`;
  - `Scripts.cpp:106-110`: areas in AI units.
- Test launch: `BkEditorTestMapPath` must also copy the script file (D-20).
- `05-PARITY.md`: the M2 rows get their evidence in 04-08.

### Canonical references
- `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`: the spec (preservation invariant, overlay, equivalence, test tiers). 04-01 changes its M2 sections.
- `.planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md` and `03-06-SUMMARY.md`: the M1 decisions and the D-12 evidence.
- `.planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md` and `05-CONTEXT.md`: the owner of every MFC feature; M3's D-19/D-20 (heights), D-25/D-26/D-27 (selection, properties, links).
- `04-PARITY.md` (this folder): the M2 rows per plan.

</code_context>

<specifics>
## Specific Ideas

- Johannes asked for M2 to be implemented **fully**, at feature parity with the MFC editor for these areas. The only deferrals allowed are M3's items (per `05-PARITY.md`) and things that need new art or renderer phases (D-23).
- Where the MFC editor has a gesture, the portable editor uses the same one (click/right-click/double-click/Enter/Insert/Delete in each tool), so MFC-trained map makers are at home. Trackpad equivalents follow M1: Ctrl-click stands in for right-click, Alt-drag for middle-drag.
- Shipped data, with rough counts over the 57 `Data/Maps` maps:
  - roads: 57 maps;
  - rivers: 37;
  - bridges: 31;
  - entrenchments: 29;
  - Lua scripts: 48.

  `Data/Scenarios` holds 1,696 `.bzm`. The M2 sweep (D-25 item 4) uses `Data/Maps`.
- Every family of bridges ships as `_01` (horizontal) and `_02` (vertical). This is what makes "rotate a bridge" a variant swap.
- The inventory of every MFC feature and its owner is `05-PARITY.md`. `04-PARITY.md` expands the M2 rows into what each plan delivers and how each row closes.

</specifics>

<deferred>
## Deferred Ideas

- **Free camera rotation (phase 3's D-12):** closed as not planned for the editor (D-23). Reopening it needs a renderer phase for terrain plus new multi-angle art.
- **Owned by M3 (phase 5), per `05-PARITY.md`:**
  - unit creation info (M5);
  - garrison / tow / couple links, and units inside a trench (O13, O18 fields, O21);
  - multi-selection (O9–O11, O14, O20). Start commands with several units at once get easier with it; D-17 works one unit at a time until then;
  - the full properties panel beyond Script ID (O17/O19);
  - add or delete players (M4);
  - the damage tool (MT1);
  - heights, shades and Update Map, which would also re-fit road/river Z after height edits (M8/M9/TR4–TR12);
  - fields and filters;
  - layers L1–L15;
  - the minimap;
  - Check Map (M7), which catches dangling script IDs;
  - New Map;
  - the RMG composers.
- **Moving a whole bridge or entrenchment by drag:** the MFC editor redraws instead. Not added.
- **The game's Custom Mission list showing user maps:** its own later phase (phase 3 deferred list).

</deferred>

---

*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Context gathered: 2026-09-30*

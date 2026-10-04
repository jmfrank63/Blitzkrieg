# Phase 4: Map editor M2: roads, rivers, bridges, AI groups, scripts - Research

**Researched:** 2026-09-30
**Domain:** Native game-engine tooling (C++ engine bridge, Zig core and app, Dear ImGui, SDL3): making the map's road, river, bridge, entrenchment, fence, script, group, start-command, reserve-position, AI-general and camera-anchor data editable with undo, snapshot-plus-overlay save and test-in-game.
**Confidence:** HIGH on the codebase mechanisms (every one read from source this session, with line ranges); MEDIUM on three items that need a spike (marked `[ASSUMED]` and listed in the Assumptions Log).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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

### Deferred Ideas (OUT OF SCOPE)

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
</user_constraints>

<phase_requirements>
## Phase Requirements

No requirement IDs are mapped to this phase. The requirement set is CONTEXT.md's D-01..D-25 plus the M2 rows of `04-PARITY.md` (listed per plan). Mapping of decisions to the research sections that enable them:

| Decision / row | Research support |
|---|---|
| D-01 (record-level overlay, no MFC save-time rewrites) | "Saving" facts in Architecture Patterns 1 and 2; the extended not-copied list in Corrections C8; sweep design in Validation Architecture |
| D-02, D-03 (commands, derived data stored) | Architecture Patterns 2-4 (record commands, derive-then-put, bridge tombstones for compounds); Corrections C6, C10 |
| D-04 (cascade delete, `FindReferences`) | Corrections C3, C11; Pitfalls 7, 8, 16; plan notes 04-01 |
| D-05 (palette filter) | Plan notes 04-01 (where the three game types are refused); GameDB enum values quoted there |
| D-06 (marker layer) | Architecture Pattern 5; Pitfalls 13, 17 |
| D-07, D-08, D-09 (roads and rivers) | Plan notes 04-02 (`CVSOBuilder`, gestures, engine sync, river passability probe); Corrections C10; Pitfalls 1-4 |
| D-10, D-11, D-12 (bridges) | Plan notes 04-03 (algorithm quoted from `RoadDrawState.cpp`); Corrections C1, C6; Pitfalls 5, 6, 7 |
| D-13 (entrenchments) | Plan notes 04-04 (trench algorithm, section rule, property tests instead of goldens) |
| D-14 (fences) | Plan notes 04-03 (fence run algorithm) |
| D-15, D-16 (script IDs, groups) | Plan notes 04-05; Corrections C7, C9; Pitfall 9 |
| D-17, D-18 (start commands, reserve positions) | Plan notes 04-06; Corrections C3, C4; Pitfalls 8, 9, 10 |
| D-19 (AI general) | Plan notes 04-07; Corrections C2; Pitfall 11 |
| D-20, D-21, D-22 (script file, areas, anchors) | Plan notes 04-05; Corrections C8; Pitfalls 12, 14, 15 |
| D-23 (camera rotation closed) | Spec text only (04-01); evidence already in `03-06-SUMMARY.md` and the spec's M1 scope paragraph |
| D-24, D-25 (plans, exit criteria) | Validation Architecture (tiers, commands, per-feature matrix); Environment Availability; Open Questions |
| 04-PARITY rows (O16 refs, S4, VO1-VO7, MT2, G1, M2, M6, U1-U3, AI1) | Plan notes per plan; Validation Architecture matrix |
</phase_requirements>

## Summary

M2 is not a new architecture: it repeats M1's chain (tool -> `Editor` -> bridge vtable -> C ABI `Guarded` -> session, snapshot plus working copy updated together, engine kept in step) for about a dozen more collections. The M1 sound list (03-10) is the exact template for every simple collection: a typed record, insert/replace/erase by index, validated in `session.cpp`, both copies updated, one core command with `before`/`after`. What is genuinely new is (a) four derive-then-store cases (road/river sampling, bridge span plans, fence runs, trench pieces), (b) edits that touch several objects and records at once (cascade delete, bridge draw/rotate/delete, trench draw/delete), (c) the app's input layer (right button, double click, Enter/Insert keys, a tool registry, a marker layer), and (d) observing what the game actually consumed.

Reading the source turned up thirteen places where CONTEXT.md's wording differs from what the code does (see Corrections). Most are small (a default radius of 256 AI units, not "4 cells"; `futureBuildLinkIDs` is recorded but nothing marks the span; the map-file tier's data-only startup has no object database). Three change design: the MFC editor's own delete removes a reserve position whenever the artillery or the truck is deleted (D-04 says it clears the truck), the frame indices the MFC editor picks at random never reach the file (only the packed span/fence/trench type is saved, so D-03's determinism is only about the working copy and the engine), and the MFC editor cannot edit AI-general sides that do not already exist in the file. None of them blocks the plans; each has a recommended resolution below.

The largest risks are the trench builder port (about 700 lines of MFC state machine with integer truncation, and no way to run the MFC editor for a golden output), the app-side gesture layer (`view_math.kindOf` ignores every button but the left one, macOS delivers Ctrl+click as a left click, and `BK_EDITOR_AUTO` has no right-click, double-click or panel verbs yet), and the game-side observability seams the game-reads-it tier needs (Lua `Trace` writes to the in-game console only, and nothing prints what camera, areas, groups, start commands, reserve positions or AI parcels the game read).

**Primary recommendation:** Build on the sounds precedent (typed record + index + both copies + one core command) for simple collections; use bridge-side tombstones (like paints and deleted objects) for every edit that touches several objects; keep all geometry (bridge spans, fence runs, trench pieces, area conversion) in one GFX-free, object-database-free C++ unit that takes plain numbers, so the map-file tier and the bridge call the same function; and add one env-gated `BK_MAP_TRACE` seam in the game so the game-reads-it tier can assert on what the game consumed.

## Corrections to CONTEXT.md (facts found in source that change the plan)

Each is a fact about the code, not an opinion. "Resolution" is what this research recommends the planner do; the user's decision text stays locked unless the resolution says otherwise, and any deviation must be logged in the phase's decision log (the run is autonomous).

| # | CONTEXT says | Source says | Resolution |
|---|---|---|---|
| C1 | D-12: "The engine shows such a bridge intact and marked, as M1 already does (`futureBuildLinkIDs`)" | `futureBuildLinkIDs` is only filled (`session.cpp:166`) and cleared (`session.cpp:219`, `314`); no other reference exists in `Sources/` or `tools/`. Nothing marks the span. The MFC editor marks it with `SetSpecular( 0xFF0000FF )` (`TemplateEditorFrame1.cpp:1972`), and `IVisObj::SetSpecular( DWORD color )` exists (`Scene/Scene.h:224`) [VERIFIED by grep and Read this session]. | M2 must implement the mark: either `SetSpecular( 0xFF0000FF )` on the span visuals (bridge, after `UpdateSessionWorld`) or an overlay outline (D-06). Re-apply after undo/redo/rotate. Add an engine-tier or app check that a toggled bridge looks different. |
| C2 | D-19: a new parcel has "radius 4 AI cells" | `rAIGeneralParcelInfo.fRadius = CAIGState::PARCEL_POINT_RADIUS / fAITileXCoeff` with `PARCEL_POINT_RADIUS = fWorldCellSize * 4.0f` (`StateAIGeneral.cpp:22,190`). `fWorldCellSize / fAITileXCoeff` is 64 AI units, so the value is **256 AI units = 8 AI tiles (`TILE_SIZE = 32`, `aiconsts.h:13`) = 4 map (vis) tiles**. The same value is the minimum radius while dragging (`StateAIGeneral.cpp:272-275`). Computed: `python3` gives `256.0`. | Use 256 AI units as default and minimum; say "4 map tiles" in UI text. |
| C3 | D-04: reserve positions naming the deleted object as truck get the truck field cleared; start-command `linkID` is removed | `RemoveObjectFromReservePositions` erases the **whole position** when the artillery **or** the truck is the deleted object (`TemplateEditorFrame1.cpp:4194-4208`). `RemoveObjectFromAIStartCommand` removes the object from the command's unit list only and erases the command if the list is empty; it never touches the command's target `linkID` (`TemplateEditorFrame1.cpp:4025-4049`). | Recommended: follow the MFC editor for reserve positions (erase the position for either role), because a towed gun with no truck is a position the MFC editor refuses to create (C4). For a start command's target `linkID` set it to 0 (RMGC_INVALID, `MapInfo_Consts.cpp:12`: `const int RMGC_INVALID_LINK_ID_VALUE = 0;`) rather than leave a dangling link. Record the D-04 amendment. |
| C4 | D-18: "click an artillery unit, then optionally a truck" | `SaveReservePosition` does not add the position when there is no truck and the artillery is `IsArtillery( type )` with a non-empty `vPeoplePoints` (towed gun): `bAdd = false` (`TemplateEditorFrame1.cpp:4143-4160`, `bAdd = false` at 4158). A position whose artillery and truck link IDs are both 0 is dropped by the MFC save (`TemplateEditorFrame1.cpp:2980-2984`), and a formation member gets link ID 0 (`2962-2967`). | The bridge validates a reserve position when it is added (role of the clicked unit, towing force vs weight, crew points) and refuses with a status note; link ID 0 and "both 0" are refused. The validation needs the object database, so it lives in the bridge, not the core. |
| C5 | Spec and D-24: the map-file tier can build expected values with "the same function" including bridge/fence/trench geometry | `tools/zig/data_only_startup.cpp:1-8`: "No window, no GFX, no object database: the object database is only needed to pack frame indices, which this tier never does." `NDataOnly::Start` registers storage only. The spec text (data-only variant "storage, constants, object database") is wrong today. The bridge creates the object database only after the renderer (`bridge.cpp:474-482`, comment "After the renderer, because LoadDB reads textures"), although `GameDB.cpp` itself uses only `IDataStorage`. | Write the geometry as pure functions over plain-number inputs (span length, direction, origins, index lists); the bridge fills the inputs from `SBridgeRPGStats` / `SFenceRPGStats` / `SEntrenchmentRPGStats`, map-file tests fill them from literals. Optionally spike (A1) whether `NDataOnly::Start` can also create and `LoadDB` the object database; do not depend on it. Fix the spec text in 04-01. |
| C6 | D-03: bridge frame indices "chosen deterministically" because the MFC editor picks them at random | The saved `nFrameIndex` is the **packed type**, not the picked index: `CMapInfo::PackFrameIndices` runs before every MFC save (`TemplateEditorFrame1.cpp:3238`) and maps a concrete index to `BRIDGE_SPAN_TYPE_BEGIN` / `_CENTER` / `_END` (`0x00000001`, `0x00000002`, `0x00000004`, `RPGStats.h:1307-1309`), a fence index to `( 1 << dir ) \| FENCE_TYPE_NORMAL` (`FENCE_TYPE_NORMAL = 0x00010000`, `RPGStats.h:1086`; `RPGStats.cpp:1823-1836`) and a trench index to `ENTRENCHMENT_LINE/FIREPLACE/TERMINATOR/ARC = 1/2/4/8` (`RPGStats.h:1002-1005`; `RPGStats.cpp:1684-1695`). The random pick only decides which sprite the working copy and the engine show. | Snapshot record gets the packed type directly (a pure function of role/direction). The working copy gets a variant chosen with the seeded overloads (`GetRandomBeginIndex( -1, 0, &seed )` etc.), never the `rand()` ones. Guard empty index lists: `indices[ rand() % indices.size() ]` and `*pSeed %= indices.size()` divide by zero when a list is empty (`RPGStats.h:1043-1054`, `1400-1412`). |
| C7 | D-15 / 04-PARITY: script ID closes with "engine `GetObjectScriptID`" | `IAIEditor` has `GetObjectScriptID` but no setter (`AILogic/AILogic.h:8-70`). The AI takes `nScriptID` only through `AddNewObject` (`AILogicInternal.cpp` `scripts.AddObjToScriptGroup( pResult, object.nScriptID )`). | Script ID edit changes snapshot and working copy only, like sounds (the engine never needs it: it matters at mission start). The engine-tier check is: set, save, reopen, `GetObjectScriptID` on the reopened object equals it. |
| C8 | D-01 / 04-PARITY "not copied" list names four MFC save rewrites | A fifth: `MakeCamera()` resizes `playersCameraAnchors` to `diplomacies.size() - 1` (filled with `VNULL3`) on every open (`TemplateEditorFrame1.cpp:3421-3435`, the `resize` at 3435), so an MFC save rewrites the vector's size. Also, the MFC editor's AI-general tab can only edit a side that already exists (`nSide < sidesInfo.size()` in every handler, `StateAIGeneral.cpp:59,249,331,451`); nothing in the MFC code creates sides. | Do not resize on open. "Set camera for player N" pads to `max( size, N + 1 )` with `VNULL3` (the game treats `VNULL3` as unset: `iMissionInternal.cpp` `if ( vCameraStartPos == VNULL3 ) vCameraStartPos = mapinfo.vCameraAnchor`) and never shrinks. Editing a missing AI side creates it and every lower one empty; undo restores the old vector size (Pitfall 11). Add the resize to the not-copied table. |
| C9 | D-16: New group takes "the first unused ID from 0 up" | MFC asks for an ID (`CGetGroupID`) and bumps it up to the first unused value at or above it (`GroupManagerDialog.cpp:83-108`). | Offer an ID field defaulting to 0, bumped to the first unused at or above it. |
| C10 | D-07: sampled points "from `ITerrainEditor` ... `SampleCurve`" | The MFC edit tool does not use `ITerrainEditor::SampleCurve`; it calls `CVSOBuilder::Update( pVSO, bKeepKeyPoints, DEFAULT_STEP, fWidth, fOpacity )` then `CVSOBuilder::UpdateZ( altitudes, pVSO )` (`VectorStripeObjectsState.cpp:209-214, 651-656`). `CVSOBuilder` is GFX-free and deterministic (no `rand()` in `VSO_StaticMethods.cpp`), and it keeps key-point widths and opacities, which `ITerrainEditor::SampleCurve` does not. `DEFAULT_STEP = 30.0f` (`VSO_StaticMethods.cpp:15`). | Use `CVSOBuilder` (RandomMapGen, already linked into MapFile and the map-file test). |
| C11 | D-04: `FindReferences` fix list | Link ID 0 is "no link ID" (`RMGC_INVALID_LINK_ID_VALUE = 0`) and shipped maps carry hundreds of objects with 0 (measured on arnheim, `session.cpp:390-395`). `FindReferences` / cascade must never treat 0 as a reference. Existing tests assert the old refusals: `tools/zig/editor_bridge_test.cpp` `TestDeleteIsRefusedWhileReferred` (line 2278; still valid for a bridge span) and `Sources/editor/core/editor.zig:922` (`"still referred to by bridge 0"`, fake bridge at `fake_bridge.zig:346`). | Ignore 0 in every reference match. Keep the span refusal message; update the fake bridge to model start-command references. |
| C12 | Phase 3 research (Windows job about 75 minutes) | `.github/workflows/cross-platform.yml:170` now says `timeout-minutes: 110` (cold cache about 25 minutes in the engine tier and 25 in the release random-missions tier). Last full run: 1h18m (run 36596882722 on main); last branch run 46m34s [VERIFIED: `gh run list`]. | Budget is comfortable for a few dozen more engine-tier cases in the existing executables. Do not add a second engine-linking test executable on Windows. |
| C13 | D-08 / "Ctrl-click stands in for right-click" | `view_math.kindOf` returns null for every button but the left one ("the right button does nothing yet", `view_math.zig:429-435`). SDL delivers Ctrl+click as a left click with `SDL_KMOD_CTRL` by default (`SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK` default "0") [CITED: wiki.libsdl.org/SDL3/SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK]. `tools.Event` has only press/drag/release/key and `tools.Key` only delete/rotate_left/rotate_right (`tools.zig:17-18`). D-14 uses Ctrl as a *modifier* in the Fence tool, so Ctrl+click cannot mean right-click there. | 04-01 adds right press/drag/release, double click (`event.button.clicks == 2`), and Enter/Insert/Escape/Space keys to the core `Event`/`Key`; Ctrl+left is converted to a right-button event only in tools that use the right button (roads/rivers, entrenchment), never in the Fence tool. |

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|---|---|---|---|
| Road/river control-point editing gestures, width/opacity handles | Editor core (Zig `tools`) | Editor app (input, markers) | Tools turn plain events into commands; only the app sees SDL/ImGui. |
| Sampling a road/river (`points`), `UpdateZ`, descriptor load | Engine bridge (C++, `CVSOBuilder`) | Map-file tier (same function for expected values) | Needs the data storage and the altitudes; deterministic; already GFX-free in RandomMapGen. |
| Drawing the stripe, animated river layer | Engine (Scene/GFXGPU via `ITerrainEditor`) | - | The engine draws; the editor only keeps it in step (D-09). |
| River passability in the editor AI | Engine bridge (`IAIEditor::AddRiver`/`DeleteRiver`) | - | Same calls the MFC tool makes. Roads never touch the AI (D-09). |
| Bridge span, fence run and trench piece plans | Shared GFX-free C++ unit (plain-number inputs) | Bridge (fills inputs from RPG stats), map-file tests (literals) | The "same function" rule (spec, "What equivalent means") without needing an object database in the data-only tier (C5). |
| Span/piece objects, link IDs, `bridges[i]`, `entrenchments[i]` | Engine bridge session (snapshot + working + engine, tombstones) | Core (holds handles only) | Objects already live there; compound undo needs atomic rollback across both copies and the engine. |
| Simple records (start commands, reserve positions, areas, groups, anchors, AI general, script file) | Engine bridge session (both copies) | Core (one command per edit, before/after) | The sound-list precedent (03-10). |
| Reserve-position role and towing checks, action-command list | Engine bridge (has object DB and storage) | Core (sees only refuse/ok and the list) | Needs `SMechUnitRPGStats` and `editor\actions.ini` through the storage. |
| Reference cascade on delete, `FindReferences` | Map-file overlay (`MapOverlay.cpp`) | Bridge session (applies, tombstones) | Pure data logic, testable without the engine. |
| Markers (control points, areas, parcels, anchors, lines) | Editor app (ImGui background draw list) | Bridge (`BkEditorWorldToScreen`) | D-06; same route as the M1 sound markers and brush outline. |
| Tool registry, panels, key bindings, `BK_EDITOR_AUTO` verbs | Editor app | Core (tool logic) | Panels and verbs share one command registry so scripted runs call what buttons call. |
| Script file copy, test-launch copy, "Open script" | Editor app (`Files`, `SDL_OpenURL`) | Bridge (`BkEditorTestMapPath` for the destination) | File I/O already goes through core `files.zig` (temp + swap + fake for tests). |
| What the game consumed (camera, script, areas, groups, start commands, reserve positions, parcels) | Game (`BK_MAP_TRACE` seam, stderr) | Editor app game-reads-it mode (parses the log) | Nothing else observes it (Lua `Trace` goes to the console buffer only). |

## Standard Stack

No new external dependency is needed. Everything below is already in the tree and already built by `build.zig`.

### Core
| Component | Version | Purpose | Why standard here |
|---|---|---|---|
| Zig | 0.16.0 (`zig version`; CI installs "Zig 0.16.0") | Build, core, app, test steps | The repo's only build system [VERIFIED: local `zig version`, `cross-platform.yml`] |
| `CVSOBuilder` (`RandomMapGen/VSO_Types.h`, `VSO_StaticMethods.cpp`, compiled at `build.zig:604-605`) | in tree | Create, resample and Z-fit roads/rivers | The exact function the MFC tools call (C10) |
| `CMapInfo::TerrainHitTest`, `GetBoundingPolygon` (`RandomMapGen/MapInfo_StaticMethods.cpp:1096`) | in tree | Which road/river is under a point | GFX-free, what `CVSOState` uses |
| `CMapInfo::PackFrameIndex` / `UnpackFrameIndex(es)` | in tree | Type <-> sprite index for fence/entrenchment/bridge | Already used by `AddObjectToSession` (`session.cpp:692-694`) |
| `IAIEditor` (`AILogic/AILogic.h`), `ITerrainEditor` (`Scene/Terrain.h`) | in tree | Engine sync (river AI, road/river draw, `CanAddObject` probe) | The engine's own editing interfaces |
| `NMapFile::AreEquivalent`, `AssertEveryFieldIsCompared` | in tree | Read-back verification and expected-value comparison | Already compares every M2 field (`MapEquivalence.cpp:249-409`) |
| Dear ImGui (dcimgui, docking) + SDL3 3.4.0 | vendored | Panels, markers, dialogs (`SDL_ShowOpenFileDialog`, `SDL_OpenURL`) | M1's UI stack |
| `core/files.zig` (`Files`, `FakeFiles`, `StdFiles`) | in tree | Script copy, Save As copy-along, test-launch copy | Has `copy`, `exists`, `delete`, `rename` and a fake for core tests |

### Supporting
| Component | Purpose | When to use |
|---|---|---|
| `tools/zig/map_file_test.cpp` (745 lines) | Map-file tier: read/overlay/expected-value cases | Every M2 overlay op and the M2 sweep |
| `tools/zig/editor_bridge_test.cpp` (3659 lines, `main` at 3492) | Engine tier C++: bridge C ABI against the real engine | All M2 engine cases; add new `TestM2*` functions in this executable |
| `Sources/editor/app/c_bridge_test.zig` | Engine tier Zig: core `Editor` driving the real bridge, `expectEngineMatches` after each step | End-to-end M2 command round trips |
| `BK_EDITOR_AUTO` (`auto.zig`, `smoke.zig` `AutoRunner`) | App tier scenario and shots | The M2 scenario |
| `--game-reads-it` (`main.zig:535`) + `BK_AUTO_UI` | Game tier | The M2 game-reads-it scenario |

### Alternatives Considered
| Instead of | Could use | Tradeoff |
|---|---|---|
| Typed record + index per collection | One opaque byte-blob record API for all collections | Blob makes the fake bridge and undo trivially uniform, but the app still needs typed reads for drawing, so the ABI would have both. Use blobs only if the planner finds the per-collection surface too large. |
| Remove+Add of a road/river in the engine per edit | `const_cast` mutate of the engine's `GetTerrainInfo()` then `UpdateRoad(nID)` (what MFC does, `VectorStripeObjectsState.cpp:130-215`) | Mutating the engine's copy blurs "the engine is never the source"; Remove+Add costs the same `Init` per drag frame and needs the saved-to-engine ID map anyway (D-03). Recommended: Remove+Add. |
| Bridge-side tombstones for compound edits | Core-side compound command (`[]Command`) | Core-side compounds need every low-level bridge call to bypass its own refusals (a referenced span cannot be deleted); tombstones keep atomic rollback in one place, like paints and deleted objects today. |

**Installation:** none. Build and test with the existing steps (see Validation Architecture).

**Version verification:** no `npm`/`pip`/`cargo` package is introduced, so the Package Legitimacy Gate does not apply.

## Package Legitimacy Audit

Not applicable: this phase installs no external packages (no new `build.zig.zon` dependency, no vendored addition). If a planner ever proposes one (for example a Lua binding to syntax-check the test script), the gate must run first.

## Architecture Patterns

### System Architecture Diagram

```
 SDL events (left/right/middle, double click, keys, wheel)   ImGui panels (Groups, Start commands,
        |                                                    Reserve, AI general, Areas, Script...)
        v                                                            |
 view.zig  --- Ctrl+left => right (only tools that use it) ---+      | button = same registry
        |                                                     |      v
        +--> tool registry: Roads&Rivers | Bridge | Trench | Fence | Areas | AI General | Reserve | Select/Brush/Place
                     |  Event: press/drag/release, right_*, double_click, key(enter/insert/escape/...)
                     v
              core Editor  (history.reserve -> bridge call -> record; gesture keys merge drags)
                     |                          ^ read models: document.objects, sounds_generation-style
                     v                          | generation counters for each M2 collection
              Bridge vtable (fake in core tests / RealBridge in app)
                     |  C ABI, every entry Guarded, refusals = BK_EDITOR_REFUSED + message
                     v
   +-----------------+------------------------------------------------------------+
   | session: snapshot (what is saved)  <->  working (what the engine was built from)|
   |  simple records: insert/replace/erase by index in BOTH copies (sound precedent) |
   |  derive calls: CVSOBuilder sample + UpdateZ | span/fence/trench plans (pure C++) |
   |  compound edits: objects (tombstones) + bridges[i]/entrenchments[i] + refs       |
   +--------+--------------------+---------------------------+----------------------+
            |                    |                           |
            v                    v                           v
   ITerrainEditor          IAIEditor                 NMapFile::Write -> read back ->
   Remove/AddRoad/River    AddRiver/DeleteRiver,      AreEquivalent(snapshot, readBack)
   (engine draws)          AddNewObject, DeleteObject  (BK_EDITOR_FAILED on any difference)
                                                                 |
   Test in game: BkEditorTestMapPath dir  ->  copy <script>.lua beside mapeditor_test.bzm
                                                                 v
                          Game -editor-test ... prints BK_MAP_TRACE lines (new seam) -> game-reads-it parses
```

### Recommended Project Structure

New code goes in new files so the shared hot files (`session.cpp` 1631 lines, `panels.zig` 2215, `panels_logic.zig` 2251, `bridge.h` 722) do not grow into unreviewable monoliths. `build.zig` lists C++ sources explicitly (`build.zig:3765` for MapFile, `3806-3809` for EditorBridge), so each new `.cpp` needs an entry; after editing `build.zig` run `zig test tools/zig/build_hermeticity_test.zig` and never `zig fmt build.zig`.

```
Sources/src/MapFile/
  MapOverlay.{h,cpp}       extend: fixed FindReferences, cascade result, record-level ops
  MapRecords.{h,cpp}       NEW  insert/replace/erase per M2 collection + expected-value builder helpers
  MapGeometry.{h,cpp}      NEW  pure plans: bridge spans, fence runs, trench pieces, area conversion (plain numbers in/out)
Sources/src/EditorBridge/
  session_records.cpp      NEW  start commands, reserve positions, areas, groups, anchors, AI general, script file
  session_vso.cpp          NEW  roads/rivers: derive, put, engine sync, pick, river AI, probe
  session_groups.cpp       NEW  bridges, entrenchments, fences: plans -> objects, tombstones, group pick, rotate, toggle
  bridge.h / bridge.cpp    extend (entry points stay thin wrappers around the above)
Sources/editor/core/
  records.zig              NEW  record types shared by fake bridge, commands and tools
  history.zig / editor.zig extend: record command variants, generation counters
  tools_vso.zig, tools_groups.zig, tools_ai.zig ...  NEW tools (one file per tool family)
Sources/editor/app/
  markers.zig              NEW  marker layer (kinds, colours, View > Markers switches)
  tool_registry.zig        NEW  Tool list, shortcut keys, per-tool event needs (right button, double click)
  commands.zig             NEW  named commands shared by buttons and BK_EDITOR_AUTO `do=` verbs
  panels_m2.zig            NEW  Groups, Start commands, Reserve, AI General, Areas, Script, Camera panels
tools/zig/fixtures/        NEW  m2_script.lua for the game-reads-it scenario
```

### Pattern 1: Simple collections follow the sound precedent (03-10)

**What:** A typed `extern struct` record, `Add(index|-1)`, `Set(index)`, `Delete(index)`, a read-all call with two-pass sizing, validation in `session.cpp`, snapshot and working copy updated together, engine untouched unless the collection feeds it. Core side: `sound_add` / `sound_edit` / `sound_delete` commands with `index`, `before`, `after`, merge by gesture (`editSound`), and a generation counter the panel watches.
**When to use:** start commands (`std::list`, index by position), reserve positions (`std::list`), script areas, camera anchors (fixed-size, `Set` only), AI-general sides, script file (a string), reinforcement groups (keyed by ID, not index: use `SetGroup(id, ids[])` / `DeleteGroup(id)`).
**Example (existing, to copy):**
```cpp
// Source: Sources/src/EditorBridge/bridge.cpp:1831-1850 (BkEditorAddSound)
BkEditorStatus BkEditorAddSound( BkEditorSession *pSession, int nIndex, const BkEditorSoundRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !SoundRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen ) { pSession->szMessage = "no map is open"; return BK_EDITOR_REFUSED; }
		// ... index range -> BK_EDITOR_BAD_ARGUMENT; AddSoundToSession decides refused vs failed
	} );
}
```
Zig side: `Editor.addSound` reserves history room first, calls the bridge, then `recordAssumeCapacity` (`editor.zig:418-427`), so a recording failure can never happen after the bridge committed.

### Pattern 2: Derive once, store, put raw on undo (D-03)

**What:** Two families of bridge calls per derived record. The edit call takes what the user changed (control points, widths, opacity, descriptor name) and returns the full record the bridge derived; the command stores that full record as `after` (and the old one as `before`). Undo and redo call the raw put with the stored record; nothing is resampled. For records with no derived data one call serves both.
**Roads and rivers (the MFC recipe, verbatim order):**
```cpp
// Source: VectorStripeObjectsState.cpp:644-657 and RandomMapGen/VSO_Types.h (CreateVSO, Update, UpdateZ)
SVectorStripeObject vso;
CVSOBuilder::CreateVSO( &vso, szDescName, controlPoints );            // loads the descriptor, UniquePolygon( 2.0 )
CVSOBuilder::Update( &vso, false, CVSOBuilder::DEFAULT_STEP,           // 30.0f
                     fWidth * fWorldCellSize / 2.0f, fOpacity );
CVSOBuilder::UpdateZ( altitudes, &vso );                               // control points and sampled points
// saved nID: the bridge's own (above every nID in use), never the engine's rand()
```
An edit of an existing record calls `Update( &vso, true, ... )` so key-point widths and opacities are kept (`SBackupKeyPoints`); MFC calls it twice around `LoadKeyPoints` after an insert or delete (`VectorStripeObjectsState.cpp:534-545, 566-577`); keep that order.

### Pattern 3: Edits that touch several objects use bridge-side tombstones

**What:** A cascade delete, a bridge draw/rotate/delete, a trench draw/delete, a fence run are each one bridge call that changes objects and records atomically and keeps its own undo record (the way `tombstones` and `paints` already work, `session.h:43-67`). The core keeps a small handle (link IDs, group index) in one command, so undo/redo are `restore`/`remove` calls and the core never has to bypass a refusal.
**Ordering rules that keep the game loaders alive (Pitfall 7):**
- Undo of a draw: erase the `bridges[i]` / `entrenchments[i]` entry first, then delete the spans/pieces (a span named by `bridges` cannot be deleted; that refusal stays, D-04).
- Redo of a draw: restore the spans/pieces by link ID first (tombstones keep link IDs, `nLinkIDFloor` never re-hands them out), then insert the entry.
- Delete of a whole group: reverse of the above; the tombstone must remember the entry's index.
- Cascade delete: remove the object, then edit the referencing records; the tombstone stores every removed or edited record with its list index; restore re-inserts in reverse order.
**Not a violation of D-02:** the record of what changed still exists and undo still writes it back; it is kept in the bridge next to the engine objects it must stay consistent with.

### Pattern 4: One pure geometry unit, plain numbers in and out (C5)

**What:** `MapGeometry.cpp` takes structs of numbers (span length in vis units, direction, per-index origins, index lists for begin/line/end, trench width, fence per-direction centre indices and origins, tile size, world/AI coefficients from `fmtTerrain.h`) and returns positions, directions, packed frame types and section groupings. No `IObjectsDB`, no `IAIEditor`, no rendering. The bridge builds the inputs from the RPG stats; the map-file tier's expected-value builder calls the same functions with literal inputs; the engine tier asserts that what the bridge built equals the function's output for the real stats' inputs.
**Constants it must use, not re-derive:** `fAITileXCoeff = 32.0f * FP_SQRT_2 / 64.0f`, `fWorldCellSize = fCellSizeX * FP_SQRT_2` (`fmtTerrain.h:4-7`), `Vis2AI` truncation `int( x + 0.3f )` (`fmtTerrain.h:29-37`), `FitVisOrigin2AIGrid` (`fmtTerrain.h:54-60`), `SAIConsts::TILE_SIZE = 32` (`aiconsts.h:13`).

### Pattern 5: Input, tool registry and marker layer (04-01 builds it, later plans plug in)

- `tools.Event` gets `right_press`, `right_drag`, `right_release`, `double_click`; `tools.Key` gets `enter`, `insert`, `escape`, `space` (C13). `view.zig` keeps the left-button-only guards (`left_button_down`, `view_math.staleGesture`) and adds the same guard for the right button.
- Keys already taken in `view.zig:416-431`: Delete/Backspace, Q, E, 1, 2, 3, Z and Y with Ctrl/Cmd, Home; and the camera scroll keys W/A/S/D and the arrows (`view.zig:651-656`). New tool shortcuts must avoid all of these (use 4-9, 0, and letters not listed).
- A tool registry replaces the closed `Tool = enum { select, brush, place }` (`view.zig:21`) and the `switch` in `dispatch` (`view.zig:581-588`); each tool declares whether it needs the right button or double click, so Ctrl+left is converted only there.
- Markers use `igGetBackgroundDrawList()` and `BkEditorWorldToScreen` (`view.zig:466-483` is the pattern). A road's control polyline, width handles and key points are drawn only for the selected road; batch the conversions if a call per point proves slow (a selected road has hundreds of sampled points).
- A named-command registry (`commands.zig`) is what panel buttons call and what `BK_EDITOR_AUTO` `do=<name>[:arg]` verbs call, so a scripted run needs no ImGui text entry.

### Pattern 6: Observing what the game consumed

The game already has env-gated stderr trace seams (`BK_SCRIPT_TRACE` in `Scripts.cpp:726`, `BK_AI_TRACE` in `WorldClient.cpp`, `BK_SOUND_TRACE` used by `--game-reads-it`). Add one more, `BK_MAP_TRACE`, printing one line per consumed item: camera start (`iMissionInternal.cpp` after `vCameraStartPos` is final), script name and whether it loaded and `Init` ran, areas registered (`InitAreas`), reinforcement groups held back (`LoadUnits`), bridges and entrenchments loaded (`LoadBridges`/`LoadEntrenchments`), start commands launched (`InitStartCommands`), reserve positions applied (`InitReservePositions`), and each general's parcel and mobile-ID counts (`CGeneral::Init( const SAIGeneralSideInfo & )`). Also mirror `CScripts::Trace` (`Scripts.cpp:1748-1762`, which writes only to `CONSOLE_STREAM_CONSOLE`) to stderr under the same variable so the test script can print what it found.

### Anti-Patterns to Avoid
- **Resampling on undo or at save.** The file must hold exactly the stored derived record (D-01, D-03).
- **Using the engine's `AddRoad` nID or `GetTerrainInfo()` as saved data.** `CTerrain::AddRoad` overwrites `nID` with `rand()` (`TerrainEditor.cpp:242-264`).
- **`std::min` / `std::max`.** Use `Min` / `Max` from `Misc/Tools.h` (the overlay already does).
- **Editing the `roads3`/`rivers` vector by index from Zig.** Sampled records are variable length; keep them behind the bridge.
- **Copying the MFC "save rebuilds everything" model** (`HandOutLinks`, `CheckMap`, shade recompute, camera resize on open).

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---|---|---|---|
| Road/river sampling, widths, key points, Z | A new spline sampler | `CVSOBuilder::CreateVSO` + `Update` + `UpdateZ` | It is what the MFC tool runs; deterministic; keeps key-point widths and opacity |
| Which road/river is under the pointer | Geometry hit test in Zig | `CMapInfo::TerrainHitTest( terrain, point, THT_ROADS3D / THT_RIVERS, &indices )` | Same selection order as the MFC editor (cycle with right-click) |
| Frame index <-> type | Own tables | `CMapInfo::PackFrameIndex` for one object, `GetTypeFromIndex`; seeded `GetIndexFromType( type, &seed )` for the working copy | Pack and unpack are already the file's definition; the seeded overloads avoid `rand()` |
| Saved-map comparison | Field-by-field asserts in each test | `NMapFile::AreEquivalent`; the compile-time field guard `AssertEveryFieldIsCompared` | Already compares every M2 field and fails to compile on a new one |
| River passability | Own tile locking | `IAIEditor::AddRiver` / `DeleteRiver` | Same call the MFC tool makes; probe with `IAIEditor::CanAddObject` |
| Grid fit and unit conversion | Re-derived constants | `Vis2AI`, `AI2Vis`, `FitVisOrigin2AIGrid` (`fmtTerrain.h`) | The `+0.3` truncation is part of the file format's habit |
| Line rasterising for fences | New Bresenham | Port `a_dirLine` (`RoadDrawState.cpp:122-182`) unchanged | It decides which tiles a fence run covers |
| File copy/overwrite/rename in tests | `std.fs` calls in tools | `core/files.zig` `Files` + `FakeFiles` | Testable without a disk; already used by safe save |
| Native file pick dialog | Custom picker | `SDL_ShowOpenFileDialog` via the existing `showDialog` (`panels.zig:1092`) | Pattern exists and is tested |
| Undo merging of a drag | New merge logic | `beginGesture` + `mergeable` + `touchTop` / `dropTop` (`editor.zig:263-292`) | A drag that ends where it began must drop its entry; already handled |

**Key insight:** every hard part of M2 already exists somewhere in the engine or the MFC editor. The work is porting the MFC gestures faithfully, wiring them through M1's chain, and proving each result four ways (core, map file, engine, game).

## Common Pitfalls

### Pitfall 1: A road or river with fewer than two sampled points kills the game loader
**What goes wrong:** `CStaticMap::Load3DRoads` and `UpdateRiverPassability` loop `for ( int j = 0; j < points.size() - 1; ++j )` (`AIStaticMap.cpp:209-257, 258-314`): `size() - 1` on an empty vector is a huge unsigned bound. `CVSOBuilder::SliceSpline` also does `pPoints->back().bKeyPoint = true` on a list that stays empty when a curve is shorter than one 30-unit step (`VSO_StaticMethods.cpp:17-47`), and `SampleCurve` reads `rControlPoints[1]` with only an `NI_ASSERT_T` (compiled out in release) guarding it.
**Why it happens:** `CreateVSO` runs `UniquePolygon( ..., RMGC_MINIMAL_VIS_POINT_DISTANCE )` (2.0 vis units, `Polygons_Methods.cpp:11`), so a double-click that repeats the last click drops a control point silently, and two close clicks can leave one.
**How to avoid:** After `CreateVSO` require `controlpoints.size() >= 2`; after `Update` require `points.size() >= 2`; otherwise refuse with a status note ("too short"). Never call `Update` before the first check.
**Warning signs:** an edit that "worked" in the editor and a crash or hang at Test in game.

### Pitfall 2: The read-time passability fix makes a new road fail the read-back
**What goes wrong:** `CMapInfo::operator&` sets `fPassability = 1` for every `roads3` record whose value is 0 on **read** (`MapInfo_Methods.cpp:136-140` binary, `183-187` XML). `SaveSessionMap` reads the file back and compares it to the snapshot (`session.cpp:358-370`), so a road created from a descriptor whose `Passability` is 0 is written as 0, read back as 1 and refused with `BK_EDITOR_FAILED ... reads back different at terrain.roads3[i].fPassability`.
**How to avoid:** Apply the same fix when the road record is built (roads only, not rivers). All shipped road descriptors have `Passability="1"` (grep of `Data/Terrain/sets/*/Roads3D/*.xml`), but a mod's may not.

### Pitfall 3: Two ID spaces for roads and rivers
**What goes wrong:** `CTerrain::AddRoad` / `AddRiver` overwrite `nID` with `rand()` in the engine's own copy and push the record to the end of its `roads3`/`rivers`; `AddRoad` then `std::sort`s the engine's `roads` (the drawn objects) but not `roads3` (`TerrainEditor.cpp:242-318`). `RemoveRoad` always returns `false` (`TerrainEditor.cpp` end of `RemoveRoad`), `RemoveRiver` returns `true`.
**How to avoid:** The saved `nID` is the bridge's (max + 1 over the saved list). Keep `map<savedID, engineID>` per kind, update it after every Remove+Add, and compare engine and copy by that map in a new `BkEditorVsoMatchesEngine` check (points, controlpoints, width, opacity; not `nID`). Never trust the return values of `RemoveRoad`.

### Pitfall 4: River AI must follow the MFC order, on undo too
**What goes wrong:** The MFC tool calls `IAIEditor::DeleteRiver( current )` when a drag starts (control point, width handle) and `IAIEditor::AddRiver( current )` on mouse-up (`VectorStripeObjectsState.cpp:288-293, 455-465`); add calls `AddRiver` before the terrain add; delete calls `DeleteRiver` with the stored record then `RemoveRiver` (`1042-1094`). `UpdateRiverPassability` unlocks/locks tiles from the record's `points`, so `DeleteRiver` must receive the record **as it was locked**, not the edited one.
**How to avoid:** One rule in the bridge: any change to a river = `DeleteRiver( before )`, engine Remove+Add, `AddRiver( after )`. Undo/redo do the same with the stored records, so they need no special case. Do not update AI passability for roads (D-09).
**Observable:** `IAIEditor::CanAddObject` on a probe unit whose rectangle sits on the river centre answers false while the river is locked, because it tests `IsRectOnLockedTiles( rect, AI_CLASS_ANY )` and the four unit classes (`AIEditorInternal.cpp:411-441`) [VERIFIED]. `AILogic::InitEditor` loads rivers and 3D roads at open (`LoadMap( terrainInfo )`, `bLoadRivers = true`, `AIStaticMap.h:84`), so the probe works on a shipped river map (arnheim) from the start.

### Pitfall 5: Frame index confusion and empty index lists
**What goes wrong:** The snapshot must hold the packed type; the engine object and the working copy the sprite index (`session.cpp:688-703` explains what happens when the packed one reaches the engine: 65537 indexes a fence's segments). `AddObjectToSession` builds the working record with `nFrameIndex = 0` (`MapOverlay.cpp:137-143`), which is wrong for a span, a fence or a trench piece. `GetIndexLocal` divides by `indices.size()`.
**How to avoid:** Extend `SAddObject` (`MapOverlay.h:13-22`) with concrete `nFrameIndex`, `fHP`, `nScriptID`; the plan function returns the packed type for the snapshot and the bridge derives the working index with the seeded overloads; refuse a bridge/fence/entrenchment descriptor whose relevant index lists are empty.

### Pitfall 6: Bridge span plans have three unit systems and two nudges
**What goes wrong:** `GetPointsForBridge` works in vis units, snaps the start to the AI grid with `FitVisOrigin2AIGrid( &p, stats->GetOrigin( beginSpanIndex ) )`, spaces spans by `fLength = GetSpanStats( states[0].lines[0] ).fLength * fWorldCellSize / 2.0f`, and nudges the first span `x - 0.1` (horizontal) or the last `y + 0.1` (vertical) (`RoadDrawState.cpp:71-109`). The commit then converts each span with `Vis2AI` (integer truncation), applies the same nudge again in AI units (`998-1006`), and the MFC save applies it once more to the already truncated value (`TemplateEditorFrame1.cpp:3168-3179`). The net saved value is the truncated integer AI position with `-0.1` (first span, horizontal) or `+0.1` (last span, vertical).
**How to avoid:** One function, one nudge, in AI units after truncation; assert in the map-file tier that a horizontal bridge's first span x has fractional part `.9` and a vertical bridge's last span y has `.1` (floats: compare with the arithmetic result, not a literal). Guard `lines` empty (`states[0].lines[0]`), `nParts = int( ( end - begin ) / fLength )` of 0 (one begin and one end span), and off-map spans (`IAIEditor::IsObjectInsideOfMap` refuses through `PlaceOneObject`): the whole draw is refused and nothing is left behind (D-10 all-or-nothing).
**Direction:** `_01` families have `<Direction>01000000` (horizontal = 1) and `_02` `00000000` (vertical = 0) in `Data/Bridges/*/0N/1.xml`, matching `EDirection { VERTICAL = 0, HORIZONTAL = 1 }` (`RPGStats.h:1311-1315`) [VERIFIED for asphaltbridge, woodenbig_heavy, woodenlittle, railwaybridge_little, asphaltbridge_special]. `Data/objects.xml` has 18 bridge names, each `_01` with an `_02` partner (for example `W_WoodenBig_Heavy_01` / `_02`); there is no `W_AsphaltBridge_Special_*`. A mod may lack a partner: that is the refusal path.

### Pitfall 7: The game's bridge and trench loaders assert with compiled-out asserts
**What goes wrong:** `LoadBridges` and `LoadEntrenchments` guard every link with `NI_ASSERT_T`, then dereference (`AILogicInternal.cpp:607-646`); `pSpan->SetFullBrige` on a null, or `segments[0]` on an empty section, crashes the game at mission start in release.
**How to avoid:** Every M2 edit must keep every `bridges[i][j]` and every `sections[s][k]` naming an existing object, and no section empty. Consequences: a bridge span or trench piece is never deleted singly (D-04 keeps this refusal); undo/redo orders as in Pattern 3; the M2 sweep and the engine tier validate all links after every step.

### Pitfall 8: Link ID 0, held-back units and reserve-position casts
**What goes wrong:** (a) 0 is "no link"; many shipped objects carry it. (b) `LoadUnits` holds back every `mapInfo.objects[i]` whose `nScriptID` belongs to a reinforcement group (`GetGroupById`) and never places it (`AILogicInternal.cpp:463-486`, note: `objects` only, scenario objects are not checked). `InitStartCommands()` then builds `unitsBuffer[i] = CLinkObject::GetObjectByLink( id )` with no null check (`663-677`; the overload at 678-702 does check) [ASSUMED: what `RegisterGroup` does with a null]. (c) `InitReservePositions` does `checked_cast<CAIUnit*>( pLinkObject )` (`703-722`), undefined in release if the object is not a unit; `CScripts::Init` builds `reservePositions[artillery] = truck` for both directions (`CScripts::Init`, `Scripts.cpp`), so link 0 collides.
**How to avoid:** Refuse link ID <= 0 everywhere a link is stored; refuse a reserve position whose artillery or truck is not an SGVOGT_UNIT; warn (status note, not a refusal) when a script ID the user assigns to a group is carried by an object named in a start command or reserve position.

### Pitfall 9: Groups and script IDs
**What goes wrong:** `SReinforcementGroupInfo::GetGroupById( -1 )` returns the group holding -1 if a user ever added it. The map keeps groups in an `unordered_map` and writes them sorted (`fmtMap.cpp:264-287`), so file order never depends on insertion; but the read-back compare is order-independent too, so nothing detects a lost group except a count. A script ID field has range -1 (none) or 0..32000 (D-15).
**How to avoid:** Validate group script IDs 0..32000 on add; duplicates are skipped (MFC does the same, `GroupManagerDialog.cpp:129-160`). "Hide checked" needs each object's script ID in the bridge's object read: `BkEditorObjectRecord` has no script ID field today (`bridge.h:189-198`), so append one (and update `c_bridge.zig` and the layout asserts).

### Pitfall 10: Reserve-position semantics and order
**What goes wrong:** MFC `push_front`s new positions (`TemplateEditorFrame1.cpp` `SaveReservePosition`) while D-01 says append; the game does not care about order (it keys by link ID). `vPos` is in AI units: the click goes through `Vis2AI` (`ObjectPlacerState.cpp` reserve branch). The MFC save drops a position when both link IDs are 0 (C4).
**How to avoid:** Append; convert with the truncation rule; refuse per C4; keep `std::list` order for untouched records.

### Pitfall 11: AI general sides, parcel units and the game's own angle mapping
**What goes wrong:** (a) Creating side records changes behaviour: `CGeneral::Init( const SAIGeneralSideInfo & )` always adds a `CGeneralTaskToHoldReinforcement` even for an empty side (`GeneralInternal.cpp:449-495`), while a side index beyond `sidesInfo.size()` uses plain `Init()` (`SupremeBeing.cpp:44-49`). (b) Generals exist only for sides other than the player's (`theDipl.GetMyParty() != i`), and not in network games or with `nogeneral` set (`SupremeBeing.cpp:39-45`), so a parcel on the player's own side is inert in a mission. (c) MFC stores angles as `wDir * 2*pi / 0xFFFF` and rotates reinforce points with trigonometry (`StateAIGeneral.cpp:158-200, 300-330`), while the game reads them through `GetVectorByDirection( WORD )`, a piecewise-linear then normalised mapping (`AIGeometry.cpp:49-72`) and a complex product (`GeneralIntendant.cpp:480-484`, `operator^` = complex multiply, `Misc/Geometry.h:36`). The two agree at multiples of 8192 (45 degrees) and drift by up to a few degrees between; `0xFFFF` vs 65536 adds a further 1.5e-5.
**How to avoid:** Store what the MFC formula stores (parity). Create a missing side and all lower ones on first edit of that side and record the previous vector size so undo restores it. The game-reads-it check for a parcel uses direction 0 and compares the position the game prints in `BK_MAP_TRACE`; do not assert exact equality at arbitrary angles.

### Pitfall 12: Script file name form, and silent script failure
**What goes wrong:** The game strips everything up to the last `\` from `szScriptFile`, prepends the directory of the map's own storage name and appends `.lua` (`iMissionInternal.cpp` script block after the rivers-border block; `Scripts.cpp:111-128`). The MFC editor's browse strips path and the last four characters (`MapOptionsDialog.cpp` `OnGetFile`). A script with a Lua error is not run at all: `ReadScriptFile` returns false, `Init` is never called, and errors show only when the global `ShowScriptErrors` is set (`CScripts::Init`, `Scripts.cpp`).
**How to avoid:** Store a bare name: no separators, no `..`, no `.lua` suffix, characters limited to letters, digits, `_`, `-`, `.` (a name read from a file is kept verbatim until the user changes it). Save As of a shipped map and Test in game copy `<name>.lua` (overwrite; stale copies from an earlier test are otherwise picked up). The core `Files` interface has `exists/copy/rename/delete/realPath` but no directory listing (`files.zig:21-66`); the dialog's ".lua files beside the map" needs a new `list` method (with a fake implementation).

### Pitfall 13: The input layer only knows the left button
**What goes wrong:** `view_math.kindOf` drops every non-left button (`view_math.zig:432-435`); `left_button_down` and `staleGesture( buttons, panning, left )` assume one button; SDL sends a double click as a press/release with `clicks == 1` first and then another with `clicks == 2`, so a double-click "commit" arrives after the single click already added a point (MFC has the same order and relies on `UniquePolygon` dedupe, Pitfall 1); Ctrl+click is a left click with `SDL_KMOD_CTRL` on macOS (C13).
**How to avoid:** Add right-button state next to `left_button_down`, extend `staleGesture` and its tests, take `event.button.clicks` for double click, and keep the Ctrl->right conversion per tool. `BK_EDITOR_AUTO` needs `rclick=`, `dblclick=`, `INSERT` (its named keys today: DELETE, HOME, ESCAPE/ESC, SPACE, ENTER/RETURN, TAB, UP, DOWN, LEFT, RIGHT, END, BACKSPACE, `auto.zig` header) and a way to switch tools and run panel commands (`tool=`, `do=`). `AutoRunner.push` sets `clicks = 1` (`smoke.zig:1799`); a double click sends two pairs, the second with 2.

### Pitfall 14: Script areas are AI units, keyed by name
**What goes wrong:** `CScripts::InitAreas` stores `areas[szName] = area` (`Scripts.cpp:106-110`), so duplicate names silently collapse (the last wins) and Lua looks areas up by that exact string; `GetScriptAreaParams` returns `center` and radius or half-size **raw** (`Scripts.cpp:2242-2263`), i.e. AI units. `BkEditorWorldToMap` returns the **unrounded** conversion (`Vis2AIFast`, `bridge.h:621-625`), while the MFC rule truncates (`Vis2AI`: `int( x + 0.3f )`, `fmtTerrain.h:29`), radius through `x` only (`TemplateEditorFrame1.cpp:3535-3555`).
**How to avoid:** Apply the truncation in the bridge (or the geometry unit) once when a new or edited area is stored; refuse empty and duplicate names case-sensitively (Lua is); untouched areas stay byte-exact (D-21). MFC quirk not to copy: it converts Vis->AI->Vis in place on every save, so its areas drift.

### Pitfall 15: Camera anchors
**What goes wrong:** "Set camera for player N" stores into `playersCameraAnchors[N]`, the neutral one into `vCameraAnchor` (the neutral slot is index `diplomacies.size() - 1`, `TemplateEditorFrame1.cpp:5035-5051`). The vector may be shorter than N + 1 in a file (C8). The MFC value is the screen-centre ground point with its terrain height (`GetPos3`); the bridge's `BkEditorScreenToWorld` answers on the z = 0 plane (`bridge.h:592-605`), and `ICamera::SetAnchor( const CVec3 & )` takes a full vector (`Camera.h:41`).
**How to avoid:** Pad with `VNULL3` on demand, never shrink; give the anchor the terrain height at (x, y) [ASSUMED that z matters for where the game frames the view; the game-reads-it camera trace measures it]. Test in game starts at player 0's anchor (`iMissionInternal.cpp` anchor order quoted in D-22); an unset (`VNULL3`) anchor falls back exactly as the game does.

### Pitfall 16: `FindReferences` and existing tests
**What goes wrong:** `FindReferences` compares reinforcement `ids[j] == nLinkID` (`MapOverlay.cpp:106-113`), which are script IDs, and ignores entrenchments, reserve positions and `mobileScriptIDs`. Tests asserting the old refusal: see C11.
**How to avoid:** Fix per D-04 and C3; the status-bar summary comes from the same function; ignore link ID 0.

### Pitfall 17: Screen picking is on the z = 0 plane
**What goes wrong:** `GetPos3`'s terrain ray cast never resolves in the bridge's headless session, so `BkEditorScreenToWorld` and `BkEditorWorldToScreen` both use the z = 0 plane (the `BkEditorWorldToScreen` decision in `STATE.md`; `bridge.h:608-619`). On hills a click lands where the z = 0 ray meets the ground plane, not the visible terrain point. Markers are drawn through the same conversion, so a marker sits under the cursor, but a road's `UpdateZ` height follows the altitudes, so the drawn stripe and the control markers can look offset on steep ground.
**How to avoid:** Accept and note it (same as M1's object placement); draw road handles at `point + z` only if the conversion learns z (out of scope); make the M2 scenario and the game-reads-it map flat ground.

### Pitfall 18: A Zig test in a file nothing imports never runs
**What goes wrong:** `test-editor-core` compiles `core/root.zig` and `refAllDecls`; `test-map-editor-panels` compiles `panels_logic.zig`; `test-map-editor-view` compiles `view_math.zig` (and `view.zig` via the built app); `test-map-editor-auto` compiles `auto.zig`. A new file with tests that is imported by none of them is silently not tested.
**How to avoid:** Import each new pure Zig file from the matching root, or add a step; run the step and see the new test names in the output.

### Pitfall 19: Windows specifics
**What goes wrong:** Engine paths use backslashes and the bridge takes engine-form paths (`Data\Maps\...`, `bridge.h:66-72`); the packaged `MapEditor.exe` is a GUI-subsystem program and prints only through `crt.attachParentConsole()`; the GOG game folder `D:\GOG\Blitzkrieg` on win-home is read-only (stdout only).
**How to avoid:** New verbs and modes print through the existing routes; Windows runs happen on win-home in the repo checkout, never in the GOG folder; artefacts go under `zig-out/local-test`.

### Pitfall 20: The MFC trench code uses a two-argument `fabs`
**What goes wrong:** `fabs( a, b )` in the trench builder is the project's Euclidean length (`Misc/Geometry.h:53`, `fabs( const CVec2 & )` calls `fabs( a.x, a.y )`); `GPoint` is an integer point, so every trench point is truncated; angles go through `CAngle`, which wraps with `fmod` into [0, 2pi) (`RoadDrawState.cpp:184-237`). A port that reads `fabs( x, y )` as two absolute values, or keeps floats where MFC truncates, produces different pieces.
**How to avoid:** Port with `hypot`, integer truncation at the same places and the same wrap; test with properties (Plan notes 04-04), because the MFC editor cannot be run here for a golden output.

## Code Examples

### Frame-index packing (the file's definition)
```cpp
// Source: Sources/src/RandomMapGen/MapInfo_Types.h (TPackFrameIndex), used at session.cpp:692-694
const int nType = pStats->GetTypeFromIndex( pInfo->nFrameIndex );   // e.g. BRIDGE_SPAN_TYPE_BEGIN = 0x00000001
if ( nType != -1 )
	pInfo->nFrameIndex = nType;
// snapshot record: the type. working record and engine object: pStats->GetIndexFromType( nType, &nSeed ) (seeded overload)
```

### River passability probe for the engine tier
```cpp
// Source: Sources/src/AILogic/AIEditorInternal.cpp:411-441 (CanAddObject), AILogic.h IAIEditor
// A unit rectangle centred on a river point: false while the river's tiles are locked.
IAIEditor *pAI = GetSingleton<IAIEditor>();
SMapObjectInfo probe;  probe.szName = "<any SGVOGT_UNIT, e.g. a tank>";  probe.vPos = CVec3( riverCentreAI.x, riverCentreAI.y, 0 );
const bool bBlockedWhileRiverExists = !pAI->CanAddObject( probe );
// after BkEditor river delete: expect CanAddObject( probe ) == true
```

### Bridge span plan (from `RoadDrawState.cpp:71-109`, `969-1031`, `TemplateEditorFrame1.cpp:3168-3179`)
```
input : first,last (vis, already locked to one axis: horizontal keeps y, vertical keeps x)
        stats : direction, L = span(lines[0]).fLength * fWorldCellSize / 2, origin(beginSpan)
HORIZONTAL: if first.x > last.x swap;  p0 = FitVisOrigin2AIGrid( first, origin );  n = int( ( last.x - first.x ) / L )
            positions (vis) = { (p0.x - 0.1, p0.y) } + { (p0.x + ( i + 0.5 ) * L, p0.y) for i < n } + { (p0.x + n * L, p0.y) }
VERTICAL  : swap on y;  positions = { (p0.x, p0.y) } + { (p0.x, p0.y + ( i + 0.5 ) * L) } + { (p0.x, p0.y + n * L + 0.1) }
each span : Vis2AI (truncate) ; HORIZONTAL first: x -= 0.1 ; VERTICAL last: y += 0.1
            frame type: first BEGIN(1), last END(4), others CENTER(2) ; fHP 1 (or -1) ; nDir 0 ; nPlayer 0 ; nScriptID -1
record    : objects.push_back x count ; bridges.push_back( { link IDs in span order } )
```

### Trench piece rules (from `RoadDrawState.cpp:1408-1584`)
```
terminators : first at points[0], angle = line( p0, p1 ) + pi ; last at points[n-1], angle = line( p[n-2], p[n-1] )
pieces      : one per consecutive pair, at the midpoint, nDir = int( angle / 2pi * 65535 )
              length > 0.9 * lineWidth  -> straight: alternate FIREPLACE / LINE starting with FIREPLACE (switcher = false first)
              else                      -> ARC ; angle += pi when ( previousAngle - angle ) <= pi (type 2)
sections    : the first section starts with the begin terminator; a section is closed right before the first straight piece that follows an arc,
              and the next section starts with that straight piece; the end terminator is appended to the last section
record      : entrenchments.push_back( sections ) after every piece and terminator object is appended to objects
```

## Plan Notes (what each plan needs that the code does not say obviously)

### 04-01 Foundations
- **Spec edits** (`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`): the M2 row in "The editor set" (line 24); the preservation-invariant paragraph and "Out (later milestones)" (74-87); "Saving: the snapshot and the overlay" (244-268) gets the record-level rules of D-01; "References to deleted objects" (312-318) becomes the cascade with its two refusals; the data-only variant sentence in "Architecture, 1. Engine bridge" (99-104) is wrong (C5); "What equivalent means" (538-580) gets the M2 expected-value operations; "Errors, Edit" (514-517) drops "So is a refused delete of a referenced object" except for spans and passengers; write D-23's closure of camera rotation with the 03-06 numbers already quoted in the M1 scope paragraph (33-51); add the fifth not-copied MFC rewrite (C8).
- **Overlay and builder:** new `MapRecords` operations over `SLoadMapInfo` for every collection; `SAddObject` gains `nFrameIndex`, `fHP`, `nScriptID`; `NextLinkID` unchanged. The byte-for-byte guarantee for untouched records comes free because edits mutate the vector in place; the map-file tests prove it (untouched collection bytes equal the unedited save).
- **Cascade delete:** extend `STombstone` (`session.h:56-64`) with the references it removed or edited (index and record), so `BkEditorRestoreObject` puts them back in reverse order; `BkEditorDeleteObject` reports what it changed (message and a summary count). Keep the passenger (`nLinkWith`) and bridge-span refusals. Rules: link ID 0 matches nothing; start command: remove the ID from `unitLinkIDs` (erase the command when empty), clear a target `linkID` to 0; reserve position: erase for artillery or truck (C3 recommendation); `mobileScriptIDs` and reinforcement groups reference script IDs and are left alone, with a status note when the last object carrying a referenced script ID goes.
- **Palette filter (D-05):** `catalogue.cpp:113` computes `placeable` from `WhyNotAMapObject` and `WhyNotPlacedAlone`; `AddObjectToSession` refuses through the same two (`session.cpp:665-675`). `WhyNotAMapObject` also guards `PlaceOneObject`, so it must not learn about bridges (loaded spans would stop being placed). Add the refusal to `WhyNotPlacedAlone`'s family (a third function used by the catalogue and `BkEditorAddObject` only), with `SGVOGT_ENTRENCHMENT = 4`, `SGVOGT_BRIDGE = 6`, `SGVOGT_FENCE = 9` (`Main/GameDB.h:27,29,32`) [VERIFIED], and mirror it in `panels_logic.zig:isPlaceable` (today `5, 100 => false`). The new tools use a separate internal add.
- **Input, registry, markers, verbs:** Patterns 5 and Pitfall 13. Keep M1's keys.
- **Game seam:** implement `BK_MAP_TRACE` and the `Trace` mirror here so plans 02-07 can assert on it (Pattern 6).
- **Render proof (D-09 last paragraph):** an existing capture already shows road stripes on GFXGPU: `zig-out/local-test/03-15-release-check.tga` (coldwinter, 1280x800) has wheel-track roads at the upper right and lower left [observed]. The 04-01 measurement should still be numeric: engine tier, arnheim (rivers with animated layers): capture, `RemoveRiver`, capture again and count differing pixels above a threshold; two captures a second apart must differ inside the river's polygon (the animated layer moves). Write to `zig-out/local-test`.
- **Size warning:** 04-01 carries the spec, overlay, cascade, generic command, registry, markers, verbs, palette filter, game seam and render proof. Order its tasks so the extension points land first (record command plus fake bridge, event/key extension, registry, verbs), then cascade and palette.

### 04-02 Roads and rivers
- **Bridge surface:** list descriptors for the season (`working.szSeasonFolder + "Roads3D\\"` / `"Rivers\\"`, names without extension, sorted; enumerate through `IDataStorage`, using the helper the MFC editor uses, `EnumFilesInDataStorage` with `GetSingleton<IDataStorage>()`, `TemplateEditorFrame1.cpp:4850-4895`); count/get/add/edit/put/delete per kind; pick (`TerrainHitTest`, cycle through overlaps with repeated right-click as `CVSOSelectState::OnRButtonDown` does); `VsoMatchesEngine`; a river-block probe (test only).
- **Gestures, from `VectorStripeObjectsState.cpp`:** hit radii `CONTROL_POINT_RADIUS = fWorldCellSize / 5.0f`, `KEY_POINT_RADIUS = fWorldCellSize / 3.0f` (vis units, lines 16-19); a key point's width handle is at `point.vPos +- vNorm * fWidth`, height-fitted with `UpdateZ`; control-point drag: `CW_SINGLE` moves the point, `CW_MULTI` moves it and everything after it (with Ctrl, everything before it), `CW_ALL` moves all (`130-176`); width drag: new width is `fabs( vNorm * ( shift . vNorm ) )` (`179-206`), the width spinner shows `int( 0.5f + width * 2 / fWorldCellSize )`; right-drag vertically sets opacity `activeOpacity + shiftPixels / 100.0f` clamped to 0..1, all points in `CW_ALL` mode (`217-257`); Insert adds the midpoint after the active control point (before it when it is the last); Delete with a control point grabbed removes that point (ignored while only 2 remain), and with no point grabbed removes the whole road/river (`506-598`); Enter/Space ends editing. **Quirk to decide:** MFC applies Insert and Delete only while the left button is still held down on the point (`activePoint.isValid` is set on press and cleared on release, and the key handlers also require the recorded press); on a trackpad that is awkward, so the portable tool applies them to the last-grabbed or hovered control point (a recorded parity deviation); adding: click adds, right-click removes the last point (pops two, pushes the cursor), double-click or Enter/Space creates the object (`612-741`). There is no Escape in MFC; D-08's Esc-cancels is an addition.
- **Tool parameters:** width 1..16, default 3 (`fWidth = w * fWorldCellSize / 2`, so the default is about 67.88 vis units; `CVSOBuilder::DEFAULT_WIDTH = 120.0f` is not what the tool uses), opacity 0..1.
- **Engine sync:** Remove+Add per edit (Standard Stack), river AI per Pitfall 4, `UpdateZ` from the altitudes (heights only; `MakeWorkingCopy` recomputes shades on the working copy alone, which `UpdateZ` does not read; it never changes altitudes, D-09). The same derived record is written to `snapshot.terrain` (what is saved) and `working.terrain` (what the engine and the AI were built from); the save path never writes `working` (`session.cpp:346-350`).
- **Commands:** `vso_add` / `vso_edit` (before and after full records, drag merged by gesture) / `vso_delete`, with the saved `nID` inside the record.

### 04-03 Bridges and fences
- **Drawing:** Pattern 4 and the plan pseudo-code above; stats come from `SBridgeRPGStats` (`GetSpanStats`, `GetOrigin`, `states[0].begins/lines/ends`). The ghost is best drawn as app markers from the plan (no engine objects while dragging); the MFC editor makes scene-only span visuals (`m_tempSpans`) and a single translucent AI object (`tmpSpan`, opacity 128), which the bridge should not imitate (an AI object locks tiles).
- **Picking a group:** `ObjectAt` skips `SGVOGT_BRIDGE` and `SGVOGT_ENTRENCHMENT` (`session.cpp:993`). Add a second query (`BkEditorPickGroup( sx, sy )` returning kind and index) instead of changing the meaning of `BkEditorObjectAt`, so the M1 Select tool keeps working while the Bridge/Entrenchment tools use groups.
- **Rotate:** the partner name swaps `_01` and `_02` (case preserved, `W_` prefix kept); refuse when `GetDesc( partner )` is null, when either list of index lists is empty, when a span would be off the map or refused by the engine; the new group is planned about the old group's centre (mean of the first and last span positions), same span count, rebuilt along the other axis, and swapped in as one tombstoned edit (old group removed, new group inserted at the same `bridges` index so list order is kept).
- **Built during play:** allowed only when the span name contains `WoodenBig_Heavy_` (this matches `W_WoodenBig_Heavy_01/02` too, `RoadDrawState.cpp:1253`); snapshot and working `fHP` become -1 / 1 (the game reads the map's -1 to build later; the engine cannot take a negative HP, `session.cpp:146-170`); apply the visual mark (C1) and re-apply it on undo, redo, rotate and reopen. 05-PARITY VO3's wording is corrected in 04-08.
- **Fences (D-14):** algorithm in `RoadDrawState.cpp:519-622, 841-898, 1034-1107`. The tile mapping must be `ITerrainEditor::GetAITileIndex`; expose it (a `BkEditorWorldToAITile`) or prove in the engine tier that the static `CMapInfo::GetAITileIndices` gives the same tiles. One fence per two AI tiles along the run (the loop advances the iterator twice), packed frame `( 1 << dir ) | FENCE_TYPE_NORMAL` with `dir` in {0, 1, 2, 3} from `GetCenterIndex` (horizontal drag: 1 when the drag goes left, else 3 with the `+2` tile shift; vertical: 0 when it goes up with the `-2` shift, else 2); single fence: 0, or 1 with Ctrl. `nPlayer 0`, `fHP 1`. The MFC refusal box uses `patches.GetSizeX() * 16 - 1`; refuse any tile whose `>> 1` cell falls outside.

### 04-04 Entrenchments
- **Data model:** each trench is one `entrenchments[i]` (a list of sections, each a list of link IDs) plus its piece objects appended to `objects`; the desc name is `"Entrenchment"` (`RoadDrawState.cpp:249-251`); pieces use `nPlayer` from the tool, `fHP 1`, `nScriptID -1`, frame type per Pitfall 5, `nDir = int( angle / 2pi * 65535 )`.
- **Builder:** polyline of clicked points, with connector arcs generated by `CConnector` when the turn to the cursor exceeds 30 degrees (`RoadDrawState.cpp:293-364`, first arc point at 15 degrees then 30 degrees each, the shorter of the clockwise and anticlockwise runs), straight runs split by `SplitLineToSegrments` (`260-290`, integer points), commit at double-click (`1408-1584`); right-click clears the path (`1388-1398`); Delete on a hovered piece removes the whole entrenchment (`1312-1358`); hover highlights the whole entrenchment (`796-839`). The MFC editor never calls `IAIEditor::AddNewEntrencment` (no caller in `MapEditor/`); pieces are ordinary `AddObjectByAI` objects and the game groups them in `LoadEntrenchments`. Do the same: no engine grouping call.
- **Verification without goldens:** the MFC editor cannot be run in this environment, so prove fidelity by properties over many random polylines: first and last objects are terminators with opposite orientation; consecutive piece centres are spaced by the piece width; straight runs alternate fireplace and line starting with fireplace; an arc always separates two sections; no section is empty; the union of sections equals the set of new objects; the plan function is deterministic (twice equal). Keep the integer truncation (Pitfall 20).
- **Scope guard:** script ID and garrison on a trench are M3 (05-PARITY O18).

### 04-05 Script IDs, groups, script file, areas, camera anchors
- **Script ID (D-15):** field in the single-selection Properties panel; edit snapshot and working only (C7); one command reusing the `place`-style before/after; merge by gesture. Add `script_id` to `BkEditorObjectRecord` (Pitfall 9).
- **Groups (D-16):** `groups` is an `unordered_map< int, SGroupsVector >`; edit calls are `SetGroup( id, ids[] )` and `DeleteGroup( id )` (records keyed by ID, not index; the tombstone remembers nothing else because the map has no order). "Select objects" selects the objects carrying any of a group's script IDs; the Select tool is single selection today, so this highlights them with markers and selects the first. "Hide checked": mirror of what the game does (units of a group are held back, `AILogicInternal.cpp:463-486`, `objects` only): draw them dimmed with a marker, or set their visual opacity to 0 (`IVisObj::SetOpacity( BYTE )`, `Scene.h:222`; whether picking still finds an invisible object is [ASSUMED], check in the engine tier).
- **Script file (D-20):** Pitfall 12. Dialog lists `.lua` files beside the map (new `Files.list`), "Choose other..." opens `SDL_ShowOpenFileDialog`, copies beside the map (ask before overwrite), stores the bare name; "Open script" calls `SDL_OpenURL` with a `file://` URL built from a validated path (never a user string) [ASSUMED that the OS opens a `.lua` sensibly on both platforms; manual check]. A missing file is a status warning. Test in game: after `BkEditorTestMapPath`, copy `<name>.lua` beside `mapeditor_test.bzm` in the returned directory (overwrite).
- **Areas (D-21):** MFC geometry: rectangle `center = ( first + last ) / 2`, `vAABBHalfSize = abs( first - last ) / 2`; circle `center = first`, `fR = length( first - last )`; conversions Pitfall 14. Move and resize handles are an addition to MFC (MFC only draws, adds and deletes). List click calls `BkEditorSetCamera` at the area's centre in world units (AI units times `fAITileXCoeff`, `AI2Vis`).
- **Anchors (D-22):** Pitfall 15; the player index is the Objects panel's player; list with Go to and Clear (Clear = `VNULL3`; a neutral clear = `VNULL3` too).

### 04-06 Start commands and reserve positions
- **Command types:** `editor\actions.ini` through the storage (`AIStartCommand.cpp:22-46` reads `[actions]` as name = id in file order; the default is entry index 9, `DEFAULT_ACTION_COMMAND_INDEX = 9` (`AIStartCommand.cpp:17`), which is `STOP = 9` in the shipped file and `ACTION_COMMAND_STOP = 9` (`Common/Actions.h:420`)). Expose the list from the bridge; do not read the file in Zig.
- **Record:** `SAIStartCommand { cmdType, unitLinkIDs, linkID, vPos (AI units), fromExplosion, fNumber }`; the MFC target position is entered in map tiles (`vPos = value * 2 * TILE_SIZE` in `SetVPosX` / `SetVPosY`, `AIStartCommand.cpp`), which is the same AI unit as the click's `Vis2AI`. Editing replaces the whole record and keeps `fromExplosion`. A soldier stands for his squad's link ID (the document's objects are already squads, and `BkEditorObjectAt` maps a soldier to his squad), deduplicated, link 0 skipped (`TemplateEditorFrame1.cpp:2994-3038`).
- **Reserve positions:** role classification, towing check and the towed-gun-needs-truck rule (`ObjectPlacerState.cpp:525-617`, C4) live in the bridge; the panel lists positions, draws gun -> truck -> spot lines, deletes; Enter commits (`CObjectPlacerState::OnKeyDown`, `ObjectPlacerState.cpp:627-641`). `vPos` in AI units via `Vis2AI` on the ground click.
- **Red lines:** markers from each listed unit to the target; nothing to do in the engine.

### 04-07 AI general
- **Record:** `SAIGeneralMapInfo { sidesInfo[ side ] { mobileScriptIDs, parcels[ { reinforcePoints[ { vCenter, wDir } ], eType, vCenter, fRadius, wDefenceDirection } ] } }` with `EPATCH_DEFENCE = 1`, `EPATCH_REINFORCE = 2` (`fmtAIGeneral.h`). One edit call per gesture that returns the whole side (small records), stored before and after; sides created on demand (C8, Pitfall 11).
- **Gestures** (`StateAIGeneral.cpp`): click outside every parcel makes a defence parcel with radius 256 AI units (C2) and direction 0; a click inside a parcel (distance to centre less than `fRadius * fAITileXCoeff` in vis units) adds a reinforce point stored relative to the centre and rotated by minus the defence angle (`RotatePoint( -( wDefenceDirection * FP_2PI / 0xFFFF ) )`), `wDir = 0`; drag the centre handle moves the parcel, drag the arrow handle sets radius (minimum 256) and direction (`fAlpha = polarAngle( arrow - centre ) - pi/2`, wrapped to 0..2pi, `WORD( fAlpha * 0xFFFF / 2pi )`), drag a point handle moves it, drag a point's arrow sets its `wDir`; Enter/Insert/Space toggles the type, Delete removes the point or the parcel. Handle radii (vis units): parcel arrow `fWorldCellSize / 2`, parcel centre `fWorldCellSize`, point arrow `fWorldCellSize / 2`, point centre `fWorldCellSize / 1.5` (`StateAIGeneral.cpp:22-41`). Draw colours per type index 0..2 (`PARCEL_COLORS`, `POSITION_COLORS`).
- **Mobile script IDs:** add/remove ints, no duplicates.

### 04-08 Integration and exit
- **Sweep (D-25.4):** two parts. Record-level operations (roads/rivers via `CVSOBuilder`, start commands, reserve positions, areas, groups, AI parcels, anchors, script file) run in the map-file tier over `Data/Maps` (data-only startup, fast, all five targets could run it); bridge/fence/trench edits and cascades need the object database, so they run in the engine tier locally (`--m2-sweep`, macOS). Both: apply one edit of each kind present, undo everything, save, compare the file bytes to an unedited save of the same map, then run `AreEquivalent`.
- **Game-reads-it M2:** add a mode next to `gameReadsIt` (`main.zig:535`) that builds the map through the core `Editor` on `RealBridge` (so the map is really editor-made), test-launches it and parses the log for: script ran and found the area and the group, camera at the anchor, `bridges=N`, `entrenchments=N`, `startcmds`, `reserve`, `general` lines. The shot (`BK_AUTO_UI` `shot`) shows the road and bridge; compare with a seeded local reference as `map-editor-auto` does.
- **CI:** dispatch `cross-platform.yml` on the branch (`gh workflow run`), collect the six job results; the engine tier passes on `macos-14` and `windows-latest` (MSVC) and reports skipped elsewhere. Windows local run on win-home in `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg`.
- **Parity evidence:** fill `04-PARITY.md` "Closes with" for every row, correct VO3 wording, copy to `05-PARITY.md`.

## State of the Art

Not applicable in the usual sense (a fixed 2003 engine and a repo-internal editor). The only "current versus old" facts that matter:

| Old approach (MFC editor) | M2 approach | Impact |
|---|---|---|
| Save rebuilds the whole map from UI state and the engine, renumbers links (`HandOutLinks`), recomputes shades, resizes camera anchors | Snapshot plus record-level overlay, read-back verified | Untouched records are byte-identical; sweep can prove it |
| Random sprite index per span/fence/piece, saved as packed type | Packed type computed, working index seeded | Tests are deterministic (C6) |
| Engine's `GetTerrainInfo()` is mutated in place and is what gets saved | Bridge copy is saved; engine follows by Remove+Add | Engine is never the source of the file (D-01) |

## Assumptions Log

Every claim tagged `[ASSUMED]` above and here needs confirmation before it becomes a locked decision; each is cheap to test.

| # | Claim | Section | Risk if wrong |
|---|---|---|---|
| A1 | `IObjectsDB::LoadDB` can run in the data-only startup without a renderer (`GameDB.cpp` uses only `IDataStorage`; the bridge's comment says LoadDB reads textures) | C5, Pattern 4 | Low: the geometry unit takes plain numbers, so the map-file tier does not need it; only the optional "expected values from real stats" would |
| A2 | The game uses the camera anchor's z when it frames the start view | Pitfall 15 | Low: a start view offset on hilly maps; measured by the camera trace in the game-reads-it tier |
| A3 | A start command naming a held-back (reinforcement) unit passes a null to `RegisterGroup` and crashes | Pitfall 8 | Medium: if it does not crash, the warning is only advice; if it does, a refusal may be warranted |
| A4 | `SDL_OpenURL( "file:///...lua" )` opens a usable editor on macOS and Windows | 04-05 | Low: manual check only; fall back to showing the path |
| A5 | The game finds `<generated root>\maps\<basename>.lua` through its composite storage | 04-05, D-20 | Medium: the game-reads-it script trace proves it; if not, the test copy needs another location |
| A6 | `IScene::Pick` still finds an object whose visual opacity is 0 | 04-05 (Hide checked) | Low: choose the marker-only route instead |
| A7 | On a road or river A/B capture, the animated layer changes pixels between two frames a second apart | 04-01 render proof | Low: if it does not, measure the static stripe only and note that animation is unobserved |
| A8 | Windows engine-tier time grows only marginally with about 25 more cases | C12 | Low: last full run 1h18m against a 110 minute limit |
| A9 | `AILogic::InitEditor` river/road loading happens for every opened map (`LoadMap( terrainInfo )` default argument) | Pitfall 4 | Low: verified in `AIStaticMap.h:84`; the arnheim probe test proves it |

## Open Questions

Each has a recommended default so the run can continue without asking.

1. **D-04's truck clause.** What we know: MFC erases the whole position for either role and refuses towed guns without a truck (C3, C4). Unclear: whether the user wants the friendlier "clear the truck" anyway. Recommendation: erase the position (MFC parity), log the amendment.
2. **Bridge and fence ghost.** MFC shows scene-only span visuals and a translucent AI object. Recommendation: draw the ghost as app markers from the plan (outline and span centres); no engine objects while dragging.
3. **"Hide checked".** Recommendation: dimmed markers plus a status count first; switch to visual opacity 0 only if A6 holds.
4. **`BK_MAP_TRACE` line format.** Recommendation: one `key=value` line per item, prefix `BK_MAP_TRACE: `, for example `camera x=.. y=.. z=..`, `script name=.. loaded=1 init=1`, `area name=.. cx=.. cy=..`, `group id=.. held=N`, `bridges n=N`, `entrenchments n=N`, `startcmd launched=N`, `reserve applied=N`, `general side=I parcels=N mobile=N`, `parcel side=I idx=J type=T cx=.. cy=.. r=.. dir=..`.
5. **Where the M2 scenario schedule lives.** `map-editor-auto` keeps its schedule as a string in `build.zig`. Recommendation: a second step `map-editor-auto-m2` with its schedule as an array of entries joined in `build.zig` (still one `BK_EDITOR_AUTO` string), so the M1 step stays untouched.
6. **AI tile mapping for fences.** Recommendation: add a small bridge call that wraps `ITerrainEditor::GetAITileIndex` and use it; assert equality with the static helper once in the engine tier.
7. **Insert/Delete of a control point on a trackpad.** Recommendation: apply to the last-grabbed or hovered control point (parity deviation, recorded).
8. **`Files.list`.** Recommendation: add `list( dir, extension, out )` to the `Files` interface with `StdFiles` and `FakeFiles` implementations (the core stays std-only).
9. **Should 04-01 be split?** D-24 locks eight plans. Recommendation: keep eight, but order 04-01's tasks as in its plan note and treat the game seam and the render proof as the last two tasks so a slip there does not block 04-02.

## Environment Availability

| Dependency | Required by | Available | Version | Fallback |
|---|---|---|---|---|
| Zig | every step | yes | 0.16.0 (`/opt/homebrew/bin/zig`) | none needed |
| macOS arm64 GPU (metal) | engine, app, game tiers | yes per spec and 03-15 evidence (not re-probed) | metal | none; CI runs the engine tier on macos-14 |
| `gh` CLI | CI dispatch and run inspection | yes (authenticated; `gh run list` worked) | - | none |
| `ssh win-home` | Windows local run | yes (`ver` answered `Microsoft Windows [Version 10.0.26100.9278]`) | - | CI `windows-latest` |
| Repo at `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg` (win-home) | Windows run | not re-probed this session | - | fetch/pull, then build there |
| Free disk on the Mac | any build | 11 GiB free of 460 GiB (`df -h /`) | - | no package builds on the Mac; keep `zig-out/local-test` artefacts small; clean old logs |
| `sips`, `ffmpeg` | inspecting TGA shots as PNG | yes | - | none needed |
| Node and `gsd-tools.cjs` | planning tooling | yes | - | none needed |

**Missing dependencies with no fallback:** none.
**With fallback:** none needed; the Windows repo path and free disk are the two things to re-check at 04-08.

## Validation Architecture

`workflow.nyquist_validation` is absent from `.planning/config.json` (only `projectName`, `created`, `workflow.use_worktrees`, `primaryLanguage`, `targetPlatform`, `toolchain`, `includeResearch`, `nextStep`), so this section is required.

### Test Framework
| Property | Value |
|---|---|
| Framework | Zig `std.testing` (core, panels_logic, view_math, auto, testlaunch); C++ executables with a `Check()` harness (`tools/zig/map_file_test.cpp`, `tools/zig/editor_bridge_test.cpp`); Zig engine test (`Sources/editor/app/c_bridge_test.zig`); scripted app runs (`smoke.zig`, `BK_EDITOR_AUTO`); game harness (`--game-reads-it`, `BK_AUTO_UI`) |
| Config file | none: steps are defined in `build.zig` (`test-editor-core` 2778, `test-map-editor-view` 2792, `test-map-editor-panels` 2810, `test-map-editor-testlaunch` 2824, `test-map-editor-auto` 2838, `test-map-files` 5696, `test-map-files-all` 5710, `test-editor-bridge` 5829, `test-map-editor-engine` 6110, `map-editor-auto` 6074, `map-editor-game-reads-it` 6090) |
| Quick run command | `zig build test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false` (the same Zig unit tiers CI runs; `zig build test` also runs them plus the platform tiers) |
| Map-file tier | `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false` (CI: Linux x64, Linux arm64, Windows MSVC, macOS arm64 and macOS Intel; not Windows MinGW) |
| Full local suite | `zig build test-editor-bridge test-map-editor-engine map-editor-host-check map-editor-smoke map-editor-game-reads-it map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` (the M1 exit command, 11 PASS/done lines) plus `test-map-files-all` for the sweep |
| After editing `build.zig` | `zig test tools/zig/build_hermeticity_test.zig` (never `zig fmt build.zig`) |
| Estimated runtime | quick about 2 minutes; full suite several minutes; CI 47 to 78 minutes |

### Phase Requirements -> Test Map

Each feature is proven on every tier that applies. "Core" = fake bridge, all six CI targets. "Map file" = data-only C++, five targets. "Engine" = real `ITerrainEditor` / `IAIEditor` through `test-editor-bridge` (C++) and `test-map-editor-engine` (Zig), macOS arm64 and Windows MSVC. "App" = `map-editor-auto` M2 scenario (macOS, local). "Game" = M2 `--game-reads-it` (macOS, local).

| Feature (decisions, PARITY rows) | Core | Map file | Engine | App scenario | Game reads it |
|---|---|---|---|---|---|
| Cascade delete and fixed `FindReferences` (D-04, O16 refs) | do/undo/redo restores every reference; refused span/passenger delete leaves document and history unchanged (fake models start-command and reserve references) | each reference kind vs the builder: start-command unit removed (empty command erased), target cleared, reserve position erased, groups and mobile IDs untouched, link 0 ignored, entrenchment sections found | delete a unit that has a start command and a reserve position, undo, save: equals the original; span delete still refused | select unit, Delete, Ctrl+Z, shot equals the pre-delete shot | - |
| Palette filter (D-05) | - | - | catalogue reports `placeable == 0` for game types 4, 6, 9 and `BkEditorAddObject` refuses; existing objects of those types still load and move | palette shows none of the three (panels_logic test in the panels tier) | - |
| Generic record commands and generation counters (D-02) | round trip per variant; drag merge drops the entry when it ends where it began | - | - | - | - |
| Roads and rivers (D-07..D-09, VO5-VO7) | tool gestures against the fake: add (click, click, double-click), Enter/right-click/Esc, drag control point in three width modes, drag width handle, right-drag opacity, Insert, Delete (point and whole); each is one undo step; refused "too short" | `CreateVSO`+`Update`+`UpdateZ` equals the bridge's record (deterministic, twice equal); add/edit/delete overlay vs builder; new road passes read-back (passability fix); nID above all in use | `VsoMatchesEngine` after every step; river: `CanAddObject` probe false while present, true after delete and after undo of an add, false again after redo; road: AI untouched; A/B capture proves stripes and animated layer draw | draw a road and a river, drag a point, undo, redo, save, compare shot | road and river present in the shot; `BK_MAP_TRACE` road/river counts |
| Bridges (D-10, D-11, D-12, VO2, VO3, S4) | draw all-or-nothing (refusal leaves history unchanged: wrong axis, off map, no room), rotate (partner missing refused), toggle, delete whole; undo orders | span plan from literals: positions, `.9`/`.1` nudges, packed types, `bridges[i]` order; overlay draw/rotate/toggle/delete vs builder | draw, rotate, toggle, delete through `IAIEditor`; every `bridges` link resolves after each step (`WorldMatchesMap`); built-during-play span carries the mark; group pick returns the whole group | draw, rotate, toggle, undo, redo, delete | `BK_MAP_TRACE bridges n=`; one bridge rotated, one built during play; shot shows the bridge |
| Fences (D-14, VO1) | drag one axis places every second tile; Ctrl flips a single fence; one step per drag | fence run plan vs builder (frame `( 1 << dir ) \| 0x00010000`, offsets +2/-2) | run placed; tile mapping equals `ITerrainEditor::GetAITileIndex` | draw a fence, undo | fences in the shot |
| Entrenchments (D-13, VO4, O18 drawing) | polyline commit, right-click cancel, hover group, Delete whole, undo | trench plan properties over many random polylines (terminators, alternation, arcs separate sections, no empty section, deterministic); overlay vs builder | draw and delete; every section link resolves; no `AddNewEntrencment` call needed | draw, hover, delete, undo | `BK_MAP_TRACE entrenchments n=`; `LoadEntrenchments` did not crash |
| Script ID and groups (D-15, D-16, G1) | field command round trip; group new (ID bump), delete, add/remove IDs (duplicates skipped); one step each | group ops vs builder; groups written sorted (bytes equal to an unedited save when untouched) | set ID, save, reopen, `GetObjectScriptID` equals | `do=` commands for group and ID, save | grouped unit is held back (`BK_MAP_TRACE group id= held=N`, units baseline as in M1) |
| Script file (D-20, M6) | bare-name validation (separators, `..`, `.lua` suffix, characters) | value from file kept verbatim until changed | Save As copies the script (`FakeFiles` in core; real file in the engine test) | Test in game with the copy | script ran (`Trace` mirrored) and found the area and the group |
| Script areas (D-21, MT2) | rectangle and circle tools, name rules (empty, duplicate), rename, delete, handles, one step each | Vis->AI truncation rule on new areas; untouched areas byte-exact | add area, save, read back equals expected; `GetScriptAreaParams` values | draw both kinds, rename, delete | `Trace` prints the area centre in AI units equal to the stored value |
| Camera anchors (D-22, M2) | set player N pads with `VNULL3`, never shrinks, neutral separately, clear, one step each | padding rule vs builder; no resize on open (bytes equal) | `BkEditorSetCamera` "Go to" moves the view | set anchor for player 0 | camera trace equals the anchor |
| Start commands (D-17, U1, U2) | add with unit, click target, dedupe, edit, delete; empty command erased on cascade | overlay vs builder, `fromExplosion` kept | add, save, reopen | add, list, Delete | `BK_MAP_TRACE startcmd launched=N` |
| Reserve positions (D-18, U3) | add with truck, without (allowed vs refused by role), delete | overlay vs builder | role classification and towing checks against real RPG stats; refusals leave the map unchanged | add position | `BK_MAP_TRACE reserve applied=N` |
| AI general (D-19, AI1) | click creates parcel (radius 256), click inside adds point, drags, type toggle, Delete; side creation and undo restoring the vector size | overlay vs builder; radius/direction conversions; MFC store formula | parcel read back through the engine's file path | create, drag, toggle | `BK_MAP_TRACE parcel ...` at direction 0 equals the stored centre and the point rotated by the store formula |
| Preservation sweep (D-25.4) | - | record-level edits + undo over every `Data/Maps` map; bytes equal to an unedited save; `test-map-files-all` stays 1,755 of 1,755 | bridge/fence/trench edits + undo over the same maps (`--m2-sweep`, macOS local) | - | - |
| Test in game with script and anchor (D-20, D-22) | - | - | `BkEditorTestMapPath` directory gets the copied script | `test`, `waitgame` | the M2 game-reads-it scenario in one run |

### Sampling Rate
- **Per task commit:** the core tier plus the tier the task touches (the quick command above); a C++ change adds `zig build test-map-files ... -Dtest-mode=run` and, when it touches the bridge, `zig build test-editor-bridge ... -Dtest-mode=run`.
- **Per plan (each of 04-02..04-07):** the full local suite command above, and `zig build map-editor-auto` with the plan's scenario segment.
- **Per wave merge:** the full local suite plus `test-map-files-all`; one CI dispatch on the branch after Wave 2.
- **Phase gate:** everything green, CI six jobs, the Windows run on win-home, then Johannes's hand try on `--release=fast`.
- **Max feedback latency:** 300 seconds for the quick loop.

### Wave 0 Gaps
- [ ] `Sources/editor/core` record types, `history.zig` variants and the fake-bridge extension (collections, cascade, references): every core test depends on them.
- [ ] `tools.Event`/`Key` extension, right-button and double-click routing in `view.zig`, `staleGesture` for two buttons, registry, marker layer, `commands.zig`.
- [ ] `auto.zig` verbs `rclick=`, `dblclick=`, `tool=`, `do=` and key names `INSERT`; parser tests in `test-map-editor-auto`; `AutoRunner` handlers in `smoke.zig`.
- [ ] `MapRecords`, `MapGeometry` and their entries in `build.zig` (`addMapFile`, line 3765) plus hermeticity run.
- [ ] `map_file_test.cpp` M2 cases and the M2 sweep entry; `editor_bridge_test.cpp` `TestM2*` functions (keep the Windows CRT-to-stderr routing already in `main`); `c_bridge_test.zig` M2 round trip.
- [ ] Game seam `BK_MAP_TRACE` plus the `Trace` mirror; `tools/zig/fixtures/m2_script.lua` (Lua 4 dialect, `function Init()` calling `GetScriptAreaParams`, `GetNUnitsInScriptGroup`, `Trace` with number arguments only).
- [ ] A/B render capture test for roads and rivers (04-01), artefacts in `zig-out/local-test`.
- [ ] `Files.list` (core), if 04-05 keeps the ".lua beside the map" list.

### Manual-Only Verifications
| Behavior | Why manual | Instructions |
|---|---|---|
| Trackpad feel of double-click finish and Ctrl-click as right-click | Real input cannot be produced from the agent shell | Johannes draws a road and a trench on the release build |
| "Open script" opens a sensible editor on macOS and on Windows | OS file association | Click the button with a real `.lua` beside a saved map |
| Visual mark of a built-during-play bridge and of Hide checked | Judgement of legibility | Look at the shot and the live view |
| GPU look of roads and animated rivers on Windows | CI engine tier checks numbers only | Look at a win-home run of the M2 scenario shot |
| The hand try (D-25.9) | User approval | `--release=fast`, `zig-out/game/macos/arm64/release` per memory |

## Security Domain

`security_enforcement` is not set in `.planning/config.json`, so it applies. This is a desktop editor that reads user files and writes them, copies user files, opens a URL and feeds a scripting engine, so input validation and file handling are what matter.

### Applicable ASVS Categories
| ASVS category | Applies | Standard control |
|---|---|---|
| V2 Authentication, V3 Session, V4 Access control | no | none: no accounts or network |
| V5 Input Validation | yes | Bounded ranges (width 1..16, opacity 0..1, script ID -1..32000, group IDs >= 0), name rules (non-empty, unique, fixed-size buffers with length checks), bare script names, link IDs > 0, refusal with a status note; C++ entry points check pointers, counts and finite floats as `SoundRecordWellFormed` does |
| V6 Cryptography | no | none |
| V7 Error handling and logging | yes | Every bridge entry `Guarded`; `BK_MAP_TRACE` is env-gated stderr and prints no paths beyond map-relative names |
| V12 Files and resources | yes | Script copy only into the map's own folder or the test-launch folder, overwrite only after asking (except the test folder), no symlink following (`Files.realPath`), bare file names validated with `NPlatform::Paths::IsRelativeDataName` as `BkEditorTestMapPath` does, shipped folders never written (`shipped.zig`) |
| V14 Configuration | yes | Build changes stay hermetic (`build_hermeticity_test.zig`) |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard mitigation |
|---|---|---|
| Script file name with `..`, separators, a drive letter or an absolute path ends up in `szScriptFile` or in a copy destination | Tampering, Information disclosure | Validate to `[A-Za-z0-9_.-]` with no leading dot and no `.lua` suffix; build every destination from a validated basename and a fixed directory |
| Overwriting a user's file when copying a script | Tampering | Ask first; never overwrite a file the map does not name; the test-launch copy overwrites only inside the generated-data folder |
| `SDL_OpenURL` receives a user-controlled string with another scheme | Elevation of privilege | Build a `file://` URL from a validated absolute path; never pass a typed string |
| A hostile or corrupt map crashes markers, picking or the game: empty `points`, `bridges` naming a missing link, huge `sections`, `sidesInfo` sizes, NaN positions | Denial of service | Guard every index and size in drawing and picking; treat a record the editor cannot draw as "unknown, kept as is"; never assert in these paths; the game loaders' asserts are compiled out (Pitfall 7) |
| A Lua script copied into the test folder runs inside the game | Elevation of privilege | It is the user's own file; Lua 4 exposes only the registered API (`Scripts.cpp` list) and the base libraries; the M2 fixture script is ours and reads only |
| Unbounded per-frame work: hundreds of markers, batched conversions | Denial of service | Draw detail only for the selected road; cap marker counts per kind |
| Fixed-size ImGui text buffers overflow on paste | Tampering | Use the existing `[name_capacity]u8` pattern with `ImGuiInputTextFlags` limits and check on commit |

## Project Constraints (from CLAUDE.md)

No `CLAUDE.md` exists in the worktree root (checked with `ls`). The directives that bind this phase come from the invocation and from the user's auto-memory (`MEMORY.md`), and are treated as locked:

- C++: never `std::min` / `std::max`; use `Min` / `Max` from `Misc/Tools.h`.
- Never `zig fmt build.zig` and never gate on `--check` (about 15 unrelated regions would reformat); after editing `build.zig` run `zig test tools/zig/build_hermeticity_test.zig`.
- Test artefacts, screenshots, logs go in `zig-out/local-test`, never `/tmp`.
- Mac disk is tight (about 12 GB): no package builds on the Mac; Windows testing through `ssh win-home` (repo at `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg`; `D:\GOG\Blitzkrieg` is read-only).
- Measure graphics artefacts (F9 capture, TGA) instead of guessing; headless capture limits apply (no screencapture from the agent shell; use `BkEditorCaptureFrame`, `BK_GFX_TRACE`, `run_in_background`).
- The hand try runs the release build (`--release=fast`, `zig-out/game/macos/arm64/release`).
- A failed `expect` before `w.destroy()` hangs worker tests: `defer` destroy.
- New C++ test executables copy the `gfxgpu-factory-test` recipe (no `linkSystemLibrary(stdc++)`, no `link_libc`); Windows test mains route CRT asserts to stderr (already done in `editor_bridge_test.cpp:3492+`).
- Copy binaries fresh (`rm` then `cp`) or macOS SIGKILLs the launch.
- Keep the Co-Authored-By trailer on new commits; never rewrite history to add it. The orchestrator commits; this research does not.
- Several sessions share one checkout: commit via `.worktrees/`, mind the 25 GB zig cache.
- `BkEditor*` and `SMapObjectInfo` positions are map units (AI), the camera is world units; message parameters that carry pointers are pointer-wide (not touched here).

## Sources

### Primary (HIGH confidence, read this session)
- `Sources/src/MapFile/{MapOverlay,MapEquivalence,MapFile}.{h,cpp}`: overlay ops, comparator and its field guard, reader and writer.
- `Sources/src/EditorBridge/{bridge.h,bridge.cpp,session.h,session.cpp,catalogue.cpp}`: the C ABI, `Guarded`, tombstones, bridge build, `ObjectAt`, `AddObjectToSession`, `DeleteObjectFromSession`, sound precedent.
- `Sources/src/Formats/{fmtMap.h,fmtVSO.h,fmtAIGeneral.h,fmtTerrain.h}`, `RandomMapGen/{MapInfo_Types.h,MapInfo_Methods.cpp,MapInfo_StaticMethods.cpp,VSO_Types.h,VSO_StaticMethods.cpp,Polygons_Types.h}`: record layouts, conversions, `CVSOBuilder`, hit test, packing.
- `Sources/src/Scene/{Terrain.h,TerrainEditor.cpp,Scene.h,Camera.h}`, `AILogic/{AILogic.h,AIEditorInternal.cpp,AIStaticMap.cpp,AILogicInternal.cpp,SupremeBeing.cpp,GeneralInternal.cpp,GeneralIntendant.cpp,AIGeometry.cpp,Scripts/Scripts.cpp}`, `GameTT/iMissionInternal.cpp`, `Main/{GameDB.h,RPGStats.h,RPGStats.cpp}`, `Common/Actions.h`, `StreamIO/ConsoleBuffer.cpp`: engine behaviour and the game readers.
- MFC reference: `MapEditor/{VectorStripeObjectsState,RoadDrawState,StateAIGeneral,MapToolState,ObjectPlacerState,GroupManagerDialog,AIStartCommand,MapOptionsDialog,TabVOVSODialog,TemplateEditorFrame1}.cpp` at the line ranges cited.
- Editor Zig: `Sources/editor/core/{history,editor,bridge,tools,files,fake_bridge,root}.zig`, `Sources/editor/app/{view,view_math,auto,c_bridge,c_bridge_test,main,panels_logic}.zig`, `smoke.zig` (event synthesis).
- `build.zig` (lines 2767-2850, 3745-3830, 5690-5730, 5825-5860, 6030-6135), `.github/workflows/cross-platform.yml`, `tools/zig/{data_only_startup.cpp,editor_bridge_test.cpp,map_file_test.cpp}`.
- Data: `Data/Bridges/*/0N/1.xml`, `Data/objects.xml` (bridge names), `Data/Terrain/sets/*/Roads3D` and `Rivers`, `Data/Editor/actions.ini`, capture `zig-out/local-test/03-15-release-check.tga`.
- Docs: `04-CONTEXT.md`, `04-PARITY.md`, `03-RESEARCH.md`, `03-VALIDATION.md`, `STATE.md`, the portable map editor design spec.

### Secondary (MEDIUM confidence)
- `https://wiki.libsdl.org/SDL3/SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK`: Ctrl+click default. The fetch provider classifies as LOW by the seam (`classify-confidence --provider webfetch`), but it is the SDL project's own documentation.
- `gh run list --workflow cross-platform.yml`: recent run durations.

### Tertiary (LOW confidence)
- None used as a basis for a recommendation; unverified items are in the Assumptions Log.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, no new components; every reused piece was read.
- Architecture: HIGH for the chain and the precedents (sounds, tombstones, paints); MEDIUM for the compound-edit mechanism choice (two viable options, one recommended).
- Pitfalls: HIGH where quoted with lines; MEDIUM where marked `[ASSUMED]` (A3, A5, A6).
- Parity of the trench builder: MEDIUM: the MFC editor cannot be run here, so fidelity is by code reading and property tests.

**Research date:** 2026-09-30
**Valid until:** the end of phase 4 (repo-internal; re-check the cited line ranges if `session.cpp`, `bridge.h`, `view.zig` or `auto.zig` change before a plan starts).

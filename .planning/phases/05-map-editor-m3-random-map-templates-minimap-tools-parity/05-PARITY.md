# Phase 5: MFC Map Editor parity checklist

This is the checklist of record (05-CONTEXT D-36). It has one row per user-facing feature of `Sources/src/MapEditor`, built from:
- `editor.rc` (menus, dialogs, accelerators);
- `MainFrm.cpp` (the real toolbars);
- the workspace panes and tabs (`MapEditorBarWnd.cpp`);
- every `*State.cpp` / `State*.cpp`;
- every dialog class;
- the load and save pipelines.

Inventory date: 2026-09-30.

**Columns**
- **Owner**: `M1` is phase 3 (done). `M2` is phase 4. `M3` is this phase; its plan number follows D-39. `NF` means "not a feature": the MFC code is dead or unreachable, so the MFC editor cannot do it. The reason and its file:line are given.
- **Evidence**: filled in when the row closes, with a test name, an auto scenario step or a hand-try note. A row without evidence is open. The MFC editor is deleted (plan 05-11) only when every row is closed.

File references are relative to `Sources/src/MapEditor/` unless stated. `TEF` = `TemplateEditorFrame1.cpp`.

## 1. File menu and file handling

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| F1 | New map (Ctrl+N): size 1–32 patches, Square lock, season, name, MOD | TEF:2362, NewMapDialog.cpp | M3 05-01 | File → New (D-23) | |
| F2 | Open (Ctrl+O) with map list, Browse, MOD choice | TEF:1357, OpenMapDialog.cpp | M1 | File → Open; mod via File → Mod (spec D-26 revised) | 03-VERIFICATION |
| F3 | Open: try name, then `.xml`, then `.bzm` | TEF:1357 | M1 | MapFile reader picks the newer of the pair | test-map-files |
| F4 | Load: `RemoveNonExistingObjects` on MOD change + `loadmap_log.txt` | TEF:1357–2113 | M1 / M3 05-05 | Replaced by design: unknown objects are kept and warned about (spec). Explicit removal is offered by Check Map (D-33) | |
| F5 | Load: create zero altitudes if missing | TEF:1658 | M3 05-01 | Bridge open path; test with a map lacking altitudes | |
| F6 | Save (Ctrl+S) | TEF:4378 | M1 | | 03-VERIFICATION |
| F7 | Save As | TEF:4452 | M1 | | 03-VERIFICATION |
| F8 | Save in XML (Ctrl+X) / Save in BZM (Ctrl+B) | TEF:4406, 4413 | M3 05-01 | D-24 (Ctrl+Shift+X/B) | |
| F9 | Recent maps (10) | MainFrm.cpp:889–937 | M1 | Open Recent | 03-VERIFICATION |
| F10 | Create Random Map dialog: template, context, graph index, setting, direction, level, BZM, DDS, name | TEF:207–298, CreateRandomMapDialog.cpp | M3 05-08 | D-01..D-05 | |
| F11 | Exit (grayed item) / close with save prompt | editor.rc:2061, MainFrm.cpp:693 | M1 | Quit and close prompts | 03-VERIFICATION |
| F12 | Drag-and-drop a map onto the window | MainFrm.cpp:1120 | M3 05-11 | D-34 | |
| F13 | Single instance: a second launch passes its file (WM_COPYDATA) | MainFrm.cpp:1200, editor.cpp:125 | M3 05-11 | D-34 local IPC | |
| F14 | Map path on the command line | editor.cpp:188 | M3 05-11 | D-34 | |
| F15 | Window title: name, ext, `*`, WxH patches, MOD | TEF:153–205 | M3 05-01 | D-34 | |

## 2. Map menu / Map toolbar

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| M1 | Fill Entire Map | TEF:4921 | M3 05-02 | D-22 (the update-rect typo is not copied) | |
| M2 | Player Camera: set the camera anchor per player / neutral | TEF:5035 | M2 | camera anchors | |
| M3 | Diplomacy dialog: sides, game type, attacking side | TEF:5878, TabSimpleObjectsDiplomacyDialog.cpp | M1 | Players panel | 03-VERIFICATION |
| M4 | Diplomacy: add/delete player (≤16 + neutral), Insert/Delete/0/1 keys, popup | TabSimpleObjectsDiplomacyDialog.cpp:263, 434 | M3 05-05 | D-30 | |
| M5 | Units Creation Info: party, aviation (5 slots × name/formation/count), paratroopers, relax time, appear points | TEF:5070, UnitCreation.cpp, PEPointsListDialog.cpp | M3 05-05 | D-30 | |
| M6 | Script (map script file `szScriptFile`) | TEF:5102, MapOptionsDialog.cpp | M2 | scripts | |
| M7 | Check Map: duplicates, links, player index, parties; report + `checkmap_log.txt` | TEF:5938, 5952–6390 | M3 05-05 | D-33 (never a silent fix on save) | |
| M8 | Update Map (Ctrl+U): heights, terrain, shades, object/road/river Z, grid snap | TEF:5138 | M3 05-02 | D-20 | |
| M9 | Instant Update Map Mode | TEF:5249, DrawShadeState.cpp:268 | M3 05-02 | D-20 | |
| M10 | Fit Objects To Grid (default on) | TEF:5287, ObjectPlacerState.cpp:97,167,359 | M3 05-02 | D-20 | |
| M11 | Create minimap images (the minimap bar's Create button) | TEF:300, MiniMapDialog.cpp:464 | M3 05-07 | D-17 | |

## 3. Unit menu

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| U1 | Add Start Command (property dialog, map click sets position, red lines) | TEF:3870, ObjectPlacerState.cpp:754 | M2 | | |
| U2 | Start Commands List (Delete/Space keys) | TEF:4051, AIStartCommandsDialog.cpp | M2 | | |
| U3 | Artillery (reserve) positions mode | TEF:4122, ObjectPlacerState.cpp:525–622, 968–1005 | M2 | | |

## 4. Layers menu / Layers toolbar

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| L1 | Terrain (`SCENE_SHOW_TERRAIN`) | TEF:5318 | M3 05-06 | D-32 | |
| L2 | Grid | TEF:5345 | M3 05-06 | | |
| L3 | Wire Frame (`IGFX::SetWireframe`) | TEF:5372 | M3 05-06 | Needs GFXGPU wireframe support; measure in the plan | |
| L4 | Depth Complexity | TEF:5618 | M3 05-06 | Measure on GFXGPU | |
| L5 | Terrain Noise | TEF:5402 | M3 05-06 | | |
| L6 | Black Stripes (border) | TEF:5412 | M3 05-06 | | |
| L7 | Units | TEF:5456 | M3 05-06 | | |
| L8 | Objects | TEF:5483 | M3 05-06 | | |
| L9 | Bounding Boxes | TEF:5510 | M3 05-06 | | |
| L10 | Shadows | TEF:5537 | M3 05-06 | | |
| L11 | Haze ("Hase") | TEF:5591 | M3 05-06 | | |
| L12 | War fog (`ID_SHOW_SCENE_8`, handler works, no menu) | TEF:5564 | M3 05-06 | Offered because it works in code | |
| L13 | Selector (double circles around the selection) | TEF:5777, 4648 | M3 05-04 | D-25 | |
| L14 | Units Passability (`ToggleAIInfo`) | TEF:5645 | M3 05-06 | | |
| L15 | Unit Fire Ranges + filter combo / selected units | TEF:4991, 2115, MainFrm.cpp:622 | M3 05-06 | | |
| L16 | Storage coverage | TEF:5682–5746 | NF | The computation is commented out (TEF:5713–5746) and the button is in no toolbar or menu | |

## 5. View menu, toolbars, help, options, tools

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| V1 | Toolbars show/hide (File, Settings, Map, Layers, Unit, View) | MainFrm.cpp:743–826 | M3 05-11 | View menu panel toggles (D-34) | |
| V2 | Workspace bar, Minimap bar, Status bar toggles | MainFrm.cpp:732, 834 | M3 05-11 / 05-07 | D-34, D-14 | |
| V3 | Customize toolbars | MainFrm.cpp:384 | M3 05-11 | ImGui docking + Reset layout (D-34) | |
| V4 | Brush size combo 1×1..16×16 (default 2×2) | MainFrm.cpp:487–503, 658 | M3 05-01 | D-22; M1 slider reaches only 9×9 | |
| V5 | Player combo (placement owner, synced) | MainFrm.cpp:675 | M1 | Place tool player | 03-VERIFICATION |
| V6 | Status bar: tile position; VIS/SCRIPT coords; object name/ScriptID/pos/box or "N selected" | MainFrm.cpp:167–279, TEF:1185–1284, InputState.cpp:98 | M3 05-01 | D-34 | |
| V7 | Arrow keys pan the camera | TEF:793 | M1 | view.zig | view tests |
| H1 | Help Contents (`mapEditor.chm`) | MainFrm.cpp:708 | M3 05-11 | Help → Keys and tools window. The .chm is not shipped and is Windows-only | |
| H2 | About | editor.cpp:260 | M3 05-11 | | |
| T1 | Run Blitzkrieg | TEF:6649 | M1 | Test in game | map-editor-auto |
| T2 | Options: game command-line parameters, default save format | MapEditorOptions.cpp, TEF:6464 | M3 05-11 / 05-01 | D-34, D-24 | |
| T3 | Tool 0: RMG graphs list | MainFrm.cpp:939 | M3 05-08 | D-13 | |
| T4 | Tool 1: contexts list | MainFrm.cpp:990 | M3 05-08 | D-13 | |
| T5 | Tool 2: patches list | MainFrm.cpp:1031 | M3 05-08 | D-13 | |
| T6 | Tool 3: game maps list | MainFrm.cpp:1072 | M3 05-08 | D-13 | |
| T7 | Tool 4: re-update and resave every patch | MainFrm.cpp:1138 | NF | No menu item; it would rewrite shipped `Data` | |
| T8 | Mod switch (clears managers, swaps storage, reloads DB) | MODCollector.cpp, TEF:6392 | M1 | File → Mod | 03-VERIFICATION |

## 6. Edit toolbar

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| E1 | Undo | TEF:2659, IUndoRedoCmd.cpp | NF (exceeded) | Commented out; no command is ever pushed. The portable editor has real undo/redo (M1) | |
| E2 | Cut | none | NF | No handler | |
| E3 | Copy | TEF:2237 | NF | Body commented out | |
| E4 | Paste | TEF:2230, ObjectPlacerState.cpp:1092 | NF | Body commented out; `PlacePasteGroup` never called | |

## 7. Terrain pane

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| TR1 | Tile palette with masked thumbnails | TabTileEditDialog.cpp:82, 244 | M1 | Brush tile picker | 03-15 |
| TR2 | Tile properties (name, variants) | TabTileEditDialog.cpp:284–316 | M3 05-02 | D-35 (tile 0 included) | |
| TR3 | Paint tiles, click and drag, N×N brush, brush preview | TileDrawState.cpp | M1 (+V4) | | 03-VERIFICATION |
| TR4 | Heights: brush size 2–16 with the profile.tga pattern | TabTerrainAltitudesDialog.cpp:156, 181 | M3 05-02 | D-18 | |
| TR5 | Heights: raise (left), lower (right), level (middle / left+right) | DrawShadeState.cpp:186–336 | M3 05-02 | D-18; Alt+drag = middle on a trackpad | |
| TR6 | Heights: level modes Zero / Click Tile / Instant Average / Click Average | TabTerrainAltitudesDialog.cpp:472–494 | M3 05-02 | | |
| TR7 | Heights: Height Speed, Level ratio % | :210, :230 | M3 05-02 | | |
| TR8 | Heights: invalid-height rollback, Ctrl override | DrawShadeState.cpp:261,285,308 | M3 05-02 | | |
| TR9 | Heights: brush footprint drawing | DrawShadeState.cpp:91 | M3 05-02 | | |
| TR10 | Generate heights (Hills FBM / Rocks HYBRID / Dunes RIDGED, granularity, min/max Z) | TabTerrainAltitudesDialog.cpp:312 | M3 05-02 | | |
| TR11 | Heights: Update button | :496 | M3 05-02 | = Update Map | |
| TR12 | Heights: Set Zero | :359 | M3 05-02 | | |
| TR13 | Heights: Multi / Heterogenous radios | editor.rc:503–507 | NF | Hidden and disabled | |
| TR14 | Fields: field-set combo + Browse | TabTerrainFieldsDialog.cpp:125–212 | M3 05-03 | D-21 | |
| TR15 | Fields: Randomize Polygon (min length, width, disturbance) | :229–277, StateTerrainFields.cpp:350 | M3 05-03 | | |
| TR16 | Fields: Fill Terrain / Place Objects / Modify Heights / Check Passability / Update Map | :413–493 | M3 05-03 | | |
| TR17 | Fields: season mismatch prompt | :391 | M3 05-03 | | |
| TR18 | Fields: polygon Add / Select / Edit states and keys | StateTerrainFields.cpp:25–219 | M3 05-03 | | |
| TR19 | Fields: Remove Objects | editor.rc:1333, StateTerrainFields.cpp:381 | NF | Hidden and disabled; empty code | |

## 8. Objects pane: Objects tab

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| O1 | Object list, list/thumbnail view, read-only properties | TabSimpleObjectsDialog.cpp:776–886 | M1 (+M3 05-04 for the properties popup) | Palette with pictures | |
| O2 | Nine quick filter toggles, Ctrl+click assigns | :424–516 | M3 05-03 | D-31 | |
| O3 | Filter combo + toggle; New/Delete Filter; global exclusions | :158, 175, 399, 523 | M3 05-03 | D-31 | |
| O4 | Filters Composer (conditions of folder words, add/delete/rename) | CreateFilterDialog.cpp, CreateFilterNameDialog.cpp, SetupFilterDialog.cpp | M3 05-03 | D-31 | |
| O5 | Placement player | :557–670 | M1 | | |
| O6 | Direction wheel (placement angle; turns ghost and selection) | DirectionButton.cpp, TEF:1053 | M3 05-04 | D-28 | |
| O7 | Placement ghost | ObjectPlacerState.cpp:251–297 | M1 | | |
| O8 | Place; flags auto player; edge refusal | :333–392 | M1 | | |
| O9 | Select one object / whole squad | :393–523 | M1 (single) / M3 05-04 (squad) | D-25 | |
| O10 | Ctrl+click add to selection | :446 | M3 05-04 | D-25 | |
| O11 | Rubber band (screen rect); Ctrl = tile rect | :209–243, 886–962 | M3 05-04 | D-25 | |
| O12 | Drag-move selection with grid snap | :92–207 | M1 (single) / M3 05-04 (group) | | |
| O13 | Garrison / tow / couple by dropping, with cursor feedback | :796–877, 1325 | M3 05-04 | D-27 | |
| O14 | Right-click: deselect / cycle overlapping | :968–1073 | M3 05-04 | D-25 | |
| O15 | Double-click / Enter / Space opens properties | :627, 1183 | M3 05-04 | D-26 | |
| O16 | Delete key removes the selection (also from start commands/reserve positions) | :627–743 | M1 (single) / M3 05-04 (multi) / M2 (references) | | |
| O17 | Properties: building (units, Script ID, player, health) | SEditorMApObject.cpp:47–65 | M3 05-04 | D-26 | |
| O18 | Properties: trench (units, Script ID) | :220–227 | M3 05-04 (fields) / M2 (trench drawing) | | |
| O19 | Properties: unit (units, angle, Script ID, scenario unit, player incl. flag swap, health, formation) | :295–321 | M3 05-04 | D-26 | |
| O20 | Properties: multi-selection (angle, Script ID, player, behaviour) | :658–681 | M3 05-04 | D-26 | |
| O21 | Unlink a garrisoned unit (double-click in units) | TEF:3678 | M3 05-04 | D-27 | |
| O22 | In-tab Diplomacy button | editor.rc:371 | NF | Hidden and disabled; the menu path is M3 row | |

## 9. Objects pane: Fences, Bridges; Vector pane: Entrenchments, Roads, Rivers

| # | MFC feature | MFC source | Owner | Evidence |
|---|---|---|---|---|
| VO1 | Fences: list, ghost (Ctrl flip), axis-locked drag | FenceSetupWindow.cpp, RoadDrawState.cpp:519–622, 841–898, 1034 | M2 | |
| VO2 | Bridges: list, ghost, drag begin/middle/end spans, all-or-nothing commit | BridgeSetupDialog.cpp, RoadDrawState.cpp:626–667, 969 | M2 | |
| VO3 | Bridges: Enter toggles destroyed/intact; Delete removes the span group | RoadDrawState.cpp:1241–1384 | M2 | |
| VO4 | Entrenchments: path draw, segment/turn/terminator build, hover highlight, Enter props, Delete | RoadDrawState.cpp:673–1585 | M2 | |
| VO5 | Roads/Rivers: width modes single/multi/all, width, opacity, type list | TabVOVSODialog.cpp | M2 | |
| VO6 | Roads/Rivers: select/add/edit states, Insert/Delete points, width handles, opacity drag | VectorStripeObjectsState.cpp | M2 | |
| VO7 | Rivers update AI passability | VectorStripeObjectsState.cpp:1042–1094 | M2 | |

## 10. Map Tools, Groups, AI Settings panes

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| MT1 | Damage tool: % to add, left damage / right heal / middle repair | TabToolsDialog.cpp:95, MapToolState.cpp:29–186 | M3 05-04 | D-29 | |
| MT2 | Script areas: rectangle/circle draw, name dialog, list centres camera, delete | MapToolState.cpp:188–274, TabToolsDialog.cpp:109–149, AreaNameDialog.cpp | M2 | | |
| G1 | Reinforcement groups: list, new (auto-ID), delete, hide checked, script IDs per group | GroupManagerDialog.cpp, GetGroupID.cpp, EnterScriptIDDialog.cpp | M2 | | |
| AI1 | AI general: side radios, mobile reinforcements, positions, parcels on the map, placeholders, type dialog | TabAIGeneralDialog.cpp, StateAIGeneral.cpp, TabAIGeneral*Dialog.cpp | M2 | | |

## 11. Minimap bar

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| MM1 | Editor mode: terrain colours, heights gradient in Heights, units in 17 colours, fire ranges, camera frame, patch grid | MiniMapDialog.cpp:395, MiniMapTypes.cpp | M3 05-07 | D-14 | |
| MM2 | Click/drag moves the camera | MiniMapDialog.cpp:308–346 | M3 05-07 | D-15 | |
| MM3 | Game mode (the map's own tga/dds) | MiniMapDialog.cpp:458, 469–515 | M3 05-07 | D-16 | |
| MM4 | Create (DDS+TGA, 256 + 512 `_large`) | TEF:300 | M3 05-07 | D-17 | |
| MM5 | Hidden Close button; viewport rubber band | editor.rc, MiniMapDialog.cpp | NF | Hidden / commented out | |

## 12. RMG composers

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| R1 | Containers Composer: list (all columns), Add/Delete/Check!, File New/Open/Save/Save As/Exit, popups | RMG_CreateContainerDialog.cpp | M3 05-09 | D-06..D-10, D-12 | |
| R2 | Container patches: add maps from storage, season/folder/script consistency, size, N/E/S/W indices | :207–318, 930–975 | M3 05-09 | D-10 copies outside maps in | |
| R3 | Patch properties: setting, tri-state N/E/S/W, multi-edit | RMG_PatchPropertiesDialog.cpp | M3 05-09 | | |
| R4 | Graphs Composer: list, Check!, zoom slider, node canvas (add/move/resize/link/delete) | RMG_CreateGraphDialog.cpp:645, 1061–1440 | M3 05-09 | D-11 | |
| R5 | Graph node properties: container path + Browse; size/season checks, merge script IDs | RMG_GraphNodePropertiesDialog.cpp, RMG_CreateGraphDialog.cpp:1611–1690 | M3 05-09 | | |
| R6 | Graph link properties: road/river, VSO desc, min length, radius, width, disturbance, parts, multi | RMG_GraphLinkPropertiesDialog.cpp | M3 05-09 | | |
| R7 | Fields Composer: list, Check!, Save All, File menu | RMG_CreateFieldDialog.cpp | M3 05-10 | | |
| R8 | Fields: terrain tab (season, available tiles, shells, shell tiles, tile weight, shell width) | RMG_FieldTerrainDialog.cpp, RMG_FieldTilePropertiesDialog.cpp, RMG_FieldTerrainShellPropertiesDialog.cpp | M3 05-10 | | |
| R9 | Fields: objects tab (filter, available objects, shells: width/step/probability, weight) | RMG_FieldObjectsDialog.cpp, RMG_FieldObjectPropertiesDialog.cpp, RMG_FieldObjectsShellPropertiesDialog.cpp | M3 05-10 | Needs O4 filters | |
| R10 | Fields: heights tab (profile tga, height, pattern size min/max, positive %) | RMG_FieldHeightsDialog.cpp | M3 05-10 | | |
| R11 | Templates Composer: list (all columns), graphs/fields/VSO lists with weights, default field, VSO width/opacity | RMG_CreateTemplateDialog.cpp, RMG_Template*PropertiesDialog.cpp | M3 05-10 | | |
| R12 | Templates: Diplomacy…, Units… (unit creation grid), script file, MOD combo; writes Template + QuickLoadMapInfo | RMG_CreateTemplateDialog.cpp:328–355, 1117, 1344, 1443 | M3 05-10 | | |
| R13 | Templates: Check! | RMG_CreateTemplateDialog.cpp:1269 | NF (added anyway) | Does nothing in the MFC editor; M3 implements it (D-12) | |
| R14 | Composer list files `Editor\Default*.xml` | composers :18–19 | M3 (replaced) | Folder scan (D-08); the files are not shipped | |
| R15 | Composer layout persistence (`ResizeDialogStyles`) | ResizeDialog.cpp:339–382 | M3 05-11 | ImGui ini layout | |
| R16 | Random map generator tab `IDD_TAB_RANDOM_MAP_GENERATOR` | editor.rc:402 | NF | No class uses it | |
| R17 | `CPESelectStringsDialog` | PESelectStringsDialog.cpp | NF | Included only, never opened | |

## 13. Dead tabs and handlers (MFC cannot do these)

| # | Item | Evidence | Owner |
|---|---|---|---|
| D1 | Sounds tab (`IDD_TAB_SOUNDS`: show river/building/forest sounds, ambient/circle combos) | No class; `STATE_SOUNDS` commented out (TEF:1115, 1141). The map sound list itself is M1 | NF |
| D2 | Forests tab (`IDD_TAB_VO_FORESTS`) | No class or state | NF |
| D3 | `AddSounds` on save | Commented out (TEF:~3212–3236); runs on load only for a display that is commented out (4755–4773). The game runs it itself (`GameTT/iMissionInternal.cpp:1465`) | NF |
| D4 | `ID_BUTTONFOG`, `ID_BUTTOSHOWINFO`, `ID_BUTTONSHOWBOXES`, `ID_TOOLS_CLEARCASH`, `OnButtonDeleteArea`, `ID_FILE_SAVE_BINARY` | No UI (TEF:2356, 3300, 3374, 2733, 3510) | NF |
| D5 | Groups tab timer; AI `CAIGAddState`; AI message label | Unmapped or empty | NF |

## 14. Save and load behaviour

| # | MFC behaviour | MFC source | Owner | Portable decision | Evidence |
|---|---|---|---|---|---|
| S1 | Save runs `CheckMap(false)`, fixing silently | TEF:2934 | M3 05-05 | Replaced: the check only warns (D-33). Preservation invariant | |
| S2 | Save recomputes full-map shades | TEF:2934–3291 | M3 05-01 | Replaced: region shades at edit time and explicit Update Map (D-19, D-20) | |
| S3 | Save writes the QuickLoadMapInfo chunk | TEF:3238–3258 | M1 | MapFile | test-map-files |
| S4 | Save packs frame indices, writes squads once, nudges bridge spans | TEF:2934–3291 | M1 / M2 | Overlay packs only edited objects (spec); bridge spans M2 | |
| S5 | Load recomputes full shades with the season sun | TEF:1657 | M1 (not copied) | Engine built from the snapshot; shades unchanged | test-map-files-all |
| S6 | Load: scenario units drawn blue; future-build bridges blue | TEF:1357–2113 | M3 05-04 / M1 | Scenario-unit tint in D-26; future-build list M1 | |
| S7 | Load: garrisons placed beside their host | TEF:1357–2113 | M3 05-04 | Links shown and editable (D-27) | |

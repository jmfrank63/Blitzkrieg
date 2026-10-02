# Phase 5: MFC Map Editor parity checklist

This is the checklist of record (05-CONTEXT D-36). It has one row per user-facing feature of `Sources/src/MapEditor`, built from:
- `editor.rc` (menus, dialogs, accelerators);
- `MainFrm.cpp` (the real toolbars);
- the workspace panes and tabs (`MapEditorBarWnd.cpp`);
- every `*State.cpp` / `State*.cpp`;
- every dialog class;
- the load and save pipelines.

Inventory date: 2026-09-30.

**Anchor re-verification (2026-10-01, plan 05-01):** every anchor cited by plan 05-01's rows was grepped against the MFC source and holds (TEF:2362 OnFileNewMap, TEF:1658 zero-altitudes load, TEF:4406/4413 save-XML/BZM handlers, TEF:153 SetMapModified/title block, TEF:1185 OnUpdateTileCoord, TEF:2934 SaveMap, MainFrm.cpp:487 FillBrushSize, MainFrm.cpp:167 indicators, InputState.cpp:98 UpdateSatusBar, DrawShadeState.cpp:210 the ±1-vertex update rect). The MFC tree is untouched by this phase, so the remaining anchors stand as inventoried; each later plan re-verifies the anchors it closes.

**Columns**
- **Owner**: `M1` is phase 3 (done). `M2` is phase 4. `M3` is this phase; its plan number follows D-39. `NF` means "not a feature": the MFC code is dead or unreachable, so the MFC editor cannot do it. The reason and its file:line are given.
- **Evidence**: filled in when the row closes, with a test name, an auto scenario step or a hand-try note. A row without evidence is open. The MFC editor is deleted (plan 05-11) only when every row is closed.

File references are relative to `Sources/src/MapEditor/` unless stated. `TEF` = `TemplateEditorFrame1.cpp`.

## 1. File menu and file handling

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| F1 | New map (Ctrl+N): size 1–32 patches, Square lock, season, name, MOD | TEF:2362, NewMapDialog.cpp | M3 05-01 | File → New (D-23) | `editor-bridge: M3 new map ok` (TestM3NewMap); `map_new:8x8:summer:M3Auto` in map-editor-m3-auto; panels_logic "new map fields" tests | |
| F2 | Open (Ctrl+O) with map list, Browse, MOD choice | TEF:1357, OpenMapDialog.cpp | M1 | File → Open; mod via File → Mod (spec D-26 revised) | 03-VERIFICATION |
| F3 | Open: try name, then `.xml`, then `.bzm` | TEF:1357 | M1 | MapFile reader picks the newer of the pair | test-map-files |
| F4 | Load: `RemoveNonExistingObjects` on MOD change + `loadmap_log.txt` | TEF:1357–2113 | M1 / M3 05-05 | Replaced by design: unknown objects are kept and warned about (spec). Explicit removal is offered by Check Map (D-33) | |
| F5 | Load: create zero altitudes if missing | TEF:1658 | M3 05-01 | Bridge open path; test with a map lacking altitudes | TestM3NewMap's altitude-less crafted-map case (`editor-bridge: M3 new map ok`) | |
| F6 | Save (Ctrl+S) | TEF:4378 | M1 | | 03-VERIFICATION |
| F7 | Save As | TEF:4452 | M1 | | 03-VERIFICATION |
| F8 | Save in XML (Ctrl+X) / Save in BZM (Ctrl+B) | TEF:4406, 4413 | M3 05-01 | D-24 (Ctrl+Shift+X/B) | TestM3NewMap saves both formats and reads them back equal; `file_save_xml`/`file_save_bzm` commands; `do=file_save_bzm` step in map-editor-m3-auto; settings `default_format` (enginePath test) | |
| F9 | Recent maps (10) | MainFrm.cpp:889–937 | M1 | Open Recent | 03-VERIFICATION |
| F10 | Create Random Map dialog: template, context, graph index, setting, direction, level, BZM, DDS, name | TEF:207–298, CreateRandomMapDialog.cpp | M3 05-08 | D-01..D-05 | |
| F11 | Exit (grayed item) / close with save prompt | editor.rc:2061, MainFrm.cpp:693 | M1 | Quit and close prompts | 03-VERIFICATION |
| F12 | Drag-and-drop a map onto the window | MainFrm.cpp:1120 | M3 05-11 | D-34 | |
| F13 | Single instance: a second launch passes its file (WM_COPYDATA) | MainFrm.cpp:1200, editor.cpp:125 | M3 05-11 | D-34 local IPC | |
| F14 | Map path on the command line | editor.cpp:188 | M3 05-11 | D-34 | |
| F15 | Window title: name, ext, `*`, WxH patches, MOD | TEF:153–205 | M3 05-01 | D-34 | panels_logic formatTitleM3 tests; `expect=title:` steps in map-editor-m3-auto (coldwinter, M3Auto, m3.bzm, 8x8) | |

## 2. Map menu / Map toolbar

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| M1 | Fill Entire Map | TEF:4921 | M3 05-02 | D-22 (the update-rect typo is not copied) | | `map-file: M3 fill ok` (TestM3FillRegion: fill undone byte-identical, crosses by content); `editor-bridge: M3 update and fill ok` (TestM3UpdateMapAndFill: every tile the type's own, undone byte-exact); `do=map_fill:0` + `expect=undo_depth:5` in map-editor-m3-auto |
| M2 | Player Camera: set the camera anchor per player / neutral | TEF:5035 | M2 | camera anchors | **Closed (M2, 04-01/04-03/04-04):** `map-file: M2 camera anchor records ok`, `editor-bridge: M2 camera anchors ok`, `map-editor-engine: M2 camera anchors round trip ok`, `map-editor-auto-m2` frames 3–19; the game starts its camera at player 0's anchor (`map-editor-game-reads-it-m2`: `camera at player 0's anchor 2172,2172, source=player`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| M3 | Diplomacy dialog: sides, game type, attacking side | TEF:5878, TabSimpleObjectsDiplomacyDialog.cpp | M1 | Players panel | 03-VERIFICATION |
| M4 | Diplomacy: add/delete player (≤16 + neutral), Insert/Delete/0/1 keys, popup | TabSimpleObjectsDiplomacyDialog.cpp:263, 434 | M3 05-05 | D-30 | |
| M5 | Units Creation Info: party, aviation (5 slots × name/formation/count), paratroopers, relax time, appear points | TEF:5070, UnitCreation.cpp, PEPointsListDialog.cpp | M3 05-05 | D-30 | |
| M6 | Script (map script file `szScriptFile`) | TEF:5102, MapOptionsDialog.cpp | M2 | scripts | **Closed (M2, 04-10, 04-13):** `editor-bridge: M2 script file ok`; `map-editor-auto-m2` names the script, chooses it beside a user map (`script_choose`), brings it along a Save As (`script_copy_along_yes`) and Test in game runs it (`expect=test_game_script:m2_script`); `map-editor-game-reads-it-m2`: `script m2_script ran (loaded=1 init=1)`; CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| M7 | Check Map: duplicates, links, player index, parties; report + `checkmap_log.txt` | TEF:5938, 5952–6390 | M3 05-05 | D-33 (never a silent fix on save) | |
| M8 | Update Map (Ctrl+U): heights, terrain, shades, object/road/river Z, grid snap | TEF:5138 | M3 05-02 | D-20 | | `editor-bridge: M3 update and fill ok` (TestM3UpdateMapAndFill: the composite's progress heard 7+snapped, the snap exactly FitVisOrigin2AIGrid's own, undo byte-exact); `do=map_update` + `expect=undo_depth:4` in map-editor-m3-auto; Ctrl+U in the Map menu |
| M9 | Instant Update Map Mode | TEF:5249, DrawShadeState.cpp:268 | M3 05-02 | D-20 | | `do=instant_update` + `expect=undo_depth:5` in map-editor-m3-auto (a setting, never an undo step); the session's flag drives the per-stroke objects-Z pass (session_terrain.cpp); BkEditorSetTerrainModes with the MFC's own defaults |
| M10 | Fit Objects To Grid (default on) | TEF:5287, ObjectPlacerState.cpp:97,167,359 | M3 05-02 | D-20 | | `do=fit_grid` + `expect=undo_depth:5` in map-editor-m3-auto; `editor-bridge: M3 update and fill ok` (the update pass snaps every sprite with passability, exactly FitVisOrigin2AIGrid); the placer/drag fit through BkEditorSnapToGrid in the same test |
| M11 | Create minimap images (the minimap bar's Create button) | TEF:300, MiniMapDialog.cpp:464 | M3 05-07 | D-17 | |

## 3. Unit menu

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| U1 | Add Start Command (property dialog, map click sets position, red lines) | TEF:3870, ObjectPlacerState.cpp:754 | M2 | Unit > Add start command, Start Commands window, Start Target tool | **Closed (M2, 04-11):** `editor-bridge: M2 start commands ok`, `map-editor-engine: M2 start command round trip ok`, `map-editor-auto-m2` frames 234–271 (red line in `m2_startcmds`); the game launches it (`startcmd launched 0 -> 1`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| U2 | Start Commands List (Delete/Space keys) | TEF:4051, AIStartCommandsDialog.cpp | M2 | Start Commands window | **Closed (M2, 04-11):** `map-editor-auto-m2` `startcmds_window:1` and shot `m2_startcmds_panel`; commands `startcmd_select`, `startcmd_delete`; core test 'Remove of the last unit deletes the command in one step'; (details: phase 4 `04-PARITY.md`) |
| U3 | Artillery (reserve) positions mode | TEF:4122, ObjectPlacerState.cpp:525–622, 968–1005 | M2 | Reserve Positions tool, Unit > Artillery positions mode | **Closed (M2, 04-11):** `editor-bridge: M2 reserve positions ok`, `map-editor-auto-m2` frames 273–303; the game applies it (`reserve applied 0 -> 1`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |

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
| L13 | Selector (double circles around the selection) | TEF:5777, 4648 | M3 05-04 | D-25 | core `selection circles: the inner ring rides the footprint, the outer follows, a small object never vanishes` (panels_logic selectionCircles, drawn per selected object by markers.zig drawSelectionCircles, the dashed rubber band beside them); seen around both banded squads in a capture of map-editor-m3-auto after frame 196 (05-04 debugging; the frames themselves assert the selection, not the pixels) |
| L14 | Units Passability (`ToggleAIInfo`) | TEF:5645 | M3 05-06 | | |
| L15 | Unit Fire Ranges + filter combo / selected units | TEF:4991, 2115, MainFrm.cpp:622 | M3 05-06 | | |
| L16 | Storage coverage | TEF:5682–5746 | NF | The computation is commented out (TEF:5713–5746) and the button is in no toolbar or menu | |

## 5. View menu, toolbars, help, options, tools

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| V1 | Toolbars show/hide (File, Settings, Map, Layers, Unit, View) | MainFrm.cpp:743–826 | M3 05-11 | View menu panel toggles (D-34) | |
| V2 | Workspace bar, Minimap bar, Status bar toggles | MainFrm.cpp:732, 834 | M3 05-11 / 05-07 | D-34, D-14 | |
| V3 | Customize toolbars | MainFrm.cpp:384 | M3 05-11 | ImGui docking + Reset layout (D-34) | |
| V4 | Brush size combo 1×1..16×16 (default 2×2) | MainFrm.cpp:487–503, 658 | M3 05-01 | D-22; M1 slider reaches only 9×9 | core test "the brush takes sizes 1..16, even sizes hanging right and below"; `brush_size` command; palette combo 1..16 default 2×2 | |
| V5 | Player combo (placement owner, synced) | MainFrm.cpp:675 | M1 | Place tool player | 03-VERIFICATION |
| V6 | Status bar: tile position; VIS/SCRIPT coords; object name/ScriptID/pos/box or "N selected" | MainFrm.cpp:167–279, TEF:1185–1284, InputState.cpp:98 | M3 05-01 | D-34 | panels_logic visScriptLine/objectLine tests; `expect=status:VIS:` / `expect=status:SCRIPT` steps in map-editor-m3-auto; the box is left out exactly like the MFC's own else-branch until a record carries one; the N-selected wording activates with 05-04's multi-selection | |
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
| TR2 | Tile properties (name, variants) | TabTileEditDialog.cpp:284–316 | M3 05-02 | D-35 (tile 0 included) | | `editor-bridge: M3 tile info ok` (TestM3TileInfo: tile 0 described - name, variants, index; every offered tile answers); `do=tile_info:0` + `expect=status:tile 0:` in map-editor-m3-auto; the picker's context menu (tile 0 included, the MFC's `> 0` guard not copied) |
| TR3 | Paint tiles, click and drag, N×N brush, brush preview | TileDrawState.cpp | M1 (+V4) | | 03-VERIFICATION |
| TR4 | Heights: brush size 2–16 with the profile.tga pattern | TabTerrainAltitudesDialog.cpp:156, 181 | M3 05-02 | D-18 | | `editor-bridge: M3 heights ok` (TestM3Heights: the editor\\profile.tga pattern, brush 2..16, BAD_ARGUMENT outside); the Heights panel's brush slider (panels_m3); core tools tests |
| TR5 | Heights: raise (left), lower (right), level (middle / left+right) | DrawShadeState.cpp:186–336 | M3 05-02 | D-18; Alt+drag = middle on a trackpad | | `editor-bridge: M3 heights ok` (raise, lower, level strokes, each vs the same-function expected map and undone byte-exact); `tool=heights` raise drag and level drag frames in map-editor-m3-auto; Alt+drag = middle and left+right = level in the view |
| TR6 | Heights: level modes Zero / Click Tile / Instant Average / Click Average | TabTerrainAltitudesDialog.cpp:472–494 | M3 05-02 | | | `editor-bridge: M3 heights ok` (all four level modes - zero, click tile, instant average, click average - each against its own expected map); the Heights panel's mode combo |
| TR7 | Heights: Height Speed, Level ratio % | :210, :230 | M3 05-02 | | | `editor-bridge: M3 heights ok` (speed and ratio ride the pattern math the expected maps build); the Heights panel's Height Speed and Level ratio % fields with commit-on-deactivate |
| TR8 | Heights: invalid-height rollback, Ctrl override | DrawShadeState.cpp:261,285,308 | M3 05-02 | | | `editor-bridge: M3 heights ok` (the cliff-making stroke REFUSED with the MFC's own words, nothing changed; Ctrl keeps it; the kept cliff undone byte-exact) |
| TR9 | Heights: brush footprint drawing | DrawShadeState.cpp:91 | M3 05-02 | | | the heights brush draws the M1 brush's own footprint outline at the cursor (view.zig, the heights tool's corner rectangle); on screen in every m3-auto heights frame |
| TR10 | Generate heights (Hills FBM / Rocks HYBRID / Dunes RIDGED, granularity, min/max Z) | TabTerrainAltitudesDialog.cpp:312 | M3 05-02 | | | `editor-bridge: M3 heights ok` (Hills/Rocks/Dunes generate with the MFC formula's own bounds and both range ends attained, each one undo step, undone byte-exact); `do=heights_generate:0` in map-editor-m3-auto; the panel's confirmation popup |
| TR11 | Heights: Update button | :496 | M3 05-02 | = Update Map | | the MFC's Update button is Update Map - one command, one undo step (`do=map_update` in map-editor-m3-auto, `editor-bridge: M3 update and fill ok`); Ctrl+U in the Map menu |
| TR12 | Heights: Set Zero | :359 | M3 05-02 | | | `editor-bridge: M3 heights ok` (set zero vs the same-shades expected map, undone byte-exact); the Heights panel's Set Zero with its confirmation popup |
| TR13 | Heights: Multi / Heterogenous radios | editor.rc:503–507 | NF | Hidden and disabled | |
| TR14 | Fields: field-set combo + Browse | TabTerrainFieldsDialog.cpp:125–212 | M3 05-03 | D-21 | BkEditorListRmg walks the mounted storages' Scenarios/FieldSets (storage-relative, lowercased, .xml stripped; a missing folder is an empty list, not an error - D-08); `editor-bridge: M3 fields ok` (the scan is sorted, a summer set answers its season, an unknown name REFUSES); `do=fields_set:summer\field00` in map-editor-m3-auto; the Fields panel's combo + Browse |
| TR15 | Fields: Randomize Polygon (min length, width, disturbance) | :229–277, StateTerrainFields.cpp:350 | M3 05-03 | | RandomizeEdges with the MFC's exact arguments (cut, 10, width, disturbance point, min*cell, 512*cell, true), the dialog's three numbers clamped before the call (T-05-03-04); `editor-bridge: M3 fields ok` (the randomized apply fills and undoes byte-exact); the panel's Randomize fields and `fields_randomize` command |
| TR16 | Fields: Fill Terrain / Place Objects / Modify Heights / Check Passability / Update Map | :413–493 | M3 05-03 | | ApplyFieldInSession runs the passes over one polygon (ValidateFieldSet + FillTileSet; FillObjectSet into a scratch summer map + each placed one through the palette-add rules; the profile-tga gradient + FillProfilePattern + full shades; the passability report that changes nothing; the nested Update Map) as ONE composite token; `editor-bridge: M3 fields ok` (terrain half, objects half and randomized apply each undone byte-exact, the report answers placed vs held); `map-file: M3 field region ok` (the tile region record byte-exact); the panel's five checkboxes |
| TR17 | Fields: season mismatch prompt | :391 | M3 05-03 | | the mismatch asks before applying, the MFC's dialog above PlaceField: `fields_apply` refuses naming both seasons and `:yes` answers (commands.zig, the heights confirmations' split); the bridge does not gate on season - TestM3Fields records the confirmation is app-side, as the MFC's was |
| TR18 | Fields: polygon Add / Select / Edit states and keys | StateTerrainFields.cpp:25–219 | M3 05-03 | | tools_fields.zig's three states with the MFC keys - click adds, right-click takes the rubber and the last, double-click/Enter closes by the UniquePolygon+area rule, Esc keeps the last point, Insert bisects the next edge, Delete removes the dragged vertex, the drag keeps z on the ground (POINT_RADIUS = cell/4); tools_fields.zig's test + view.zig dispatch; `tool=fields`, three presses and the closing dblclick in map-editor-m3-auto; needs_right_button/needs_double_click in the registry |
| TR19 | Fields: Remove Objects | editor.rc:1333, StateTerrainFields.cpp:381 | NF | Hidden and disabled; empty code | |

## 8. Objects pane: Objects tab

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| O1 | Object list, list/thumbnail view, read-only properties | TabSimpleObjectsDialog.cpp:776–886 | M1 (+M3 05-04 for the properties popup) | Palette with pictures | |
| O2 | Nine quick filter toggles, Ctrl+click assigns | :424–516 | M3 05-03 | D-31 | the nine quick toggles (`filter_toggle:<slot>`), Ctrl+click's assign (`filter_assign`, persisted as filter_setup in settings); panels_logic's collection tests; map-editor-m3-auto assigns slot 0 and gates the palette count both ways (`expect=palette_count:...`) |
| O3 | Filter combo + toggle; New/Delete Filter; global exclusions | :158, 175, 399, 523 | M3 05-03 | D-31 | the combo (`filter_select`, `none` clears) and New/Delete Filter (`filter_new`/`filter_delete`) over the merged list - shipped filter.xml/filterSetup.xml read through the engine's own reader, user filters from `<UserRoot>mapeditor/filter.xml` winning by key, reloaded on a generation bump; `editor-bridge: M3 filters ok` (shipped filters with their words, a user override plus an addition, catalogue entries filtering as the MFC matcher predicts); panels_logic paletteObjectVisible tests |
| O4 | Filters Composer (conditions of folder words, add/delete/rename) | CreateFilterDialog.cpp, CreateFilterNameDialog.cpp, SetupFilterDialog.cpp | M3 05-03 | D-31 | drawFiltersComposer (the Tools window): conditions of folder words with Add/Delete/Rename (`filter_new`/`filter_rename`/`filter_words`), Save writes the user-owned filters (`filters_save` through BkEditorSaveObjectFilters); `do=filters_composer` opens and closes it in map-editor-m3-auto; `editor-bridge: M3 filters ok` |
| O5 | Placement player | :557–670 | M1 | | |
| O6 | Direction wheel (placement angle; turns ghost and selection) | DirectionButton.cpp, TEF:1053 | M3 05-04 | D-28 | the palette's direction wheel (panels_m3 drawDirectionWheel; panels_logic `the direction wheel reads the drag angle the MFC's way: y up, whole degrees`) sets the placer's angle and turns every selected object to it as the MFC frame does (TEF:1053-1090), one drag = ONE undo step (`wheel_gesture`): core `the direction wheel turns a multi-selection to its angle as ONE undo step per drag`; map-editor-m3-auto 283-290 (`wheel_turn` with nothing selected: `placer_angle` only, no edit) and 300-315 (a T34 selected: `expect=angle:@0:90`, one step, undo back to 180, redo); Q/E unchanged (view `select a unit, Q turns it ...`) |
| O7 | Placement ghost | ObjectPlacerState.cpp:251–297 | M1 | | |
| O8 | Place; flags auto player; edge refusal | :333–392 | M1 | | |
| O9 | Select one object / whole squad | :393–523 | M1 (single) / M3 05-04 (squad) | D-25 | core `a click on a squad member selects the whole squad`; the picks answer a soldier's squad link (BkEditorObjectAt/BkEditorPickObjects); `editor-bridge: M3 multi-select ok` (TestM3MultiSelect moves a squad record whole); map-editor-m3-auto 184-189 (two squads placed, `expect=objects:2`) |
| O10 | Ctrl+click add to selection | :446 | M3 05-04 | D-25 | core `Ctrl+click toggles members, a plain click replaces, and a press on nothing clears the whole set` (the scripted pointer carries no Ctrl, so no auto frame) |
| O11 | Rubber band (screen rect); Ctrl = tile rect | :209–243, 886–962 | M3 05-04 | D-25 | core `the band on empty ground selects by screen rectangle`, `the Ctrl band selects by tile rectangle, passing the span over`; TestM3MultiSelect's BkEditorPickObjects block (two-pass, both members in the band; `editor-bridge: M3 multi-select ok`); the rectangle pick honours the gameplay scale like the point pick (Scene/SpriteVisObj.cpp, 05-04 fix); map-editor-m3-auto 190-196 (a screen band dragged from empty ground selects both: `expect=selection_count:2`) and 208-210 (`band_select`, the Ctrl band's tile read) |
| O12 | Drag-move selection with grid snap | :92–207 | M1 (single) / M3 05-04 (group) |  | core `a group drag moves the whole selection as one undo step, and undo restores every member`; `editor-bridge: M3 multi-select ok` (batch move saved == builder, byte-exact undo, an off-map member refuses the whole move); `map-editor-engine: M3 multi-select group move ok`; map-editor-m3-auto 198-203 (a drag from a selected squad moves both, ONE step) |
| O13 | Garrison / tow / couple by dropping, with cursor feedback | :796–877, 1325 | M3 05-04 | D-27 | `editor-bridge: M3 properties and links ok` (TestM3PropertiesAndLinks: a live garrison pair through BkEditorCanLink/SetLink, the WaterTankBig slot refusal naming the rule, unlink, the host delete and restore byte-exact); core `the drop links, garrisons beside the host, and the unlink clears`; map-editor-m3-auto 250-269 (`link_make:@0=@1`, `expect=link_with:@0=@1`, ONE step, undo and redo walk it) |
| O14 | Right-click: deselect / cycle overlapping | :968–1073 | M3 05-04 | D-25 | core `the selector's right click alone deselects, and a right click with the left held cycles`; map-editor-m3-auto 204-206 and 281-282 (`rclick` alone: `expect=selection_count:0`) |
| O15 | Double-click / Enter / Space opens properties | :627, 1183 | M3 05-04 | D-26 | view `in Select a double click, Enter or Space on a selection asks for the Properties window (O15)` (the Select tool takes the double click since 05-04); main.zig opens the window on the ask; map-editor-m3-auto 234 (`props_open:1`) |
| O16 | Delete key removes the selection (also from start commands/reserve positions) | :627–743 | M1 (single) / M3 05-04 (multi) / M2 (references) | M2: the delete cascades through start commands and reserve positions and restores every reference on undo (D-04) | **multi (05-04):** core `Delete takes the whole selection through the cascade as one undo step`; `editor-bridge: M3 multi-select ok` (two objects deleted, the cascade's references restored on undo byte for byte); map-editor-m3-auto 212-228 (Delete takes both as ONE step, `expect=objects:0`; one undo brings both back, `expect=objects:2`; redo). **M2 part closed (04-02):** `map-file: M2 cascade kinds ok`, `map-file: M2 find references ok`, `editor-bridge: M2 cascade delete (all kinds) ok`, `map-editor-engine: M2 delete round trip ok`; `test-editor-bridge-m2-sweep` cascade-deleted and restored a unit on 37 shipped maps byte-exact (details: phase 4 `04-PARITY.md`) |
| O17 | Properties: building (units, Script ID, player, health) | SEditorMApObject.cpp:47–65 | M3 05-04 | D-26. M2 (04-09) delivered the single-selection Script ID field and its `script_id` command; M3 keeps the rest | panels_logic `property kinds: the catalogue's game types answer their MFC manipulator's set` (building: units, Script ID, player, health); `map-file: M3 object fields ok` (SetObjectPlayer/Angle/Formation/Link, byte-exact inverses); `editor-bridge: M3 properties and links ok` (the fields edit saved == builder, byte-exact undo); core `properties fields commit as one undo step; an equal value records nothing`; panels_logic `health percent clamps to the MFC's own 1..100, non-finite stands at 100`; map-editor-m3-auto 234-244 (`props_set:player=1`, `props_set:health=50` ONE step each, `expect=hp:@0:50`). Script ID: `editor-bridge: M2 script ids ok`, `map-editor-engine: M2 script id round trip ok` (04-09) |
| O18 | Properties: trench (units, Script ID) | :220–227 | M3 05-04 (fields) / M2 (trench drawing) | M2: the Entrenchment tool draws, selects and deletes a trench whole | panels_logic `property kinds: the catalogue's game types answer their MFC manipulator's set` (trench: units, Script ID - the read-only units list and the M2 `script_id` field); the trench's units are its garrison, linked through the same rules (`editor-bridge: M3 properties and links ok`). **M2 part (drawing) closed (04-08):** `map-file: M2 trench properties ok (500 polylines)`, `editor-bridge: M2 entrenchments draw ok`, `editor-bridge: M2 entrenchment delete ok`, `map-editor-engine: M2 entrenchment round trip ok (19 pieces, 2 sections)` (details: phase 4 `04-PARITY.md`) |
| O19 | Properties: unit (units, angle, Script ID, scenario unit, player incl. flag swap, health, formation) | :295–321 | M3 05-04 | D-26. M2 (04-09) delivered the single-selection Script ID field and its `script_id` command; M3 keeps the rest | panels_logic `property kinds ...` (unit: units, angle, Script ID, scenario unit, player, health, formation) and `angle: the MFC's degrees-to-direction pair turns and reads back`; core `the formation rides only a squad, and the flag swap renames the record`; `editor-bridge: M3 properties and links ok` (player/hp/angle on a unit saved == builder, formation refused on a non-squad naming the rule, the flag swap Flag_neutral -> Flag_german); `map-file: M3 object fields ok`; map-editor-m3-auto 234-244. Script ID: as O17 (04-09) |
| O20 | Properties: multi-selection (angle, Script ID, player, behaviour) | :658–681 | M3 05-04 | D-26 | drawMultiFields (angle, Script ID, player; the MFC's Behaviour combo is commented-out code there and is not offered) through applyObjectFieldsMany - one commit over every member, ONE undo step: core `the direction wheel turns a multi-selection to its angle as ONE undo step per drag` (the same many-member fields path, two members, one undo); `props_set` applies to the whole selection (commands.zig) |
| O21 | Unlink a garrisoned unit (double-click in units) | TEF:3678 | M3 05-04 | D-27 | the units list's double-click unlink (panels_m3 drawUnitsList -> `link_unlink`); core `the drop links, garrisons beside the host, and the unlink clears`; `editor-bridge: M3 properties and links ok` (BkEditorUnlink, byte-exact); map-editor-m3-auto 274-277 (`link_unlink:@0`, ONE step, `expect=link_with:@0=0`) |
| O22 | In-tab Diplomacy button | editor.rc:371 | NF | Hidden and disabled; the menu path is M3 row | |

## 9. Objects pane: Fences, Bridges; Vector pane: Entrenchments, Roads, Rivers

| # | MFC feature | MFC source | Owner | Evidence |
|---|---|---|---|---|
| VO1 | Fences: list, ghost (Ctrl flip), axis-locked drag | FenceSetupWindow.cpp, RoadDrawState.cpp:519–622, 841–898, 1034 | M2 | **Closed (04-07):** `map-file: M2 fence plan ok`, `editor-bridge: M2 fences ok`, `map-editor-auto-m2` frames 97–118; the fence run appears in the game's shot (`fences +10`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| VO2 | Bridges: list, ghost, drag begin/middle/end spans, all-or-nothing commit | BridgeSetupDialog.cpp, RoadDrawState.cpp:626–667, 969 | M2 | **Closed (04-06):** `map-file: M2 bridge plan ok`, `editor-bridge: M2 bridges draw ok`, `map-editor-engine: M2 bridge round trip ok (5 spans)`, `map-editor-auto-m2` frames 67–96; the game loads two new bridges (`bridges 0 -> 2`) and its shot shows the rotated one over the new road; CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| VO3 | Bridges: Enter toggles built during play (fHP -1), WoodenBig_Heavy only; Delete removes the span group (reworded by phase 4 D-12: the MFC toggle is "built during play", not destroyed/intact) | RoadDrawState.cpp:1241–1384 | M2 | **Closed (04-06):** `editor-bridge: M2 bridge rotate and toggle ok`, `editor-bridge: M2 bridge delete ok`; `map-editor-auto-m2` Enter, then `expect=bridge_built`, with undo and redo; the game loads a built-during-play bridge; `test-editor-bridge-m2-sweep` deleted 23 shipped bridges whole and undid each byte-exact; CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| VO4 | Entrenchments: path draw, segment/turn/terminator build, hover highlight, Enter props, Delete | RoadDrawState.cpp:673–1585 | M2 | **Closed (04-08):** as O18 drawing, plus `map-file: M2 trench overlay ok` and `map-editor-auto-m2` frames 119–139; the trench appears in the game's shot. The trench's properties (Enter props) are O18's fields, M3 05-04; CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| VO5 | Roads/Rivers: width modes single/multi/all, width, opacity, type list | TabVOVSODialog.cpp | M2 | **Closed (04-05, 04-13):** the Roads & Rivers panel; in mode All the width and opacity sliders re-width the selected line as one undo step (core test, `map-editor-auto-m2` `vso_width_mode:all` leg); (details: phase 4 `04-PARITY.md`) |
| VO6 | Roads/Rivers: select/add/edit states, Insert/Delete points, width handles, opacity drag | VectorStripeObjectsState.cpp | M2 | **Closed (04-05):** `map-file: M2 vso builder ok`, `editor-bridge: M2 roads ok`, `editor-bridge: M2 road edits ok (8 edits)`, `map-editor-engine: M2 road round trip ok`, `map-editor-auto-m2` frames 28–66; the game reads one more road (`roads 3 -> 4`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| VO7 | Rivers update AI passability | VectorStripeObjectsState.cpp:1042–1094 | M2 | **Closed (04-05):** `editor-bridge: M2 rivers and passability ok` (present after add, gone after delete); the game reads one more river (`rivers 0 -> 1`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |

## 10. Map Tools, Groups, AI Settings panes

| # | MFC feature | MFC source | Owner | Portable equivalent / note | Evidence |
|---|---|---|---|---|---|
| MT1 | Damage tool: % to add, left damage / right heal / middle repair | TabToolsDialog.cpp:95, MapToolState.cpp:29–186 | M3 05-04 | D-29 | the Damage tool (tools_damage.zig; the Map Tools panel's whole-percent field, default 10, panels_m3 drawDamageTool): core `the damage tool damages, heals and repairs, one undo step per click`, `the damage tool keeps a unit at 1% and refuses stats-less objects`; view `the Damage tool - left damages, right heals, Alt+left and the middle button repair to full, one step each`; `editor-bridge: M3 damage ok` (TestM3Damage: 30% hit saved == builder (D-40.3), engine state checked against the record, byte-exact undo, the 1% unit and squad floors, repair, a record no engine object carries stats for refused without a crash - the MFC's null dereference not copied); map-editor-m3-auto 320-334 (25%: a real left click damages to 75%, a right click heals to 100%, `damage:@0:damage`/`damage:@0:repair`, ONE step each) |
| MT2 | Script areas: rectangle/circle draw, name dialog, list centres camera, delete | MapToolState.cpp:188–274, TabToolsDialog.cpp:109–149, AreaNameDialog.cpp | M2 | Script Areas tool (key 8) and panel | **Closed (04-10):** `map-file: M2 script areas ok`, `editor-bridge: M2 script areas ok`, `map-editor-auto-m2` frames 186–232; the game's script finds the area (`area m2_area found at 3072,3072 (lua 3072,3072)`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| G1 | Reinforcement groups: list, new (auto-ID), delete, hide checked, script IDs per group | GroupManagerDialog.cpp, GetGroupID.cpp, EnterScriptIDDialog.cpp | M2 | Groups window | **Closed (04-09):** `editor-bridge: M2 groups ok`, `editor-bridge: M2 hide checked ok`, `map-editor-engine: M2 groups round trip ok`, `map-editor-auto-m2` frames 140–185; the game holds the group's unit and lands it (`group 900 held 1`, `script group 4245 landed one unit`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |
| AI1 | AI general: side radios, mobile reinforcements, positions, parcels on the map, placeholders, type dialog | TabAIGeneralDialog.cpp, StateAIGeneral.cpp, TabAIGeneral*Dialog.cpp | M2 | AI General tool (key 9) and panel | **Closed (04-12):** `map-file: M2 parcel formulas ok`, `editor-bridge: M2 ai general ok`, `editor-bridge: M2 ai general edits ok`, `map-editor-engine: M2 ai general round trip ok`, `map-editor-auto-m2` frames 304–337; the enemy general reads the parcel (`general side 1 parcels 0 -> 1 (parcel type=1 r=256 dir=0)`); CI run 36692345194 (details: phase 4 `04-PARITY.md`) |

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
| S2 | Save recomputes full-map shades | TEF:2934–3291 | M3 05-01 | Replaced: region shades at edit time and explicit Update Map (D-19, D-20) | `map-file: M3 altitude region ok` + `editor-bridge: M3 altitudes ok`: the region primitive shades at edit time (GrowForShades ±1 vertex) and undo restores bytes exactly — the whole-map recompute at save is gone | |
| S3 | Save writes the QuickLoadMapInfo chunk | TEF:3238–3258 | M1 | MapFile | test-map-files |
| S4 | Save packs frame indices, writes squads once, nudges bridge spans | TEF:2934–3291 | M1 / M2 | Overlay packs only edited objects (spec); bridge spans M2 | **Closed (M1 packing; M2 bridge spans, 04-06):** `editor-bridge: M2 bridges draw ok` (the save equals the `NMapGeometry::PlanBridge` map); `test-map-files-all` 1,755 of 1,755; the M2 sweeps restore every edit byte-exact (`map-file: M2 sweep 59 maps, 460 edits`, `editor-bridge: M2 sweep 57 maps, 238 edits`); (details: phase 4 `04-PARITY.md`) |
| S5 | Load recomputes full shades with the season sun | TEF:1657 | M1 (not copied) | Engine built from the snapshot; shades unchanged | test-map-files-all |
| S6 | Load: scenario units drawn blue; future-build bridges blue | TEF:1357–2113 | M3 05-04 / M1 | Scenario-unit tint in D-26; future-build list M1 | markers.zig drawScenarioTint rings every scenarioObjects record blue (the records' `scenario` flag) and the Properties window shows the scenario-unit line (panels_m3); the future-build bridges stay M1's. Visual only: the record is untouched (`editor-bridge: M3 properties and links ok` saves == builder) |
| S7 | Load: garrisons placed beside their host | TEF:1357–2113 | M3 05-04 | Links shown and editable (D-27) | BkEditorSetLink places a garrison beside its host (the MFC's -30,+30 load-time layout) and markers.zig drawLinkLines draws one line per garrison, tow or coupling from the passenger to its host; links editable through the drop, `link_make`/`link_unlink` and the units list: core `the drop links, garrisons beside the host, and the unlink clears`; `editor-bridge: M3 properties and links ok`; map-editor-m3-auto 250-277 |

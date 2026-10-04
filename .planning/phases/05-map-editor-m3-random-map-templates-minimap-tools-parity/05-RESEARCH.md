# Phase 5: Map editor M3: random map templates, minimap tools, full parity - Research

**Researched:** 2026-09-30
**Domain:** Portable map editor (Zig app + C++ EditorBridge + RandomMapGen engine) reaching MFC Map Editor parity
**Confidence:** HIGH (every load-bearing claim read from source this session; file:line anchors throughout)

## Summary

Phase 5 completes the portable editor. All the hard engine machinery M3 needs already exists and is linked into the editor build: `CMapInfo::CreateRandomMap` (RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp:793) is a self-contained static that writes map + `.seed` + `.lua` + minimap images under a caller-supplied `rszOutputRoot`, driven headless today by `tools/zig/random_missions_test.cpp` — that file is the exact model for D-02/D-04 (bridge entry, seed determinism). The five RMG composers need no new engine code either: `SRMContainer`/`SRMGraph`/`SRMFieldSet`/`SRMTemplate`/`SRMContext`/`SRMSetting` each serialise through both `IDataTree` (XML, labels in RMG_Methods.cpp) and `IStructureSaver` (binary), and `LoadDataResource`/`SaveDataResource` already resolve through mounted storages. The shipped corpus verified on disk: **43 templates, 102 graphs, 404 containers, 27 field sets, 10 settings, 46 chapters, 1696 patch .bzm files** (D-07's numbers confirmed).

The carried railroad crash is pinned: the game builds the AI railroad graph at map load via `CAILogic::Init` (AILogicInternal.cpp:820-821) → `CRailroadGraphConstructor::Construct` (AILogic/RailroadGraph.cpp:624) → `CSplineEdge::CSplineEdge` (RailroadGraph.cpp:52), which reads `edgeDescriptor.controlpoints[0]` unconditionally (line 55) and indexes `edgeParts[nControlPointsSize-1]` — `edgeParts[-1]` on an empty vector (line 69). Phase 4 empirically confirmed "<2 control points crashes at open" (04-REVIEW-FIX.md:60). The guard belongs in `CRailroadGraphConstructor::Construct`'s filter loop (RailroadGraph.cpp:626-630): skip `TYPE_RAILROAD` entries with `controlpoints.size() < 2`.

**Primary recommendation:** Follow D-39's 11-plan split. Each feature maps to an existing engine call or serialiser (anchors below); the genuinely new C++ surface is small — a bridge region-edit primitive for altitudes (D-19), a `CreateRandomMap` bridge entry + D-05 `szMODName` fix, a `CreateMiniMapImage` entry, and the AILogic railroad guard. Everything else is Zig app/panel work over the existing snapshot/overlay/undo architecture.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
Copied from `.planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-CONTEXT.md` `<decisions>` (D-01..D-40). The full text is authoritative in that file; the plan-split and exit criteria most shape planning:

- **D-01..D-05** Create Random Map dialog (MFC fields + seed), one new bridge entry point into `CMapInfo::CreateRandomMap`, output under user/mod maps folder via `rszOutputRoot`, never `Data`; main-thread with `IProgressHook` progress modal (19 steps, no cancel); determinism test with fixed seed instead of pulling 999.1's polygon-fill; `szMODName` bug (RMGeneration.cpp:971) fixed in M3.
- **D-06..D-13** Five composers as dockable ImGui windows under Tools; read/write existing formats unchanged via RandomMapGen serialisers; lists by scanning storage folders (not `Editor\Default*.xml`); shipped RMG files read-only; user RMG files under `<UserRoot>rmg/` mounted as a storage layer; container patch outside storages offered a copy-in (replaces the MFC refusal); graph canvas with MFC node/link interactions; Check! per composer with MFC validation rules, fixes explicit and undoable; Templates Check! implemented (MFC's is dead); Tools 0-3 → Export lists to `<UserRoot>mapeditor/logs/`.
- **D-14..D-17** Minimap panel (Editor mode: terrain colours, heights gradient, 17 player colours, fire ranges, camera frame, patch grid; click/drag moves camera; Game mode from `<map>.tga`/`_h.dds`); Create Minimap Images is explicit, calls `CreateMiniMapImage`, writes `<map>_large` (512) and `<map>` (256) DDS+TGA.
- **D-18..D-24** Heights tool (brush 2-16 profile.tga, raise/lower/level, 4 level modes, speed/ratio, invalid-height rollback unless Ctrl, Generate Hills/Rocks/Dunes TG_FBM/TG_HYBRID/TG_RIDGED, Set Zero); **D-19 preservation invariant extended to altitudes** (region edit + `UpdateTerrainShades` over grown region + push, undo restores region); Update Map command; Instant Update; Fit To Grid via `FitVisOrigin2AIGrid`; Fields tool (polygon keys, Randomize, Fill/Place/Modify/Check/Update, season prompt); Fill Entire Map (typo not copied; brush 1-16); New Map (1-32 patches, season, name, mod); Save as XML/BZM (Ctrl+Shift+X/B).
- **D-25..D-31** Multi-selection (Ctrl+click, rubber bands, squads, group move/delete, cycle overlap); Properties panel per kind (`SEditorMApObject` fields, commit-on-deactivate, Script ID/HP as overlay fields); links via `CheckForInserting` rules editing `link.nLinkWith`, delete-host-unlinks-passengers; direction wheel; Damage tool (%, left damage/right heal/middle repair, HP ≥1% clamp, MFC null bug not copied); Players add/delete (≤16+neutral, re-map to neutral); Unit Creation Info (party, 5 aviation slots, paratroops, relax time, appear points); object filters (9 toggles, combo, Filters Composer, shipped `Data/Editor/filter.xml`/`filterSetup.xml`, user filters to `<UserRoot>mapeditor/filter.xml`).
- **D-32..D-35** Layers menu bridged to `IScene::ToggleShow`/`IGFX::SetWireframe`, re-applied after open/new, remembered; Check Map (duplicates, links, player index, parties, + unknown objects; results panel + `checkmap_log.txt`; fix-all undoable; save never fixes silently); app shell (Options, View, status bar, title, drag-drop, command-line map, single instance via local socket/pipe, Help window, About); tile properties including index 0.
- **D-36..D-38** `05-PARITY.md` is the checklist of record, rows close only with evidence; every M3 feature gets core/map-file/engine/auto-tier coverage (D-37); MFC editor deletion plan runs last (D-38 list: `Sources/src/MapEditor/`, `Sources/src/bin/MapEditor.exe`, A7.sln project, vscode task, `tools/zig/stage.zig` + `game_install.ps1` entries, openspy check assertion).
- **D-39** 11 plans: 1 Foundations (spec D-19 change, altitude region primitive, New Map, save format, brush 1-16, status bar/title, parity checklist verified) → 2 Heights/Update/Fill/tile props → 3 Filters/Fields → 4 Multi-select/properties/links/wheel/damage → 5 Players/UnitCreation/Check Map → 6 Layers/fire ranges → 7 Minimap → 8 Create Random Map + Export lists → 9 Containers+Graphs composers + user RMG root → 10 Fields+Templates composers → 11 App shell + parity verification + hand try + MFC deletion. Waves: 2-8 depend on 1; 9 on 8; 10 on 3+9; 11 on all + phase 4.
- **D-40** Ten exit criteria (see Validation Architecture below).

### the agent's Discretion
- Exact ImGui layouts, window names and shortcut keys where the MFC editor has none, and the tick rate of the minimap refresh.
- The single-instance IPC mechanism per platform (Unix domain socket or named pipe).
- The internal form of the region-edit primitive for altitudes, as long as it is deterministic and tested with the same function (D-19).
- Whether a composer shares one generic "list + properties" widget.

### Deferred Ideas (OUT OF SCOPE)
- The game's Custom Mission list offering user-generated random maps or user templates (a game feature).
- 999.1's polygon-fill speed-up (M3 only supplies the determinism harness, D-04).
- Free camera rotation (phase 4's revisit).
- "Not a feature" items (undo/cut/copy/paste, storage coverage, Sounds/Forests/RMG tabs, `ID_TOOL_4`, `CPESelectStringsDialog`, hidden noise radios) — listed in 05-PARITY.md, not deferred, not re-implemented.
</user_constraints>

## Project Constraints (no AGENTS.md — conventions from CONTEXT files and spec)

- C++ never uses `std::min`/`std::max`; use `Min`/`Max` from `Misc/Tools.h`.
- Never `zig fmt build.zig`.
- GOG files and the AchtungPanzer2 mod are never committed.
- Bridge calls are guarded, status-returning, and document their units (world vs map vs VIS tiles).
- Core (`Sources/editor/core`) is std-only; SDL, ImGui and the C ABI live in the app.
- Every edit is a command with do/undo; the engine is updated immediately.
- Shipped data is read-only (Save → Save As); user data under `<UserRoot>`, mod-specific under `<UserRoot>mods/<Folder>/`.
- Tests run in tiers; artefacts go to `zig-out/local-test`.
- Working directory for all M3 work: `.worktrees/map-editor-6` (branch `feat/map-editor-m2`); every plan's tests also run on win-home (`ssh win-home`, repo at `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg`).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Random map generation | Engine (RandomMapGen static) | EditorBridge (new entry) + app dialog | `CreateRandomMap` owns all generation; the bridge only marshals parameters/seed and the progress hook |
| RMG composers (CRUD + XML I/O) | App (ImGui windows) | Engine serialisers (`operator&(IDataTree&)`) via bridge | UI is app; format fidelity must come from RandomMapGen's own serialisers, never re-typed in Zig |
| Minimap render | App (CPU texture) | Engine (`CreateMiniMapImage` for file output; `BkEditor*` snapshot data for live panel) | Live panel is a view over session data; image files are an engine pipeline |
| Heights/fields/fill terrain edits | EditorBridge (region primitive) | Engine (`UpdateTerrainShades`, `FillTileSet`, `FillObjectSet`, `FillProfilePattern`, `CHField`) | Preservation invariant lives in the bridge snapshot/overlay; engine computes derived state |
| Update Map / UpdateAllHeights / FitToGrid | Engine calls via bridge | App command/undo | Deterministic engine functions; undo = region restore |
| Multi-select / properties / links / damage | Core (selection model) + EditorBridge (record edits) | Engine validation (`CheckForInserting` rules) | Selection is pure app state; edits must land as overlay records (`nScriptID`, `fHP`, `link.nLinkWith`) |
| Players / Unit Creation Info | EditorBridge records | Engine (`SUnitCreationInfo`, `partys.xml`) | Map-data records with `MutableValidate`-style validation |
| Layers toggles | Engine (`IScene::ToggleShow`, `IGFX::SetWireframe`) via bridge | App settings persistence | Renderer state is engine-owned; M3 fixes the MFC desync by re-applying after open |
| Check Map | Core/engine checks via bridge | App results panel + log | Checks read session + catalogue; fixes are undoable record edits |
| Railroad <2-CP crash guard | Engine (AILogic load path) | Editor Check Map flag | The game must never crash on a malformed map; the editor must flag it |

## Standard Stack

No new external packages. Everything is in-repo C++ (engine/bridge) and Zig (app). **Package Legitimacy Audit: not applicable — this phase installs no external packages.**

| Component | Location | Role in M3 |
|-----------|----------|-----------|
| `CMapInfo::CreateRandomMap` | RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp:793 | The whole generator (D-01..D-05) |
| `CMapInfo::CreateMiniMapImage` (static) | RandomMapGen/MapInfo_StaticMethods_MiniMapCreation.cpp:52 | Minimap image files (D-17) |
| `CMapInfo::UpdateTerrainShades` | RandomMapGen/MapInfo_StaticMethods.cpp:504 → `CVertexAltitudeInfo::UpdateShades` (VA_Types.h:501) | D-19 region shades |
| `CMapInfo::UpdateTerrainCrosses` | RandomMapGen/MapInfo_StaticMethods.cpp:433 | Cross recompute after tile edits |
| `CMapInfo::FillTileSet` / `FillObjectSet` / `FillProfilePattern` | MapInfo_StaticMethods_RMGeneration.cpp (used at :1019, :1742-1743, :1766; fields tool StateTerrainFields.cpp:417-481) | Fields tool fills (D-21) |
| `CHField`/`NPerlinNoise` | RandomMapGen/TerrainGenerator.h, PNoise.h | Generate heights (D-18) |
| RMG serialisers | RMG_Types.h structs; XML labels in RMG_Methods.cpp | Composers (D-06..D-12) |
| EditorBridge | Sources/src/EditorBridge/bridge.h (1429 lines, C ABI) + session*.cpp | New entry points (region edit, RMG I/O, random map, minimap) |
| Zig app | Sources/editor/{core,app} | Panels, tools, auto verbs, tests |

## Engine Seam Inventory (verified anchors)

### CreateRandomMap — the full pipeline
`bool CMapInfo::CreateRandomMap( SMissionStats *pMissionStats, const std::string &rszContextFileName, int nLevel, int nGraph, int nAngle, bool bSaveAsBZM, bool bSaveAsDDS, SRMUsedTemplateInfo *pRMUsedTemplateInfo, IProgressHook *pProgressHook, const std::string &rszOutputRoot )` — MapInfo_StaticMethods_RMGeneration.cpp:793 [VERIFIED: signature read].

Step sequence (each `pProgressHook->Step()` site cited):
1. :824-835 singletons + first Step; :837-841 loads `SRMTemplate` (`RMGC_TEMPLATE_XML_NAME` = `"Template"`, RMG_Consts.cpp:17); :843-868 merges `SRMSetting` extra fields (`"ChapterSetting"`, RMG_Consts.cpp:23).
2. :873-882 output root: `const std::string szOutputRoot = rszOutputRoot.empty() ? std::string( pDataStorage->GetName() ) : rszOutputRoot;` and `szRandomMapName = szOutputRoot + "maps\\" + szFinalMap` unless the name is already absolute (drive letter). **`rszOutputRoot` must be backslash-terminated with a trailing separator** — `random_missions_test.cpp:70-76` builds exactly that ("backslashes and a trailing one, which is what CreateRandomMap appends names to").
3. :888-900 creates the host directory (`std::filesystem::create_directories`, with `\\`→`/` fix-up off Windows).
4. :901-922 **seed handling**: writes `<map>.seed` from `IRandomGen::GetSeed()`, then `const unsigned int nLegacySeed = Random(); pRandomGen->SetSeed( pRandomGenSeed ); NWin32Random::Seed( int( nLegacySeed ) ); srand( nLegacySeed );` — the comment explains the tile variants (`rand()` in `STileTypeDesc::GetMapsIndex`) and polygon jitter (`NWin32Random` in `RandomizeEdges`) are re-seeded from the stored seed so regeneration reproduces. **This is why D-01's optional seed is settable and why D-04's determinism test works.**
5. :924-925 `mapInfo.Create(template.size, nSeason, szSeasonFolder, 0, nType)`; :941-950 loads tileset/crosset descs; :960-972 copies script file name, diplomacies, camera anchors, unitCreation, forest sounds, chapter — and **the D-05 bug**: `mapInfo.szMODName = randomMapTemplate.szChapterName;` (:971) — it stores the chapter name instead of the active mod's name. `szMODVersion` comes from the template (:972). The fix: record the active mod name+version like the M1 save.
6. :988-1003 loads `RoadLevel` (`RMGC_ROAD_LEVEL_FILE_NAME` = `"RoadLevel"`, RMG_Consts.cpp:19) and builds the VSO height gradient from its profile tga.
7. :1005-1027 default field set load + `ValidateFieldSet(tilesetDesc, MOST_COMMON_TILES[season])` + `FillTileSet` for the whole map.
8. :1029-1056 graph selection (index or `graphs.GetRandom(true)`), load `SRMGraph`; :1058-1086 angle selection (0-3 or `Random(4)`).
9. :1088-1103 rotates unit-creation appear points around the AI center.
10. :1105-1280 per graph node: loads `SRMContainer` (`"Container"`), picks a patch by angle+setting via `GetIndices`, places it (`AddMapInfo`), collects river/road begin/end VSO points. Patch maps load via `LoadTypedSuperLatestDataResource(name, ".bzm", 1, ...)` (:1172) — patches are read as .bzm/.xml pairs through storage.
11. :1294-1432 chapter context (`SRMContext`, label `"ChapterUnitsTable"`, RMG_Consts.cpp:22): `context.IsValid(SRMTemplateUnitsTable::DEFAULT_LEVELS_COUNT=3, players)`; rewrites unit names from `unitPlaceHolders[player][unitRPGType]` per level; erases objects whose type has no entry.
12. :1441-1448 renumbers river/road IDs; :1450-1675 links: `nParts < 8 → 8` (:1455-1458), `CVSOBuilder::FindPath` + `CreateVSO` + `MergeVSO` for each graph link, VSO height patterns applied.
13. :1679-1686 `fieldGraph.FindPolygons`.
14. :1697-1770 polygon fill loop: `RandomizeEdges(...)`, per-polygon field set via `fields.GetRandom()`, `FillTileSet` + `FillObjectSet` + `FillProfilePattern` (this is 999.1's cost centre).
15. :1778-1779 `mapInfo.UpdateTerrain(terrainPatchesRect)`, `mapInfo.UpdateObjects(terrainTilesRect)` — note `UpdateObjects` renumbers all link IDs (MapInfo_StaticMethods.cpp:513-562).
16. :1788-1834 objectives' `vPosOnMap` on the 512×512 briefing map (fixed a scenario-objects index bug — comment at :1814-1816).
17. :1838-1859 minimap images: `SRMImageCreateParameter( szMapImageName, CTPoint<int>( 0x200, 0x200 ), bSaveAsDDS, false, INTERMISSION_IMAGE_BRIGHTNESS/CONSTRAST/GAMMA )` and `SRMImageCreateParameter( szRandomMapName, CTPoint<int>( 0x100, 0x100 ), bSaveAsDDS )` — 512 `_large` + 256 [VERIFIED: RMGeneration.cpp:1853-1854]. `szMapImage = rszOutputRoot + pMissionStats->szMapImage`; MFC sets `szMapImage = szFinalMap + "_large"` (TemplateEditorFrame1.cpp:256).
18. :1861-1901 `PackFrameIndices`, `SQuickLoadMapInfo.FillFromMapInfo`, save as .bzm (chunk 1 = mapInfo, chunk `RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER` = 2, name `"QuickLoadMapInfo"` — MapInfo_Consts.cpp:28-29) or .xml (`AddTypedSuper` + `RMGC_QUICK_LOAD_MAP_INFO_NAME`).
19. :1908-1929 copies the template's `<script>.lua` beside the map.

Step count: `RMGC_CREATE_RANDOM_MAP_STEP_COUNT = 19`, `RMGC_CREATE_MINIMAP_IMAGE_STEP_COUNT = 10` [VERIFIED: MapInfo_Consts.cpp:33-34 verbatim].

### IProgressHook
`StreamIO/ProgressHook.h:3-11` [VERIFIED]: `interface IProgressHook : public IRefCount { virtual void STDCALL SetNumSteps( const int nRange, const float fPercentage = 1.0f ) = 0; virtual void STDCALL Step() = 0; virtual void STDCALL Recover() = 0; virtual void STDCALL SetCurrPos( const int nPos ) = 0; virtual int STDCALL GetCurrPos() const = 0; virtual void Stop() = 0; };`
The MFC implementation `CCreateRandomMapProgress` (CreateRandomMapDialog.cpp/.h:13-51) drives an `IDD_PROGRESS` dialog; the frame constructs it with `RMGC_CREATE_RANDOM_MAP_STEP_COUNT` (TemplateEditorFrame1.cpp:282). **Bridge note:** a C ABI cannot vtable into this interface directly; there is already a precedent for driving such interfaces from tool code — `StreamIOZig/legacy_bridge.cpp:143` keeps a "Vtable mirror of StreamIO/ProgressHook.h IProgressHook, for driving the" engine from Zig-side tooling [VERIFIED: grep hit]. The M3 bridge entry should take a plain C callback (`void (*)(int step, void *user)`) and adapt it inside session.cpp into an `IProgressHook` implementation, following that mirror's pattern.

### Existing CreateRandomMap callers (all verified)
- MFC editor: TemplateEditorFrame1.cpp:207-298 (dialog fill: settings from `Scenarios\Settings\`, templates from `Scenarios\Templates\`, contexts from `Scenarios\Chapters\`; `RMGC_ANY_SETTING_NAME` = `"<any setting>"` appended; output name normalised under `maps\`).
- Game briefing/random missions: `Sources/src/Main/RandomMapHelper.cpp:75` (writes into `NGeneratedData::Root(userProfile MOD)` and then an `.xml` mission + `.seed`); `Sources/src/GameTT/Mission.cpp:169` (same root, records `SRMUsedTemplateInfo` via `AddUsedTemplate`). **D-05's fix must not disturb these** — they pass their own mission stats; the fix only changes what `szMODName` is set from inside the generated map.
- Headless test: `tools/zig/random_missions_test.cpp:179-261` — the determinism recipe [VERIFIED]: generate → copy `first.bzm` → `CreateObject<IRandomGenSeed>(STREAMIO_RANDOM_GEN_SEED)` → `pSeed->Restore(seedStream)` → `GetSingleton<IRandomGen>()->SetSeed(pSeed)` → regenerate with the same graph index + angle → compare. Note the test compares with `NMapFile::AreEquivalent(map, again, &szWhere)`; **D-40.5 demands byte-identical `.bzm`**, which is stricter than the random-missions tier — plan the determinism test as a byte comparison.

### CreateMiniMapImage (static)
MapInfo_StaticMethods_MiniMapCreation.cpp:52-161+ [VERIFIED head]: loads `minimap.xml` from the season folder (`RMGC_MINIMAP_FILE_NAME`), creates `SRMMiniMapCreateParameter::MML_COUNT` layers at `tiles*2` resolution, and computes per-tile colours by **averaging the tile's UV rect sampled from `<tilesetdesc><high-quality DDS>`** (`pDataStorage->OpenStream( ( rLoadMapInfo.terrain.szTilesetDesc + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) )...)`, :95) — i.e. the `_h.dds` D-14 names for the live panel. Rivers layer from `desc.miniMapCenterColor/miniMapBorderColor` (:191-213), y-flipped (`imageSize.y - ... - 1`). The MFC Create button path is `CTemplateEditorFrame::CreateMiniMap` (TemplateEditorFrame1.cpp:300) which saves changes first then calls the static.

### Terrain update seams (D-19/D-20/D-21)
- `UpdateTerrainShades(&terrain, rect, sunlight)` → `CVertexAltitudeInfo::UpdateShades` (MapInfo_StaticMethods.cpp:504-511; VA_Types.h:501). Sunlight per season from global vars `Scene.SunLight.<Season>.Diffuse/Ambient/Direction` (VA_Types.h:475-499) — **the shade depends on neighbouring vertices' normals, so the update region must be grown**; the MFC grows the brush rect by 1 vertex on each side: `updateRect( cornerTile.x - 1, cornerTile.y - 1, cornerTile.x + 1 + patternSize, cornerTile.y + 1 + patternSize )` [VERIFIED: DrawShadeState.cpp:210-213]. D-19's "shade kernel" growth = this ±1.
- `UpdateTerrainCrosses(&terrain, rect, tilesetDesc, crossetDesc)` (MapInfo_StaticMethods.cpp:433-476) — runs `CTerrainBuilder` per patch; used by full `UpdateTerrain`.
- `CStaticMap::UpdateAllHeights` (AILogic/AIStaticMap.cpp:1134) exposed through `CAIEditor::UpdateAllHeights` (AILogic/AIEditorInternal.cpp:450-452 → `theStaticMap.UpdateAllHeights()`); the MFC Update Map calls `pAIEditor->UpdateAllHeights()` first (TemplateEditorFrame1.cpp:5163).
- `FitVisOrigin2AIGrid( CVec3 *pPos, const CVec2 &vOrigin )` [VERIFIED: Formats/fmtTerrain.h:54-59 verbatim] — `Vis2AI( pPos ); FitAIOrigin2AIGrid( pPos, vOrigin ); AI2Vis( pPos );` where `FitAIOrigin2AIGrid` subtracts the origin, rounds to a 32-unit grid, adds it back. Used on place/move when Fit-To-Grid is on (ObjectPlacerState.cpp:104,171,360) and in Update Map's sprite-object pass (TemplateEditorFrame1.cpp:5186-5214 — only `SGVOT_SPRITE` buildings/objects/terraobj with non-empty passability).

### CheckForInserting (D-27 link rules)
`CObjectPlacerState::CheckForInserting( std::vector<SMapObject*> &v, SMapObject *p, int *pType )` — ObjectPlacerState.cpp:1325-1424 [VERIFIED]:
- Passengers must all be infantry (`SUnitBaseRPGStats::IsInfantry()`).
- Building target: `slots.size() + nRestSlots + nMedicalSlots >= v.size()`.
- Trench target: check commented out (accepts).
- Vehicle target: `vEntrancePoint != VNULL2 && nPassangers >= v.size()`.
- Tow fallback (type 2): host is `RPG_TYPE_TRN_CARRIER` or `RPG_TYPE_TRN_TRACTOR` with `vTowPoint != VNULL2` and `fTowingForce > 0`, target is artillery with `vPeoplePoints` and `fTowingForce > fWeight`.
- Train coupling fallback (type 1): host `IsTrain(type)` and target `IsTrain(type)`.

### RMG serialisers (composer formats)
Structs with both `operator&( IStructureSaver &ss )` and `operator&( IDataTree &ss )` [VERIFIED: RMG_Types.h — SRMLevelVSOParameter :55-56, SRMPatch :79-80, SRMContainer :143-144, SRMGraphNode :170-171, SRMGraphLink :225-226, SRMGraph :267-268, SRMObjectSetShell :297-298, SRMTileSetShell :318-319, SRMFieldSet :368-369, SRMVSODesc :395-396, SRMTemplate :490-491, SRMTemplateUnitsTable :698-699, SRMContext :768-769, SRMSetting :777-778]. XML labels in RMG_Methods.cpp: e.g. `"Size"/"Season"/"SeasonFolder"/"UsedScriptIDs"/"UsedScriptAreas"` (container :101-105, graph :288-292), `"Rect"` (node :219), `"Type"` (link :246), `"Objects"/"Width"/"Ratio"` (object shell :404-407), `"Tiles"/"Width"` (tile shell :426-427), `"Height"/"PositiveRatio"` (field set :455-459), `"Fields"/"Graphs"/"CameraAnchor"/"Diplomacies"/"ChapterName"/"MissionIndex"/"AttackingSide"/"MODName"/"MODVersion"` (template :571-592), `"UnitCreation"` (units table :1115). `SRMGraphLink` defaults [VERIFIED: RMG_Consts.cpp:30-38 verbatim]: `DEFAULT_RADIUS = fWorldCellSize * 4; DEFAULT_PARTS = 8; DEFAULT_MIN_LENGTH = fWorldCellSize * 4; DEFAULT_DISTANCE = 0.3f; DEFAULT_DISTURBANCE = 0.1f;` and `SRMVSODesc::DEFAULT_WIDTH = fWorldCellSize * 2; DEFAULT_OPACITY = 1.0f;`.
Shipped corpus counts [VERIFIED: `find Data/Scenarios/... -name "*.xml" | wc -l`]: templates **43**, graphs **102**, containers **404**, field sets **27**, settings **10**, chapters **46**; patches **1696** `.bzm`. Templates/Settings etc. are organised in season subfolders (Templates/{Africa,Spring,Summer,Winter}).

## MFC → Portable Feature Notes (per D-39 plan, with anchors)

### Plan 05-01 Foundations
- New Map: `OnFileNewMap` TEF:2362-2444+ [VERIFIED head] — NeedSaveChanges first, MOD key `<current MOD>` (`RMGC_CURRENT_MOD_FOLDER`) keeps old mod, dialog fields size/season/name/MOD (NewMapDialog.cpp [CITED: PARITY F1]); engine create via `CMapInfo::Create` + fill with season's most common tile (D-23; `MOST_COMMON_TILES` used at RMGeneration.cpp:1012).
- Load zero altitudes if missing: TEF:1658 [CITED: PARITY F5].
- Save as XML/BZM: TEF:4406/4413 [CITED: PARITY F8]; portable uses Ctrl+Shift+X/B (D-24).
- Title: TEF:153-205 [VERIFIED head — SetWindowText with name+ext]; brush combo 1×1..16×16 default 2×2: MainFrm.cpp:487-503 [Cited: PARITY V4].
- Status bar VIS/SCRIPT coords + object line: MainFrm.cpp:167-279, TEF:1185-1284 [Cited: PARITY V6].

### Plan 05-02 Heights, Update Map, Instant Update, Fit To Grid, Fill, tile properties
- Heights dialog: brush slider `SetRange( 2, 16, true )` [VERIFIED: TabTerrainAltitudesDialog.cpp:159]; pattern from `"editor\\profile.tga"` [VERIFIED: :183]; defaults `fParameters = {1.0f, 0.03f, -3.0f, 3.0f, 0.3f}` (height, level ratio, min Z, max Z, granularity) [VERIFIED: :146-154]; level modes enum `LEVEL_TO_0..LEVEL_TO_3`, default `LEVEL_TO_2` [VERIFIED: :141-143]; gen types `TG_FBM/TG_MULTI/TG_HETERO/TG_HYBRID/TG_RIDGED` (:250-278) — of which MULTI/HETERO radios are hidden+disabled in editor.rc:503-507 [Cited: PARITY TR13], leaving the three D-18 names.
- Stroke semantics [VERIFIED: DrawShadeState.cpp:186-336]: middle or L+R = level (to 0 / click-tile height / instant average / click average per mode), left = add pattern, right = subtract; after apply, if not Ctrl and `!CVertexAltitudeInfo::IsValidHeight(altitudes, updateRect)` the pattern is **subtracted back** (rollback); else if Instant Update (`m_bNeedUpdateUnitHeights`) `pAIEditor->ApplyPattern(...)` + `UpdateObjectsZ(updateRect)`; then `CMapInfo::UpdateTerrainShades(&terrain, updateRect, GetSunLight(season))` + `pTerrainEditor->Update(CTRect<int>(0,0,0,0))`. Right-button ApplyPattern uses `fRatio = -1` then restores 1 (:317-319). Click-tile height = average of the 4 vertices of the tile under the cursor (:151-154); instant average recomputed per move (`GetAverageHeight`, :162,238); click average frozen at stroke start (`fAverageHeight`, :244).
- Generate heights [VERIFIED: TabTerrainAltitudesDialog.cpp:312-357]: confirm; `NPerlinNoise::Init(); CHField hfield(w,h); SfBmValues fBmValue = CHField::fBmDefVals[type]; fBmValue.featSize = granularity; hfield.Generate(fBmValue);` then every altitude `= ((H - range.min) * (maxZ - minZ) * fWorldCellSize / range) + minZ * fWorldCellSize`; then `OnButtonUpdate()` + minimap refresh. Set Zero: confirm, `altitudes.SetZero()`, Update, minimap (:359-387).
- Update Map [VERIFIED: TEF:5138-5231]: progress `m_objectsAI.size() + 7` steps; `UpdateAllHeights` → `UpdateTerrain(full tiles)` → `UpdateTerrainShades(full altitudes, season sun)` → `pTerrainEditor->Update(patches-1)` → `UpdateObjectsZ(full)` → if `ifFitToAI`: per sprite object `FitVisOrigin2AIGrid(&vPos, pRPG->GetOrigin(nFrameIndex))` + `MoveObject` (buildings/objects/terraobj with passability) → timer update. `SetMapModified`.
- Instant Update toggle = `OnShowScene14` (TEF:5249, `m_bNeedUpdateUnitHeights`); Fit toggle = `OnButtonfit` (TEF:5287, `ifFitToAI`).
- Fill Entire Map [VERIFIED: TEF:4921-4968]: confirm; every tile ← selected tile's `GetMapsIndex()` over `patches*16`; **the typo**: `terrainRect.maxx =- 1; terrainRect.maxy =- 1;` (:4951-4952) makes the terrain update rect (0,0,-1,-1) — do not copy; then full `OnButtonUpdate()` + minimap + SetMapModified.
- Tile properties: name + variant count, tile 0 included (D-35); MFC `> 0` bug at TabTileEditDialog.cpp:284-316 [Cited: PARITY TR2].

### Plan 05-03 Filters + Fields
- Filters: nine quick toggles + combo + New/Delete at TabSimpleObjectsDialog.cpp:424-516/158/175 [Cited: PARITY O2-O4]; Filters Composer dialogs CreateFilterDialog.cpp etc. Shipped format [VERIFIED head of Data/Editor/filter.xml]: `<base><filters><item><key>Buildings</key><data><Filter><item><data><item>buildings</item>...` — conditions are lists of folder words; filter.xml = 671 lines, filterSetup.xml = 41 lines.
- Fields tool [VERIFIED: StateTerrainFields.cpp + TabTerrainFieldsDialog anchors]: keys — click adds vertex, right-click removes last (double pop + push, :155-170), double-click/Enter/Space closes via `UniquePolygon` + area check (>:172-207), Esc keeps only the last point (:208-217); Select/Edit states drag a vertex (`CVSOBuilder::UpdateZ` keeps it on the ground, :80-88), Insert bisects the next edge (:109-120), Delete removes the dragged vertex (:121-128). `POINT_RADIUS = fWorldCellSize / 4.0f` (:222). Randomize: `RandomizeEdges(cut, 10, fP[1], CTPoint(0, fP[2]), &out, fP[0]*fWorldCellSize, 512*fWorldCellSize, true)` (:350-359) with dialog params min length / width / disturbance [Cited: TabTerrainFieldsDialog.cpp:229-277]. Application (:312-516): polygon cut by map bounds → optional RandomizeEdges → season check (`GetSelectedSeason(map) != GetSelectedSeason(fieldSet)` → `IDS_INVALID_FIELD_SEASON` YES/NO, :391-406) → `ValidateFieldSet` + `FillTileSet` (terrain) → `FillObjectSet` into a scratch summer `CMapInfo` and `AddObjectByAI` each object (optionally `CanAddObject`-filtered) (:430-457) → if `fHeight > 0` gradient from `szProfileFileName+".tga"` + `FillProfilePattern(patternSize, fPositiveRatio)` + full `UpdateObjectsZ` + full `UpdateTerrainShades` (:459-492) → optional full `OnButtonUpdate` (:493-496). One application = one undo step recording tiles, crosses, altitudes, objects (D-21).

### Plan 05-04 Multi-selection, properties, links, wheel, damage
- Selection/keys anchors [Cited: PARITY O9-O15, ObjectPlacerState.cpp:393-523 single/squad, :446 Ctrl+click, :209-243/886-962 rubber bands, :92-207 drag-move, :968-1073 right-click cycle/deselect, :796-877/1325 links, :627/1183 properties open].
- Properties [VERIFIED head: SEditorMApObject.cpp:47-81] — building manipulator registers exactly `"Units"` (VAL_UNITS, read-only count + "DblClick for unlink"), `"Script ID"` (VAL_INT), `"Player"` (VAL_COMBO over `diplomacies.size()`), `"Health"` (VAL_FLOAT). Trench (:220-227) and unit (:295-321) and multi (:658-681) sets per PARITY O18-O20 [Cited]. Flag swap `Flag_<party>` and scenario-unit blue tint per D-26 (TEF:1357-2113 [Cited: PARITY S6]).
- Damage tool [VERIFIED: MapToolState.cpp:29-186]: `hpAdded = nParameters[0]/100.0f` (default 10 per TabToolsDialog.cpp:95 [Cited]); clamp keeps `fHP <= 1.0` and `>= fMinHP` where `fMinHP = 0.01f` if `IsTechnics() || IsHuman()` else 0 (:57-70); `hpAdded *= pTmp->pRPG->fMaxHP;` then `IAIEditor::DamageObject` (:72-75). Right = negative (heal, :122-131); middle = repair to full `(fHP - 1.0f) * fMaxHP` (:173-177). **The MFC null bug**: `pTmp->pRPG->fMaxHP` is dereferenced without a null check (:72, :133, :174) and `pObjects[0]` is used unguarded — do not copy (D-29).
- Direction wheel: DirectionButton.cpp, TEF:1053 [Cited: PARITY O6].

### Plan 05-05 Players, Unit Creation, Check Map
- Players add/delete ≤16+neutral with Insert/Delete/0/1 keys: TabSimpleObjectsDiplomacyDialog.cpp:263/434 [Cited: PARITY M4].
- Unit Creation Info: TEF:5070-5084 [VERIFIED head] opens `CPropertieDialog` with `m_unitCreationInfo.GetManipulator()`; fields per D-30 (party from partys.xml, 5 aviation slots name/formation/count, paratroop squad+count, relax time, appear points edited on map or PEPointsListDialog [Cited]).
- Check Map [VERIFIED: TEF:5938-6144 head]: duplicate detection = same position + same `szKey` + same frameIndex, non-squad → delete + report name/pos/scriptID (6006-6071); player index out of range → re-map to `diplomacies.size()-1` (6010-6014); invalid/duplicate links → clear `pLink` / delete (6078-6144); parties tail at 6145-6390 [Cited]. MFC calls `CheckMap(false)` silently on save (TEF:2934 [Cited: PARITY S1]) — replaced by warn-only (D-33). **M3 adds**: unknown object types (M1's warning) and **road/railroad with <2 control points** (the carried bug); results panel with jump-to + `<UserRoot>mapeditor/logs/checkmap_log.txt`; fix-all one undoable command.

### Plan 05-06 Layers
- Handler anchors TEF:5318-5777 [Cited: PARITY L1-L15]: Terrain/Grid/WireFrame (`IGFX::SetWireframe`)/Depth Complexity/Terrain Noise/Black Stripes/Units/Objects/Bounding Boxes/Shadows/Haze/War Fog (works, no menu)/Selector/Units Passability (`ToggleAIInfo`)/Unit Fire Ranges + filter combo (TEF:4991, 2115; MainFrm.cpp:622). Storage coverage L16 = NF (computation commented out TEF:5622-5746, no UI). Wireframe/depth-complexity need GFXGPU support — measure in plan (PARITY L3/L4 notes).

### Plan 05-07 Minimap
- Live panel [VERIFIED: MiniMapDialog.cpp:308-346 + MiniMapTypes.cpp]: click/drag → `newCamera3 = ((point - rect) scaled to terrain) + camera3 - center3` then `NormalizeCamera` (the screen-centre offset, :320-329); paint order terrain → fire ranges (`pScene->GetAreas(&pShootAreas,...)`, MiniMapTypes.cpp:87-96, circles+sectors :102-148) → units → camera frame (4 screen corners via `GetPos3`, :25-80) → patch grid (XOR lines, MiniMapDialog.cpp:299-305). Units in the 17 player colours — verbatim table at MiniMapTypes.cpp:149-167 (`MINIMAP_PLAYER_COLORS_COUNT = 17`, green/red/blue/yellow/cyan/magenta/white then 0x80 mixtures, index 16 grey) — drawn in AI tile space, passability rect with 5-tile minimum (:223-263), one marker per squad gated by `FilterName("squads\\german_hmg")` (:268-308). Game mode reads `<map>.tga` or `<map>_h.dds` (UpdateControls :495-512). Viewport rubber band and Close are commented out (MM5 NF, :541-560).
- Create images: TEF:300 CreateMiniMap → save-first → `CMapInfo::CreateMiniMapImage` static (sizes verified above); writes `<map>_large` (512) + `<map>` (256) DDS+TGA (D-17; both formats [Cited: CONTEXT D-17], sizes [Verified: RMGeneration.cpp:1853-1854]).

### Plan 05-08 Create Random Map + Export lists
- Dialog fields [VERIFIED: CreateRandomMapDialog.cpp:54-82 vID table]: template (combo+browse), context (combo+browse), graph index (edit), setting (combo, `<any setting>` appended by the frame TEF:244), direction radios 0/90/180/270, level radios 0/1/2, Save-as-BZM + Save-as-DDS checkboxes, map name (edit+browse). OK disabled until all non-empty (:238-245). MFC runs generation modal with `CCreateRandomMapProgress(this, RMGC_CREATE_RANDOM_MAP_STEP_COUNT, ...)` (TEF:282-283) and does **not** open the map afterwards — M3 does (D-02).
- M3 adds the seed field: set `IRandomGen` seed before calling (the generator stores/reseeds itself, :901-922); read back `SRMUsedTemplateInfo` + the stored `.seed` to display "seed used".
- Export lists Tools 0-3: MainFrm.cpp:939-1138 [Cited: PARITY T3-T6] writing `graphs_list.txt`, `contexts_list.txt`, `patches_list.txt`, `maps_list.txt` in MFC format — exact line format to be captured in plan 05-08 from those handlers (open question below). Destination `<UserRoot>mapeditor/logs/` (D-13). `ID_TOOL_4` NF (would rewrite `Data`).

### Plan 05-09 Containers + Graphs composers + user RMG root
- Container Add [VERIFIED: RMG_CreateContainerDialog.cpp:195-230]: multi-select `.bzm`/`.xml` file dialog; **the refusal** — `if ( szFilePath.find( szStorageName ) != 0 ) { return; }` (path must live under the active storage; PARITY cites :207). D-10 replaces with copy-into-`<rmg root>/Scenarios/Patches/<season>/`. Patch properties (setting, tri-state N/E/S/W indices, multi-edit) at :207-318/930-975 + RMG_PatchPropertiesDialog.cpp [Cited: PARITY R2-R3].
- Graph composer canvas [partly verified]: handlers at RMG_CreateGraphDialog.cpp:1061-1548 (LButtonDown/Up, MouseMove, DblClk) [VERIFIED: grep anchors]; zoom slider and node/link interactions per PARITY R4-R6 [Cited]; `nParts >= 8` validation matches the engine's own clamp (RMGeneration.cpp:1455-1458).
- Composer list files `Editor\Default*.xml` not shipped → folder scan (D-08, PARITY R14).
- User RMG storage root (D-09): `<UserRoot>rmg/` mounted in `BkEditorStart`/`BkEditorSetMod` beside the mod storage; everything (patches, graphs, field sets, profiles, templates) then resolves through storage exactly like shipped data.

### Plan 05-10 Fields + Templates composers
- Fields Composer tabs (terrain/objects/heights) RMG_Field*Dialog.cpp [Cited: PARITY R8-R10] mapping 1:1 onto `SRMFieldSet` fields (`tilesShells`/`objectsShells`/`szProfileFileName`+`fHeight`+`patternSize`+`fPositiveRatio`, RMG_Types.h:327-370 [Verified]). Objects tab needs the O4 filters.
- Templates Composer: lists + weights (`fields`, `graphs`, `vso` weight vectors), `nDefaultFieldIndex`, VSO width/opacity; Diplomacy…/Units… (unit creation grid)/script file/MOD combo; writes Template + `QuickLoadMapInfo` entry (RMG_CreateTemplateDialog.cpp:328-355/1117/1344/1443 [Cited: PARITY R11-R12]). The MFC Templates "Check!" does nothing (R13) — M3 implements graph/field/VSO validation (D-12).
- Composer round trip (D-40.4): every shipped file loads, saves, reloads equal **through the portable composer code** — drive `LoadDataResource`/`SaveDataResource` for all 43/102/404/27 files on the data-only tier (the `random-missions-test` linking pattern, build.zig:6704+, is the template for such a tool).

### Plan 05-11 App shell + deletion
- Options (`szGameParameters`, default format): MapEditorOptions.cpp, TEF:6464 [Cited: PARITY T2]; View toggles + Reset layout (D-34); single instance: MFC uses `WM_COPYDATA` (MainFrm.cpp:1200, editor.cpp:125 [Cited: PARITY F13]) — M3 uses per-user local socket/named pipe at agent's discretion; drag-drop MainFrm.cpp:1120, command-line map editor.cpp:188 [Cited]. Help window (no .chm shipped), About. Composer layout persistence = ImGui ini (PARITY R15).
- Deletion list (D-38) + Runtime State Inventory below.

## The Railroad <2-Control-Points Crash (carried bug)

**Crash chain** [VERIFIED by reading]:
1. `CAILogic::Init` — AILogic/AILogicInternal.cpp:820-821: `CRailroadGraphConstructor railroadGraphConstructor; railroadGraphConstructor.Construct( terrainInfo, &theRailRoadGraph );`
2. `CRailroadGraphConstructor::Construct` — AILogic/RailroadGraph.cpp:624-630: for every `terrain.roads3[i]` with `eType == SVectorStripeObjectDesc::TYPE_RAILROAD`, `railroads.push_back( new CRailroad( terrain.roads3[i] ) );`
3. `CSplineEdge::CSplineEdge( const SVectorStripeObject &edgeDescriptor )` — RailroadGraph.cpp:52-79: `Vis2AI( &p3, edgeDescriptor.controlpoints[0] );` (line 55 — **reads [0] of a possibly-empty vector**), `edgeParts.resize( nControlPointsSize + 1 )` (line 59 — size 1 when empty), then `edgeParts[nControlPointsSize-1]` (line 69 — **`edgeParts[-1]` when empty: out-of-bounds write**).
4. Empirical confirmation from phase 4: `.planning/phases/04-.../04-REVIEW-FIX.md:60` — "a railroad with fewer than 2 control points crashes the AI's `CRailroad` graph at open."

**Where the guard goes (recommendation):** in `CRailroadGraphConstructor::Construct`'s loop — skip railroads whose `controlpoints.size() < 2` (RailroadGraph.cpp:626-630). Rationale: (a) it is the earliest engine consumer of raw file data at load, before any spline/graph math; (b) guarding in `CSplineEdge` itself would need a degenerate-edge contract every caller then handles, and the graph cannot use a 0/1-point railroad anyway (`FindIntersections`/`SetEdges` assume a real spline); (c) a one-line skip matches how the MFC editor's own writers never produce such roads (the M2 bridge refuses them: `LongEnough` = `rVso.controlpoints.size() >= 2 && rVso.points.size() >= 2`, EditorBridge/session_vso.cpp:160 [Verified], with an engine test on a 1-point and 0-point road, 04-REVIEW-FIX.md:57-59).

**Editor side (D-33):** Check Map flags any road/railroad record with `controlpoints.size() < 2` as an error entry (jump-to + log). The bridge already exposes per-VSO control counts (`BkEditorVso` fills `control_count`, bridge.cpp:2705 [Verified grep]).

**Test tiers:** (1) engine/game tier — craft a map with a <2-CP railroad, run the game under `BK_AUTO_UI` (the `map-editor-game-reads-it` pattern) and assert clean exit; the crash is in `CAILogic::Init` which the editor's headless bridge session does not run, so proving the guard needs the real game (or an AILogic-level engine test if one exists — none does today). (2) `BK_EDITOR_AUTO` step: place railroad, delete points below 2, Check Map reports it.

## Portable Editor Reuse Map (what exists, what M3 adds)

**Exists and reusable:**
- `Sources/src/EditorBridge/bridge.h` (1429 lines) [Verified]: session open/save/close, paint + `SPaintRecord` undo tokens, tombstones, catalogue, tile pictures, mods + `BkEditorSetMod`/`BkEditorPaths`, camera/zoom/yaw, `BkEditorCaptureFrame`, screen/world/map transforms, sounds records, camera anchors, script file/areas, start commands, reserve positions, AI general, groups, hidden script IDs, generic `BkEditorUndoEdit`/`RedoEdit`, and M2's full VSO set (`BkEditorVso/Add/Move/SetWidth/SetOpacity/InsertPoint/DeletePoint/Pick/Delete/MatchesEngine`).
- `Sources/editor/core`: `history.zig` (undo, drag merging), `document.zig`, `settings.zig` (lenient `key=value`, `mapeditor.cfg` — add layers/default-format/game-params/filters keys), `files.zig`/`shipped.zig` (read-only shipped, user folders), `fake_bridge.zig` (core tests), `tools*.zig` (registry with right-button/Ctrl-as-right/double-click support from 04-03), `records.zig` (generic record path with `record_add`/`record_delete` from 04-09).
- `Sources/editor/app`: `panels.zig`/`panels_logic.zig`/`panels_m2.zig` (menu bar already has File/Edit/Map/Tools/View/Test — panels.zig:2125-2195 [Verified grep]; Sounds panel's commit-on-deactivate pattern for D-26), `view.zig`/`view_math.zig`, `pictures.zig` (GPU texture upload → minimap texture), `auto.zig` (`BK_EDITOR_AUTO` Action union — key/press/drag/release/click/rpress/rdrag/rrelease/rclick/dblclick/tool/text/wheel/open/save/saveas/test_in_game/waitgame/shot/compare/do/expect/exit [Verified: auto.zig:130-155], held-press support from 04-05), `testlaunch.zig` (takes Options game parameters), `game_reads_common.zig` (game-reads-it harness).
- Engine linked into the editor build: `build.zig addMapEditor` (2198) links RandomMapGen; `addRandomMissionsTest` (2200) shows the data-only/headless startup + full engine linking pattern (build.zig:6704-6790 [Verified]).

**Gaps M3 must fill (new C ABI surface, all `Guarded`, status-returning):**
1. Altitude region-edit primitive (D-19): set heights over a region + `UpdateTerrainShades` over region grown by the shade kernel (±1 vertex, DrawShadeState.cpp:210-213) + engine push, with an undo record of the region (MapFile gets the altitude region record beside `MapOverlay`).
2. `BkEditorCreateRandomMap`-style entry (D-02): params (template, context, level, graph, angle, bzm, dds, name, seed in/out, output root) + progress callback adapted to `IProgressHook` inside the bridge; returns `SRMUsedTemplateInfo` + seed used.
3. RMG record I/O for the five composers: list/scan + read/write `SRMContainer`/`SRMGraph`/`SRMFieldSet`/`SRMTemplate` (+ context/setting reads for the dialog combos) through the RandomMapGen serialisers — either generic record marshalling or per-composer structs; must round-trip byte-equal through `SaveDataResource`.
4. `BkEditorCreateMiniMapImage` (D-17) on the saved map.
5. Terrain generate (CHField), fields application (`FillTileSet`/`FillObjectSet`/`FillProfilePattern` over a polygon), Fill Entire Map, Update Map composite (D-20) — engine calls with progress callbacks.
6. Layer toggles (`IScene::ToggleShow`/`IGFX::SetWireframe`/`ToggleAIInfo`) + fire-range layer state queries.
7. Check Map queries (duplicates/links/player/parties/<2-CP VSO/unknown types) over the session — likely core-side over existing record reads + catalogue.
8. Multi-select move/delete/properties as record edits (players, angle, script ID, HP, formation, flag swap, link/unlink) — extend the object records `nScriptID`/`fHP` overlay fields (already `SMapObjectInfo` fields) and `link.nLinkWith` (M2's delete cascade already maintains references, 04-02).
9. Unit creation records (`SUnitCreationInfo` per player) + players add/delete (diplomacies vector is already bridged via `BkEditorSetDiplomacy`; add/delete changes its length — check the M1 assumption baked into bridge validation).

## Runtime State Inventory (for the deletion plan 05-11)

> This phase deletes the MFC editor — a removal/migration, so the inventory is required.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — the MFC editor writes only into the data storage root it was pointed at; no repo-held DB or per-user store besides the M1 `<UserRoot>` the portable editor already owns (files.zig/shipped.zig) | None |
| Live service config | None — no external services | None |
| OS-registered state | MFC single-instance used `WM_COPYDATA` (MainFrm.cpp:1200 [Cited]); no OS registrations to clean | None (portable editor replaces with its own IPC) |
| Secrets/env vars | None — MFC options were registry/dialog-based (MapEditorOptions.cpp) with no repo env keys | None |
| Build artifacts / installed packages | `Sources/src/MapEditor/` tree; checked-in `Sources/src/bin/MapEditor.exe`; `MapEditor` project in `Sources/src/A7.sln`; `.vscode/tasks.json` "Build MapEditor (Debug)"; `Editors/MapEditor.exe` in `tools/zig/stage.zig` and `tools/zig/game_install.ps1`; MapEditor assertion in `tools/openspy/check_no_gamespy_runtime.ps1` (D-38 list) | Delete all in 05-11; keep `Sources/src/RandomMapGen` (game uses it) and `Data/Editor` (portable editor reads filters/markers); rewrite code comments citing `MapEditor/…` file:line |

**Verification after deletion (D-40.9):** `git grep -n 'src/MapEditor\|MapEditor.vcxproj\|Editors/MapEditor'` finds only history in `.planning`/`docs/superpowers/plans`; game, editor and every CI job still build.

## Common Pitfalls

### Pitfall 1: Backslash-terminated `rszOutputRoot`
**What goes wrong:** generation writes to a wrong path or fails stream creation. **Why:** `CreateRandomMap` concatenates `rszOutputRoot + "maps\\" + name` (RMGeneration.cpp:873-877) and the minimap name (1839). **Avoid:** build the root exactly like `random_missions_test.cpp`'s `GeneratedRoot` — absolute, backslashes, trailing separator. **Warning sign:** maps appear next to the wrong folder or `Can't create stream`.

### Pitfall 2: Determinism is seed + graph + angle (and legacy `rand`)
**What goes wrong:** regenerating "with the same seed" differs. **Why:** the map depends on `IRandomGen` (seeded), `NWin32Random` and `srand` (re-seeded from the stored seed, RMGeneration.cpp:912-921), and on the graph/angle choice when `nGraph`/`nAngle` are -1. **Avoid:** fix graph and angle in the determinism test (the random-missions tier's recipe, random_missions_test.cpp:231-261) and restore the `.seed` before the second run. **Warning sign:** `AreEquivalent` diff at tile variants or polygon jitter.

### Pitfall 3: Byte-identical ≠ AreEquivalent
D-40.5 demands byte-identical `.bzm`; the random-missions tier compares via `NMapFile::AreEquivalent`. Also remember `SVertexAltitude` is written as a raw struct — 3 padding bytes per vertex differ between a fresh read and a copied buffer (STATE.md, 04-01 decision: "read the map fresh for every write they compare, never a copy").

### Pitfall 4: Main-thread generation + progress modal deadlock
**What goes wrong:** the modal pumps messages but the generator never yields; or the bridge callback crosses the C ABI into Zig while C++ holds engine singletons. **Why:** D-03 mandates main-thread; `Step()` is the only yield point (19 of them). **Avoid:** the `IProgressHook` adapter lives in C++ (session.cpp), and the callback only sets an atomic/pushes an event the app renders after the call returns — or the app accepts a frozen progress window updated inside `Step()` via the platform message pump, exactly as MFC's `CCreateRandomMapProgress` does (it only invalidates a progress control). **Warning sign:** beach-balled window during generation.

### Pitfall 5: Shade region under-growth
**What goes wrong:** heights edit changes a vertex but its neighbour's shade goes stale → visual seam and non-preservation vs engine. **Why:** shades derive from vertex normals (±1 neighbours). **Avoid:** always grow the shade-update region by 1 vertex (DrawShadeState.cpp:210-213) and record exactly that grown region in the undo record (D-19's "region grown by the shade kernel"). **Warning sign:** `test-map-files-all` byte mismatches along brush edges.

### Pitfall 6: Composer XML drift
**What goes wrong:** portable-written RMG XML fails to load in the game's `CreateRandomMap`. **Why:** hand-rolled serialisation in Zig. **Avoid:** all reads/writes go through the RandomMapGen `operator&(IDataTree&)` serialisers behind bridge entry points; D-40.4's round trip (load→save→reload equal) on all 43/102/404/27 shipped files is the gate; the authored-file end-to-end (D-40.5) closes the loop into the game.

### Pitfall 7: `UpdateObjects` link renumbering
`mapInfo.UpdateObjects` renumbers every link ID (MapInfo_StaticMethods.cpp:513-562). Any M3 bridge path that calls engine-level update helpers after link edits must keep the overlay's `nLinkWith` mapping consistent — M2's ID mapping vector pattern (04-05 decision) is the precedent.

### Pitfall 8: Deleting the MFC editor too early
The deletion plan runs last and only after every PARITY row (M2's included) is closed with evidence (D-38/D-40.1). Wireframe/depth-complexity layers (L3/L4) are the risk rows — they need GFXGPU support that must be measured first.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Random map generation | Any generator logic in Zig | `CMapInfo::CreateRandomMap` (RMGeneration.cpp:793) | 2100 lines of validated engine behaviour; the game must load the result |
| RMG XML formats | Zig-side XML reading/writing of composer files | RandomMapGen `operator&(IDataTree&)` serialisers via bridge | Label fidelity, defaults, weight vectors; files must load in the game |
| Determinism harness | Custom seed bookkeeping | `.seed` file + `IRandomGenSeed::Restore` + `SetSeed` (random_missions_test.cpp recipe) | The engine already re-seeds `NWin32Random`/`srand` from the stored seed |
| Minimap terrain colours | Per-tile colour sampling in Zig | `CreateMiniMapImage`'s averaging (MiniMapCreation.cpp:94-161) for files; same tileset-DDS averaging for the live panel | Identical colours between panel and written images |
| Terrain generation noise | New noise impl | `CHField`/`NPerlinNoise` (`fBmDefVals`) | MFC-parity output, including the min/max scaling formula |
| Polygon fill / jitter | New polygon math | `RandomizeEdges`, `CutByPolygonCore`, `UniquePolygon`, `ApplyTilesInPolygon` (RandomMapGen/Polygons, used by both RMG and the fields tool) | 999.1 owns performance; M3 reuses semantics |
| Progress hook crossing | Zig vtable into `IProgressHook` | C callback adapted in C++ (session.cpp), pattern of StreamIOZig/legacy_bridge.cpp:143 | C ABI cannot safely mirror a C++ interface vtable |
| Spline/railroad math | Guarding inside spline code | Skip `<2`-CP railroads in `Construct` (RailroadGraph.cpp:626-630) | Earliest load consumer; graph cannot use a degenerate railroad |

## Test-Tier Strategy (D-37 / D-40)

**Existing zig build steps** [Verified: build.zig step registry]:
- `zig build test` (core + map-file tiers, all CI targets); `test-map-files` / `test-map-files-all` (5699/5713; the 1,755-map sweep); `test-map-files-m2-sweep` (5730).
- `test-editor-bridge` (5849), `test-editor-bridge-m2-sweep` (5865).
- `test-map-editor-engine` (6607) — drives the real engine through editor-core commands.
- `map-editor-smoke` (6063), `map-editor-auto` (6110), `map-editor-auto-m2` (6551), `map-editor-game-reads-it` (6567), `map-editor-game-reads-it-m2` (6587), `test-map-editor-auto` (2838, the schedule parser + TGA comparison tests).
- `test-random-missions` (option `--random-missions-sweep all|cover|only=`, 1848; built by `addRandomMissionsTest` 6704).

**New steps M3 should add:**
1. `zig build map-editor-m3-auto` (D-37/D-40.7): a `BK_EDITOR_AUTO` scenario beside `map-editor-auto-m2`, one scripted step per new tool/composer/menu command; runs locally on macOS arm64 + win-home.
2. Composer round-trip (D-40.4): a data-only-tier tool (link pattern of `addRandomMissionsTest`) that loads, saves, reloads and compares all shipped templates/graphs/containers/field sets; run on the five engine-C++ targets like the random-missions tier.
3. Determinism test (D-04/D-40.5): generate twice with a fixed seed through the editor's Create Random Map path, byte-compare `.bzm`; then `BK_AUTO_UI` game load + clean exit (the game-reads-it pattern).
4. Engine-tier additions (D-40.3): heights, fields, fill, Update Map, multi-select move/delete, properties, links, damage, players, unit creation, New Map — each saved, read back, compared with the expected-value builder (the M2 round-trip test shape).
5. Railroad guard: game-reads-it scenario with a <2-CP railroad map (loads cleanly, exits) + Check Map auto step.

## Validation Architecture

**Framework:** Zig `zig build` step matrix + C++ test executables (random-missions-test pattern); app-tier scenarios via `BK_EDITOR_AUTO`; game-tier via `BK_AUTO_UI`/`BK_MAP_TRACE`. Config: `build.zig` (never `zig fmt`). Quick run: `zig build test`. Full local: `zig build test map-editor-smoke map-editor-auto map-editor-auto-m2 test-map-editor-engine test-editor-bridge test-random-missions=cover` (+ `map-editor-m3-auto` when added); sweeps (`test-map-files-all`, `*-m2-sweep`) before the phase gate.

### D-40 exit criteria → signals

| # | Exit criterion | Automated signal | Human |
|---|----------------|------------------|-------|
| 1 | Every PARITY row closed with evidence | PR fills Evidence column; NF rows cite file:line | Johannes reviews |
| 2 | Core+map-file tiers green on 6 CI targets | CI run green (M1 model: run id in SUMMARY) | — |
| 3 | Engine tier green (macOS arm64 + Windows-MSVC) incl. new round trips | `test-map-editor-engine` + expected-value builder comparisons | — |
| 4 | Composer round trip: 43+102+404+27 files load/save/reload equal, data-only tier, 5 engine-C++ targets | new composer round-trip step | — |
| 5 | Authored template+graph+container+field set, fixed seed → byte-identical `.bzm`; game loads under `BK_AUTO_UI`, clean exit | determinism + game-reads-it steps | — |
| 6 | `CreateMiniMapImage` writes 4 images beside a saved user map; minimap click moves camera | `map-editor-m3-auto` shot comparison | eyeball the shots |
| 7 | `zig build map-editor-m3-auto` passes locally (macOS arm64 + win-home) | local runs recorded | — |
| 8 | `test-map-files-all` 1,755 maps, 0 FAIL | sweep output | — |
| 9 | MFC editor fully deleted; `git grep` clean; everything still builds | grep command + full CI matrix | — |
| 10 | Hand try on the release build approves M3 | — | Johannes signs off |

**Sampling rate:** per task/commit — quick tier (`zig build test` + touched tier); per wave merge — full local matrix; phase gate — all ten rows above. Every plan's GUI-adjacent tests also run on win-home (`ssh win-home`).

**Wave 0 gaps:** none in test infrastructure — tiers exist; the new steps above are created by the plans that need them (m3-auto in plan 1 or 8's predecessors, round-trip in 9/10, determinism in 8).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `SRMImageCreateParameter` writes both DDS and TGA (D-17's "both") | Minimap | Extra format flag plumbing in the bridge entry |
| A2 | Export-list line formats (graphs_list.txt etc.) can be lifted verbatim from MainFrm.cpp:939-1138 | Plan 05-08 | Format mismatch vs MFC output; low risk (logs are informational) |
| A3 | `CAIEditor::ApplyPattern` remains the correct Instant-Update seam in the portable bridge (it is MFC-internal; M3's engine tier must confirm the equivalent headless path) | Heights/Instant Update | May need a bridge-side objects-Z update instead |
| A4 | Layers toggles are reachable headless through the bridge session's scene (Wireframe/Depth Complexity on GFXGPU unmeasured) | Plan 05-06 | PARITY L3/L4 already flag "measure in the plan" |
| A5 | Diplomacy length changes (players add/delete) are compatible with existing bridge validation (M1 assumed fixed size) | Plan 05-05 | Validation rewrite; caught by core tests early |
| A6 | Unit creation records can reuse the M2 generic record path (`record_add`/`record_delete`) | Plan 05-05 | A bespoke record type; moderate |

## Open Questions (RESOLVED)

All four questions are dispositioned inside their consuming plans; nothing remains open for planning or execution to answer up front. Each entry keeps the original question and names the disposition.

1. **Wireframe/Depth-Complexity on GFXGPU** — do the renderer paths exist headless and on macOS Metal? Measure first in plan 05-06 (PARITY L3/L4 already say so).
   *Resolution (05-06 Task 1):* measured before the menu ships by the engine-tier probe `TestM3LayerProbe` (per layer: does the call succeed headless, does the state read back, does a captured frame change). `BkEditorLayers` returns a per-layer availability mask from the probe's findings; unavailable layers are greyed out with the probe finding as tooltip — never silently no-op'd — and PARITY L3/L4 evidence cites the probe by name and result.
2. **`CAIEditor` availability in the bridge session** — `UpdateAllHeights`/`ApplyPattern` live behind `IAIEditor`; confirm the editor-bridge session constructs it (MFC-only singleton today?). Decides whether Update Map's objects-Z pass is `pAIEditor`-driven or reimplemented in the bridge.
   *Resolution (05-02, assumption A3):* 05-02 measures first whether the bridge session can construct IAIEditor headless; if it cannot, Update Map's objects-Z pass is reimplemented bridge-side over `CMapInfo::UpdateObjectsZ` / per-object `UpdateZ` (the VSO builder already exposes UpdateZ). The measurement and the choice land in the 05-02 SUMMARY and the code comments.
3. **Progress-modal UX on macOS** — MFC pumps messages inside `Step()`; confirm the SDL app can repaint its ImGui modal from the C++ callback without re-entrancy (Pitfall 4). If not, a frozen-but-animated modal is the fallback (agent's discretion per CONTEXT).
   *Resolution (05-08 Task 2):* if the platform pump proves safe from the C callback it is used; otherwise the accepted fallback ships — the modal's bar animates from the step counter the callback stores while the window stays frozen. D-03's main-thread mandate holds either way; the choice is documented in the 05-08 SUMMARY.
4. **Composer data model width** — one generic "list + properties" widget vs five bespoke windows (agent's discretion); the round-trip gate is unaffected either way.
   *Resolution (05-09, inherited by 05-10):* 05-09 decides for containers+graphs (the graphs canvas forces at least one bespoke window) and records the decision; 05-10 inherits it for fields+templates, with the Templates Composer keeping a bespoke layout where unavoidable (weights grid + unit-creation grid). Agent's discretion per CONTEXT; the round-trip gate is unaffected either way.

## Sources

### Primary (HIGH confidence — read this session)
- `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp` (CreateRandomMap 793-1934, szMODName 971, seed 901-922)
- `Sources/src/RandomMapGen/MapInfo_StaticMethods_MiniMapCreation.cpp` (CreateMiniMapImage 52+, tile colour averaging 94-161)
- `Sources/src/RandomMapGen/MapInfo_StaticMethods.cpp` (UpdateTerrainCrosses 433, UpdateTerrainShades 504, UpdateObjects 513)
- `Sources/src/RandomMapGen/{RMG_Types.h, RMG_Consts.cpp, MapInfo_Consts.cpp}` (structs, labels, step counts 19/10, QuickLoadMapInfo)
- `Sources/src/StreamIO/ProgressHook.h`; `Sources/src/StreamIOZig/legacy_bridge.cpp:143`
- `Sources/src/AILogic/RailroadGraph.cpp` (52-79 spline ctor, 624-650 Construct); `AILogicInternal.cpp:820-821`; `AILogic/AIStaticMap.cpp:1134`; `AIEditorInternal.cpp:450`
- `Sources/src/MapEditor/`: TemplateEditorFrame1.cpp (207-298, 300-319, 2362-2444, 4921-4968, 5138-5304, 5938-6144), CreateRandomMapDialog.cpp (full), TabTerrainAltitudesDialog.cpp (1-520), DrawShadeState.cpp (60-439), StateTerrainFields.cpp (full), MiniMapDialog.cpp (290-569), MiniMapTypes.cpp (1-330), MapToolState.cpp (20-209), SEditorMApObject.cpp (40-159), ObjectPlacerState.cpp (1325-1424), RMG_CreateContainerDialog.cpp (195-230), RMG_CreateGraphDialog.cpp (anchors)
- `Sources/src/EditorBridge/` (bridge.h surface, session_vso.cpp:160 LongEnough), `Sources/editor/` (auto.zig 130+, settings.zig, panels.zig menu anchors), `tools/zig/random_missions_test.cpp` (determinism recipe 179-261), `build.zig` (step registry), `Data/Editor/filter.xml`
- `.planning/phases/04-.../04-REVIEW-FIX.md:57-60` (railroad crash + LongEnough)
- On-disk corpus counts (find/wc): 43/102/404/27/10/46/1696

### Secondary (MEDIUM — cited via 05-PARITY.md anchors, not re-read)
- TEF save handlers (4406/4413, 2934), MainFrm.cpp toolbars/Tools 0-4 (939-1138), editor.rc dialog/toggle states, NewMapDialog, UnitCreation.cpp, PEPointsListDialog, RMG_*Dialog property dialogs, TabSimpleObjectsDialog filter UI, DirectionButton, MapEditorOptions.

## Metadata

**Confidence breakdown:**
- Engine seams (CreateRandomMap, minimap, shades, serialisers, crash site): HIGH — read directly, anchors verified
- MFC feature behaviour: HIGH for read regions (heights, fields, damage, minimap, check-map, fill, update); MEDIUM for PARITY-cited regions (properties tail, unit creation, export lists)
- Portable-side gaps/test strategy: HIGH (bridge.h, auto.zig, build.zig read; tier model proven by phases 3-4)

**Research date:** 2026-09-30
**Valid until:** 2026-10-30 (repo-internal facts; stable unless phase 4 follow-ups land first)

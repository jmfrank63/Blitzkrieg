# Phase 4: Map editor M2 (roads, rivers, bridges, AI groups, scripts) - Pattern Map

**Mapped:** 2026-09-30
**Files analyzed:** 58 new or modified files (grouped by plan in the tables below)
**Analogs found:** 53 / 58 (5 have no in-repo analog; see "No Analog Found")

All paths are relative to the worktree root `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6`. Every analog below was checked with `git ls-files` and is tracked source (the `Sources/`, `tools/zig/` and `build.zig` trees hold no install or capability mirrors). There is no `CLAUDE.md`; the binding rules are in the auto-memory and RESEARCH.md "Project Constraints": never `std::min`/`std::max` (use `Min`/`Max`), never `zig fmt build.zig` (run `zig test tools/zig/build_hermeticity_test.zig` after editing it), test artefacts in `zig-out/local-test`, no package builds on the Mac, defer `destroy` in worker tests.

**Main analog for everything simple: the M1 sound list (03-10).** It is the exact chain the planner should clone once per M2 collection:

```
tool/panel -> Editor.addX/editX/deleteX (history.reserve, bridge call, record) -> bridge.zig VTable
  -> c_bridge.zig (toC/fromC + two-pass read) -> bridge.h C ABI -> bridge.cpp (Guarded, WellFormed, bounds)
  -> session.cpp (Validate + AddXToSession/SetXInSession/DeleteXFromSession: snapshot AND working together)
  -> test: fake_bridge.zig + editor.zig tests | editor_bridge_test.cpp TestSoundList | map_file_test.cpp
```

## File Classification

### 04-01 Foundations

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/MapFile/MapRecords.h` / `.cpp` (NEW) | utility (overlay ops) | CRUD on in-memory map | `Sources/src/MapFile/MapOverlay.h` / `.cpp` | exact |
| `Sources/src/MapFile/MapGeometry.h` / `.cpp` (NEW) | utility (pure geometry) | transform (numbers in, plan out) | none in repo; port sources `Sources/src/MapEditor/RoadDrawState.cpp`, constants in `Sources/src/Formats/fmtTerrain.h` | no analog (see below) |
| `Sources/src/MapFile/MapOverlay.h` / `.cpp` (MOD: `FindReferences`, cascade, `SAddObject` fields) | utility | CRUD | itself (`MapOverlay.cpp:81-126`, `173-216`) | exact |
| `Sources/src/MapFile/MapEquivalence.cpp` (likely unchanged; already compares every M2 field) | utility | transform | itself (`:249-260`, `:380-410`, `:442-460`) | exact |
| `Sources/src/EditorBridge/session.h` / `session.cpp` (MOD: tombstone extension, refs, palette filter) | service | CRUD + engine sync | itself (`session.h:54-64`, `session.cpp:830-965`) | exact |
| `Sources/src/EditorBridge/catalogue.cpp` (MOD: `placeable` for types 4, 6, 9) | service | request-response | itself (`catalogue.cpp:113`) | exact |
| `Sources/src/EditorBridge/bridge.h` / `bridge.cpp` (MOD: new ABI, `BkEditorPickGroup`, `BkEditorTestMapPath` script copy) | controller (C ABI) | request-response | sound block `bridge.h:634-712`, `bridge.cpp:1795-1890` | exact |
| `Sources/src/EditorBridge/session_records.cpp` (NEW, 04-01 scaffold, filled in 04-05..07) | service | CRUD | `session.cpp:506-646` (sound Read/Validate/Add/Set/Delete) | exact |
| `Sources/editor/core/records.zig` (NEW) | model | CRUD | `Sources/editor/core/bridge.zig:41-90` (`ObjectRecord`, `SoundRecord`) | exact |
| `Sources/editor/core/bridge.zig` (MOD: VTable + record wrappers) | model / interface | request-response | itself (`:92-155`) | exact |
| `Sources/editor/core/history.zig` (MOD: record command variants) | model | event-driven (undo stack) | itself (`:11-36`) | exact |
| `Sources/editor/core/editor.zig` (MOD: record edits, replay, generations, cascade) | service | CRUD + undo | itself (`:394-465`, `:477-526`, `:549-574`) | exact |
| `Sources/editor/core/fake_bridge.zig` (MOD: collections, cascade, references) | test double | CRUD | itself (`:41-66`, `:199-223`, `:333-372`, `:406-465`) | exact |
| `Sources/editor/core/tools.zig` (MOD: `Event`/`Key` extension) | utility | event-driven | itself (`:16-18`) | exact |
| `Sources/editor/core/files.zig` (MOD: `Files.list`, only if 04-05 keeps the .lua list) | utility | file-I/O | itself (`:21-66` vtable, `:76-160` `StdFiles`, `:180-330` `FakeFiles`) | exact |
| `Sources/editor/core/root.zig` (MOD: import every new core file) | config | - | itself (`:1-16`) | exact |
| `Sources/editor/app/markers.zig` (NEW) | component | streaming (per-frame draw) | `Sources/editor/app/view.zig:456-483` (`drawSoundMarkers`) | role-match |
| `Sources/editor/app/tool_registry.zig` (NEW) | provider | event-driven | `Sources/editor/app/view.zig:21`, `:562-588` (`Tool`, `switchTool`, `dispatch`) | role-match |
| `Sources/editor/app/commands.zig` (NEW) | utility | request-response | `Sources/editor/app/panels.zig:2138-2151` (`addSoundAtViewCentre`, public for smoke) | role-match |
| `Sources/editor/app/view.zig` / `view_math.zig` (MOD: right button, double click, keys) | component | event-driven | themselves (`view.zig:285-350`, `:416-432`; `view_math.zig:409-473`) | exact |
| `Sources/editor/app/auto.zig` (MOD: `rclick=`, `dblclick=`, `tool=`, `do=`, key `INSERT`) | utility | event-driven (parser) | itself (`:93-223`, tests `:379-450`) | exact |
| `Sources/editor/app/smoke.zig` (MOD: `AutoRunner` handlers) | utility | event-driven | itself (`:1585-1616`, `:1792-1803`, `:1829-1893`) | exact |
| `Sources/editor/app/panels_logic.zig` (MOD: `isPlaceable`) | utility | transform | itself (`:81-86`, test `:1200-1204`) | exact |
| `Sources/editor/app/c_bridge.zig` (MOD: adapters) | provider | request-response | itself (`:66-90`, `:225-295`) | exact |
| `tools/zig/map_file_test.cpp` (MOD: overlay + cascade cases) | test | CRUD | itself (`:147-249`) | exact |
| `tools/zig/editor_bridge_test.cpp` (MOD: `TestM2*`) | test | request-response | itself (`TestSoundList :3301-3490`, `TestDeleteIsRefusedWhileReferred :2275-2302`) | exact |
| `Sources/src/GameTT/iMissionInternal.cpp`, `AILogic/AILogicInternal.cpp`, `AILogic/Scripts/Scripts.cpp`, `AILogic/GeneralInternal.cpp` (MOD: `BK_MAP_TRACE`) | utility (env-gated trace) | event-driven (stderr) | `Sources/src/Scene/SoundScene.cpp:12-19`, `Sources/src/AILogic/Scripts/Scripts.cpp:726-728` | exact |
| `Sources/editor/app/testlaunch.zig` (MOD: `BK_MAP_TRACE` parser) | utility | transform (log parse) | itself (`mapSoundTrace :133-170`, tests `:398-440`) | exact |
| `build.zig` (MOD: file lists, M2 scenario step) | config | - | itself (`:3765`, `:3806-3809`, `:6030-6112`) | exact |

### 04-02 Roads and rivers

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/EditorBridge/session_vso.cpp` (NEW) | service | CRUD + engine sync + derive | `session.cpp:1202-1353` (`PaintIntoSession`/`PutRegionBack`/undo by stored record) and `session.cpp:724-828` (`PlaceObjectInSession` rollback) | role-match |
| `Sources/editor/core/tools_vso.zig` (NEW) | controller (tool) | event-driven (drag gesture) | `Sources/editor/core/tools.zig:97-150` (`Selector`) and `:25-82` (`Brush`) | role-match |
| `bridge.h`/`bridge.cpp`/`bridge.zig`/`c_bridge.zig`/`fake_bridge.zig`/`history.zig`/`editor.zig` additions | see 04-01 sound chain | | sound chain | exact |
| `tools/zig/*_test.cpp` VSO cases | test | CRUD | `TestPaintUndoIsExact :2412-2466`, `TestSoundList` | role-match |

### 04-03 Bridges and fences, 04-04 Entrenchments

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/EditorBridge/session_groups.cpp` (NEW) | service | CRUD, compound edits with tombstones | `session.cpp:830-965` (`DeleteObjectFromSession`/`RestoreObjectInSession`), `session.cpp:648-722` (`AddObjectToSession`), `session.cpp:146-178` (`BuildBridges`) | role-match |
| `Sources/editor/core/tools_groups.zig` (NEW: bridge, fence, trench tools) | controller (tool) | event-driven | `tools.zig:25-150` | role-match |
| `session.cpp` `ObjectAt` change or new `BkEditorPickGroup` | service | request-response | `session.cpp:967-1010` | exact |

### 04-05 Script IDs, groups, script file, areas, camera anchors

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `session_records.cpp` (groups, areas, anchors, script file) | service | CRUD | `session.cpp:506-646` | exact |
| `Sources/editor/app/panels_m2.zig` (NEW: Groups, Areas, Script, Camera panels) | component | CRUD (commit-on-deactivate) | `Sources/editor/app/panels.zig:1978-2151` (`drawSounds`, `commitSoundEdit`) | exact |
| `panels.zig` (MOD: Properties Script ID field, State fields, `draw`) | component | CRUD | `panels.zig:248-272`, `:407-422`, `:515-522`, `:564-605` | exact |
| `Sources/editor/app/testlaunch.zig` / `bridge.cpp` `BkEditorTestMapPath` (script copy beside test map) | utility | file-I/O | `bridge.cpp:1944-1993`, `core/files.zig` `copy` | role-match |

### 04-06 Start commands and reserve positions, 04-07 AI general

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `session_records.cpp` (start cmds, reserve, AI side) | service | CRUD (`std::list` index by position, keyed side vector) | `session.cpp:506-646` | exact |
| `Sources/editor/core/tools_ai.zig` (NEW: AI general, reserve, areas tools) | controller (tool) | event-driven | `tools.zig:97-150` | role-match |
| `panels_m2.zig` (Start Commands, Reserve, AI General panels) | component | CRUD | `panels.zig:1978-2128` | exact |

### 04-08 Integration and exit

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/editor/app/main.zig` (MOD: M2 `--game-reads-it` mode) | controller | request-response + log parse | itself (`gameReadsIt :535-790`, `.extra_env :721`) | exact |
| `Sources/editor/app/c_bridge_test.zig` (MOD: M2 round trip) | test | CRUD | itself (`:119-223`) | exact |
| `build.zig` `map-editor-auto-m2` step | config | - | `build.zig:6030-6075` | exact |
| `tools/zig/fixtures/m2_script.lua` (NEW) | test fixture | - | none (Lua 4 dialect, see RESEARCH Wave 0) | no analog |
| M2 sweep (`tools/zig/map_file_test.cpp` + `--m2-sweep` in `editor_bridge_test.cpp`) | test | batch | `map_file_test.cpp` round-trip tests; `test-map-files-all` | role-match |

---

## Pattern Assignments

### 1. Overlay layer: `MapRecords.{h,cpp}` and `MapOverlay.{h,cpp}` (utility, CRUD)

**Analog:** `Sources/src/MapFile/MapOverlay.h` and `MapOverlay.cpp`

**Shape to copy** (`MapOverlay.h:7-57`): everything lives in `namespace NMapOverlay`, takes `SLoadMapInfo *pMap`, needs neither the engine nor the object database, returns `bool`, fills an out-struct for undo (`SDeletedObject`) and never renumbers.

```cpp
// MapOverlay.h:42-56
struct SDeletedObject { SMapObjectInfo object; bool bScenario; size_t nIndex; ... };
bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal, SDeletedObject *pDeleted = 0 );
bool RestoreObject( SLoadMapInfo *pMap, const SDeletedObject &rDeleted );
```

**Insert/erase with index kept for undo** (`MapOverlay.cpp:196-215`): record the index, `erase`, and on restore `insert( begin() + Min( nIndex, size() ), record )`. Copy exactly this for `startCommandsList` / `reservePositionsList` (they are `std::list`, so use an iterator advance; keep `Min`, never `std::min`), `entrenchments`, `bridges`, `scriptAreas`, roads, rivers. `reinforcements.groups` is an `unordered_map`, keyed by ID with no index (RESEARCH Pattern 1, Plan notes 04-05).

**Untouched stays byte-identical** (D-01): edits mutate the vector in place, so never rebuild a collection. `MapOverlay.cpp:167-170` (`MoveObject` changes three fields "and the record is otherwise the snapshot's") is the model.

**`SAddObject` extension** (Pitfall 5): add `nFrameIndex`, `fHP`, `nScriptID` next to the existing fields at `MapOverlay.h:13-22`, and drop the hard-coded `object.nScriptID = -1; object.fHP = 1.0f; object.nFrameIndex = 0;` at `MapOverlay.cpp:137-143` in favour of the fields (default keeps today's values).

**`FindReferences` fix** (`MapOverlay.cpp:81-126`). Current bug and the change:

```cpp
// MapOverlay.cpp:106-113 (BUG: ids[] are SCRIPT IDs, not link IDs)
for ( ...groups.begin() ...; ++it )
	for ( size_t j = 0; j < it->second.ids.size(); ++j )
		if ( it->second.ids[j] == nLinkID ) { pReferences->push_back( Numbered( "reinforcement group", it->first ) ); break; }
```

Change: match reinforcement groups against the object's `nScriptID` (only when it is `>= 0`), add loops for `entrenchments[i][s][k]`, `reservePositionsList` (`nArtilleryLinkID`, `nTruckLinkID`) and `aiGeneralMapInfo.sidesInfo[side].mobileScriptIDs`, and **ignore link ID 0** everywhere (C11: 0 is `RMGC_INVALID_LINK_ID_VALUE`, hundreds of shipped objects carry it). Keep `Numbered( "what", index )` for the status-bar strings. Keep the `nLinkWith` passenger loop (`:115-125`).

**Cascade delete result:** extend `DeleteObject` (`:173-206`) so the reference edits are applied and returned, not refused. Its refusal branch (`:177-188`, "still referred to by ...") stays only for a bridge span, an entrenchment piece and a passenger. Store every removed or edited record with its list index so restore can re-insert in reverse order (Pattern 3).

**Tests:** `tools/zig/map_file_test.cpp` `TestObjectOverlay :147-212` (add/move/refuse) and `TestDeleteWithNoReferences :216-249`. Note `:191-211` currently asserts a referenced object refuses to go: update it to the new cascade for start-command references and keep it for a span.

---

### 2. `MapGeometry.{h,cpp}` (utility, pure transform): partial analog only

No existing file has this role. Copy the *style* of `MapOverlay.cpp` (namespace, `Min`/`Max`, no engine includes) and take the algorithms and constants from these tracked sources:

- Constants and conversions, use, do not re-derive: `Sources/src/Formats/fmtTerrain.h:4-7` (`fWorldCellSize = fCellSizeX * FP_SQRT_2`, `fAITileXCoeff = 32.0f * FP_SQRT_2 / 64.0f`), `:29-37` (`Vis2AI` truncates with `int( x + 0.3f )`), `:54-60` (`FitVisOrigin2AIGrid`).
- Bridge span plan, `Sources/src/MapEditor/RoadDrawState.cpp:71-104` (`GetPointsForBridge`); the plan pseudo-code is in RESEARCH "Code Examples". Excerpt:

```cpp
float fLength = pStats->GetSpanStats(pStats->states[0].lines[0]).fLength * fWorldCellSize / 2.0f;
if ( pStats->direction == SBridgeRPGStats::EDirection::HORIZONTAL )
{
	if ( begin.x > end.x ) std::swap( begin, end );
	CVec3 vBeginPoint( begin.x, begin.y, 0.0f );
	FitVisOrigin2AIGrid( &vBeginPoint, pStats->GetOrigin( pStats->GetRandomBeginIndex() ) );
	int nParts = ( end.x - begin.x ) / fLength;
	retVal.push_back( CVec2( vBeginPoint.x - 0.1f, vBeginPoint.y ) );
	for ( int nIndex = 0; nIndex < nParts; ++nIndex )
		retVal.push_back( CVec2( vBeginPoint.x + ( nIndex + 0.5f ) * fLength, vBeginPoint.y ) );
	retVal.push_back( CVec2( vBeginPoint.x + nParts * fLength, vBeginPoint.y ) );
}
```

Port rules: take **plain numbers** (span length, direction, per-index origins, index lists), not `SBridgeRPGStats` (the map-file tier has no object database, C5); do not call `GetRandomBeginIndex` (uses `rand()`), take the origin as an input; guard empty `lines`, `nParts == 0`, empty index lists (Pitfall 5, 6); one nudge, in AI units after truncation. Fence run: `RoadDrawState.cpp:122-182` (`a_dirLine`, port unchanged) and `:519-622, 841-898, 1034-1107`. Trench: `RoadDrawState.cpp:247-465, 1135-1216, 1384-1585`; use `hypot` for the two-argument `fabs( a, b )` and keep integer truncation and `CAngle` wrapping (Pitfall 20). Register the new `.cpp` in `build.zig:3765` (MapFile `.files`).

**Verification without goldens** for the trench port: property tests over random polylines (RESEARCH Plan notes 04-04), not reference output.

---

### 3. Session layer: `session_records.cpp` (service, CRUD), for start commands, reserve positions, areas, groups, anchors, AI general, script file

**Analog:** `Sources/src/EditorBridge/session.cpp:506-646` (sound list: Read, Validate, Add, Set, Delete), declared in `session.h:235-251`.

**Read: two-pass sizing, always the total** (`session.cpp:506-528`):

```cpp
bool ReadSessionSounds( SEditorSession *pSession, BkEditorSoundRecord *pOut, int nCapacity, int *pnCount )
{
	const std::vector<SMapSoundInfo> &rSounds = pSession->snapshot.sounds.sounds;
	const int nTotal = int( rSounds.size() );
	*pnCount = nTotal;
	const int nWrite = nTotal < nCapacity ? nTotal : nCapacity;
	for ( int i = 0; i < nWrite; ++i ) { ... memset( &rRecord, 0, sizeof rRecord ); strncpy( rRecord.name, ..., sizeof rRecord.name - 1 ); ... }
	return nCapacity >= nTotal;
}
```

**Validate once, in an anonymous namespace; sets `szMessage`, returns false** (`session.cpp:530-569`): "the caller-bug checks (null record, bad index, unterminated or non-finite fields) already ran in bridge.cpp". Refusals (unknown name, off map, negative value) are decided here. Reuse `WorldToTile( pSession, x, y, &tx, &ty )` (`session.h:224`) as the on-map oracle.

**Add / Set / Delete update BOTH copies together, engine untouched unless the collection feeds it** (`session.cpp:571-646`):

```cpp
bool AddSoundToSession( SEditorSession *pSession, int nIndex, const BkEditorSoundRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen ) { pSession->szMessage = "no map is open"; return false; }
	SMapSoundInfo info;
	if ( !ValidateSoundRecord( pSession, rRecord, &info ) ) { if ( pbRefused != 0 ) *pbRefused = true; return false; }
	std::vector<SMapSoundInfo> &rSnap = pSession->snapshot.sounds.sounds;
	std::vector<SMapSoundInfo> &rWork = pSession->working.sounds.sounds;
	const int nCount = int( rSnap.size() );
	const int nAt = nIndex < 0 ? nCount : nIndex;
	if ( nAt < 0 || nAt > nCount || nAt > int( rWork.size() ) ) { pSession->szMessage = "that index is out of range"; if ( pbRefused != 0 ) *pbRefused = true; return false; }
	rSnap.insert( rSnap.begin() + nAt, info );
	rWork.insert( rWork.begin() + nAt, info );
	return true;
}
```

Copy the `(bool, bool *pbRefused)` return contract verbatim: `false` + `*pbRefused = true` becomes `BK_EDITOR_REFUSED`, `false` alone becomes `BK_EDITOR_FAILED`.

**Per-collection adjustments:**
- `startCommandsList`, `reservePositionsList` are `std::list`: replace `insert( begin() + nAt )` with `std::advance`, keep the index range rule.
- Script areas: store **AI units**, convert Vis to AI once with the MFC rule (`int( x + 0.3f )`, radius through x) at add/edit; names non-empty and unique, case-sensitive (Pitfall 14).
- Camera anchors: pad `playersCameraAnchors` to `Max( size, N + 1 )` with `VNULL3`, never shrink, never resize on open (C8, Pitfall 15).
- AI general: creating a missing side creates every lower one, and the edit returns the whole side so undo can restore the vector size (Pitfall 11).
- Script file: bare name only, `[A-Za-z0-9_.-]`, no separators, no `..`, no `.lua`; a value read from a file is kept verbatim until changed (Pitfall 12).
- Reserve position: role and towing checks need the object database, so they stay here (C4); refuse link ID <= 0 and both-0.

Register the file in `build.zig:3806-3809` (EditorBridge `.files`).

---

### 4. C ABI: `bridge.h` and `bridge.cpp` (controller, request-response)

**Analog:** `bridge.h:634-712` (sound record, doc comments, four entry points) and `bridge.cpp:1795-1890`.

**Record struct: fixed arrays, ints for bools, floats for positions** (`bridge.h:666-673`):

```c
typedef struct { char name[64]; float x, y, z; int repeat_ms, repeat_random_ms; int mute_in_combat; int min_radius, max_radius; } BkEditorSoundRecord;
```

**Doc-comment habit:** every entry point states the unit (map vs world), the index sentinel (`-1` appends, `0..count` inserts), which case is `BK_EDITOR_BAD_ARGUMENT` (caller bug) vs `BK_EDITOR_REFUSED` (ordinary no, with `BkEditorLastMessage`), and "a refusal changes nothing: not the snapshot, not the working copy". Copy that wording (`bridge.h:681-712`).

**Read entry point** (`bridge.cpp:1795-1810`): zero `*pnCount` first, `Guarded`, check `pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 )` as BAD_ARGUMENT, then `bMapOpen`, then the session reader.

**Guarded** (`bridge.cpp:130-148`), every entry point:

```cpp
template<class F>
BkEditorStatus Guarded( BkEditorSession *pSession, F body )
{
	if ( pSession == 0 ) return BK_EDITOR_NO_SESSION;
	try { pSession->szMessage.clear(); return body(); }
	catch ( ... ) { pSession->szMessage = "the engine threw"; return BK_EDITOR_FAILED; }
}
```

**Add entry point with WellFormed + range + refusal mapping** (`bridge.cpp:1812-1850`): `SoundRecordWellFormed` (null, unterminated name via `strnlen`, `std::isfinite`) in an anonymous namespace, then `bMapOpen`, then the index range against the snapshot size, then `if ( XToSession(...) ) return BK_EDITOR_OK; return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;`. Guard with `if`s, never asserts.

**Compound edits (cascade delete, bridge draw/rotate/delete, trench):** clone `BkEditorDeleteObject` / `BkEditorRestoreObject` (`bridge.cpp:686-716`): one call, `bMapOpen` check, `bool bRefused`, session function. The cascade `BkEditorDeleteObject` must additionally report what it changed (message plus a summary count) for the status bar.

**Test-launch script copy** (D-20): `BkEditorTestMapPath` (`bridge.cpp:1944-1993`) already validates a bare `.bzm` name (`IsBareTestMapName`), builds `szDir` = generated-data `maps` folder and removes a stale `.xml` sibling (`:1979-1989`). Add the script copy in the app after this call returns (destination = `szDir`), validating the script basename the same way; do not accept a user-typed path.

---

### 5. Compound edits with tombstones: `session_groups.cpp` (service, CRUD + engine)

**Analogs:** `DeleteObjectFromSession` (`session.cpp:830-915`), `RestoreObjectInSession` (`:917-965`), `AddObjectToSession` (`:648-722`), `SEditorSession::STombstone` and `nLinkIDFloor` (`session.h:54-67`), and the paint token stack (`session.h:43-53`, `session.cpp:1202-1353`).

**Tombstone-keyed undo** (`session.h:54-67`, `session.cpp:911-912`):

```cpp
struct STombstone { NMapOverlay::SDeletedObject snapshot, working; bool bPlaced; ... };
std::unordered_map<int, STombstone> tombstones;
int nLinkIDFloor;   // "so an add never takes the ID of an object a later undo will restore"
// ... end of DeleteObjectFromSession:
pSession->tombstones[nLinkID] = tombstone;
pSession->nLinkIDFloor = Max( pSession->nLinkIDFloor, nLinkID + 1 );
UpdateSessionWorld( pSession );
```

**Order of work in a delete** (`session.cpp:836-913`): (1) refuse unknown-type object, (2) `RefuseSharedLinkID`, (3) map decides via `NMapOverlay::DeleteObject` on the snapshot, then the working copy, (4) only then the engine: `ReleaseLink( pAIObject )` **before** `DeleteObject` so a restore does not hit "Repeated link", squads go through `GetUnitsInFormation`, (5) `byLinkID.erase`, (6) store tombstone, (7) `UpdateSessionWorld`. Copy this order for every compound edit; the engine is the last thing touched.

**Restore rebuilds the engine object from the WORKING record** (`session.cpp:943-961`), never the snapshot (packed frame index). If the engine refuses, roll both copies back by deleting the record again and report `bRefused`.

**Draw-time frame indices** (Pitfall 5, `session.cpp:688-703`): snapshot gets the packed type via `CMapInfo::PackFrameIndex( pObjectsDB, pAdded )` (`:692-693`); working copy and engine get the seeded unpacked index. Bridge, fence and trench objects need the same `SAddObject` extension as section 1.

**Mandatory ordering (Pattern 3, Pitfall 7):** undo of a draw erases the `bridges[i]`/`entrenchments[i]` entry first, then deletes spans/pieces (a span named by `bridges` refuses delete); redo restores spans by link ID first, then inserts the entry. The tombstone remembers the entry's list index.

**Placing spans/pieces** reuse `PlaceOneObject` (`session.cpp:72-94`) and `IsObjectInsideOfMap` for the all-or-nothing draw refusal; the M1 rollback template is `PlaceObjectInSession :724-828` ("The engine goes back as well as the map").

**Group pick** (`ObjectAt`, `session.cpp:967-1010`): the skip is at `:993`:

```cpp
const EObjGameType eType = pMapObject->pDesc->eGameType;
if ( eType == SGVOGT_BRIDGE || eType == SGVOGT_ENTRENCHMENT ) continue;
```

Do not change `BkEditorObjectAt`'s meaning (M1 Select tool); add `BkEditorPickGroup( sx, sy )` returning kind and index and reusing the `linkByAI` lookup at `:995-1004`.

**Bridge "built during play" mark** (C1): `futureBuildLinkIDs` is only filled (`session.cpp:165-166`) and cleared (`:219`, `:314`); nothing draws it. Implement the mark (`IVisObj::SetSpecular( 0xFF0000FF )`) after `UpdateSessionWorld`, and re-apply on undo, redo, rotate and reopen.

**Tests:** `editor_bridge_test.cpp` `TestDeleteRestoreKeepsTheObject :2733-2801`, `TestSquadDeletesAndRestores :2803`, `TestBridgeSpansAreBuilt :2304-2323`, `TestDeleteIsRefusedWhileReferred :2275-2302` (keep for spans; copy its "save and compare to `NMapFile::Read( BRIDGE_MAP )` with `AreEquivalent`" tail).

---

### 6. Roads and rivers derive-then-store: `session_vso.cpp` (service, CRUD + engine sync)

**Analog:** paints. `PaintIntoSession` (`session.cpp:1202-1295`) is the only existing "engine keeps its own terrain copy, so update both copies and the engine, raw undo by stored record" code.

Patterns to copy:
- **Capture the engine's state before the change so a mid-way refusal can put it back raw** (`:1250-1257`, `engineBefore`, then `RestoreRegion` at `:1265` and `:1275`).
- **Apply the same deterministic function to snapshot, then working, undoing the snapshot if the second fails** (`:1259-1278`).
- **Undo and redo put the recorded record back raw, never re-run the function** (`PutRegionBack :1301-1316`, `UndoPaintInSession :1319-1335`). This is D-03 in one file: the road/river edit call returns the full derived record, the command stores it as `after`, undo/redo call the raw put.
- **A `false` from the engine is not an answer**: read the engine back and compare (`session.h:143-147`; `TerrainMatchesEngine :1355`). Add `BkEditorVsoMatchesEngine` beside it and compare by the saved-ID-to-engine-ID map (Pitfall 3), never by `nID`.
- Remove+Add per edit, river AI order `DeleteRiver( before )`, engine Remove+Add, `AddRiver( after )` on every change including undo/redo (Pitfall 4).
- After `CVSOBuilder::CreateVSO`, require `controlpoints.size() >= 2`; after `Update`, `points.size() >= 2` (Pitfall 1); apply the read-time `fPassability = 1` fix to new roads (Pitfall 2).

Recipe (RESEARCH Pattern 2, from `VectorStripeObjectsState.cpp:644-657`; signatures in `Sources/src/RandomMapGen/VSO_Types.h:71-86`):

```cpp
SVectorStripeObject vso;
CVSOBuilder::CreateVSO( &vso, szDescName, controlPoints );
CVSOBuilder::Update( &vso, false, CVSOBuilder::DEFAULT_STEP, fWidth * fWorldCellSize / 2.0f, fOpacity );
CVSOBuilder::UpdateZ( altitudes, &vso );
// saved nID: max + 1 over the saved list, never the engine's rand()
```

Register in `build.zig:3806-3809`. `RandomMapGen/VSO_*.cpp` is already compiled for MapFile (`build.zig:604-605`).

---

### 7. Core commands: `history.zig`, `editor.zig`, `bridge.zig`, `records.zig`

**`history.zig` Command union** (`:11-36`): add variants next to `sound_add/edit/delete`. Each carries the whole before-record and after-record, plus an index or ID:

```zig
sound_add: struct { index: usize, record: SoundRecord },
sound_edit: struct { index: usize, before: SoundRecord, after: SoundRecord },
sound_delete: struct { index: usize, record: SoundRecord },
```

For variable-length records (roads, rivers, trench pieces, groups) the command owns memory: follow `paint: struct { tokens: std.ArrayListUnmanaged(i32) }` and extend `Command.deinit` (`:30-35`) so the history frees it (`History.clear`, `dropTop`, `dropRedoBranch` all call `entry.command.deinit`). Alternative to keep the core allocation-free: keep the derived record in the bridge and store a handle, as paints store tokens.

**`editor.zig` add/edit/delete with the reserve-then-commit discipline** (`:418-465`):

```zig
pub fn addSound(self: *Editor, index: i32, record: SoundRecord) EditError!void {
    try self.history.reserve(self.allocator);                        // room first: recording can never fail after the bridge committed
    try self.noteOutcome(self.bridge.addSound(index, record));       // refusal -> Refused, message into the status bar
    ...
    self.history.recordAssumeCapacity(self.allocator, .{ .sound_add = .{ .index = actual, .record = record } }, 0);
    self.sounds_generation +%= 1;
}
```

**Edit with gesture merge and drop-if-back-at-start** (`editSound :434-453`, same shape as `place :332-354`): read `before`, `if (std.meta.eql(before, record)) return;`, `mergeable( gesture, .tag )`, reserve only when not merging, call the bridge, then either `dropTop` (edit returned to the entry's own `before`) or update `after` + `touchTop`, else `recordAssumeCapacity`. `mergeable` is `:287-292`; `beginGesture` is `:264-269`. Use one gesture key per drag for control points, width handles, radius, parcel handles.

**Delete reads the record first so undo can re-add it exactly there** (`deleteSound :459-465`).

**Replay (undo/redo)** (`:477-526`): each new variant gets a case, `forwards` picks `after`/`before`, always through the same bridge call, bump the matching `*_generation`:

```zig
.sound_edit => |e| {
    const value = if (forwards) e.after else e.before;
    try self.noteOutcome(self.bridge.setSound(@intCast(e.index), value));
    self.sounds_generation +%= 1;
},
```

`undo`/`redo` (`:549-574`) reserve the opposite stack and `document.objects` capacity before replay; a `Refused` during replay is `drifted` to `Failed`. If a new command can add document objects (bridge draw, trench), `restoreInto`/`removeFrom` (`:528-541`) are the model.

**Generation counters:** one `u32` per collection next to `sounds_generation` (`:30-35`), bumped in every edit and every replay case. Panels compare against `*_generation_seen`.

**Cascade delete in the core:** `delete` (`:356-363`) stays one command; the bridge does the compound work and the record becomes a handle (link ID), like `.delete`. After the bridge call, refresh the affected collections (bump their generations).

**`bridge.zig`:** add record structs next to `SoundRecord` (`:67-90`: fixed `name: [name_capacity]u8`, `nameSlice`, `setName`), VTable entries (`:96-130`, doc comment per entry, index sentinel documented), and a one-line wrapper per entry (`:132-154`, single-line `pub fn x(self: Bridge, ...) Status { return self.vtable.x(self.ptr, ...); }`). Keep `Status`, `EditError`, `check` as they are (`:8-28`). The core stays std-only.

**Import rule (Pitfall 18):** `core/root.zig` uses `refAllDecls`; add every new core file there (`pub const records = @import("records.zig");` ...) or its tests never run.

**Tests to copy** (`editor.zig:928-998`): "add, undo, redo" (asserts fake list and generation before/after), "edit, undo", "delete then undo restores same record at same index", "a refused add leaves history and generation unchanged" (checks `undo_stack.items.len` and the generation). Add the cascade-delete test next to `:916-926` (which asserts the old refusal message `"still referred to by bridge 0"`, fake at `fake_bridge.zig:346`).

---

### 8. Fake bridge: `fake_bridge.zig` (test double, CRUD)

**Analog:** itself. New collection = one `ArrayListUnmanaged` field, one seed fixture, one `CallKind`, vtable entries, validation mirroring the C++ rule.

```zig
// fake_bridge.zig:41
pub const CallKind = enum { open, save, add, place, delete, restore, diplomacy, map_type, attacking_side, paint, undo_paint, redo_paint, sound_add, sound_edit, sound_delete };
// :57  sounds_list: std.ArrayListUnmanaged(SoundRecord) = .empty,   (kept across a fake reopen)
// :127 pub fn addSoundFixture(self: *FakeBridge, new_record: SoundRecord) !void { ... }
```

**Two-pass read** (`:406-412`, returns `.ok` if `out.len >= total`, else `.refused`), **validate returning `Status` with `self.say(...)`** (`:421-432`), **add/set/delete** (`:434-465`: `message_len = 0`, `bad_argument` for a caller-bug index, `refused` with a message for rules, `orderedRemove`, `self.record( kind, idx )`). Copy the "knowingly kinder or simpler" header doc (`:1-25`) and extend it when the fake models start-command and reserve references.

**Cascade in the fake:** replace the boolean `referenced` set (`:58`, `:118-123`, refusal at `:345-348`) with modelled start-command / reserve lists so the core's cascade test restores every reference on undo. `deleteObject` at `:333-354` keeps the unknown-object refusal (`:340-343`), `shared()` refusal, tombstone + `link_floor`; `restoreObject :356-372` re-inserts at `min( index, len )`.

**Every new `Bridge.VTable` field must appear in the fake vtable (`:199-223`) and the real one (`c_bridge.zig:66-90`) or Zig fails to compile**; that is the intended safety net.

---

### 9. App adapter: `c_bridge.zig` (provider, request-response)

**Analog:** `c_bridge.zig:225-295`.

- Conversions both ways with `std.mem.zeroes` for the C struct and `@memcpy` truncated to `out.name.len - 1` (`toSoundRecord :227-240`, `toCSoundRecord :244-258`); bool <-> int as `!= 0` / `if (b) 1 else 0`.
- **Two-pass read** (`vtableSounds :262-279`): sizing call with `null, 0`; accept `.ok` or `.refused`; `page_allocator.alloc( c.BkEditorXRecord, total )` (not the C allocator: no libc on Windows) with `defer free`; second call; `got != count` is `.failed`.
- Add/set/delete wrappers are one-liners over `status( c.BkEditorX( self.session, ... ) )` (`:281-295`).
- Layout guard: add `comptime { std.debug.assert(@sizeOf(...) == ...) }` entries next to `:18-26` for any `extern struct` handed straight to C (as `PaintCell` is). `BkEditorObjectRecord` needs a `script_id` field appended (Pitfall 9); update `toRecord :148-160` and the core `ObjectRecord` (`bridge.zig:42-50`).

**Engine tier Zig** (`c_bridge_test.zig`): `expectEngineMatches` (`:35-41`) after every step and `expectDocumentIsBridge` (`:56-75`, reads the raw C records, not through `RealBridge`) are the pattern; the main test (`:119-223`) ends with `while (try editor.undo()) {}` then asserts `!editor.dirty()` and engine/document equality. Add each M2 command with do / undo / redo / undo-all, and the `map-editor-engine: PASS` print.

---

### 10. Tools: `tools_vso.zig`, `tools_groups.zig`, `tools_ai.zig` (controller, event-driven)

**Analog:** `Sources/editor/core/tools.zig`.

**Event/Key extension point** (`:16-18`):

```zig
pub const Key = enum { delete, rotate_left, rotate_right };
pub const Event = union(enum) { press: Pointer, drag: Pointer, release: Pointer, key: Key };
```

04-01 adds `right_press/right_drag/right_release`, `double_click`, and keys `enter`, `insert`, `escape`, `space` (C13). Every existing `switch` over `Event` must handle the new variants (`Brush.handle :37-51` uses an explicit `.key => {}`; `Placer` and `Selector` use `else`/exhaustive).

**Tool struct shape:** `handle( self, editor: *Editor, event: Event ) EditError!void`, state = only what a gesture needs between events (`gesture: u32`, grab offset), everything else in the `Editor`. Gesture start: `self.gesture = editor.beginGesture();` on press, `0` on release (`Brush :39-48`, `Selector :104-114, 131`).

**Refused mid-drag is skipped, not the end of the drag** (`Selector :126-129`):

```zig
editor.place(link_id, pose, self.gesture) catch |err| if (err != error.Refused) return err;
```

Copy for control-point / width / radius drags. A press that resolves to nothing starts no gesture (`:105-109`). `Pointer` carries `world_x/y` (scene units) AND `map_x/y` (file units, sqrt 2 apart, `:12-16`): tools that store positions must use `map_*` (the tests at `:232-254` exist because this was once wrong); road/river control points are stored in the units their record needs (Vis for VSO, AI for areas and commands) and the conversion belongs in the bridge.

**Tests:** copy `tools.zig:152-328` (`opened( &fake )`, `at( &editor, x, y )` via `editor.resolve`, drive events, assert on `fake` and `editor.document`, `_ = try editor.undo()`, and "one undo step per gesture" by counting `fake.calls`).

Import each tool file from `core/root.zig`.

---

### 11. Input layer: `view.zig`, `view_math.zig` (component, event-driven)

**Analog:** the same files.

- `view_math.kindOf` is the choke point: `:429-435` returns `null` for every button but left. Extend `ButtonEvent`/`EventKind` for right (`sdl_button_right = 3` already declared `:413`) and add `sdl_button_rmask`. Copy the `staleGesture` shape (`:445-452`) and its three tests (`:454-473`) for a second held button.
- `view.zig handleEvent :285-324`: press resolves the pointer, sets `self.left_button_down`, dispatches `.press`; release falls back to `self.hover` when the pointer cannot resolve so a gesture always ends. Add a parallel `right_button_down` and include it in `hasActiveMouseGesture :153-155` and `update`'s stale check (`:636`). Double click: `button.clicks == 2` (the single click already arrived, Pitfall 13).
- Ctrl+left as right-click only in tools that declare it (RESEARCH C13); `Input.modState()` (`:46`) is the reading point (`handleWheel :359` uses it).
- `handleMotion :335-350` dispatches `.drag` only while `motion.state & SDL_BUTTON_LMASK != 0`; add the right mask.
- Keys `handleKey :416-432`: existing (Delete/Backspace, Q, E, 1, 2, 3, Ctrl/Cmd+Z, Y, Home) plus camera keys W/A/S/D/arrows (`:651-656`). Copy the `if (!key.repeat)` guard for new tool keys; pick unused keys (4-9, 0, other letters).
- `Tool = enum { select, brush, place }` (`:21`) and the `switch` in `dispatch :581-588` become the registry (`tool_registry.zig`); `switchTool :562-569` must end an open gesture (both buttons) before swapping.
- Errors: `noteEditResult :182-185` and `noteToolError :593-599` (Refused clears the view part because the editor status already has the words). Keep this unchanged for every new tool.
- Update `smoke.zig`'s and `view.zig`'s own `FakeCamera` rig tests when `handleEvent`'s signature or state fields change (`view.zig:742-810`, tests near `:1234`).

---

### 12. Marker layer: `markers.zig` (component, per-frame draw)

**Analog:** `view.zig:456-483` (`drawSoundMarkers`) and `:493-538` (brush outline).

```zig
const draw_list = imgui.c.igGetBackgroundDrawList();
for (sounds, 0..) |sound, index| {
    const screen = real.worldToScreen(sound.x, sound.y) orelse continue;   // skip per item when the conversion fails
    const selected = selected_sound != null and selected_sound.? == index;
    ...
    imgui.c.ImDrawList_AddQuadFilled(draw_list, top, right, bottom, left, color);
    imgui.c.ImDrawList_AddTextEx(draw_list, .{ .x = ..., .y = ... }, outlineColor(), name.ptr, name.ptr + name.len);
}
```

Rules: colours via `imgui.c.igColorConvertFloat4ToU32` (`:452-459`), background draw list so panels draw over it, `worldToScreen( wx, wy ) orelse continue` (world units in, z = 0 plane, Pitfall 17), guard against ImGui capturing the mouse (`if (Input.capture().mouse) return;` at `:496`) for hover-driven outlines, stack buffers sized for the worst case (`max_outline_points :448`), and cap counts per kind (RESEARCH Security). Draw detail only for the selected road; markers for the active tool are always on, others follow View > Markers. `drawOverlay( self, real, sounds, selected_sound )` (`:493`, called at `panels.zig:569`) becomes `drawOverlay( self, real, markers_state )`.

Units: a marker for an AI-unit record (areas, start-command targets, reserve positions, parcels) needs AI -> Vis (`* fAITileXCoeff`) before `worldToScreen`; sound positions are already world units (`bridge.h:658-665`).

---

### 13. Panels: `panels_m2.zig` and `panels.zig` (component, CRUD with commit-on-deactivate)

**Analog:** `panels.zig:1978-2128` (`drawSounds`, `commitSoundEdit`) and `:2138-2151` (`addSoundAtViewCentre`).

**State fields per collection** (`panels.zig:248-272`): the cached list, `*_generation_seen`, `selected_*: ?usize`, and an `*_edit` struct with `index`, `name_buffer: [core.bridge.name_capacity:0]u8`, the field copies and `active: bool`.

**Cache reload:** `loadSounds :407-422` (free previous, `&.{}`, `mapIsOpen` guard, two-pass sizing through `self.editor.bridge`, discard on any mismatch) called from `mapOpened :515-522` (reset selection, `edit.active = false`, load, take the generation) and from `draw` when `state.sounds_generation_seen != editor.sounds_generation` (`:1983-1986`).

**Panel body pattern** (`:1978-2097`): `beginPanel( "Name", pos, size, cond, null )` + `defer endPanel( open )`; `mapIsOpen` gate; commit a mid-edit field when the selection moved away (`:1994-1995`); one selectable row per record with `ig.igPushIDInt`; separator; buttons; wrapped disabled-colour note text; edit fields loaded into `edit` only when `edit.index != index or !edit.active`; each field followed by

```zig
_ = ig.igInputFloatEx("x", &edit.x, 0, 0, "%.1f", 0);
active = active or ig.igIsItemActive();
committed = committed or ig.igIsItemDeactivatedAfterEdit();
```

then `edit.active = active; if (committed) commitSoundEdit( state, index );`. Combos commit on selection; checkboxes commit on click.

**Commit** (`:2110-2128`): build the record from the edit fields, `state.view.noteEditResult( editor, editor.editSound( index, record, 0 ) )`, reload, re-take the generation. For a drag-like slider that should merge use one gesture id for the whole activation.

**Pure logic goes in `panels_logic.zig`** (unit-conversion helpers `msToSeconds`/`secondsToMs` with the non-finite guard `:97-110`, `soundRadiusError :152`, default name helper `:141`), so `test-map-editor-panels` covers it.

**Public entry for scripted runs:** `pub fn addSoundAtViewCentre( state: *State ) void` (`:2138`) is called by `smoke.zig:738` and is the model for `commands.zig` (a named command that buttons, menu items and `BK_EDITOR_AUTO do=` all call). Register the new panels in `draw` (`:592-597`) and give them layout entries.

**Palette filter (D-05):** `isPlaceable` (`panels_logic.zig:81-86`, `5, 100 => false`) becomes `4, 5, 6, 9, 100 => false`; update its test (`:1200-1204` currently asserts 4, 6, 9 ARE placeable, so it must flip). Mirror in C++ by adding a third function next to `WhyNotPlacedAlone` (`session.h:166`) used only by `catalogue.cpp:113` and `BkEditorAddObject` (not by `WhyNotAMapObject`, which also guards `PlaceOneObject` and would stop loaded spans from being placed). `panels.zig:434-441` already filters on both `logic.isPlaceable` and `entry.placeable`.

---

### 14. `BK_EDITOR_AUTO` verbs: `auto.zig` and `smoke.zig` (utility)

**Analog:** the same two files.

**Parser** (`auto.zig:93-223`): one `Action` union variant per verb; `parseAction` splits `name=value` and chains `if (std.mem.eql(u8, name, "..."))` with a specific `fail( failure, entry, "reason", error.BadX )` message per malformed case (`:175-223`). `Failure` records the offending entry for main.zig to print. Names for files use `validateName` (`[A-Za-z0-9_-]{1,64}`, `:299-305`). New verbs: `rclick=<point>`, `dblclick=<point>`, `tool=<name>`, `do=<name>[:arg]`, `text=<...>`; key `INSERT`. Reuse `parsePoint`.

**Parser tests** (`auto.zig:379-450`): `expectAction( "3:verb=...", .{ ... } )` for every action, a whole-schedule test, and one line per bad token in `expectBad` (entry named, reason non-empty). These run under `test-map-editor-auto` with nothing staged.

**Runner** (`smoke.zig:1585-1616`): `run` switches on the action, prints `BK_EDITOR_AUTO: frame N action <text>`, returns `false` to stop. Button events are hard-wired to left with `clicks = 1` (`pushButton :1792-1803`) and motion `state` to `SDL_BUTTON_LMASK` (`pushMotion :1780-1790`): parameterise on button and click count (a double click sends two down/up pairs, the second with `clicks = 2`, Pitfall 13). Named keys are the `named` table at `smoke.zig:1871-1886`; add `INSERT` with `SDLK_INSERT`/`SDL_SCANCODE_INSERT`. Unknown key names `self.fail( "key={s}: unknown key name", ... )` (`:1619`).

**Outcome judging:** `afterFrame :1566-1583` fails a scripted open/save if the view or editor status line is non-empty; the same rule is what makes `do=` and gesture verbs fail loudly on a refusal.

**Scenario table (`--smoke`)**: `smoke.zig:300-303, 336-339` (`.{ .name, .inputs, .expect }`) and the `Input`/`Expect` enums (`:75-77`, `:181-185`), `prepare` reads (`:509-520`), handlers (`:737-738`, `:1039-1056`). Add M2 rows the same way, and a `map-editor-auto-m2` step in `build.zig` cloned from `:6030-6075` (schedule string via `b.fmt`, `BK_EDITOR_AUTO_DIR`, `BK_EDITOR_AUTO_GAME`, `has_side_effects = true`, `dependOn( &smoke_run.step )`, cleanup step with `delete-matching-files`).

---

### 15. Map-file tier tests: `tools/zig/map_file_test.cpp`

**Analog:** itself.

- Harness: `static int g_nFailures`; `Check( bool, const char * )` prints `FAIL:` and counts (`:10-20`); functions return `void`/`bool`; `main` calls each in turn.
- Read a shipped map, keep `const CMapInfo original = map;`, apply overlay calls, compare with `NMapFile::AreEquivalent( before, after, &szWhere )` and `szWhere.find( "field" )` (`:96-127`, `:191-209`).
- Untouched-collection byte-identity: write, read back, `AreEquivalent`; and compare the bytes of two saves for D-25.4 (`TestWritesWhatItRead :81-94`).
- For M2 the expected map is built with the same functions (MapRecords/MapGeometry) from literal inputs, per the "same function" rule; assert nudge fractions as arithmetic results, not literals (Pitfall 6).
- Shipped data used: `Data\\Maps\\Multiplayer\\coldwinter.bzm` (small, has roads) and `arnheim` (rivers, bridges: `BRIDGE_MAP` in the bridge test).

---

### 16. Engine tier tests: `tools/zig/editor_bridge_test.cpp`

**Analog:** `TestSoundList :3301-3490` for a collection, `TestDeleteIsRefusedWhileReferred :2275-2302` for refusals, `TestEntryPointsBeforeAMap :144-292` for the "every entry point answers a status with no session / no map" table.

Structure to copy from `TestSoundList`:
1. Read the original with `NMapFile::Read`, open it with `BkEditorOpenMap`, compare `BkEditorX` (two-pass, `nCount` first, then `records( nCount > 0 ? nCount : 1 )`) with the file.
2. Add (`-1`), read, set, read, **save and read back** (`BkEditorSaveMap` to `szScratch + "\\x.bzm"`, `NMapFile::Read`, compare fields), delete, count equals original.
3. Refusal block: one `Check( ... == BK_EDITOR_REFUSED, "an unknown name is refused" )` per rule, `BK_EDITOR_BAD_ARGUMENT` for a null record, a past-the-end index; then re-read and assert the list is unchanged.
4. Save to scratch and `AreEquivalent( original, saved, &szWhere )` (preservation), `remove( path )` the scratch file. Artefacts under `zig-out/local-test` only.

Also add every new C entry point to the two tables at `:200-265` (null session -> `BK_EDITOR_NO_SESSION`, no map -> `BK_EDITOR_REFUSED`), keeping the Windows CRT-to-stderr routing already in `main :3492`. Do not add a second engine-linking test executable on Windows (C12).

---

### 17. Game seam: `BK_MAP_TRACE` (utility, env-gated stderr)

**Analog:** `Sources/src/Scene/SoundScene.cpp:12-19` and `Sources/src/AILogic/Scripts/Scripts.cpp:726-728`.

```cpp
// SoundScene.cpp:14-19
static bool IsSoundTraceOn() { static const bool bOn = getenv( "BK_SOUND_TRACE" ) != 0; return bOn; }
// ... usage :1004
fprintf( stderr, "BK_SOUND_TRACE: map sound object=\"%s\" pos=(%.0f,%.0f) instance=%d\n", name, x, y, int( wInstanceID ) );
// Scripts.cpp:726
if ( getenv( "BK_SCRIPT_TRACE" ) ) fprintf( stderr, "BK_SCRIPT_TRACE: group %d has %d entries ...", ... );
```

One line per consumed item, prefix `BK_MAP_TRACE: `, `key=value` (Open Question 4 format). Mirror `CScripts::Trace` (`Scripts.cpp:1748-1762`) to stderr under the same variable. Never print absolute paths.

**Log parser** (`testlaunch.zig:133-170`, `mapSoundTrace`): `std.mem.splitScalar( u8, log, '\n' )`, trim `\r`, `startsWith( line, "BK_..._TRACE: kind " )`, helper `quotedField( line, "object=\"" )` (`:185-190`) and `tracePos` (`:192-203`), tolerance +-1 because the log rounds. Tests inline a multi-line log string (`:398-440`). The game is launched with the extra env pairs the way `main.zig:721` does: `.extra_env = &.{ .{ "BK_AUTO_UI", auto_ui }, .{ "BK_NO_HELP", "1" }, .{ "BK_SOUND_TRACE", "1" }, .{ "BK_AUDIO_NULL", "1" } }`; add `BK_MAP_TRACE`. `AutoRunner.runTest` builds the same pairs (`smoke.zig:1687-1700`).

---

### 18. `Files.list` and script copy (utility, file-I/O)

**Analog:** `core/files.zig`.

Add a vtable entry (`:26-43`) with a doc comment and a one-line wrapper (`:46-66`), a `StdFiles` impl modelled on `isDataRootImpl :103-112` (`self.dir.openDir( self.io, os_dir, .{ .iterate = true } ) catch return ...; defer dir.close( self.io ); var it = dir.iterate(); while ( it.next( self.io ) catch null ) |entry| ...`), and a `FakeFiles` impl over `entries` (`:180-330`, path comparison via `samePath`). `copy` already overwrites (`:22-24`, doc comment) so "ask before overwrite" is the caller's job. `realPath` (`:64`) is the symlink-safe check for the copy destination (Security V12). Test through `FakeFiles` with `op_log` assertions like the existing save tests.

---

## Shared Patterns

### Two copies, one edit (snapshot + working)
**Source:** `session.cpp:585-596` (add), `:622-623` (set), `:643-644` (delete), `:756-768` (move).
**Apply to:** every new session function. The snapshot is what is saved; the working copy is what the engine was built from. `SaveSessionMap` (`session.cpp:346-350`) writes the snapshot only. Update both in the same function or `TerrainMatchesEngine`/`WorldMatchesSession`-style checks and the read-back will disagree.

### Save read-back verification
**Source:** `session.cpp:332-372`. `NMapFile::Write` -> `NMapFile::Read` -> `NMapFile::AreEquivalent( snapshot, readBack, &szWhere )`, failure "the written map reads back different at <field>". New records must survive this (Pitfall 2 fPassability; C8 anchor-size rule; script area units).

### Status vocabulary
**Source:** `bridge.cpp:1845-1848`. `BK_EDITOR_BAD_ARGUMENT` = caller bug (null, bad index, non-finite, unterminated), no message needed; `BK_EDITOR_REFUSED` = ordinary no with `szMessage` set; `BK_EDITOR_FAILED` = something went wrong. Zig side: `bridge.zig:8-28` maps refused to `error.Refused` and everything else to `error.Failed`; `Editor.noteOutcome` (`editor.zig:81-91`) copies the bridge's message into the status bar.

### Refusal leaves history and document unchanged
**Source:** `editor.zig:985-998` test and the reserve-then-bridge-then-record order (`:418-427`). `history.reserve` first, then the bridge call, then `recordAssumeCapacity`; a throw from the bridge returns before anything is recorded.

### Units
**Source:** `bridge.h:566-625`, `fmtTerrain.h:29-37`. Object and record positions cross the ABI in map (AI) units; camera, sound positions and `worldToScreen` in world (scene) units; `BkEditorWorldToMap` is unrounded, the MFC `Vis2AI` truncates with `+0.3`. Document the unit in every new struct field comment.

### Gesture merge
**Source:** `editor.zig:264-292, 434-453`. One `beginGesture()` per press-drag-release (or per slider activation); merge by tag and key; drop the entry when the drag ends at its own `before`.

### Guards not asserts
**Source:** `session.cpp:74-75, 87`, `bridge.cpp:1819-1828`. Engine asserts are compiled out in release and the game loaders dereference (Pitfall 7); every index, size, empty vector and non-finite float is an `if`.

### Tests must be imported to run
**Source:** `core/root.zig:1-16` (`refAllDecls`), RESEARCH Pitfall 18. New core files into `root.zig`; new pure app files into `panels_logic.zig`/`view_math.zig`/`auto.zig` roots or a new `build.zig` step; confirm the new test names appear in the step output.

### Build wiring
**Source:** `build.zig:3765` (MapFile `.files`), `:3806-3809` (EditorBridge `.files`). After any `build.zig` edit: `zig test tools/zig/build_hermeticity_test.zig`; never `zig fmt build.zig`. Test steps: `test-editor-core`, `test-map-editor-view`, `test-map-editor-panels`, `test-map-editor-testlaunch`, `test-map-editor-auto`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto`, `map-editor-game-reads-it` (line numbers in RESEARCH "Test Framework").

### C++ conventions
`Min`/`Max` from `Misc/Tools.h` (see `MapOverlay.cpp:68`, `:213`), Hungarian prefixes (`pSession`, `nIndex`, `rRecord`, `szMessage`, `bRefused`), tabs, `StdAfx.h` first include, `Numbered( "what", index )` for reference names, no `std::min`/`std::max`.

### Zig conventions
Four-space indent, `///` doc comments on every public field and function that explain the *why* and the unit, `std.ArrayListUnmanaged` with an explicit allocator, `errdefer` on staged allocations (`document.zig:31-57`), `defer destroy` before the first fallible `expect` in worker tests.

---

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `Sources/src/MapFile/MapGeometry.{h,cpp}` | utility | transform | No pure-geometry unit exists; the only source is the MFC state machine (`RoadDrawState.cpp`) and `fmtTerrain.h` constants. Use `MapOverlay.cpp`'s namespace and `Min`/`Max` style, port the algorithms with plain-number inputs, verify with property tests (trench) and literal expected values (bridge, fence). |
| `Sources/editor/app/tool_registry.zig` | provider | event-driven | Today's registry is a closed `enum` and a `switch` (`view.zig:21, 581-588`); there is no table-driven registry with per-tool "needs right button / double click" flags to copy. Design from RESEARCH Pattern 5. |
| `Sources/editor/app/commands.zig` | utility | request-response | Only single functions (`addSoundAtViewCentre`) exist; the named-command table shared by panels and `do=` verbs is new. |
| `tools/zig/fixtures/m2_script.lua` | fixture | - | No Lua fixture exists; Lua 4 dialect, `function Init()` calling `GetScriptAreaParams`, `GetNUnitsInScriptGroup`, `Trace` with number arguments only (RESEARCH Wave 0). Shipped scripts under `Data/Maps` are the dialect reference. |
| Bridge "built during play" visual mark | engine visual | - | `futureBuildLinkIDs` is recorded but nothing draws it (C1); `IVisObj::SetSpecular( DWORD )` (`Scene/Scene.h:224`) is the call, applied as in `TemplateEditorFrame1.cpp:1972`. |

---

## Metadata

**Analog search scope:** `Sources/src/MapFile`, `Sources/src/EditorBridge`, `Sources/editor/core`, `Sources/editor/app`, `tools/zig/{editor_bridge_test,map_file_test,data_only_startup}.cpp`, `build.zig`, plus `Sources/src/{Scene/SoundScene.cpp,AILogic/Scripts/Scripts.cpp,Formats/fmtTerrain.h,MapEditor/RoadDrawState.cpp,RandomMapGen/VSO_Types.h}` for game-seam and port sources.
**Files scanned:** about 30 read in whole or in ranges (largest: `session.cpp`, `bridge.cpp`, `editor.zig`, `panels.zig`, `view.zig`, `smoke.zig`, `auto.zig`).
**Tracked-source gate:** all analog paths verified with `git ls-files`; none are gitignored mirrors.
**Pattern extraction date:** 2026-09-30
**Line-number validity:** taken from the working tree as of this run; RESEARCH.md warns to re-check ranges in `session.cpp`, `bridge.h`, `view.zig` and `auto.zig` before each plan starts, since Wave 2 plans share those files and shift line numbers as they land.

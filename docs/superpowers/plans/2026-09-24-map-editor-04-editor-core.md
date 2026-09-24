# Map Editor Plan 4: Editor Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A headless Zig editor core: a document of the open map, every M1 edit as a command with exact undo and redo, the tools that turn pointer and key input into those commands, and a fake bridge that lets all of it run on six CI targets. The C bridge gets the calls the core needs to read a map back, undo exactly, and pick the object under the cursor.

**Architecture:** The core (`Sources/editor/core`) talks to the engine only through a Zig `Bridge` interface: a pointer plus a vtable, with one call for each C entry point in `Sources/src/EditorBridge/bridge.h`. `FakeBridge` implements that interface in memory for the core tier. Plan 5 adds the adapter over the real C ABI. Before the core is built, the bridge gains four things. It can list its objects and map fields. It restores a deleted object with its own link ID, and undoes or redoes a paint through a token, putting back exactly the region it recorded. It draws and picks objects through a `CWorldBase` subclass, as the MFC editor does.

**Tech Stack:** Zig 0.16 (std only), C++17 for the bridge and `MapFile` additions, the engine's Scene, AILogic, Common and GameTT, SDL3 in the engine tier.

**Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (sections "Editor core", "Engine bridge → Picking and camera", "Saving: the snapshot and the overlay", "Terrain edits", "Testing → Test tiers and the M1 CI gate", "Errors → Edit").

**Previous plan:** `docs/superpowers/plans/2026-09-21-map-editor-03-engine-bridge.md`. Its Task 7 records why picking is here, and its measured findings apply unchanged. Read its "Global Constraints" too; they are repeated below where they bite.

## The M1 plan series

1. **Overlay spike** (landed): GFXGPU overlay hook, frame capture, vendored ImGui.
2. **Map files** (landed): `Sources/src/MapFile`, the data-only startup, the comparator, the overlay, the terrain function, the map-file tier.
3. **Engine bridge** (landed): `NMain::InitializeWithWindow`, `Sources/src/EditorBridge`, the engine tier on macOS arm64 and Windows-MSVC.
4. **Editor core** (this plan): the bridge's read-back, exact-undo and picking calls; the Zig document, commands, history, tools, fake bridge; the core tier on all six targets.
5. **Editor app:** the adapter over the C ABI, window, frame loop, ImGui panels, `BK_EDITOR_AUTO`, save, autosave, test-launch, `install-map-editor`, packaging.

## Global Constraints

- All work happens on branch `feat/portable-map-editor` in the worktree `.worktrees/map-editor`. Never commit in the main checkout.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- Never run `zig fmt` on `build.zig` or gate on `zig fmt --check` for it; edit it by hand. **Run `zig test tools/zig/build_hermeticity_test.zig` before pushing any `build.zig` change.**
- Test artifacts go under `zig-out/local-test/`, never `/tmp`.
- Build on macOS with `zig build <step> -Dtarget=aarch64-macos`. Thousands of libc header errors (FP_ZERO, ldiv_t) mean the macOS SDK lookup is broken (`xcode-select`), not `build.zig`.
- The core imports nothing but `std`. No `@cImport`, no engine header, no SDL: it has to build and run on `x86_64-windows-gnu`, where the engine C++ does not compile.
- Zig collections follow the repository's idiom: `std.ArrayListUnmanaged(T) = .empty` with the allocator passed to each call (`Sources/src/GFXGPU/lifetime.zig`). Tests use `std.testing.allocator`, so a leak fails the test.
- No C++ exception crosses the C ABI: every new entry point goes through `Guarded` (`bridge.cpp:37`) and returns a status.
- **An assert is not a guard.** `NI_ASSERT_T` compiles away in release in front of null dereferences, and in a Windows debug build, `NI_ASSERT_T`, `NI_ASSERT_SLOW_T` and the CRT's `assert` all stop the process. The portable builds define neither `_DEBUG` nor `_DO_ASSERT_SLOW`, so macOS never trips them; only Windows-MSVC CI does. Guard with an `if`, and do not add an assert in front of a case the code handles.
- **Engine return values lie.** `CAIEditor::AddNewObject`, `MoveObject` and `TurnObject` end in `return false` whatever happened. Check every engine edit by reading the state back, as `PlaceObjectInSession` does.
- **Cross-module `dynamic_cast` fails on macOS** (Itanium ABI: each dylib has its own typeinfo copy). Plan 3 hit it with `ITerrain` → `ITerrainEditor` and fixed it with an in-module accessor (`ITerrain::GetEditor`). Expect the same wherever a type crosses from GameTT into the bridge.
- The engine tier runs where there is a GPU device: macOS arm64 locally and in CI, Windows-MSVC in CI. The core and map-file tiers run everywhere they build.

---

## Decisions this plan takes

**Undo is exact, and the bridge enforces it.** The spec says a paint's undo "restores exactly those [tiles and crosses], in the copy and in the engine". The bridge is the only place that holds both, so it keeps the record. `BkEditorPaint` returns a token, and `BkEditorUndoPaint` / `BkEditorRedoPaint` take it. The bridge keeps the order itself: newest first for undo, and the most recently undone for redo. A token out of order is refused rather than guessed at. The engine gets the recorded region back raw through a new `ITerrainEditor::RestoreRegion`, never through `ITerrainEditor::Update`. `CTerrain::Update` runs `PreprocessMapSegment` first (`Scene/TerrainEditor.cpp:126`), which could change tiles the copy just restored, and it regenerates crosses the file may never have held.

**A deleted object comes back as itself.** The spec's core test says "delete then undo restores the same object, player and link ID". The bridge keeps each deleted record (both copies, its list and its index) and `BkEditorRestoreObject` puts it back. Undo of an add is a delete, and redo of an add is a restore. So the core never needs to remap an ID: every object keeps its link ID for the whole session.

**Link IDs are never reused within a session.** `NMapOverlay::NextLinkID` looks only at the objects currently in the map. Delete the object with the highest link ID, add a new one, and the new one gets the deleted object's ID. Undo both, and the restore would bring back the wrong object. The session keeps a floor (one above every link ID it has handed out or deleted), and `SAddObject` takes the ID to use.

**Objects are drawn and picked through `CWorldBase` (option A of plan 3, Task 7).** The research (`CWorldBase::Update`, `WorldBase.cpp:546-599`; `CreateMapObject`, `:249-289`) shows that building the visuals yourself (option B) still needs GameTT's `CMO*` classes, or about 3,600 lines of their model-path, season and shadow rules. It saves no dependency and would drift from the game. A `CWorldBase` subclass with an empty `ResetSelection` is what the MFC editor is (`TemplateEditorFrame1.h:75`). Two things are measured first rather than assumed: that GameTT's map-object factory is loaded in the bridge's process, and that the objects GameTT creates survive `CWorldBase`'s `dynamic_cast`s in the bridge's copy of `Common`.

**Unknown objects are read-only.** The spec's preservation invariant writes an unknown-type object back unchanged. `DeleteObjectFromSession` deletes it from the snapshot today, so it is refused from now on.

**Tools take resolved input.** The spec's tools take "world position, object under the cursor, buttons, modifiers, keys". A `Pointer` event carries the world point, its tile and the object under it, already resolved. `Editor.resolve` does the three bridge calls, so the tools never pick and the tests can script them directly.

**The document does not hold terrain.** It holds what panels and undo need: path, map fields, diplomacy and objects. Tiles belong to the bridge, and a paint command holds only its tokens. The dirty flag is derived from the history (the undo depth against the depth at the last save or open), so undoing back to the saved state makes the map clean again.

---

## File Structure

| File | Responsibility |
|---|---|
| `Sources/src/MapFile/MapOverlay.h/.cpp` | `SAddObject::nLinkID`; `SDeletedObject`, `DeleteObject` filling it, `RestoreObject`; `CaptureRegion`. |
| `tools/zig/map_file_test.cpp` | Map-file tier checks for the overlay additions. |
| `Sources/src/Scene/Terrain.h`, `TerrainInternal.h`, `TerrainEditor.cpp` | `ITerrainEditor::RestoreRegion`. |
| `Sources/src/EditorBridge/bridge.h/.cpp` | New entry points: `BkEditorObjects`, `BkEditorDiplomacy`, `BkEditorRestoreObject`, `BkEditorUndoPaint`, `BkEditorRedoPaint`, `BkEditorObjectAt`; `BkEditorPaint` gains a token; the summary gains `map_type` and `attacking_side`. |
| `Sources/src/EditorBridge/session.h/.cpp` | Tombstones, the link-ID floor, paint records, the read-back. |
| `Sources/src/EditorBridge/world.h/.cpp` | `CEditorWorld`, the `CWorldBase` subclass; the AI-object-to-link-ID lookup for picking. |
| `tools/zig/editor_bridge_test.cpp` | Engine-tier checks for all of the above. |
| `Sources/editor/core/root.zig` | The module root: re-exports, and pulls every file's tests in. |
| `Sources/editor/core/bridge.zig` | `Bridge` interface, `Status`, records, `check`. |
| `Sources/editor/core/fake_bridge.zig` | `FakeBridge`: an in-memory map that follows the real bridge's rules. |
| `Sources/editor/core/document.zig` | `Document`: path, map fields, diplomacy, objects, filled from a bridge. |
| `Sources/editor/core/history.zig` | `Command`, `History`: undo, redo, merging, the clean mark. |
| `Sources/editor/core/editor.zig` | `Editor`: open, save, every edit, undo and redo, selection, status line, `resolve`. |
| `Sources/editor/core/tools.zig` | `Brush`, `Placer`, `Selector`, and the input event types. |
| `build.zig` | Step `test-editor-core`, in `zig build test`; Common and the world file in `addEditorBridge`. |
| `.github/workflows/cross-platform.yml` | An "Editor core tier" step in all six jobs. |

---

### Task 1: The bridge reads the map back

The core builds its document from the bridge. Today it can learn only the counts in `BkEditorMapSummary`. This task adds the object list, the map's own fields and the diplomacy table. It also closes the one hole where an edit reaches an unknown object.

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `Sources/src/EditorBridge/bridge.cpp`
- Modify: `Sources/src/EditorBridge/session.h`, `Sources/src/EditorBridge/session.cpp`
- Test: `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `SEditorSession::snapshot`, `unknownLinkIDs` (session.h).
- Produces (bridge.h):
  ```c
  /* BkEditorMapSummary gains two fields, at the end: */
  int map_type;        /* nType */
  int attacking_side;  /* nAttackingSide */

  typedef struct
  {
  	int link_id;
  	char name[64];     /* truncated at 63, always terminated */
  	float x, y;        /* the map's vPos, not the engine's */
  	int dir;           /* nDir as the map holds it */
  	int player;        /* the map's owner, which the engine may not share */
  	int scenario;      /* 1: scenarioObjects, 0: objects */
  	int known;         /* 0: the database does not know the type */
  } BkEditorObjectRecord;
  BkEditorStatus BkEditorObjects( BkEditorSession *session, BkEditorObjectRecord *out, int capacity, int *out_count );
  BkEditorStatus BkEditorDiplomacy( BkEditorSession *session, int player, int *out_value );
  ```
- Produces (session.h): `bool ReadSessionObjects( SEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount );`

- [ ] **Step 1: Write the failing tests**

Add to `tools/zig/editor_bridge_test.cpp`, and call both from `main` after `TestMapsOwnFields`:

```cpp
// The document is built from this, so it has to be the map as read: every
// object in file order, objects before scenario objects, with the map's own
// owner - not the engine's, which is 0 for anything but a building.
static void TestObjectsReadBack( BkEditorSession *pSession )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;

	int nCount = -1;
	Check( BkEditorObjects( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED, "a short buffer is refused" );
	Check( nCount == int( map.objects.size() + map.scenarioObjects.size() ), "and still reports the full count" );

	std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
	if ( !Check( BkEditorObjects( pSession, &records[0], nCount, &nCount ) == BK_EDITOR_OK, "the list fits" ) )
		return;
	for ( int i = 0; i < nCount; ++i )
	{
		const bool bScenario = i >= int( map.objects.size() );
		const SMapObjectInfo &rObject = bScenario ? map.scenarioObjects[i - map.objects.size()] : map.objects[i];
		if ( !Check( records[i].link_id == rObject.link.nLinkID &&
		             strcmp( records[i].name, rObject.szName.c_str() ) == 0 &&
		             records[i].x == rObject.vPos.x && records[i].y == rObject.vPos.y &&
		             records[i].dir == rObject.nDir && records[i].player == rObject.nPlayer &&
		             records[i].scenario == ( bScenario ? 1 : 0 ) && records[i].known == 1,
		             Describe( "object %d reads back as the file has it", i ).c_str() ) )
			return;
	}

	Check( summary.map_type == map.nType && summary.attacking_side == map.nAttackingSide, "the summary carries the map's own fields" );
	for ( int nPlayer = 0; nPlayer < int( map.diplomacies.size() ); ++nPlayer )
	{
		int nValue = -1;
		Check( BkEditorDiplomacy( pSession, nPlayer, &nValue ) == BK_EDITOR_OK && nValue == map.diplomacies[nPlayer],
		       Describe( "player %d's side reads back", nPlayer ).c_str() );
	}
	int nIgnored = 0;
	Check( BkEditorDiplomacy( pSession, int( map.diplomacies.size() ), &nIgnored ) == BK_EDITOR_BAD_ARGUMENT, "a player past the table is a caller bug" );
}

// An object whose type the database does not know is kept as it is: the
// preservation invariant writes it back unchanged, so no edit may reach it.
static void TestUnknownObjectIsReadOnly( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szCopy = szScratch + "\\coldwinter-unknown-read-only.bzm";
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	map.objects[0].szName = "No_Such_Object_In_Any_Database";
	const int nLinkID = map.objects[0].link.nLinkID;
	if ( !Check( NMapFile::Write( szCopy.c_str(), map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szCopy.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	BkEditorObjectRecord record;
	int nCount = 0;
	BkEditorObjects( pSession, &record, 1, &nCount );
	Check( record.link_id == nLinkID && record.known == 0, "the list marks it unknown" );
	Check( BkEditorDeleteObject( pSession, nLinkID ) == BK_EDITOR_REFUSED, "its delete is refused" );
	Check( BkEditorMoveObject( pSession, nLinkID, record.x + 32, record.y ) == BK_EDITOR_REFUSED, "and so is its move" );

	const std::string szSaved = szScratch + "\\coldwinter-unknown-read-only-saved.bzm";
	CMapInfo saved;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapEquivalence::Compare( map, saved, &szError ), ( "and the saved map is the one read: " + szError ).c_str() );
	remove( szCopy.c_str() );
	remove( szSaved.c_str() );
}
```

`Describe` and `Check` already exist in the file (`Describe` is a printf-style `std::string` helper). If `NMapEquivalence::Compare` is spelled differently, use the name `MapEquivalence.h` declares. Plan 3's `TestUneditedSaveIsEquivalent` calls it.

- [ ] **Step 2: Run the tier and watch it fail to compile**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: compile errors for `BkEditorObjects`, `BkEditorDiplomacy`, `map_type`.

- [ ] **Step 3: Add the declarations**

In `bridge.h`, append `map_type` and `attacking_side` to `BkEditorMapSummary` with the comments above. Add `BkEditorObjectRecord`, `BkEditorObjects` and `BkEditorDiplomacy` after `BkEditorEngineObjectState`, with this comment:

```c
/* The map as the bridge holds it - the snapshot with the session's edits in
   it - one record per object, objects before scenario objects, in file order.
   Like BkEditorCatalogue, out_count is always the total, and a buffer too
   short for it is BK_EDITOR_REFUSED with nothing written past capacity. */
```

and for `BkEditorDiplomacy`:

```c
/* One player's entry in the diplomacy table: 0 and 1 are the two sides, 2 is
   neutral. A player outside the table is BK_EDITOR_BAD_ARGUMENT. */
```

- [ ] **Step 4: Implement the read-back**

In `session.cpp`, after `FindSnapshotObject`:

```cpp
bool ReadSessionObjects( SEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount )
{
	const CMapInfo &rMap = pSession->snapshot;
	const int nTotal = int( rMap.objects.size() + rMap.scenarioObjects.size() );
	*pnCount = nTotal;
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	int nOut = 0;
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size() && nOut < nCapacity; ++i, ++nOut )
		{
			const SMapObjectInfo &rObject = (*lists[nList])[i];
			BkEditorObjectRecord &rRecord = pOut[nOut];
			memset( &rRecord, 0, sizeof rRecord );
			rRecord.link_id = rObject.link.nLinkID;
			strncpy( rRecord.name, rObject.szName.c_str(), sizeof rRecord.name - 1 );
			rRecord.x = rObject.vPos.x;
			rRecord.y = rObject.vPos.y;
			rRecord.dir = rObject.nDir;
			rRecord.player = rObject.nPlayer;
			rRecord.scenario = nList;
			rRecord.known = std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(),
			                           rObject.link.nLinkID ) == pSession->unknownLinkIDs.end() ? 1 : 0;
		}
	return nCapacity >= nTotal;
}
```

(`#include <algorithm>` and `<cstring>` if they are not already in.) In `bridge.cpp`, fill the two new summary fields where the others are (`bridge.cpp:178`), from `rMap.nType` and `rMap.nAttackingSide`. Add the entry points after `BkEditorEngineObjectState`, following `BkEditorCatalogue`'s shape:

```cpp
BkEditorStatus BkEditorObjects( BkEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return ReadSessionObjects( pSession, pOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDiplomacy( BkEditorSession *pSession, int nPlayer, int *pnValue )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnValue == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nPlayer < 0 || nPlayer >= int( pSession->snapshot.diplomacies.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnValue = pSession->snapshot.diplomacies[nPlayer];
		return BK_EDITOR_OK;
	} );
}
```

- [ ] **Step 5: Refuse edits to unknown objects**

At the top of `DeleteObjectFromSession` and `PlaceObjectInSession` (after their `bMapOpen` check), add:

```cpp
	// The preservation invariant: an object the database does not know is
	// written back exactly as it was read, so no edit may reach it.
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
```

- [ ] **Step 6: Run the tier**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS`.

- [ ] **Step 7: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): the bridge lists its objects and map fields, and leaves unknown objects alone

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Exact undo in the bridge

**Files:**
- Modify: `Sources/src/MapFile/MapOverlay.h`, `Sources/src/MapFile/MapOverlay.cpp`
- Modify: `Sources/src/Scene/Terrain.h:74-78`, `Sources/src/Scene/TerrainInternal.h`, `Sources/src/Scene/TerrainEditor.cpp`
- Modify: `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`, `session.h`, `session.cpp`
- Test: `tools/zig/map_file_test.cpp`, `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `NMapOverlay::Paint`, `UndoPaint`, `SPaintUndo`, the anonymous `Record` and `FindObject` (`MapOverlay.cpp:16, 41`).
- Produces (MapOverlay.h):
  ```cpp
  struct SAddObject { /* ... */ int nLinkID; /* -1: NextLinkID; otherwise this one, which must be free */ };
  struct SDeletedObject
  {
  	SMapObjectInfo object;
  	bool bScenario;
  	size_t nIndex;
  	SDeletedObject() : bScenario( false ), nIndex( 0 ) {  }
  };
  bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal, SDeletedObject *pDeleted = 0 );
  // Puts the record back at its index (or the end of its list, if the list is
  // now shorter). Refuses when the link ID is in use again.
  bool RestoreObject( SLoadMapInfo *pMap, const SDeletedObject &rDeleted );
  // The region's tiles and patches as they are now, in SPaintUndo's layout.
  void CaptureRegion( const SLoadMapInfo &rMap, const CTRect<int> &rPatches, SPaintUndo *pOut );
  ```
- Produces (Terrain.h, in `ITerrainEditor`):
  ```cpp
  // Puts a region back as it was: the tiles of its cell rectangle and its
  // patches, row-major, in NMapOverlay::SPaintUndo's layout. Unlike Update,
  // runs no preprocessing and regenerates nothing - undo has to land on the
  // exact state it recorded, not on what the builder would make of it.
  virtual void STDCALL RestoreRegion( const CTRect<int> &rcPatches, const std::vector<SMainTileInfo> &tiles,
                                      const std::vector<STerrainPatchInfo> &patches ) = 0;
  ```
- Produces (bridge.h):
  ```c
  BkEditorStatus BkEditorPaint( BkEditorSession *session, const BkEditorPaintCell *cells, int count, int *out_token );
  BkEditorStatus BkEditorUndoPaint( BkEditorSession *session, int token );
  BkEditorStatus BkEditorRedoPaint( BkEditorSession *session, int token );
  BkEditorStatus BkEditorRestoreObject( BkEditorSession *session, int link_id );
  ```

- [ ] **Step 1: Write the failing map-file tests**

Add to `tools/zig/map_file_test.cpp` and call them next to `TestFailedPaintChangesNothing`:

```cpp
// A deleted object comes back exactly: same record, same list, same place in it.
static void TestDeleteThenRestoreIsTheOriginal( const char *pszMap )
{
	CMapInfo map, original;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	original = map;
	// The first object nothing refers to.
	int nLinkID = -1;
	for ( size_t i = 0; i < map.objects.size() && nLinkID < 0; ++i )
	{
		std::vector<std::string> references;
		NMapOverlay::FindReferences( map, map.objects[i].link.nLinkID, &references );
		if ( references.empty() )
			nLinkID = map.objects[i].link.nLinkID;
	}
	if ( !Check( nLinkID >= 0, "the map has an unreferenced object" ) )
		return;
	NMapOverlay::SDeletedObject deleted;
	std::string szRefusal;
	Check( NMapOverlay::DeleteObject( &map, nLinkID, &szRefusal, &deleted ), szRefusal.c_str() );
	Check( deleted.object.link.nLinkID == nLinkID, "the delete hands back the record" );
	Check( NMapOverlay::RestoreObject( &map, deleted ), "the record goes back" );
	Check( NMapEquivalence::Compare( original, map, &szError ), ( "and the map is the original: " + szError ).c_str() );
	Check( !NMapOverlay::RestoreObject( &map, deleted ), "a second restore is refused: the link ID is in use" );
}

// An add can be told its link ID, and is refused one already taken.
static void TestAddTakesAGivenLinkID( const char *pszMap )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	NMapOverlay::SAddObject add;
	add.szName = map.objects[0].szName;
	add.nLinkID = NMapOverlay::NextLinkID( map ) + 10;
	int nLinkID = -1;
	Check( NMapOverlay::AddObject( &map, add, &nLinkID ) && nLinkID == add.nLinkID, "an add takes the link ID it is given" );
	add.nLinkID = map.objects[0].link.nLinkID;
	Check( !NMapOverlay::AddObject( &map, add, &nLinkID ), "and refuses one in use" );
}

// CaptureRegion reads what UndoPaint writes, so capture-paint-restore is the identity.
static void TestCaptureRestoresAPaint( const char *pszMap )
{
	CMapInfo map, original;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	original = map;
	std::vector<NMapOverlay::SPaintCell> cells( 1 );
	cells[0].nX = 5;
	cells[0].nY = 5;
	cells[0].tile = BYTE( ( map.terrain.tiles[5][5].tile + 1 ) % 4 );
	NMapOverlay::SPaintUndo undo, after;
	Check( NMapOverlay::Paint( &map, cells, &undo ), "the paint runs" );
	NMapOverlay::CaptureRegion( map, undo.rPatches, &after );
	NMapOverlay::UndoPaint( &map, undo );
	Check( NMapEquivalence::Compare( original, map, &szError ), ( "undo is the original: " + szError ).c_str() );
	NMapOverlay::UndoPaint( &map, after );
	NMapOverlay::SPaintUndo again;
	NMapOverlay::CaptureRegion( map, undo.rPatches, &again );
	Check( again.tiles.size() == after.tiles.size() &&
	       memcmp( &again.tiles[0], &after.tiles[0], after.tiles.size() * sizeof after.tiles[0] ) == 0,
	       "restoring the capture is the painted state" );
}
```

Run each against the same map the other overlay tests use (the tier's fixed sample; see how `TestFailedPaintChangesNothing` is called).

- [ ] **Step 2: Run the map-file tier and watch it fail to compile**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: compile errors for `SDeletedObject`, `RestoreObject`, `CaptureRegion`, `SAddObject::nLinkID`.

- [ ] **Step 3: Implement the overlay additions**

In `MapOverlay.h`, add `int nLinkID;` to `SAddObject` (initialised to `-1`), `SDeletedObject`, the fourth `DeleteObject` parameter, `RestoreObject` and `CaptureRegion`, with the comments from the Interfaces block. In `MapOverlay.cpp`:

`AddObject`: replace `object.link.nLinkID = NextLinkID( *pMap );` with

```cpp
	if ( rAdd.nLinkID >= 0 && FindObject( pMap, rAdd.nLinkID, 0, 0 ) != 0 )
		return false;
	object.link.nLinkID = rAdd.nLinkID >= 0 ? rAdd.nLinkID : NextLinkID( *pMap );
```

`DeleteObject`: after `FindObject` succeeds and before the `erase`:

```cpp
	if ( pDeleted )
	{
		pDeleted->object = (*pList)[nIndex];
		pDeleted->bScenario = pList == &pMap->scenarioObjects;
		pDeleted->nIndex = nIndex;
	}
```

and the two new functions:

```cpp
bool RestoreObject( SLoadMapInfo *pMap, const SDeletedObject &rDeleted )
{
	if ( pMap == 0 || FindObject( pMap, rDeleted.object.link.nLinkID, 0, 0 ) != 0 )
		return false;
	std::vector<SMapObjectInfo> &rList = rDeleted.bScenario ? pMap->scenarioObjects : pMap->objects;
	const size_t nIndex = Min( rDeleted.nIndex, rList.size() );
	rList.insert( rList.begin() + nIndex, rDeleted.object );
	return true;
}

void CaptureRegion( const SLoadMapInfo &rMap, const CTRect<int> &rPatches, SPaintUndo *pOut )
{
	if ( pOut != 0 )
		Record( rMap.terrain, rPatches, pOut );
}
```

`Record` is in the anonymous namespace above, so `CaptureRegion` can call it.

- [ ] **Step 4: Run the map-file tier**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the same pass line as before, with no failure lines.

- [ ] **Step 5: Write the failing engine-tier tests**

Add to `editor_bridge_test.cpp` and call them from `main` after `TestPaintReachesEngineAndFile`. The existing paint test calls `BkEditorPaint( pSession, cells, n )`. Give it a fourth argument, `&nToken`, with a local `int nToken = 0;`.

```cpp
// A paint undone is the map as it was, in the file and in the engine; redone,
// it is the paint again. Out of order is refused.
static void TestPaintUndoIsExact( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	NMapFile::Read( SHIPPED_MAP, &original, &szError );
	const unsigned char tile = (unsigned char)( ( original.terrain.tiles[20][20].tile + 1 ) % 4 );
	BkEditorPaintCell first[] = { { 20, 20, tile }, { 21, 20, tile } };
	BkEditorPaintCell second[] = { { 22, 20, tile } };
	int nFirst = -1, nSecond = -1;
	Check( BkEditorPaint( pSession, first, 2, &nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorPaint( pSession, second, 1, &nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const std::string szPainted = szScratch + "\\undo-painted.bzm";
	BkEditorSaveMap( pSession, szPainted.c_str() );

	Check( BkEditorUndoPaint( pSession, nFirst ) == BK_EDITOR_REFUSED, "the older paint is not undone first" );
	Check( BkEditorUndoPaint( pSession, nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorUndoPaint( pSession, nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const std::string szUndone = szScratch + "\\undo-undone.bzm";
	CMapInfo undone;
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szUndone.c_str(), &undone, &szError ), szError.c_str() ) )
		Check( NMapEquivalence::Compare( original, undone, &szError ), ( "two paints undone are the original: " + szError ).c_str() );

	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_REFUSED, "redo takes the most recently undone first" );
	Check( BkEditorRedoPaint( pSession, nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	CMapInfo painted, redone;
	const std::string szRedone = szScratch + "\\undo-redone.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRedone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     NMapFile::Read( szPainted.c_str(), &painted, &szError ) && NMapFile::Read( szRedone.c_str(), &redone, &szError ) )
		Check( NMapEquivalence::Compare( painted, redone, &szError ), ( "redone is the paint, crosses and all: " + szError ).c_str() );

	// A new paint ends the redo branch.
	Check( BkEditorUndoPaint( pSession, nSecond ) == BK_EDITOR_OK, "undo once more" );
	int nThird = -1;
	BkEditorPaint( pSession, second, 1, &nThird );
	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_REFUSED, "a paint after an undo drops what could be redone" );
	remove( szPainted.c_str() );
	remove( szUndone.c_str() );
	remove( szRedone.c_str() );
}

// Delete then restore is the original object, in the map and in the engine,
// and add - delete - restore keeps the added object's link ID.
static void TestDeleteRestoreKeepsTheObject( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	NMapFile::Read( SHIPPED_MAP, &original, &szError );

	int nLinkID = -1;
	BkEditorObjectState before;
	for ( size_t i = 0; i < original.objects.size() && nLinkID < 0; ++i )
	{
		const int nCandidate = original.objects[i].link.nLinkID;
		if ( BkEditorEngineObjectState( pSession, nCandidate, &before ) != BK_EDITOR_OK )
			continue;		// not placed
		if ( BkEditorDeleteObject( pSession, nCandidate ) == BK_EDITOR_OK )
			nLinkID = nCandidate;
	}
	if ( !Check( nLinkID >= 0, "some placed object can be deleted" ) )
		return;
	Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	BkEditorObjectState after;
	Check( BkEditorEngineObjectState( pSession, nLinkID, &after ) == BK_EDITOR_OK &&
	       after.x == before.x && after.y == before.y && after.dir == before.dir && after.player == before.player,
	       "the engine holds it again, where it was" );
	const std::string szSaved = szScratch + "\\restore-saved.bzm";
	CMapInfo saved;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapEquivalence::Compare( original, saved, &szError ), ( "delete then restore is the original map: " + szError ).c_str() );
	Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_REFUSED, "nothing to restore twice" );

	// The link-ID hazard: delete the highest ID, add, then undo both. The add
	// must not have been handed the deleted object's ID.
	int nHighest = -1;
	for ( size_t i = 0; i < original.objects.size(); ++i )
		nHighest = Max( nHighest, original.objects[i].link.nLinkID );
	for ( size_t i = 0; i < original.scenarioObjects.size(); ++i )
		nHighest = Max( nHighest, original.scenarioObjects[i].link.nLinkID );
	if ( BkEditorDeleteObject( pSession, nHighest ) == BK_EDITOR_OK )
	{
		BkEditorObjectState anywhere;
		BkEditorEngineObjectState( pSession, nLinkID, &anywhere );
		int nAdded = -1;
		if ( Check( BkEditorAddObject( pSession, original.objects[0].szName.c_str(), anywhere.x + 64, anywhere.y, 0, 0, &nAdded ) == BK_EDITOR_OK,
		            BkEditorLastMessage( pSession ) ) )
		{
			Check( nAdded != nHighest, "an add never reuses a deleted object's link ID" );
			Check( BkEditorDeleteObject( pSession, nAdded ) == BK_EDITOR_OK, "undo the add" );
			Check( BkEditorRestoreObject( pSession, nHighest ) == BK_EDITOR_OK, "undo the delete" );
			Check( BkEditorRestoreObject( pSession, nAdded ) == BK_EDITOR_OK && BkEditorDeleteObject( pSession, nAdded ) == BK_EDITOR_OK,
			       "and the added object's own restore still finds it" );
		}
	}
	remove( szSaved.c_str() );
}
```

- [ ] **Step 6: Run the tier and watch it fail to compile**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: compile errors for `BkEditorUndoPaint`, `BkEditorRedoPaint`, `BkEditorRestoreObject`, and the four-argument `BkEditorPaint`.

- [ ] **Step 7: `ITerrainEditor::RestoreRegion`**

Declare it in `Terrain.h` next to `Update` (the Interfaces block has the text) and in `TerrainInternal.h`'s `CTerrain`. Implement it in `TerrainEditor.cpp` after `CTerrain::Update`, mirroring `NMapOverlay::UndoPaint` (`MapOverlay.cpp:287`) for the data and `Update` (`:145-152`) for dropping the drawn patches:

```cpp
void CTerrain::RestoreRegion( const CTRect<int> &rcPatches, const std::vector<SMainTileInfo> &tiles,
                              const std::vector<STerrainPatchInfo> &patchInfos )
{
	size_t nTile = 0;
	for ( int y = rcPatches.miny * STerrainPatchInfo::nSizeY; y < rcPatches.maxy * STerrainPatchInfo::nSizeY; ++y )
		for ( int x = rcPatches.minx * STerrainPatchInfo::nSizeX; x < rcPatches.maxx * STerrainPatchInfo::nSizeX; ++x, ++nTile )
			if ( nTile < tiles.size() )
				terrainInfo.tiles[y][x] = tiles[nTile];
	size_t nPatch = 0;
	for ( int y = rcPatches.miny; y < rcPatches.maxy; ++y )
		for ( int x = rcPatches.minx; x < rcPatches.maxx; ++x, ++nPatch )
		{
			if ( nPatch < patchInfos.size() )
				terrainInfo.patches[y][x] = patchInfos[nPatch];
			// The drawn patch is built from the info on demand; drop it so the
			// next frame builds it from what was just put back.
			for ( std::list<STerrainPatch>::iterator it = patches.begin(); it != patches.end(); ++it )
				if ( it->nX == x && it->nY == y )
				{
					patches.erase( it );
					break;
				}
		}
	vOldAnchor.x = -1000000;
	vOldAnchor.y = -1000000;
}
```

`rcPatches` here uses `SPaintUndo`'s `miny`/`maxy` half-open bounds (`Record`, `MapOverlay.cpp:46-51`), not the inclusive `top`/`bottom` that `Update` iterates. Keep the two apart.

- [ ] **Step 8: The session's records**

In `session.h`, add to `SEditorSession`:

```cpp
	// Every paint of this session, by token (its index). A paint is undone by
	// putting back `before` and redone by putting back `after` - never by
	// running the function again, which would pick new random cross artwork
	// and, through the preprocessing pass, depend on everything painted since.
	struct SPaintRecord
	{
		NMapOverlay::SPaintUndo before, after;
	};
	std::vector<SPaintRecord> paints;
	std::vector<int> appliedPaints;		// undo takes the back
	std::vector<int> undonePaints;		// redo takes the back; a new paint clears it
	// Deleted objects, by link ID, for BkEditorRestoreObject: the snapshot's
	// record and the working copy's, which differ in the frame index.
	struct STombstone
	{
		NMapOverlay::SDeletedObject snapshot, working;
	};
	std::unordered_map<int, STombstone> tombstones;
	// One above every link ID this session has handed out or deleted, so an add
	// never takes the ID of an object a later undo will restore.
	int nLinkIDFloor;
```

Initialise `nLinkIDFloor( 0 )` in the constructor, and in `OpenMapIntoSession` clear `paints`, `appliedPaints`, `undonePaints` and `tombstones` and set `nLinkIDFloor = NMapOverlay::NextLinkID( snapshot )` once the map is read.

Declare:

```cpp
bool PaintIntoSession( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells, int *pnToken );
bool UndoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused );
bool RedoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused );
bool RestoreObjectInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused );
```

- [ ] **Step 9: Paint records and their undo**

In `PaintIntoSession`, after the engine's `Update` and `UpdateTerrain` at the end, record the paint:

```cpp
	SEditorSession::SPaintRecord record;
	record.before = undo;
	NMapOverlay::CaptureRegion( pSession->snapshot, undo.rPatches, &record.after );
	pSession->paints.push_back( record );
	*pnToken = int( pSession->paints.size() ) - 1;
	pSession->appliedPaints.push_back( *pnToken );
	pSession->undonePaints.clear();
	return true;
```

and add, after it:

```cpp
namespace {
// Puts one recorded region back into both copies and the engine, raw.
bool PutRegionBack( SEditorSession *pSession, const NMapOverlay::SPaintUndo &rRegion )
{
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pEngineTerrain == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	NMapOverlay::UndoPaint( &pSession->snapshot, rRegion );
	NMapOverlay::UndoPaint( &pSession->working, rRegion );
	pEngineTerrain->RestoreRegion( rRegion.rPatches, rRegion.tiles, rRegion.patches );
	// The AI's passability follows the tiles. UpdateTerrain takes inclusive
	// patch bounds, the way PaintIntoSession hands it rPatches; SPaintUndo's
	// are half-open, so the right and bottom edges come in by one.
	const CTRect<int> &r = rRegion.rPatches;
	pAIEditor->UpdateTerrain( CTRect<int>( r.minx, r.miny, r.maxx - 1, r.maxy - 1 ), pSession->working.terrain );
	return true;
}
}

bool UndoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession->appliedPaints.empty() || pSession->appliedPaints.back() != nToken )
	{
		pSession->szMessage = "paints are undone newest first";
		*pbRefused = true;
		return false;
	}
	if ( !PutRegionBack( pSession, pSession->paints[nToken].before ) )
		return false;
	pSession->appliedPaints.pop_back();
	pSession->undonePaints.push_back( nToken );
	return true;
}

bool RedoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession->undonePaints.empty() || pSession->undonePaints.back() != nToken )
	{
		pSession->szMessage = "paints are redone in the order they were undone";
		*pbRefused = true;
		return false;
	}
	if ( !PutRegionBack( pSession, pSession->paints[nToken].after ) )
		return false;
	pSession->undonePaints.pop_back();
	pSession->appliedPaints.push_back( nToken );
	return true;
}
```

Check `AffectedPatches`' convention before trusting the `- 1`. `PaintIntoSession` hands `rPatches` from `AffectedPatches` to both `Update` and `UpdateTerrain`, and `Paint` hands the same rectangle to `Record`. If `AffectedPatches` returns the half-open `minx..maxx` that `Record` iterates, `Update` is already being given a half-open rectangle and the `- 1` above is wrong. Read `AffectedPatches` (`MapOverlay.cpp:211`) and pass `UpdateTerrain` whatever `PaintIntoSession` passes it today, converted the same way.

- [ ] **Step 10: Tombstones and the link-ID floor**

In `AddObjectToSession`, before the snapshot's `AddObject`:

```cpp
	NMapOverlay::SAddObject add = rAdd;
	add.nLinkID = Max( NMapOverlay::NextLinkID( pSession->snapshot ), pSession->nLinkIDFloor );
```

Pass `add` to both `AddObject` calls, and after the object is placed set `pSession->nLinkIDFloor = nLinkID + 1;`. The working copy then gets the same ID explicitly, where today it relies on computing the same `NextLinkID`.

In `DeleteObjectFromSession`, pass `&tombstone.snapshot` and `&tombstone.working` to the two `DeleteObject` calls (with `SEditorSession::STombstone tombstone;` declared above them). After both succeed:

```cpp
	pSession->tombstones[nLinkID] = tombstone;
	pSession->nLinkIDFloor = Max( pSession->nLinkIDFloor, nLinkID + 1 );
```

Then add `RestoreObjectInSession`:

```cpp
bool RestoreObjectInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused )
{
	*pbRefused = false;
	std::unordered_map<int, SEditorSession::STombstone>::iterator it = pSession->tombstones.find( nLinkID );
	if ( it == pSession->tombstones.end() )
	{
		pSession->szMessage = "no deleted object has that link ID";
		*pbRefused = true;
		return false;
	}
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine is not there";
		return false;
	}
	if ( !NMapOverlay::RestoreObject( &pSession->snapshot, it->second.snapshot ) )
	{
		pSession->szMessage = "the link ID is in use again";
		*pbRefused = true;
		return false;
	}
	NMapOverlay::RestoreObject( &pSession->working, it->second.working );
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( it->second.working.object.szName.c_str() );
	IRefCount *pAIObject = pDesc != 0 ? PlaceOneObject( it->second.working.object, pDesc, pAIEditor ) : 0;
	if ( pAIObject == 0 )
	{
		std::string szIgnored;
		NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szIgnored );
		NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );
		pSession->szMessage = "the engine would not take the object back";
		*pbRefused = true;
		return false;
	}
	pSession->byLinkID[nLinkID] = pAIObject;
	pSession->tombstones.erase( it );
	return true;
}
```

The engine object comes from the working record, whose frame index is unpacked, the way `OpenMapIntoSession` builds it. The snapshot keeps its packed index for the save.

- [ ] **Step 11: The entry points**

In `bridge.h`, change `BkEditorPaint` to take `int *out_token` (it may be null), and document the token:

```c
/* out_token names this paint for BkEditorUndoPaint and BkEditorRedoPaint. The
   bridge keeps the order: undo takes the newest applied paint, redo the most
   recently undone, and a new paint drops everything undone. A token out of
   that order is BK_EDITOR_REFUSED. Undo puts back exactly the tiles and
   crosses the paint recorded, in the map and the engine; redo puts back
   exactly what the paint left - it does not paint again. */
BkEditorStatus BkEditorUndoPaint( BkEditorSession *session, int token );
BkEditorStatus BkEditorRedoPaint( BkEditorSession *session, int token );

/* Puts a deleted object back as it was: same record, same link ID, same place
   in its list, and in the engine where it stood. Undo of a delete, and redo of
   an add. BK_EDITOR_REFUSED when there is no such deleted object. */
BkEditorStatus BkEditorRestoreObject( BkEditorSession *session, int link_id );
```

In `bridge.cpp`, `BkEditorPaint` passes a local token to `PaintIntoSession` and stores it in `*pnToken` when that is not null. The three new entry points follow `BkEditorDeleteObject` (`bridge.cpp:298`): check `bMapOpen`, then call the session function, and turn `bRefused` into `BK_EDITOR_REFUSED` or `BK_EDITOR_FAILED`.

- [ ] **Step 12: Run both tiers**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run && zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the map-file pass line, then `editor-bridge: PASS`.

If `TerrainMatchesEngine` fails after an undo, do not loosen it. The engine and the copy then hold different regions, which is exactly what the editor would draw wrongly. Print the first difference it names and fix `RestoreRegion` or the rectangle conversion.

- [ ] **Step 13: Commit**

```bash
git add Sources/src/MapFile Sources/src/Scene Sources/src/EditorBridge tools/zig/map_file_test.cpp tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): paints and deletes undo exactly, and link IDs are never reused

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: The core's bridge interface, the fake, and the core tier on six targets

**Files:**
- Create: `Sources/editor/core/root.zig`, `Sources/editor/core/bridge.zig`, `Sources/editor/core/fake_bridge.zig`
- Modify: `build.zig` (after `const test_step = b.step("test", ...)`, ~2677)
- Modify: `.github/workflows/cross-platform.yml` (all six jobs)

**Interfaces:**
- Consumes: the C ABI of Tasks 1-2, mirrored rather than imported.
- Produces (bridge.zig): `Status`, `MapInfo`, `ObjectRecord`, `PaintCell`, `Bridge` (with forwarding methods named as in the vtable below), `EditError`, `check(Status) EditError!void`.
- Produces (fake_bridge.zig): `FakeBridge` with `init(allocator, width_tiles, height_tiles, players) FakeBridge`, `deinit()`, `bridge() Bridge`, `addFixture(record, referenced) !void`, `tile(x, y) u8`, `calls`, `tile_size`.

- [ ] **Step 1: Write the interface**

`Sources/editor/core/bridge.zig`:

```zig
//! The core's view of the engine bridge (Sources/src/EditorBridge/bridge.h).
//! One call per C entry point, same arguments, same statuses, so the fake and
//! the real adapter (plan 5) can be read side by side. The core never sees a
//! C type: this file is plain Zig so the core builds on every target,
//! including x86_64-windows-gnu, where the engine C++ does not.
const std = @import("std");

pub const Status = enum(c_int) {
    ok = 0,
    bad_argument = 1,
    no_session = 2,
    no_device = 3,
    data_missing = 4,
    refused = 5,
    failed = 6,
};

/// What a command sees of a status: a refusal is an ordinary answer with a
/// message for the status bar, anything else is a failure.
pub const EditError = error{ Refused, Failed, OutOfMemory };

pub fn check(status: Status) EditError!void {
    return switch (status) {
        .ok => {},
        .refused => error.Refused,
        else => error.Failed,
    };
}

pub const MapInfo = struct {
    width_tiles: i32 = 0,
    height_tiles: i32 = 0,
    season: i32 = 0,
    player_count: i32 = 0,
    map_type: i32 = 0,
    attacking_side: i32 = 0,
};

pub const name_capacity = 64;

/// BkEditorObjectRecord.
pub const ObjectRecord = struct {
    link_id: i32 = -1,
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    dir: i32 = 0,
    player: i32 = 0,
    scenario: bool = false,
    known: bool = true,

    pub fn nameSlice(self: *const ObjectRecord) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ObjectRecord, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorPaintCell, layout included: the adapter hands a slice of these
/// straight to the C call.
pub const PaintCell = extern struct { x: c_int, y: c_int, tile: u8 };

pub const Bridge = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        lastMessage: *const fn (ptr: *anyopaque) []const u8,
        openMap: *const fn (ptr: *anyopaque, path: []const u8, info: *MapInfo) Status,
        saveMap: *const fn (ptr: *anyopaque, path: []const u8) Status,
        objects: *const fn (ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status,
        diplomacy: *const fn (ptr: *anyopaque, player: i32, value: *i32) Status,
        addObject: *const fn (ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status,
        placeObject: *const fn (ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status,
        deleteObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        restoreObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        setDiplomacy: *const fn (ptr: *anyopaque, player: i32, value: i32) Status,
        setMapType: *const fn (ptr: *anyopaque, value: i32) Status,
        setAttackingSide: *const fn (ptr: *anyopaque, value: i32) Status,
        paint: *const fn (ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status,
        undoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        redoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        screenToWorld: *const fn (ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status,
        worldToTile: *const fn (ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status,
        objectAt: *const fn (ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status,
    };

    pub fn lastMessage(self: Bridge) []const u8 { return self.vtable.lastMessage(self.ptr); }
    pub fn openMap(self: Bridge, path: []const u8, info: *MapInfo) Status { return self.vtable.openMap(self.ptr, path, info); }
    pub fn saveMap(self: Bridge, path: []const u8) Status { return self.vtable.saveMap(self.ptr, path); }
    pub fn objects(self: Bridge, out: []ObjectRecord, total: *usize) Status { return self.vtable.objects(self.ptr, out, total); }
    pub fn diplomacy(self: Bridge, player: i32, value: *i32) Status { return self.vtable.diplomacy(self.ptr, player, value); }
    pub fn addObject(self: Bridge, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status { return self.vtable.addObject(self.ptr, name, x, y, dir, player, link_id); }
    pub fn placeObject(self: Bridge, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status { return self.vtable.placeObject(self.ptr, link_id, x, y, dir, player); }
    pub fn deleteObject(self: Bridge, link_id: i32) Status { return self.vtable.deleteObject(self.ptr, link_id); }
    pub fn restoreObject(self: Bridge, link_id: i32) Status { return self.vtable.restoreObject(self.ptr, link_id); }
    pub fn setDiplomacy(self: Bridge, player: i32, value: i32) Status { return self.vtable.setDiplomacy(self.ptr, player, value); }
    pub fn setMapType(self: Bridge, value: i32) Status { return self.vtable.setMapType(self.ptr, value); }
    pub fn setAttackingSide(self: Bridge, value: i32) Status { return self.vtable.setAttackingSide(self.ptr, value); }
    pub fn paint(self: Bridge, cells: []const PaintCell, token: *i32) Status { return self.vtable.paint(self.ptr, cells, token); }
    pub fn undoPaint(self: Bridge, token: i32) Status { return self.vtable.undoPaint(self.ptr, token); }
    pub fn redoPaint(self: Bridge, token: i32) Status { return self.vtable.redoPaint(self.ptr, token); }
    pub fn screenToWorld(self: Bridge, sx: f32, sy: f32, wx: *f32, wy: *f32) Status { return self.vtable.screenToWorld(self.ptr, sx, sy, wx, wy); }
    pub fn worldToTile(self: Bridge, wx: f32, wy: f32, tx: *i32, ty: *i32) Status { return self.vtable.worldToTile(self.ptr, wx, wy, tx, ty); }
    pub fn objectAt(self: Bridge, sx: f32, sy: f32, link_id: *i32) Status { return self.vtable.objectAt(self.ptr, sx, sy, link_id); }
};

test "check turns a refusal into Refused and everything else into Failed" {
    try check(.ok);
    try std.testing.expectError(error.Refused, check(.refused));
    try std.testing.expectError(error.Failed, check(.failed));
    try std.testing.expectError(error.Failed, check(.no_device));
}

test "a paint cell has the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 12), @sizeOf(PaintCell));
    try std.testing.expectEqual(@as(usize, 8), @offsetOf(PaintCell, "tile"));
}
```

- [ ] **Step 2: Write the fake's tests first**

These pin the fake to the real bridge's rules, as Tasks 1-2 measured them. A fake that is kinder than the bridge would let core tests pass that the app then fails. Put them at the bottom of `Sources/editor/core/fake_bridge.zig`:

```zig
fn fixture(allocator: std.mem.Allocator) !FakeBridge {
    var fake = FakeBridge.init(allocator, 8, 8, 2);
    errdefer fake.deinit();
    var tank: ObjectRecord = .{ .link_id = 1, .x = 40, .y = 40, .dir = 0, .player = 0 };
    tank.setName("T34");
    try fake.addFixture(tank, false);
    var bridge_span: ObjectRecord = .{ .link_id = 2, .x = 100, .y = 40, .dir = 0, .player = 0 };
    bridge_span.setName("Bridge_Span");
    try fake.addFixture(bridge_span, true); // referred to, like a span named by a bridge
    var mystery: ObjectRecord = .{ .link_id = 3, .x = 150, .y = 150, .dir = 0, .player = 1, .known = false };
    mystery.setName("No_Such_Object");
    try fake.addFixture(mystery, false);
    return fake;
}

test "the fake refuses what the bridge refuses" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    try std.testing.expectEqual(Status.refused, b.deleteObject(2)); // referenced
    try std.testing.expectEqual(Status.refused, b.deleteObject(3)); // unknown type
    try std.testing.expectEqual(Status.refused, b.placeObject(3, 10, 10, 0, 1)); // unknown type
    try std.testing.expectEqual(Status.refused, b.placeObject(1, -5, 10, 0, 0)); // off the map
    var link: i32 = -1;
    try std.testing.expectEqual(Status.refused, b.addObject("T34", 9999, 10, 0, 0, &link));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(7, 0));
}

test "the fake never reuses a deleted object's link ID" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    try std.testing.expectEqual(Status.ok, b.deleteObject(1));
    var added: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.addObject("T34", 60, 60, 0, 0, &added));
    try std.testing.expect(added > 3);
    try std.testing.expectEqual(Status.ok, b.restoreObject(1));
    try std.testing.expectEqual(Status.refused, b.restoreObject(1));
}

test "the fake's paints undo newest first and redo in undo order" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var first: i32 = -1;
    var second: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 5 }}, &first));
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 6 }}, &second));
    try std.testing.expectEqual(Status.refused, b.undoPaint(first));
    try std.testing.expectEqual(Status.ok, b.undoPaint(second));
    try std.testing.expectEqual(@as(u8, 5), fake.tile(1, 1));
    try std.testing.expectEqual(Status.ok, b.undoPaint(first));
    try std.testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try std.testing.expectEqual(Status.refused, b.redoPaint(second));
    try std.testing.expectEqual(Status.ok, b.redoPaint(first));
    try std.testing.expectEqual(Status.refused, b.paint(&.{.{ .x = 8, .y = 0, .tile = 1 }}, &first)); // off the map
}

test "the fake lists objects the way BkEditorObjects does" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var total: usize = 0;
    var none: [0]ObjectRecord = .{};
    try std.testing.expectEqual(Status.refused, b.objects(&none, &total));
    try std.testing.expectEqual(@as(usize, 3), total);
    var out: [3]ObjectRecord = undefined;
    try std.testing.expectEqual(Status.ok, b.objects(&out, &total));
    try std.testing.expectEqualStrings("T34", out[0].nameSlice());
    try std.testing.expect(!out[2].known);
}
```

- [ ] **Step 3: Write the fake**

Top of `Sources/editor/core/fake_bridge.zig`:

```zig
//! An in-memory map behind the Bridge interface, for the core tier. It
//! follows the real bridge's rules where the core can see them - refusals,
//! link IDs, the order paints undo and redo in, the object list's shape - and
//! nothing else: there is no terrain function, so a paint sets exactly the
//! cells it names. Screen and world coordinates are the same thing here.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const Status = bridge_mod.Status;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const PaintCell = bridge_mod.PaintCell;
const Bridge = bridge_mod.Bridge;

/// World units per tile, standing in for the engine's own conversion.
pub const tile_size: f32 = 32.0;
/// How far from an object's centre a point still picks it.
pub const pick_radius: f32 = 16.0;

pub const CallKind = enum { open, save, add, place, delete, restore, diplomacy, map_type, attacking_side, paint, undo_paint, redo_paint };
pub const Call = struct { kind: CallKind, id: i32 = 0 };

const Tombstone = struct { record: ObjectRecord, index: usize };
/// A mutable empty slice to start from; Allocator.free ignores a zero length.
var no_tiles: [0]u8 = .{};
const PaintRecord = struct { cells: []PaintCell, before: []u8 };

pub const FakeBridge = struct {
    allocator: std.mem.Allocator,
    info: MapInfo,
    objects_list: std.ArrayListUnmanaged(ObjectRecord) = .empty,
    referenced: std.AutoHashMapUnmanaged(i32, void) = .empty,
    tombstones: std.AutoHashMapUnmanaged(i32, Tombstone) = .empty,
    diplomacy_table: std.ArrayListUnmanaged(i32) = .empty,
    tiles: []u8 = &no_tiles,
    paints: std.ArrayListUnmanaged(PaintRecord) = .empty,
    applied: std.ArrayListUnmanaged(i32) = .empty,
    undone: std.ArrayListUnmanaged(i32) = .empty,
    link_floor: i32 = 0,
    calls: std.ArrayListUnmanaged(Call) = .empty,
    message_buffer: [160]u8 = undefined,
    message_len: usize = 0,

    pub fn init(allocator: std.mem.Allocator, width_tiles: i32, height_tiles: i32, players: i32) FakeBridge {
        return .{
            .allocator = allocator,
            .info = .{ .width_tiles = width_tiles, .height_tiles = height_tiles, .player_count = players },
        };
    }

    pub fn deinit(self: *FakeBridge) void {
        for (self.paints.items) |record| {
            self.allocator.free(record.cells);
            self.allocator.free(record.before);
        }
        self.paints.deinit(self.allocator);
        self.applied.deinit(self.allocator);
        self.undone.deinit(self.allocator);
        self.objects_list.deinit(self.allocator);
        self.referenced.deinit(self.allocator);
        self.tombstones.deinit(self.allocator);
        self.diplomacy_table.deinit(self.allocator);
        self.calls.deinit(self.allocator);
        self.allocator.free(self.tiles);
        self.* = undefined;
    }

    /// An object in the map before it opens; `referenced` makes its delete
    /// refused, as a bridge or start command holding it would.
    pub fn addFixture(self: *FakeBridge, record: ObjectRecord, referenced: bool) !void {
        try self.objects_list.append(self.allocator, record);
        if (referenced) try self.referenced.put(self.allocator, record.link_id, {});
    }

    pub fn tile(self: *const FakeBridge, x: i32, y: i32) u8 {
        return self.tiles[@intCast(y * self.info.width_tiles + x)];
    }

    pub fn bridge(self: *FakeBridge) Bridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *FakeBridge {
        return @ptrCast(@alignCast(ptr));
    }

    fn say(self: *FakeBridge, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    fn record(self: *FakeBridge, kind: CallKind, id: i32) void {
        self.calls.append(self.allocator, .{ .kind = kind, .id = id }) catch {};
    }

    fn onMap(self: *const FakeBridge, x: f32, y: f32) bool {
        return x >= 0 and y >= 0 and
            x < @as(f32, @floatFromInt(self.info.width_tiles)) * tile_size and
            y < @as(f32, @floatFromInt(self.info.height_tiles)) * tile_size;
    }

    fn indexOf(self: *const FakeBridge, link_id: i32) ?usize {
        for (self.objects_list.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
    }

    fn nextLinkId(self: *const FakeBridge) i32 {
        var next: i32 = self.link_floor;
        for (self.objects_list.items) |object| next = @max(next, object.link_id + 1);
        return next;
    }

    const vtable: Bridge.VTable = .{
        .lastMessage = lastMessage,
        .openMap = openMap,
        .saveMap = saveMap,
        .objects = objects,
        .diplomacy = diplomacy,
        .addObject = addObject,
        .placeObject = placeObject,
        .deleteObject = deleteObject,
        .restoreObject = restoreObject,
        .setDiplomacy = setDiplomacy,
        .setMapType = setMapType,
        .setAttackingSide = setAttackingSide,
        .paint = paint,
        .undoPaint = undoPaint,
        .redoPaint = redoPaint,
        .screenToWorld = screenToWorld,
        .worldToTile = worldToTile,
        .objectAt = objectAt,
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }

    fn openMap(ptr: *anyopaque, path: []const u8, info: *MapInfo) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.open, 0);
        if (std.mem.eql(u8, path, "missing.bzm")) {
            self.say("no such map", .{});
            return .data_missing;
        }
        if (self.tiles.len == 0) {
            self.tiles = self.allocator.alloc(u8, @intCast(self.info.width_tiles * self.info.height_tiles)) catch return .failed;
            @memset(self.tiles, 0);
            self.diplomacy_table.resize(self.allocator, @intCast(self.info.player_count)) catch return .failed;
            for (self.diplomacy_table.items, 0..) |*side, player| side.* = @intCast(player % 2);
        }
        self.link_floor = self.nextLinkId();
        info.* = self.info;
        return .ok;
    }

    fn saveMap(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.save, 0);
        if (path.len == 0) return .bad_argument;
        return .ok;
    }

    fn objects(ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status {
        const self = from(ptr);
        total.* = self.objects_list.items.len;
        const count = @min(out.len, self.objects_list.items.len);
        @memcpy(out[0..count], self.objects_list.items[0..count]);
        return if (out.len >= self.objects_list.items.len) .ok else .refused;
    }

    fn diplomacy(ptr: *anyopaque, player: i32, value: *i32) Status {
        const self = from(ptr);
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        value.* = self.diplomacy_table.items[@intCast(player)];
        return .ok;
    }

    fn addObject(ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (name.len == 0) return .bad_argument;
        if (!self.onMap(x, y)) {
            self.say("the engine would not place the object there", .{});
            return .refused;
        }
        var object: ObjectRecord = .{ .link_id = self.nextLinkId(), .x = x, .y = y, .dir = dir, .player = player };
        object.setName(name);
        self.objects_list.append(self.allocator, object) catch return .failed;
        self.link_floor = object.link_id + 1;
        link_id.* = object.link_id;
        self.record(.add, object.link_id);
        return .ok;
    }

    fn placeObject(ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const index = self.indexOf(link_id) orelse return .bad_argument;
        const object = &self.objects_list.items[index];
        if (!object.known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (!self.onMap(x, y)) {
            self.say("the engine would not take that placement for the object", .{});
            return .refused;
        }
        object.x = x;
        object.y = y;
        object.dir = dir;
        object.player = player;
        self.record(.place, link_id);
        return .ok;
    }

    fn deleteObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (!self.objects_list.items[index].known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (self.referenced.contains(link_id)) {
            self.say("still referred to by bridge 0", .{});
            return .refused;
        }
        const removed = self.objects_list.orderedRemove(index);
        self.tombstones.put(self.allocator, link_id, .{ .record = removed, .index = index }) catch return .failed;
        self.link_floor = @max(self.link_floor, link_id + 1);
        self.record(.delete, link_id);
        return .ok;
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const tombstone = self.tombstones.get(link_id) orelse {
            self.say("no deleted object has that link ID", .{});
            return .refused;
        };
        if (self.indexOf(link_id) != null) {
            self.say("the link ID is in use again", .{});
            return .refused;
        }
        const index = @min(tombstone.index, self.objects_list.items.len);
        self.objects_list.insert(self.allocator, index, tombstone.record) catch return .failed;
        _ = self.tombstones.remove(link_id);
        self.record(.restore, link_id);
        return .ok;
    }

    fn setDiplomacy(ptr: *anyopaque, player: i32, value: i32) Status {
        const self = from(ptr);
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        self.diplomacy_table.items[@intCast(player)] = value;
        self.record(.diplomacy, player);
        return .ok;
    }

    fn setMapType(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.info.map_type = value;
        self.record(.map_type, value);
        return .ok;
    }

    fn setAttackingSide(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.info.attacking_side = value;
        self.record(.attacking_side, value);
        return .ok;
    }

    fn paint(ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        for (cells) |cell| {
            if (cell.x < 0 or cell.y < 0 or cell.x >= self.info.width_tiles or cell.y >= self.info.height_tiles) {
                self.say("cell {d},{d} is not on the map", .{ cell.x, cell.y });
                return .refused;
            }
        }
        const copy = self.allocator.dupe(PaintCell, cells) catch return .failed;
        const before = self.allocator.alloc(u8, cells.len) catch {
            self.allocator.free(copy);
            return .failed;
        };
        for (cells, 0..) |cell, index| {
            before[index] = self.tile(cell.x, cell.y);
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        self.paints.append(self.allocator, .{ .cells = copy, .before = before }) catch return .failed;
        token.* = @intCast(self.paints.items.len - 1);
        self.applied.append(self.allocator, token.*) catch return .failed;
        self.undone.clearRetainingCapacity();
        self.record(.paint, token.*);
        return .ok;
    }

    fn undoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.applied.items.len == 0 or self.applied.items[self.applied.items.len - 1] != token) {
            self.say("paints are undone newest first", .{});
            return .refused;
        }
        const entry = self.paints.items[@intCast(token)];
        // Backwards, so a cell named twice in one paint ends at its first value.
        var index = entry.cells.len;
        while (index != 0) {
            index -= 1;
            const cell = entry.cells[index];
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = entry.before[index];
        }
        _ = self.applied.pop();
        self.undone.append(self.allocator, token) catch return .failed;
        self.record(.undo_paint, token);
        return .ok;
    }

    fn redoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.undone.items.len == 0 or self.undone.items[self.undone.items.len - 1] != token) {
            self.say("paints are redone in the order they were undone", .{});
            return .refused;
        }
        for (self.paints.items[@intCast(token)].cells) |cell| {
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        _ = self.undone.pop();
        self.applied.append(self.allocator, token) catch return .failed;
        self.record(.redo_paint, token);
        return .ok;
    }

    fn screenToWorld(ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status {
        const self = from(ptr);
        if (!self.onMap(sx, sy)) return .refused;
        wx.* = sx;
        wy.* = sy;
        return .ok;
    }

    fn worldToTile(ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status {
        const self = from(ptr);
        if (!self.onMap(wx, wy)) return .refused;
        tx.* = @intFromFloat(@floor(wx / tile_size));
        ty.* = @intFromFloat(@floor(wy / tile_size));
        return .ok;
    }

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        const self = from(ptr);
        // The last one listed wins, as the topmost drawn would.
        var index = self.objects_list.items.len;
        while (index != 0) {
            index -= 1;
            const object = self.objects_list.items[index];
            if (!object.known) continue; // never placed, so never drawn
            if (@abs(object.x - sx) <= pick_radius and @abs(object.y - sy) <= pick_radius) {
                link_id.* = object.link_id;
                return .ok;
            }
        }
        return .refused;
    }
};
```

- [ ] **Step 4: The module root**

`Sources/editor/core/root.zig`:

```zig
//! The Map Editor core: no UI, no engine, no C. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Editor core".
pub const bridge = @import("bridge.zig");
pub const fake_bridge = @import("fake_bridge.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
```

Later tasks add their files here.

- [ ] **Step 5: The build step**

In `build.zig`, immediately after `const test_step = b.step("test", ...)` (~2677), in the same function:

```zig
    // The editor core tier: plain Zig against the fake bridge, so it runs on
    // every target, the MinGW job included.
    const editor_core_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/core/root.zig"),
        .target = b.graph.host,
        .optimize = .Debug,
    });
    const editor_core_tests = b.addTest(.{ .root_module = editor_core_module });
    const editor_core_tests_run = b.addRunArtifact(editor_core_tests);
    const editor_core_step = b.step("test-editor-core", "Run the Map Editor core tests against the fake bridge");
    editor_core_step.dependOn(&editor_core_tests.step);
    if (test_mode == .run) editor_core_step.dependOn(&editor_core_tests_run.step);
    test_step.dependOn(editor_core_step);
```

- [ ] **Step 6: Run the tier**

Run: `zig build test-editor-core -Dtest-mode=run --summary all`
Expected: the `test-editor-core` line shows every test passed and no leaks. Then run `zig test tools/zig/build_hermeticity_test.zig`. Expected: all passed.

- [ ] **Step 7: The CI step in all six jobs**

In `.github/workflows/cross-platform.yml`, add to each of the six jobs, directly after its "Map file tier" step (the MinGW job has none, so put it after that job's "Run GFX GPU effect table tests"):

```yaml
      - name: Editor core tier
        run: zig build test-editor-core <that job's flags> -Dtest-mode=run
```

`<that job's flags>` means the exact `-Dtarget`, `--sysroot` and `-Dmsvc-*` / `-Dwindows-sdk-*` arguments of the step it follows, copied verbatim. The build script is configured before the host-only module is chosen, and a job that configures differently from its neighbours caches differently.

- [ ] **Step 8: Commit**

```bash
git add Sources/editor/core build.zig .github/workflows/cross-platform.yml
git commit -m "feat(editor): the core's bridge interface, a fake that keeps the bridge's rules, and the core tier on six targets

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: The document, open and save

**Files:**
- Create: `Sources/editor/core/document.zig`, `Sources/editor/core/editor.zig`
- Modify: `Sources/editor/core/root.zig`

**Interfaces:**
- Consumes: `Bridge`, `MapInfo`, `ObjectRecord`, `EditError`, `check` (Task 3).
- Produces (document.zig):
  ```zig
  pub const Document = struct {
      path: std.ArrayListUnmanaged(u8),
      info: MapInfo,
      diplomacy: std.ArrayListUnmanaged(i32),
      objects: std.ArrayListUnmanaged(ObjectRecord),
      pub fn deinit(self: *Document, allocator: std.mem.Allocator) void;
      pub fn reload(self: *Document, allocator: std.mem.Allocator, b: Bridge, path: []const u8, info: MapInfo) EditError!void;
      pub fn indexOf(self: *const Document, link_id: i32) ?usize;
      pub fn find(self: *Document, link_id: i32) ?*ObjectRecord;
  };
  ```
- Produces (editor.zig, grown in Tasks 5-6):
  ```zig
  pub const Editor = struct {
      allocator: std.mem.Allocator,
      bridge: Bridge,
      document: Document,
      pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor;
      pub fn deinit(self: *Editor) void;
      pub fn open(self: *Editor, path: []const u8) EditError!void;
      pub fn save(self: *Editor, path: []const u8) EditError!void;
      pub fn status(self: *const Editor) []const u8;
  };
  ```

- [ ] **Step 1: Write the failing tests**

At the bottom of `Sources/editor/core/editor.zig` (create the file with just the tests and the imports):

```zig
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const fake_mod = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const Bridge = bridge_mod.Bridge;
const EditError = bridge_mod.EditError;
const ObjectRecord = bridge_mod.ObjectRecord;
const FakeBridge = fake_mod.FakeBridge;
const Document = document_mod.Document;

pub fn testFixture(allocator: std.mem.Allocator) !FakeBridge {
    var fake = FakeBridge.init(allocator, 8, 8, 2);
    errdefer fake.deinit();
    var tank: ObjectRecord = .{ .link_id = 1, .x = 40, .y = 40 };
    tank.setName("T34");
    try fake.addFixture(tank, false);
    var span: ObjectRecord = .{ .link_id = 2, .x = 100, .y = 40 };
    span.setName("Bridge_Span");
    try fake.addFixture(span, true);
    var mystery: ObjectRecord = .{ .link_id = 3, .x = 150, .y = 150, .player = 1, .known = false };
    mystery.setName("No_Such_Object");
    try fake.addFixture(mystery, false);
    return fake;
}

test "open fills the document from the bridge" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(@as(i32, 8), editor.document.info.width_tiles);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expectEqualStrings("T34", editor.document.find(1).?.nameSlice());
    try std.testing.expect(!editor.document.find(3).?.known);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, editor.document.diplomacy.items);
}

test "a failed open keeps the map that was open" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectError(error.Failed, editor.open("missing.bzm"));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqualStrings("no such map", editor.status());
}

test "save moves the document to the saved path" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try editor.save("renamed.bzm");
    try std.testing.expectEqualStrings("renamed.bzm", editor.document.path.items);
}
```

Add `pub const document = @import("document.zig");` and `pub const editor = @import("editor.zig");` to `root.zig`.

- [ ] **Step 2: Run the tier and watch it fail**

Run: `zig build test-editor-core -Dtest-mode=run`
Expected: compile error, `Editor` undeclared and `document.zig` missing.

- [ ] **Step 3: The document**

`Sources/editor/core/document.zig`:

```zig
//! The open map as the panels and the history see it: its path, its own
//! fields, the diplomacy table and every object, in the bridge's order. It
//! holds no terrain - tiles live in the bridge - and no dirty flag: the
//! history knows whether the map differs from what was saved.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const Bridge = bridge_mod.Bridge;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const EditError = bridge_mod.EditError;

pub const Document = struct {
    path: std.ArrayListUnmanaged(u8) = .empty,
    info: MapInfo = .{},
    diplomacy: std.ArrayListUnmanaged(i32) = .empty,
    objects: std.ArrayListUnmanaged(ObjectRecord) = .empty,

    pub fn deinit(self: *Document, allocator: std.mem.Allocator) void {
        self.path.deinit(allocator);
        self.diplomacy.deinit(allocator);
        self.objects.deinit(allocator);
        self.* = undefined;
    }

    /// Replaces everything with what the bridge holds for the map it just
    /// opened. Asks for the object count first, the way BkEditorObjects is
    /// meant to be called.
    pub fn reload(self: *Document, allocator: std.mem.Allocator, b: Bridge, path: []const u8, info: MapInfo) EditError!void {
        var total: usize = 0;
        var none: [0]ObjectRecord = .{};
        const sizing = b.objects(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        var objects: std.ArrayListUnmanaged(ObjectRecord) = .empty;
        errdefer objects.deinit(allocator);
        try objects.resize(allocator, total);
        try bridge_mod.check(b.objects(objects.items, &total));

        var diplomacy: std.ArrayListUnmanaged(i32) = .empty;
        errdefer diplomacy.deinit(allocator);
        try diplomacy.resize(allocator, @intCast(info.player_count));
        for (diplomacy.items, 0..) |*side, player| try bridge_mod.check(b.diplomacy(@intCast(player), side));

        self.path.clearRetainingCapacity();
        try self.path.appendSlice(allocator, path);
        self.objects.deinit(allocator);
        self.objects = objects;
        self.diplomacy.deinit(allocator);
        self.diplomacy = diplomacy;
        self.info = info;
    }

    pub fn indexOf(self: *const Document, link_id: i32) ?usize {
        for (self.objects.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
    }

    pub fn find(self: *Document, link_id: i32) ?*ObjectRecord {
        const index = self.indexOf(link_id) orelse return null;
        return &self.objects.items[index];
    }
};
```

- [ ] **Step 4: The editor, open and save**

Above the tests in `editor.zig`:

```zig
/// The core's one entry point for the app: every edit goes through here, so
/// the bridge, the document and the history never disagree.
pub const Editor = struct {
    allocator: std.mem.Allocator,
    bridge: Bridge,
    document: Document = .{},
    status_buffer: [256]u8 = undefined,
    status_len: usize = 0,

    pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor {
        return .{ .allocator = allocator, .bridge = b };
    }

    pub fn deinit(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.* = undefined;
    }

    /// The status bar's line: the bridge's reason for the last refusal or
    /// failure, empty after a success.
    pub fn status(self: *const Editor) []const u8 {
        return self.status_buffer[0..self.status_len];
    }

    fn noteOutcome(self: *Editor, status_code: bridge_mod.Status) EditError!void {
        if (status_code == .ok) {
            self.status_len = 0;
            return;
        }
        const message = self.bridge.lastMessage();
        const len = @min(message.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..len], message[0..len]);
        self.status_len = len;
        return bridge_mod.check(status_code);
    }

    /// On failure the map that was open stays open, as the bridge keeps it.
    pub fn open(self: *Editor, path: []const u8) EditError!void {
        var info: bridge_mod.MapInfo = .{};
        try self.noteOutcome(self.bridge.openMap(path, &info));
        try self.document.reload(self.allocator, self.bridge, path, info);
    }

    pub fn save(self: *Editor, path: []const u8) EditError!void {
        try self.noteOutcome(self.bridge.saveMap(path));
        self.document.path.clearRetainingCapacity();
        try self.document.path.appendSlice(self.allocator, path);
    }
};
```

- [ ] **Step 5: Run the tier**

Run: `zig build test-editor-core -Dtest-mode=run --summary all`
Expected: all tests pass, no leaks.

- [ ] **Step 6: Commit**

```bash
git add Sources/editor/core
git commit -m "feat(editor): the core's document, filled from the bridge on open

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Commands, undo and redo

**Files:**
- Create: `Sources/editor/core/history.zig`
- Modify: `Sources/editor/core/editor.zig`, `Sources/editor/core/root.zig`

**Interfaces:**
- Consumes: `Editor`, `Document` (Task 4), `Bridge` (Task 3).
- Produces (history.zig):
  ```zig
  pub const Pose = struct { x: f32, y: f32, dir: i32, player: i32 };
  pub const Command = union(enum) {
      paint: struct { tokens: std.ArrayListUnmanaged(i32) },
      add: struct { object: ObjectRecord, index: usize },
      place: struct { link_id: i32, before: Pose, after: Pose },
      delete: struct { object: ObjectRecord, index: usize },
      diplomacy: struct { player: i32, before: i32, after: i32 },
      map_type: struct { before: i32, after: i32 },
      attacking_side: struct { before: i32, after: i32 },
      pub fn deinit(self: *Command, allocator: std.mem.Allocator) void;
  };
  pub const Entry = struct { command: Command, gesture: u32 };
  pub const History = struct {
      pub fn deinit(self: *History, allocator: std.mem.Allocator) void;
      pub fn clear(self: *History, allocator: std.mem.Allocator) void;       // and marks clean
      pub fn record(self: *History, allocator: std.mem.Allocator, command: Command, gesture: u32) !void;
      pub fn top(self: *History) ?*Entry;
      pub fn markClean(self: *History) void;
      pub fn touchTop(self: *History) void;   // a merge changed the top entry
      pub fn dirty(self: *const History) bool;
      pub fn canUndo / canRedo(self: *const History) bool;
  };
  ```
- Produces (editor.zig):
  ```zig
  pub fn beginGesture(self: *Editor) u32;          // never 0
  pub fn paint(self: *Editor, cells: []const PaintCell, gesture: u32) EditError!void;
  pub fn addObject(self: *Editor, name: []const u8, x: f32, y: f32, dir: i32, player: i32) EditError!i32;
  pub fn place(self: *Editor, link_id: i32, pose: Pose, gesture: u32) EditError!void;
  pub fn delete(self: *Editor, link_id: i32) EditError!void;
  pub fn setDiplomacy(self: *Editor, player: i32, value: i32) EditError!void;
  pub fn setMapType(self: *Editor, value: i32) EditError!void;
  pub fn setAttackingSide(self: *Editor, value: i32) EditError!void;
  pub fn undo(self: *Editor) EditError!bool;       // false: nothing to undo
  pub fn redo(self: *Editor) EditError!bool;
  pub fn dirty(self: *const Editor) bool;
  selection: ?i32
  ```
  `gesture` 0 means "never merge". Two commands of the same kind and gesture merge into one entry. That is one brush drag, or one drag-move of one object.

- [ ] **Step 1: Write the failing tests**

Append to `editor.zig`:

```zig
fn openFixture(fake: *FakeBridge) !Editor {
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

test "add, undo, redo keeps one link ID and the document in step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const link = try editor.addObject("T34", 60, 60, 0, 1);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.document.find(link) == null);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(i32, 1), editor.document.find(link).?.player);
    try std.testing.expect(fake.calls.items[fake.calls.items.len - 1].kind == .restore);
}

test "delete then undo restores the same object, player and link ID" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const before = editor.document.find(1).?.*;
    try editor.delete(1);
    try std.testing.expect(editor.document.find(1) == null);
    try std.testing.expect(try editor.undo());
    const after = editor.document.find(1).?.*;
    try std.testing.expectEqual(before.player, after.player);
    try std.testing.expectEqual(@as(usize, 0), editor.document.indexOf(1).?);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(editor.document.find(1) == null);
}

test "a refused delete leaves the document and the history unchanged" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectError(error.Refused, editor.delete(2));
    try std.testing.expectEqualStrings("still referred to by bridge 0", editor.status());
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!(try editor.undo()));
}

test "place, undo, redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.place(1, .{ .x = 70, .y = 80, .dir = 4096, .player = 1 }, 0);
    try std.testing.expectEqual(@as(f32, 70), editor.document.find(1).?.x);
    _ = try editor.undo();
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try std.testing.expectEqual(@as(i32, 0), editor.document.find(1).?.dir);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(i32, 4096), editor.document.find(1).?.dir);
    try std.testing.expectError(error.Refused, editor.place(1, .{ .x = -1, .y = 0, .dir = 0, .player = 0 }, 0));
    try std.testing.expectEqual(@as(f32, 70), editor.document.find(1).?.x);
}

test "one gesture of moves is one undo step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.place(1, .{ .x = 50, .y = 40, .dir = 0, .player = 0 }, gesture);
    try editor.place(1, .{ .x = 60, .y = 40, .dir = 0, .player = 0 }, gesture);
    try editor.place(1, .{ .x = 70, .y = 40, .dir = 0, .player = 0 }, gesture);
    _ = try editor.undo();
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try std.testing.expect(!(try editor.undo()));
}

test "one gesture of paints is one undo step, undone newest first" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = 1, .y = 1, .tile = 4 }}, gesture);
    try editor.paint(&.{.{ .x = 2, .y = 1, .tile = 4 }}, gesture);
    try editor.paint(&.{.{ .x = 3, .y = 1, .tile = 9 }}, editor.beginGesture());
    _ = try editor.undo();
    try std.testing.expectEqual(@as(u8, 0), fake.tile(3, 1));
    try std.testing.expectEqual(@as(u8, 4), fake.tile(2, 1));
    _ = try editor.undo();
    try std.testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try std.testing.expectEqual(@as(u8, 0), fake.tile(2, 1));
    _ = try editor.redo();
    try std.testing.expectEqual(@as(u8, 4), fake.tile(2, 1));
}

test "diplomacy, map type and attacking side undo and redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setDiplomacy(1, 2);
    try editor.setMapType(3);
    try editor.setAttackingSide(1);
    try std.testing.expectEqual(@as(i32, 2), editor.document.diplomacy.items[1]);
    _ = try editor.undo();
    _ = try editor.undo();
    _ = try editor.undo();
    try std.testing.expectEqual(@as(i32, 1), editor.document.diplomacy.items[1]);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.map_type);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.attacking_side);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(i32, 2), editor.document.diplomacy.items[1]);
}

test "saving marks clean, and undoing past the save makes it dirty again" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setMapType(3);
    try editor.save("fixture.bzm");
    try std.testing.expect(!editor.dirty());
    _ = try editor.undo();
    try std.testing.expect(editor.dirty());
    _ = try editor.redo();
    try std.testing.expect(!editor.dirty());
    _ = try editor.undo();
    try editor.setMapType(5); // the saved state is no longer reachable
    try std.testing.expect(editor.dirty());
}

test "a new edit drops the redo branch" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setMapType(3);
    _ = try editor.undo();
    try editor.setAttackingSide(1);
    try std.testing.expect(!(try editor.redo()));
}
```

Add `pub const history = @import("history.zig");` to `root.zig`.

- [ ] **Step 2: Run the tier and watch it fail**

Run: `zig build test-editor-core -Dtest-mode=run`
Expected: compile errors for `history.zig` and the missing `Editor` methods.

- [ ] **Step 3: The history**

`Sources/editor/core/history.zig`:

```zig
//! Undo and redo. A command records what it needs to go both ways; the
//! Editor does the bridge calls, this file only keeps the stacks and knows
//! whether the map differs from the last save.
const std = @import("std");
const ObjectRecord = @import("bridge.zig").ObjectRecord;

pub const Pose = struct { x: f32, y: f32, dir: i32, player: i32 };

pub const Command = union(enum) {
    /// One bridge token per paint call of the gesture, oldest first.
    paint: struct { tokens: std.ArrayListUnmanaged(i32) = .empty },
    /// The object as added and where the document holds it, for redo.
    add: struct { object: ObjectRecord, index: usize },
    place: struct { link_id: i32, before: Pose, after: Pose },
    delete: struct { object: ObjectRecord, index: usize },
    diplomacy: struct { player: i32, before: i32, after: i32 },
    map_type: struct { before: i32, after: i32 },
    attacking_side: struct { before: i32, after: i32 },

    pub fn deinit(self: *Command, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .paint => |*p| p.tokens.deinit(allocator),
            else => {},
        }
    }
};

pub const Entry = struct { command: Command, gesture: u32 };

pub const History = struct {
    undo_stack: std.ArrayListUnmanaged(Entry) = .empty,
    redo_stack: std.ArrayListUnmanaged(Entry) = .empty,
    /// The undo depth at the last open or save; null once that state can no
    /// longer be reached by undo or redo.
    clean_depth: ?usize = 0,

    pub fn deinit(self: *History, allocator: std.mem.Allocator) void {
        self.clear(allocator);
        self.undo_stack.deinit(allocator);
        self.redo_stack.deinit(allocator);
        self.* = undefined;
    }

    pub fn clear(self: *History, allocator: std.mem.Allocator) void {
        for (self.undo_stack.items) |*entry| entry.command.deinit(allocator);
        for (self.redo_stack.items) |*entry| entry.command.deinit(allocator);
        self.undo_stack.clearRetainingCapacity();
        self.redo_stack.clearRetainingCapacity();
        self.clean_depth = 0;
    }

    /// Takes ownership of the command, even on error.
    pub fn record(self: *History, allocator: std.mem.Allocator, command: Command, gesture: u32) !void {
        var owned = command;
        errdefer owned.deinit(allocator);
        for (self.redo_stack.items) |*entry| entry.command.deinit(allocator);
        self.redo_stack.clearRetainingCapacity();
        if (self.clean_depth) |depth| {
            if (depth > self.undo_stack.items.len) self.clean_depth = null;
        }
        try self.undo_stack.append(allocator, .{ .command = owned, .gesture = gesture });
    }

    pub fn top(self: *History) ?*Entry {
        if (self.undo_stack.items.len == 0) return null;
        return &self.undo_stack.items[self.undo_stack.items.len - 1];
    }

    /// A merge changed the top entry; if that entry was the saved state, the
    /// saved state is gone.
    pub fn touchTop(self: *History) void {
        if (self.clean_depth) |depth| {
            if (depth == self.undo_stack.items.len) self.clean_depth = null;
        }
    }

    pub fn markClean(self: *History) void {
        self.clean_depth = self.undo_stack.items.len;
    }

    pub fn dirty(self: *const History) bool {
        return self.clean_depth == null or self.clean_depth.? != self.undo_stack.items.len;
    }

    pub fn canUndo(self: *const History) bool {
        return self.undo_stack.items.len != 0;
    }

    pub fn canRedo(self: *const History) bool {
        return self.redo_stack.items.len != 0;
    }
};

test "the clean mark follows the undo depth" {
    var history: History = .{};
    defer history.deinit(std.testing.allocator);
    try std.testing.expect(!history.dirty());
    try history.record(std.testing.allocator, .{ .map_type = .{ .before = 0, .after = 1 } }, 0);
    try std.testing.expect(history.dirty());
    history.markClean();
    try std.testing.expect(!history.dirty());
    history.touchTop();
    try std.testing.expect(history.dirty());
}
```

- [ ] **Step 4: The commands in the editor**

Add to `Editor` (imports: `const history_mod = @import("history.zig"); const Pose = history_mod.Pose; const Command = history_mod.Command; const PaintCell = bridge_mod.PaintCell;`):

```zig
    history: history_mod.History = .{},
    selection: ?i32 = null,
    next_gesture: u32 = 1,
```

In `deinit`, add `self.history.deinit(self.allocator);`. At the end of `open`, add `self.history.clear(self.allocator); self.selection = null;`. At the end of `save`, add `self.history.markClean();`. Then add:

```zig
    pub fn dirty(self: *const Editor) bool {
        return self.history.dirty();
    }

    /// A fresh key for one press-drag-release. Never 0, which means "never merge".
    pub fn beginGesture(self: *Editor) u32 {
        const gesture = self.next_gesture;
        self.next_gesture +%= 1;
        if (self.next_gesture == 0) self.next_gesture = 1;
        return gesture;
    }

    fn mergeable(self: *Editor, gesture: u32, tag: std.meta.Tag(Command)) ?*history_mod.Entry {
        if (gesture == 0) return null;
        const entry = self.history.top() orelse return null;
        if (entry.gesture != gesture or std.meta.activeTag(entry.command) != tag) return null;
        return entry;
    }

    pub fn paint(self: *Editor, cells: []const PaintCell, gesture: u32) EditError!void {
        if (cells.len == 0) return;
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.paint(cells, &token));
        if (self.mergeable(gesture, .paint)) |entry| {
            try entry.command.paint.tokens.append(self.allocator, token);
            self.history.touchTop();
            return;
        }
        var tokens: std.ArrayListUnmanaged(i32) = .empty;
        tokens.append(self.allocator, token) catch |err| {
            tokens.deinit(self.allocator);
            return err;
        };
        // record owns the command from here, on error too.
        try self.history.record(self.allocator, .{ .paint = .{ .tokens = tokens } }, gesture);
    }

    pub fn addObject(self: *Editor, name: []const u8, x: f32, y: f32, dir: i32, player: i32) EditError!i32 {
        var link_id: i32 = -1;
        try self.noteOutcome(self.bridge.addObject(name, x, y, dir, player, &link_id));
        var object: ObjectRecord = .{ .link_id = link_id, .x = x, .y = y, .dir = dir, .player = player };
        object.setName(name);
        const index = self.document.objects.items.len;
        try self.document.objects.append(self.allocator, object);
        try self.history.record(self.allocator, .{ .add = .{ .object = object, .index = index } }, 0);
        return link_id;
    }

    pub fn place(self: *Editor, link_id: i32, pose: Pose, gesture: u32) EditError!void {
        const object = self.document.find(link_id) orelse return error.Failed;
        const before: Pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
        if (std.meta.eql(before, pose)) return;
        try self.noteOutcome(self.bridge.placeObject(link_id, pose.x, pose.y, pose.dir, pose.player));
        applyPose(object, pose);
        if (self.mergeable(gesture, .place)) |entry| {
            if (entry.command.place.link_id == link_id) {
                entry.command.place.after = pose;
                self.history.touchTop();
                return;
            }
        }
        try self.history.record(self.allocator, .{ .place = .{ .link_id = link_id, .before = before, .after = pose } }, gesture);
    }

    pub fn delete(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        const object = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
        try self.history.record(self.allocator, .{ .delete = .{ .object = object, .index = index } }, 0);
    }

    pub fn setDiplomacy(self: *Editor, player: i32, value: i32) EditError!void {
        if (player < 0 or @as(usize, @intCast(player)) >= self.document.diplomacy.items.len) return error.Failed;
        const slot = &self.document.diplomacy.items[@intCast(player)];
        if (slot.* == value) return;
        try self.noteOutcome(self.bridge.setDiplomacy(player, value));
        const before = slot.*;
        slot.* = value;
        try self.history.record(self.allocator, .{ .diplomacy = .{ .player = player, .before = before, .after = value } }, 0);
    }

    pub fn setMapType(self: *Editor, value: i32) EditError!void {
        const before = self.document.info.map_type;
        if (before == value) return;
        try self.noteOutcome(self.bridge.setMapType(value));
        self.document.info.map_type = value;
        try self.history.record(self.allocator, .{ .map_type = .{ .before = before, .after = value } }, 0);
    }

    pub fn setAttackingSide(self: *Editor, value: i32) EditError!void {
        const before = self.document.info.attacking_side;
        if (before == value) return;
        try self.noteOutcome(self.bridge.setAttackingSide(value));
        self.document.info.attacking_side = value;
        try self.history.record(self.allocator, .{ .attacking_side = .{ .before = before, .after = value } }, 0);
    }

    fn applyPose(object: *ObjectRecord, pose: Pose) void {
        object.x = pose.x;
        object.y = pose.y;
        object.dir = pose.dir;
        object.player = pose.player;
    }

    /// Runs a command backwards (`forwards` false) or forwards again. The
    /// bridge keeps its own order, so every call here is one it expects; a
    /// refusal means the two have drifted, which is a failure, not a note.
    fn replay(self: *Editor, command: *Command, forwards: bool) EditError!void {
        switch (command.*) {
            .paint => |p| {
                if (forwards) {
                    for (p.tokens.items) |token| try self.noteOutcome(self.bridge.redoPaint(token));
                } else {
                    var index = p.tokens.items.len;
                    while (index != 0) {
                        index -= 1;
                        try self.noteOutcome(self.bridge.undoPaint(p.tokens.items[index]));
                    }
                }
            },
            .add => |a| if (forwards) try self.restoreInto(a.object, a.index) else try self.removeFrom(a.object.link_id),
            .delete => |d| if (forwards) try self.removeFrom(d.object.link_id) else try self.restoreInto(d.object, d.index),
            .place => |p| {
                const pose = if (forwards) p.after else p.before;
                try self.noteOutcome(self.bridge.placeObject(p.link_id, pose.x, pose.y, pose.dir, pose.player));
                applyPose(self.document.find(p.link_id) orelse return error.Failed, pose);
            },
            .diplomacy => |d| {
                const value = if (forwards) d.after else d.before;
                try self.noteOutcome(self.bridge.setDiplomacy(d.player, value));
                self.document.diplomacy.items[@intCast(d.player)] = value;
            },
            .map_type => |m| {
                const value = if (forwards) m.after else m.before;
                try self.noteOutcome(self.bridge.setMapType(value));
                self.document.info.map_type = value;
            },
            .attacking_side => |s| {
                const value = if (forwards) s.after else s.before;
                try self.noteOutcome(self.bridge.setAttackingSide(value));
                self.document.info.attacking_side = value;
            },
        }
    }

    fn removeFrom(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        _ = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
    }

    fn restoreInto(self: *Editor, object: ObjectRecord, index: usize) EditError!void {
        try self.noteOutcome(self.bridge.restoreObject(object.link_id));
        try self.document.objects.insert(self.allocator, @min(index, self.document.objects.items.len), object);
    }

    fn drifted(err: EditError) EditError {
        return if (err == error.Refused) error.Failed else err;
    }

    /// False when there is nothing to undo. On a failure the entry stays
    /// where it was and the status line says why; the map should be reopened.
    pub fn undo(self: *Editor) EditError!bool {
        const count = self.history.undo_stack.items.len;
        if (count == 0) return false;
        // Room first: once the bridge has undone it, the entry must not be
        // lost to an allocation failure.
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.undo_stack.items[count - 1];
        self.replay(&entry.command, false) catch |err| return drifted(err);
        _ = self.history.undo_stack.pop();
        self.history.redo_stack.appendAssumeCapacity(entry);
        return true;
    }

    pub fn redo(self: *Editor) EditError!bool {
        const count = self.history.redo_stack.items.len;
        if (count == 0) return false;
        try self.history.undo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.redo_stack.items[count - 1];
        self.replay(&entry.command, true) catch |err| return drifted(err);
        _ = self.history.redo_stack.pop();
        self.history.undo_stack.appendAssumeCapacity(entry);
        return true;
    }
```

`ObjectRecord` is already imported in `editor.zig` (Task 4, Step 1).

- [ ] **Step 5: Run the tier**

Run: `zig build test-editor-core -Dtest-mode=run --summary all`
Expected: all tests pass, no leaks.

- [ ] **Step 6: Commit**

```bash
git add Sources/editor/core
git commit -m "feat(editor): every edit is a command with undo and redo, and one drag is one step

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Tools

**Files:**
- Create: `Sources/editor/core/tools.zig`
- Modify: `Sources/editor/core/editor.zig` (`resolve`), `Sources/editor/core/root.zig`

**Interfaces:**
- Consumes: `Editor` with its edits and `beginGesture` (Task 5), `Bridge.screenToWorld`, `worldToTile`, `objectAt`.
- Produces (tools.zig):
  ```zig
  pub const Pointer = struct { world_x: f32, world_y: f32, tile: ?[2]i32 = null, object: ?i32 = null };
  pub const Key = enum { delete, rotate_left, rotate_right };
  pub const Event = union(enum) { press: Pointer, drag: Pointer, release: Pointer, key: Key };
  pub const rotate_step: i32 = 4096;   // 1/16 turn of the engine's 65536
  pub const Brush = struct { tile: u8, radius: i32, pub fn handle(*Brush, *Editor, Event) EditError!void; pub fn deinit(*Brush, Allocator) void };
  pub const Placer = struct { name: []const u8, dir: i32, player: i32, pub fn handle(*Placer, *Editor, Event) EditError!void };
  pub const Selector = struct { pub fn handle(*Selector, *Editor, Event) EditError!void };
  ```
- Produces (editor.zig): `pub fn resolve(self: *Editor, sx: f32, sy: f32) EditError!tools.Pointer`. It returns `Refused` when the point is off the terrain.

The diplomacy editor's model is the document's `diplomacy` and `info` plus the three setters from Task 5. A panel reads the one and calls the other, so it needs no type of its own.

- [ ] **Step 1: Write the failing tests**

Bottom of `Sources/editor/core/tools.zig`:

```zig
const testing = std.testing;
const testFixture = editor_mod.testFixture;

fn opened(fake: *fake_mod.FakeBridge) !Editor {
    var editor = Editor.init(testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

fn at(editor: *Editor, x: f32, y: f32) !Pointer {
    return editor.resolve(x, y);
}

test "a brush drag paints the cells it crossed, once each, as one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 7, .radius = 0 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 40, 40) });   // cell 1,1
    try brush.handle(&editor, .{ .drag = try at(&editor, 45, 40) });    // still 1,1: no paint
    try brush.handle(&editor, .{ .drag = try at(&editor, 70, 40) });    // cell 2,1
    try brush.handle(&editor, .{ .release = try at(&editor, 70, 40) });
    try testing.expectEqual(@as(u8, 7), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 7), fake.tile(2, 1));
    var paints: usize = 0;
    for (fake.calls.items) |call| {
        if (call.kind == .paint) paints += 1;
    }
    try testing.expectEqual(@as(usize, 2), paints);
    _ = try editor.undo();
    try testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), fake.tile(2, 1));
}

test "a brush with a radius paints a square, clipped to the map" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 3, .radius = 1 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 5, 5) });     // cell 0,0 at the corner
    try brush.handle(&editor, .{ .release = try at(&editor, 5, 5) });
    try testing.expectEqual(@as(u8, 3), fake.tile(0, 0));
    try testing.expectEqual(@as(u8, 3), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), fake.tile(2, 2));
}

test "the placer adds and selects" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var placer: Placer = .{ .name = "T34", .dir = 0, .player = 1 };
    try placer.handle(&editor, .{ .press = try at(&editor, 200, 200) });
    const link = editor.selection.?;
    try testing.expectEqual(@as(i32, 1), editor.document.find(link).?.player);
}

test "drag-move moves, keeps the grab offset, and is one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 45, 40) });  // grabs T34 at 40,40, 5 to its right
    try testing.expectEqual(@as(?i32, 1), editor.selection);
    try selector.handle(&editor, .{ .drag = try at(&editor, 65, 60) });
    try selector.handle(&editor, .{ .drag = try at(&editor, 85, 80) });
    try selector.handle(&editor, .{ .release = try at(&editor, 85, 80) });
    try testing.expectEqual(@as(f32, 80), editor.document.find(1).?.x);
    try testing.expectEqual(@as(f32, 80), editor.document.find(1).?.y);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
}

test "a refused position mid-drag is skipped, not the end of the drag" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .drag = .{ .world_x = 2, .world_y = 40 } });  // 40 - 38 would be fine
    try selector.handle(&editor, .{ .drag = .{ .world_x = -30, .world_y = 40 } }); // off the map: refused
    try testing.expectEqual(@as(f32, 2), editor.document.find(1).?.x);
    try selector.handle(&editor, .{ .drag = .{ .world_x = 20, .world_y = 40 } });
    try testing.expectEqual(@as(f32, 20), editor.document.find(1).?.x);
}

test "delete then undo restores the same object; a refused delete keeps the selection" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 100, 40) });   // the referenced span
    try selector.handle(&editor, .{ .release = try at(&editor, 100, 40) });
    try testing.expectError(error.Refused, selector.handle(&editor, .{ .key = .delete }));
    try testing.expectEqual(@as(?i32, 2), editor.selection);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(?i32, null), editor.selection);
    _ = try editor.undo();
    try testing.expectEqual(@as(i32, 0), editor.document.find(1).?.player);
}

test "rotate turns by a step and wraps" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .key = .rotate_left });
    try testing.expectEqual(@as(i32, 65536 - rotate_step), editor.document.find(1).?.dir);
    try selector.handle(&editor, .{ .key = .rotate_right });
    try testing.expectEqual(@as(i32, 0), editor.document.find(1).?.dir);
}

test "a press on nothing clears the selection" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .press = try at(&editor, 220, 220) });
    try testing.expectEqual(@as(?i32, null), editor.selection);
}
```

Add `pub const tools = @import("tools.zig");` to `root.zig`.

- [ ] **Step 2: Run the tier and watch it fail**

Run: `zig build test-editor-core -Dtest-mode=run`
Expected: compile errors for `tools.zig`, `Editor.resolve`.

- [ ] **Step 3: `Editor.resolve`**

In `editor.zig` (add `const tools = @import("tools.zig");`):

```zig
    /// A screen point as the tools want it: the world point, its tile, and the
    /// object under it. Refused when the point is off the terrain; a point on
    /// the terrain but past the map's edge has no tile, and a point over no
    /// object has no object - neither is an error.
    pub fn resolve(self: *Editor, sx: f32, sy: f32) EditError!tools.Pointer {
        var pointer: tools.Pointer = .{ .world_x = 0, .world_y = 0 };
        try bridge_mod.check(self.bridge.screenToWorld(sx, sy, &pointer.world_x, &pointer.world_y));
        var tx: i32 = 0;
        var ty: i32 = 0;
        if (self.bridge.worldToTile(pointer.world_x, pointer.world_y, &tx, &ty) == .ok) pointer.tile = .{ tx, ty };
        var link_id: i32 = -1;
        if (self.bridge.objectAt(sx, sy, &link_id) == .ok) pointer.object = link_id;
        return pointer;
    }
```

- [ ] **Step 4: The tools**

Top of `Sources/editor/core/tools.zig`:

```zig
//! The tools turn plain input - a pointer already resolved to world, tile and
//! object, and a few keys - into Editor calls. They hold only what a gesture
//! needs between events; everything else is in the Editor.
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const PaintCell = bridge_mod.PaintCell;

pub const Pointer = struct { world_x: f32, world_y: f32, tile: ?[2]i32 = null, object: ?i32 = null };
pub const Key = enum { delete, rotate_left, rotate_right };
pub const Event = union(enum) { press: Pointer, drag: Pointer, release: Pointer, key: Key };

/// A sixteenth of a turn. Directions are the engine's: 65536 is a full turn
/// (SEngineObjectState::wDir is a WORD).
pub const rotate_step: i32 = 4096;
const full_turn: i32 = 65536;

pub const Brush = struct {
    tile: u8,
    /// 0 is one cell, 1 a 3x3 square, and so on.
    radius: i32 = 0,
    gesture: u32 = 0,
    painted: std.AutoHashMapUnmanaged([2]i32, void) = .empty,

    pub fn deinit(self: *Brush, allocator: std.mem.Allocator) void {
        self.painted.deinit(allocator);
        self.* = undefined;
    }

    pub fn handle(self: *Brush, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                self.gesture = editor.beginGesture();
                self.painted.clearRetainingCapacity();
                try self.stamp(editor, pointer);
            },
            .drag => |pointer| if (self.gesture != 0) try self.stamp(editor, pointer),
            .release => {
                self.gesture = 0;
                self.painted.clearRetainingCapacity();
            },
            .key => {},
        }
    }

    /// Paints the cells under the brush that this stroke has not painted yet,
    /// in one bridge call: the terrain function runs once per call, and each
    /// run is an undo record in the bridge.
    fn stamp(self: *Brush, editor: *Editor, pointer: Pointer) EditError!void {
        const centre = pointer.tile orelse return;
        const allocator = editor.allocator;
        var cells: std.ArrayListUnmanaged(PaintCell) = .empty;
        defer cells.deinit(allocator);
        const width = editor.document.info.width_tiles;
        const height = editor.document.info.height_tiles;
        var y = centre[1] - self.radius;
        while (y <= centre[1] + self.radius) : (y += 1) {
            var x = centre[0] - self.radius;
            while (x <= centre[0] + self.radius) : (x += 1) {
                if (x < 0 or y < 0 or x >= width or y >= height) continue;
                const cell = [2]i32{ x, y };
                if (self.painted.contains(cell)) continue;
                try self.painted.put(allocator, cell, {});
                try cells.append(allocator, .{ .x = x, .y = y, .tile = self.tile });
            }
        }
        try editor.paint(cells.items, self.gesture);
    }
};

pub const Placer = struct {
    name: []const u8,
    dir: i32 = 0,
    player: i32 = 0,

    pub fn handle(self: *Placer, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| editor.selection = try editor.addObject(self.name, pointer.world_x, pointer.world_y, self.dir, self.player),
            else => {},
        }
    }
};

pub const Selector = struct {
    gesture: u32 = 0,
    grab_x: f32 = 0,
    grab_y: f32 = 0,

    pub fn handle(self: *Selector, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                const link_id = pointer.object orelse {
                    editor.selection = null;
                    self.gesture = 0;
                    return;
                };
                const object = editor.document.find(link_id) orelse return;
                editor.selection = link_id;
                self.grab_x = object.x - pointer.world_x;
                self.grab_y = object.y - pointer.world_y;
                self.gesture = editor.beginGesture();
            },
            .drag => |pointer| {
                if (self.gesture == 0) return;
                const link_id = editor.selection orelse return;
                const object = editor.document.find(link_id) orelse return;
                const pose: editor_mod.Pose = .{
                    .x = pointer.world_x + self.grab_x,
                    .y = pointer.world_y + self.grab_y,
                    .dir = object.dir,
                    .player = object.player,
                };
                // A position the engine will not take is skipped: the object
                // stays at the last one it took and the drag goes on. The
                // status line says why.
                editor.place(link_id, pose, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .release => self.gesture = 0,
            .key => |key| {
                const link_id = editor.selection orelse return;
                switch (key) {
                    .delete => try editor.delete(link_id),
                    .rotate_left, .rotate_right => {
                        const object = editor.document.find(link_id) orelse return;
                        const step: i32 = if (key == .rotate_left) -rotate_step else rotate_step;
                        try editor.place(link_id, .{
                            .x = object.x,
                            .y = object.y,
                            .dir = @mod(object.dir + step, full_turn),
                            .player = object.player,
                        }, 0);
                    },
                }
            },
        }
    }
};
```

Export `Pose` from `editor.zig` (`pub const Pose = history_mod.Pose;`) so `tools.zig` can name it. In the test "a refused position mid-drag", the press is at the object's own centre, so the grab offset is zero. That makes the scripted world points the object's positions.

- [ ] **Step 5: Run the tier**

Run: `zig build test-editor-core -Dtest-mode=run --summary all`
Expected: all tests pass, no leaks.

- [ ] **Step 6: Commit and check CI**

```bash
git add Sources/editor/core
git commit -m "feat(editor): the brush, placer and selector tools

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

Push and run the workflow (`gh workflow run "Cross-platform validation" --ref feat/portable-map-editor`; it does not trigger on branch pushes). Expected: "Editor core tier" green in all six jobs, the MinGW job included.

---

### Task 7: Objects drawn, and the object under the cursor

**Files:**
- Create: `Sources/src/EditorBridge/world.h`, `Sources/src/EditorBridge/world.cpp`
- Modify: `Sources/src/EditorBridge/session.h`, `session.cpp`, `bridge.h`, `bridge.cpp`
- Modify: `build.zig` (`addEditorBridge`, ~3564; the editor bridge test's link list)
- Test: `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `CWorldBase` (`Common/WorldBase.h`): `Init( ISingleton* )` (`WorldBase.cpp:207`), `Update( const NTimer::STime& )` (h:218), `Clear()` (h:217), `FindByVis` (h:234), `IsExistByVis` (h:239), pure `ResetSelection( SMapObject* )` (h:159). Also `IScene::Pick`, `SetGlobalVar`, and `SEditorSession::byLinkID`.
- Produces (bridge.h):
  ```c
  /* The object under a screen point, as a link ID. Bridges and entrenchments
     are passed over, as the MFC editor passes them over
     (TemplateEditorFrame1.cpp:3384-3400): they are edited as wholes in M2.
     BK_EDITOR_REFUSED means nothing pickable is there. */
  BkEditorStatus BkEditorObjectAt( BkEditorSession *session, float sx, float sy, int *out_link_id );
  ```

- [ ] **Step 1: Measure the two unknowns before writing anything**

The research names two things that decide whether option A works as written. Settle both first, in a scratch test of the engine tier, before building on either.

1. **Does GameTT load in the bridge's process?** `CWorldBase::CreateMapObject` creates `MISSION_MO_*` objects, and only GameTT's factory registers them (`GameTT/MissionObjectFactory.cpp:74-79`). Run the engine tier and read its `Loaded module ...` lines. Plan 3's CI log lists "Main game logic", which is GameTT's descriptor name. Confirm it with `grep -an "Main game logic" Sources/src/GameTT/*.cpp`.
2. **Do GameTT's objects survive `CWorldBase`'s `dynamic_cast`s in the bridge's copy of `Common`?** `grep -an dynamic_cast Sources/src/Common/WorldBase.cpp` lists them (the research cites `:776`). Each one casts an object created in GameTT to a class that `Common` also compiles. On macOS those are two typeinfo copies, and the cast returns null (see Global Constraints). Write the smallest check that exercises one: open `SHIPPED_MAP` with the world in place (Steps 2-4), then count the scene's objects after one `Update` with `IScene::Pick` over the whole screen. A squad's soldiers, which reach their `CMOSquad` through such a cast (`AIUpdateSquads`, `WorldBase.cpp:1361`), are the case to look at.

Write down what you measured in this plan, under this step, the way plan 3's "Measured" notes are written. If a cast does fail, fix it the way plan 3 fixed `ITerrain`. Add a virtual accessor to the interface the object already has, returning `this` as the needed type from inside the module that defines it. Do not add a `static_cast` that assumes the type.

- [ ] **Step 2: Link Common into the bridge**

In `build.zig`, `addEditorBridge` adds `Sources/src/Common` as an include path only. Add `Sources/src/EditorBridge/world.cpp` to its sources. Link the static `Common` library into it the way `addLegacyProjectDll` links it for GameTT (`build.zig:1815`), then pass the library in from the call site. Run `zig test tools/zig/build_hermeticity_test.zig`.

- [ ] **Step 3: `CEditorWorld`**

`Sources/src/EditorBridge/world.h`:

```cpp
#ifndef __EDITOR_BRIDGE_WORLD_H__
#define __EDITOR_BRIDGE_WORLD_H__
#include "../Common/WorldBase.h"

// The engine's own object layer, as the MFC editor uses it
// (TemplateEditorFrame1.h:75): CWorldBase::Update turns the AI's notifications
// into map objects with visuals in the scene, which is what draws a unit and
// what IScene::Pick finds. The editor has no selection in the game's sense, so
// the one pure virtual does nothing.
class CEditorWorld : public CWorldBase
{
public:
	virtual void ResetSelection( SMapObject *pMO ) {  }
};

#endif // __EDITOR_BRIDGE_WORLD_H__
```

Match the exact declaration of `ResetSelection` in `WorldBase.h:159`, calling convention included.

- [ ] **Step 4: The world in the session**

In `SEditorSession`, add `CEditorWorld *pWorld;` (initialised to 0, deleted in `BkEditorStop`) and a reverse map, `std::unordered_map<IRefCount*, int> linkByAI;`.

- In `BkEditorStart`, once the engine is up: `SetGlobalVar( "editor", 1 );`. Without it, `CWorldBase::Update` runs the AI handlers only when the segment timer is due (`WorldBase.cpp:574`). Then `pSession->pWorld = new CEditorWorld; pSession->pWorld->Init( GetSingletonGlobal() );`.
- In `OpenMapIntoSession`: before building the new map, call `pWorld->Clear()`. After the objects are placed, run one world update. Then rebuild `linkByAI` from `byLinkID`.
- After every successful edit that places, moves, turns or removes an engine object (`AddObjectToSession`, `PlaceObjectInSession`, `DeleteObjectFromSession`, `RestoreObjectInSession`), run one world update and keep `linkByAI` in step.
- The update is what the MFC editor does after each edit (`TemplateEditorFrame1.cpp:697-698`): advance the game timer, then `pWorld->Update( pTimer->GetGameTime() )`. Find the portable timer call the game uses with `grep -an "GetSingleton<IGameTimer>\|pTimer->Update" Sources/src/Game Sources/src/Main Sources/src/GameTT`, and use the same one.

- [ ] **Step 5: Write the failing test**

```cpp
// A camera put on an object answers, at the middle of the screen, with that
// object - the picking half of "the camera is on cell 83,36 and the middle of
// the screen is 83,36".
static void TestObjectUnderTheCursor( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	NMapFile::Read( SHIPPED_MAP, &map, &szError );
	int nPicked = 0, nTried = 0;
	for ( size_t i = 0; i < map.objects.size() && nTried < 20; ++i )
	{
		BkEditorObjectState state;
		if ( BkEditorEngineObjectState( pSession, map.objects[i].link.nLinkID, &state ) != BK_EDITOR_OK )
			continue;
		++nTried;
		BkEditorSetCamera( pSession, state.x, state.y );
		BkEditorFrame( pSession );
		int nLinkID = -1;
		if ( BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f, &nLinkID ) == BK_EDITOR_OK &&
		     nLinkID == map.objects[i].link.nLinkID )
			++nPicked;
	}
	printf( "editor-bridge: %d of %d objects picked at the middle of the screen\n", nPicked, nTried );
	// Not every one: a small object can sit behind a big neighbour, and that
	// neighbour is a right answer too. Most of them is the test.
	Check( nTried > 0 && nPicked * 2 > nTried, "the object under the camera is the one picked, for most objects" );

	int nNothing = -1;
	BkEditorSetCamera( pSession, 16.0f, 16.0f );
	BkEditorFrame( pSession );
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, &nNothing ) != BK_EDITOR_FAILED, "a point over nothing is an answer, not a failure" );
}
```

Pass the window size the harness created (640x480, `editor_bridge_test.cpp` `main`). Call it after `TestCatalogueCameraAndFrame`. The "most of them" threshold is a first guess: print the measured ratio, record it in this plan, and tighten the check to just under it.

- [ ] **Step 6: `BkEditorObjectAt`**

In `session.cpp`:

```cpp
bool ObjectAt( SEditorSession *pSession, float sx, float sy, int *pnLinkID, bool *pbRefused )
{
	*pbRefused = false;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 || pSession->pWorld == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	// The MFC editor's pick (TemplateEditorFrame1.cpp:3384-3400), without the
	// entrenchment exception it makes for a move already under way.
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nCount = 0;
	pScene->Pick( CVec2( sx, sy ), &pObjects, &nCount, SGVOGT_UNKNOWN );
	for ( int i = 0; i < nCount; ++i )
	{
		IVisObj *pVisObj = pObjects[i].first;
		if ( !pSession->pWorld->IsExistByVis( pVisObj ) )
			continue;
		SMapObject *pMapObject = pSession->pWorld->FindByVis( pVisObj );
		if ( pMapObject == 0 || pMapObject->pDesc == 0 )
			continue;
		const EObjGameType eType = pMapObject->pDesc->eGameType;
		if ( eType == SGVOGT_BRIDGE || eType == SGVOGT_ENTRENCHMENT )
			continue;
		std::unordered_map<IRefCount*, int>::const_iterator it = pSession->linkByAI.find( pMapObject->pAIObj );
		if ( it == pSession->linkByAI.end() )
			continue;
		*pnLinkID = it->second;
		return true;
	}
	pSession->szMessage = "nothing to pick there";
	*pbRefused = true;
	return false;
}
```

Take `Pick`'s exact signature, and the names of `SMapObject`'s descriptor and AI-object members, from `Scene/Scene.h` and `Common/WorldBase.h`. The research did not quote them, and the MFC editor's call at `TemplateEditorFrame1.cpp:3384` is the model to copy. The entry point in `bridge.cpp` follows `BkEditorDeleteObject`'s shape and checks `pnLinkID` for null (`BK_EDITOR_BAD_ARGUMENT`).

- [ ] **Step 7: Run the tier**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the new "objects picked" line, then `editor-bridge: PASS`. Save a frame as well. The existing capture path writes to `zig-out/local-test`; if it does not, add a readback of one frame there. Look at it to check that units and buildings are drawn, not only terrain. The pick ratio says the layer exists; the picture says it draws.

- [ ] **Step 8: Commit, push, and check both GPU runners**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp build.zig docs/superpowers/plans/2026-09-24-map-editor-04-editor-core.md
git commit -m "feat(editor): objects are drawn through CWorldBase, and a screen point picks one

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

Run the workflow and read both engine-tier logs, not only the job colour: the macOS arm64 log and the Windows-MSVC one. Windows builds debug with asserts live, so this task's new `Common` code may trip asserts that macOS never evaluates. They now fail in seconds with the file and line on stderr. Fix each one by guarding the case, not by deleting a check that protects a dereference.

---

## Self-review notes

- **Spec coverage, "Editor core":** document (Task 4), commands with do and undo for paint, add, move, rotate, delete, set player and set diplomacy (Task 5; player is a field of `place`), undo stack with redo and merging (Task 5), tools (Task 6), bridge interface with a fake that records calls (Task 3). "Delete records the whole object and remaps the engine id when undo re-adds it": the bridge's restore keeps the link ID, so there is nothing to remap (see "Decisions").
- **Spec coverage, "Testing → Core":** every command round-trips (Task 5), brush drag, drag-move, and delete-undo keeping object, player and link ID (Task 6), diplomacy (Task 5), and a refused delete leaving document and history unchanged (Task 5).
- **Spec coverage, "Picking and camera":** `BkEditorObjectAt` (Task 7).
- **Left for plan 5:** the Zig adapter over the C ABI, and an engine-tier run of the core against the real bridge. The fake's rules are pinned by Task 3's tests against what Tasks 1-2 measured on the real one, which is what keeps the two honest until then.

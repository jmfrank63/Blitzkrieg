# Map Editor Plan 3: Engine Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Start the engine on a window the editor owns, build a map's engine state from the snapshot plan 2 reads, edit terrain and objects through the engine's own editor interfaces, and save the result — all behind a flat C ABI with no C++ exception crossing it.

**Architecture:** `NMain::Initialize` grows a portable sibling that takes a `GFXNativeWindow` instead of an `HWND`, and the game switches to it. A new C++ library, `Sources/src/EditorBridge`, exposes an `extern "C"` API over integer handles: start and stop, open a map, the object catalogue, terrain and object edits, picking, camera, frame, and save. It owns the snapshot plan 2 gave it, keeps a working copy with frame indices unpacked, and records every engine object against the snapshot object it came from by link ID. Saving is the snapshot with the overlay laid over it, which is `NMapOverlay` again — the bridge adds no second implementation of the save rules.

**Tech Stack:** C++17, the engine statics plus Scene, AILogic and GFXGPU, Zig 0.16 build, SDL3 for the window.

**Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (sections "Engine bridge", "Startup contract", "Building the engine state", "Saving: the snapshot and the overlay", "Terrain edits", "Testing → Test tiers and the M1 CI gate").

**Research:** `zig-out/local-test/bridge-research.md` — the measured call sites, interface signatures and traps. Read it before Task 2; it is the difference between this plan and guesswork.

## The M1 plan series

1. **Overlay spike** (landed, `0309dd8ff`): GFXGPU overlay hook, frame capture, vendored ImGui, the spike program.
2. **Map files** (landed): `Sources/src/MapFile`, the data-only startup, the equivalence comparator, the snapshot overlay, the terrain function, the map-file tier on five CI targets.
3. **Engine bridge** (this plan): `NMain::InitializeWithWindow`, `Sources/src/EditorBridge`, building the engine state, editing calls, picking, camera, the engine test tier.
4. **Editor core:** Zig document, commands, undo and redo, tools, fake bridge, core tests on all six targets.
5. **Editor app:** window, frame loop, ImGui panels, `BK_EDITOR_AUTO`, save, autosave, test-launch, `install-map-editor`, packaging.

## Global Constraints

- All work happens on branch `feat/portable-map-editor` in the worktree `.worktrees/map-editor`. Never commit in the main checkout.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`.
- Never run `zig fmt` on `build.zig` or gate on `zig fmt --check` for it; edit it by hand.
- **Run `zig test tools/zig/build_hermeticity_test.zig` before pushing any `build.zig` change.** It token-matches the whole file against shell and build-tool names, several of which are ordinary English verbs; a comment containing one fails every target. It takes a second.
- Test artifacts go under `zig-out/local-test/`, never `/tmp`.
- Build on macOS with `zig build <step> -Dtarget=aarch64-macos`. Thousands of libc header errors (FP_ZERO, ldiv_t) mean the macOS SDK lookup is broken (`xcode-select`), not `build.zig`.
- **A new C++ executable copies `gfxgpu-factory-test`'s module configuration** (build.zig ~1940): `addProjectIncludePaths`, `addMsvcIncludePaths`, `addLinuxCxxIncludePaths`, `addMsvcLibraryPaths`, `addMacosSysrootPaths`, `linkMsvcRuntime`. Never `module.linkSystemLibrary("stdc++")` (two standard libraries — `std_abs.h` conflicts on Linux) and never `.link_libc = true` (two CRTs — duplicate `_cexit` on MSVC). Engine cppflags on every target. The TU opens with `#include "StdAfx.h"` and engine headers and nothing else.
- **For a Windows run step:** install every shared library in the import chain and `run.addPathDir(b.path("zig-out/bin"))`. `StreamIO` imports `StreamIOOptionsAbi` *and* `PlatformRuntime`; ELF and Mach-O find them by rpath, so a missing one is green everywhere except Windows, where it appears as exit code 53 (`STATUS_DLL_NOT_FOUND` truncated).
- **CI checks out sparsely.** Any data the tier reads must be added to the `sparse-checkout` list of every job that runs it, and the test must fail loudly when it finds nothing. `Data/Maps`, `Data/Terrain/sets/*/tileset.xml` and `.../crosset.xml` are already listed.
- `NI_ASSERT_T` compiles away in release. Never let an assert be the only thing between a null pointer and a dereference.
- No C++ exception may cross the C ABI: every entry point wraps its body in `try`/`catch(...)` and returns a status.

---

## File Structure

| File | Responsibility |
|---|---|
| `Sources/src/Main/Initialization.cpp` | `NMain::InitializeWithWindow` holds the body; the `HWND` form forwards. |
| `Sources/src/Main/iMain.h` | Declares the new entry point. |
| `Sources/src/Game/GameMain.cpp` | The game calls it and drops its `HWND` cast. |
| `Sources/src/EditorBridge/bridge.h` | The C ABI: structs, status codes, entry points. Nothing else includes engine headers. |
| `Sources/src/EditorBridge/bridge.cpp` | Startup, shutdown, the session handle, the exception firewall. |
| `Sources/src/EditorBridge/session.h/.cpp` | One editing session: snapshot, working copy, engine state, link-ID map. |
| `Sources/src/EditorBridge/catalogue.cpp` | `IObjectsDB::GetAllDescs()` as plain records. |
| `tools/zig/editor_bridge_test.cpp` | The engine tier: opens maps, edits, saves, compares. |
| `build.zig` | `addEditorBridge` library, `addEditorBridgeTest`, step `test-editor-bridge`. |
| `.github/workflows/cross-platform.yml` | The step, reporting "skipped: no GPU device" where there is none. |

---

### Task 1: A portable startup entry point

**Files:**
- Modify: `Sources/src/Main/Initialization.cpp:65`
- Modify: `Sources/src/Main/iMain.h:73`
- Modify: `Sources/src/Game/GameMain.cpp:625`

**Interfaces:**
- Consumes: `IGFX::Init( const char*, GFXNativeWindow )`.
- Produces:
  ```cpp
  namespace NMain
  {
      // The body of Initialize, taking the window the renderer actually wants.
      bool STDCALL InitializeWithWindow( GFXNativeWindow window );
      // Kept for source compatibility; forwards hWnd3D and ignores the rest,
      // which it already did.
      bool STDCALL Initialize( HWND hWnd3D, HWND hWndInput, HWND hWndSound, bool bGame );
  }
  ```

- [ ] **Step 1: Write the failing test**

Add to `tools/zig/map_file_test.cpp` — the data-only tier already runs on five targets and can prove the signature exists and links without a window:

```cpp
// Plan 3 Task 1: the portable entry point exists and is callable. It is not
// called here - there is no window and no GPU device in this tier - but a
// tier that links Main would catch its removal.
static void TestPortableStartupIsDeclared()
{
	bool ( STDCALL *pfn )( GFXNativeWindow ) = &NMain::InitializeWithWindow;
	Check( pfn != 0, "NMain::InitializeWithWindow is declared" );
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `no member named 'InitializeWithWindow' in namespace 'NMain'`.

- [ ] **Step 3: Split the body**

In `Initialization.cpp`, rename the existing definition and add the forwarder:

```cpp
bool STDCALL NMain::InitializeWithWindow( GFXNativeWindow window )
{
	// The body that was NMain::Initialize, with hWnd3D replaced by the window
	// IGFX::Init has always taken. The other two handles and bGame were never
	// read; the four-argument form below keeps them for source compatibility.
	... unchanged body, passing `window` where hWnd3D went ...
}
bool STDCALL NMain::Initialize( HWND hWnd3D, HWND hWndInput, HWND hWndSound, bool bGame )
{
	return InitializeWithWindow( static_cast<GFXNativeWindow>( hWnd3D ) );
}
```

Declare both in `iMain.h` beside `SetupGlobalVarConsts`.

- [ ] **Step 4: Switch the game and drop its cast**

`GameMain.cpp:625` currently casts its `SDL_Window*` to `HWND`. Replace that call with `NMain::InitializeWithWindow( pWindow )` and delete the cast.

- [ ] **Step 5: Run the test and the game smoke**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

Run: `zig build install-game -Dtarget=aarch64-macos --release=fast`, then the staged game with `BK_AUTO_UI="900:shot,1000:exit"`.
Expected: exit 0 and a shot written. The window path is what changed; a black or missing frame means the cast was load-bearing after all.

- [ ] **Step 6: Commit**

```bash
git add Sources/src/Main/Initialization.cpp Sources/src/Main/iMain.h Sources/src/Game/GameMain.cpp tools/zig/map_file_test.cpp
git commit -m "feat(main): a startup entry point that takes the window the renderer wants

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: The C ABI, a session, and the exception firewall

**Files:**
- Create: `Sources/src/EditorBridge/bridge.h`, `Sources/src/EditorBridge/bridge.cpp`
- Create: `Sources/src/EditorBridge/session.h`, `Sources/src/EditorBridge/session.cpp`
- Create: `tools/zig/editor_bridge_test.cpp`
- Modify: `build.zig`

**Interfaces:**
- Consumes: `NMain::InitializeWithWindow` (Task 1), `NMapFile::Read`/`Write` and `NMapOverlay::*` (plan 2).
- Produces:
  ```c
  /* bridge.h - no engine header appears here. */
  typedef enum {
      BK_EDITOR_OK = 0,
      BK_EDITOR_BAD_ARGUMENT = 1,
      BK_EDITOR_NO_SESSION = 2,
      BK_EDITOR_NO_DEVICE = 3,     /* no window or no GPU device */
      BK_EDITOR_DATA_MISSING = 4,  /* a map, tileset or descriptor is not there */
      BK_EDITOR_REFUSED = 5,       /* the edit is not allowed; see the message */
      BK_EDITOR_FAILED = 6
  } BkEditorStatus;

  typedef struct BkEditorSession BkEditorSession;

  /* The last message, owned by the bridge, valid until the next call on the
     same session. Never null: an empty string when there is nothing to say. */
  const char *BkEditorLastMessage( BkEditorSession *session );

  BkEditorStatus BkEditorStart( void *window, const char *data_root, BkEditorSession **out );
  BkEditorStatus BkEditorStop( BkEditorSession *session );
  ```

- [ ] **Step 1: Write the failing test**

`tools/zig/editor_bridge_test.cpp`:

```cpp
// The engine tier. Needs a hidden SDL window and a GPU device; where there is
// none it prints "skipped: no GPU device" and exits 0, because the spec's tier
// table says this tier never reports a pass it did not earn.
#include "StdAfx.h"
#include "../../Sources/src/EditorBridge/bridge.h"

static int g_nFailures = 0;

static bool Check( bool bCondition, const char *pszWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", pszWhat );
		++g_nFailures;
	}
	return bCondition;
}

int main( int argc, char **argv )
{
	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( 0, "Data", &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		printf( "editor-bridge: skipped: no GPU device\n" );
		return 0;
	}
	if ( !Check( status == BK_EDITOR_OK, "the bridge starts" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return 1;
	}
	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	if ( g_nFailures == 0 )
		printf( "editor-bridge: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails — neither the step nor `bridge.h` exists.

- [ ] **Step 3: Write the header**

`bridge.h` is the C ABI above, plus this note at the top:

```c
/* The editor's view of the engine. Flat C: integer handles, plain structs,
   status codes. No engine header is included here and no C++ exception
   crosses it - every entry point wraps its body and returns a status, because
   a throw through this boundary would cross a module edge and unwind into
   Zig, which has no idea what to do with it. */
```

- [ ] **Step 4: Write the session and the firewall**

`session.h` holds the state the spec's "Building the engine state" names:

```cpp
// One editing session. The snapshot is the map exactly as read, frame indices
// still packed; the working copy has UnpackFrameIndices applied, which picks a
// random visual variant per type and so is never what gets written back for an
// object the editor did not touch.
struct SEditorSession
{
	CMapInfo snapshot;
	CMapInfo working;
	std::string szMapPath;
	std::string szMessage;
	// Engine object by snapshot link ID. An object whose type the database
	// does not know is absent here and listed in unknownLinkIDs instead.
	std::unordered_map<int, CPtr<IRefCount> > byLinkID;
	std::vector<int> unknownLinkIDs;
	bool bStarted;
	SEditorSession() : bStarted( false ) {  }
};
```

`bridge.cpp` gives every entry point the same shape, so the firewall is one pattern and not twelve:

```cpp
// Every entry point looks like this. The catch is not decoration: an engine
// throw that escaped here would unwind through the C ABI into Zig.
template<class F>
static BkEditorStatus Guarded( SEditorSession *pSession, F body )
{
	if ( pSession == 0 )
		return BK_EDITOR_NO_SESSION;
	try
	{
		pSession->szMessage.clear();
		return body();
	}
	catch ( ... )
	{
		pSession->szMessage = "the engine threw";
		return BK_EDITOR_FAILED;
	}
}
```

`BkEditorStart` does the game's startup without `CMainLoop`: `NMain::LoadAllModules`, storage, `consts.xml` + `SetupGlobalVarConsts`, `CreateObjectsDB` + `LoadDB`, then `NMain::InitializeWithWindow( window )` and `IGFX::SetMode`. A null window, or a renderer that will not start, returns `BK_EDITOR_NO_DEVICE` — not a failure, because CI runners without a GPU must skip rather than fail.

- [ ] **Step 5: Add the library and the step to build.zig**

`addEditorBridge` mirrors `addMapFile`; `addEditorBridgeTest` copies `gfxgpu-factory-test`'s module configuration exactly (see Global Constraints) and links Scene, AILogic, GFXGPU and MapFile. The run step installs `streamio_zig`, `options_bridge`, `platform_runtime`, `sdl_dynamic` and `gfx_gpu`, and calls `addPathDir(zig-out/bin)`.

- [ ] **Step 6: Run it**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS` on a machine with a GPU device, `editor-bridge: skipped: no GPU device` without one. Both exit 0; only a real failure exits 1.

- [ ] **Step 7: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp build.zig
git commit -m "feat(editor): the bridge's C ABI, a session, and an exception firewall

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Open a map and build its engine state

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `session.cpp`, `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      int width_tiles, height_tiles;
      int season;
      int player_count;
      int object_count;          /* objects + scenarioObjects */
      int unknown_object_count;  /* in the snapshot, not in the engine */
  } BkEditorMapSummary;

  BkEditorStatus BkEditorOpenMap( BkEditorSession *session, const char *path, BkEditorMapSummary *out );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	Check( BkEditorOpenMap( pSession, "Data\\Maps\\Multiplayer\\coldwinter.bzm", &summary ) == BK_EDITOR_OK,
	       "a shipped map opens" );
	Check( summary.width_tiles > 0 && summary.height_tiles > 0, "and reports its size" );
	Check( summary.object_count > 0, "and its objects" );
	Check( summary.unknown_object_count == 0, "and knows every type in it" );
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `no member named 'BkEditorOpenMap'`.

- [ ] **Step 3: Build the engine state**

`session.cpp`, in the order the research note records from `TemplateEditorFrame1.cpp:1657-1790`:

```cpp
// The MFC editor's order, which is the order the engine expects. The numbered
// comments match zig-out/local-test/bridge-research.md.
// 1. Altitudes, then shades.
if ( working.terrain.altitudes.GetSizeX() == 0 )
{
	working.terrain.altitudes.SetSizes( working.terrain.patches.GetSizeX() * 16 + 1,
	                                    working.terrain.patches.GetSizeY() * 16 + 1 );
	working.terrain.altitudes.SetZero();
}
CMapInfo::UpdateTerrainShades( &working.terrain,
                               CTRect<int>( 0, 0, working.terrain.altitudes.GetSizeX(), working.terrain.altitudes.GetSizeY() ),
                               CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( working.nSeason ) ) );
// 2. The AI editor, before the terrain.
pAIEditor->SetDiplomacies( working.diplomacies );
pAIEditor->Init( working.terrain );
// 3. The terrain into the scene.
ITerrain *pTerrain = CreateTerrain();
pTerrain->Load( szMapPath.c_str(), working.terrain );
pScene->SetTerrain( pTerrain );
```

Then the object loop, with the guard the MFC editor does not have:

```cpp
// The MFC editor reads pObjectsDB->GetDesc( name )->eGameType with no null
// check (TemplateEditorFrame1.cpp:1751 and 1783). GetDesc returns 0 for a name
// the database does not know, NI_ASSERT_T compiles away in release, and the
// dereference crashes. An unknown object is kept in the snapshot, listed, and
// never placed - which is also what makes the map survive a save.
const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( it->szName.c_str() );
if ( pDesc == 0 )
{
	unknownLinkIDs.push_back( it->link.nLinkID );
	continue;
}
if ( pDesc->eGameType == SGVOGT_BRIDGE )
{
	bridgeSpans.push_back( *it );   // placed as linked spans, below
	continue;
}
if ( !pAIEditor->IsObjectInsideOfMap( *it ) )
	continue;
IRefCount *pAIObject = 0;
if ( pAIEditor->AddNewObject( *it, &pAIObject ) && pAIObject != 0 )
	byLinkID[it->link.nLinkID] = pAIObject;
```

- [ ] **Step 4: Run the test**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS`

- [ ] **Step 5: Add the unknown-object case**

Write a map with an object renamed to something the database cannot know (the map-file tier already has `TestUnknownObjectSurvives` to copy from), open it, and assert `summary.unknown_object_count == 1` and that the session still opened. This is the case that crashes the MFC editor.

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS`

- [ ] **Step 6: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): a map's engine state, and an unknown type that does not crash it

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Save through the overlay, and prove nothing else moved

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `session.cpp`, `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `NMapFile::Write`, `NMapFile::AreEquivalent`, `NMapOverlay::*` (plan 2).
- Produces:
  ```c
  BkEditorStatus BkEditorSaveMap( BkEditorSession *session, const char *path );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
// The spec's first engine-tier check: open a shipped map and save it with no
// edits; the result is equivalent to what was read. The bridge writes the
// snapshot, not the engine's state, so this is a test of that discipline.
	Check( BkEditorSaveMap( pSession, "zig-out\\local-test\\bridge-roundtrip.bzm" ) == BK_EDITOR_OK,
	       "a map saves" );
	CMapInfo original, saved;
	std::string szError, szWhere;
	Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &original, &szError ), szError.c_str() );
	Check( NMapFile::Read( "zig-out\\local-test\\bridge-roundtrip.bzm", &saved, &szError ), szError.c_str() );
	Check( NMapFile::AreEquivalent( original, saved, &szWhere ),
	       szWhere.empty() ? "and an unedited save is equivalent" : ( "save differs at " + szWhere ).c_str() );
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `no member named 'BkEditorSaveMap'`.

- [ ] **Step 3: Implement save**

```cpp
// The snapshot with the edits laid over it, never the engine's state. The
// engine's copy has UnpackFrameIndices applied - a random visual variant per
// type - so writing it back would rewrite every frame index on the map.
BkEditorStatus Save( const char *pszPath )
{
	std::string szError;
	if ( !NMapFile::Write( pszPath, snapshot, &szError ) )
	{
		szMessage = szError;
		return BK_EDITOR_FAILED;
	}
	return BK_EDITOR_OK;
}
```

- [ ] **Step 4: Run the test**

Expected: `editor-bridge: PASS`. A difference here means the snapshot was mutated by building the engine state, which it must not be.

- [ ] **Step 5: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): the bridge saves the snapshot, not the engine's copy

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Object edits, in the engine and in the snapshot together

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `session.cpp`, `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Produces:
  ```c
  BkEditorStatus BkEditorAddObject( BkEditorSession *session, const char *name,
                                    float x, float y, int dir, int player, int *out_link_id );
  BkEditorStatus BkEditorMoveObject( BkEditorSession *session, int link_id, float x, float y );
  BkEditorStatus BkEditorTurnObject( BkEditorSession *session, int link_id, int dir );
  BkEditorStatus BkEditorSetObjectPlayer( BkEditorSession *session, int link_id, int player );
  BkEditorStatus BkEditorDeleteObject( BkEditorSession *session, int link_id );
  BkEditorStatus BkEditorSetDiplomacy( BkEditorSession *session, int player, int value );
  /* The spec's "players and diplomacy" is three fields. These two are the map's
     own, not a player's: nType is the mission kind and nAttackingSide is which
     side attacks in it. Both live in the snapshot only - the engine has no say
     in them - so they take no engine call and cannot be refused. */
  BkEditorStatus BkEditorSetMapType( BkEditorSession *session, int type );
  BkEditorStatus BkEditorSetAttackingSide( BkEditorSession *session, int side );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
// Every edit changes the engine and the snapshot together, and the saved map
// equals the expected map the overlay builds on its own. That equality is the
// whole point of the tier: it proves the two paths agree.
	int nLinkID = -1;
	Check( BkEditorAddObject( pSession, szKnownName, 100.0f, 100.0f, 0, 0, &nLinkID ) == BK_EDITOR_OK,
	       "an object is added through the engine" );
	Check( nLinkID > 0, "and comes back with a link ID" );
	Check( BkEditorMoveObject( pSession, nLinkID, 132.0f, 100.0f ) == BK_EDITOR_OK, "and moves" );
	Check( BkEditorTurnObject( pSession, nLinkID, 1024 ) == BK_EDITOR_OK, "and turns" );
	Check( BkEditorSaveMap( pSession, "zig-out\\local-test\\bridge-edited.bzm" ) == BK_EDITOR_OK, "and saves" );

	// The expected value, built by the overlay alone - no engine involved.
	CMapInfo expected;
	Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &expected, &szError ), szError.c_str() );
	NMapOverlay::SAddObject add;
	add.szName = szKnownName; add.vPos = CVec3( 132.0f, 100.0f, 0.0f ); add.nDir = 1024; add.nPlayer = 0;
	int nExpectedLinkID = -1;
	NMapOverlay::AddObject( &expected, add, &nExpectedLinkID );
	Check( nExpectedLinkID == nLinkID, "the two paths agree on the link ID" );
	CMapInfo saved;
	Check( NMapFile::Read( "zig-out\\local-test\\bridge-edited.bzm", &saved, &szError ), szError.c_str() );
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "and the saved map is the expected one" : ( "edited save differs at " + szWhere ).c_str() );
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `no member named 'BkEditorAddObject'`.

- [ ] **Step 3: Implement the edits**

Each one does the snapshot first through `NMapOverlay`, then the engine, and rolls the snapshot back if the engine refuses:

```cpp
// Snapshot first, engine second, and the snapshot is put back if the engine
// says no - otherwise a refused engine edit would leave a map that saves
// something the editor never showed.
```

`BkEditorDeleteObject` returns `BK_EDITOR_REFUSED` with `NMapOverlay::DeleteObject`'s refusal text when something still refers to the object. The engine object is found through `byLinkID`, and `IAIEditor::MoveObject` takes `short` coordinates while the ABI takes floats — round once, in one place, and say so.

- [ ] **Step 4: Run the test**

Expected: `editor-bridge: PASS`

- [ ] **Step 5: Add the map's own two fields**

```cpp
	// nType and nAttackingSide are the map's, not a player's: no engine call,
	// so the only thing to prove is that they reach the saved file and that
	// nothing else moves with them.
	Check( BkEditorSetMapType( pSession, 1 ) == BK_EDITOR_OK, "the map type is set" );
	Check( BkEditorSetAttackingSide( pSession, 1 ) == BK_EDITOR_OK, "and the attacking side" );
	Check( BkEditorSaveMap( pSession, "zig-out\\local-test\\bridge-fields.bzm" ) == BK_EDITOR_OK, "and it saves" );
	CMapInfo fields;
	Check( NMapFile::Read( "zig-out\\local-test\\bridge-fields.bzm", &fields, &szError ), szError.c_str() );
	Check( fields.nType == 1 && fields.nAttackingSide == 1, "both reached the file" );
```

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS`

- [ ] **Step 6: Add the refused delete**

Find an object something refers to (the map-file tier's `TestObjectOverlay` shows how), delete it through the bridge, and assert `BK_EDITOR_REFUSED`, a non-empty message, and that a save afterwards is still equivalent to the unedited map.

- [ ] **Step 7: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): object and diplomacy edits, engine and snapshot together

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Terrain edits, and the engine agreeing with the copy

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `session.cpp`, `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `NMapOverlay::Paint`, `AffectedPatches`, `SPaintUndo` (plan 2).
- Produces:
  ```c
  typedef struct { int x, y; unsigned char tile, noise; } BkEditorPaintCell;
  BkEditorStatus BkEditorPaint( BkEditorSession *session, const BkEditorPaintCell *cells, int count );
  /* A world point to the tile it falls in - ITerrainEditor::GetTileIndex,
     which is how a brush turns a click into cells. Screen to world is
     BkEditorScreenToWorld in Task 7; the two compose. */
  BkEditorStatus BkEditorWorldToTile( BkEditorSession *session, float wx, float wy, int *out_x, int *out_y );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
// The spec's engine-tier terrain check: after a paint, the engine's terrain
// equals the bridge's copy in tiles and patch crosses. The bridge's copy is
// what gets saved, so a disagreement means the editor shows one thing and
// writes another.
	BkEditorPaintCell cell;
	cell.x = 20; cell.y = 20; cell.tile = nNewTile; cell.noise = nNoise;
	Check( BkEditorPaint( pSession, &cell, 1 ) == BK_EDITOR_OK, "a cell paints through the bridge" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       "and the engine's terrain matches the copy that will be saved" );

	// The brush's other half: a world point becomes the cell it falls in.
	int nTileX = -1, nTileY = -1;
	Check( BkEditorWorldToTile( pSession, fWorldX, fWorldY, &nTileX, &nTileY ) == BK_EDITOR_OK,
	       "a world point becomes a tile index" );
	Check( nTileX >= 0 && nTileY >= 0, "inside the map" );
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `no member named 'BkEditorPaint'`.

- [ ] **Step 3: Implement paint and the comparison**

`BkEditorPaint` runs `NMapOverlay::Paint` on the snapshot's terrain — the same deterministic function the map-file tier tests — and then pushes the affected region into the engine:

```cpp
// The function runs once per command on the bridge's own terrain, and the
// region it touched is pushed to the engine. The engine is never the source of
// the saved terrain; the spec is explicit about that, because the engine's
// update and the function agree only inside the region.
const CTRect<int> r = NMapOverlay::AffectedPatches( snapshot.terrain, cells );
... NMapOverlay::Paint( &snapshot, cells, &undo ) ...
pAIEditor->UpdateTerrain( r, snapshot.terrain );
if ( ITerrainEditor *pEditor = static_cast<ITerrainEditor*>( pScene->GetTerrain() ) )
	pEditor->Update( r );
```

`BkEditorWorldToTile` is `ITerrainEditor::GetTileIndex( CVec3( wx, wy, 0 ), &x, &y )`.

`BkEditorTerrainMatchesEngine` is a debug entry point that compares `ITerrainEditor::GetTerrainInfo()` against the session's terrain in `tiles` and patch crosses, and fills the message with the first difference. It exists for this tier; the editor never calls it.

Note the cross-module `dynamic_cast` caveat recorded in `Common/MOBuilding.cpp`: on the Itanium ABI a `dynamic_cast` across a module boundary returns null. Use `static_cast`, as that file does, and say why.

- [ ] **Step 4: Run the test**

Expected: `editor-bridge: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): terrain paint through the bridge, with the engine checked against it

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Picking, camera, catalogue and frame

**Files:**
- Modify: `Sources/src/EditorBridge/bridge.h`, `session.cpp`
- Create: `Sources/src/EditorBridge/catalogue.cpp`
- Modify: `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Produces:
  ```c
  typedef struct { char name[64]; int game_type; } BkEditorCatalogueEntry;
  BkEditorStatus BkEditorCatalogue( BkEditorSession *session, BkEditorCatalogueEntry *out, int capacity, int *out_count );

  BkEditorStatus BkEditorScreenToWorld( BkEditorSession *session, float sx, float sy, float *wx, float *wy );
  BkEditorStatus BkEditorObjectAt( BkEditorSession *session, float sx, float sy, int *out_link_id );
  BkEditorStatus BkEditorSetCamera( BkEditorSession *session, float wx, float wy );
  BkEditorStatus BkEditorFrame( BkEditorSession *session );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
	int nCount = 0;
	std::vector<BkEditorCatalogueEntry> entries( 4096 );
	Check( BkEditorCatalogue( pSession, &(entries[0]), int( entries.size() ), &nCount ) == BK_EDITOR_OK,
	       "the object catalogue reads" );
	Check( nCount > 0, "and is not empty" );

	// Put the camera on a known object and ask what is under its screen point.
	Check( BkEditorSetCamera( pSession, fObjectX, fObjectY ) == BK_EDITOR_OK, "the camera moves" );
	Check( BkEditorFrame( pSession ) == BK_EDITOR_OK, "a frame draws" );
	float wx = 0.0f, wy = 0.0f;
	Check( BkEditorScreenToWorld( pSession, 320.0f, 240.0f, &wx, &wy ) == BK_EDITOR_OK, "a screen point becomes a world point" );
	int nHit = -1;
	BkEditorObjectAt( pSession, 320.0f, 240.0f, &nHit );   /* may legitimately hit nothing */
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `no member named 'BkEditorCatalogue'`.

- [ ] **Step 3: Implement them**

`catalogue.cpp` walks `IObjectsDB::GetAllDescs()` into fixed-size records — a fixed `char name[64]` rather than a pointer, so the caller owns nothing and there is nothing to free across the ABI. Truncate and say so in the comment.

Picking is `IScene::Pick( point, &pObjects, &nCount, type )` filtered the way the MFC editor filters it (entrenchments and bridges dropped, `TemplateEditorFrame1.cpp:3384-3395`), then `IAIEditor::AIToLink` to turn the hit into a snapshot link ID. `BkEditorScreenToWorld` is `IScene::GetPos3`. `BkEditorSetCamera` is `ICamera::SetAnchor`.

- [ ] **Step 4: Run the test**

Expected: `editor-bridge: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): catalogue, picking, camera and frame across the bridge

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: The engine tier in CI, skipping honestly

**Files:**
- Modify: `.github/workflows/cross-platform.yml`
- Modify: `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` if the tier's reach turns out to differ from the table

**Interfaces:** none; this task is the gate.

- [ ] **Step 1: Confirm the skip path is honest**

Run the test with no display and no GPU device available and confirm it prints `editor-bridge: skipped: no GPU device` and exits 0, and that a *failure* still exits 1. A tier that cannot tell those apart is worse than no tier — the map-file tier learned this when CI swept zero maps and passed.

- [ ] **Step 2: Add the step to the jobs that can run it**

Windows-MinGW cannot build the engine C++ at all (`Platform/LegacyVariant.h` needs MSVC's `comutil.h`), so it is excluded exactly as the map-file tier is. Add the step to the other five in each job's own style, with `-Dtest-mode=run`.

- [ ] **Step 3: Add the data the tier reads to the sparse checkout**

It opens shipped maps and their tilesets, so `Data/Maps` and `Data/Terrain/sets/*/tileset.xml` and `crosset.xml` are already listed. If the engine turns out to need more — object models, textures — measure what and add exactly that, not all of `Data`.

- [ ] **Step 4: Run the branch through CI and read every job**

Expected: five jobs run the tier; those without a GPU device report the skip. Record in the commit which runners actually exercised it, because "green" and "ran" are different claims.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/cross-platform.yml
git commit -m "ci(editor): the engine tier, skipping where there is no GPU device

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Done when

- The game starts through `NMain::InitializeWithWindow` with no `HWND` cast.
- `zig build test-editor-bridge` opens two shipped maps, saves them unedited to an equivalent result, adds/moves/turns/deletes an object and changes diplomacy with the saved map equal to the overlay's expected map, paints a cell with the engine's terrain matching the copy that gets saved, and refuses a delete that would orphan a reference.
- A map with an unknown object type opens, lists it, and saves it unchanged — the case that crashes the MFC editor.
- The tier is a required CI step on the five targets that build the engine C++, and reports "skipped: no GPU device" rather than a pass where there is none.

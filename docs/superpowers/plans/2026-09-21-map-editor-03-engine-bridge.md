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

- [ ] **Step 1: Write the failing change**

There is no unit test for this task and inventing one would be worse than
saying so: the map file tier is deliberately renderer-free and does not link
`Main`, so a symbol check there would either fail to link or quietly drag the
renderer into a tier whose whole point is not having one. The bridge tier of
Task 2 links `Main` and exercises the real call.

The failing step is the game itself. Change `GameMain.cpp:625` first, before
the function exists:

```cpp
	// Was: NMain::Initialize( (HWND)pWindow, 0, 0, true )
	if ( !NMain::InitializeWithWindow( pWindow ) )
		return 0xDEAD;
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build install-game -Dtarget=aarch64-macos --release=fast`
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

- [ ] **Step 4: Run the game smoke**

Run: `zig build install-game -Dtarget=aarch64-macos --release=fast`, then the staged game with `BK_AUTO_UI="900:shot,1000:exit"` and `-monitor=2`.
Expected: exit 0 and a shot written, and the shot shows the menu rather than a black frame. The window path is the only thing that changed; a black or missing frame means the cast was load-bearing after all.

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS` - unchanged, and it must stay that way. If this task made the map file tier need `Main`, the split went wrong.

- [ ] **Step 5: Commit**

```bash
git add Sources/src/Main/Initialization.cpp Sources/src/Main/iMain.h Sources/src/Game/GameMain.cpp
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
     same session. Never returns null: an empty string when there is nothing to
     say, and a fixed string when session is null - a start that failed before
     it had anywhere to put a message still has to be printable, and the
     caller holds a null session exactly then. */
  const char *BkEditorLastMessage( BkEditorSession *session );

  /* Why BkEditorStart takes **out: on failure it may or may not have got as far
     as allocating a session. It sets *out to whatever it has - null if it
     failed before allocating - so the caller can print
     BkEditorLastMessage(*out) unconditionally. */

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
	// A real hidden window, not a null handle. Passing 0 here would take the
	// BK_EDITOR_NO_DEVICE path on every machine and the tier would skip itself
	// into always-green, which is the failure mode the map file tier already
	// had once when it swept zero maps and passed.
	// A headless runner has no video driver at all, and that is a legitimate
	// skip. Anything else going wrong in SDL is a failure: calling it "no GPU
	// device" would file a broken runner under the one outcome nobody looks at.
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( strstr( pszError, "video driver" ) != 0 || strstr( pszError, "No available" ) != 0 )
		{
			printf( "editor-bridge: skipped: no video driver (%s)\n", pszError );
			return 0;
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "editor-bridge-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		// SDL started, so there is a video driver; failing to make a hidden
		// window after that is a real problem and not a skip.
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( pWindow, "Data", &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		printf( "editor-bridge: skipped: no GPU device\n" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	if ( !Check( status == BK_EDITOR_OK, "the bridge starts" ) )
	{
		// pSession may be null here - the start can fail before it allocates
		// one - which is why BkEditorLastMessage is defined for a null session.
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}
	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "editor-bridge: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
```

The test includes `<SDL3/SDL.h>`; `addEditorBridgeTest` already links SDL because the bridge does. `BK_EDITOR_NO_DEVICE` must therefore mean "the renderer would not start on a real window", not "no window was passed" - a null window is a caller bug and returns `BK_EDITOR_BAD_ARGUMENT`.

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
// Defined for a null session on purpose: a failed start hands the caller a null
// session and a reason in the same breath, and the caller should not have to
// branch to read it.
const char *BkEditorLastMessage( BkEditorSession *pSession )
{
	if ( pSession == 0 )
		return "the session could not be created";
	return pSession->szMessage.c_str();
}

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

`BkEditorStart` does the game's startup without `CMainLoop`: `NMain::LoadAllModules`, storage, `consts.xml` + `SetupGlobalVarConsts`, `CreateObjectsDB` + `LoadDB`, then `NMain::InitializeWithWindow( window )` and `IGFX::SetMode`.

The two failure modes are kept apart on purpose:
- `window == 0` is a caller bug and returns `BK_EDITOR_BAD_ARGUMENT`.
- A real window on which the renderer will not start - no GPU device on the
  runner - returns `BK_EDITOR_NO_DEVICE`, which the tier reports as a skip.

Collapsing the two would let a test that forgot its window skip on every
machine and never be noticed.

- [ ] **Step 5: Add the library and the step to build.zig**

`addEditorBridge` mirrors `addMapFile`; `addEditorBridgeTest` copies `gfxgpu-factory-test`'s module configuration exactly (see Global Constraints) and links `Main`, `MapFile`, `RandomMapGen`, `Formats`, `Misc`, and - because `Main` brings them - `lualib` and `zlib`. The run step installs `streamio_zig`, `options_bridge`, `platform_runtime` and `sdl_dynamic`, and calls `addPathDir(zig-out/bin)`.

**Two things beyond the library are needed before the bridge can start the
engine, and the first one is not where it looks.**

*The host executable's globals have to be filled before the first module
loads.* Measured: `member call on null pointer of type 'ISingleton'` in
`libAILogic.dylib`, `Cheats.cpp:22`, inside the `dlopen` that
`LoadAllModules` does - `SCheats`'s static constructor reads a global var while
the module is still being loaded. The obvious reading, that the module's own
`GlobalsLoader` had not run, is wrong: `lldb` shows it runs first and its
`dlopen` of `libStreamIO` succeeds. `g_pGlobalSingleton` is a tentative
definition in every module *and* in the host, so the loader coalesces all of
them onto the host executable's copy, and that copy is the one that was still
null. `Game.exe` never sees this because `Sources/src/Game/GlobalsLoader.cpp`
fills it in a static initializer before `main`; a host with no such translation
unit has to do it itself. `NMain::EnsureGlobalHooks` is therefore made
non-static and declared in `iMain.h`, and `BkEditorStart` calls it *before*
`LoadAllModules` - `LoadAllModules` calls it only after its loading loop, which
is too late.

*The executable has to live inside the installation it starts.* Every engine
module links its own copy of `NPlatform::Paths` and `BaseRoot()` derives from
`executableRoot()`, so telling the bridge which installation to use moves only
the bridge's own copy; a binary run from the build cache leaves every module
resolving data against `.zig-cache/...`. `NPlatform::Paths::SetRoots` (added in
this task, with `SetInjectedRootsForTest` forwarding to it) is still needed -
the editor is told which game directory to edit rather than inferring one - but
it settles one copy out of a dozen.

So this step also stages `editor-bridge-test` beside `Game` in the install
layout and runs it there, which makes the tier depend on `install-game`.
(`addRunArtifact` runs the installed copy once the artifact is installed, not
the one in the cache - the cache copy cannot resolve `@rpath` from that cwd at
all.) The test asserts the two agree rather than skipping: with the staging in
place a mismatch is a build mistake, and skipping on it would be exactly the
always-green outcome this tier exists to avoid.

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

  /* An OS filesystem path ending in .bzm or .xml - what a file dialog hands
     back - not a storage-relative name. The bridge opens it with
     NMapFile::Read, which takes a path; NMapFile::ReadNewest is the
     storage-relative one and is deliberately not used here, because the editor
     opens the file the user picked rather than the newer of a pair it inferred.
     The path is still written with the engine's separator: OpenFileStream
     splits on backslash only. */
  BkEditorStatus BkEditorOpenMap( BkEditorSession *session, const char *path, BkEditorMapSummary *out );
  ```

  `ITerrain::Load` takes the same path as its name argument, as the MFC editor
  passes `szSelectedFileMapFullName` (research note, step 3).

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

**Five things this turned up that the plan did not have:**

*`CAIEditor::AddNewObject` never reports success.* It ends in an unconditional
`return false` (`AIEditorInternal.cpp:46`) and reports by writing the object out
instead; the MFC editor tests the pointer and ignores the bool. Taking the bool
at its word placed nothing at all, on every map, and said nothing about it - the
summary still looked right because it counted what the file held. That is why
the summary now also reports `placed_object_count` and `bridge_span_placed`:
what the engine ended up holding is the only thing that can catch this.

*Placing one object is more than `AddNewObject`.* The recipe is
`CTemplateEditorFrame::AddObjectByAI` (`TemplateEditorFrame1.cpp:2778-2810`):
clamp `fHP` to 1, zero `nFrameIndex` for a squad, and hand the engine a player
only for a building - everything else goes in unowned and gets its player back
from the editor above. The map keeps the real value throughout, so a save is
unaffected.

*Bridges are built afterwards, through `CMapInfo::bridges`.* A span found in
`objects` or `scenarioObjects` is set aside, and the bridges are built from the
link ID lists, so one bridge's spans keep the file's order
(`TemplateEditorFrame1.cpp:1948-1979`). A span stored with negative HP is one
the mission builds later: the engine will not take it that way, so it is created
whole and its link ID listed. Measured: arnheim 11 spans, dessau 8, vitebsk 7,
all placed.

*A fourth unguarded null, and shipped Data trips it.* `CheckStaticObject`
(`AIEditorInternal.cpp:278`) dereferences the RPG stats it asks for, and
`GetRPGStats` returns 0 when the stats file is missing. `Data\Maps\dessau.bzm`
names `Logs08`, whose stats at `objects\simpleobjects\common\summer\logs\08`
are not in the installation. Such an object is now simply not inside the map and
is not placed; dessau is in the test for it.

*The tier writes only to a scratch directory.* Shipped `Data` is read-only for
every tier, so the map the unknown-object case constructs goes in
`zig-out/local-test`, passed to the executable as its second argument. Writing
it into the installation and removing it afterwards would leave it behind on any
run that is killed in between. Nothing else is needed beside a map:
`CTerrain::LoadLocal` keeps the path only as a name and takes the tileset,
crosset and roadset from storage (`TerrainInternal.cpp:88-110`).

**Two more the plan did not have:**

*`EnsureGlobalHooks` has to fill `g_pGlobalRandomGen` too.* It is the fourth of
the globals every `GlobalsLoader.cpp` sets, it comes out of the singleton rather
than off a StreamIO hook, and `AILogic` reaches for it as early as
`IAIEditor::Clear` - `Weather::SwitchAutomatic` (`Weather.cpp:38`). Measured:
`member call on null pointer of type 'IRandomGen'` on the first `Clear`.

*The unguarded `GetDesc` is not only in the MFC editor's object loop.*
`CMapInfo::UnpackFrameIndex` and `CMapInfo::PackFrameIndex`
(`MapInfo_StaticMethods.cpp`) do the same thing: assert with `NI_ASSERT_T`,
which compiles away, then dereference. `UnpackFrameIndices` runs while the
working copy is being made, so an unknown object takes the session down before
the bridge's own guard is reached. Both now return early and leave the frame
index exactly as it stands, which is what an untouched object needs anyway. The
map file tier never saw this because it does not unpack.

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

**What this turned up:**

*"Refused" has to be read back from the engine - and from all three fields.*
`CAIEditor::MoveObject` and `TurnObject` end in the same unconditional
`return false` as `AddNewObject` (`AIEditorInternal.cpp:126` and `161`), and
they refuse silently - `CanSetNewCoord` and `IsRectInsideOfMap` simply leave
the object where it was. So each edit applies the overlay, calls the engine,
then asks the engine what it is actually holding and rolls the snapshot back if
the two disagree.

Reading back the position alone is not enough, and the gap is not theoretical.
The engine's object for a simple static object is a `CGivenPassabilityStObject`,
whose `GetDir` returns 0 whatever it is told and whose `SetPlayerForEditor` is
`CStaticObject`'s empty body - so turning a tree or giving it to a player is
ignored in silence. Checking only the position wrote both into the file:
measured as `a refused edit reached the file at objects[260].nDir`. Each of the
three is now asked for only when it is changing and read back only when it was
asked for, so an object that was never turned is not refused for having no
direction.

*A refusal has to put the engine back as well as the map.* The three changes go
to the engine one after another, so a move that takes followed by a turn that
does not leaves the engine showing the object at a position the file never
records - the same disagreement the refusal exists to prevent, the other way
round. The rollback restores what the engine was *reading* beforehand, never
what the snapshot says: `PlaceOneObject` hands the engine player 0 for
everything that is not a building while the map keeps the real owner, so
restoring the map's value would change the engine rather than put it back. A
rollback that itself does not take is reported as a failure and not an ordinary
refusal - the two now disagree and nothing in the bridge can mend it.

`BkEditorPlaceObject` exists so that case is reachable at all: the single-field
calls change one thing each, so the partial failure cannot happen through them.
It is also what a caller dragging an object while turning it wants.
`BkEditorEngineObjectState` reports what the engine holds rather than what the
map says, which is how a caller - or a test - checks the two agree.

Two engine accessors were needed for the read-back. `CAIEditor::GetDir` returns the
static object's own direction rather than a flat 0, which distinguishes a
`CTerraMeshStaticObject` that kept the turn from a `CGivenPassabilityStObject`
that cannot - the flat 0 is also how the MFC editor writes every static object
out facing north when it saves from the engine
(`TemplateEditorFrame1.cpp:3560`). `IAIEditor::GetPlayer` is new, and answers
-1 where the object's kind has no owner, which is not the same as belonging to
player -1.

*An object the map holds but the engine does not* - one outside the map, or one
whose RPG stats are missing - is refused rather than moved in the file alone,
which would leave the two out of step.

*`CMapInfo::PackFrameIndex` was private.* The overlay deliberately leaves an
added object's frame index at 0 because packing needs the object database, and
the bridge has it. The plural `PackFrameIndices` is public but walks the whole
map, rewriting every other object's index on the way, so the singular is now
public beside it. The test adds a poplar on purpose: packing only does anything
for a fence, an entrenchment or a span, so an ordinary object is the case where
the extra step has to be invisible.

*Diplomacy goes to the engine whole.* `IAIEditor::SetDiplomacies` takes the
entire table, not one player, so the bridge changes the map's and hands that
over.

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

**What this turned up:**

*A cross's artwork is chosen with `rand()`.* `STileTypeDesc::GetMapsIndex`
(`Formats/fmtTerrain.h:84-92`) rolls against the tileset's probability ranges
every time a cross is regenerated, so painting the same cell twice gives the
same terrain with different pictures on it. The plan asked for the engine's
patch crosses to equal the bridge's; they never can, and neither can two
independently painted maps. Measured as `engine has 67/157, the map has 67/147`
- the same joined tile, a different picture of it. Both comparisons now check
everything that decides the shape of the terrain - the tiles, and each cross's
position, joined tile and flags - and leave the roll out. A painted map is
therefore not byte-reproducible; the shipped-map round-trip is unaffected
because it never repaints.

*The engine keeps its own `STerrainInfo`.* `CTerrain::Load` copies the terrain
in and `CTerrain::Update` works on that copy, so painting the bridge's map left
the engine showing the old tiles - measured as `tile 20,20: engine has 159/1,
the map has 160/1`. The painted cells go in through `ITerrainEditor::SetTile`
and the engine then runs its own `Update`, which is the same preprocessing pass
and the same cross generation the overlay just ran.

*`ITerrainEditor` needed a way out of `ITerrain`.* It is a sibling base of
`CTerrain`, not related to `ITerrain`, so there is no `static_cast` between them
and a `dynamic_cast` would cross the module boundary and return null on the
Itanium ABI (the note in `Common/MOBuilding.cpp`). `ITerrain::GetEditor` is new
and does the cast inside Scene, where the two are one object.

*A map's positions are AI coordinates.* `ITerrainEditor::GetTileIndex` takes a
world point, so the test converts with `AI2Vis` before calling
`BkEditorWorldToTile` - which is what the MFC editor spells out before every
such call (`TemplateEditorFrame1.cpp:1747`).

*A paint that fails has to leave the map alone.* `NMapOverlay::Paint` writes
the cells before it loads the tileset it needs, and then has three ways out
that return false - no storage, an unreadable tileset, a cross pass that
refuses. Every one of them now puts the region back and empties the caller's
undo record, so "false" means nothing happened; a caller that took it at its
word and saved would otherwise have written a paint that never ran. The map
file tier arranges it by naming a tileset that is not there. The bridge puts
the engine back with it.

*Noise is not the brush's to choose.* `BkEditorPaintCell` has no noise field:
the preprocessing pass both sides run ends in `CTerrainBuilder::SetNoise`,
which writes `HasNoise(tile)` across the region whatever was there before
(`RandomMapGen/TerrainBuilder.cpp:257-264`), and `CTerrain::SetTile` derives it
as well. A value passed in was discarded without a word, which is worse than
not offering it. The map and the engine could not actually drift apart over it,
because both recompute.

*A cell off the map was skipped in silence.* `NMapOverlay::Paint` ignores one,
so a brush that ran over the edge came back reporting success. The bridge
refuses and names the cell.

*Legacy sources read as binary to grep.* `RandomMapGen/TerrainBuilder.cpp` and
several others carry Cyrillic comments in a legacy codepage, so a plain `grep`
skips them silently and the definition of a function looks absent from the
tree. Use `grep -a`.

- [ ] **Step 5: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): terrain paint through the bridge, with the engine checked against it

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Catalogue, camera and frame (picking deferred to plan 4)

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

**Picking is not a one-liner, and the plan was wrong about that.**

`IScene::Pick` works on the visual objects in the scene, and the bridge builds
none: it places AI objects and loads the terrain, and that is all the scene
holds. Measured - `Pick` at the middle of the screen over a map with 243 placed
objects returns 0. The MFC editor gets its hits from `FindByVis`, which is
`CWorldBase`'s (`Sources/src/Common/WorldBase.h:234`); `CWorldBase::Update`
turns the AI's notifications into `SMapObject`s with visual objects attached,
and both the game's `CWorldClient` and the MFC editor derive from it.

So picking needs a visual-object layer, which the editor needs anyway to draw
anything but terrain. `CWorldBase` is close to usable as it stands - one pure
virtual, `ResetSelection` - but its `Init` wants eleven singletons and the
`Common` library brings the map objects, the sounds, the war fog and
`InterfaceScreenBase` with it. That is a bigger decision than this task, and it
belongs with whoever owns the editor's object layer.

**Decided 2026-09-21: it goes in plan 4.** The editor core needs the layer to
draw objects at all, so picking follows it rather than the other way round.
`BkEditorObjectAt` is not part of plan 3; plan 4 adds it alongside whatever it
builds the visuals with. The two options weighed were deriving from
`CWorldBase` - the engine's own machinery, shared with `CWorldClient` and the
MFC editor, at the cost of the `Common` library - and building visuals in the
bridge with `IVisObjBuilder`, which is a smaller dependency but re-implements a
slice of `CWorldBase` and would drift from what the game does.

The rest of the task landed: the catalogue (5559 objects), the camera, a frame,
and screen-to-world. The two conversions are checked by composing them rather
than by returning OK - a camera put on an object and asked what is under the
middle of the screen answers with that object's own cell, measured as
`the camera is on cell 83,36 and the middle of the screen is 83,36`.

- [ ] **Step 4: Run the test**

Expected: `editor-bridge: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): catalogue, camera and frame across the bridge

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

Which of those five will actually exercise it is measured, not assumed
(`zig build gpu-device-probe`, 2026-09-21):

| Runner | What the probe said | This tier |
|---|---|---|
| `macos-14` | `driver=metal`, window claimed | **runs** |
| `windows-latest` (MSVC) | `driver=direct3d12`, window claimed | **runs** |
| `macos-15-intel` | no device: "does not meet the hardware requirements for SDL_GPU Metal" | skips |
| `ubuntu-24.04`, `-arm` | no video device at all | skips |

Two runners running it is the point: the Direct3D path gets exercised rather
than only compiled, which no test in this repository has ever done.

**Decided 2026-09-21: only those two get the step.** The tier needs 2.3 GB of
`Data` (Step 3), and a runner that can only ever print
`skipped: no GPU device` would be fetching all of it for one line of output.
`macos-14` and `windows-latest` get the wide checkout and the step;
`ubuntu-24.04`, `ubuntu-24.04-arm` and `macos-15-intel` keep the narrow
checkout and the map file tier, which is what covers them. The GPU probe stays
in every job, so a runner that gains a device shows up in the probe's output
before anyone has to guess.

- [ ] **Step 3: Add the data the tier reads to the sparse checkout**

**Measured 2026-09-21, and it is nearly all of `Data`.** Built an installation
whose `Data` was a symlink farm and added trees until the tier passed, then took
them away again until it stopped. What it needs is 2.3 GB of the 2.8 GB tree:
everything except `movies`, `Music`, `Old`, `Bugs`, `Medals` and `Scenarios`.
The big items are `Units` 1.2 GB, `Buildings` 255 MB, `UI` 195 MB, `Sounds`
186 MB, `Maps` 122 MB, `Objects` 93 MB.

That is not the editor being greedy: `NMain::InitializeWithWindow` starts the
whole game. Along the way it wanted `Data/Cursor` for `ICursor::Init`,
`UI\escapemenu\EscapeMenuReactions.xml` for the message links,
`units\technics\common\tankpit.xml` for `CUnitCreation::InitConsts`,
`sin.arr` for `AILogic`'s trigonometry, and `Sounds` for the ack manager's
constants - each one found by running into it, because every one of them is an
unguarded dereference of the data tree that was not there rather than a message.

So the sparse checkout cannot be narrowed usefully, and the cost has to be paid
where the tier actually runs rather than in all five jobs. See the note under
Step 2.

- [ ] **Step 4: Run the branch through CI and read every job**

Expected: five jobs build the tier, `macos-14` and `windows-latest` (MSVC) run it, and the other three print `skipped: no GPU device`. Record in the commit which runners actually exercised it, because "green" and "ran" are different claims - and here three of five green jobs will not have run a thing.

If a runner that should have run it skips instead, that is a regression in the
skip logic and not a runner change: check it against the probe's output in the
same run, which is why the probe stays in every job.

**First run, 2026-09-21 (run 35628279394): both new jobs failed to build, and
neither failure was in the bridge.** `install-game` is the first thing in this
repository's CI that builds the game executable and the shadercross tool, so
two gaps that had never been exercised came out at once.

- macOS: `unable to find dynamic system library 'objc'` for the game, and every
  framework missing for shadercross. `addMacosSysrootPaths` added the SDK's
  framework directory but not its `usr/lib`, and shadercross - a host tool -
  was never given either, because `--sysroot` is global to the build and
  nothing had noticed. The library path has to be written *relative to the
  sysroot* while the framework path is absolute: zig prefixes `--sysroot` onto
  a library path and does not onto a framework path. Measured with a
  two-line C file - an absolute one comes out as `<sysroot>/<sysroot>/usr/lib`,
  warns `unable to open library directory`, and then finds nothing. Reproduce
  the whole thing locally with
  `zig build --sysroot "$(xcrun --show-sdk-path)" test-editor-bridge ...`,
  which is the only way to see it without pushing.
- Windows: the test executable would not link -  `VariantClear`,
  `_com_issue_error`, `CoCreateGuid`, `SysAllocString`. `Main` reaches COM
  through `Platform/LegacyVariant.h`, and every other module that links `Main`
  calls `linkComSupport`; the bridge test did not.

Note also that the workflow only triggers on `main` and on pull requests, so a
branch push runs nothing: use `gh workflow run "Cross-platform validation"
--ref <branch>`.

**Second run (35634565482): both got further and both failed again.** macOS
built the game and shadercross and then failed staging - `stage.zig` copies
`Data/THIRD-PARTY-NOTICES.txt` to the install root
(`tools/zig/verify_runtime.zig:40`) and the sparse patterns covered `.xml`,
`.arr` and `.spp` but not `.txt`. Windows got past COM and stopped on
`GetFileVersionInfoSizeA`, `GetFileVersionInfoA` and `VerQueryValueA` -
`version.lib`.

Each of those costs a full CI round to discover, so the fix is the general one
rather than the next symbol: the test executable hosts the same engine the game
does, so it links the same Windows imports `addGame` does, minus the
splash-screen resources a test has no window to show. Likewise the sparse
patterns now name every loose file at the top of `Data` - by name, because
`/Data/*` would match the directories and pull the whole tree back.

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

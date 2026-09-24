# Map Editor Plan 2: Map Files Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Read and write Blitzkrieg map files without a renderer, prove byte-level preservation over every shipped map on all six CI targets, and build the two things the editor will save with: the snapshot overlay and the terrain function.

**Architecture:** A new GFX-free C++ library, `Sources/src/MapFile`, holds three units. `MapFile` reads and writes `.bzm` and `.xml`, lifted from the game's reader and the MFC editor's writer. `MapEquivalence` compares two `SLoadMapInfo` values field by field and refuses to compile when a field is added to the struct. `MapOverlay` applies edits to a snapshot — object add/move/delete, diplomacy, and the deterministic terrain function — which is both what the bridge will save with in plan 3 and what the tests build their expected value with. A data-only startup wires the three StreamIO globals and one storage so all of this runs in a console test with no window and no GPU.

**Tech Stack:** C++17, Zig 0.16 build, the existing engine statics `Formats`, `RandomMapGen`, `Misc`, and the staged `StreamIO` shared library.

**Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (sections "Map files", "Saving: the snapshot and the overlay", "Terrain edits", "Testing → What equivalent means", "Testing → Test tiers and the M1 CI gate").

**Research:** `zig-out/local-test/mapfile-research.md` — measured call sites, fixture sizes and build patterns. Read it before Task 2.

## The M1 plan series

The spec's M1 is split into plans that each end in working, tested software. This is plan 2. The later ones are written when their predecessor has landed, so they can build on what was learned:

1. **Overlay spike** (landed, `0309dd8ff`): GFXGPU overlay hook, frame capture, vendored ImGui, the spike program.
2. **Map files** (this plan): `MapFile`, the data-only startup, the equivalence comparator, the snapshot overlay without the engine, the terrain function, the map-file test tier in CI.
3. **Engine bridge:** `NMain::InitializeWithWindow`, `Sources/src/EditorBridge` C ABI, building the engine state, editing calls, picking, camera, the engine test tier.
4. **Editor core:** Zig document, commands, undo and redo, tools, fake bridge, core tests on all six targets.
5. **Editor app:** window, frame loop, ImGui panels, `BK_EDITOR_AUTO`, save, autosave, test-launch, `install-map-editor`, packaging.

## Global Constraints

- All work happens on branch `feat/portable-map-editor` in the worktree `.worktrees/map-editor`. Never commit in the main checkout.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`.
- Never run `zig fmt` on `build.zig` or gate on `zig fmt --check` for it; edit it by hand.
- Test artifacts (readbacks, logs, map dumps) go under `zig-out/local-test/`, never `/tmp`.
- Build on macOS with `zig build <step> -Dtarget=aarch64-macos`. If the build fails with thousands of libc header errors (FP_ZERO, ldiv_t), the macOS SDK lookup is broken (`xcode-select`), not `build.zig`.
- Nothing in this plan may include a header from `Sources/src/GFX`, `Sources/src/GFXGPU`, `Sources/src/Scene` or `Sources/src/UI`. The tier's whole point is that it runs where there is no GPU.
- `NI_ASSERT_T` compiles away in release (`Misc/ModernAssert.h:57-58`). Never let an assert be the only thing standing between a null pointer and a dereference; write the check.
- Floats are compared with `==`. Nothing in this tier recomputes a float, so any difference is a bug, not a rounding artefact.
- Shipped map data under `Data/` is read-only. A test that writes a map writes it under `zig-out/local-test/`.

---

## File Structure

| File | Responsibility |
|---|---|
| `Sources/src/MapFile/MapFile.h/.cpp` | Read and write a map, both formats. Nothing else. |
| `Sources/src/MapFile/MapEquivalence.h/.cpp` | Compare two `SLoadMapInfo` values and say where they differ. |
| `Sources/src/MapFile/MapOverlay.h/.cpp` | Apply edits to a snapshot: objects, diplomacy, the terrain function. |
| `tools/zig/data_only_startup.h/.cpp` | Wire the StreamIO globals and one storage, with no window. |
| `tools/zig/map_file_test.cpp` | The map-file tier: round trips, edits, sweeps. |
| `build.zig` | `addMapFile` library, `addMapFileTest`, steps `test-map-files` and `test-map-files-all`. |
| `.github/workflows/cross-platform.yml` | Run `test-map-files` on all six targets. |

**Why `Sources/src/MapFile` and not `Sources/src/Formats` as the spec says.** `MapFile` must call `CMapInfo::IsValid`, `PackFrameIndices` and `UpdateTerrainCrosses`, which live in `RandomMapGen`, and `RandomMapGen` already includes `Formats/fmtMap.h`. Putting these files in the `Formats` library would make it depend on the library that depends on it. Task 1 amends the spec to the new path and records the reason.

---

### Task 1: Amend the spec with what the research found

**Files:**
- Modify: `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (section "Map files", around line 145)

**Interfaces:**
- Consumes: nothing.
- Produces: nothing in code. The two notes below are the premises Tasks 2-8 argue from, so they belong in the spec before any of them is written.

- [ ] **Step 1: Correct the unit's path and say why**

In the "Map files" section, replace the sentence

> M1 adds a small, GFX-free C++ unit, `Formats/MapFile.{h,cpp}`, with a reader and a writer for both formats.

with

```markdown
M1 adds a small, GFX-free C++ library, `Sources/src/MapFile`, with a reader
and a writer for both formats, an equivalence comparator, and the snapshot
overlay. It is its own library rather than part of `Formats` because it calls
`CMapInfo::IsValid`, `PackFrameIndices` and `UpdateTerrainCrosses`, which live
in `RandomMapGen` — and `RandomMapGen` already includes `Formats/fmtMap.h`, so
putting these files in `Formats` would make it depend on the library that
depends on it.
```

- [ ] **Step 2: Record the null-descriptor trap next to the frame-index rules**

At the end of the "Building the engine state" section, after the paragraph beginning "Every engine object is recorded against the snapshot object", add:

```markdown
**Frame indices and unknown types.** `CMapInfo::PackFrameIndices` and
`UnpackFrameIndices` (`RandomMapGen/MapInfo_StaticMethods.cpp:74-118`, loops at
606-638) look each object's type up with `pGDB->GetDesc( name )` and touch
`nFrameIndex` only for FENCE, ENTRENCHMENT and BRIDGE objects. For a name the
object database does not know, `GetDesc` returns null; the `NI_ASSERT_T` that
guards the next line compiles away in release (`Misc/ModernAssert.h:57-58`) and
the dereference crashes. Both functions iterate every object with no other
guard. So the editor never repacks a whole map: it packs only the objects it
added or edited, and only when their type is known. An unknown object's
`nFrameIndex` is written back exactly as it was read.
```

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
git commit -m "docs(editor): the map file unit's real home, and the frame-index null descriptor

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Data-only startup, and a map that reads

**Files:**
- Create: `tools/zig/data_only_startup.h`, `tools/zig/data_only_startup.cpp`
- Create: `Sources/src/MapFile/MapFile.h`, `Sources/src/MapFile/MapFile.cpp`
- Create: `tools/zig/map_file_test.cpp`
- Modify: `build.zig` (new `addMapFile` next to `addFormats` at line 3479; new `addMapFileTest` next to `addGameBootstrapSmoke` at line 5149; call both from the target loop near line 1413)

**Interfaces:**
- Consumes: `CMapInfo` (`RandomMapGen/MapInfo_Types.h`), `SLoadMapInfo` (`Formats/fmtMap.h`), `OpenFileStream`/`CreateFileStream`/`CreateStructureSaver`/`CreateDataTreeSaver` (`StreamIO/StructureSaver.h:94-121`).
- Produces:
  ```cpp
  // tools/zig/data_only_startup.h
  namespace NDataOnly
  {
      // Wires g_pGlobalSaveLoadSystem, g_pGlobalSingleton and
      // g_pfnGlobalGetTempRawBuffer from the staged StreamIO shared library, and
      // registers a storage rooted at szDataRoot so LoadDataResource works.
      // Returns false and writes the reason to stderr if either step fails.
      bool Start( const char *pszModuleRoot, const char *pszDataRoot );
  }

  // Sources/src/MapFile/MapFile.h
  namespace NMapFile
  {
      enum EFormat { FORMAT_BZM, FORMAT_XML };
      // Reads one map. szPath is a real filesystem path ending in .bzm or .xml.
      // Returns false and fills pszError when the file cannot be opened, the
      // stream throws, or CMapInfo::IsValid() is false.
      bool Read( const char *pszPath, CMapInfo *pMap, std::string *pError );
  }
  ```

- [ ] **Step 1: Write the failing test**

`tools/zig/map_file_test.cpp`:

```cpp
// The map file tier. Runs with no window and no GPU device: see
// tools/zig/data_only_startup.cpp for what "no window" costs.
#include "StdAfx.h"
#include <cstdio>
#include <cstring>
#include <string>
#include "data_only_startup.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"

static int g_nFailures = 0;

static bool Check( bool bCondition, const char *pszWhat )
{
    if ( !bCondition )
    {
        std::printf( "FAIL: %s\n", pszWhat );
        ++g_nFailures;
    }
    return bCondition;
}

static bool TestReadsASmallMap()
{
    CMapInfo map;
    std::string szError;
    // 157 KB, the smallest shipped map; see the research note's fixture list.
    // Backslashes: OpenFileStream splits a path on '\\' only
    // (StreamIO/StructureSaver.h:96), so a forward-slash path becomes one long
    // file name in a storage rooted at ".\". Every path in this tier is written
    // the way the engine writes them.
    const bool bRead = NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError );
    Check( bRead, szError.empty() ? "coldwinter.bzm reads" : szError.c_str() );
    if ( !bRead )
        return false;
    Check( map.terrain.tiles.GetSizeX() > 0, "the map has tiles" );
    Check( !map.terrain.szTilesetDesc.empty(), "the map names a tileset" );
    return true;
}

int main( int argc, char **argv )
{
    if ( !NDataOnly::Start( argc > 1 ? argv[1] : ".", "Data" ) )
        return 1;
    TestReadsASmallMap();
    if ( g_nFailures == 0 )
        std::printf( "map-file: PASS\n" );
    return g_nFailures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Run it to make sure it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails — neither `addMapFileTest` nor `MapFile.h` exists yet.

- [ ] **Step 3: Write the data-only startup**

`tools/zig/data_only_startup.h`:

```cpp
#ifndef __DATA_ONLY_STARTUP_H__
#define __DATA_ONLY_STARTUP_H__
namespace NDataOnly
{
    bool Start( const char *pszModuleRoot, const char *pszDataRoot );
}
#endif // __DATA_ONLY_STARTUP_H__
```

`tools/zig/data_only_startup.cpp`:

```cpp
// Everything the save/load system needs and nothing else. The game gets here
// through NMain::LoadAllModules, which loads every module and then calls
// EnsureGlobalHooks (Main/LoadDLLs.cpp:56-67); a map file test needs only the
// three globals that function wires, plus one storage for LoadDataResource.
// No window, no GFX, no object database - see the plan's Task 5 for why the
// object database stays out.
#include "StdAfx.h"
#include <cstdio>
#include <string>
#include "data_only_startup.h"
#include "../../Sources/src/Platform/DynamicLibrary.h"
#include "../../Sources/src/StreamIO/StreamIOTypes.h"
#include "../../Sources/src/StreamIO/StructureSaver.h"

typedef ISaveLoadSystem* (STDCALL *GETSLS_HOOK)();
typedef ISingleton* (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );

namespace NDataOnly
{
static std::string SharedLibraryName( const char *pszRoot )
{
    std::string szPath( pszRoot );
    if ( !szPath.empty() && szPath[szPath.size() - 1] != '/' && szPath[szPath.size() - 1] != '\\' )
        szPath += '/';
#if defined(_WIN32) || defined(_WIN64)
    return szPath + "StreamIO.dll";
#elif defined(__APPLE__)
    return szPath + "libStreamIO.dylib";
#else
    return szPath + "libStreamIO.so";
#endif
}

bool Start( const char *pszModuleRoot, const char *pszDataRoot )
{
    static NPlatform::DynamicLibrary streamio;
    const std::string szLibrary = SharedLibraryName( pszModuleRoot );
    if ( !streamio.IsLoaded() && !streamio.Load( szLibrary.c_str() ) )
    {
        std::fprintf( stderr, "data-only startup: cannot load %s: %s\n", szLibrary.c_str(), streamio.GetError() );
        return false;
    }
    if ( GETSLS_HOOK hook = reinterpret_cast<GETSLS_HOOK>( streamio.GetFunction( "GetSLS_Hook" ) ) )
        g_pGlobalSaveLoadSystem = hook();
    if ( GETSINGLETONGLOBAL_HOOK hook = reinterpret_cast<GETSINGLETONGLOBAL_HOOK>( streamio.GetFunction( "GetSingletonGlobal_Hook" ) ) )
        g_pGlobalSingleton = hook();
    g_pfnGlobalGetTempRawBuffer = reinterpret_cast<GETTEMPRAWBUFFER_HOOK>( streamio.GetFunction( "GetTempRawBuffer_Hook" ) );
    if ( GetSLS() == 0 || GetSingletonGlobal() == 0 || g_pfnGlobalGetTempRawBuffer == 0 )
    {
        std::fprintf( stderr, "data-only startup: %s loaded but a hook is missing\n", szLibrary.c_str() );
        return false;
    }
    // LoadDataResource (tilesets, crossets) reads storage-relative names.
    CPtr<IDataStorage> pStorage = OpenStorage( pszDataRoot, STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
    if ( pStorage == 0 )
    {
        std::fprintf( stderr, "data-only startup: cannot open storage at %s\n", pszDataRoot );
        return false;
    }
    RegisterSingleton( IDataStorage::tidTypeID, pStorage );
    return true;
}
}
```

- [ ] **Step 4: Write the reader**

`Sources/src/MapFile/MapFile.h`:

```cpp
#ifndef __MAP_FILE_H__
#define __MAP_FILE_H__
#include <string>
class CMapInfo;
namespace NMapFile
{
    enum EFormat { FORMAT_BZM, FORMAT_XML };
    bool Read( const char *pszPath, CMapInfo *pMap, std::string *pError );
}
#endif // __MAP_FILE_H__
```

`Sources/src/MapFile/MapFile.cpp`:

```cpp
// The game's reader (GameTT/iMissionInternal.cpp:1362-1392) and the MFC
// editor's writer (MapEditor/TemplateEditorFrame1.cpp:3238-3258), lifted into
// something with no UI and no renderer under it. Unlike the MFC editor this
// does not call RemoveNonExistingObjects: a map the editor could not fully
// understand still has to come back out unchanged.
#include "StdAfx.h"
#include "MapFile.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../StreamIO/StructureSaver.h"

namespace NMapFile
{
static bool HasExtension( const char *pszPath, const char *pszExtension )
{
    const size_t nPath = std::strlen( pszPath ), nExt = std::strlen( pszExtension );
    if ( nPath < nExt )
        return false;
    return NStr::CompareAsciiNoCase( pszPath + nPath - nExt, pszExtension ) == 0;
}

bool Read( const char *pszPath, CMapInfo *pMap, std::string *pError )
{
    if ( pszPath == 0 || pMap == 0 )
        return false;
    const bool bXml = HasExtension( pszPath, ".xml" );
    if ( !bXml && !HasExtension( pszPath, ".bzm" ) )
    {
        if ( pError ) *pError = std::string( pszPath ) + ": not a .bzm or .xml map";
        return false;
    }
    try
    {
        CPtr<IDataStream> pStream = OpenFileStream( pszPath, STREAM_ACCESS_READ );
        if ( pStream == 0 )
        {
            if ( pError ) *pError = std::string( pszPath ) + ": cannot open";
            return false;
        }
        if ( bXml )
        {
            CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
            saver.AddTypedSuper( pMap );
        }
        else
        {
            CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
            CSaverAccessor saver = pSaver;
            saver.Add( 1, pMap );
        }
    }
    catch ( ... )
    {
        if ( pError ) *pError = std::string( pszPath ) + ": the stream threw while reading";
        return false;
    }
    if ( !pMap->IsValid() )
    {
        if ( pError ) *pError = std::string( pszPath ) + ": CMapInfo::IsValid() is false";
        return false;
    }
    return true;
}
}
```

- [ ] **Step 5: Add the library and the test to build.zig**

Next to `addFormats` (line 3479), a library that links what `MapFile` needs:

```zig
fn addMapFile(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const map_file_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, map_file_module);
    addMsvcIncludePaths(b, map_file_module, toolchain);
    map_file_module.addIncludePath(b.path("Sources/src/Formats"));
    map_file_module.addIncludePath(b.path("Sources/src/RandomMapGen"));
    map_file_module.addIncludePath(b.path("Sources/src/Common"));
    map_file_module.addIncludePath(b.path("Sources/src/Main"));
    map_file_module.addIncludePath(b.path("Sources/src/Image"));
    map_file_module.addCSourceFiles(.{
        .files = &.{"Sources/src/MapFile/MapFile.cpp"},
        .flags = cppflagsForOptimize(optimize),
    });
    return b.addLibrary(.{
        .name = "MapFile",
        .linkage = .static,
        .root_module = map_file_module,
    });
}
```

Next to `addGameBootstrapSmoke` (line 5149), the test executable. It links the engine statics and runs with `StreamIO` staged beside it, exactly as `console_bridge_test` does (build.zig:2105-2115):

```zig
fn addMapFileTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    map_file: *std.Build.Step.Compile,
    formats: *std.Build.Step.Compile,
    randommapgen: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
    stream_io: *std.Build.Step.Compile,
    test_mode: build_support.TestMode,
) void {
    const module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    addProjectIncludePaths(b, module);
    addMsvcIncludePaths(b, module, toolchain);
    module.addIncludePath(b.path("Sources/src"));
    module.addCSourceFiles(.{
        .files = &.{ "tools/zig/data_only_startup.cpp", "tools/zig/map_file_test.cpp" },
        .flags = cppflagsForOptimize(optimize),
    });
    module.linkLibrary(map_file);
    module.linkLibrary(randommapgen);
    module.linkLibrary(formats);
    module.linkLibrary(misc);
    switch (target.result.os.tag) {
        .windows => {
            addMsvcLibraryPaths(b, module, toolchain);
            linkMsvcRuntime(module, optimize);
        },
        .linux => module.linkSystemLibrary("stdc++", .{}),
        .macos => module.linkSystemLibrary("c++", .{}),
        else => {},
    }
    const exe = b.addExecutable(.{ .name = "map-file-test", .root_module = module });
    exe.subsystem = .console;
    if (target.result.os.tag == .windows) exe.entry = .{ .symbol_name = "mainCRTStartup" };

    // The test loads StreamIO by path at run time; hand it zig-out/bin.
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.addArg(b.path("zig-out/bin").getPath(b));
    run.step.dependOn(&b.addInstallArtifact(stream_io, .{}).step);
    const step = b.step("test-map-files", "Read and rewrite the shipped maps; check they are unchanged");
    step.dependOn(&exe.step);
    if (test_mode == .run) step.dependOn(&run.step);

    const run_all = b.addRunArtifact(exe);
    run_all.setCwd(b.path("."));
    run_all.addArg(b.path("zig-out/bin").getPath(b));
    run_all.addArg("--all");
    run_all.step.dependOn(&b.addInstallArtifact(stream_io, .{}).step);
    const step_all = b.step("test-map-files-all", "Sweep every shipped map, not just the CI sample");
    step_all.dependOn(&exe.step);
    if (test_mode == .run) step_all.dependOn(&run_all.step);
}
```

Call both from the target loop, next to `addGameBootstrapSmoke` at line 1413:

```zig
    const map_file = addMapFile(b, target, optimize, toolchain);
    addMapFileTest(b, target, optimize, toolchain, map_file, formats, randommapgen, misc, stream_io, test_mode);
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

If it fails with a missing `GetSLS()`, `StreamIO` is not staged in `zig-out/bin`; run `zig build install-game -Dtarget=aarch64-macos` once first and check `zig-out/bin` holds the shared library.

- [ ] **Step 7: Commit**

```bash
git add tools/zig/data_only_startup.h tools/zig/data_only_startup.cpp tools/zig/map_file_test.cpp Sources/src/MapFile/MapFile.h Sources/src/MapFile/MapFile.cpp build.zig
git commit -m "feat(editor): a map reads with no window and no GPU device

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Read `.xml`, and pick a format the way the game does

**Files:**
- Modify: `Sources/src/MapFile/MapFile.h`, `Sources/src/MapFile/MapFile.cpp`
- Modify: `tools/zig/map_file_test.cpp`

**Interfaces:**
- Consumes: `NMapFile::Read` from Task 2.
- Produces:
  ```cpp
  // Picks the newer of <szBase>.xml and <szBase>.bzm, as the game does
  // (GameTT/iMissionInternal.cpp:1370-1381), and reads it. szBase carries no
  // extension. Returns false when neither exists.
  bool ReadNewest( const char *pszBase, CMapInfo *pMap, std::string *pError );
  ```

- [ ] **Step 1: Write the failing test**

Append to `tools/zig/map_file_test.cpp`, and call it from `main` after `TestReadsASmallMap`:

```cpp
static void TestReadsXmlAndPicksTheNewer()
{
    CMapInfo fromXml;
    std::string szError;
    // Data\\Maps ships exactly two .xml maps: river3d and road3d.
    Check( NMapFile::Read( "Data\\Maps\\river3d.xml", &fromXml, &szError ) ||
           szError.find( "cannot open" ) != std::string::npos,
           "an .xml map reads, or says plainly that it is not there" );

    CMapInfo newest;
    szError.clear();
    Check( NMapFile::ReadNewest( "Data\\Maps\\Multiplayer\\coldwinter", &newest, &szError ),
           szError.empty() ? "a map name with no extension reads" : szError.c_str() );
    Check( newest.terrain.tiles.GetSizeX() > 0, "the map picked by mtime has tiles" );

    CMapInfo missing;
    szError.clear();
    Check( !NMapFile::ReadNewest( "Data\\Maps\\Multiplayer\\no_such_map", &missing, &szError ),
           "a map that is not there fails" );
    Check( !szError.empty(), "and says so" );
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails with "no member named 'ReadNewest'".

- [ ] **Step 3: Implement it**

Add to `MapFile.h` inside `namespace NMapFile`:

```cpp
    bool ReadNewest( const char *pszBase, CMapInfo *pMap, std::string *pError );
```

Add to `MapFile.cpp`:

```cpp
// The game's rule, through the game's mechanism: ask the registered storage
// for both files' stats and take the newer (iMissionInternal.cpp:1370-1381).
// .xml wins only when it is strictly newer, which is what lets a freshly
// written .bzm take precedence over a stale .xml beside it. Zeroing the stats
// first matters - GetStreamStats leaves them untouched for a file that is not
// there, and an uninitialised mtime compares any way it likes.
bool ReadNewest( const char *pszBase, CMapInfo *pMap, std::string *pError )
{
    if ( pszBase == 0 || pMap == 0 )
        return false;
    IDataStorage *pStorage = GetSingleton<IDataStorage>();
    if ( pStorage == 0 )
    {
        if ( pError ) *pError = "no storage is registered; call NDataOnly::Start first";
        return false;
    }
    const std::string szXml = std::string( pszBase ) + ".xml";
    const std::string szBzm = std::string( pszBase ) + ".bzm";
    SStorageElementStats statsXml, statsBzm;
    Zero( statsXml );
    Zero( statsBzm );
    pStorage->GetStreamStats( szXml.c_str(), &statsXml );
    pStorage->GetStreamStats( szBzm.c_str(), &statsBzm );
    if ( statsXml.mtime == 0 && statsBzm.mtime == 0 )
    {
        if ( pError ) *pError = std::string( pszBase ) + ": neither .xml nor .bzm is there";
        return false;
    }
    return Read( ( statsXml.mtime > statsBzm.mtime ? szXml : szBzm ).c_str(), pMap, pError );
}
```

`ReadNewest` therefore takes a **storage-relative** base name (`Data\\Maps\\Multiplayer\\coldwinter`), not an absolute path, because that is what `GetStreamStats` resolves. `Read` keeps taking a path, since it opens its own scoped storage through `OpenFileStream`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/MapFile/MapFile.h Sources/src/MapFile/MapFile.cpp tools/zig/map_file_test.cpp
git commit -m "feat(editor): the map reader takes .xml, and picks a format by mtime as the game does

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Write a map back

**Files:**
- Modify: `Sources/src/MapFile/MapFile.h`, `Sources/src/MapFile/MapFile.cpp`
- Modify: `tools/zig/map_file_test.cpp`

**Interfaces:**
- Consumes: `NMapFile::Read` (Task 2).
- Produces:
  ```cpp
  // Writes pMap to szPath. The format comes from the extension. Frame indices
  // are written exactly as they stand in pMap - this never calls
  // PackFrameIndices, see the spec's "Frame indices and unknown types".
  // Both formats carry the SQuickLoadMapInfo chunk the MFC editor writes.
  bool Write( const char *pszPath, const CMapInfo &rMap, std::string *pError );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
static void TestWritesWhatItRead()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), "read for write test" ) )
        return;
    const char *pszOut = "zig-out\\local-test\\coldwinter-roundtrip.bzm";
    Check( NMapFile::Write( pszOut, map, &szError ), szError.empty() ? "the map writes" : szError.c_str() );
    CMapInfo reread;
    szError.clear();
    Check( NMapFile::Read( pszOut, &reread, &szError ), szError.empty() ? "what was written reads" : szError.c_str() );
    Check( reread.objects.size() == map.objects.size(), "the same number of objects came back" );
    Check( reread.terrain.tiles.GetSizeX() == map.terrain.tiles.GetSizeX(), "the terrain is the same size" );
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails with "no member named 'Write'".

- [ ] **Step 3: Implement the writer**

Add to `MapFile.h`:

```cpp
    bool Write( const char *pszPath, const CMapInfo &rMap, std::string *pError );
```

Add to `MapFile.cpp`:

```cpp
// The MFC editor's writer (TemplateEditorFrame1.cpp:3238-3258) without the
// PackFrameIndices call in front of it: what is in pMap is what goes to disk.
// SQuickLoadMapInfo::FillFromMapInfo (MapInfo_Methods.cpp:15-29) reads only
// fields of the map, so the quick chunk needs no object database either.
bool Write( const char *pszPath, const CMapInfo &rMap, std::string *pError )
{
    if ( pszPath == 0 )
        return false;
    const bool bXml = HasExtension( pszPath, ".xml" );
    if ( !bXml && !HasExtension( pszPath, ".bzm" ) )
    {
        if ( pError ) *pError = std::string( pszPath ) + ": not a .bzm or .xml map";
        return false;
    }
    try
    {
        SQuickLoadMapInfo quickLoadMapInfo;
        quickLoadMapInfo.FillFromMapInfo( rMap );
        CPtr<IDataStream> pStream = CreateFileStream( pszPath, STREAM_ACCESS_WRITE );
        if ( pStream == 0 )
        {
            if ( pError ) *pError = std::string( pszPath ) + ": cannot create";
            return false;
        }
        // The savers take a non-const reference; the map is not modified.
        CMapInfo &rWritable = const_cast<CMapInfo&>( rMap );
        if ( bXml )
        {
            CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
            CTreeAccessor saver = pSaver;
            saver.AddTypedSuper( &rWritable );
            saver.Add( RMGC_QUICK_LOAD_MAP_INFO_NAME, &quickLoadMapInfo );
        }
        else
        {
            CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
            CSaverAccessor saver = pSaver;
            saver.Add( 1, &rWritable );
            saver.Add( RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, &quickLoadMapInfo );
        }
    }
    catch ( ... )
    {
        if ( pError ) *pError = std::string( pszPath ) + ": the stream threw while writing";
        return false;
    }
    return true;
}
```

`RMGC_QUICK_LOAD_MAP_INFO_NAME` and `RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER` come from `RandomMapGen/MapInfo_Consts.cpp:28-29`; include whichever header declares them (`MapInfo_Types.h` or `MapInfo_Consts.h`).

- [ ] **Step 4: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/MapFile/MapFile.h Sources/src/MapFile/MapFile.cpp tools/zig/map_file_test.cpp
git commit -m "feat(editor): a map writes back, in both formats, with its quick-load chunk

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: The equivalence comparator

**Files:**
- Create: `Sources/src/MapFile/MapEquivalence.h`, `Sources/src/MapFile/MapEquivalence.cpp`
- Modify: `build.zig` (add `MapEquivalence.cpp` to `addMapFile`'s `.files`)
- Modify: `tools/zig/map_file_test.cpp`

**Interfaces:**
- Consumes: `SLoadMapInfo` (`Formats/fmtMap.h:372-405`) and every nested struct the spec's "What equivalent means" lists.
- Produces:
  ```cpp
  // Compares two maps field by field. Returns true when they are equivalent.
  // On a difference, fills pWhere with a path like
  // "terrain.patches[3][7].crosses.base[2]" and returns false. Stops at the
  // first difference: a sweep wants a name, not a diff.
  bool AreEquivalent( const SLoadMapInfo &rLeft, const SLoadMapInfo &rRight, std::string *pWhere );
  ```

- [ ] **Step 1: Write the failing test**

```cpp
static void TestComparatorSeesADifference()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), "read for comparator test" ) )
        return;
    CMapInfo same = map;
    std::string szWhere;
    Check( NMapFile::AreEquivalent( map, same, &szWhere ), "a copy is equivalent" );

    CMapInfo moved = map;
    if ( !moved.objects.empty() )
    {
        moved.objects[0].vPos.x += 1.0f;
        szWhere.clear();
        Check( !NMapFile::AreEquivalent( map, moved, &szWhere ), "a moved object is not equivalent" );
        Check( szWhere.find( "objects[0].vPos" ) != std::string::npos, "and the comparator names the field" );
    }

    CMapInfo repainted = map;
    if ( repainted.terrain.tiles.GetSizeX() > 0 )
    {
        repainted.terrain.tiles[0][0].tile = BYTE( repainted.terrain.tiles[0][0].tile + 1 );
        szWhere.clear();
        Check( !NMapFile::AreEquivalent( map, repainted, &szWhere ), "a changed tile is not equivalent" );
        Check( szWhere.find( "terrain.tiles" ) != std::string::npos, "and it names the tile" );
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails with "no member named 'AreEquivalent'".

- [ ] **Step 3: Write the comparator**

`Sources/src/MapFile/MapEquivalence.h`:

```cpp
#ifndef __MAP_EQUIVALENCE_H__
#define __MAP_EQUIVALENCE_H__
#include <string>
struct SLoadMapInfo;
namespace NMapFile
{
    bool AreEquivalent( const SLoadMapInfo &rLeft, const SLoadMapInfo &rRight, std::string *pWhere );
}
#endif // __MAP_EQUIVALENCE_H__
```

`Sources/src/MapFile/MapEquivalence.cpp` opens with the guard that makes a new field impossible to miss, then compares every member of `SLoadMapInfo` in declaration order. Floats compare with `==`: nothing here recomputes one.

```cpp
// A field-by-field comparison of two maps, in the declaration order of
// SLoadMapInfo (Formats/fmtMap.h:372-405).
//
// The static_assert below is the point of the file. Add a field to
// SLoadMapInfo and this stops compiling, which is the only way a new field
// cannot slip silently past the preservation tests. When it fires: add the
// field to CompareMap, then update the size.
#include "StdAfx.h"
#include "MapEquivalence.h"
#include "../Formats/fmtMap.h"
#include <cstdio>

namespace NMapFile
{
// sizeof is checked per pointer width, since the struct is full of containers.
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
static_assert( sizeof( SLoadMapInfo ) == BK_SLOADMAPINFO_SIZE_64,
               "SLoadMapInfo changed: add the new field to MapEquivalence.cpp, then update BK_SLOADMAPINFO_SIZE_64" );
#endif

namespace {
struct CPath
{
    std::string *pWhere;
    std::string szPrefix;
    CPath( std::string *_pWhere, const std::string &_szPrefix ) : pWhere( _pWhere ), szPrefix( _szPrefix ) {  }
    bool Fail( const std::string &szField ) const
    {
        if ( pWhere ) *pWhere = szPrefix.empty() ? szField : szPrefix + "." + szField;
        return false;
    }
    CPath Under( const std::string &szField ) const
    {
        return CPath( pWhere, szPrefix.empty() ? szField : szPrefix + "." + szField );
    }
    static std::string Index( const std::string &szField, int nIndex )
    {
        char szBuffer[32];
        std::snprintf( szBuffer, sizeof szBuffer, "[%d]", nIndex );
        return szField + szBuffer;
    }
};
}
```

Write one `Compare<Struct>( left, right, path )` per struct the spec lists, each returning `false` through `path.Fail( "<field>" )`. Do them in this order, so a reviewer can tick them off against the spec's list: `SCrossTileInfo`, `SVertexAltitude`, `STerrainPatchInfo`, `STerrainInfo`, `SLinkInfo`, `SMapObjectInfo`, `SEntrenchmentInfo`, `SReinforcementGroupInfo`, `SUCAircraft`, `SUCAviation`, `SUnitCreation`, `SUnitCreationInfo`, `SAIStartCommand`, `SBattlePosition`, `SSoundInfo`, `SMapSoundInfo`, `SAIGeneralParcelInfo`, `SAIGeneralSideInfo`, `SAIGeneralMapInfo`, `SScriptArea`, and finally `SLoadMapInfo`. Their field lists are in `Formats/fmtMap.h`, `Formats/fmtUnitCreation.h` and `Formats/fmtAIGeneral.h` at the lines the research note records.

Two of them carry the traps:

```cpp
// STerrainInfo: tiles is a 2-D array, patches is a 2-D array of structs.
// Compare the sizes first - a size mismatch indexed blindly is a crash, not a
// failed test.
static bool CompareTerrain( const STerrainInfo &l, const STerrainInfo &r, const CPath &path )
{
    if ( l.szTilesetDesc != r.szTilesetDesc ) return path.Fail( "szTilesetDesc" );
    if ( l.szCrossetDesc != r.szCrossetDesc ) return path.Fail( "szCrossetDesc" );
    if ( l.szNoise != r.szNoise ) return path.Fail( "szNoise" );
    if ( l.tiles.GetSizeX() != r.tiles.GetSizeX() || l.tiles.GetSizeY() != r.tiles.GetSizeY() )
        return path.Fail( "tiles.size" );
    for ( int y = 0; y < l.tiles.GetSizeY(); ++y )
        for ( int x = 0; x < l.tiles.GetSizeX(); ++x )
            if ( l.tiles[y][x].tile != r.tiles[y][x].tile || l.tiles[y][x].noise != r.tiles[y][x].noise )
                return path.Fail( CPath::Index( CPath::Index( "tiles", y ), x ) );
    if ( l.patches.GetSizeX() != r.patches.GetSizeX() || l.patches.GetSizeY() != r.patches.GetSizeY() )
        return path.Fail( "patches.size" );
    for ( int y = 0; y < l.patches.GetSizeY(); ++y )
        for ( int x = 0; x < l.patches.GetSizeX(); ++x )
            if ( !ComparePatch( l.patches[y][x], r.patches[y][x],
                                path.Under( CPath::Index( CPath::Index( "patches", y ), x ) ) ) )
                return false;
    if ( l.altitudes.GetSizeX() != r.altitudes.GetSizeX() || l.altitudes.GetSizeY() != r.altitudes.GetSizeY() )
        return path.Fail( "altitudes.size" );
    for ( int y = 0; y < l.altitudes.GetSizeY(); ++y )
        for ( int x = 0; x < l.altitudes.GetSizeX(); ++x )
            if ( !CompareAltitude( l.altitudes[y][x], r.altitudes[y][x],
                                   path.Under( CPath::Index( CPath::Index( "altitudes", y ), x ) ) ) )
                return false;
    if ( l.rivers != r.rivers ) return path.Fail( "rivers" );
    if ( l.roads3 != r.roads3 ) return path.Fail( "roads3" );
    return true;
}

// SMapObjectInfo: the packed frame index is compared, never the unpacked one.
static bool CompareObject( const SMapObjectInfo &l, const SMapObjectInfo &r, const CPath &path )
{
    if ( l.szName != r.szName ) return path.Fail( "szName" );
    if ( l.vPos.x != r.vPos.x || l.vPos.y != r.vPos.y || l.vPos.z != r.vPos.z ) return path.Fail( "vPos" );
    if ( l.nDir != r.nDir ) return path.Fail( "nDir" );
    if ( l.nPlayer != r.nPlayer ) return path.Fail( "nPlayer" );
    if ( l.nScriptID != r.nScriptID ) return path.Fail( "nScriptID" );
    if ( l.fHP != r.fHP ) return path.Fail( "fHP" );
    if ( l.nFrameIndex != r.nFrameIndex ) return path.Fail( "nFrameIndex" );
    if ( l.link.nLinkID != r.link.nLinkID ) return path.Fail( "link.nLinkID" );
    if ( l.link.bIntention != r.link.bIntention ) return path.Fail( "link.bIntention" );
    if ( l.link.nLinkWith != r.link.nLinkWith ) return path.Fail( "link.nLinkWith" );
    return true;
}
```

If `SCrossTileInfo` or `SVertexAltitude` has no `operator==`, compare their members by hand rather than adding one: an `operator==` on a format struct is a thing other code can start relying on by accident.

Task 8's paint test wants one of these on its own, so expose it beside `AreEquivalent`:

```cpp
// True when two terrains have the same altitudes, height and shade, everywhere.
// The paint tests assert this: the terrain function must never touch them.
bool CompareAltitudeArrays( const STerrainInfo &rLeft, const STerrainInfo &rRight );
```

For `BK_SLOADMAPINFO_SIZE_64`, print `sizeof(SLoadMapInfo)` once from the test, then put the number in the header as a `const size_t` with a comment giving the date and the value.

- [ ] **Step 4: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 5: Commit**

```bash
git add Sources/src/MapFile/MapEquivalence.h Sources/src/MapFile/MapEquivalence.cpp tools/zig/map_file_test.cpp build.zig
git commit -m "feat(editor): two maps compare field by field, and a new field breaks the build

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: The sweep, and the CI tier

**Files:**
- Modify: `tools/zig/map_file_test.cpp`
- Modify: `.github/workflows/cross-platform.yml` (one step per target, beside the existing `test-gfxgpu-core` steps at lines 89, 222, 324 and the Linux arm/mingw blocks)

**Interfaces:**
- Consumes: `NMapFile::Read`, `Write`, `AreEquivalent`.
- Produces: the steps `test-map-files` (the CI sample) and `test-map-files-all` (every shipped map), already declared in Task 2.

- [ ] **Step 1: Write the failing test**

```cpp
// Read a map, write it untouched, read it back: equivalent. Then write it a
// second time and compare the two files byte for byte - the spec's idempotent
// save. A map that survives both has not been quietly normalised.
static void TestRoundTrip( const char *pszPath )
{
    CMapInfo original;
    std::string szError;
    if ( !Check( NMapFile::Read( pszPath, &original, &szError ), szError.empty() ? pszPath : szError.c_str() ) )
        return;
    const std::string szFirst = std::string( "zig-out\\local-test\\roundtrip-1" ) + Extension( pszPath );
    const std::string szSecond = std::string( "zig-out\\local-test\\roundtrip-2" ) + Extension( pszPath );
    if ( !Check( NMapFile::Write( szFirst.c_str(), original, &szError ), szError.c_str() ) )
        return;
    CMapInfo reread;
    szError.clear();
    if ( !Check( NMapFile::Read( szFirst.c_str(), &reread, &szError ), szError.c_str() ) )
        return;
    std::string szWhere;
    if ( !NMapFile::AreEquivalent( original, reread, &szWhere ) )
        Check( false, ( std::string( pszPath ) + ": differs at " + szWhere ).c_str() );
    if ( !Check( NMapFile::Write( szSecond.c_str(), reread, &szError ), szError.c_str() ) )
        return;
    Check( FilesAreIdentical( szFirst.c_str(), szSecond.c_str() ),
           ( std::string( pszPath ) + ": saving twice gave two different files" ).c_str() );

    // The spec's third check: the quick-load chunk on disk has to describe the
    // map that is on disk with it. A stale quick chunk is how a map list ends
    // up showing the wrong player count for a map the editor touched.
    SQuickLoadMapInfo quickOnDisk, quickExpected;
    if ( Check( NMapFile::ReadQuickInfo( szFirst.c_str(), &quickOnDisk, &szError ), szError.c_str() ) )
    {
        quickExpected.FillFromMapInfo( reread );
        Check( QuickInfoEqual( quickOnDisk, quickExpected ),
               ( std::string( pszPath ) + ": the quick-load chunk does not match the map beside it" ).c_str() );
    }
}
```

`ReadQuickInfo` is the fourth entry point on `NMapFile`; add it to `MapFile.h` and `MapFile.cpp` in this step:

```cpp
// Reads only the quick-load chunk: chunk RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER
// of a .bzm, or RMGC_QUICK_LOAD_MAP_INFO_NAME of an .xml.
bool ReadQuickInfo( const char *pszPath, SQuickLoadMapInfo *pQuick, std::string *pError );
```

`QuickInfoEqual` is a local helper in the test comparing every member of `SQuickLoadMapInfo` (`RandomMapGen/MapInfo_Types.h`; `FillFromMapInfo` is at `MapInfo_Methods.cpp:15-29` and shows which members there are).

Write the two helpers beside it: `Extension` returns `".bzm"` or `".xml"` from the path, and `FilesAreIdentical` reads both files through `OpenFileStream` and compares length then bytes in 64 KB blocks.

Then the sweep, driven by `--all`:

```cpp
// The CI sample: all 59 .bzm and 2 .xml maps in Data\\Maps, plus seven of the
// 1,696 scenario patches, so the tier covers scenario maps without reading
// 144 MB on six runners. The seven were taken with
//   find Data/Scenarios -name '*.bzm' | sort | awk 'NR%250==1'
// which spreads them over the seasons and patch kinds; regenerate the list the
// same way if the scenario set changes. --all sweeps everything; that is the
// local step, test-map-files-all.
static const char *g_pszScenarioSample[] = {
    "Data\\Scenarios\\Patches\\Africa\\p_settle_E_1.bzm",
    "Data\\Scenarios\\Patches\\common\\road_junc\\winter\\p_junc_gr_asph_W_1.bzm",
    "Data\\Scenarios\\Patches\\spring_Ukraine\\p_army_N_2.bzm",
    "Data\\Scenarios\\Patches\\spring_Ukraine\\p_troops_gr_sw_1_2.bzm",
    "Data\\Scenarios\\Patches\\summer_Russia\\p_ambush_gr_NS_1.bzm",
    "Data\\Scenarios\\Patches\\summer_Ukraine\\p_lg_village_a_10.bzm",
    "Data\\Scenarios\\Patches\\winter_Russia\\p_bridge_rail_n_4.bzm",
};

static void SweepMaps( bool bAll )
{
    std::vector<std::string> paths;
    CollectMaps( "Data/Maps", &paths );
    if ( bAll )
        CollectMaps( "Data/Scenarios", &paths );
    else
        for ( size_t i = 0; i < sizeof( g_pszScenarioSample ) / sizeof( g_pszScenarioSample[0] ); ++i )
            paths.push_back( g_pszScenarioSample[i] );
    std::printf( "map-file: sweeping %d maps\n", int( paths.size() ) );
    for ( size_t i = 0; i < paths.size(); ++i )
        TestRoundTrip( paths[i].c_str() );
}
```

`CollectMaps` walks a directory for `.bzm` and `.xml` through the platform's directory iteration (`Sources/src/Platform/Files.h`), never `<dirent.h>` directly.

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails on the missing helpers, or the sweep reports the first map that differs. A real difference here is the interesting outcome: record which map and which field before changing anything.

- [ ] **Step 3: Make the sweep pass**

Fix what it finds. The likely causes, in the order they are worth checking:
- a field the comparator reads but the writer does not write (compare `MapEquivalence.cpp` against `fmtMap.h`'s `operator&`);
- a container the writer writes in a different order than it read;
- a map that genuinely fails `IsValid` — skip it by name with a comment saying which and why, rather than loosening `IsValid`.

- [ ] **Step 4: Run the sweep and the full sweep**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: sweeping <n> maps` then `map-file: PASS`

Run: `zig build test-map-files-all -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the same, over about 1,755 maps. Note the wall-clock time in the commit message; if it runs over ten minutes, say so, because that decides whether it can ever move into CI.

- [ ] **Step 5: Add the tier to CI**

In `.github/workflows/cross-platform.yml`, after each target's `test-gfxgpu-core` step, add one step in that target's style. Linux x86_64 (line ~89):

```yaml
      - name: Map file tier
        run: zig build test-map-files -Dtarget=x86_64-linux-gnu -Dtest-mode=run
```

Windows MSVC (line ~222) takes the four toolchain `-D` flags the neighbouring steps pass; macOS (line ~324) takes `--sysroot "$MACOS_SYSROOT"`; Linux arm64 (line ~420) and the mingw job follow their own neighbours. Use `-Dtest-mode=run` everywhere: this tier needs no GPU, so there is no reason for any target to only compile it.

- [ ] **Step 6: Commit**

```bash
git add tools/zig/map_file_test.cpp .github/workflows/cross-platform.yml
git commit -m "test(editor): every shipped map survives a read and a write, on all six targets

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: The object and diplomacy overlay

**Files:**
- Create: `Sources/src/MapFile/MapOverlay.h`, `Sources/src/MapFile/MapOverlay.cpp`
- Modify: `build.zig` (add `MapOverlay.cpp` to `addMapFile`'s `.files`)
- Modify: `tools/zig/map_file_test.cpp`

**Interfaces:**
- Consumes: `NMapFile::AreEquivalent` (Task 5), `CMapInfo::GetUsedLinkIDs` (`MapInfo_Types.h:243`).
- Produces:
  ```cpp
  namespace NMapOverlay
  {
      struct SAddObject { std::string szName; CVec3 vPos; WORD nDir; int nPlayer; bool bScenario; };
      struct SMoveObject { int nLinkID; CVec3 vPos; WORD nDir; int nPlayer; };

      // The link ID a new object gets: one above every ID in use.
      int NextLinkID( const SLoadMapInfo &rMap );

      // Lists every record that refers to nLinkID: bridges, start commands,
      // reinforcements, groups, and objects linked with it. Empty means the
      // object can be deleted.
      void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences );

      // Apply an edit to pMap in place. Delete returns false and fills
      // pRefusal when something refers to the object.
      bool AddObject( SLoadMapInfo *pMap, const SAddObject &rAdd, int *pnLinkID );
      bool MoveObject( SLoadMapInfo *pMap, const SMoveObject &rMove );
      bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal );
      bool SetDiplomacy( SLoadMapInfo *pMap, int nPlayer, BYTE nDiplomacy );
  }
  ```

- [ ] **Step 1: Write the failing test**

```cpp
static void TestObjectOverlay()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    const CMapInfo original = map;

    // An added object lands at the end of its list with a fresh link ID.
    NMapOverlay::SAddObject add;
    add.szName = original.objects.empty() ? std::string( "Unknown_Test_Object" ) : original.objects[0].szName;
    add.vPos = CVec3( 100.0f, 100.0f, 0.0f );
    add.nDir = 0;
    add.nPlayer = 0;
    add.bScenario = false;
    int nNewLinkID = -1;
    Check( NMapOverlay::AddObject( &map, add, &nNewLinkID ), "an object is added" );
    Check( map.objects.size() == original.objects.size() + 1, "at the end of the list" );
    Check( nNewLinkID > NMapOverlay::NextLinkID( original ) - 1, "with a link ID above every one in use" );

    // A moved object keeps its record and changes three fields.
    if ( !original.objects.empty() )
    {
        NMapOverlay::SMoveObject move;
        move.nLinkID = original.objects[0].link.nLinkID;
        move.vPos = CVec3( original.objects[0].vPos.x + 32.0f, original.objects[0].vPos.y, original.objects[0].vPos.z );
        move.nDir = WORD( original.objects[0].nDir + 1024 );
        move.nPlayer = original.objects[0].nPlayer;
        Check( NMapOverlay::MoveObject( &map, move ), "an object moves" );
        Check( map.objects[0].nFrameIndex == original.objects[0].nFrameIndex, "and keeps its packed frame index" );
        Check( map.objects[0].fHP == original.objects[0].fHP, "and its HP" );
        Check( map.objects[0].nScriptID == original.objects[0].nScriptID, "and its script ID" );
    }

    // A referenced object refuses to be deleted, and says what holds it.
    CMapInfo forDelete = original;
    int nReferenced = -1;
    for ( size_t i = 0; i < forDelete.objects.size() && nReferenced < 0; ++i )
    {
        std::vector<std::string> references;
        NMapOverlay::FindReferences( forDelete, forDelete.objects[i].link.nLinkID, &references );
        if ( !references.empty() )
            nReferenced = forDelete.objects[i].link.nLinkID;
    }
    if ( nReferenced >= 0 )
    {
        std::string szRefusal;
        const CMapInfo before = forDelete;
        Check( !NMapOverlay::DeleteObject( &forDelete, nReferenced, &szRefusal ), "a referenced object refuses to go" );
        Check( !szRefusal.empty(), "and names what refers to it" );
        std::string szWhere;
        Check( NMapFile::AreEquivalent( before, forDelete, &szWhere ), "and a refused delete changes nothing" );
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails with "'NMapOverlay' has not been declared".

- [ ] **Step 3: Implement the overlay**

`MapOverlay.cpp` carries the rules from the spec's "Saving" section. The parts that are easy to get wrong:

```cpp
// A new object's link ID is one above every ID in use, so it can never
// collide with a reference held elsewhere in the map.
int NextLinkID( const SLoadMapInfo &rMap )
{
    CUsedLinkIDs used;   // std::set<int>, RandomMapGen/RMG_Types.h:26
    CMapInfo::GetUsedLinkIDs( rMap, &used );
    int nMax = 0;
    for ( CUsedLinkIDs::const_iterator it = used.begin(); it != used.end(); ++it )
        nMax = Max( nMax, *it );
    return nMax + 1;
}

// Everything that can hold an object's link ID. The spec's list: bridges,
// startCommandsList.unitLinkIDs, reinforcements, the groups inside them, and
// link.nLinkWith of any object. A passenger pointing at a vehicle counts, so
// deleting the vehicle is refused while the passenger is there.
void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences )
```

Write each of those five loops out; do not collapse them, because the string each one contributes is what the status bar shows the user in plan 5.

`AddObject` appends to `objects` or `scenarioObjects`, sets `link.nLinkID` from `NextLinkID`, `link.nLinkWith` to -1 and `link.bIntention` to false, `fHP` to 1.0f and `nScriptID` to -1, and leaves `nFrameIndex` at 0 — packing it is the bridge's job in plan 3, where the object database exists.

- [ ] **Step 4: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 5: Add the allowed delete and the diplomacy change**

The spec's tier list names both; the refused delete in Step 1 only covers half of it.

```cpp
// A delete nothing refers to takes the record out and leaves every other
// record alone - including the link IDs, which are never renumbered.
static void TestDeleteWithNoReferences()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    int nFree = -1;
    size_t nIndex = 0;
    for ( size_t i = 0; i < map.objects.size() && nFree < 0; ++i )
    {
        std::vector<std::string> references;
        NMapOverlay::FindReferences( map, map.objects[i].link.nLinkID, &references );
        if ( references.empty() )
        {
            nFree = map.objects[i].link.nLinkID;
            nIndex = i;
        }
    }
    if ( !Check( nFree >= 0, "the map has an object nothing refers to" ) )
        return;
    const size_t nBefore = map.objects.size();
    const int nNeighbourLinkID = map.objects[nIndex + 1 < nBefore ? nIndex + 1 : 0].link.nLinkID;
    std::string szRefusal;
    Check( NMapOverlay::DeleteObject( &map, nFree, &szRefusal ), "an unreferenced object deletes" );
    Check( map.objects.size() == nBefore - 1, "and the list is one shorter" );
    for ( size_t i = 0; i < map.objects.size(); ++i )
        if ( map.objects[i].link.nLinkID == nFree )
            Check( false, "the deleted link ID is gone" );
    bool bNeighbourKept = false;
    for ( size_t i = 0; i < map.objects.size(); ++i )
        bNeighbourKept = bNeighbourKept || map.objects[i].link.nLinkID == nNeighbourLinkID;
    Check( bNeighbourKept, "and nothing else was renumbered" );
}

// Diplomacy is a byte per player, and changing one changes exactly one.
static void TestDiplomacyChange()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    if ( !Check( map.diplomacies.size() >= 2, "the map has at least two players" ) )
        return;
    const std::vector<BYTE> before = map.diplomacies;
    const BYTE nNew = BYTE( before[1] == 0 ? 1 : 0 );
    Check( NMapOverlay::SetDiplomacy( &map, 1, nNew ), "diplomacy changes" );
    Check( map.diplomacies[1] == nNew, "for the player asked for" );
    Check( map.diplomacies[0] == before[0], "and for no one else" );
    Check( !NMapOverlay::SetDiplomacy( &map, int( before.size() ) + 5, 0 ),
           "a player that does not exist is refused" );
}
```

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 6: Add the unknown-object check and run again**

```cpp
// An object whose type the database does not know is still an object: it goes
// out exactly as it came in. This is the check that guards the frame-index
// trap in the spec's "Frame indices and unknown types".
static void TestUnknownObjectSurvives()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    if ( map.objects.empty() )
        return;
    map.objects[0].szName = "No_Such_Object_In_Any_Database";
    map.objects[0].nFrameIndex = 12345;
    const CMapInfo expected = map;
    Check( NMapFile::Write( "zig-out\\local-test\\unknown-object.bzm", map, &szError ), szError.c_str() );
    CMapInfo reread;
    szError.clear();
    if ( !Check( NMapFile::Read( "zig-out\\local-test\\unknown-object.bzm", &reread, &szError ), szError.c_str() ) )
        return;
    std::string szWhere;
    Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
           szWhere.empty() ? "the unknown object survived" : ( "unknown object changed at " + szWhere ).c_str() );
}
```

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 7: Commit**

```bash
git add Sources/src/MapFile/MapOverlay.h Sources/src/MapFile/MapOverlay.cpp tools/zig/map_file_test.cpp build.zig
git commit -m "feat(editor): object and diplomacy edits laid over a snapshot, and a refused delete

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: The terrain function

**Files:**
- Modify: `Sources/src/MapFile/MapOverlay.h`, `Sources/src/MapFile/MapOverlay.cpp`
- Modify: `tools/zig/map_file_test.cpp`

**Interfaces:**
- Consumes: `CMapInfo::UpdateTerrainCrosses( STerrainInfo*, const CTRect<int>&, const STilesetDesc&, const SCrossetDesc& )` (`MapInfo_StaticMethods.cpp:420`), `LoadDataResource` for the tileset and crosset named by the map.
- Produces:
  ```cpp
  namespace NMapOverlay
  {
      struct SPaintCell { int nX, nY; BYTE tile, noise; };   // the two fields SMainTileInfo has

      // The affected region R of the spec's "Terrain edits": every patch
      // holding a painted cell or a cell next to one, in PATCH coordinates,
      // which is what UpdateTerrainCrosses takes.
      CTRect<int> AffectedPatches( const STerrainInfo &rTerrain, const std::vector<SPaintCell> &rCells );

      // Everything a paint command must remember to undo itself: the tiles and
      // the patch crosses of all of R, before the paint ran.
      struct SPaintUndo
      {
          CTRect<int> rPatches;
          std::vector<SMainTileInfo> tiles;    // SMainTileInfo is { BYTE tile; BYTE noise; }
          std::vector<STerrainPatchInfo> patches;
      };

      // Sets the cells, runs PreprocessMapSegment and regenerates the crosses
      // of R - the deterministic function the bridge, the editor and the tests
      // all use. Fills pUndo with the state of R beforehand.
      bool Paint( SLoadMapInfo *pMap, const std::vector<SPaintCell> &rCells, SPaintUndo *pUndo );
      void UndoPaint( SLoadMapInfo *pMap, const SPaintUndo &rUndo );
  }
  ```

- [ ] **Step 1: Write the failing test**

```cpp
// Four cases from the spec: inside one patch, across a patch border, a thin
// strip the preprocessing pass eats, and undo putting the region back exactly.
static void TestPaint()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    const CMapInfo original = map;

    std::vector<NMapOverlay::SPaintCell> cells;
    NMapOverlay::SPaintCell cell;
    cell.nX = 20; cell.nY = 20; cell.noise = original.terrain.tiles[20][20].noise;
    cell.tile = BYTE( original.terrain.tiles[20][20].tile + 1 );
    cells.push_back( cell );

    NMapOverlay::SPaintUndo undo;
    Check( NMapOverlay::Paint( &map, cells, &undo ), "one cell paints" );
    Check( map.terrain.tiles[20][20].tile == cell.tile, "the painted cell has the new tile" );

    // Outside R nothing moved. R is in patches; convert once and check a
    // corner well away from it.
    Check( CompareAltitudeArrays( map.terrain, original.terrain ), "altitudes are untouched" );
    Check( map.terrain.rivers == original.terrain.rivers, "rivers are untouched" );
    Check( map.terrain.roads3 == original.terrain.roads3, "roads are untouched" );

    // Undo restores R exactly.
    NMapOverlay::UndoPaint( &map, undo );
    std::string szWhere;
    Check( NMapFile::AreEquivalent( original, map, &szWhere ),
           szWhere.empty() ? "undo put the region back" : ( "undo left " + szWhere ).c_str() );
}

// A cell on a patch border pulls in the neighbouring patch, whose crosses read
// across the border.
static void TestPaintOnAPatchBorder()
{
    CMapInfo map;
    std::string szError;
    if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
        return;
    std::vector<NMapOverlay::SPaintCell> cells;
    NMapOverlay::SPaintCell cell;
    // STerrainPatchInfo::nSizeX cells to a patch, so this is the last column
    // of patch 0 and its right neighbour has to be in R.
    cell.nX = STerrainPatchInfo::nSizeX - 1; cell.nY = 4; cell.noise = map.terrain.tiles[4][cell.nX].noise;
    cell.tile = BYTE( map.terrain.tiles[4][cell.nX].tile + 1 );
    cells.push_back( cell );
    const CTRect<int> r = NMapOverlay::AffectedPatches( map.terrain, cells );
    Check( r.minx == 0 && r.maxx >= 2, "a border cell pulls in the next patch" );
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: the build fails with "no member named 'Paint'".

- [ ] **Step 3: Implement the function**

```cpp
// The spec's "Terrain edits", step by step. R is in patch coordinates, which
// is what CMapInfo::UpdateTerrainCrosses iterates (MapInfo_StaticMethods.cpp:
// 450-463) - passing tile coordinates gives a rectangle a few thousand patches
// wide and a very long wait.
CTRect<int> AffectedPatches( const STerrainInfo &rTerrain, const std::vector<SPaintCell> &rCells )
{
    const int nPatchesX = rTerrain.patches.GetSizeX(), nPatchesY = rTerrain.patches.GetSizeY();
    CTRect<int> r( nPatchesX, nPatchesY, 0, 0 );
    for ( size_t i = 0; i < rCells.size(); ++i )
    {
        // The 8-neighbourhood in cells, then the patches those cells fall in.
        const int nMinPatchX = Max( 0, ( rCells[i].nX - 1 ) / STerrainPatchInfo::nSizeX );
        const int nMaxPatchX = Min( nPatchesX - 1, ( rCells[i].nX + 1 ) / STerrainPatchInfo::nSizeX );
        const int nMinPatchY = Max( 0, ( rCells[i].nY - 1 ) / STerrainPatchInfo::nSizeY );
        const int nMaxPatchY = Min( nPatchesY - 1, ( rCells[i].nY + 1 ) / STerrainPatchInfo::nSizeY );
        r.minx = Min( r.minx, nMinPatchX );  r.maxx = Max( r.maxx, nMaxPatchX + 1 );
        r.miny = Min( r.miny, nMinPatchY );  r.maxy = Max( r.maxy, nMaxPatchY + 1 );
    }
    return r;
}
```

`Paint` then: compute R, record `pUndo` (the tiles of R's cell rectangle and the patches of R, copied before anything changes), set the cells, load the tileset and crosset with `LoadDataResource( rMap->terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc )` as `CMapInfo::UpdateTerrainCrosses` does (`MapInfo_Methods.cpp:220-230`), and call `CMapInfo::UpdateTerrainCrosses( &rMap->terrain, R, tilesetDesc, crossetDesc )`.

`UndoPaint` copies `rUndo`'s tiles and patches straight back. It does not re-run the function: the spec is explicit that undo restores what was recorded, because the preprocessing pass makes the order of commands matter.

- [ ] **Step 4: Run the test to verify it passes**

Run: `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `map-file: PASS`

- [ ] **Step 5: Add the thin-strip case**

Find a cell whose repaint leaves a one-cell strip of a lower-priority terrain and check that `PreprocessMapSegment` removed it — that is, that a tile inside R but outside the painted set changed. Assert on that explicitly:

```cpp
// The preprocessing pass removes one-cell-thin strips, so it can change tiles
// in R that were never painted. That is the engine's behaviour and the saved
// map has to match it, so the test asserts it happens rather than tolerating
// it.
Check( nChangedOutsideC > 0, "the preprocessing pass changed a tile outside the painted set" );
```

If no shipped map gives that shape at a convenient cell, build the strip first with a preparatory paint and say so in a comment.

- [ ] **Step 6: Commit**

```bash
git add Sources/src/MapFile/MapOverlay.h Sources/src/MapFile/MapOverlay.cpp tools/zig/map_file_test.cpp
git commit -m "feat(editor): the terrain paint function, its affected region and its undo

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Done when

- `zig build test-map-files -Dtest-mode=run` passes on all six CI targets and is a required step in `cross-platform.yml`.
- `zig build test-map-files-all` passes locally over every shipped `.bzm` and `.xml` map.
- Adding a field to `SLoadMapInfo` fails to compile until `MapEquivalence.cpp` learns it.
- `NMapOverlay` can add, move and delete an object, refuse a delete that would orphan a reference, change diplomacy, and paint terrain with an exact undo — all without a renderer, a window or the object database.

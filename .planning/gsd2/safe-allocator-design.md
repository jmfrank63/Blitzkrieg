# Safe allocator design (M003 S01, 2026-10-10)

Answers the five design questions of `SPEC-safe-allocator.md` with what S01 built and measured. Everything below
was measured on win-home (Windows x64, MSVC, Zig 0.17.0) unless a row says otherwise. Linux x64, macOS arm64 and
macOS x64 were not run in S01; their cells are marked "not run" and nobody should read a result into them.

The code lives in `Sources/src/BkMemory/` (`bk_memory.zig`, `bk_memory.h`, `BkMemory.x64.def`, `new_delete.cpp`,
`bk_memory_allocator.zig`), the test in `tools/zig/bk_memory_cross_module_test.cpp` with
`bk_memory_consumer_a.cpp`, `bk_memory_consumer_b.cpp` and `bk_memory_consumer_common.h`, and the benchmark in
`tools/zig/bk_memory_bench.zig`. Build steps: `bk-memory`, `test-bk-memory-zig`, `test-bk-memory`,
`bk-memory-bench`; the backing choice is the build option `-Dbk-mem-allocator=safe|smp|crt|safe_page|safe_c`.

## 1. Where the instance lives (D074)

One new pure-Zig shared library, `BkMemory` (`BkMemory.dll` on Windows), owns the only `std.heap.SafeAllocator` of
the process. It is a comptime-initialised global (`var instance = SafeAllocator.init(backing, .{ .canary = ... })`),
so it exists before any static constructor of any module runs. `PlatformRuntime` was not used: the allocator must
be reachable from modules that do not link the platform layer, and it must not depend on anything that allocates.

The canary is `0x426b4d31`, different from the default of the per-executable allocator Zig's `start.zig` creates,
so a block that crosses between the two panics instead of corrupting.

Every block has a 16-byte header in front of the user pointer (`size: u64`, `align_log2: u8`, 3 reserved bytes,
`magic: u32`). The header makes `free` and `realloc` independent of the caller knowing size or alignment (the
question 4 of the spec: C `free` and unsized C++ `delete` carry neither), and a bad magic turns a pointer BkMemory
did not hand out into a panic ("was not allocated by BkMemory"), a freed magic into a "double free" panic.
Alignment is at least 16; for alignment above 16 the pad in front of the block is the alignment itself, so the
user pointer stays aligned and the header sits directly before it.

The C ABI (`bk_memory.h`, 12 exports, ordinals fixed in `BkMemory.x64.def`):

| Entry | Contract |
| --- | --- |
| `bk_mem_alloc(size)` | at least 16-aligned; size 0 returns a unique pointer; NULL is out of memory |
| `bk_mem_alloc_aligned(size, align)` | power-of-two alignment, otherwise NULL |
| `bk_mem_calloc(n, size)` | zeroed; `n * size` overflow returns NULL |
| `bk_mem_realloc(p, size)` | `p == NULL` allocates; `size == 0` frees and returns NULL (differs from glibc and MSVC, on purpose) |
| `bk_mem_realloc_aligned(p, size, align)` | never lowers the block's alignment; a stricter request moves the block |
| `bk_mem_free(p)` | NULL ignored |
| `bk_mem_free_sized(p, size, align)` | what sized/aligned `operator delete` calls; size and alignment are cross-checked in Debug only, 0 = not given |
| `bk_mem_size(p)` | the requested size of a live block (the `_msize` replacement) |
| `bk_mem_live_count()`, `bk_mem_live_bytes()` | cheap counters, tests take deltas |
| `bk_mem_report()` | runs the final leak check now, returns the leak count, closes the allocator |
| `bk_mem_closed_free_count()` | frees that arrived after `bk_mem_report` |

Zig code that hands memory to C++ uses `Sources/src/BkMemory/bk_memory_allocator.zig`, a `std.mem.Allocator` over
`bk_mem_alloc_aligned`/`bk_mem_realloc_aligned`/`bk_mem_free`. S03 imports that for the editors and the Zig engine
modules instead of `smp_allocator`/`c_allocator`/`page_allocator`, so memory that crosses to C++ is released by the
same instance. Its `resize` only succeeds for an unchanged length; growth goes through `remap`.

## 2. How operator new/delete reach it

`new_delete.cpp` defines all 20 replaceable forms: 8 `new` (scalar and array, plain, nothrow, aligned, aligned
nothrow) and 12 `delete` (scalar and array, plain, nothrow, sized, aligned, sized+aligned, aligned+nothrow). It
includes only `bk_memory.h`. The throwing `new` forms map size 0 to 1, loop on `std::get_new_handler()` and throw
`std::bad_alloc` when there is none; the nothrow forms return NULL (and swallow an exception from the handler).
Sized and aligned deletes call `bk_mem_free_sized`.

Why per-module copies are safe: `new_delete.cpp` is compiled into every module (exe and each DLL), not shipped as a
library of its own. On Windows each module binds its own `operator new` unless it defines one, and the loader does
not interpose. Because every copy forwards to the same `BkMemory.dll`, it does not matter which module's copy a
call binds to, and a block allocated in one module may be freed by any other. Measured: in the cross-module test
the consumer DLLs import `BkMemory.dll`, `MSVCP140D.dll` (only `std::get_new_handler`), `KERNEL32` and `ntdll`, and
no CRT `operator new`/`delete`/`malloc` (llvm-readobj).

What S02 must check per module:

- `-std=c++17` and `-fsized-deallocation -faligned-allocation` (the `linkEngineCxxRuntime` helper in `build.zig` sets them
  for `new_delete.cpp` itself). A module compiled without them still links, but calls the unsized/unaligned
  forms; those work, they only skip the Debug cross-check.
- Link order, measured in T03: `new_delete.cpp` must come before the CRT libraries, or lld reports duplicate
  operator symbols from `msvcrtd.lib`; `BkMemory.lib` must come after the CRT libraries, because `BkMemory.dll`
  exports Zig's `_DllMainCRTStartup` and a consumer DLL whose entry resolves to it never runs its static
  constructors. `linkEngineCxxRuntime(b, module, optimize)` (M003 S02 T01, replacing `linkBkMemory`) does the three
  steps in that order: include path and `new_delete.cpp`, then `linkMsvcRuntime`, then `BkMemory.lib`. It is the
  only way a C++ module gets BkMemory and is deliberately not part of `linkMsvcRuntime` (pure-Zig CloudSync uses
  that). Call it before the module's own C++ sources so `new_delete.cpp` is the first object on the link line.
- `BkMemory` is libc-free on Windows (libc only for the `crt` and `safe_c` backends and on non-Windows), because
  linking libc propagated `-lc` into importing modules and produced duplicate ucrt symbols in the exe.
- CRT arrangement, settled in S02 T01: the S01 hack (statically listing `libvcruntimed`/`libucrtd` in the test DLLs)
  is gone. The test DLLs now link the same dynamic CRT as the engine DLLs through `linkEngineCxxRuntime`; the
  consumer DLL imports `MSVCP140D.dll` and BkMemory like an engine DLL, carries no static UCRT, and its static
  constructor and destructor still run (the cross-module test asserts both). `BkMemory.lib` is generated from
  `BkMemory.x64.def`, so it carries only the `bk_mem_*` imports; with it after the CRT libraries the DLL entry
  point is the CRT's `_DllMainCRTStartup`.

### Roll-out and import audit (M003 S02 T02)

Every C++ module that is a DLL or an exe in the Game, Map Editor, Resource Editor and engine-hosted test graphs
gets `new_delete.cpp`, the dynamic CRT and `BkMemory.lib` through one of three helpers in `build.zig`, all calling
the same `linkEngineCxxRuntimeFlags`:

- `linkEngineCxxRuntimeEngine`: the module's sources use the engine's cppflags. `new_delete.cpp` is compiled with the
  same flags, because the debug STL records `_ITERATOR_DEBUG_LEVEL` in each object and lld-link refuses a level 2
  object next to a level 0 one (measured: `/failifmismatch` on every engine DLL when `new_delete.cpp` was built with
  plain `-std=c++17`).
- `linkEngineCxxRuntimeFlags(b, module, optimize, flags)`: the platform tests whose sources use `cppflags_debug` or
  `cppflags_release` directly.
- `linkEngineCxxRuntime`: plain `-std=c++17` modules (the BkMemory test modules, PlatformRuntime and its consumers).

The call goes immediately after `createModule`, before any of the module's own C++ sources: with the sources
first, lld resolves their `operator new` references to the lazily loaded `msvcrtd.lib` members and then reports the
operators again from `new_delete.obj` (measured on PlatformRuntime and every engine DLL).

Modules with the helper: PlatformRuntime, StreamIOOptionsAbi (options bridge), StreamIO (legacy bridge), Scene,
AILogic, GameTT, Game, Image, Net, Input, Anim, UI, SFX, GFX, GFXGPU, BuildVersion, BetaKeyGen, FontGen,
map-file-test, editor-bridge-test, resource-bridge tiers, `linkEditorEngine` (MapEditor, ResourceEditor and the Zig
editor tiers, which gained `addMsvcIncludePaths` because `new_delete.cpp` is the only C++ in them),
random-missions-test, rmg-determinism-test, composer-roundtrip-test, preview-scene-spike, resource-mod-roundtrip,
input-module-test, sfx-module-test, and the platform-foundation and platform-client tests (runtime lifecycle, client,
clock, sync, debug, dynamic library, module, storage gate, consumers A and B), plus gfxgpu-abi-test, options-bridge-test,
net-module-test, gfxgpu-factory-test and console-bridge-test.

Left out on purpose: the static libraries (zlib, libpng, LuaLib, Misc, Formats, RandomMapGen, MapFile, Common,
Main, the editor bridge archive), which resolve the operators through the DLL or exe that links them; the pure Zig
modules (CloudSync, BkMemory itself, GfxGpuZig, StreamIO's Zig core); `platform-abi-layout-test` (header layout
only, never allocates) and the ImGui static module; the Input/Resource model test modules built without engine DLLs.

`BkMemory.dll` (`libBkMemory.so`/`.dylib`) is a load-time import, so it is staged beside the Game and both editors
(`stage_runtime_files`, the debug-file list and `verify-x64-runtime`'s required list).

Executables that enter at `main` instead of `mainCRTStartup` (gfxgpu-factory-test, the abi and bridge tests) have no
CRT startup. Returning from such a `main` ends the process's last thread before BkMemory's detach report, and the
report reads thread-local storage (the smp allocator's thread index, `std.debug.lockStderr`'s Io state), so a
report that has anything to print faults with exit code 5 (0xC0000005) instead of reporting. gfxgpu-factory-test now
leaves through `std::exit`, which runs the report on the live thread. Any other `main`-entry harness that gets
leak reports on must do the same.

`zig build bk-memory-import-audit` (`tools/zig/bk_memory_import_audit.zig`, unit-tested by
`test-bk-memory-import-audit` on in-memory PE images) parses the import tables of the staged shipped modules
(`stage_runtime_files`, the two editors if installed) and of each built engine-hosted tool (passed as an artifact, so a
stale test exe left in the layout cannot decide the result). It fails, naming the module and symbol, when a module
imports `??2@`, `??3@`, `??_U@` or `??_V@` from `msvcp*`/`vcruntime*`/`ucrtbase*`/`api-ms-win-crt*`, or imports
`msvcp*` without importing `BkMemory.dll`. A `vcruntime`-only import does not mark a module as C++: pure Zig
CloudSync.dll imports it for `memcpy`. Allowlist: `SDL3.dll`, `rclone.exe` (third party) and `BkMemory.dll` itself.

Result on win-home (Windows x64 MSVC, Debug): 24 modules audited, 0 failures; every engine DLL, Game.exe,
MapEditor.exe, ResourceEditor.exe and the six engine-hosted tools import `BkMemory.dll` and no CRT operator.
CloudSync.dll imports neither the C++ standard library nor BkMemory. Linux and macOS: not run (the step prints
"not run on <os>"; the layouts are ELF and Mach-O).

ELF and Mach-O: the spec asked to check interposition and `-Bsymbolic`. Not run in S01; on those platforms the
per-module copies and the `.fini_array` hook are compiled for Linux but unexercised.

## 3. Lifetime and report placement (D075)

- Existence: the comptime global exists before every static constructor. The cross-module test has a static
  object in consumer B whose constructor allocates with `new` and whose destructor frees an adopted block;
  `FreeLibrary(B)` runs it and the counters balance.
- The report runs at library detach, not in the exe's `main`. On Windows `BkMemory` exports a `DllMain` that Zig's
  `_DllMainCRTStartup` calls; on `process detach` it runs `detachHook`. BkMemory is loaded first and detached
  last, so every static destructor of the importing modules has run and the report sees real leaks, not memory
  owned by objects torn down later. On Linux a `.fini_array` entry (`bk_mem_fini_entry`) does the same; compiled,
  not run on win-home.
- Mutex check: before reporting, every SafeAllocator shard mutex is inspected. A held mutex means a thread was
  killed mid-allocation (`ExitProcess`), so the records are not trusted: it logs `allocator in use at exit` and
  exits with code 4.
- Leaks: it logs `N leaked block(s) at exit` after SafeAllocator's own report and terminates the process with
  exit code 3 (`TerminateProcess` on Windows, `_exit` elsewhere), so a leak is a test failure even if `main`
  returned 0.
- `bk_mem_report()` can be called earlier by a test; it closes the allocator, later frees are counted no-ops
  (`bk_mem_closed_free_count`), a later alloc is a panic, and the detach hook then does nothing.
- Release policy: the report runs when the backend is a SafeAllocator one and the build is Debug. Release builds
  with the `smp` or `crt` backend keep no leak records and do not report. `BK_MEM_REPORT=0|1` overrides the build
  default for diagnosis (`1` has no effect on a backend without records).
- `BK_MEM_REPORT=log` (D079) runs the same report with stacks and then prints `bk_mem: N leaked block(s)` on
  stderr (also for N = 0) but keeps the exit code, so a tier stays green while its leaks are being fixed. A held
  shard mutex still exits 4. The cross-module test has a fourth run for it: planted leak, exit 0, summary line.
- Output goes through `std.log` scope `BkMemory` and SafeAllocator's own log to stderr as plain text (no colour
  escapes), so a test can match on it.

Measured with the planted leak (`test-bk-memory`, mode `plant-leak`):

- Exit code seen: 3, and it survived process detach (the build step uses `expectExitCode(3)`).
- stderr: SafeAllocator printed `leaked [addr, len: 20 align: 16] allocated at` followed by a 7-frame stack
  (the default in Debug), then `BkMemory` printed `1 leaked block(s) at exit`.
- Stack symbolisation: frames in the exe symbolise to function names with file and line through the PDB; frames in
  the consumer DLLs show addresses only (`???:?:?`). So on Windows a report names the call site only for the exe;
  for a DLL the address must be resolved against the module's PDB by hand. Not run on Linux or macOS.
- `BK_MEM_REPORT=0` gave exit 0 for the same planted leak.
- The default run (every form crossing exe, A and B) and the `report` run exit 0. Everything above passed in
  Debug and with `-Doptimize=ReleaseSafe`, and with the `safe_page` backend.

Not covered: `NRefCount::LeakObjectsOnExit` (`GameMain.cpp`, `AILogicInternal.cpp`, `ArmAllModulesLeakOnExit`)
stays armed until S04 replaces it with a safe shutdown order. Until then the Game deliberately skips deleting
ref-counted objects at exit, and the detach report will see them as leaks the moment the engine is routed through
BkMemory; S02 must either keep the report off for the Game (`BK_MEM_REPORT=0`) or accept the known list.

## 4. C allocation per source

`malloc` is never replaced globally: no symbol interposition of `malloc`/`free` in the CRT or libc, and no
`_CrtSetAllocHook`. Reasons: third-party and system libraries (graphics driver user-mode parts, the C runtime
itself, SDL's platform code) allocate with it and free with it internally; replacing it globally would route their
blocks through headers they never wrote and make a foreign pointer panic. Each source is routed individually, at
the call or through its hook; what has no hook stays on the CRT and is listed.

| Source | Today | Decision |
| --- | --- | --- |
| `Image/ImagePNG.cpp` | `malloc`/`free` of `row_pointers` rows (lines ~125-127, 231-233, free at the error and end paths); `png_create_read_struct(..., 0, 0, 0)` and `png_create_write_struct(..., 0, 0, 0)` use libpng's own allocator | **Done (M003 S02 T03).** Own `malloc`/`free` are `bk_mem_alloc`/`bk_mem_free`. libpng is routed at its defaults instead of through `png_create_*_struct_2` wrappers: `png_malloc_default`, `png_free_default` and the `png_create_struct`/`png_destroy_struct` fallbacks in `pngmem.c` call `bk_mem_*`, so every `png_create_*_struct(..., 0, 0, 0)` caller (ImagePNG and anything else) is covered without touching the call sites. |
| `Main/CloudSyncFacade.cpp` | `std::malloc`/`std::calloc`/`std::free` of strings the caller frees (lines ~224, 283, 298) | **Done.** All sites use `bk_mem_alloc`/`bk_mem_calloc`/`bk_mem_free`; the string is allocated and freed inside the file, so the crossing rule holds. The facade test exe (`cloudsync-facade-test`) now links BkMemory through `linkEngineCxxRuntimeFlags` and is in the import audit. |
| `SFX/AudioBackendOpen.cpp` (miniaudio) | `AudioAllocMalloc`/`AudioAllocRealloc`/`AudioAllocFree` already installed in `contextConfig.allocationCallbacks` and `engineConfig.allocationCallbacks` (lines ~26-41, 1400-1437), calling `std::malloc`/`realloc`/`free` | **Done.** The three bodies call `bk_mem_alloc`/`bk_mem_realloc`/`bk_mem_free`; the `sz == 0 ? 1 : sz` guard is kept; no call site changed. |
| `SFX/AudioBackendXiphVorbis.c` | own `realloc` of PCM data, `calloc` of the stream, `free` (lines ~96, 159, 175, 189, 216) | **Done.** Includes `bk_memory.h` and uses `bk_mem_realloc`/`bk_mem_calloc`/`bk_mem_free`. The PCM buffer is only ever freed in this file. |
| libvorbis / libogg (`Sources/sdk/xiph`) | `_ogg_malloc`/`_ogg_calloc`/`_ogg_realloc`/`_ogg_free` are macros for `malloc`/`calloc`/`realloc`/`free` in `ogg/os_types.h` | **Not routed in S02 T03; stays on the CRT.** Vendored SDK code, excluded from the T03 search by its plan; every block it allocates is freed by the same library (`ov_clear`, `ogg_*_clear`), so nothing crosses a family boundary. Redefining the four macros in the vendored `os_types.h` is the follow-up if the leak count wants them. |
| zlib | `StreamIO/ZipFile.cpp:180-181`, `AutoRun/ZipFile.cpp:183-184`, `Main/GameCreation.cpp:376-377` and `1605-1606` set `zalloc`/`zfree` to 0 | **Done, simpler than planned.** The default allocators in `zlib/zutil.c` (`zcalloc`/`zcfree`, the non-MSDOS path) call `bk_mem_calloc`/`bk_mem_free`. Every `zalloc = 0` site (ZipFile, GameCreation, `compress.c`, `uncompr.c`, `gzio.c`) is covered and the call sites are unchanged, so there are no wrapper functions and no use of `opaque`. |
| libpng's internal zlib | goes through libpng's `png_zalloc`/`png_zfree`, which use `png_malloc`/`png_free` and therefore the `_2` hooks above | **Done.** Covered by the `pngmem.c` defaults above: `png_malloc_default`/`png_free_default` are what `png_zalloc`/`png_zfree` reach when no user functions are set. |
| Lua (`LuaLib/LuaSrc/lmem.c`) | Lua 4 style `lua_open(0)` in `LuaLib/Script.cpp:33`; `luaM_realloc` calls `realloc`/`free` in `lmem.c` (lines ~129-130), and the file has a `debug_realloc` under a debug switch | **Done.** The non-debug `luaM_realloc` calls `bk_mem_realloc`/`bk_mem_free` (through `realloc`/`free` macros defined only when `LUA_DEBUG` is off). The `_DEBUG` allocator keeps its guard bytes but its two underlying calls, `(malloc)(realsize)` and `(free)(block)`, are `bk_mem_alloc`/`bk_mem_free`, so a Debug build is on BkMemory as well. The comment at line 23 of `lmem.c` records that modules link private copies of the file while `luaM_realloc` is exported: after the change every copy forwards to BkMemory. |
| SDL3 | `SDL_Init`/`SDL_InitSubSystem` in `Platform/SDLApplication.cpp` (lines ~101, 185) | **Done.** `Sources/src/BkMemory/bk_memory_sdl.h` holds `BkMemoryInstallSdlFunctions()` (one `SDL_SetMemoryFunctions(bk_mem_*)` call). Engine: `SDLApplication.cpp` installs it from a static initializer of PlatformRuntime (so before any caller of the module, including the `SDL_GetBasePath`/`SDL_GetPrefPath` calls in `Paths.cpp` and the message box and clipboard calls in `System.cpp`, which run without `SDLApplication`) and again at the top of `ShowSplash` and `Initialize`, ahead of `SDL_SetHint`, `SDL_SetAppMetadata` and `SDL_Init*`; repeating it with the same functions is harmless. Editors: `kit/host.zig` calls `SDL_SetMemoryFunctions` with extern `bk_mem_*` declarations before `SDL_Init`. No SDL call precedes these in `Sources/src` or `Sources/editor`: the editor `main.zig` calls are all after `Host.start`, and `options_bridge.cpp` and `bridge.cpp` call SDL only once a window or display exists. |
| stb (`Sources/sdk/stb`) | `STBI_MALLOC`/`STBI_REALLOC`/`STBI_FREE`-style macros default to `malloc` | `stb_vorbis.c` is the only stb source compiled (inside `AudioBackendOpen.cpp`). Its decoder allocations are freed by stb itself, so it **stays on the CRT**; miniaudio's own allocations, most of that file's traffic, go through the routed callbacks above. |
| Engine-owned `malloc`/`realloc`/`free`/`strdup`/`_msize` outside the files above | not inventoried in S01 beyond a search that found `ImagePNG.cpp`, `CloudSyncFacade.cpp`, `AudioBackendOpen.cpp` | **Searched again (M003 S02 T03)** over `Sources/src` and `Sources/editor` for `malloc`, `calloc`, `realloc`, `strdup`, `_strdup`, `_msize`, `free`, `_aligned_*`, `HeapAlloc`, `GlobalAlloc` and `LocalAlloc` outside zlib, libpng, LuaSrc, `sdk`, xiph and BkMemory: no C or C++ hit remains. The only other site is Zig, listed below. `strdup` and `_msize` do not occur. |

What stays on the CRT, and why:

- The C runtime's own allocations (stdio buffers, `getenv`, locale, `std::locale`/iostream internals, thread-local
  data): they never leave the CRT and are not engine blocks.
- Graphics driver and Windows API allocations: not ours; freed by their owner (`LocalFree`, COM release).
- `MSVCP140D.dll` containers' internal allocation goes through `operator new`, which is routed; the only symbol the
  consumer DLLs import from it is `std::get_new_handler`.
- Memory returned by a third-party library that the caller must release with that library's free (for example SDL
  strings freed with `SDL_free` after the memory functions are installed). It follows SDL's installed functions,
  so it ends up in BkMemory anyway.
- `Sources/src/GFXGPU/abi.zig`: the renderer state is `libc.malloc`ed and `libc.free`d inside that one file;
  the C++ side only holds the opaque handle and releases it through the module's destroy entry. It never
  becomes a BkMemory block.
- Zig module allocators (`std.heap.c_allocator` in `StreamIOZig/streamio.zig` and the GFXGPU renderer,
  `smp_allocator` in CloudSync): allocations of the Zig libraries themselves, owned and freed by them. They are
  not part of the C++ engine rollout; moving them onto BkMemory is a later task if the leak count wants it.
- The `malloc`/`free` in `pngrutil.c`'s `_WIN32_WCE` `strtod` shim: dead code on every supported platform.
- `stb_vorbis` and libvorbis/libogg: see the table.

Rule for crossing: a block must be released by the function family that allocated it. After this slice that family
is `bk_mem_*` (and `operator new`/`delete`, which forward to it); mixing `bk_mem_free` with CRT `free` on one
block is a panic by bad header magic, which is what we want.

## 5. Release allocator choice (R049)

Measured by `zig build bk-memory-bench` (24 passes, one per process so peak memory is per pass, about 4 minutes):
small blocks are 16-512 B with a 4096-slot sliding live window (10^7 allocations), large blocks are 8-64 KiB with
64 slots (10^6 allocations), 1 and 8 threads. Numbers are ns per op (a free plus an alloc counts as 2 ops) with
the peak private bytes in MiB in parentheses. ReleaseFast except the last row. Every row includes BkMemory's 16-byte
header, the `crt` row is `std.heap.c_allocator` (CRT heap) behind that header.

| backend | small 1t | small 8t | large 1t | large 8t |
| --- | --- | --- | --- | --- |
| crt (c_allocator + header) | 57.3 (2.6) | 67.7 (144.6) | 251.1 (6.3) | 1472.6 (159.0) |
| smp | 32.2 (15.6) | 62.5 (159.0) | 5348.0 (24.9) | 2466.8 (189.2) |
| safe + smp | 82.1 (7.5) | 65.2 (145.0) | 5567.2 (30.0) | 2418.8 (189.0) |
| safe + page | 467.7 (7.4) | 170.4 (144.8) | 9244.6 (9.0) | 4538.1 (154.9) |
| safe + c | 81.0 (3.6) | 83.5 (149.8) | 296.5 (6.4) | 1504.5 (160.2) |
| safe + smp, Debug | 4400.0 (2.9) | 1232.2 (144.4) | 9602.7 (19.6) | 2817.9 (188.0) |

Reading it:

- The ~145 MiB floor at 8 threads is thread stacks (16 MiB each, committed), not allocator memory.
- `smp` is the fastest for small blocks (32 ns single-threaded) but has a large-block cliff: for 8-64 KiB it
  falls back to a mapping per block, about 20x slower than the CRT heap single-threaded (5.3 us against 0.25 us)
  and a higher peak (25 MiB against 6 MiB).
- SafeAllocator over `smp` adds about 50 ns single-threaded on small blocks in ReleaseFast (82 ns), the same
  order as the CRT heap's 57 ns; with 8 threads the difference vanishes (65 against 68).
- `page_allocator` is 6-14x slower: a syscall per block. It is useful only to prove the backing is swappable.
- `safe + c` costs 15-40 ns over `crt` and is the best of the SafeAllocator rows for large blocks.
- Debug SafeAllocator is 4.4 us per op single-threaded: it captures a 7-frame stack for every allocation. That is
  what makes the Debug leak reports useful and also what makes the heaviest missions slow in Debug; the
  `BK_MEM_REPORT`/backend options are the escape for runs that do not need a report.
- The header costs 16 bytes per block (more for alignment above 16, where the pad equals the alignment). The
  SafeAllocator footer (metadata, checksum, plus the trace words in Debug) was not measured separately; the table's
  peaks include it.

Decision:

- Debug and test builds: `safe` (SafeAllocator over `smp_allocator`), report on at detach. That is the only
  configuration the cross-module test fails a leak in.
- Release builds: the header layer without SafeAllocator, keeping the same entry points. Preferred backing is the
  CRT heap (`crt`): no large-block cliff (251 ns against 5348 ns), the smallest peak in every column, and only
  25 ns slower than `smp` on small single-threaded blocks. `smp` stays selectable with
  `-Dbk-mem-allocator=smp` and S02 decides between the two with the engine's own load. This is a preference from
  synthetic loops, not a measured engine result.
- Open for S02, deferred on purpose because the engine is not routed through BkMemory yet: random missions
  `repeat=20` heap and time, and Game frame time on a heavy map. The `crt` backend links libc, which on MSVC
  collided with the explicit dynamic CRT in the cross-module test (duplicate `_invalid_parameter_noinfo`,
  `_wctype`, `__pctype_func`); it works as a benchmark and the rollout must make the release BkMemory use the
  same dynamic CRT as the engine DLLs.

## Resource Editor tiers on BkMemory (M003 S02 T05)

All run in debug on win-home, one foreground command each, with `BK_MEM_REPORT=count` (the leak summary line is
printed and the exit code stays 0). Full per-tier rows, durations and log names are in
`.planning/gsd2/safe-allocator-tier-results.md`.

| Tier | Result | Leaked blocks at exit |
| --- | --- | --- |
| `resource-editor-host-check` | pass | 43750 |
| `resource-editor-smoke` | pass | 43703 |
| `resource-editor-batch` | pass | 43588 |
| `resource-editor-game-reads-it` | pass | 43709 |
| `resource-editor-auto-<editor>`, 19 editors (core, wpn, unt, spt, msh, obt, fnc, bld, bdg, pcp, eff, til, 3rd, 3rv, mip, chc, cgc, mdc, gui) | all pass | 43226 to 44151 (msh highest, 3rd lowest) |

- The count is the same order in every editor, so it is a fixed start-up residue (engine singletons kept for the
  process lifetime), not per-editor growth. S04 owns driving it to zero.
- No editor needed an engine fix: the allocator rollout caused no cross-allocator free, exit 4 or preview-scene
  failure in any Resource Editor tier.
- Every resource-editor run step prints the `bk_mem:` summary line, so T04's helper already covers them.
- `test-editor-bridge` (debug) passed with 151745 leaked blocks but took about 49 min against the 1022 s
  baseline; not investigated in T05, flagged for S04.
- The full sweep `tools/zig/run-resource-sweep.sh` (about 27 min) is left to the maintainer.
## Results matrix

| Check | Windows x64 MSVC (win-home) | Linux x64 | macOS arm64 | macOS x64 |
| --- | --- | --- | --- | --- |
| `test-bk-memory-zig` (8 unit tests) | pass | not run | not run | not run |
| `test-bk-memory` cross-DLL, clean and report runs | pass (Debug, ReleaseSafe, `safe_page`) | not run | not run | not run |
| planted leak: exit 3 and SafeAllocator report | pass | not run | not run | not run |
| `BK_MEM_REPORT=0` suppresses it | pass | not run | not run | not run |
| `-Dbk-mem-allocator=safe_c` / `crt` with the cross test | link fails (duplicate CRT symbols), benchmark only | not run | not run | not run |
| `bk-memory-bench` table above | measured | not run | not run | not run |
| symbol names in leak stacks | exe yes, DLL addresses only | not run | not run | not run |
| every C++ module on the allocator, PE import audit (`bk-memory-import-audit`) | pass: 24 modules, 0 CRT operator imports | not run | not run | not run |
| `test-platform-foundation`, `test-platform-client`, `test-bk-memory` after the roll-out | pass | not run | not run | not run |
| `test-input-module`, `test-sfx-module`, `test-net-module`, `test-console-bridge`, `gfxgpu-factory-test`, `verify-x64-runtime` | fail on leak reports only (exit 3), expected until BK_MEM_REPORT=log in T04 | not run | not run | not run |
| random missions `repeat=20` heap | deferred to S02 | not run | not run | not run |
| Game frame time on a heavy map | deferred to S02 | not run | not run | not run |
| `.fini_array` detach hook | compiled for Linux, not exercised | not run | not applicable | not applicable |

## Hand-off to S02 and S03

- S02: add `linkBkMemory` to every engine module, the Game and the editor bridges; apply the table in question 4;
  decide the real-DLL CRT arrangement (the test DLLs use a static UCRT); keep `NRefCount::LeakObjectsOnExit`
  armed and the report policy explicit until S04; measure random missions and frame time and pick `crt` or `smp`
  for release.
- S03: use `bk_memory_allocator.zig` in the editors and Zig engine modules; do not create a second SafeAllocator.
- Known, not fixed in S01: the test DLLs carry a static UCRT, DLL frames are not symbolised, the `.fini_array`
  hook is unrun, a backend without leak records (`smp`, `crt`) cannot fail the planted-leak run.

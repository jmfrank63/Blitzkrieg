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

- `-std=c++17` and `-fsized-deallocation -faligned-allocation` (the `linkBkMemory` helper in `build.zig` sets them
  for `new_delete.cpp` itself). A module compiled without them still links, but calls the unsized/unaligned
  forms; those work, they only skip the Debug cross-check.
- Link order, measured in T03: `new_delete.cpp` must come before the CRT libraries, or lld reports duplicate
  operator symbols from `msvcrtd.lib`; `BkMemory.lib` must come after the CRT libraries, because `BkMemory.dll`
  exports Zig's `_DllMainCRTStartup` and a consumer DLL whose entry resolves to it never runs its static
  constructors. `linkBkMemory(b, module)` adds the include path, the source and the library; it is deliberately not
  part of `linkMsvcRuntime` yet.
- `BkMemory` is libc-free on Windows (libc only for the `crt` and `safe_c` backends and on non-Windows), because
  linking libc propagated `-lc` into importing modules and produced duplicate ucrt symbols in the exe.
- The test DLLs that have static destructors needed `libvcruntimed`/`libucrtd` statically listed before the import
  CRT libraries, so they carry a static UCRT (they import no `ucrtbased`/`vcruntime` DLL). That is a hack of the
  test DLLs. The rollout of the real engine DLLs must settle the CRT arrangement; this is the open risk of S02.

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
| `Image/ImagePNG.cpp` | `malloc`/`free` of `row_pointers` rows (lines ~125-127, 231-233, free at the error and end paths); `png_create_read_struct(..., 0, 0, 0)` and `png_create_write_struct(..., 0, 0, 0)` use libpng's own allocator | Own `malloc`/`free` become `bk_mem_alloc`/`bk_mem_free` (or `new`/`delete`). libpng: use `png_create_read_struct_2` / `png_create_write_struct_2` with `bk_mem_alloc`/`bk_mem_free` wrappers (`png_malloc_ptr`, `png_free_ptr`), so libpng's own allocations are recorded. |
| `Main/CloudSyncFacade.cpp` | `std::malloc`/`std::calloc`/`std::free` of strings the caller frees (lines ~224, 283, 298) | Route through `bk_mem_*`; the string returned to the caller is freed by the same module family, which today it already is. |
| `SFX/AudioBackendOpen.cpp` (miniaudio) | `AudioAllocMalloc`/`AudioAllocRealloc`/`AudioAllocFree` already installed in `contextConfig.allocationCallbacks` and `engineConfig.allocationCallbacks` (lines ~26-41, 1400-1437), calling `std::malloc`/`realloc`/`free` | Change the three bodies to `bk_mem_alloc`/`bk_mem_realloc`/`bk_mem_free`; no call site changes. Keep the `sz == 0 ? 1 : sz` guard (BkMemory returns a pointer for 0 as well). |
| `SFX/AudioBackendXiphVorbis.c` | own `realloc` of PCM data, `calloc` of the stream, `free` (lines ~96, 159, 175, 189, 216) | Plain C: include `bk_memory.h` and use `bk_mem_realloc`/`bk_mem_calloc`/`bk_mem_free`. |
| libvorbis / libogg (`Sources/sdk/xiph`) | `_ogg_malloc`/`_ogg_calloc`/`_ogg_realloc`/`_ogg_free` are macros for `malloc`/`calloc`/`realloc`/`free` in `ogg/os_types.h` | Redefine the four macros to the `bk_mem_*` entries in `os_types.h` for the vendored copy (it is vendored, so this is a local patch recorded next to the file); vorbis's `misc.h` only redefines them under its `_VDBG` debug switch. |
| zlib | `StreamIO/ZipFile.cpp:180-181`, `AutoRun/ZipFile.cpp:183-184`, `Main/GameCreation.cpp:376-377` and `1605-1606` set `zalloc`/`zfree` to 0 | Set `zalloc`/`zfree` to small `bk_mem_*` wrappers (`opaque` unused). Four call sites plus `AutoRun`. |
| libpng's internal zlib | goes through libpng's `png_zalloc`/`png_zfree`, which use `png_malloc`/`png_free` and therefore the `_2` hooks above | Covered by the `png_create_*_struct_2` change. |
| Lua (`LuaLib/LuaSrc/lmem.c`) | Lua 4 style `lua_open(0)` in `LuaLib/Script.cpp:33`; `luaM_realloc` calls `realloc`/`free` in `lmem.c` (lines ~129-130), and the file has a `debug_realloc` under a debug switch | There is no allocator argument in this Lua; change `lmem.c` to call `bk_mem_realloc`/`bk_mem_free` for the non-debug path. The comment at line 23 of `lmem.c` records that modules link private copies of the file while `luaM_realloc` is exported: after the change every copy forwards to BkMemory, which is what is wanted. |
| SDL3 | `SDL_Init`/`SDL_InitSubSystem` in `Platform/SDLApplication.cpp` (lines ~101, 185) | `SDL_SetMemoryFunctions(bk_mem_alloc, bk_mem_calloc, bk_mem_realloc, bk_mem_free)` before the first `SDL_Init*`. It must run before SDL is first used by anything in the process; memory SDL allocated before it would be freed through the new functions, so the call has to be the first SDL call. The Zig side has `SDL_SetMemoryFunctions` in `vendor/zig-sdl3` (`sdl3.zig`). |
| stb (`Sources/sdk/stb`) | `STBI_MALLOC`/`STBI_REALLOC`/`STBI_FREE`-style macros default to `malloc` | Define the macros to `bk_mem_*` in the one translation unit that includes the implementation. Not inspected line by line in S01. |
| Engine-owned `malloc`/`realloc`/`free`/`strdup`/`_msize` outside the files above | not inventoried in S01 beyond a search that found `ImagePNG.cpp`, `CloudSyncFacade.cpp`, `AudioBackendOpen.cpp` | S02 repeats the search over the full tree; `_msize` becomes `bk_mem_size`, `strdup` a `bk_mem_alloc` + copy helper. Anything found goes into this table. |

What stays on the CRT, and why:

- The C runtime's own allocations (stdio buffers, `getenv`, locale, `std::locale`/iostream internals, thread-local
  data): they never leave the CRT and are not engine blocks.
- Graphics driver and Windows API allocations: not ours; freed by their owner (`LocalFree`, COM release).
- `MSVCP140D.dll` containers' internal allocation goes through `operator new`, which is routed; the only symbol the
  consumer DLLs import from it is `std::get_new_handler`.
- Memory returned by a third-party library that the caller must release with that library's free (for example SDL
  strings freed with `SDL_free` after the memory functions are installed). It follows SDL's installed functions,
  so it ends up in BkMemory anyway.

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

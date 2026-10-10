# Milestone: the engine allocates through Zig's SafeAllocator, and no leaks remain (2026-10-10)

Branch `fix/memory-leaks` (rebased onto `main` `1bd2ed2ab`; it has no commits of its own yet). Zig 0.17.0 (scoop,
default `zig` on win-home). Read first: `.planning/gsd2/HANDOVER-memory-leaks.md` (what was fixed, how memory is
measured, what is still open, the rules for this work) and the "SafeAllocator Introduced" section of
https://ziglang.org/download/0.17.0/release-notes.html. The allocator is newer than the models' training data: read
`~/scoop/apps/zig/0.17.0/lib/std/heap/SafeAllocator.zig` rather than recalling an API.

## Goal

Every allocation the C++ engine makes, in the Game, the Map Editor, the Resource Editor and the engine-hosted tools
and tests, goes through one Zig-owned `std.heap.SafeAllocator` per process, and so do the Zig editors and the Zig
engine modules. In debug and test builds its leak report at shutdown is a test failure. Every leak it reports is
fixed, on every platform, until the reports are empty. CI is green on every job.

## What SafeAllocator gives and costs (from its source)

- `deinit` reports every leak and frees all backing memory; mismatched frees, double frees and allocations from
  another instance (`Options.canary`) panic or fault; each allocation carries a checksummed footer with metadata
  and, in debug, `stack_trace_frames` (default 7) frames of the allocating stack.
- It reuses memory only as far as its backing allocator does: with a backing allocator that never reuses, freed
  memory is never reused. Measure what the chosen backing allocator does in a long run (the 624-case random
  missions sweep, the 16-minute `test-editor-bridge` debug tier) before routing the Game through it.
- At most 128 threads per process share it (`threads: [128]Thread`).
- Zig's `free` needs the length and alignment; C++ unsized `delete` and C `free` do not give them.

## Design questions S01 must answer with measurements, not opinions

1. **One instance per process across modules.** The engine is many shared libraries (on Windows one DLL per module,
   the debug CRT shared). Memory allocated in one module is freed in another, so every module must reach the same
   allocator. Where does it live (a new small shared library, or `PlatformRuntime`), and how does each module's
   global `operator new`/`delete` (every form: sized, aligned, nothrow, array) reach it? On Windows each module links
   its own `operator new` unless it defines one; on ELF and Mach-O check interposition and `-Bsymbolic`.
2. **Lifetime.** The allocator must exist before the first static constructor of any module and outlive the last
   static destructor. Where does the leak report run so it sees real leaks and not memory still owned by objects
   that are torn down later? The previous fix found static constructor order matters (`2adb90a10`, theCheats on
   macOS).
3. **C allocation.** The engine's own `malloc`/`realloc`/`free`/`_msize`/`strdup` calls, and third-party code
   with allocator hooks: SDL (`SDL_SetMemoryFunctions`, before SDL is first used), Lua (`lua_newstate`'s allocator),
   zlib (`zalloc`/`zfree`), miniaudio (already routed through `AudioAlloc*` in `AudioBackendOpen.cpp`), stb. Decide
   per source; anything left on the CRT is listed with the reason.
4. **Size and alignment on free.** A small header before each block (size and alignment) or another scheme; `realloc`
   and `_msize` semantics kept.
5. **Release builds.** Measure SafeAllocator in `--release=fast` (no stack traces, no write-after-free check) against
   the CRT: random missions `repeat=20` heap and time, Game frame time on a heavy map. If it is too slow or grows,
   release uses `std.heap.SmpAllocator` behind the same entry points; record the numbers and the choice.

## Slices

1. **S01 Spike and design.** The allocator entry points (`bk_mem_alloc`/`free`/`realloc`/aligned forms, exported
   from one shared library), the C++ `operator new`/`delete` replacement linked into every engine module, and one
   engine-hosted test (`test-platform-foundation` or a new small one) on Windows x64 MSVC that allocates in one DLL
   and frees in another, leaks on purpose and sees the report fail it. Answers to the five questions above in
   `.planning/gsd2/safe-allocator-design.md`.
2. **S02 The C++ engine through the allocator.** Every engine module, the Game, the editor bridges and the
   engine-hosted tools and tests. Third-party hooks as decided in S01. `install-game install-map-editor
   install-resource-editor` build in debug and `--release=fast`; the tiers in `AGENTS.md` pass on win-home; random
   missions `repeat=20` heap flat (the existing gate) with the new allocator; the bridge tier's time recorded against
   today's 1022 s.
3. **S03 The Zig side.** The editors (`Sources/editor`: today `smp_allocator`, and `c_bridge.zig` uses
   `c_allocator`/`page_allocator`) and the Zig engine modules (StreamIO, GFXGPU, CloudSync) use SafeAllocator in debug
   and test builds and fail on its report; `DebugAllocator` (deprecated in 0.17) is gone. Memory that crosses to C++
   uses the shared allocator.
4. **S04 Fix every leak the reports show.** Start with the known exit-time leaks: `NRefCount::LeakObjectsOnExit`
   (`GameMain.cpp`, `MainLoopCommands.cpp` `ArmAllModulesLeakOnExit`) replaced by a safe shutdown order, the
   `CLinkObject` registries (`LinkObject.cpp`), libdecor/GTK loaded for hidden test windows on Wayland. Then whatever
   the Game, the Map Editor, the Resource Editor and the tools report. A port bug is fixed, never adopted; a leak from
   the 2003 code is fixed too. One commit per cause, with the report before and after.
5. **S05 Gates and CI.** Every engine-hosted debug tier fails on a leak report; GFXGPU counts SDL GPU creates
   against releases (textures, buffers, transfer buffers, samplers, pipelines) and a test that opens and closes maps
   checks they return; the Map Editor and Resource Editor auto tiers get a memory budget like the random missions
   gate. CI green on every job (start it with `gh workflow run cross-platform.yml -R jmfrank63/Blitzkrieg --ref
   fix/memory-leaks`; the agent never pushes, it asks the maintainer to push). The macOS machines are measured by the
   maintainer's agent over ssh; record their results only when someone ran them.

## Rules

- `AGENTS.md` applies: CRLF, conventional commits with a scope, tests write only under `zig-out/local-test`, never
  into shipped `Data/` or the user's profile, saves, settings or cloud sync. GSD never pushes.
- The rules of `HANDOVER-memory-leaks.md`: a leak is a bug to fix, never worked around with smaller sweeps, bigger
  runners or longer limits; a leak seen on one platform is assumed on the others until measured; a result counts only
  for the machine it was measured on.
- Run tiers one at a time, each bounded below the agent's 10-minute tool limit (Start-Process plus Wait-Process with
  a timeout, logs under `zig-out/local-test/tier-logs`). The debug `test-editor-bridge` takes about 16 minutes: run it
  in the background with a 40-minute bound and wait for it; do not end the session while it runs.
- Before a task completes, `git status` shows nothing it touched as modified or untracked.

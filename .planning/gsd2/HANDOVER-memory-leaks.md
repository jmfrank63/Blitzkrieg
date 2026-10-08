# Handover: memory leaks (2026-10-08)

Written on win-home for a Linux x64 machine. Finding the leak below comes before anything else in the plan
(`HANDOVER.md`), Small tools included, by the maintainer's decision of 2026-10-08: "we should make sure we have no
leaks at all, nowhere on any platform and not on game or editors."

## Rules for this work

- A leak is a bug to fix. Never work around it with a smaller sweep, a bigger runner or a longer limit.
- A leak seen on one platform is assumed to exist on the others until a measurement on that platform shows otherwise.
- A result counts only for the machine it was measured on. An agent asks the maintainer to run a check on a machine
  it cannot reach, and never claims a result for a platform nobody ran.
- The rule covers the game, the Map Editor, the Resource Editor, ELK and the small tools.

## Status (2026-10-09): the per-map leak is fixed and gated

Everything below "The leak we have seen" is the original brief, kept for reference. Its steps 1 to 6 are done.
All branches are merged; only `main` remains (`1eaf95723`, CI run 37836080796 green on the same tree).

The causes found and fixed, all of them in the engine and present on every platform:

- `CAILogic::Clear` set `NRefCount::LeakObjectsOnExit()`, so every AI object of every map was kept on purpose
  (the bulk of the 32 MB per mission). Removing it exposed a reference cycle between a vehicle and its turrets
  (`CTurret` holds its owner): `CUnits::DetachTurrets` breaks it before clearing and in `~CUnits` (`56073a38c`).
- GFXGPU uploads outside a frame were never retired by SDL's Vulkan backend: at most 256 now, then a wait for idle
  (`1c8482d80`).
- The editor bridge never took the game messages the world posts for the Game's interface (`e97e809d2`).
- The war fog kept its pending new and deleted units across maps (`b745d3250`).
- `bk_storage_name` returned a new copy of the storage name per call (`bcfe6c657`).

How it is measured: `random-missions-test ... repeat=<n>` (or `-Drandom-missions-repeat=<n>`) generates and opens
the same maps every round (both random generators restart) and prints after every case the platform's memory
counter and the live heap to the byte: every busy block of every process heap on Windows, every in-use block of
every malloc zone on macOS (walked as `heap` does; `malloc_zone_statistics` miscounts blocks), glibc's arenas on
Linux. Under Valgrind it also prints a memcheck change report per case.

Results with `only=summer_ukraine\securearea00 repeat=20`, each on its own machine:

| Machine | Heap at the end of each round, once warm |
| --- | --- |
| Linux x64 (AMD GPU) | within 4 KB from round 6, no trend; memcheck: +0 definitely and indirectly lost per case |
| Windows x64 (win-home) | identical rounds 11-18 and 19-20; -71 KB from round 2 to 20 |
| macOS x64 (macbook-air-frank) | 115,601,136 bytes in 135,222 blocks, identical in rounds 2-20 |
| macOS arm64 (macbook-pro-johannes) | 114,727,264 bytes in 147,703 blocks, identical in rounds 9-20 |

The gate: the Linux x64, Windows x64 MSVC and macOS arm64 Game jobs run "Random missions leak gate" (20 rounds),
which fails when the heap after round 20 is more than 4 KiB above the heap after round 10. A planted leak of 100
bytes per map fails it. First CI results: Linux +1,136 bytes, Windows +160, macOS arm64 -9,856.

The full 624-case sweep still rises (Linux CI 177 to 547 MB): new chapters bring new textures, meshes, sprites and
stats into the caches. That is caching, not a leak; whether those caches should be bounded is a separate decision.

## Still open

- Exit-time leaks (not growth): the `CLinkObject` registries (`LinkObject.cpp`, about 90 KB including the last
  map's objects) are never destroyed; the Game sets `LeakObjectsOnExit` at quit (`GameMain.cpp`,
  `MainLoopCommands.cpp` `ArmAllModulesLeakOnExit`) and needs a safe shutdown order instead; about 240 KB in
  fontconfig, Pango and GTK through libdecor for SDL's Wayland decorations, which hidden test windows need not load.
- The rest of "No leaks anywhere" below: gates for the Map Editor and Resource Editor tiers, GPU object counts,
  `DebugAllocator` for the Zig editors (they use `smp_allocator`, `c_bridge.zig` `c_allocator`/`page_allocator`).
- Run the measurement from the machines over ssh: `macbook-air-frank`, `macbook-pro-johannes` and win-home are
  reachable; the test needs the desktop session (macOS: `launchctl asuser`, Windows: a scheduled task with `/it`).

## The leak we have seen

`test-random-missions` on Linux x64 (`zig build test-random-missions -Dtarget=x86_64-linux-gnu --release=fast
-Dtest-mode=run -Drandom-missions-sweep=all`, CI on Xvfb and lavapipe) is killed by the runner every time, after
about 467 missions, at `ussr\leningrad\1 ... summer_russia\securearea05`. Run 37752686285 logged the memory every 20
seconds (the step's `free`/`ps` loop in `.github/workflows/cross-platform.yml`, linux-game):

| Time | `random-missions` RSS | Available |
| --- | --- | --- |
| 10:01:22 (sweep starts) | 0.9 GB | 14.1 GB |
| 10:04:02 | 8.5 GB | 6.7 GB |
| 10:06:22 | 14.4 GB | 0.9 GB |
| 10:08:02 | 15.2 GB | 0.27 GB |
| 10:09:02 | runner: "received a shutdown signal" | |

A steady 0.9 GB per 20 s, about **32 MB per mission**, never given back. The same full sweep passes on Windows
(16 GB runner) and macOS arm64 (7 GB runner), but that does not prove they do not leak: a GPU driver keeps textures
and buffers in video memory, outside the process's RSS, while lavapipe (a CPU Vulkan) keeps them in ordinary process
memory where the leak shows. So either the leak is Linux-only, or it is everywhere and only Linux makes it visible.
Find out which.

## Machines

Four real machines can run the game and its editors, each with a real GPU: **Linux x64**, **macOS x64** (an Intel
Mac), **Windows x64** and **macOS arm64**. CI covers only some of this: its macOS Intel runner has no Metal device
and Linux runs on lavapipe. Use the machines to answer what CI cannot, per platform, on real hardware:

| Machine | Memory counter | Leak tool |
| --- | --- | --- |
| Linux x64 | `VmRSS` in `/proc/self/status` | heaptrack, Valgrind; lavapipe to put GPU memory in RSS |
| macOS x64 | `task_info` / Activity Monitor | `leaks --atExit -- <tool>`, Instruments (Allocations, Leaks) |
| macOS arm64 | as macOS x64; unified memory, so GPU allocations show in the process's footprint | as macOS x64 |
| Windows x64 | `GetProcessMemoryInfo` / Task Manager (and GPU memory per process) | debug CRT `_CrtDumpMemoryLeaks`, UMDH |

Start on Linux x64 (it shows the leak), then run the same `only=` sweep with the per-mission measurement on the other
three. That answers whether the leak is Linux-only or everywhere.

## The test

`tools/zig/random_missions_test.cpp`. One engine session for the whole sweep (`BkEditorStart`, line 349); per case
(`RunCase`, from line 190): `CMapInfo::CreateRandomMap` into `zig-out/local-test/...`, `NMapFile::Read` of the
result, `BkEditorOpenMap` (the engine loads the map and its textures), and for `bRegenerate` cases a second
`CreateRandomMap` from the stored seed. The test directory is deleted after a passing case, so disk does not grow.

Run it by hand (after `zig build install-game test-random-missions --release=fast -Dtest-mode=compile`, or just the
build step once): the binary is staged beside Game, and must run from there, as the build step does
(`build.zig`, `addEngineHostedTool`):

```sh
cd zig-out/game/linux/x86_64/release
./random-missions-test . "$REPO/zig-out/local-test" only=leningrad
```

`only=<text>` selects the cases whose path contains the text; `cover-from=<n>` resumes. `BK_DEBUG_LOG=1` enables
`DebugTrace` output in release builds.

## Suggested order

1. Reproduce and measure per mission: print `VmRSS` from `/proc/self/status` after each case (a small change to
   `RunCase`), on a real GPU first, then under lavapipe (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`,
   the CI setup). Same growth on the real GPU means a CPU-side leak, which then exists on every platform.
2. Split generation from loading: run a sweep that skips `BkEditorOpenMap`, and one that opens the same shipped map
   repeatedly without generating. Whichever grows holds the leak.
3. Find the allocations: `heaptrack ./random-missions-test . "$REPO/zig-out/local-test" only=leningrad` (works on the
   release binary through `LD_PRELOAD`, no rebuild), then `heaptrack_print --print-leaks`. Valgrind works too but is
   very slow on the engine. For GPU objects, count SDL GPU creates and releases (textures, buffers, transfer buffers,
   samplers) per mission in GFXGPU; `-Dsdl-debug=true` turns on SDL GPU validation.
4. Fix the cause, not the symptom. A port bug is fixed, never adopted; a leak that was already in the 2003 code is
   fixed too.
5. Check the fix on all four machines (see Machines), with the per-mission measurement flat on each.
6. Turn the per-mission measurement into a gate (see below), then let CI prove Linux x64 Game green, then merge
   `feat/ci-per-product-jobs` (with the fix) into `main` and push.

## No leaks anywhere: what the gate should become

The decision covers the game, the Map Editor, the Resource Editor and later ELK and the small tools, on every
platform. Suggested pieces, to be planned as their own phase once the first leak is understood:

- **A memory budget in the long-running tiers.** Random missions, the Map Editor scenarios, the Resource Editor's
  `resource-editor-auto-*` runs, the Game's own tiers: record the process memory after warm-up and at the end, and fail
  when it grows more than a small, stated budget. Works on every platform with the platform's own counter (`VmRSS` on
  Linux, `task_info` on macOS, `GetProcessMemoryInfo` on Windows).
- **GPU object counts.** Every create in GFXGPU counted against its release; a test that opens and closes maps checks
  the counts return to where they were. This catches what RSS cannot see on a real GPU.
- **A leak checker per platform in CI.** Linux: heaptrack or Valgrind's leak check on a short run of the engine tier.
  macOS: `leaks --atExit -- <tool>`. Windows: the debug CRT's `_CrtDumpMemoryLeaks` in debug builds (the CI jobs run
  debug tiers already).
- **Zig code.** The editors' Zig side should allocate through `std.heap.DebugAllocator` in debug and test builds and
  fail on its leak report at exit; check what `Sources/editor` uses today.

## Environment notes

- `AGENTS.md` applies: Zig 0.16, CRLF for all text files, tests write only under `zig-out/local-test`, never into
  shipped `Data/` or the user's profile, saves, settings or cloud sync. Commit messages: conventional with a scope,
  for example `fix(engine): ...`.
- Long steps: a full random-missions sweep takes about 10 minutes of CPU on a fast machine once built, the first build
  of the release engine more. Run `only=` subsets while investigating.
- CI: `gh` needs `-R jmfrank63/Blitzkrieg`. Logs of a single job before the run ends:
  `gh api --allow-escape-sequences repos/jmfrank63/Blitzkrieg/actions/jobs/<job-id>/logs`.

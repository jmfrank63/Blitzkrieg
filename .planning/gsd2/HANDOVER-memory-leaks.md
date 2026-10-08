# Handover: memory leaks (2026-10-08)

Written on win-home for a Linux x64 machine. Finding the leak below comes before anything else in the plan
(`HANDOVER.md`), Small tools included, by the maintainer's decision of 2026-10-08: "we should make sure we have no
leaks at all, nowhere on any platform and not on game or editors."

## Where the branches are

- `main` at `036bd9eb6`: M001 (the Resource Editor) merged. Its first CI run (37720133938) was red only because
  macos-intel ran out of time on a cold cache; fixed on the CI branch below.
- `feat/ci-per-product-jobs` at the commit that adds this file, pushed, **not merged**. It is the CI rework of
  2026-10-08: one job per platform and product ("Windows x64 MSVC - Game", "- Resource Editor", "- Map Editor", the
  same for macOS arm64 and Linux x64), never-running jobs that name what is not tested and why, caches saved from
  main only, the release Windows package only, slimmer uploads. Windows x64 Game went from 1 h 22 min to 35 min.
  Every job is green except **Linux x64 - Game**, which fails on the leak. Merge it into `main` once that job is
  green.
- `feat/resource-editor` has one commit not on `main`: `32a193718` (auto-bld grey-pixel limit, from a real Intel Mac).
  Merge or cherry-pick it before M001 is closed.

Start from `feat/ci-per-product-jobs`; work on a branch `fix/memory-leaks` from it (CI runs on `fix/**`).

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
5. Turn the per-mission measurement into a gate (see below), then let CI prove Linux x64 Game green, then merge
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

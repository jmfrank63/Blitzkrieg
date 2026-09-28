---
phase: 03-map-editor-plan-6-finish-m1
plan: 02
subsystem: map-editor
tags: [zig, process-spawn, editor-bridge, imgui, test-launch]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 01)
    provides: "Game -editor-test switch (session-only profile, forced windowed, no cloud sync, no first-visit help), BK_AUTO_UI units= verb, and the SMiniMapUnitInfo/map-unit coordinate note"
provides:
  - "BkEditorPaths and BkEditorTestMapPath: the host roots and the per-profile/mod generated-data path a test copy writes to, with stale-sibling cleanup"
  - "testlaunch.zig: argv-array construction, gamePath, and non-blocking Running.poll/terminate/waitBlocking for spawning Game beside MapEditor"
  - "MapEditor --game-reads-it: a headless tier that places a unit, test-launches Game, and asserts the placed unit is played"
  - "F5 / Test in game in the interactive app, with the D-06 still-running Restart/Keep prompt and failure reporting"
affects: [03-03, 03-04, 03-08]

# Actuals (#2632)
actuals:
  tokens: 18700
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Cross-thread/deferred UI state as a small enum state machine tested standalone (TestLaunchPrompt), mirroring panels_logic.PathSlot/FileActions's existing request/deliver/take shape."
    - "A dedicated, allocator-backed Io.Threaded instance per process, never std.Io.Threaded.global_single_threaded, once the app needs to spawn a child process (that singleton's allocator is .failing by design)."
    - "Non-blocking child lifecycle polling reimplements the OS's own WNOHANG/WaitForSingleObject probe rather than calling std.process.Child.wait, which blocks."

key-files:
  created:
    - Sources/editor/app/testlaunch.zig
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - build.zig
    - tools/zig/editor_bridge_test.cpp

key-decisions:
  - "BkEditorTestMapPath lower-cases the mod folder before NGeneratedData::ModKey (which itself keeps case), so \"Some Mod\" and \"some mod\" resolve to the same generated-data folder - matching the game's own -mod= command-line handling, which lower-cases the whole line before this bridge ever sees a mod name."
  - "The BK_AUTO_UI units= query point is the placed object's MAP position (point.map_x/point.map_y), not the scene/world position BkEditorSetCamera takes - bridge.h documents AI-side coordinates (what GetMiniMapInfo reports) as map units, and this was confirmed empirically against the actual test map before locking in the formula (see \"Coordinate scale correction\" below)."
  - "testMapPath/saveCopy failures go to the status bar like any other edit failure; only a spawn that cannot find Game beside MapEditor (FileNotFound) gets its own modal - matching the plan's exact three-modal list (still-running prompt, exit report, missing-game) rather than inventing a fourth."

requirements-completed: [D-01, D-03, D-04, D-05, D-06, D-07, D-08, D-09, SPEC-TEST-LAUNCH, SPEC-GAME-READS-IT]

coverage:
  - id: D1
    description: "Headless test-launch tier: the editor places a unit for player 0, writes a test copy the game's generated-data mount finds, and the real Game plays it end to end (zig build map-editor-game-reads-it)"
    requirement: "SPEC-GAME-READS-IT"
    verification:
      - kind: e2e
        ref: "zig build map-editor-game-reads-it -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (prints \"map-editor: game reads it PASS\")"
        status: pass
    human_judgment: false
  - id: D2
    description: "BkEditorPaths and BkEditorTestMapPath: host roots, the exact generated-data maps path per profile/mod, argument validation (bare .bzm name, IsRelativeDataName), stale-sibling cleanup, and REFUSED-with-out[0]==0 on a short buffer"
    requirement: "D-01, D-02, D-08, D-09"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestPathsAndTestMapPath (zig build test-editor-bridge, \"editor-bridge: PASS\")"
        status: pass
    human_judgment: false
  - id: D3
    description: "testlaunch.zig: argv-array construction (a mod folder with a space stays one element), gamePath beside the running executable, and exit-outcome classification (clean/early_failure/failure)"
    requirement: "D-08, D-09"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-testlaunch (folded into `test`)"
        status: pass
    human_judgment: false
  - id: D4
    description: "TestLaunchPrompt (D-06): idle/asking_restart/restarting/reporting transitions - ask while running, Keep leaves it alone, Restart waits for the exit then starts, an early failure reports the code and log path, a clean exit reports nothing"
    requirement: "D-06"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-panels"
        status: pass
    human_judgment: false
  - id: D5
    description: "F5 and the \"Test in game\" menu item in the running interactive app: the game opens windowed beside the editor showing the placed unit, the editor keeps drawing, a second F5 shows the restart prompt, and quitting the game leaves the map, its \"*\" and its undo history intact"
    verification: []
    human_judgment: true
    rationale: "Visual/interactive confirmation that a real window opens beside the editor's and behaves correctly - embedded as this plan's <human-check> per workflow.human_verify_mode's end-of-phase default; the verifier harvests it into the phase's UAT."
  - id: D6
    description: "The app tier still passes with panels.State's new fields (io, environ, test_game, test_prompt, mod_folder) wired through every caller"
    verification:
      - kind: automated_ui
        ref: "zig build map-editor-host-check map-editor-smoke (2 PASS lines) and zig build test (full suite)"
        status: pass
    human_judgment: false

# Metrics
duration: ~55min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 02: Test launch in the game Summary

**The Map Editor test-copies the current map into the game's generated-data mount and starts `Game -editor-test` beside itself - headlessly provable end to end (`map-editor-game-reads-it`) and reachable in the running app via F5, with a still-running Restart/Keep prompt and failure reporting.**

## Performance

- **Duration:** ~55 min
- **Started:** approx. 2026-09-28T03:20:00Z (not captured at session start; derived from the prior plan's completion timestamp and this plan's first commit)
- **Completed:** 2026-09-28T04:10:13Z
- **Tasks:** 2 completed
- **Files modified:** 10 (1 created, 9 modified)

## Accomplishments

- `BkEditorPaths` (host `BaseRoot`/`UserRoot`, OS-native separators) and `BkEditorTestMapPath` (the profile+mod generated-data `maps\<name>.bzm` path, reusing `NProfile::GeneratedDirectory`/`NGeneratedData::ModKey` rather than re-deriving sanitization) added to the bridge, both `Guarded`, both engine-tier tested.
- `BkEditorTestMapPath` validates `file_name` (bare, `IsRelativeDataName`, ends `.bzm`) and `profile` (non-empty), creates the target directory, deletes a stale same-stem `.xml` sibling so the game's newer-of-the-pair load can't pick up a leftover, and is `REFUSED` with `out[0] == 0` on a too-short buffer.
- New `Sources/editor/app/testlaunch.zig` (std-only, imports nothing of SDL or the engine adapter): `buildArgv` (never a shell string - a mod folder with a space stays one argv element), `gamePath` (beside the running executable, D-08), and `Running.poll/terminate/waitBlocking` - a non-blocking lifecycle probe (POSIX `waitpid(..., WNOHANG)`; Windows `WaitForSingleObject`+`GetExitCodeProcess`) so D-06's still-running check never stalls a frame. Windows is deliberately not put in a kill-on-close job (T-03-02-05 - accepted risk, matches D-03).
- `MapEditor --game-reads-it <map> [<log>]`: places the catalogue's first `SGVOGT_UNIT` for player 0 at the smoke's own free-ground point, writes a test copy, spawns `Game` with `BK_AUTO_UI=units=.../shot/exit`, and asserts the log shows the unit near player 0 and the document is still dirty at its original path (D-01) - `zig build map-editor-game-reads-it` passes locally.
- `TestLaunchPrompt` (panels_logic.zig): D-06's idle/asking_restart/restarting/reporting state machine, standalone-tested.
- Interactive app: a "Test" menu with "Test in game" (F5), the still-running Restart/Keep modal, an exit-failure report modal, a "no game beside MapEditor" modal on a `FileNotFound` spawn, and `pollTestGame` polling the running game once a frame.
- `build.zig`: `map-editor-game-reads-it` (local-only) and `test-map-editor-testlaunch` (host, folded into `test`) steps; `has_side_effects = true` added to `test-editor-bridge`'s run (it reads staged `Data`, not a file input of the step).

## Task Commits

Each task was committed atomically:

1. **Task 1: Headless test launch end to end - the game plays a map the editor edited** - `095a99c6a` (feat)
2. **Task 2: Test in game in the app - menu and F5, restart prompt, failure report, engine-tier path checks** - `e0d337826` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE/ROADMAP/REQUIREMENTS update)

## Files Created/Modified

- `Sources/editor/app/testlaunch.zig` - new: argv construction, gamePath, non-blocking process lifecycle
- `Sources/src/EditorBridge/bridge.h` / `bridge.cpp` - `BkEditorPathSet`, `BkEditorPaths`, `BkEditorTestMapPath`
- `Sources/editor/app/c_bridge.zig` - `RealBridge.paths/testMapPath/saveCopy`
- `Sources/editor/app/main.zig` - `--game-reads-it` mode; a real `Io.Threaded` instance (see Deviations); `io`/`environ` threaded into every `panels.State.init` call and `pollTestGame` into `run`'s loop
- `Sources/editor/app/view.zig` - `unit_game_type` exported for `--game-reads-it`'s unit lookup
- `Sources/editor/app/panels.zig` - `State.{io,environ,paths,test_game,test_prompt,mod_folder}`, the "Test" menu, F5, the three modals, `pollTestGame`
- `Sources/editor/app/panels_logic.zig` - `TestLaunchPrompt`
- `build.zig` - `map-editor-game-reads-it`, `test-map-editor-testlaunch` steps; `has_side_effects` on `test-editor-bridge`
- `tools/zig/editor_bridge_test.cpp` - `TestPathsAndTestMapPath`

## Decisions Made

- Mod folder lower-cased before `NGeneratedData::ModKey` (see key-decisions above) - matches the game's own `-mod=` handling.
- The `units=` query uses the placed object's **map** position, not its scene/world position - see "Coordinate scale correction" below.
- `testMapPath`/`saveCopy` failures go to the status bar; only a `FileNotFound` spawn gets a modal - matching the plan's exact three-modal list.

## Coordinate scale correction (important for future plans reading GetMiniMapInfo-shaped data)

03-01-SUMMARY.md's factor ("1 `SMiniMapUnitInfo` unit = 2 AI tiles = 64 world units") is the right divisor, but the *input* it divides is not the scene/world coordinate `BkEditorSetCamera`/`BkEditorScreenToWorld` use - it is the **map** coordinate (`bridge.h`'s own doc comment already said this: "Map units are the file's and the AI's"). Verified empirically against the actual generated test map: querying `BK_AUTO_UI=units=<map_x/64>x<map_y/64>x1` found the placed unit at radius 1; the scene-world equivalent (`world_x/64`) missed it by roughly 20 units of this scale (about 1300 world units away). `main.zig`'s `gameReadsIt` uses `point.map_x`/`point.map_y` accordingly; any future code reading `GetMiniMapInfo`-shaped AI data from the editor side should do the same.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `std.Io.Threaded.global_single_threaded`'s allocator is `.failing`, so `std.process.spawn` always returned `OutOfMemory`**
- **Found during:** Task 1, first `zig build map-editor-game-reads-it` run
- **Issue:** `main.zig`'s `io` came from `std.Io.Threaded.global_single_threaded.io()`, a minimal fallback whose `allocator` field is `.failing` by design (zero-concurrency, no-allocation intent). `std.process.spawn`'s POSIX path allocates an arena from that same allocator for argv/env construction, so every spawn failed before `fork()` ever ran, surfacing as a bare `error.OutOfMemory` with no other symptom.
- **Fix:** `main()` now builds its own `var threaded: std.Io.Threaded = .init(gpa, .{ .environ = minimal.environ });` once, and every mode (interactive, `--check`, `--smoke`, `--game-reads-it`) shares its `.io()`. This also fixed `t.scanEnviron()`'s PATH lookup to see the real environment instead of an empty block.
- **Files modified:** `Sources/editor/app/main.zig`
- **Verification:** `zig build map-editor-game-reads-it` spawns and runs `Game` successfully; `zig build test` (full suite) still passes.
- **Committed in:** `095a99c6a` (Task 1 commit)

**2. [Rule 3 - Blocking] `view.zig`'s `unit_game_type` was private, but `--game-reads-it` needs the same SGVOGT_UNIT constant to find a placeable unit in the catalogue**
- **Found during:** Task 1
- **Issue:** `main.zig`'s new headless mode needed the placer's own "SGVOGT_UNIT" game-type constant to scan the catalogue for the first placeable unit, exactly as the placer tool's own default does; duplicating the literal `1` would drift silently if the constant ever changed.
- **Fix:** Exported `view.zig`'s `unit_game_type` (`const` to `pub const`), updated its doc comment to name the second caller.
- **Files modified:** `Sources/editor/app/view.zig`
- **Verification:** compiles; `zig build test-map-editor-view` (unaffected, still passes) confirms no regression to the constant's original caller.
- **Committed in:** `095a99c6a` (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking issues, both necessary for Task 1 to compile and run at all). No scope creep: neither changed the plan's design, both were pre-existing gaps the plan's own new capability (process spawning) exposed.

## Issues Encountered

- The plan's own coordinate-scale note (03-01-SUMMARY.md) named "world units" as the `units=` divisor's input; empirically this was the wrong coordinate (map units, not scene/world units - see the correction above). Caught by the plan's own acceptance criterion (`n >= 1` units of player 0) failing at `total 0` on the first real run, not by code review - a reminder that this measurement was explicitly flagged in 03-01-SUMMARY as needing the verifier's confirmation before this plan relied on it, and it did in fact need a correction.
- `std.time`/`std.Thread.sleep` have no runtime clock/sleep functions in this Zig snapshot (0.16.0) - everything monotonic goes through `std.Io.Clock`, requiring `io` threaded into `Running.poll/terminate/waitBlocking` (the plan's shorthand signatures omitted it). Not a deviation in behavior, just a necessary signature detail once actually writing the code against this Zig version.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Test in game (F5, the menu item, the still-running prompt, failure reporting) is built and automated-tested; the plan's own `<human-check>` (F5 opening the real game beside the editor, the restart prompt, the map/undo history surviving) is embedded in Task 2's `<verify>` for the phase-level UAT per `workflow.human_verify_mode: end-of-phase` - not run interactively in this session.
- `state.mod_folder` is wired through `testMapPath`/`testlaunch.start` but always `null` in this plan; 03-08 (the Mod menu) fills it in, and D-09 (test loads the editor's own mod) is otherwise complete.
- The corrected map-vs-world coordinate note above is load-bearing for any later plan reading `GetMiniMapInfo`-shaped AI data from the editor side.
- No blockers for the rest of phase 3.

## Self-Check: PASSED

- `[ -f Sources/editor/app/testlaunch.zig ]`, `[ -f Sources/src/EditorBridge/bridge.h ]`, `[ -f Sources/src/EditorBridge/bridge.cpp ]`, `[ -f Sources/editor/app/c_bridge.zig ]`, `[ -f Sources/editor/app/main.zig ]`, `[ -f Sources/editor/app/view.zig ]`, `[ -f Sources/editor/app/panels.zig ]`, `[ -f Sources/editor/app/panels_logic.zig ]`, `[ -f build.zig ]`, `[ -f tools/zig/editor_bridge_test.cpp ]` - all FOUND.
- `git log --oneline --all --grep="03-02"` - matched by subject prefix `feat(03-02)`, both commits present (`095a99c6a`, `e0d337826`).
- All task `<acceptance_criteria>` re-verified above (grep checks for `GeneratedDirectory`/`ModKey`, the zero `sdl3`/`c_bridge` token count in `testlaunch.zig`, `testlaunch.start` found in `panels.zig`) - all PASS.
- Plan-level `<verification>`: `zig build map-editor-game-reads-it` PASS; engine tier (`test-editor-bridge`), panels (`test-map-editor-panels`), testlaunch host tests (`test-map-editor-testlaunch`), host check and smoke (`map-editor-host-check`, `map-editor-smoke`) all pass; full `zig build test` also re-run clean.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

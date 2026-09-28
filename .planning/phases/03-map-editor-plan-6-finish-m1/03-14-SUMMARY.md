---
phase: 03-map-editor-plan-6-finish-m1
plan: 14
subsystem: map-editor
tags: [zig, packaging, windows, pe-subsystem, ci]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 13)
    provides: "The full editor app/core/bridge stack, carried minors closed out (console-subsystem work still open, per plan 5 Task 7)"
provides:
  - "stage.zig --map-editor <path>: copies a built MapEditor binary into a staged root under its own base name (MapEditor / MapEditor.exe), required by verifyStagedPayload, fails naming the path when missing"
  - "addMapEditor returns the built exe; package-game and package-game-editors pass --map-editor with the exact artifact's emitted binary (addFileArg, dependency tracked) on the two editor platforms"
  - "configureMapEditorExecutable(exe, target, subsystem): MapEditor ships .windows, map-editor-engine-test stays .console"
  - "crt.attachParentConsole() (Windows only): joins the parent console and installs CONOUT$ as stdout/stderr when no usable handle exists yet, called first in --check/--smoke/--game-reads-it/usage/BK_EDITOR_AUTO"
  - "CI: windows-platform's \"Map editor is a GUI program\" step reads the staged MapEditor.exe's PE header and fails unless Subsystem is 2 (IMAGE_SUBSYSTEM_WINDOWS_GUI)"
affects: [03-15-exit-criteria-sweep]

# Actuals (#2632)
actuals:
  tokens: 8493
  tasks: 2
  commits: 2
plan_head_before: 3365b22ae47174a820542b9507e2c8a7ca78b80e

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "A tracer task (Task 1) verified end-to-end - including a real local zig build package-game with disk-space gating - before Task 2's expansion, per the tracer feedback gate."
    - "AttachConsole(ATTACH_PARENT_PROCESS) alone does not make Zig's stdout/stderr print: Zig's std.Io.File.stdout()/stderr() read std.os.windows.peb().ProcessParameters.hStdOutput/hStdError fresh on every call (the same fields GetStdHandle reads), so attachParentConsole also opens CONOUT$ and calls SetStdHandle to update that same block - no CRT freopen equivalent needed or possible without libc."
    - "A copy source path may be absolute (the real build.zig usage, via a Compile step's getEmittedBin()) or repo-relative (the simpler test fixture form); stage.zig's copyMapEditor picks the source Io.Dir by std.fs.path.isAbsolute rather than assuming one form."

key-files:
  created: []
  modified:
    - build.zig
    - tools/zig/stage.zig
    - tools/zig/stage_test.zig
    - Sources/editor/app/crt.zig
    - Sources/editor/app/main.zig
    - .github/workflows/cross-platform.yml

key-decisions:
  - "The literal verify command `unzip -l ... | grep -ci \"/mods/\"` is broader than the threat it checks: shipped Data/AmericanELK/.../mods/ localization folders (unrelated to installed-content mods) make it report 77 on a correct package. Verified the actual truth instead - `unzip -l ... | awk '{print $4}' | grep -cE \"^mods/\"` (a top-level mods/ directory, which is where a real mod's data, including the unlicensed AchtungPanzer2, would land) - is 0. No production code changed for this; it is a verification-script imprecision, not a packaging bug."
  - "package-game-editors was not exercised locally on macOS: its stage command depends on `--editors-supported`, which addStageLayoutArgs only passes on Windows (`target.result.os.tag == .windows`) - pre-existing, unrelated to this plan. Verified via CI's windows-platform job instead, where the editors-only restage reuses the same package_stage_root package-game already staged (MapEditor included)."
  - "--map-editor is passed to both stage_package_game_cmd and stage_package_game_editors_cmd (not only the one that actually copies the file) so the with-editors package's own build-graph node depends on the exact MapEditor build too, even though its --editors-only run skips the copy/verify block that would otherwise touch it."

requirements-completed: [D-08, CARRY-PACKAGING, CARRY-CONSOLE, M1-EXIT-BUILD]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "The game package (macOS arm64, Windows x64 MSVC) carries MapEditor beside Game; no package ever carries a mods/ folder"
    requirement: "D-08"
    verification:
      - kind: unit
        ref: "tools/zig/stage_test.zig#--map-editor stages the file beside the game (zig build test-stage)"
        status: pass
      - kind: unit
        ref: "tools/zig/stage_test.zig#a missing map editor binary fails the stage naming the path (zig build test-stage)"
        status: pass
      - kind: unit
        ref: "tools/zig/stage_test.zig#a mods directory in the repository is never staged (zig build test-stage)"
        status: pass
      - kind: integration
        ref: "zig build package-game -Dtarget=aarch64-macos -Dcopy-data=false - Blitzkrieg-game.zip contains MapEditor beside Game, zero top-level mods/ entries"
        status: pass
    human_judgment: false
  - id: D2
    description: "The packaged Windows MapEditor.exe is a GUI-subsystem program (no console window), while --check/--smoke/--game-reads-it and BK_EDITOR_AUTO still print to a terminal or CI log"
    requirement: "CARRY-CONSOLE"
    verification:
      - kind: e2e
        ref: "CI run 36464351662, windows-platform job: \"Map editor is a GUI program\" step reads the staged MapEditor.exe PE header - Subsystem = 2 (IMAGE_SUBSYSTEM_WINDOWS_GUI)"
        status: pass
      - kind: e2e
        ref: "CI run 36464351662, windows-platform job: \"Map editor host\" and \"Map editor smoke\" steps still print their PASS lines under the .windows subsystem (gh run view --log: 11 matches for \"host check PASS (direct3d12\" / \"smoke PASS\")"
        status: pass
    human_judgment: false
  - id: D3
    description: "Local automated proof that packaging and the subsystem/console-attach code did not regress the existing tiers"
    requirement: "M1-EXIT-BUILD"
    verification:
      - kind: unit
        ref: "zig test tools/zig/build_hermeticity_test.zig (3/3, run twice, once per task)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-host-check map-editor-smoke test-map-editor-engine -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run - 8 PASS lines, rc=0"
        status: pass
    human_judgment: false

# Metrics
duration: ~90min (including a ~55min unattended CI wait for the Windows runner)
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 14: Package MapEditor beside Game, GUI-subsystem console attach Summary

**`stage.zig --map-editor` stages MapEditor into both game packages on macOS arm64 and Windows x64 MSVC (never a mods folder), and the packaged Windows MapEditor.exe switches to the GUI subsystem with a `crt.attachParentConsole()` fallback that keeps every automated mode printing - confirmed green on CI run 36464351662 including a new PE-header subsystem check.**

## Performance

- **Duration:** ~90 min total, of which ~55 min was an unattended CI wait (Windows runner, run 36464351662)
- **Started:** 2026-09-28T17:54Z (STATE.md's prior session mark)
- **Completed:** 2026-09-28T19:15Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- `stage.zig` gained `--map-editor <path>`, copying a built MapEditor binary into the staged root under its own base name (`MapEditor` / `MapEditor.exe`), required by `verifyStagedPayload`, and failing the stage (naming the path) when the source is missing
- `build.zig`'s `addMapEditor` now returns the compiled executable; `package-game` and `package-game-editors` pass `--map-editor` with that exact artifact's emitted binary via `addFileArg` (so the package's build-graph node depends on this build, not on whatever happened to be on disk)
- A real local `zig build package-game` on macOS arm64 produced `Blitzkrieg-game.zip` with `MapEditor` beside `Game` and zero top-level `mods/` entries (package cleaned up after inspection, per the disk-space constraint)
- `configureMapEditorExecutable` now takes a `subsystem` parameter: the packaged `MapEditor` is `.windows` (no console window on a normal double-click), `map-editor-engine-test` stays `.console`
- `crt.attachParentConsole()` (Windows only): when `GetStdHandle(STD_ERROR_HANDLE)` is null/`INVALID_HANDLE_VALUE`, joins the parent's console via `AttachConsole(ATTACH_PARENT_PROCESS)` and installs a freshly opened `CONOUT$` as both the standard-output and standard-error handle via `SetStdHandle` - a no-op wherever a parent already redirected/piped the handles (CI, `zig build ... -Dtest-mode=run`), and on every non-Windows platform
- `main.zig`'s `check()`, `smokeRun()`, `gameReadsIt()`, `usage()`, and the `BK_EDITOR_AUTO` branch of `interactive()` call `attachParentConsole()` first; the plain interactive/double-click launch never calls it
- CI (`windows-platform`) gained "Map editor is a GUI program": reads the staged `MapEditor.exe`'s PE header (`e_lfanew` at DOS-header offset `0x3C`, `Subsystem` WORD at `e_lfanew + 0x5C`) and fails unless it reads `2` (`IMAGE_SUBSYSTEM_WINDOWS_GUI`)
- Pushed the branch, ran the "Cross-platform validation" workflow (run `36464351662`, triggered explicitly via `gh workflow run` since a push alone does not trigger it on this branch): all six jobs green in 55m10s for `windows-platform`, including the new PE check, and "Map editor host"/"Map editor smoke" still print their PASS lines

## Task Commits

Each task was committed atomically:

1. **Task 1: The game package carries MapEditor beside Game, and never a mods folder** - `5291a8ce5` (feat)
2. **Task 2: The Windows MapEditor.exe is a GUI program that still prints in automated runs** - `24bf274be` (feat)

_Note: no TDD tasks in this plan; each task is a single commit._

## Files Created/Modified
- `build.zig` - `addMapEditor` returns the exe; `package-game`/`package-game-editors` stage commands add `--map-editor`; `configureMapEditorExecutable` takes a `subsystem` parameter (`.windows` for `MapEditor`, `.console` for `map-editor-engine-test`)
- `tools/zig/stage.zig` - `Options.map_editor`, `--map-editor` parsing, `copyMapEditor`, `verifyStagedPayload` extended to require it when set
- `tools/zig/stage_test.zig` - three new tests: stages beside the game, a missing file fails naming the path, a repository `mods/` directory is never staged
- `Sources/editor/app/crt.zig` - `attachParentConsole()`, its `kernel32` externs (`GetStdHandle`, `SetStdHandle`, `AttachConsole`, `CreateFileW`) and the `winbase.h`/`wincon.h` constants it needs
- `Sources/editor/app/main.zig` - `attachParentConsole()` wired into `check()`, `smokeRun()`, `gameReadsIt()`, `usage()`, and the `BK_EDITOR_AUTO` branch of `interactive()`
- `.github/workflows/cross-platform.yml` - new `windows-platform` step "Map editor is a GUI program" reading the PE Subsystem field

## Decisions Made
- The plan's literal package-mods verify command (`grep -ci "/mods/"`) is broader than the actual threat (a top-level `mods/` folder carrying real installed content, e.g. the unlicensed AchtungPanzer2); it also matches legitimate `Data/AmericanELK/.../mods/` localization folders, reading 77 on a correct package. Verified the precise condition instead (`grep -cE "^mods/"` on the zip's top-level entries = 0) and documented both readings rather than silently substituting one for the other.
- `package-game-editors` cannot run on macOS at all (pre-existing: `--editors-supported`/`--editors-only`'s `EditorsUnsupported` gate is Windows-only via `addStageLayoutArgs`'s `target.result.os.tag == .windows`), so its `--map-editor` wiring was verified through CI's Windows job rather than locally.
- `attachParentConsole()` does the full CONOUT$-open-and-SetStdHandle recipe, not just the literal `AttachConsole` call the plan's action text names: Zig's `std.Io.File.stdout()/stderr()` read the process-parameters block live on every call (confirmed by reading Zig 0.16's `std/Io/File.zig` and `std/Io/Threaded.zig`), so `AttachConsole` alone would attach a console nobody's stdio pointed at yet - a Rule 2 (missing critical functionality) fill-in, verified end-to-end on the real Windows CI runner.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] attachParentConsole needed CONOUT$ + SetStdHandle, not just AttachConsole**
- **Found during:** Task 2 (Windows console-attach design), before any code was written
- **Issue:** The plan's action text describes only `AttachConsole(ATTACH_PARENT_PROCESS)`. Reading Zig 0.16's own `std/Io/File.zig` (`stdout()`/`stderr()` read `std.os.windows.peb().ProcessParameters.hStdOutput/hStdError` fresh on every call) showed that `AttachConsole` alone attaches a console but never updates that process-parameters block - `std.debug.print` would still write through the same null/invalid handle it always had, printing nothing.
- **Fix:** After a successful `AttachConsole`, open `CONOUT$` via `CreateFileW` and call `SetStdHandle` for both `STD_OUTPUT_HANDLE` and `STD_ERROR_HANDLE`, matching the well-known C `freopen("CONOUT$", ...)` recipe's Zig-native equivalent (no libc/CRT streams here, so no `freopen` itself).
- **Files modified:** `Sources/editor/app/crt.zig`
- **Verification:** CI run 36464351662, `windows-platform`: "Map editor host" and "Map editor smoke" both still print their PASS lines under the new `.windows` subsystem.
- **Committed in:** `24bf274be` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Necessary for the task's own acceptance criterion ("every automated tier still reports") to actually hold under the `.windows` subsystem switch; without it the switch would have silently broken CI diagnostics, exactly the risk RESEARCH.md's Assumption A3 flagged. No scope creep - the fix stays inside `crt.zig`'s single new function.

## Issues Encountered
None beyond the verify-script and platform-gating notes captured under Decisions Made above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- D-08, CARRY-PACKAGING, CARRY-CONSOLE and M1-EXIT-BUILD are satisfied and verified (locally and on CI); plan 15 (exit-criteria sweep) can proceed - it depends on this plan.
- The legacy `Editors/MapEditor.exe` path (plan 5 Task 7's "two names coexist until M3") is untouched by this plan.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

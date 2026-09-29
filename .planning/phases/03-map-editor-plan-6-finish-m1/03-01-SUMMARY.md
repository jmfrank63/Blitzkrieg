---
phase: 03-map-editor-plan-6-finish-m1
plan: 01
subsystem: game-cli
tags: [command-line, cloud-sync, profiles, ai-logic, bk-auto-ui, test-launch]

# Dependency graph
requires: []
provides:
  - Game -editor-test switch: session-only profile, forced windowed, cloud sync off, first-visit help off
  - Editor.TestLaunch global var, read by CloudProviderSelected and ShowTutorialIfNotShown
  - BK_AUTO_UI units=<x>x<y>x<r> verb for counting units near a point
  - Measured SMiniMapUnitInfo coordinate scale and the fresh-profile screen sequence, for plan 03-02
affects: [03-02-map-editor-testlaunch]

# Actuals (#2632)
actuals:
  tokens: 3240
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Command-line switches gate behavior through a single global var (Editor.TestLaunch) read at every call site that must change, rather than threading a flag through every function - CloudProviderSelected and ShowTutorialIfNotShown each read it directly."
    - "Order-independent CLI flags: -editor-test re-applies its forced state (windowed) once more after the whole parse loop, so a -fullscreen anywhere else on the same command line cannot win by appearing later."

key-files:
  created: []
  modified:
    - Sources/src/Game/main.cpp
    - Sources/src/Game/GameMain.h
    - Sources/src/Game/GameMain.cpp
    - Sources/src/Common/InterfaceScreenBase.cpp
    - tools/zig/game_command_line_test.cpp

key-decisions:
  - "CloudProviderSelected (not each of its five call sites) gates on Editor.TestLaunch: one guard closes startup, requested, periodic, on-save and on-exit cloud sync at once, matching the plan's own reasoning."
  - "The editor-test profile defaults to \"MapEditorTest\" only when -profile= is empty; -profile=Name with -editor-test still names the session-only profile, matching D-02's \"a dedicated editor test profile (e.g. MapEditorTest)\" wording rather than hardcoding the name."
  - "SMiniMapUnitInfo's coordinate scale was determined by reading CAILogic::GetMiniMapInfo's source directly (AILogicInternal.cpp) rather than by building the Map Editor app to cross-check against a live map dump - the source gives an exact, unambiguous factor with no reliance on a scale-model interpretation of visually similar-looking numbers, and avoided a second heavy engine-tier build."

requirements-completed: [D-02, D-04, D-05, D-07, SPEC-TEST-LAUNCH]

coverage:
  - id: D1
    description: "-editor-test parses in both ParseCommandLine and ProcessCommandLine, forces windowed order-independently, and is parse-tested"
    requirement: "D-05"
    verification:
      - kind: unit
        ref: "tools/zig/game_command_line_test.cpp (zig build test-game-command-line)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A test-mode launch never reads or writes profiles/active.cfg and never runs the legacy save/screenshot migration, leaving the player's real profile untouched"
    requirement: "D-02"
    verification:
      - kind: e2e
        ref: "command: -editor-test -profile=MapEditorTest -mod=None -windowed Multiplayer/coldwinter.bzm; asserted active.cfg unchanged and profiles/MapEditorTest created"
        status: pass
    human_judgment: false
  - id: D3
    description: "A test-mode launch begins no cloud sync (startup, periodic, on save, or on exit) regardless of the profile or root config.cfg"
    requirement: "D-02"
    verification:
      - kind: e2e
        ref: "command above; asserted zero \"sync begun\" lines and the \"cloud sync: off for the editor's test game\" trace"
        status: pass
    human_judgment: false
  - id: D4
    description: "A fresh MapEditorTest profile reaches the mission with no first-visit help screen and no other first-run screen (Personal Card, etc.) over it"
    requirement: "D-07"
    verification:
      - kind: e2e
        ref: "BK_UI_TRACE log of the harness run: only \"ui\\mission\" appears before the mission; two \"first-visit help skipped\" lines, no Personal Card"
        status: pass
    human_judgment: false
  - id: D5
    description: "BK_AUTO_UI units=<x>x<y>x<r> counts each player's units near a point in a running mission"
    requirement: "D-04"
    verification:
      - kind: e2e
        ref: "BK_AUTO_UI=...,units=0x0x65535,... on coldwinter: \"BK_AUTO_UI: units near 0,0 r 65535: total 28; player 0: 14; player 1: 14\""
        status: pass
    human_judgment: false
  - id: D6
    description: "The SMiniMapUnitInfo coordinate scale is measured and recorded for plan 03-02's test-launch coordinate handling"
    verification:
      - kind: other
        ref: "Sources/src/AILogic/AILogicInternal.cpp:1364-1365 (CAILogic::GetMiniMapInfo), read directly"
        status: pass
    human_judgment: true
    rationale: "This is a design-input measurement (a fact recorded for the next plan to consume), not a behavior a test asserts; the verifier should confirm the recorded factor matches the cited source before 03-02 relies on it."

# Metrics
duration: 35min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 01: Game -editor-test end to end Summary

**`-editor-test` gives the game a session-only, cloud-free, help-free test-launch mode (D-02/D-05/D-07), and a new `BK_AUTO_UI units=` verb lets a harness count units near a point in the running mission (D-04).**

## Performance

- **Duration:** ~35 min
- **Started:** approx. 2026-09-28T02:40:00Z (not captured at session start; derived from build/commit timestamps)
- **Completed:** 2026-09-28T03:15:11Z
- **Tasks:** 2 completed
- **Files modified:** 5

## Accomplishments

- `Game -editor-test` end to end: `NGame::CommandLineOptions::editorTest` (main.cpp's `ParseCommandLine`) and `SCmdParams::bEditorTest` (GameMain.cpp's `ProcessCommandLine`) both recognize the flag; the profile block skips reading/writing `profiles/active.cfg` and the one-time legacy migration in test mode, defaulting the profile name to `MapEditorTest` when `-profile=` is empty.
- `CloudProviderSelected` returns `false` whenever `Editor.TestLaunch` is set, which closes every cloud-sync gate in the file (startup, requested/retry, periodic post-save, and exit) through the one function all five already call.
- `CInterfaceScreenBase::ShowTutorialIfNotShown` skips the first-visit help in test mode and traces `BK_UI_TRACE: first-visit help skipped for the editor's test game` to stderr when `BK_UI_TRACE` is set.
- `-editor-test` forces windowed mode order-independently: it is re-applied once more after the whole `ProcessCommandLine` parse loop, so a `-fullscreen` argument appearing later on the same command line cannot undo it.
- New `BK_AUTO_UI` verb `units=<x>x<y>x<r>`: reads `IAILogic::GetMiniMapInfo`, counts each player's units within radius `r` of `(x,y)`, and prints one line listing only players with a nonzero count plus the total; a null `IAILogic` prints `BK_AUTO_UI: units unavailable` instead of crashing.
- Two new parse-test cases in `tools/zig/game_command_line_test.cpp` cover `-editor-test` end to end (with `-profile=`, `-mod=`, `-windowed`, and a map name) and the rejected `-editor-testx`.
- Measured and recorded (see "Coordinate scale measurement" below) the `SMiniMapUnitInfo` coordinate scale and confirmed the fresh-profile screen sequence is clean.

## Task Commits

Each task was committed atomically:

1. **Task 1: Game -editor-test end to end: session-only profile, cloud off, windowed, help off** - `08627362c` (feat)
2. **Task 2: BK_AUTO_UI units= verb, and the fresh-profile mission start measured** - `f817c52ef` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE/ROADMAP/REQUIREMENTS update)

## Files Created/Modified

- `Sources/src/Game/main.cpp` - `ParseCommandLine` accepts `-editor-test`; usage text lists it
- `Sources/src/Game/GameMain.h` - `CommandLineOptions::editorTest`
- `Sources/src/Game/GameMain.cpp` - `SCmdParams::bEditorTest`, `-editor-test` parsing (forced windowed, order-independent re-assertion after the loop), profile-block test-mode branch, `CloudProviderSelected`'s `Editor.TestLaunch` gate, `BK_AUTO_UI units=` verb
- `Sources/src/Common/InterfaceScreenBase.cpp` - `ShowTutorialIfNotShown`'s `Editor.TestLaunch` gate and `BK_UI_TRACE` line
- `tools/zig/game_command_line_test.cpp` - `-editor-test` and `-editor-testx` parse-test cases

## Decisions Made

- `CloudProviderSelected` (not each of its five call sites) gates on `Editor.TestLaunch`, per the plan's own recommendation - one guard closes startup, requested/retry, periodic post-save, and exit sync at once.
- The test profile name defaults to `MapEditorTest` only when `-profile=` is empty on the command line, so `-editor-test -profile=SomeOtherName` still works as a session-only profile named `SomeOtherName` rather than always forcing `MapEditorTest`.
- The `SMiniMapUnitInfo` coordinate-scale measurement was done by reading `CAILogic::GetMiniMapInfo`'s source directly rather than building the Map Editor app to cross-check against a live per-object dump of the map - see "Coordinate scale measurement" below for why this is the stronger evidence, not a shortcut.

## Coordinate scale measurement (for plan 03-02)

`SMiniMapUnitInfo.x`/`.y` (`Sources/src/AILogic/AITypes.h`) are **neither raw world units nor plain AI tiles** - they are a coarser, halved grid. Verified directly from `CAILogic::GetMiniMapInfo` (`Sources/src/AILogic/AILogicInternal.cpp:1364-1365`):

```cpp
const SVector tile( pUnit->GetTile() );
(*pUnitsBuffer)[(*pnLen)++] = SMiniMapUnitInfo( tile.x / 2, tile.y / 2, z, pUnit->GetPlayer() );
```

`pUnit->GetTile()` is `AICellsTiles::GetTile(pUnit->GetCenter())` = `worldPos / SAIConsts::TILE_SIZE` (`Sources/src/AILogic/aiconsts.h`: `TILE_SIZE = 32`). So:

**1 `SMiniMapUnitInfo` unit = 2 AI tiles = 64 world units.**

This matches the harness's own sample readings on `coldwinter.bzm` (all three sampled units had `x`/`y` under 100 - consistent with a coarse grid, not raw world-unit magnitudes, which would run into the thousands for a map this size) and is the exact factor a caller must apply to convert a `units=` coordinate (or any future editor-side use of `GetMiniMapInfo`) to and from world/map units.

## Fresh-profile screen sequence measurement (D-07)

With a freshly deleted `MapEditorTest` profile and `BK_UI_TRACE=1` (no `BK_NO_HELP`), the harness's `BK_UI_TRACE` log shows only the `"ui\mission"` screen being positioned before the mission starts - no Personal Card, no other first-run screen. The `"first-visit help skipped for the editor's test game"` trace fires (twice - once for the mission screen, once for a screen touched during the scripted exit), confirming D-07's "the test starts at the normal mission start with the briefing skipped" is already true with no further guard needed beyond `ShowTutorialIfNotShown`'s existing `Editor.TestLaunch` gate.

## Deviations from Plan

None - plan executed exactly as written. The temporary "units sample" debug print used during the coordinate-scale measurement (Task 2, point 2) was added and removed before the final commit, per the plan's own instruction; `grep -c "units sample" Sources/src/Game/GameMain.cpp` reports 0 in the committed state.

## Issues Encountered

- The Data directory was absent from this sparse worktree checkout. Per the plan's Task 1 first action and the dispatch's disk-space gate, `df -h .` was checked before restoring (19 GB free after `git sparse-checkout disable`, well above both the dispatch's 6 GB pre-check and the plan's own 10 GB post-restore threshold), so the restore and the full `install-game` build proceeded normally.
- `zig build install-game` compiles a vendored DXC/LLVM toolchain from source on a cold cache, which took roughly 8-10 minutes the first time in this worktree; subsequent incremental builds (after small GameMain.cpp edits) took well under a minute. Not a deviation, just a timing note for whoever runs plan 03-02 next in this same worktree.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `-editor-test` is ready for plan 03-02 to spawn `Game` from the Map Editor: `Game -editor-test -profile=MapEditorTest -mod=<Name-or-None> -windowed <map>` (per D-08/D-09, only the map path and mod name vary).
- The `SMiniMapUnitInfo` coordinate factor (1 unit = 2 AI tiles = 64 world units) is recorded above for any editor-side code that reads `GetMiniMapInfo`-shaped data; plan 03-02 itself only needs to spawn the game and pass the map/mod/profile arguments, so this factor is forward-looking rather than a direct dependency of that plan's test-launch spawn work.
- No blockers for 03-02.

## Self-Check: PASSED

- `[ -f Sources/src/Game/main.cpp ]`, `[ -f Sources/src/Game/GameMain.h ]`, `[ -f Sources/src/Game/GameMain.cpp ]`, `[ -f Sources/src/Common/InterfaceScreenBase.cpp ]`, `[ -f tools/zig/game_command_line_test.cpp ]` - all FOUND.
- `git log --oneline --all --grep="03-01"` returns 2 commits (`08627362c`, `f817c52ef`).
- All task `<acceptance_criteria>` re-verified above (see per-task verify output in this summary) - all PASS.
- Plan-level `<verification>`: `zig build test-game-command-line` re-run clean (rc=0) on the final committed state.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

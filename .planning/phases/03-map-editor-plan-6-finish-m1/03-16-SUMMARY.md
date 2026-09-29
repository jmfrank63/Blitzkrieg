---
phase: 03-map-editor-plan-6-finish-m1
plan: 16
subsystem: map-editor
tags: [zig, gap-closure, status-line, test-launch, view-tests, ci, packaging]
gap_closure: true

requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plans 01-15, 03-VERIFICATION.md)
    provides: "The M1 editor and the verification's open items"
provides:
  - "Status messages carry their source; every Test-in-game outcome leaves the status line right (WINDOWS.md 1 fixed)"
  - "Restart's terminated game is an expected exit: no spurious 'The game exited with code 1'"
  - "view.zig is generic over its input (ViewWith) and has 19 event-wiring tests run by test-map-editor-view, zig build test and CI (WINDOWS.md 2 fixed)"
  - "Literal-position scroll-direction tests (WINDOWS.md 3 fixed)"
  - "game-reads-it takes an in-game baseline count before placing"
  - "CI's windows-platform job builds package-game-editors and checks both zips; the first run found and 03-16 fixed a Windows compile error in tools/zig/package.zig"
affects: [/gsd-verify-work for phase 3, the orchestrator's real-Windows test]

plan_head_before: d9be49e77

tech-stack:
  added: []
  patterns:
    - "Comptime input seam: ViewWith(comptime Input: type) plus a bridge as anytype, so an SDL-wiring file compiles into a test with SDL headers but no linked SDL, engine or ImGui (a stub editor_imgui module)"
    - "Source-tagged status slot (view_math.StatusSlot): an operation that can succeed without an edit clears only its own message"

key-files:
  created:
    - Sources/editor/app/testing/imgui_stub.zig
    - .planning/phases/03-map-editor-plan-6-finish-m1/03-16-PLAN.md
  modified:
    - Sources/editor/app/view.zig
    - Sources/editor/app/view_math.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/testlaunch.zig
    - Sources/editor/app/main.zig
    - tools/zig/package.zig
    - build.zig
    - .github/workflows/cross-platform.yml
    - .planning/phases/03-map-editor-plan-6-finish-m1/03-14-SUMMARY.md
    - .planning/WINDOWS.md

key-decisions:
  - "The Test-in-game status is written to the view's shared slot, but tagged test_launch. A later start or a modal failure clears only a test_launch message, and an ordinary edit still clears everything, as before"
  - "Restart suppresses the report for any exit shape while restarting. The prompt, not describe(), knows the terminate was requested; describe() keeps calling those shapes failures, and a test pins that"
  - "view.zig's test module uses the app's own SDL translate-c module (types only, nothing linked), this target's core, and a stub editor_imgui; it exists only on the editor platforms (macOS arm64, Windows x64 MSVC)"
  - "The CI package steps run last in the Windows job, after the mid-job cache saves, and delete zig-out/packages and the package stage so the zig-out upload stays the size it was"

requirements-completed: [CARRY-MINORS, D-06, D-15, SPEC-GAME-READS-IT, CARRY-PACKAGING]

coverage:
  - deliverable: "Status line right after every Test-in-game outcome (WINDOWS.md 1)"
    human_judgment: false
    verification:
      - {kind: unit, ref: "Sources/editor/app/panels_logic.zig#Test in game status: *", status: pass}
      - {kind: unit, ref: "Sources/editor/app/view_math.zig#StatusSlot: *", status: pass}
  - deliverable: "Restart does not report the terminated game"
    human_judgment: false
    verification:
      - {kind: unit, ref: "Sources/editor/app/panels_logic.zig#TestLaunchPrompt: Restart waits for the terminated game's exit*", status: pass}
      - {kind: unit, ref: "Sources/editor/app/testlaunch.zig#describe: the exits terminate() causes*", status: pass}
  - deliverable: "view.zig event-wiring tests incl. D-15 (WINDOWS.md 2)"
    human_judgment: false
    verification:
      - {kind: unit, ref: "Sources/editor/app/view.zig#view: *", status: pass}
      - {kind: ci, ref: "run 36558559070 Map editor unit tier (windows-platform, macos-platform)", status: pass}
  - deliverable: "Literal scroll-direction tests (WINDOWS.md 3)"
    human_judgment: false
    verification:
      - {kind: unit, ref: "Sources/editor/app/view_math.zig#a wheel notch up pans the map 20 px*", status: pass}
  - deliverable: "game-reads-it baseline"
    human_judgment: false
    verification:
      - {kind: tier, ref: "zig build map-editor-game-reads-it (zig-out/local-test/03-16-tiers.log)", status: pass}
  - deliverable: "Windows package built and checked in CI"
    human_judgment: false
    verification:
      - {kind: ci, ref: "run 36558559070 windows-platform 'Check the Windows package'", status: pass}

duration: 107min
completed: 2026-09-29
---

# Phase 3 Plan 16: Gap closure after the phase verification. Summary

**Test in game now leaves the status line right after every outcome, and Restart no longer reports the game it terminated. view.zig's SDL wiring is tested through a comptime input seam (19 tests, D-15 included). The direction tests assert literal positions, game-reads-it takes an in-game baseline, and CI builds and checks the Windows package, which found a Windows compile error in the package writer.**

## Performance

- **Duration:** about 107 min (2026-09-29T09:50:56Z to 11:38Z, most of it spent waiting for the two CI runs)
- **Tasks:** 6
- **Files modified:** 13 (1 created in the source tree)

## Accomplishments

- **Status line (WINDOWS.md 1), d421402a8.** `view_math.StatusSlot` holds one message and the source that set it (`general`, `test_launch`, `frame`, `dialog`), and `clearFrom(source)` erases only that source's message. `TestLaunchPrompt.noteLaunch(attempt, status)` maps every launch outcome:
  - a pre-start failure (no test path, the copy would not save, the log path too long) is shown as "test in game: ...";
  - a spawn failure goes to the modal and clears that line;
  - a start clears it.
  `panels.startTestGame` now reports through it. The same mechanism fixes two more stale messages: a lost frame (main.zig) is cleared by the next frame that presents, and "a dialog is already open" or "the file dialog failed" by that dialog's cancel. Tests: 4 in test-map-editor-panels and 3 StatusSlot tests.
- **Restart (D-06), f36c0dc97.** `gameExited` in the `restarting` state starts the new game and never reports that exit. testlaunch names the real shapes (`windows_terminate_exit_code` 1, SIGTERM 15, SIGKILL 9; `terminate()` uses the constant). The test feeds those four shapes instead of the old clean-exit fixture, checks that the next game's crash is still reported, and checks that a kept game's code-1 exit is still reported.
- **view.zig tests (WINDOWS.md 2), 294bcdf74.**
  - `View = ViewWith(SdlInput)`, and every bridge parameter is `anytype`. Modifiers, mouse, keyboard, focus and ImGui capture are read through `Input`.
  - The test module is `Sources/editor/app/view.zig` with the app's SDL translate-c module, this target's core and `testing/imgui_stub.zig`.
  - The 19 tests feed real `SDL_Event`s through `handleEvent` and `update`, against the core's fake bridge and a `FakeCamera`. They cover: brush stroke = one undo, Cmd+Z / Ctrl+Y / Cmd+Shift+Z, place, select-drag, Q/E with repeat ignored, Delete, the tool keys and a mid-stroke switch, the right button, the middle-button pan, wheel pan at zoom 0 and 1, Shift+wheel zoom at the pointer (including macOS's horizontal Shift-wheel), pinch and its reset, Home, arrow scrolling, edge scrolling, focus and capture gates, stale pan and stroke ends, panel hover, and D-15 (A panned and zoomed, then B, then A restored in the view and the engine, and again across closeMap).
  - A mutation check (the pan sign flipped) fails two of them.
  - test-map-editor-view depends on it on the editor platforms, and CI's windows-platform and macos-platform jobs gained a "Map editor unit tier" step. Before this, no CI job ran any test-map-editor-* step.
- **Literal direction tests (WINDOWS.md 3), 82a08a462.** A notch up from 2000,2000 must land at 1971.716,2028.284, and 0.1 s of up at 1858.579,2141.421. Down, left and right are checked too. view.zig's wheel test asserts 152.735,209.304 from the fixture's middle. The engine-tier check that screen up is world (-x,+y) was already in `TestWorldToScreenRoundTrip`.
- **game-reads-it baseline, c55b61f22.** The mode saves the unedited test copy and runs the game with `400:units=...,410:exit` at the same spot and radius. It then requires the edited run to reach at least the baseline plus 1 plus the squads' soldiers. `playerUnitsNear` moved to testlaunch.zig with tests (an unnamed player counts 0, a missing line gives null). Measured baseline: 0 (total 0 at 43,51 r 5), so the old precondition held but is now checked.
- **CI package, ff22c445c + 3ad03a687.** The windows-platform job now runs `package-game-editors` after the cache saves, then opens both zips:
  - Blitzkrieg-game.zip: 60,220 entries, 2.72 GB.
  - Blitzkrieg-game-with-editors.zip: 60,224 entries, 2.74 GB, Editors/ELK.exe, ExcelExporter.exe, MapEditor.exe, editor.exe.
  - Both have Game.exe and MapEditor.exe at the root, SeasonData/SeasonTextures.pak, and no top-level mods/.
  - The runner had C: 32 GB and D: 132 GB free. The two steps took about 3.5 min.
  The first run (36553658644) failed at compiling `tools/zig/package.zig` for Windows (see Deviations). 03-14-SUMMARY's two "verified via CI's windows-platform job" statements are corrected.

## Task Commits

1. Plan: `8efcf913d` docs(03-16)
2. Task 1, status line: `d421402a8` fix(03-16)
3. Task 2, Restart: `f36c0dc97` fix(03-16)
4. Task 3, view tests and the CI unit tier: `294bcdf74` test(03-16); literal direction tests: `82a08a462` test(03-16)
5. Task 4, baseline: `c55b61f22` test(03-16)
6. Task 5, CI package and the 03-14 correction: `ff22c445c` ci(03-16); the Windows compile fix: `3ad03a687` fix(03-16)
7. Task 6, ledger: `3e23e479e` docs(03-16)

## Verification

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false`: 32/32 steps, 315/315 tests (map-editor-view-test 46/46).
- `zig build test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto test-editor-core -Dtest-mode=run`: 20/20 steps, 277/277 tests.
- Six local tiers (`test-editor-bridge test-map-editor-engine map-editor-host-check map-editor-smoke map-editor-game-reads-it map-editor-auto`, aarch64-macos, `-Dcopy-data=false -Dtest-mode=run`), rc=0 (`zig-out/local-test/03-16-tiers.log`):
  - `editor-bridge: PASS`
  - `map-editor-engine: PASS (260 objects)`
  - 3x `host check PASS (metal, 1280x800)` and panel smoke PASS
  - `mod switch PASS`
  - `smoke PASS (52 steps, 260 objects ...)`
  - `game reads it PASS (14 units of player 0 near the placed unit, 0 before placing, plus the unit and the squads' 10 soldiers; ...)`
  - `BK_EDITOR_AUTO: done (13 actions)`
- `zig test tools/zig/build_hermeticity_test.zig`: 3/3. build.zig was not `zig fmt`-ed.
- **CI run 36558559070 at 3ad03a687: success on all six jobs.**

  | Job | Duration |
  |-----|----------|
  | windows-platform | 41m41s |
  | macos-platform | 19m34s |
  | linux-platform | 4m53s |
  | macos-intel-platform | 3m29s |
  | windows-mingw-platform | 1m38s |
  | linux-arm-platform | 1m02s |

  The Windows job is within its 110-minute budget.

## Deviations from Plan

**1. [Rule 1 - Bug] tools/zig/package.zig did not compile for Windows.** Found during Task 5 by the first CI run (36553658644, windows-platform "Package the game with the editors"). Zig 0.16's `std.Io.File.Permissions.readOnly()` refers to `windows.FILE_ATTRIBUTE_READONLY`, which its std does not have, so `package-game`/`package-game-editors` could never build on a Windows host. This was hidden because no job ever ran a package step. Fix: `unixMode` reads `toAttributes().READONLY` directly. It now cross-compiles for x86_64-windows-msvc and -gnu, and the old file reproduces the CI error locally. Files: tools/zig/package.zig. Commit 3ad03a687. Verified by CI run 36558559070.

**2. [Rule 2 - Missing critical] No CI job ran the map-editor unit steps.** Found during Task 3: test-map-editor-view, -panels, -testlaunch and -auto ran only in `zig build test`, which CI does not run. Added a "Map editor unit tier" step to the windows-platform and macos-platform jobs (294bcdf74). Both runs passed it, including the view test compiled for MSVC.

**3. [Scope] Two more stale-status paths fixed in Task 1**, as the plan allowed: the lost-frame message and the dialog-busy or dialog-failed message.

**Total deviations:** 1 bug auto-fixed, 1 missing coverage added, 1 in-scope extension. **Impact:** the Windows package can now actually be built. The orchestrator's real-Windows run of package-game on win-home would otherwise have failed at the same compile error.

## Issues Encountered

- The Windows job logs are not downloadable until the whole job finishes (the upload step runs about 8 min after the failing step).

## Next Phase Readiness

WINDOWS.md open_count is 0. The remaining human items in 03-VERIFICATION.md are unchanged except that D-15 is now covered by tests: the recovery offer at the next start, resize/hover visuals, the brush outline on hills, and Open Recent after a restart. The real-Windows run (packaged MapEditor.exe, F5 twice, Restart) is still with the orchestrator on win-home.

## Self-Check: PASSED

- The created files exist: Sources/editor/app/testing/imgui_stub.zig, 03-16-PLAN.md.
- `git log --oneline --grep="03-16"` shows 9 commits.
- Local suite and CI run 36558559070 are green, as above.

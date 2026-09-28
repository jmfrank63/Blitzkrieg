---
phase: 03-map-editor-plan-6-finish-m1
plan: 12
subsystem: testing
tags: [zig, sdl3, imgui, map-editor, automation, tga, ci]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 11)
    provides: the editor app, smoke.zig's fixed --smoke table, testlaunch.zig, the unknown-object host check
provides:
  - "BK_EDITOR_AUTO=\"frame:action,...\": a general-purpose scripting mechanism for the real editor loop, mirroring the game's own BK_AUTO_UI"
  - "a hand-rolled TGA parser and pixel-tolerance comparison (auto.zig), with a regenerable local baseline convention under zig-out/local-test"
  - "zig build map-editor-auto: the spec's editor-app scenario as one local command (paint, place, save, shot, compare, test-launch, wait, quit)"
  - "cursor isolation for synthetic-input runs (bk_imgui_backend_use_global_mouse)"
affects: [future map-editor plans that add BK_EDITOR_AUTO scenarios, CI/local-test tooling that wants a scripted editor run]

actuals:
  tokens: 18900
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "auto.zig: std-only schedule parser + TGA compare, matching the project's existing hand-rolled-TGA convention (no new image-diff dependency)"
    - "smoke.zig Driver union: one main.zig run() loop drives either the fixed --smoke table (Script) or BK_EDITOR_AUTO's parsed schedule (AutoRunner)"
    - "build-time cleanup tools are Zig artifacts run via addRunArtifact, never addSystemCommand - build_hermeticity_test.zig forbids a shell in build.zig's closure"

key-files:
  created:
    - Sources/editor/app/auto.zig
    - tools/zig/delete_matching_files.zig
  modified:
    - Sources/editor/app/smoke.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/panels.zig
    - build.zig
    - Sources/editor/imgui/imgui_backend.cpp
    - Sources/editor/imgui/imgui_backend.h

key-decisions:
  - "auto.zig's Action/Point/Wheel string fields borrow directly from the caller's BK_EDITOR_AUTO text rather than duplicating it - the same borrowing convention smoke.zig's own Input.save_as already uses, valid because the env var's text is deliberately leaked for the process's single-shot lifetime (main.zig)"
  - "waitgame= blocks the calling thread via testlaunch.Running.waitBlocking, which its own doc comment says an interactive loop must never do - accepted here because BK_EDITOR_AUTO's own loop is headless (--hidden) and draws no further frames while it waits, so nothing is watching"
  - "test-map-editor-auto's cleanup of Test in game's autoshot_*.rgba dump is a new host-built Zig tool (tools/zig/delete_matching_files.zig) run via addRunArtifact, not a shell command - an addSystemCommand(\"sh\"/\"cmd\") attempt failed build_hermeticity_test.zig's forbidden-executable audit locally before this was caught and fixed"
  - "panels.zig's mapIsOpen and requestTestLaunch made pub, plus a new State.test_extra_env field, so AutoRunner's test/waitgame handlers can drive Test in game directly - not in this plan's files_modified list, but the least invasive way to satisfy Zig's exhaustive switch over auto.zig's full Action set without duplicating Test in game's launch logic"

requirements-completed: [SPEC-AUTO, M1-EXIT-TIERS, CARRY-MINORS]

coverage:
  - id: D1
    description: "BK_EDITOR_AUTO parses a compact frame:action schedule (key, press/drag/release/click, wheel, open/save/saveas, test, waitgame, shot, compare, exit) and rejects malformed tokens, naming the offending entry"
    requirement: "SPEC-AUTO"
    verification:
      - kind: unit
        ref: "Sources/editor/app/auto.zig#parse: every action"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/auto.zig#parse: bad tokens are rejected, naming the entry"
        status: pass
    human_judgment: false
  - id: D2
    description: "compareTga does a pixel-tolerance BGRA comparison, indifferent to top-down vs bottom-up TGA descriptors"
    requirement: "SPEC-AUTO"
    verification:
      - kind: unit
        ref: "Sources/editor/app/auto.zig#compareTga: identical images differ nowhere"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/auto.zig#compareTga: a bottom-up descriptor of the same image as a top-down one compares equal"
        status: pass
    human_judgment: false
  - id: D3
    description: "zig build map-editor-auto runs the spec's editor-app smoke end to end: paint, place, save as, shot, compare (seed then compare), test-launch the game, wait for its clean exit, quit - one local command"
    requirement: "M1-EXIT-TIERS"
    verification:
      - kind: e2e
        ref: "zig build map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (run twice: first seeds reference/edited.tga, second compares at 0.1410% differing pixels and waits for the test game's exit 0)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Synthetic input stays isolated from the real cursor (plan 5 Task 7.1, carried) for both --smoke and BK_EDITOR_AUTO"
    requirement: "CARRY-MINORS"
    verification:
      - kind: e2e
        ref: "zig build map-editor-smoke -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (30 steps, 260 objects, PASS)"
        status: pass
    human_judgment: true
    rationale: "bk_imgui_backend_use_global_mouse narrows the exposure (disables the SDL3 backend's own capture mode and auto-capture hint) but cannot fully disable the backend's private MouseCanUseGlobalState fallback read - no public accessor exists in imgui_impl_sdl3.h. The smoke passing on this host is evidence, not proof the fallback never fires on every CI runner; a human/CI-signal check is the honest classification."

duration: 55min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 12: BK_EDITOR_AUTO and the editor-app tier Summary

**BK_EDITOR_AUTO generalises `--smoke`'s fixed table into a general scripting mechanism (`auto.zig`'s parser + a hand-rolled TGA comparison), and `zig build map-editor-auto` runs the spec's whole editor-app scenario - paint, place, save, shot, compare, test-launch, wait, quit - as one local command.**

## Performance

- **Duration:** 55 min
- **Started:** 2026-09-28T14:05:00Z (approx.)
- **Completed:** 2026-09-28T15:00:00Z (approx.)
- **Tasks:** 2
- **Files modified:** 8 (2 created, 6 modified)

## Accomplishments

- `Sources/editor/app/auto.zig`: a std-only parser for `BK_EDITOR_AUTO="frame:action,..."` (key/press/drag/release/click/wheel/open/save/saveas/test/waitgame/shot/compare/exit), plus a hand-rolled uncompressed-32-bit-TGA reader and `compareTga` pixel-tolerance diff - no new image-diff dependency, matching the project's existing TGA-by-hand convention.
- `smoke.zig` gained a `Driver` union and `AutoRunner`: main.zig's `run()` loop now drives either the fixed `--smoke` table or BK_EDITOR_AUTO's parsed schedule through the same synthetic-event machinery.
- `main.zig`: a new `--hidden` flag, and BK_EDITOR_AUTO read before settings/recovery load (the 03-07 rule: automated runs never touch either), driving the loop with `os_dialogs` off and exiting 2/0/1 depending on the parse and the schedule's own outcome.
- `build.zig`: `map-editor-auto` runs the spec's editor-app scenario on `coldwinter.bzm`; `test-map-editor-auto` runs `auto.zig`'s own tests on the host, folded into `test`.
- `compare=` seeds a local, never-committed reference on its first run and compares against it thereafter, logging the differing-pixel fraction either way; `test`/`waitgame=` drive and wait on a real `Test in game` launch, failing unless it exits 0 within the given time.
- Cursor isolation (`bk_imgui_backend_use_global_mouse`) for both `--smoke` and BK_EDITOR_AUTO, and `smoke.zig`'s `prepare()` now also checks `drag_via`/`drag_to` are free ground (plan 5 Task 7.1's carried item).

## Task Commits

Each task was committed atomically:

1. **Task 1: BK_EDITOR_AUTO drives the editor and writes a shot** - `6ae43ed1e` (feat)
2. **Task 2: Shot comparison, the spec's editor-app scenario with a test launch, and cursor isolation** - `ca47a8c19` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE/ROADMAP/REQUIREMENTS)

## Files Created/Modified

- `Sources/editor/app/auto.zig` - BK_EDITOR_AUTO's schedule parser, `Tga` reader and `compareTga` (new)
- `tools/zig/delete_matching_files.zig` - a small Zig artifact tool that deletes matching files in a directory, used to clean up Test in game's `autoshot_*.rgba` dump (new)
- `Sources/editor/app/smoke.zig` - `Driver` union, `AutoRunner` (delivers `auto.zig`'s schedule), `prepare()`'s `drag_via`/`drag_to` check
- `Sources/editor/app/main.zig` - `--hidden` flag, BK_EDITOR_AUTO wiring in `interactive()`, `run()`'s `driver` parameter, cursor isolation in `smokeRun`
- `Sources/editor/app/panels.zig` - `mapIsOpen`/`requestTestLaunch` made `pub`, new `State.test_extra_env` field threaded into `testlaunch.start`
- `build.zig` - `map-editor-auto` and `test-map-editor-auto` steps, the autoshot cleanup step
- `Sources/editor/imgui/imgui_backend.cpp`/`.h` - `bk_imgui_backend_use_global_mouse(bool)`

## Decisions Made

See `key-decisions` in the frontmatter above.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `panels.zig` needed changes not in this plan's `files_modified` list**
- **Found during:** Task 1 (writing `AutoRunner`'s `test`/`waitgame` handlers)
- **Issue:** Zig's exhaustive `switch` over `auto.zig`'s full `Action` set requires every variant to compile in Task 1, even though Task 1's own schedule never exercises `test`/`waitgame`. Driving Test in game from `AutoRunner` needed `panels.requestTestLaunch`/`panels.mapIsOpen` (both private) and a way to pass extra child-process environment (`BK_AUTO_UI`/`BK_NO_HELP`) that `panels.startTestGame`'s fixed `testlaunch.start` call did not expose.
- **Fix:** Made `mapIsOpen` and `requestTestLaunch` `pub`; added `State.test_extra_env: []const [2][]const u8 = &.{}`, threaded into `testlaunch.start`'s `extra_env` option. Empty by default - no behavior change for F5/the menu item.
- **Files modified:** `Sources/editor/app/panels.zig`
- **Verification:** `zig build test-map-editor-auto map-editor-auto` and `zig build map-editor-smoke` both pass (Task 1 and Task 2's own verify blocks).
- **Committed in:** `6ae43ed1e` (Task 1 commit)

**2. [Rule 3 - Blocking] `addSystemCommand("sh"/"cmd")` for the autoshot cleanup failed `build_hermeticity_test.zig`**
- **Found during:** Task 2 (the `map-editor-auto` step's own cleanup requirement: "removes `autoshot_*.rgba` ... afterwards")
- **Issue:** A first attempt used `b.addSystemCommand` with a shell (`sh -c`/`cmd /c`) to delete matching files. `zig test tools/zig/build_hermeticity_test.zig`'s own `test "current shader build closure is hermetic"` failed with `error.ForbiddenBuildProcess` - `build.zig` is in its audited closure, and the test forbids any of `addSystemCommand`/`std.process.run`/`std.process.Child`/`std.process.exec` appearing alongside a shell executable name.
- **Fix:** Wrote a small std-only Zig tool, `tools/zig/delete_matching_files.zig` (`<dir> <prefix> <suffix>`, best-effort, missing directory is not an error), compiled as a host executable and run via `b.addRunArtifact` - the pattern the hermeticity test's own `"accepts Zig artifact process"` test explicitly allows, matching existing tools like `tools/zig/compare_trees.zig`.
- **Files modified:** `build.zig`, new `tools/zig/delete_matching_files.zig`
- **Verification:** `zig test tools/zig/build_hermeticity_test.zig` passes (3/3); `zig build map-editor-auto` removed the one `autoshot_*.rgba` file the test game wrote.
- **Committed in:** `ca47a8c19` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking issues found while making the plan's own two tasks compile/pass their own verify blocks).
**Impact on plan:** No scope creep - both fixes were necessary for Task 1/2's own stated deliverables to build and pass; neither changes existing behavior for anything but the new BK_EDITOR_AUTO/cleanup paths.

## Issues Encountered

- A comment I wrote in `build.zig` (documenting why the cleanup step avoids a shell) itself contained the literal words "sh"/"cmd" as whole tokens, which `build_hermeticity_test.zig`'s audit also flags (it scans the whole file's text, not just executable calls) once any process-creation marker is present anywhere in the file (the pre-existing, legitimate `addSystemCommand` calls at build.zig:2110/2281 already satisfy that). Reworded the comment to avoid the literal tokens; resolved before committing, not a design change.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- M1's editor-app exit criterion ("the editor app tier passes locally on macOS arm64") is met: `zig build map-editor-auto` runs the full spec scenario end to end, twice (seed then compare), plus `test-map-editor-auto` and `map-editor-smoke`, all passing on this host.
- `map-editor-auto`/`test-map-editor-auto` are local-only by design (not wired into CI's GPU-runner gate) - the Windows leg has not been exercised for this specific scenario; the `test`/`waitgame`/cursor-isolation code paths are new and worth a Windows pass before relying on them there.
- `bk_imgui_backend_use_global_mouse`'s own limitation (no public switch for the SDL3 backend's `MouseCanUseGlobalState` fallback) is documented in its doc comment and in this SUMMARY's D4 coverage entry - a future plan wanting stronger cursor isolation would need to patch the vendored backend directly, which this plan deliberately did not do.
- This closes plan 5's "Carried to plan 6" BK_EDITOR_AUTO/shot-comparison item and Task 7.1's cursor-isolation/drag_via/drag_to items.

## Self-Check: PASSED

- `Sources/editor/app/auto.zig` exists: FOUND
- `tools/zig/delete_matching_files.zig` exists: FOUND
- Commit `6ae43ed1e` exists in history: FOUND
- Commit `ca47a8c19` exists in history: FOUND
- `plan_head_before`: `b9cb90f4939975ea910c6ff1c3655ec5c45c5973`; `git rev-list --count b9cb90f49..HEAD` = 2 (matches `actuals.commits`)

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

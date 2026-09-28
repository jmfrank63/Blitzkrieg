---
phase: 03-map-editor-plan-6-finish-m1
plan: 04
subsystem: map-editor
tags: [zig, imgui, sdl3, unsaved-changes, shipped-maps]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 03)
    provides: "Editor.save's safe-save contract (temp write, bridge read-back verify, one .bak per session, atomic swap) that this plan's Save/Save As redirect calls into unchanged"
provides:
  - "panels_logic.zig: Pending, UnsavedPrompt (guard/answer/saveFinished) - a pure, window-free state machine for D-23's unsaved-changes prompt"
  - "PathSlot.Result gains .cancelled: a dialog cancel is now a result, not silence"
  - "isShippedMap, needsSaveAs, defaultMapsFolder (D-17, D-18, D-28) and formatTitle's read_only suffix"
  - "main.zig: SDL_EVENT_QUIT/WINDOW_CLOSE_REQUESTED route through the same guard as the menu's Quit"
  - "smoke.zig: .window_close/.answer/.save_requested inputs and unsaved_prompt_open/prompt_cancelled/save_became_save_as expects"
affects: [03-07-autosave-and-settings, 03-08-mod-menu]

# Actuals (#2632)
actuals:
  tokens: 11523
  tasks: 2
  commits: 2
plan_head_before: 2448020616ec9f61a954fb5900ec87e7647638ef

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "UnsavedPrompt is a pure state machine (guard/answer/saveFinished) that never touches the bridge or the filesystem itself - the caller (panels.zig's act()) makes the actual save and reports the outcome back through noteSaveOutcome/saveFinished, so the prompt's own tests need no fake bridge at all."
    - "FileActions.next(dirty, needs_save_as) is the single place that turns every guarded action (Open, Quit, the modal's own Save/Save As) into a Step; a resume_pending field lets a save's outcome (arrived asynchronously through the dialog slot or synchronously through a plain Save) re-enter the same Step machinery on the next call."
    - "PathSlot's cancel is now a real Result (.cancelled), matching .arrived and .failed, instead of silently going back to idle - the one behavioral change to a already-shipped primitive plan 5 built."

key-files:
  created: []
  modified:
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/smoke.zig

key-decisions:
  - "The unsaved-changes prompt's button clicks (drawUnsavedPrompt) never call editor.save directly; they set answer_pending, and the very next act() call (same frame, since draw() runs before act() in main.zig's run loop) turns it into a Step through the same next()/act() machinery Open/Save/Quit already use - one code path for every save, prompted or not."
  - "A cancelled dialog is now always a Step (.dialog_cancelled), even outside the prompt - previously silent. act() treats it as a no-op for a plain cancel, and only feeds it to the prompt's saveFinished(false) when dialog_for_prompt marks it as the prompt's own Save As."
  - "isShippedMap treats a relative document path as already under base_root (the editor and the shipped maps both run from the installation directory) rather than trying to resolve it against a cwd - matching how the smoke's own map argument (\"Data\\\\Maps\\\\...\") and every real Open of a shipped map arrive."

patterns-established:
  - "A guarded action (Open, Quit) is represented as a Pending value the prompt carries opaquely until answer()/saveFinished() release it - Pending already has open_path/switch_mod variants for 03-07's Open Recent and 03-08's Mod switch to guard through the same prompt without changing its shape."

requirements-completed: [D-17, D-18, D-23, SPEC-SAVE-ERRORS]

coverage:
  - id: D1
    description: "With unsaved changes, Open, Quit and closing the window ask Save / Don't save / Cancel; Cancel keeps the map and its changes; Don't save goes ahead; Save saves first and goes ahead only if the save succeeded (D-23)"
    requirement: "D-23"
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - the eight UnsavedPrompt transition tests, plus two FileActions integration tests (a dirty Quit through Save, a dirty Open through a cancelled Save As)"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke - \"closing the window asks\" (unsaved_prompt_open), \"Cancel keeps it\" (prompt_cancelled), \"Ctrl+Z undoes the stroke\" (all_undone)"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to drive the modal by hand (close, Cancel, close again, Don't save; File > Open while dirty) - the automated coverage proves the state machine and the loop wiring, not the modal's on-screen feel."
  - id: D2
    description: "Save on a map with no path or a shipped map (under the installation's Data or a mod's data) opens Save As instead; a shipped map is never overwritten (D-18)"
    requirement: "D-18"
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - isShippedMap and needsSaveAs tests (relative/absolute, mixed case/separators, mods/*/data, an empty path)"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke - \"Save on a shipped map becomes Save As\" (save_became_save_as): the dialog slot waits for a save_as instead of act() writing the shipped file directly"
        status: pass
    human_judgment: true
    rationale: "The task's <human-check> asks Johannes to confirm the shipped Data file's modification time is unchanged after Save - the smoke proves the redirect happens, not that the real file survives a real click."
  - id: D3
    description: "Open and Save As dialogs start in the user maps folder <UserRoot>/maps, created if missing (D-17)"
    requirement: "D-17"
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - defaultMapsFolder tests (with and without a mod, a bad mod folder name refused)"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke map-editor-host-check - showDialog's real SDL_Show*FileDialog call ran during the \"Save on a shipped map becomes Save As\" step with defaultMapsFolder's folder created first, with no hang or crash on this host"
        status: pass
    human_judgment: true
    rationale: "The task's <human-check> asks Johannes to confirm the real platform path (~/.local/share/Nival/Blitzkrieg/maps on macOS, %APPDATA%\\Nival\\Blitzkrieg\\maps on Windows) is what the OS dialog actually opens to - not observable from the smoke alone."
  - id: D4
    description: "A shipped map's window title says (read-only)"
    requirement: "D-18"
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - \"the title marks a shipped map read-only, dirty or not\""
        status: pass
    human_judgment: false

# Metrics
duration: ~25min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 04: Unsaved-changes prompt and shipped-map protection Summary

**A pure UnsavedPrompt state machine guards Open/Quit/window-close behind Save/Don't save/Cancel (D-23); Save on a shipped map redirects to Save As instead of writing over it (D-18), and Open/Save As now default to the user's own maps folder (D-17).**

## Performance

- **Duration:** ~25 min (approximate - not captured at session start; derived from the prior plan's completion timestamp and this plan's final commit)
- **Started:** approx. 2026-09-28T04:38:48Z
- **Completed:** 2026-09-28T05:03:34Z
- **Tasks:** 2 completed
- **Files modified:** 4

## Accomplishments

- `panels_logic.zig`: `PathSlot`'s cancel is now a `Result.cancelled` instead of silently returning to idle. New `Pending` (`open_dialog`, `open_path: PathText`, `quit`, `switch_mod: NameText` - the latter two unused until 03-07/03-08) and `UnsavedPrompt` (`guard`, `answer`, `saveFinished`, `isAsking`) as a pure state machine that never touches the bridge or filesystem - the caller makes the actual save and reports back. `FileActions` gained `prompt`, `answer_pending`, `dialog_for_prompt`, `resume_pending` and a `next(dirty, needs_save_as)` that routes Open and Quit through the guard, turns a cancelled dialog into `.dialog_cancelled`, and resumes a guarded action once a prompted save lands.
- `panels.zig`: a new "Unsaved changes" modal (Save/Don't save/Cancel) drawn while the prompt asks; `act()` reports every save's outcome back to the prompt via `noteSaveOutcome`. `showDialog` now creates the user maps folder (or the active mod's) and passes it as SDL's `default_location` for both Open and Save As (D-17). `updateTitle` marks a shipped map's title `(read-only)` (D-18's title truth).
- `panels_logic.zig` also gained `isShippedMap` (separators/case normalised, a relative path taken as already under `base_root`, shipped under `<base>Data/` or `<base>mods/<any>/data/`), `needsSaveAs` (an empty path or a shipped one), and `defaultMapsFolder` (`<user_root>maps` or `<user_root>mods/<name>/maps`, refusing an unsafe mod folder name). `act()`'s Save redirect and the prompt's `needs_save_as` argument both use the real check now.
- `main.zig`: `SDL_EVENT_QUIT` and `SDL_EVENT_WINDOW_CLOSE_REQUESTED` set `state.actions.quit_requested` instead of ending the loop directly - the loop now ends only once `act()` says so, after the guard has had its say.
- `smoke.zig`: new `.window_close`/`.answer`/`.save_requested` inputs and `unsaved_prompt_open`/`prompt_cancelled`/`save_became_save_as` expects. After "the saved map opens again": a fresh brush stroke, a window close that asks, Cancel that keeps the dirty map at the same path, and Ctrl+Z undoing it cleanly. Before "Save As writes the map": a step proving Save on the smoke's own shipped map (`Data\Maps\Multiplayer\coldwinter.bzm`) redirects to Save As - the dialog slot waits for one instead of `act()` writing the shipped file - then cancels and drains it so the next real Save As step starts clean.

## Task Commits

Each task was committed atomically:

1. **Task 1: The unsaved-changes prompt on Open, Quit and window close** - `0708e4bfb` (feat)
2. **Task 2: Shipped maps are read-only; Open and Save As start in the user maps folder** - `975bffee9` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

## Files Created/Modified

- `Sources/editor/app/panels_logic.zig` - `PathSlot.Result.cancelled`, `Pending`, `PathText`/`NameText`, `UnsavedPrompt`, `FileActions` rewritten around `next(dirty, needs_save_as)`, `isShippedMap`, `needsSaveAs`, `defaultMapsFolder`, `formatTitle(..., read_only)`
- `Sources/editor/app/panels.zig` - the "Unsaved changes" modal, `act()`'s save-outcome reporting, `showDialog`'s `default_location`, `updateTitle`'s read-only suffix
- `Sources/editor/app/main.zig` - quit/close route through `quit_requested` instead of ending the loop directly
- `Sources/editor/app/smoke.zig` - new inputs/expects and script steps for the prompt and the shipped-map Save redirect

## Decisions Made

- The modal's buttons never call `editor.save` directly; they set `answer_pending`, and the same frame's `act()` call (which always runs right after `draw()`) turns it into a Step through the existing `next()`/`act()` machinery - one code path for every save, prompted or not.
- A cancelled dialog is now always a Step (`.dialog_cancelled`), even outside the prompt flow - previously silent. `act()` treats a plain cancel as a no-op; only a prompt-owned Save As cancel (tracked by `dialog_for_prompt`) reports through `saveFinished(false)`.
- `isShippedMap` takes a relative document path as already under `base_root` rather than resolving it against a cwd, matching how the smoke's own map argument and every real Open of a shipped map actually arrive (relative, backslash-separated, run from the installation directory).

## Deviations from Plan

None - plan executed exactly as written. The smoke's new "brush stroke" and shipped-map Save steps each needed one extra scripted key-press (switching back to the brush tool, which the earlier "key 1 chooses the selector" step had left active) that the plan's prose did not spell out task-by-task; this is normal test-script plumbing, not a functional deviation.

## Issues Encountered

- The first attempt at the new "Save on a shipped map becomes Save As" smoke step left the dialog slot in `.cancelled` rather than `.idle` after `deliver(null)` (the slot only returns to idle once `take()` drains the result) - the very next step's real Save As then failed with "the dialog slot was busy". Fixed by draining with `take()` right after the manual cancel, verified by a clean re-run.
- Confirmed on this host: `showDialog`'s real `SDL_ShowSaveFileDialog` call (triggered for the first time ever by the smoke, via the shipped-map Save redirect) returns without hanging or crashing under a hidden host, and the smoke's own manual `deliver(null)`+`take()` beats whatever the real dialog eventually does, so no window is left open. `defaultMapsFolder` + `createDirPath` also ran for real during this step, creating an (empty) `maps` folder under this host's real `~/.local/share/Nival/Blitzkrieg/` - alongside the `saves`/`logs`/`cache`/`screenshots` folders the engine's `Paths::Initialize()` already creates there on every host-check/smoke run (pre-existing behavior, not introduced by this plan).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `Pending`'s `open_path`/`switch_mod` variants and `FileActions.stepForPending`'s corresponding `.none` arms are ready for 03-07 (Open Recent) and 03-08 (Mod switch) to guard their own actions through the same `UnsavedPrompt` without changing its shape.
- `state.mod_folder` already threads into `defaultMapsFolder` (`showDialog`); 03-08 only needs to set it from the Mod menu for D-28's `mods/<Name>/maps` default to take effect.
- No blockers for the rest of phase 3.

## Self-Check: PASSED

- `[ -f Sources/editor/app/panels_logic.zig ]`, `[ -f Sources/editor/app/panels.zig ]`, `[ -f Sources/editor/app/main.zig ]`, `[ -f Sources/editor/app/smoke.zig ]` - all FOUND.
- `git log --oneline --all --grep="03-04"` returns 2 commits (`0708e4bfb`, `975bffee9`).
- All task `<acceptance_criteria>` re-verified: `panels_logic.zig` has `UnsavedPrompt`/`Pending` with the eight transition tests (plus two FileActions integration tests); `grep -n "quit_requested = true" Sources/editor/app/main.zig` finds the quit/close handler; `grep -n "isShippedMap\|defaultMapsFolder\|needsSaveAs" Sources/editor/app/panels_logic.zig` finds all three plus their tests; `panels.zig` passes a non-null `default_location` to both SDL file dialogs - all PASS.
- Plan-level `<verification>`: `zig build test-map-editor-panels` (rc=0, both tasks re-run after Task 2's changes), `zig build map-editor-smoke map-editor-host-check` (rc=0, "map-editor: smoke PASS (25 steps, 260 objects, ...)", "map-editor: host check PASS (metal, 1280x800)", "map-editor: panel smoke PASS (...)") - all PASS on the final committed state.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

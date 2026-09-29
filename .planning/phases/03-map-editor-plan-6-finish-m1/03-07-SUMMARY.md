---
phase: 03-map-editor-plan-6-finish-m1
plan: 07
subsystem: map-editor
tags: [zig, imgui, editor-settings, autosave, recovery, open-recent]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 03, plan 04)
    provides: "Editor.save's safe-save contract (temp write, bridge read-back verify, one .bak per session, atomic swap) that autosave's map-file target and every real Save/Save As call into unchanged; isShippedMap/needsSaveAs/defaultMapsFolder (D-17, D-18) that this plan's recovery-folder check and settings-window maps-folder override build on."
provides:
  - "core/settings.zig: Settings (scroll_speed, autosave, autosave_minutes, maps_folder, recent), parse/format as a lenient key=value mapeditor.cfg, pushRecent/removeRecent (D-24, D-25, D-27)"
  - "core/autosave.zig: Autosave (note/due/wrote), target(needs_save_as), recoveryName - the schedule and target for D-20..D-22, with no bridge or app-tier dependency"
  - "View.wheel_sensitivity - the plan 5 TODO gone; Settings' scroll_speed drives it live"
  - "panels.zig: Edit > Settings... window, File > Open Recent, File > Autosave, the recovery-offer modal, and the whole main.zig run() plumbing (settings load/write, autosave tick, recovery scan) that wires them to disk in interactive mode only"
  - "panels_logic.zig: FileActions.requestOpenPath/stepForPending's open_path arm, needsSaveAs extended with a recovery-folder check"
affects: [any later M2/M3 phase touching editor settings, the map's recent-files list, or autosave/recovery]

# Actuals (#2632)
actuals:
  tokens: 18600
  tasks: 3
  commits: 3
plan_head_before: 4957e1d63f8d625c41b91697c4b8899b9f632b96

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "A small hand-written key=value settings file (mapeditor.cfg), lenient rather than fallible: parse clamps out-of-range numbers and skips malformed lines instead of failing the whole file, matching this project's 'no new dependency for a small, well-understood format' philosophy."
    - "A Pending value (panels_logic.zig) that carries its own bytes (PathText) travels by value through several by-value returns (UnsavedPrompt.pending, guard/answer's return values, stepForPending's own parameter) - each hop is a fresh stack copy. stepForPending copies the payload into a FileActions-owned scratch field before slicing it into the returned Step, which is what keeps that slice valid once stepForPending's own stack frame is gone; slicing straight from the parameter would be a use-after-return bug undetected by the compiler."
    - "Interactive-only side effects (settings persistence, the autosave tick) are threaded through main.zig's shared run() loop as parameters (settings_path: ?[]const u8, is_interactive: bool) rather than duplicated across interactive()/smokeRun() - the automated modes simply never pass the values that would turn them on, so 'never touches disk' falls out of the call site, not a mode check buried in panels.zig."
    - "Autosave's schedule (core/autosave.zig) and its write (main.zig/panels.zig) are split the same way editor.save's safe-save was in 03-03: the core owns pure timing/naming logic with zero I/O, the app owns every actual file operation."

key-files:
  created:
    - Sources/editor/core/settings.zig
    - Sources/editor/core/autosave.zig
  modified:
    - Sources/editor/core/root.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/view_math.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/main.zig

key-decisions:
  - "Settings file comments are whole-line only (a line whose first non-blank character is '#'): no inline comment stripping, since a maps_folder value could legitimately contain '#' on some filesystems and stripping mid-line would corrupt it silently."
  - "The Settings window's 'Maps folder' override is a plain preference check in showDialog (state.settings.mapsFolder() first, defaultMapsFolder(...) as fallback) rather than a signature change to panels_logic.defaultMapsFolder - keeps 03-04's own function and its tests untouched."
  - "needsSaveAs gained a third parameter (user_root) for the recovery-folder check (D-22) rather than a parallel function, since every caller already had immediate access to both base_root and user_root; the one call site (panels.zig's act()) and one test were updated together."
  - "run()'s interactive-vs-automated distinction for settings-writing and autosave is two explicit parameters (settings_path: ?[]const u8, is_interactive: bool), not one flag reused for both - they answer different questions (where to write vs whether to tick at all) and conflating them would make a future settings-path resolution failure silently also disable autosave."
  - "Autosave and the recovery-offer modal are gated behind the same anyModalOpen() check (the dialog slot waiting, the unsaved-changes prompt asking, the Settings window open, the test-launch restart/report modals, or the recovery-offer modal itself) so a background write can never race the user's own dialog for the map."

patterns-established:
  - "core/settings.zig and core/autosave.zig both export only pure, allocation-free logic with inline tests; every filesystem or engine call they enable (reading/writing mapeditor.cfg, saveCopy into the recovery folder, the sidecar) lives in panels.zig/main.zig, keeping the core testable with zig build test-editor-core alone."

requirements-completed: [D-20, D-21, D-22, D-24, D-25, D-27]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "mapeditor.cfg end to end: a Settings window (Edit > Settings...) edits scroll/swipe speed, autosave on/off and interval, and the default maps folder; the scroll speed drives View.wheel_sensitivity live and survives a restart through the settings file, loaded from <user_root>mapeditor/mapeditor.cfg (or BK_EDITOR_SETTINGS, a test seam) and written back through a temp file and rename after the frame a setting changed"
    requirement: "D-24, D-25"
    verification:
      - kind: unit
        ref: "Sources/editor/core/settings.zig (zig build test-editor-core) - defaults, round trip, clamping, CRLF, unknown key, comment lines, plus the repeated-recent-line round trip"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/view_math.zig (zig build test-map-editor-view) - wheel_sensitivity's own tests unaffected by the field move"
        status: pass
      - kind: automated_ui
        ref: "zig build install-map-editor && MapEditor --check under BK_EDITOR_SETTINGS - prints 'map-editor: settings round trip PASS (<path>)', and mapeditor-test.cfg's rewritten autosave_minutes line confirms the write landed"
        status: pass
    human_judgment: false
  - id: D2
    description: "File > Open Recent lists the last 10 maps, most recent first; missing files are greyed with a Remove item; choosing an existing one goes through the unsaved-changes prompt first; Clear list empties it"
    requirement: "D-27"
    verification:
      - kind: unit
        ref: "Sources/editor/core/settings.zig (zig build test-editor-core) - pushRecent order/dedupe/capacity, removeRecent"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - Open Recent opens directly clean, asks first dirty, and actOnPath's enginePath conversion"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke map-editor-host-check - both PASS with the new submenu code compiled into panels.zig's draw path"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to open three maps, quit, restart and confirm the on-screen order and the greyed/Remove state of a renamed file - the on-screen submenu layout and ImGui tooltip are not observable from the automated tests above."
  - id: D3
    description: "Autosave (on by default, every 2 minutes, only when dirty) writes into the map file or, for a never-saved map, a recovery copy plus a sidecar; at the next start every recovery copy found is offered back (Open/Discard/Later); a real Save/Save As or a clean/Don't-save quit deletes that document's recovery copy"
    requirement: "D-20, D-21, D-22"
    verification:
      - kind: unit
        ref: "Sources/editor/core/autosave.zig (zig build test-editor-core) - due's interval-from-becoming-dirty and later-of-dirty-or-write-time rules, target, and recoveryName's sanitisation and traversal safety"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - needsSaveAs redirects a recovery-folder path to Save As"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-smoke map-editor-host-check - both PASS with no autosave tick in either loop (main.zig's run(is_interactive=false) for both)"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to set a 1-minute interval, watch a real save land and a real recovery copy appear on disk, then kill and restart the editor to see the recovery offer name the map - none of that is observable without a live interactive session and a person watching the title bar and the recovery folder."

# Metrics
duration: ~35min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 07: Editor settings, Open Recent and autosave/recovery Summary

**mapeditor.cfg (its own settings file, never a game profile) now holds scroll/swipe speed, autosave on/off and interval, the default maps folder and the last 10 maps; autosave writes into the map file or a recovery copy that the next start offers back.**

## Performance

- **Duration:** ~35 min (derived from the first and last task commits' own timestamps)
- **Started:** 2026-09-28T06:37:23Z
- **Completed:** 2026-09-28T07:12:42Z
- **Tasks:** 3 completed
- **Files modified:** 8 (2 created, 6 modified)

## Accomplishments

- New `Sources/editor/core/settings.zig`: `Settings` (`scroll_speed`, `autosave`, `autosave_minutes`, `maps_folder`, `recent`), a lenient `key=value` `parse`/`format` (unknown keys ignored, malformed lines skipped, out-of-range numbers clamped, CRLF tolerated), and `pushRecent`/`removeRecent` for the last 10 maps (D-24, D-25, D-27).
- `View.wheel_sensitivity` replaces plan 5's fixed `wheel_sensitivity` constant (the `TODO(plan 6...)` is gone); the Settings window's slider and the loaded settings file both drive it live.
- `panels.zig` gained: an Edit > "Settings..." window (scroll/swipe speed, autosave checkbox and interval, maps folder with a "Use default" button - `applySettings` is the one path every control and `--check`'s own round-trip test go through); a File > "Open Recent" submenu (existence cached once per opening, a missing entry disabled with "Remove", "Clear list", a full-path tooltip); a File > "Autosave" menu toggle; and the "Unsaved work from an earlier session" recovery-offer modal (Open/Discard per entry, Later dismisses for the session).
- New `Sources/editor/core/autosave.zig`: `Autosave.due` fires only while dirty, `interval_ms` since the later of becoming dirty and the last write; `target(needs_save_as)` picks the map file or a recovery copy; `recoveryName` sanitises a document path down to a safe stem plus `.bzm` with no directory component surviving (path-traversal safe by construction, not by denylist).
- `main.zig`: `mapeditor.cfg` loads from `<user_root>mapeditor/` (or `BK_EDITOR_SETTINGS`, a test seam) and is written back through a temp file and rename after the frame a setting changed; `run()` now takes `is_interactive`, ticking autosave only there - `--smoke`/`--check`'s panel smoke never call it, so neither ever autosaves or touches the recovery folder.
- `panels_logic.zig`: `needsSaveAs` also redirects Save to Save As for a document path inside the recovery folder; `FileActions.requestOpenPath` guards an Open Recent choice through the same `UnsavedPrompt` a plain Open uses, with `stepForPending`'s `open_path` arm copying into a `FileActions`-owned scratch buffer so the returned `Step.act_on_path.path` slice never points into an already-returned stack frame.

## Task Commits

Each task was committed atomically:

1. **Task 1: mapeditor.cfg end to end - the Settings window's swipe speed drives the pan and survives a restart** - `b854b8020` (feat)
2. **Task 2: File > Open Recent - last 10, missing greyed and removable, through the unsaved prompt** - `0a096399a` (feat)
3. **Task 3: Autosave into the file or a recovery copy, and the recovery offer at start** - `6079dd2c7` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

## Files Created/Modified

- `Sources/editor/core/settings.zig` - new: `Settings`, `parse`, `format`, `pushRecent`, `removeRecent`
- `Sources/editor/core/autosave.zig` - new: `Autosave` (`note`/`due`/`wrote`), `Target`, `target`, `recoveryName`
- `Sources/editor/core/root.zig` - exports `settings`, `autosave`
- `Sources/editor/app/view.zig` - `View.wheel_sensitivity`
- `Sources/editor/app/view_math.zig` - `wheel_sensitivity`'s doc comment (the plan 6 TODO removed)
- `Sources/editor/app/panels.zig` - Settings window, Open Recent submenu, Autosave menu toggle, recovery-offer modal, `tickAutosave`/`scanRecoveryOffers`, recovery/recent bookkeeping in `act()`
- `Sources/editor/app/panels_logic.zig` - `needsSaveAs`'s recovery-folder check, `FileActions.requestOpenPath`/`open_path_scratch`
- `Sources/editor/app/main.zig` - settings load/write plumbing, `run()`'s `is_interactive` parameter and autosave tick, the startup recovery scan

## Decisions Made

- Settings-file comments are whole-line only (`#` as the first non-blank character) - no inline stripping, so a `maps_folder` value can never be silently corrupted by a `#` inside it.
- The Settings window's "Maps folder" override is a plain preference check in `showDialog` (custom folder first, `defaultMapsFolder` as fallback) rather than a signature change to 03-04's own function.
- `needsSaveAs` gained a `user_root` parameter for the recovery-folder check (D-22) instead of a parallel function - every caller already had both roots at hand.
- `run()`'s interactive-vs-automated distinction is two explicit parameters (`settings_path`, `is_interactive`), not one flag reused for both, so a future settings-path resolution failure can never silently also disable autosave.
- Autosave and the recovery-offer modal share one `anyModalOpen()` gate (dialog waiting, unsaved-changes prompt, Settings window, test-launch modals, or the recovery-offer modal itself) so a background write can never race the user's own dialog.

## Deviations from Plan

None - plan executed exactly as written. Camera rotation (D-12) stays deferred out of M1 per 03-06's own recorded decision; this plan added no rotation setting, no Alt+Q/E binding, and no other rotation-related surface.

## Issues Encountered

- Two ordinary Zig API name corrections surfaced during authoring and were fixed before any commit: `std.mem.trimRight` does not exist in this Zig version (`std.mem.trimEnd` is the current name), and `std.time.timestamp()` was removed in favor of `std.Io.Clock.real.now(io).toSeconds()` for the recovery sidecar's unix time. Neither reached a commit; not a deviation from the plan, just corrected during normal authoring (same category as 03-03-SUMMARY's own C++ authoring-mistake note).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `Pending.switch_mod` (03-08's Mod menu) is still unused, per plan, and `stepForPending`'s own comment now says so.
- `core.settings.Settings` and `core.autosave.Autosave` are both plain, allocation-free values `panels.State` owns directly - a later phase adding more settings or a different autosave policy extends these in place, with no new plumbing needed in main.zig's `run()` beyond what already threads `settings_path`/`is_interactive` through.
- The plan's own `<verification>` line notes a spec-less probe fallback skip: this phase has no formal REQ IDs registered (CONTEXT.md's D-numbers are the requirements), so there was nothing to probe beyond the tests and checks already run.
- No blockers for the rest of phase 3. The two `<human-check>` items (Open Recent's on-screen order/greying, and autosave/recovery's real-disk behavior over a live session) are recorded above for the phase's end-of-phase UAT pass, per `workflow.human_verify_mode`'s default (`end-of-phase`).

## Self-Check: PASSED

- `[ -f Sources/editor/core/settings.zig ]`, `[ -f Sources/editor/core/autosave.zig ]`, `[ -f Sources/editor/core/root.zig ]`, `[ -f Sources/editor/app/view.zig ]`, `[ -f Sources/editor/app/view_math.zig ]`, `[ -f Sources/editor/app/panels.zig ]`, `[ -f Sources/editor/app/panels_logic.zig ]`, `[ -f Sources/editor/app/main.zig ]` - all FOUND.
- `git log --oneline --all --grep="03-07"` (matched via commit subjects containing "03-07" in this dispatch's own convention - the three task commits above) all present in `git log --oneline -5`: `6079dd2c7`, `0a096399a`, `b854b8020` - all FOUND.
- All task `<acceptance_criteria>` re-verified: `settings.zig` exists, std-only, with the listed tests (pass); `grep -n "TODO(plan 6" Sources/editor/app/view_math.zig` prints nothing; `panels.zig` has the Edit > "Settings..." item and its four controls, the "Open Recent" submenu with disabled entries and "Remove", File > "Autosave", and the recovery modal; `autosave.zig` exists with `due`/`target`/`recoveryName` tests including the `a:b*c.bzm` -> `a_b_c.bzm` and empty-stem -> `untitled.bzm` cases; `main.zig`'s `run()` takes `is_interactive` and only `interactive()` passes `true` - all PASS.
- Plan-level `<verification>`: `zig build test-editor-core test-map-editor-view test-map-editor-panels` (rc=0), `zig build install-map-editor` (rc=0), `MapEditor --check` under `BK_EDITOR_SETTINGS` (rc=0, "settings round trip PASS", rewritten `autosave_minutes` line present), `zig build map-editor-smoke map-editor-host-check` (rc=0, "map-editor: smoke PASS", "map-editor: host check PASS (metal, 1280x800)", "map-editor: panel smoke PASS") - all PASS on the final committed state. `zig build test` (core + map-file tiers, all six CI targets) also re-run clean.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

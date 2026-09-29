---
phase: 03-map-editor-plan-6-finish-m1
plan: 03
subsystem: map-editor
tags: [zig, std.Io.Dir, editor-bridge, safe-save, atomic-write]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 01, plan 02)
    provides: "Game -editor-test switch and the test-launch spawn/lifecycle plan 03 builds no further on, but shares the worktree and build graph with"
provides:
  - "Sources/editor/core/files.zig: Files interface, StdFiles over std.Io.Dir, FakeFiles, osPathFromEngine, tempPathFor, backupPathFor"
  - "Editor.save as the spec's safe save: temp file, bridge read-back verification, one .bak per file per session, atomic swap, no unsafe write ever reaches the user's map"
  - "SaveSessionMap read-back + AreEquivalent verification inside BkEditorSaveMap - every caller of the bridge's save, not only the editor, now gets this guarantee"
  - "Confirmed on this host: std.Io.Dir.rename replaces an existing target with no extra flag (research A2, both formats)"
affects: [03-07-autosave, 03-04, 03-08]

# Actuals (#2632)
actuals:
  tokens: 12200
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "A Files interface (ptr+vtable, the Bridge's own shape) lets StdFiles (std.Io.Dir) and FakeFiles (in-memory, op-logged) back Editor.save identically, matching the existing Bridge/FakeBridge split."
    - "Safe save as temp-write + read-back-verify (inside the bridge) + one-time backup + atomic rename, all gated on Editor.files being set - a mode with no filesystem refuses to save rather than writing unsafely."
    - "A shared FakeFiles pointer on FakeBridge lets a fake bridge's saveMap and the core's Files interface agree on the same fake filesystem, so a core test can see a whole temp-then-swap dance land in one place with no real disk."

key-files:
  created:
    - Sources/editor/core/files.zig
  modified:
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/root.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/bridge.h
    - tools/zig/editor_bridge_test.cpp
    - Sources/editor/app/smoke.zig

key-decisions:
  - "The read-back verification (D-19's 'the bridge reads its own write back') lives entirely in SaveSessionMap (C++), not duplicated in Zig: every caller of BkEditorSaveMap gets it for free, and Editor.save's job is only the temp-file/backup/swap dance around a call that is already safe to trust once it returns OK."
  - "baseNameOf is a small private helper inside editor.zig rather than a call into panels_logic.baseName, keeping the core independent of the app layer for its own status messages."
  - "Both shipped formats (.bzm and .xml) round-tripped through the new read-back check with no float-rounding mismatch on this host, so the spec's idempotent .xml fallback (read back, write again, byte-compare) is present in the test but was never actually exercised - it stays as a safety net for a platform/toolchain where XML float formatting differs, not a used path today."

patterns-established:
  - "Safe save: write to <dir>/<stem>.~save<ext>, verify via bridge read-back, back up once per session, then rename over the real path - every failure branch deletes the temp and leaves the original untouched."

requirements-completed: [D-19, D-20, SPEC-SAVE-ERRORS]

coverage:
  - id: D1
    description: "Editor.save writes to a temporary file, has the bridge verify it by reading it back, keeps one .bak per file per session (never for a new file, never a second time), and swaps the temp in only on success - every failure path leaves the original untouched and the map dirty"
    requirement: "D-19"
    verification:
      - kind: unit
        ref: "Sources/editor/core/editor.zig (zig build test-editor-core) - op-order, one-.bak-per-session, no-.bak-for-a-new-file, refused-write, failed-swap, failed-backup, and no-files tests"
        status: pass
      - kind: unit
        ref: "Sources/editor/core/files.zig (zig build test-editor-core) - path-helper tests and the real-disk StdFiles rename-replaces-target test"
        status: pass
    human_judgment: false
  - id: D2
    description: "The app's every save-capable mode (interactive, --smoke, panelSmoke, --game-reads-it) is wired to a real StdFiles, and a real Save As on disk leaves no temporary file behind"
    requirement: "D-19"
    verification:
      - kind: automated_ui
        ref: "zig build map-editor-smoke map-editor-host-check (Save As + reopen, panel smoke's Save As + reopen; both PASS; zero *.~save.* files left in zig-out/local-test)"
        status: pass
    human_judgment: false
  - id: D3
    description: "BkEditorSaveMap only answers OK once the file it wrote has been read back and found equivalent to what was meant; a read failure or a difference is BK_EDITOR_FAILED naming the reason, and a write that cannot even start (a missing directory) is refused with the path in the message"
    requirement: "SPEC-SAVE-ERRORS"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestSaveVerifiesWhatItWrote (zig build test-editor-bridge, prints editor-bridge: PASS)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The engine tier and the Zig engine tier still pass with the read-back check added to every save path"
    verification:
      - kind: integration
        ref: "zig build map-editor-smoke test-map-editor-engine (2 PASS lines)"
        status: pass
    human_judgment: false

# Metrics
duration: ~23min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 03: Safe save Summary

**Editor.save now writes a bridge-verified temporary file, keeps one `.bak` per session, and swaps atomically - `SaveSessionMap` reads its own write back and compares it before the bridge ever answers OK, so a failed save can no longer touch the map on disk.**

## Performance

- **Duration:** ~23 min (approximate - not captured at session start; derived from the prior plan's STATE.md session end and this plan's completion)
- **Started:** approx. 2026-09-28T04:13:28Z
- **Completed:** 2026-09-28T04:36:19Z
- **Tasks:** 2 completed
- **Files modified:** 10 (1 created, 9 modified)

## Accomplishments

- New `Sources/editor/core/files.zig`: a `Files` interface (`exists`/`copy`/`rename`/`delete`/`lastError`), `StdFiles` backed by `std.Io.Dir`, and `FakeFiles` (an in-memory, op-logged filesystem with `fail_copy`/`fail_rename` for tests) - plus `osPathFromEngine`, `tempPathFor` (`<dir>/<stem>.~save<ext>`), and `backupPathFor` (`<os_path>.bak`). A real-disk test on `std.testing.tmpDir` proves `std.Io.Dir.rename` replaces an existing target with no extra flag on this host (research A2).
- `Editor.save` (editor.zig) rewritten as the spec's safe save: refuses with "saving needs a file system" when `files` is unset; writes and verifies through the bridge into a `tempPathFor` sibling; on the session's first write to a path that already exists, copies it to `path.bak` (skipped, but still recorded, for a path that did not exist yet, so a file created this session never gets a `.bak` of a later version); swaps the temp over the real path with `files.rename`. Every failure branch deletes the temp and leaves the original untouched, with a status message naming the step and the reason.
- `FakeBridge` gained `files` (a shared `FakeFiles` pointer) and `fail_save`: when `files` is set, `saveMap` writes `"fake map: <n> objects"` at the OS form of its path, so a core test can see the whole temp-then-swap dance land in one fake filesystem.
- `main.zig` gives every save-capable mode (interactive, `--smoke`, `panelSmoke`, `--game-reads-it`) its own `core.files.StdFiles{ .io = io }`.
- `SaveSessionMap` (session.cpp) now reads the just-written file back with `NMapFile::Read` and compares it to the snapshot with `NMapFile::AreEquivalent`: a read failure is `"the written map does not read back: <reason>"`, a difference is `"the written map reads back different at <where>"`, both `BK_EDITOR_FAILED`. `bridge.h`'s `BkEditorSaveMap` doc now says so.
- New engine-tier test `TestSaveVerifiesWhatItWrote`: opens coldwinter, moves an object and paints a cell, saves and reads back as itself for both `.bzm` and `.xml` (an idempotent read-write-compare fallback exists for `.xml` alone, in case the XML writer ever fails to round-trip a float exactly - not needed on this host, both formats matched the original directly), and a save into a missing directory is refused with the path named in `BkEditorLastMessage`; coldwinter reopens afterward. Registered in `main`'s list right after `TestObjectEdits`.
- `smoke.zig`'s `saved` expectation now also asserts no `<stem>.~save<ext>` temp file is left beside the saved file.

## Task Commits

Each task was committed atomically:

1. **Task 1: Save swaps in a verified temporary file and keeps one .bak - core, fake and the app** - `704bcebe4` (feat)
2. **Task 2: BkEditorSaveMap reads back what it wrote and compares it** - `7c9b1418d` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

## Files Created/Modified

- `Sources/editor/core/files.zig` - new: `Files`, `StdFiles`, `FakeFiles`, path helpers
- `Sources/editor/core/editor.zig` - `Editor.files`, `Editor.backed_up`, `save()` rewritten as the safe save
- `Sources/editor/core/fake_bridge.zig` - `files`, `fail_save`, `saveMap` writes into the shared fake filesystem
- `Sources/editor/core/root.zig` - exports `files`
- `Sources/editor/app/main.zig` - a real `StdFiles` wired into every save-capable mode
- `Sources/editor/app/panels_logic.zig` - the "file actions" test now sets a `FakeFiles`
- `Sources/src/EditorBridge/session.cpp` - `SaveSessionMap` reads back and compares
- `Sources/src/EditorBridge/bridge.h` - `BkEditorSaveMap` doc update
- `tools/zig/editor_bridge_test.cpp` - `TestSaveVerifiesWhatItWrote`
- `Sources/editor/app/smoke.zig` - `saved` also checks no leftover temp file

## Decisions Made

- The read-back verification lives entirely in `SaveSessionMap` (C++), not duplicated in Zig: every caller of `BkEditorSaveMap` gets it for free, and `Editor.save`'s job is only the temp-file/backup/swap dance around a call that is already safe to trust once it returns OK.
- `baseNameOf` is a small private helper inside `editor.zig` rather than a call into `panels_logic.baseName`, keeping the core independent of the app layer for its own status messages.
- Both shipped formats (`.bzm` and `.xml`) round-tripped through the new read-back check with no float-rounding mismatch on this host, so the spec's idempotent `.xml` fallback (read back, write again, byte-compare) is present in the test but was never actually exercised - it stays as a safety net, not a used path today.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Two ordinary C++ authoring mistakes surfaced and were fixed before the first build attempt that mattered: a most-vexing-parse on `std::vector<unsigned char> tiles( size_t( nTileCount ) )` (fixed by giving the constructor a second argument, matching this file's own existing `tiles( size_t( nCount ) + 1, 0xAB )` pattern elsewhere), and a stray `.c_str()` on `NStr::Format`, which already returns `const char*` in this codebase. Neither reached a commit; not a deviation from the plan, just corrected during normal authoring.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Autosave (D-20/D-21/D-22, plan 03-07) can call `Editor.save` directly and inherit the whole safe-save contract (temp, verify, one `.bak` per session, atomic swap) with no further work in the core.
- `Editor.files` must be set by any future mode that opens an `Editor` and expects to save through it; a mode that forgets gets a clear "saving needs a file system" refusal rather than a crash or a silent no-op.
- No blockers for the rest of phase 3.

## Self-Check: PASSED

- `[ -f Sources/editor/core/files.zig ]`, `[ -f Sources/editor/core/editor.zig ]`, `[ -f Sources/editor/core/fake_bridge.zig ]`, `[ -f Sources/editor/core/root.zig ]`, `[ -f Sources/editor/app/main.zig ]`, `[ -f Sources/editor/app/panels_logic.zig ]`, `[ -f Sources/src/EditorBridge/session.cpp ]`, `[ -f Sources/src/EditorBridge/bridge.h ]`, `[ -f tools/zig/editor_bridge_test.cpp ]`, `[ -f Sources/editor/app/smoke.zig ]` - all FOUND.
- `git log --oneline --all --grep="03-03"` returns 2 commits (`704bcebe4`, `7c9b1418d`).
- All task `<acceptance_criteria>` re-verified: `Sources/editor/core/files.zig` has no `@cImport`/`sdl3`; `grep -n "tempPathFor\|backupPathFor\|files.rename\|\.rename(" Sources/editor/core/editor.zig` finds the swap; the real-disk rename test is in `files.zig` and passes under `zig build test-editor-core`; the smoke's Save As and reopen pass with real `StdFiles` and no `*.~save.*` file remains in `zig-out/local-test`; `grep -n "AreEquivalent" Sources/src/EditorBridge/session.cpp` finds the read-back comparison inside `SaveSessionMap`; `TestSaveVerifiesWhatItWrote` is in `main`'s list and the engine tier prints `editor-bridge: PASS`; the smoke and `test-map-editor-engine` pass - all PASS.
- Plan-level `<verification>`: `zig build test-editor-core test-map-editor-panels` (rc=0), `zig build test-editor-bridge` (rc=0, `editor-bridge: PASS`), `zig build map-editor-smoke test-map-editor-engine` (rc=0, 2 PASS lines), `zig build map-editor-smoke map-editor-host-check` (rc=0, both PASS, zero leftover temp files), and the full `zig build test` (rc=0) all re-run clean on the final committed state.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

---
phase: 03-map-editor-plan-6-finish-m1
plan: 15
subsystem: map-editor
tags: [zig, ci, exit-criteria, map-sweep, spec, release-build]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plans 01-14)
    provides: "The whole M1 editor: test launch, safe save, settings/autosave/recovery, mods, palette pictures, sound list, BK_EDITOR_AUTO, packaging"
provides:
  - "M1 exit-criteria evidence: full sweep 1,755/1,755, local tiers 11 PASS lines, CI run 36473046568 green on all six jobs"
  - "The portable-map-editor spec corrected to the built M1 (test launch, -mod=Name, safe save, user data, camera decision, pictures, sound list, packaging, BK_EDITOR_AUTO, exit criteria)"
  - "A release-variant Game + MapEditor stage for Johannes's hand try"
affects: [/gsd-verify-work for phase 3, M2 planning]

actuals:
  tokens: 9200
  tasks: 3
  commits: 1
plan_head_before: 87a2028f0

tech-stack:
  added: []
  patterns:
    - "Exit criteria in the spec cite the command and date, not an ephemeral CI run number for this plan; the run id lives in the SUMMARY"

key-files:
  created: []
  modified:
    - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md

key-decisions:
  - "Task 1 changed no source: every shipped map already round-trips, so MapFile.cpp and MapEquivalence.cpp were not touched"
  - "The spec cites CI by the 03-14 run for the build criterion and points here for this plan's own run, so the spec needs no edit after each CI run"
  - "Spec exit criterion 3 records that 'rotates units' means object rotation (Q/E); camera rotation is the separately deferred D-12"

requirements-completed: [M1-EXIT-SWEEP, M1-EXIT-BUILD, M1-EXIT-TIERS]

coverage:
  - id: D1
    description: "Every shipped map (1,755 .bzm plus the .xml maps) opens and saves unchanged to an equivalent map"
    requirement: "M1-EXIT-SWEEP"
    verification:
      - kind: integration
        ref: "zig build test-map-files-all -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (zig-out/local-test/03-15-t1-sweep.log: 1755 of 1755 round-tripped, map-file: PASS, 0 FAIL)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Core and map-file tiers pass on all six CI targets and MapEditor builds on its two platforms"
    requirement: "M1-EXIT-BUILD"
    verification:
      - kind: e2e
        ref: "CI run 36473046568 at a8ee30d10: all six jobs success"
        status: pass
    human_judgment: false
  - id: D3
    description: "Engine, game-reads-it and editor-app tiers pass locally on macOS arm64"
    requirement: "M1-EXIT-TIERS"
    verification:
      - kind: integration
        ref: "zig build test; zig build test-editor-bridge test-map-editor-engine map-editor-host-check map-editor-smoke map-editor-game-reads-it map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (11 PASS/done lines, rc=0)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The spec matches the software plan 6 built and decided"
    verification:
      - kind: other
        ref: "grep -c 'editor-test|mapeditor.cfg|BK_EDITOR_AUTO|-mod=' spec = 10 (threshold 4); commit a8ee30d10"
        status: pass
    human_judgment: true
    rationale: "Whether the prose says what Johannes decided is a reading judgment; the grep proves only that the named items are present."
  - id: D5
    description: "Johannes's hand try on macOS arm64 with the release build (M1-EXIT-HANDTRY)"
    requirement: "M1-EXIT-HANDTRY"
    verification: []
    human_judgment: true
    rationale: "Real trackpad, real window, real play; cannot be driven from the agent shell. PENDING: result not yet reported."

duration: ~95min (including ~27min unattended CI wait and a ~4min release build)
completed: 2026-09-29
status: complete
---

# Phase 3 Plan 15: M1 exit sweep, suite, CI, spec and hand-try hand-over Summary

**All automated M1 exit criteria are met with evidence (1,755/1,755 shipped maps round-trip, 11 local tier PASS lines, CI green on all six jobs), the spec now describes what plan 6 built and decided, and a release Game plus MapEditor stage is ready for Johannes's hand try, which is still pending.**

## Performance

- **Duration:** ~95 min, of which about 27 min was waiting for the Windows CI job and about 4 min was the release build
- **Completed:** 2026-09-29
- **Tasks:** 3 (Task 3's human check is pending, see below)
- **Files modified:** 1 (the spec)

## Accomplishments

- **Full sweep:** `zig build test-map-files-all` reported `map-file: sweeping 1755 maps`, `1755 of 1755 maps round-tripped`, `map-file: PASS`, zero `FAIL:` lines; 18 s wall time with a warm cache. No source changed.
- **Local suite:** `zig build test` rc=0 (`wheel scroll: PASS`). The six tiers ran in 2 min 49 s, rc=0, with these lines:
  - `map-editor: host check PASS (metal, 1280x800)` (three runs, one with `-mod=EditorTestMod`) and three `panel smoke PASS (5559 catalogue entries, 184 tiles, ...)`
  - `map-editor: smoke PASS (30 steps, 260 objects, ...)`
  - `editor-bridge: PASS`
  - `map-editor: game reads it PASS (4 units of player 0 near the placed unit, game exit 0)`
  - `map-editor: BK_EDITOR_AUTO: done (13 actions)`
  - `map-editor-engine: PASS (260 objects)`
- **CI:** run 36473046568 (`workflow_dispatch` on `feat/map-editor-plan-6`, head `a8ee30d10`), all six jobs success:

  | Job | Result | Time |
  |---|---|---|
  | linux-platform | success | 6.3 min |
  | linux-arm-platform | success | 1.2 min |
  | macos-platform | success | 18.0 min |
  | macos-intel-platform | success | 2.7 min |
  | windows-mingw-platform | success | 7.8 min |
  | windows-platform | success | 26.8 min |

  Windows budget check (research Pitfall 6): 26.8 min of the 110-minute timeout, well inside it; the earlier 55-min run (36464351662) was a cold-cache run.
- **Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` updated in `a8ee30d10`: M1 scope (camera rotation deferred with its 03-06 measurements, `-mod=Name`, sound list, Windows app), preservation invariant (sounds now edited), settings location, new sections "Object palette pictures", "Editor: sound list" and "User data: maps, settings and mods", packaging beside the game with the Windows GUI subsystem, the test launch as built (`-editor-test`, MapEditorTest generated-data root), safe save with `.bak`, BK_EDITOR_AUTO grammar and automated shot comparison, and each exit criterion marked with evidence.
- **Release stage:** `zig build install-map-editor -Dtarget=aarch64-macos --release=fast -Dcopy-data=false` rc=0; `Game` and `MapEditor` in `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/`. `MapEditor --check Data\Maps\Multiplayer\coldwinter.bzm` printed `host check PASS (metal, 1280x800)`, `unknown-object warning PASS`, `panel smoke PASS`; `Game -help` starts and exits 0. Disk after build: 13 GB free.

## Task Commits

1. **Task 1: full sweep** - no commit (no source change needed; log in `zig-out/local-test/03-15-t1-sweep.log`, gitignored)
2. **Task 2: whole suite locally and in CI, spec brought up to date** - `a8ee30d10` (docs)
3. **Task 3: release build for the hand try** - no commit (build artifacts are gitignored; spec hand-try line to be finalised when Johannes reports)

**Plan metadata:** the docs commit carrying this SUMMARY and STATE.md.

## Hand-try checklist (Task 3, PENDING for Johannes)

Build: `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/`. Start `MapEditor` from that folder (`cd` there, then `./MapEditor`). This is the worktree's own stage; his live installation and profile are not involved, and the test game uses the `MapEditorTest` profile.

1. Open `Data\Maps\Multiplayer\coldwinter.bzm`. Paint tiles, place a unit, move it, rotate it with Q/E, delete it, change a side in Players; undo and redo all of it.
2. Save: on a shipped map it becomes Save As into your maps folder; save.
3. F5: the game opens windowed beside the editor and plays the saved state with your unit. Quit the game: the editor still has the map and its history. F5 twice: the restart prompt.
4. Pinch and Shift+swipe zoom at the pointer, a plain swipe pans, Home resets zoom.
5. Close the window with changes: Save / Don't save / Cancel.
6. Edit > Settings: swipe speed changes the pan, autosave interval; File > Open Recent.
7. File > Mod with a mod you have installed locally (never committed).
8. Palette pictures in a few groups: each object shows its own icon; single soldiers show their squad's icon; 41 objects show a neutral frame with their name.
9. The Sounds panel on a map with sounds (none of the shipped maps has any, so add one at the view centre, edit it, undo and redo, save, reopen).
10. A shipped map opens read-only (title says read-only; Save asks for a new name).

Report "approved" or the failing numbers. Camera rotation is deliberately not on the list (deferred in 03-06).

Result: **pending**.

## Gap fix: a map in another installation's Data was saved in place (hand try, step 2)

**Found:** from the release stage, File > Open of `/Users/johannes/Projects/src/Blitzkrieg/Data/Maps/Multiplayer/coldwinter.bzm` (the main checkout's Data, not the stage's), then File > Save, wrote into that file and left `coldwinter.bzm.bak` beside it. D-18 says a shipped map is read-only and Save becomes Save As.

**Root cause:** `panels_logic.isShippedMap(engine_path, base_root)` compared text only: relative `data/...`, or an absolute path starting with the editor's own base root. Another installation's Data, or the stage's own Data reached through its symlink's target, looked like a user's file. Autosave (D-20) used the same answer, so it would have written into that file every 2 minutes as well.

**New rule** (`Sources/editor/core/shipped.zig`, std-only, the disk questions behind `Files.isDataRoot`/`Files.realPath`): a map is read-only when, for its path as given or its real path (symlinks resolved), any of these holds:
1. it is under `<base_root>Data/` or `<base_root>mods/<N>/data/`, checked against the base root and the base root's real path;
2. it is under `mods/<N>/data/` anywhere;
3. one of its ancestor folders is named `Data` (any case) and has a game data root marker at its top: `consts.xml`, `objects.xml`, `mod.xml`, `resource.description` or any `*.pak`.

A folder of your own that happens to be called `Data` stays writable because it has no marker. The user maps folder, `mods/<N>/maps`, the recovery folder and the generated test-map root have no `data` segment, so they stay writable.

**Where it applies:** Save turns into Save As, autosave writes only to the recovery copy (D-22), and the title says "(read-only)". The panels cache one answer per document path, so the disk is only asked again when the path changes. `Editor.save` also refuses any target inside a game's data before it writes anything, not even a temp file or a `.bak`. The status line then reads "not saved: <name> is inside a game's data folder, which is read-only - Save As into your maps folder instead". This catches any caller that forgets the rule.

**Commit:** `48bf7cfe8` fix(03-15): a map inside any game's Data is read-only, not only the editor's own.

**Evidence** (debug stage `zig-out/game/macos/arm64/debug`; the release stage was not touched because Johannes was running it):
- `test-editor-core` 85/85 passed, including 9 new `shipped.zig` tests. They cover own Data relative and absolute, another tree's Data, Windows drive-letter and backslash paths, a symlinked Data both ways, a symlinked base root, `mods/<N>/data`, the user maps and recovery folders, an unmarked `Data`, and a real-disk case with a real symlink. There is also a new `Editor.save` refusal test: nothing is written, copied, renamed or deleted.
- `test-map-editor-panels` 45/45 passed, including a new foreign-tree `needsSaveAs` test and an autosave-target test. `test-map-editor-auto` passed. `zig build test` passed 203/203.
- `map-editor-smoke` PASS, 35 steps. The 5 new steps build `zig-out/local-test/map-editor-smoke-foreign/Data` with a `consts.xml` marker and a copied `coldwinter.bzm`, and open it by its absolute path. The map shows as read-only. After a dirtying edit, Save becomes Save As. A due autosave writes only `map-editor-smoke-user/mapeditor/recovery/coldwinter.bzm`, because the smoke points its user root under local-test and never into the real one. A forced in-place `editor.save` returns `error.Refused`. After every step the foreign file's bytes (Wyhash), size, inode and mtime are unchanged, and there is no `.bak` and no `.~save` temp.
- Negative controls, reverted afterwards: with the panels' classifier given no `Files`, the smoke fails with "another installation's map opens read-only: ... is not read-only". With the `Editor.save` guard disabled, it fails with "Editor.save wrote into ..." and the new core test fails.
- `map-editor-host-check` passed with the relative path, the absolute path through the stage's Data symlink, and `-mod=EditorTestMod`.
- The core and panels tests also cross-compile for `x86_64-windows-gnu` (with `--test-no-exec`).

**Still to do:** rebuild the release stage (`zig build install-map-editor -Dtarget=aarch64-macos --release=fast -Dcopy-data=false`) and run its `MapEditor --check`. The orchestrator does this once Johannes has left the release folder. Then retry hand-try step 2 on the main checkout's `coldwinter.bzm`. When this was written, the main checkout's `Data/Maps/Multiplayer/coldwinter.bzm` matched git (`git status` was clean, mtime 04:34) and the first try's `.bak` was already gone. This fix only read that checkout and wrote nothing to it.

## Open items for Johannes to decide (plan 5 carried, not closed by any plan of phase 3)

Still open in `.planning/WINDOWS.md` (entries 1-3, ledger `open_count: 3`):

1. The status line is never cleared after a later success (`Sources/editor/app/panels.zig`).
2. No test of `view.zig`'s event-to-tool wiring beyond the routing function (`Sources/editor/app/view.zig`).
3. The scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (`Sources/editor/app/view_math.zig`).

They block `/gsd-ship` while `windows_enforce` is on. Options: a small follow-up plan, or waive each with a reason. Not decided here.

Other known limits carried in the phase summaries: `map-editor-auto` is local-only (not in CI); `bk_imgui_backend_use_global_mouse` cannot fully disable ImGui's private cursor fallback (03-12); the renderer's own capture-failure string is not reachable through IGFX (03-13); the Windows Test-in-game and cursor-isolation paths were not exercised on a real Windows desktop.

## Decisions Made

See key-decisions in the frontmatter.

## Deviations from Plan

None - plan executed as written. The plan's own CI-run instruction was followed with an explicit `gh workflow run`, since a push alone does not trigger the workflow on this branch.

## Issues Encountered

- `.planning/REQUIREMENTS.md` is the old port-level document and has no `M1-EXIT-*` IDs, so `requirements mark-complete` is a no-op for them, as in plans 05, 11 and 14. `M1-EXIT-HANDTRY` stays unmarked until Johannes reports.
- The worktree's untracked `.gsd/` and `.planning/milestone.lock` were left alone.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- M1's automated exit criteria are met. Remaining: Johannes's hand try, then `/gsd-verify-work` for phase 3 (harvesting the `human_judgment` items across the summaries), then the decision on the three open ledger entries.
- On "approved", a continuation records the result here and marks the spec's hand-try line met; on failures, the failing numbers become gaps for `/gsd-verify-work`.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-29*

## Self-Check: PASSED

- Spec exists and holds the commit: `git log` shows `a8ee30d10` on `feat/map-editor-plan-6`, pushed (origin at the same head when CI ran).
- CI run 36473046568 head SHA equals `a8ee30d10...`; six jobs success.
- Release stage: `Game` and `MapEditor` present; `03-15-t3-check.log` holds one `host check PASS`.
- `plan_head_before` 87a2028f0; `git rev-list --count 87a2028f0..a8ee30d10` = 1, matching `actuals.commits`.

---
phase: "3"
slug: "map-editor-plan-6-finish-m1"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-28"
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Zig `std.testing` (core tier) + C++ test executables via `zig build` (map-file and engine tiers) + scripted-SDL smoke `smoke.zig` / `BK_EDITOR_AUTO` (app tier) |
| **Config file** | none — steps defined in `build.zig` |
| **Quick run command** | `zig build test -Dtarget=aarch64-macos` |
| **Full suite command** | `zig build test-editor-bridge -Dtest-mode=run && zig build test-map-editor-engine && zig build map-editor-host-check && zig build map-editor-smoke && zig build test-map-files-all` (macOS arm64; CI runs the GPU tiers on macOS and Windows) |
| **Estimated runtime** | ~120 seconds quick; full sweep several minutes |

---

## Sampling Rate

- **After every task commit:** Run `zig build test -Dtarget=aarch64-macos` plus the tier the task touches
- **After every plan wave:** Run the full suite command locally and one CI run on both GPU runners
- **Before `/gsd-verify-work`:** Full suite must be green, plus Johannes's hand try of the M1 exit criteria
- **Max feedback latency:** 300 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 03-01-T1 | 01 | 1 | D-02, D-05, D-07 | T-03-01-01/02 | active.cfg untouched, no cloud sync | harness + unit | `zig build test-game-command-line`; Game -editor-test harness run | ❌ W0 (new cases) | ⬜ pending |
| 03-01-T2 | 01 | 1 | D-04, D-07 | — | — | harness | BK_AUTO_UI `units=` run | ❌ W0 | ⬜ pending |
| 03-02-T1 | 02 | 2 | D-01, D-04, D-05, D-08, D-09 | T-03-02-01/02/03 | argv array, sanitised paths, map file untouched | game tier | `zig build map-editor-game-reads-it`, `test-map-editor-testlaunch` | ❌ W0 | ⬜ pending |
| 03-02-T2 | 02 | 2 | D-03, D-06 | T-03-02-02 | bad names refused | engine + unit | `test-editor-bridge` (TestPathsAndTestMapPath), `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 03-03-T1 | 03 | 3 | D-19, D-20 | T-03-03-01/02/03 | original untouched on failure | core (all six CI targets) | `zig build test-editor-core` | ✅ extend | ⬜ pending |
| 03-03-T2 | 03 | 3 | D-19 | T-03-03-04 | read-back verified | engine | `test-editor-bridge` (TestSaveVerifiesWhatItWrote) | ❌ W0 | ⬜ pending |
| 03-04-T1 | 04 | 4 | D-23 | T-03-04-02 | no silent discard | unit + smoke | `test-map-editor-panels`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-04-T2 | 04 | 4 | D-17, D-18 | T-03-04-01/03 | shipped maps never written | unit + smoke | `test-map-editor-panels`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-05-T1 | 05 | 5 | D-10, D-11, D-14, D-16 | T-03-05-01 | NaN refused, steps clamped | engine + unit | `test-editor-bridge` (TestZoomStepsBoundedAndAnchored), `test-map-editor-view` | ❌ W0 | ⬜ pending |
| 03-05-T2 | 05 | 5 | D-10, D-13, D-15 | — | — | unit + smoke | `test-map-editor-view`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-05-T3 | 05 | 5 | CARRY-OUTLINE | — | — | engine | `test-editor-bridge` (TestWorldToScreenRoundTrip) | ❌ W0 | ⬜ pending |
| 03-06-T1 | 06 | 6 | D-12 | T-03-06-01 | — | engine (measurement) | `test-editor-bridge` (TestYawMeasurement) | ❌ W0 | ⬜ pending |
| 03-06-T2 | 06 | 6 | D-12 | T-03-06-02 | decision before wiring | checkpoint:decision | — | — | ⬜ pending |
| 03-06-T3 | 06 | 6 | D-12, D-13 | — | — | unit + smoke (if built) | `test-map-editor-view`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-07-T1 | 07 | 7 | D-24, D-25 | T-03-07-01/03 | lenient parse, automation never touches user state | core + check | `test-editor-core`; `MapEditor --check` with BK_EDITOR_SETTINGS | ❌ W0 | ⬜ pending |
| 03-07-T2 | 07 | 7 | D-27, D-23 | — | — | core + unit | `test-editor-core`, `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 03-07-T3 | 07 | 7 | D-20, D-21, D-22 | T-03-07-02/04 | sanitised recovery names, safe autosave | core + unit | `test-editor-core`, `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 03-08-T1 | 08 | 8 | D-26 | T-03-08-01/02/03 | folder names validated, no profile write, no unlicensed mod | host check (CI both GPU runners) | `map-editor-host-check` with `-mod=EditorTestMod` | ❌ W0 | ⬜ pending |
| 03-08-T2 | 08 | 8 | D-09, D-23, D-26 | T-03-08-01 | — | engine + unit | `test-editor-bridge` (TestModsListSetAndClear), `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 03-08-T3 | 08 | 8 | D-28 | — | preservation without a mod | engine | `test-editor-bridge` (TestSaveRecordsTheMod) | ❌ W0 | ⬜ pending |
| 03-09-T1 | 09 | 9 | D-29 | T-03-09-01 | bounded decode | engine (coverage) | `test-editor-bridge` (TestObjectPictures) | ❌ W0 | ⬜ pending |
| 03-09-T2 | 09 | 9 | D-29 | — | — | checkpoint:decision | — | — | ⬜ pending |
| 03-09-T3 | 09 | 9 | D-29 | T-03-09-02 | textures released | unit + smoke | `test-map-editor-panels`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-10-T1 | 10 | 10 | CARRY-SOUNDS | T-03-10-01/02 | refusals change nothing; list preserved | engine | `test-editor-bridge` (TestSoundList) | ❌ W0 | ⬜ pending |
| 03-10-T2 | 10 | 10 | CARRY-SOUNDS | — | — | core + Zig engine tier | `test-editor-core`, `test-map-editor-engine` | ✅ extend | ⬜ pending |
| 03-10-T3 | 10 | 10 | CARRY-SOUNDS | — | — | unit + smoke | `test-map-editor-panels`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-11-T1 | 11 | 11 | CARRY-UNKNOWN-WARNING | — | — | unit + check | `test-map-editor-panels`, `map-editor-host-check` | ✅ extend | ⬜ pending |
| 03-11-T2 | 11 | 11 | CARRY-RESIZE | T-03-11-02 | stuck gestures end | unit + smoke | `test-map-editor-view`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-11-T3 | 11 | 11 | CARRY-MINORS | — | — | unit + smoke | `test-map-editor-panels`, `map-editor-smoke` | ✅ extend | ⬜ pending |
| 03-12-T1 | 12 | 12 | SPEC-AUTO | T-03-12-01/02 | names restricted, no user state | app tier | `zig build map-editor-auto`, `test-map-editor-auto` | ❌ W0 | ⬜ pending |
| 03-12-T2 | 12 | 12 | SPEC-AUTO, M1-EXIT-TIERS | T-03-12-03 | checked TGA parse | app tier | `zig build map-editor-auto` (seed, then compare) | ❌ W0 | ⬜ pending |
| 03-13-T1 | 13 | 13 | CARRY-MINORS | T-03-13-01 | explicit skip, colour-swap probe | check | `map-editor-host-check` (run and compile modes) | ✅ extend | ⬜ pending |
| 03-13-T2 | 13 | 13 | CARRY-MINORS | T-03-13-02 | capture disarmed | engine + check | `test-editor-bridge`, `map-editor-host-check` | ✅ extend | ⬜ pending |
| 03-13-T3 | 13 | 13 | CARRY-MINORS | — | — | engine | `test-editor-bridge` (TestEntryPointsBeforeAMap) | ❌ W0 | ⬜ pending |
| 03-14-T1 | 14 | 14 | D-08, CARRY-PACKAGING | T-03-14-01 | no mods staged | unit (+ local package if disk allows) | `zig build test-stage`; `package-game` + unzip | ❌ W0 | ⬜ pending |
| 03-14-T2 | 14 | 14 | CARRY-CONSOLE | T-03-14-03 | CI output kept | CI | Windows job PASS lines + PE subsystem step | ❌ W0 | ⬜ pending |
| 03-15-T1 | 15 | 15 | M1-EXIT-SWEEP | — | — | map file | `zig build test-map-files-all` | ✅ exists | ⬜ pending |
| 03-15-T2 | 15 | 15 | M1-EXIT-BUILD, M1-EXIT-TIERS | T-03-15-02 | evidence recorded | all tiers + CI | `zig build test` + every tier; CI run | ✅ | ⬜ pending |
| 03-15-T3 | 15 | 15 | M1-EXIT-HANDTRY | T-03-15-01 | worktree release stage only | manual | release `MapEditor --check` + Johannes's hand try | — | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Test-launch harness test (spawn `Game` or a stand-in and assert start/exit, profile, mod, windowed)
- [ ] `tools/zig/editor_bridge_test.cpp` — zoom-step bounds, pointer anchoring, yaw rotation and reset
- [ ] `Sources/editor/core/editor.zig` — safe save temp+rename, `.bak` once per session, autosave interval, recovery copy
- [ ] Settings file round-trip and recent-files tests
- [ ] `smoke.zig` → `BK_EDITOR_AUTO` generalisation and shot comparison tests

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Trackpad pinch and rotate gestures feel right | D-10, D-12 | Real multi-touch input cannot be produced from the agent shell | Johannes pinches and rotates on the MacBook trackpad in MapEditor |
| Test in game end to end on a real display | D-01..D-09 | Needs a visible game window and play | Johannes presses Test in game, plays, quits back to the editor |
| Native file dialogs and the unsaved-changes prompt | D-23 | Native dialogs cannot be driven headlessly | Johannes opens, edits, quits, and answers the prompt |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending

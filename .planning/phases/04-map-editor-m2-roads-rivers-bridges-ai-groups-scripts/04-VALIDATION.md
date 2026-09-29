---
phase: "04"
slug: "map-editor-m2-roads-rivers-bridges-ai-groups-scripts"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-30"
---

# Phase 04 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Zig `std.testing` (core, panels_logic, view_math, auto, testlaunch tiers) + C++ executables with a `Check()` harness (`tools/zig/map_file_test.cpp`, `tools/zig/editor_bridge_test.cpp`) + Zig engine test (`Sources/editor/app/c_bridge_test.zig`) + `BK_EDITOR_AUTO` scripted app scenarios (`smoke.zig`, `map-editor-auto`) + game-reads-it tier (`--game-reads-it`, `BK_AUTO_UI`, `BK_MAP_TRACE`) |
| **Config file** | none — steps are defined in `build.zig` (`test-editor-core` 2778, `test-map-editor-view` 2792, `test-map-editor-panels` 2810, `test-map-editor-testlaunch` 2824, `test-map-editor-auto` 2838, `test-map-files` 5696, `test-map-files-all` 5710, `test-editor-bridge` 5829, `test-map-editor-engine` 6110, `map-editor-auto` 6074, `map-editor-game-reads-it` 6090) |
| **Quick run command** | `zig build test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false` |
| **Map-file tier** | `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false` (CI: Linux x64, Linux arm64, Windows MSVC, macOS arm64, macOS Intel; not Windows MinGW) |
| **Full suite command** | `zig build test-editor-bridge test-map-editor-engine map-editor-host-check map-editor-smoke map-editor-game-reads-it map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` (the M1 exit command, 11 PASS/done lines), plus `test-map-files-all` for the sweep |
| **After editing `build.zig`** | `zig test tools/zig/build_hermeticity_test.zig` (never `zig fmt build.zig`) |
| **Estimated runtime** | quick about 120 seconds; full suite several minutes; CI 47 to 78 minutes |

Tier legend: "Core" = fake bridge, all six CI targets. "Map file" = data-only C++, five targets. "Engine" = real `ITerrainEditor` / `IAIEditor` through `test-editor-bridge` (C++) and `test-map-editor-engine` (Zig), macOS arm64 and Windows MSVC. "App" = `map-editor-auto` M2 scenario (macOS, local). "Game" = M2 `--game-reads-it` (macOS, local).

---

## Sampling Rate

- **After every task commit:** the core tier plus the tier the task touches (the quick run command); a C++ change adds `zig build test-map-files -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false` and, when it touches the bridge, `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false`
- **After every plan (04-05..04-12):** the full suite command, and `zig build map-editor-auto-m2 map-editor-game-reads-it-m2` with the plan's scenario segment
- **After every plan wave:** the full suite plus `test-map-files-all`; one CI dispatch on the branch at the end of 04-08 (mid-phase gate) and the final one in 04-13
- **Before `/gsd-verify-work`:** everything green, CI six jobs green, the Windows run on win-home (non-GUI tiers), then the agent-run scripted walk-through on `--release=fast` (D-25.9 amended: replaces the hand try per Johannes's autonomy order)
- **Max feedback latency:** 300 seconds for the quick loop

---

## Per-Task Verification Map

Task IDs are `04-NN-Tm` for the thirteen plans (D-24's eight split; see 04-01-PLAN.md's objective). Wave equals plan order (plans share files and run one after another). Feature rows are proven on every tier that applies (see the tier legend); the command column names the tiers, and the full case list lives in 04-RESEARCH.md "Validation Architecture". The game tier is `map-editor-game-reads-it-m2`, the app tier `map-editor-auto-m2`.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 04-01-T1 (anchors tracer, record_edit) | 01 | 1 | D-01, D-02, D-22 | T-04-01-01..03 | untouched records byte-equal to an unedited save; anchor pad never shrinks | core + map file + engine | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine` | ❌ W0 | ⬜ pending |
| 04-01-T2 (NMapRecords, every collection) | 01 | 1 | D-01, D-25.2 | T-04-01-02, 03 | edit plus inverse byte-identical per collection | map file | `test-map-files` | ❌ W0 | ⬜ pending |
| 04-01-T3 (palette filter, spec) | 01 | 1 | D-05, D-23 | T-04-01-04 | game types 4, 6, 9 not placeable; `BkEditorAddObject` refuses; loaded objects still load and move | engine + unit | `test-editor-bridge`, `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 04-02-T1..T3 (cascade delete, `FindReferences`) | 02 | 2 | D-04, O16 refs | T-04-02-01..03 | refused span/trench/passenger delete leaves document and history unchanged; do/undo/redo restores every reference; link 0 ignored | core + map file + engine | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine` | ❌ W0 | ⬜ pending |
| 04-03-T1..T3 (commands, markers, input, registry, verbs) | 03 | 3 | D-06, D-22, C13 | T-04-03-01, 02 | malformed verbs fail loudly; marker caps | core + view + panels + auto parser + app | `test-editor-core`, `test-map-editor-view`, `test-map-editor-panels`, `test-map-editor-auto`, `map-editor-auto-m2` | ❌ W0 | ⬜ pending |
| 04-04-T1..T2 (game seam, anchor in game) | 04 | 4 | D-22, D-25.5 | T-04-04-01, 02 | env-gated trace, no absolute paths | testlaunch + game | `test-map-editor-testlaunch`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-04-T3 (render proof, A/B capture) | 04 | 4 | D-09 | — | artefacts in `zig-out/local-test` only | engine (measurement) | `test-editor-bridge` (`TestM2VsoRendersOnGpu`) | ❌ W0 | ⬜ pending |
| 04-05-T1..T4 (roads and rivers) | 05 | 5 | D-07, D-08, D-09, D-03, VO5-VO7 | T-04-05-01..04 | `VsoMatchesEngine` after every step; new road passes read-back; nID above all in use; "too short" refused | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-06-T1..T4 (bridges) | 06 | 6 | D-10, D-11, D-12, VO2, VO3, S4 | T-04-06-01..03 | all-or-nothing draw (refusal leaves history unchanged); partner missing refused; every `bridges` link resolves after each step | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-07-T1..T2 (fences) | 07 | 7 | D-14, VO1 | T-04-07-01, 02 | tiles outside the map refused; tile mapping equals `ITerrainEditor::GetAITileIndex` | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-08-T1..T3 (entrenchments, mid-phase CI) | 08 | 8 | D-13, VO4, O18 drawing | T-04-08-01..03 | trench plan properties hold over 500 random polylines; every section link resolves; no engine grouping call | core + map file (property) + engine + app + game + CI | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2`, CI dispatch | ❌ W0 | ⬜ pending |
| 04-09-T1..T3 (script ID, groups) | 09 | 9 | D-15, D-16, G1 | T-04-09-01..03 | groups written sorted, bytes equal to an unedited save when untouched; duplicates skipped; hidden objects not pickable | core + map file + engine + app + game | `test-editor-core`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-10-T1..T2 (script file) | 10 | 10 | D-20, M6 | T-04-10-01..04 | bare-name validation (separators, `..`, `.lua` suffix, characters); Open script uses a validated path, never a user string | core + engine + game | `test-editor-core`, `test-editor-bridge`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-10-T3..T4 (script areas) | 10 | 10 | D-21, MT2 | T-04-10-05 | name rules (empty, duplicate); Vis->AI truncation rule; untouched areas byte-exact | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-11-T1..T2 (start commands) | 11 | 11 | D-17, U1, U2 | T-04-11-02..04 | `fromExplosion` kept; empty command erased on cascade; link 0 refused | core + engine + app + game | `test-editor-core`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-11-T3..T4 (reserve positions) | 11 | 11 | D-18, U3 | T-04-11-01 | towed gun without truck refused by role; squads refused; refusals leave the map unchanged | core + engine + app + game | `test-editor-core`, `test-editor-bridge`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-12-T1..T3 (AI general) | 12 | 12 | D-19, AI1 | T-04-12-01..03 | side creation undone restores the vector size; radius and direction per the MFC store formula | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto-m2`, `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-13-T1 (full scenarios, test in game with script and anchor) | 13 | 13 | D-25.5, D-25.6, D-20, D-22 | — | the test directory gets the copied script; shots compared | app + game | `map-editor-auto-m2` (twice), `map-editor-game-reads-it-m2` | ❌ W0 | ⬜ pending |
| 04-13-T2 (preservation sweeps) | 13 | 13 | D-25.4 | — | edits plus undo leave file bytes equal to an unedited save; `test-map-files-all` stays 1,755 of 1,755 | map file + engine (macOS local) | `test-map-files-all`, `test-map-files-m2-sweep`, `test-editor-bridge-m2-sweep` | ❌ W0 | ⬜ pending |
| 04-13-T3..T4 (CI, win-home, release walk-through, parity evidence) | 13 | 13 | D-24, D-25, 04-PARITY rows | T-04-13-01..04 | six CI jobs, non-GUI tiers on win-home, "Closes with" filled for every row, VO3 wording corrected; the hand try replaced by the agent-run walk-through (amended D-25.9) | CI + release scenarios | `gh workflow run "Cross-platform validation" --ref feat/map-editor-m2`; `--release=fast` scenarios | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `Sources/editor/core` record types, `history.zig` variants and the fake-bridge extension (collections, cascade, references) — every core test depends on them
- [ ] `tools.Event`/`Key` extension, right-button and double-click routing in `view.zig`, `staleGesture` for two buttons, registry, marker layer, `commands.zig`
- [ ] `auto.zig` verbs `rclick=`, `dblclick=`, `tool=`, `do=` and key names `INSERT`; parser tests in `test-map-editor-auto`; `AutoRunner` handlers in `smoke.zig`
- [ ] `MapRecords`, `MapGeometry` and their entries in `build.zig` (`addMapFile`, line 3765) plus the hermeticity run
- [ ] `map_file_test.cpp` M2 cases and the M2 sweep entry; `editor_bridge_test.cpp` `TestM2*` functions (keep the Windows CRT-to-stderr routing already in `main`); `c_bridge_test.zig` M2 round trip
- [ ] Game seam `BK_MAP_TRACE` plus the `Trace` mirror; `tools/zig/fixtures/m2_script.lua` (Lua 4 dialect, `function Init()` calling `GetScriptAreaParams`, `GetNUnitsInScriptGroup`, `Trace` with number arguments only)
- [ ] A/B render capture test for roads and rivers (04-01), artefacts in `zig-out/local-test`
- [ ] `Files.list` (core), if 04-05 keeps the ".lua beside the map" list

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Trackpad feel of double-click finish and Ctrl-click as right-click | D-07, D-13 | Real input cannot be produced from the agent shell | Johannes draws a road and a trench on the release build |
| "Open script" opens a sensible editor on macOS and on Windows | D-20 | OS file association | Click the button with a real `.lua` beside a saved map |
| Visual mark of a built-during-play bridge and of Hide checked | D-12, D-16 | Judgement of legibility | Look at the shot and the live view |
| GPU look of roads and animated rivers on Windows | D-09 | CI engine tier checks numbers only | Look at a win-home run of the M2 scenario shot |
| The hand try (amended: replaced by the agent-run scripted walk-through, 04-13-T3) | D-25.9 | Johannes ordered the phase to run without questions | `map-editor-auto-m2` and `map-editor-game-reads-it-m2` with `--release=fast`; every M2 shot converted to PNG and inspected by the executor; not observable by an agent: trackpad feel, the OS opening a `.lua`, the GPU look on Windows (listed in 04-13-SUMMARY) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending

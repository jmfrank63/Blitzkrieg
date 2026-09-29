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
- **After every plan (04-02..04-07):** the full suite command, and `zig build map-editor-auto` with the plan's scenario segment
- **After every plan wave:** the full suite plus `test-map-files-all`; one CI dispatch on the branch after Wave 2
- **Before `/gsd-verify-work`:** everything green, CI six jobs green, the Windows run on win-home, then Johannes's hand try on `--release=fast`
- **Max feedback latency:** 300 seconds for the quick loop

---

## Per-Task Verification Map

Task IDs are per feature and plan; the planner refines them to `04-NN-Tm`. Wave equals plan order until the plans fix it. Feature rows are proven on every tier that applies (see the tier legend); the command column names the tiers, and the full case list lives in 04-RESEARCH.md "Validation Architecture".

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 04-01 (spec, overlay, generic commands) | 01 | 1 | D-01, D-02, D-25 | — | untouched records byte-equal to an unedited save | core + map file | `test-editor-core`, `test-map-files` | ❌ W0 | ⬜ pending |
| 04-01 (registry, markers, verbs, game seam) | 01 | 1 | D-06 | — | — | core + app parser | `test-editor-core`, `test-map-editor-view`, `test-map-editor-auto` | ❌ W0 | ⬜ pending |
| 04-01 (cascade delete, `FindReferences`) | 01 | 1 | D-04, O16 refs | — | refused span/passenger delete leaves document and history unchanged; do/undo/redo restores every reference | core + map file + engine + app | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto` | ❌ W0 | ⬜ pending |
| 04-01 (palette filter) | 01 | 1 | D-05 | — | game types 4, 6, 9 not placeable; `BkEditorAddObject` refuses; loaded objects of those types still load and move | engine + unit | `test-editor-bridge`, `test-map-editor-panels` | ❌ W0 | ⬜ pending |
| 04-01 (render proof, A/B capture) | 01 | 1 | D-09 | — | artefacts in `zig-out/local-test` only | engine (measurement) | `test-editor-bridge` (arnheim capture, `RemoveRiver`, capture again) | ❌ W0 | ⬜ pending |
| 04-02 (roads and rivers) | 02 | 2 | D-07, D-08, D-09, VO5-VO7 | — | `VsoMatchesEngine` after every step; new road passes read-back; nID above all in use; "too short" refused | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `test-map-editor-engine`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-03 (bridges) | 03 | 3 | D-10, D-11, D-12, VO2, VO3, S4 | — | all-or-nothing draw (refusal leaves history unchanged); partner missing refused; every `bridges` link resolves after each step | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-03 (fences) | 03 | 3 | D-14, VO1 | — | tiles outside the map refused; tile mapping equals `ITerrainEditor::GetAITileIndex` | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto` | ❌ W0 | ⬜ pending |
| 04-04 (entrenchments) | 04 | 4 | D-13, VO4, O18 drawing | — | trench plan properties hold over random polylines; every section link resolves; no `AddNewEntrencment` call | core + map file (property) + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-05 (script ID, groups) | 05 | 5 | D-15, D-16, G1 | — | groups written sorted, bytes equal to an unedited save when untouched; duplicates skipped | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-05 (script file) | 05 | 5 | D-20, M6 | — | bare-name validation (separators, `..`, `.lua` suffix, characters); Open script uses a validated path, never a user string | core + engine + game | `test-editor-core`, `test-editor-bridge`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-05 (script areas) | 05 | 5 | D-21, MT2 | — | name rules (empty, duplicate); Vis->AI truncation rule; untouched areas byte-exact | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-05 (camera anchors) | 05 | 5 | D-22, M2 | — | set player N pads with `VNULL3`, never shrinks; no resize on open | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-06 (start commands) | 06 | 6 | D-17, U1, U2 | — | `fromExplosion` kept; empty command erased on cascade; link 0 skipped | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-06 (reserve positions) | 06 | 6 | D-18, U3 | — | towed gun without truck refused by role; refusals leave the map unchanged | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-07 (AI general) | 07 | 7 | D-19, AI1 | — | side creation undone restores the vector size; radius and direction conversions per the MFC store formula | core + map file + engine + app + game | `test-editor-core`, `test-map-files`, `test-editor-bridge`, `map-editor-auto`, `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-08 (preservation sweep) | 08 | 8 | D-25.4 | — | edits plus undo leave file bytes equal to an unedited save; `test-map-files-all` stays 1,755 of 1,755 | map file + engine (`--m2-sweep`, macOS local) | `zig build test-map-files-all -Dtarget=aarch64-macos -Dtest-mode=run -Dcopy-data=false`; `--m2-sweep` | ❌ W0 | ⬜ pending |
| 04-08 (test in game with script and anchor) | 08 | 8 | D-20, D-22 | — | `BkEditorTestMapPath` directory gets the copied script | engine + app + game | `test-editor-bridge`, `map-editor-auto` (`test`, `waitgame`), `map-editor-game-reads-it` | ❌ W0 | ⬜ pending |
| 04-08 (CI, parity evidence, hand try) | 08 | 8 | D-24, D-25, 04-PARITY rows | — | six CI jobs, Windows run on win-home, "Closes with" filled for every row, VO3 wording corrected | CI + manual | `gh workflow run cross-platform.yml` on the branch; full suite command | ✅ | ⬜ pending |

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
| The hand try | D-25.9 | User approval | `--release=fast`, `zig-out/game/macos/arm64/release` per memory |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending

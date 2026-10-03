---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
verified: 2026-10-04T05:00:00Z
status: gaps_found
score: 8/10 must-haves verified
covered_files:
  - .github/workflows/cross-platform.yml
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-01-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-01-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-02-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-02-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-03-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-03-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-04-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-04-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-05-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-05-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-06-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-06-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-07-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-07-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-08-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-08-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-09-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-09-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-10-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-10-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-11-PLAN.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-11-SUMMARY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-CONTEXT.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md
  - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-REVIEW-FIX.md
  - Sources/editor/app/auto.zig
  - Sources/editor/app/c_bridge.zig
  - Sources/editor/app/c_bridge_test.zig
  - Sources/editor/app/commands.zig
  - Sources/editor/app/game_reads_common.zig
  - Sources/editor/app/game_reads_m2.zig
  - Sources/editor/app/game_reads_m3.zig
  - Sources/editor/app/main.zig
  - Sources/editor/app/marker_logic.zig
  - Sources/editor/app/markers.zig
  - Sources/editor/app/minimap.zig
  - Sources/editor/app/panels.zig
  - Sources/editor/app/panels_logic.zig
  - Sources/editor/app/panels_m2.zig
  - Sources/editor/app/panels_m3.zig
  - Sources/editor/app/single_instance.zig
  - Sources/editor/app/smoke.zig
  - Sources/editor/app/testlaunch.zig
  - Sources/editor/app/tool_registry.zig
  - Sources/editor/app/view.zig
  - Sources/editor/app/view_math.zig
  - Sources/editor/core/bridge.zig
  - Sources/editor/core/checks.zig
  - Sources/editor/core/composers.zig
  - Sources/editor/core/editor.zig
  - Sources/editor/core/fake_bridge.zig
  - Sources/editor/core/files.zig
  - Sources/editor/core/filters.zig
  - Sources/editor/core/history.zig
  - Sources/editor/core/layers.zig
  - Sources/editor/core/records.zig
  - Sources/editor/core/rmg.zig
  - Sources/editor/core/root.zig
  - Sources/editor/core/script_file.zig
  - Sources/editor/core/settings.zig
  - Sources/editor/core/tools.zig
  - Sources/editor/core/tools_ai.zig
  - Sources/editor/core/tools_damage.zig
  - Sources/editor/core/tools_fields.zig
  - Sources/editor/core/tools_groups.zig
  - Sources/editor/core/tools_heights.zig
  - Sources/editor/core/tools_vso.zig
  - Sources/editor/imgui/imgui_backend.cpp
  - Sources/editor/imgui/imgui_backend.h
  - Sources/src/A7.sln
  - Sources/src/AILogic/AILogicInternal.cpp
  - Sources/src/AILogic/GeneralInternal.cpp
  - Sources/src/AILogic/RailroadGraph.cpp
  - Sources/src/AILogic/Scripts/Scripts.cpp
  - Sources/src/EditorBridge/bridge.cpp
  - Sources/src/EditorBridge/bridge.h
  - Sources/src/EditorBridge/catalogue.cpp
  - Sources/src/EditorBridge/filters.cpp
  - Sources/src/EditorBridge/session.cpp
  - Sources/src/EditorBridge/session.h
  - Sources/src/EditorBridge/session_fields.cpp
  - Sources/src/EditorBridge/session_groups.cpp
  - Sources/src/EditorBridge/session_layers.cpp
  - Sources/src/EditorBridge/session_records.cpp
  - Sources/src/EditorBridge/session_rmg.cpp
  - Sources/src/EditorBridge/session_terrain.cpp
  - Sources/src/EditorBridge/session_vso.cpp
  - Sources/src/EditorBridge/world.h
  - Sources/src/Formats/Formats.vcxproj
  - Sources/src/Formats/Formats.vcxproj.filters
  - Sources/src/Formats/fmtMap.h
  - Sources/src/Formats/fmtMapScriptPath.h
  - Sources/src/Formats/fmtVSO.h
  - Sources/src/GFXGPU/abi.zig
  - Sources/src/GFXGPU/renderer.zig
  - Sources/src/GameTT/iMissionInternal.cpp
  - Sources/src/Main/CommandsHistory.cpp
  - Sources/src/Main/GameCreation.cpp
  - Sources/src/MapFile/MapFile.cpp
  - Sources/src/MapFile/MapGeometry.cpp
  - Sources/src/MapFile/MapGeometry.h
  - Sources/src/MapFile/MapOverlay.cpp
  - Sources/src/MapFile/MapOverlay.h
  - Sources/src/MapFile/MapRecords.cpp
  - Sources/src/MapFile/MapRecords.h
  - Sources/src/Platform/Paths.cpp
  - Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp
  - Sources/src/RandomMapGen/TerrainGenerator.cpp
  - Sources/src/RandomMapGen/WV_Types.h
  - Sources/src/Scene/SpriteVisObj.cpp
  - Sources/src/Scene/TerrainEditor.cpp
  - build.zig
  - tools/openspy/check_no_gamespy_runtime.ps1
  - tools/zig/composer_roundtrip_test.cpp
  - tools/zig/editor_bridge_test.cpp
  - tools/zig/fixtures/m2_script.lua
  - tools/zig/game_install.ps1
  - tools/zig/map_file_test.cpp
  - tools/zig/random_missions_test.cpp
  - tools/zig/rmg_determinism_test.cpp
  - tools/zig/rmg_record_io.h
  - tools/zig/stage.zig
covered_digest: "v1:sha256:cdce3f9b2ae1e9851fcabb138bdbd25f80772488af580784ace658658fca4c2d"
behavior_unverified: 0
overrides_applied: 3
overrides:
  - must_have: "zig build map-editor-m3-auto passes locally on macOS arm64 and on win-home (D-40.7)"
    reason: "win-home has no GUI over ssh; the Windows GUI legs run in CI or by hand"
    accepted_by: "Johannes (recorded in 05-VALIDATION.md D-40 row 7, 05-PARITY.md final note, memory phase5-user-decisions)"
    accepted_at: "2026-10-03"
  - must_have: "Depth Complexity layer reproduced (PARITY L4, D-32)"
    reason: "GPU renderer has no overdraw counter; the layer stays greyed with the finding as its tip"
    accepted_by: "Johannes"
    accepted_at: "2026-10-03"
  - must_have: "Minimap markers filtered by the palette's object filter (PARITY MM1, D-14)"
    reason: "Accepted as it is"
    accepted_by: "Johannes"
    accepted_at: "2026-10-03"
gaps:
  - truth: "D-40.2: zig build test (core and map-file tiers) passes on all CI targets at the phase tip"
    status: failed
    reason: "CI run 37151247396 at 0a5076cf5 (cross-platform.yml) shows the step 'Map editor unit tier' failed on macos-platform and on windows-platform; linux-platform, linux-arm-platform, macos-intel-platform and windows-mingw-platform passed. The macos-platform log shows one failing test, single_instance.test 'single instance: deinit neither hangs on a replaced socket nor removes the replacement (WR-A02)' at single_instance.zig:1144, the wall-clock assertion elapsed_ms < accept_wait_ms + 3000 (Instance.deinit took 4 s or more on the runner). The test was added by the review fix cc5f86e91, so this is its first CI run; the previous full CI runs (37098890101 at d76e3fadb, 37121998156 at e2e5d7e74) were green on all six jobs. The same test passes locally (791/791 in this verification's own rerun, plus three reruns of test-map-editor-panels, 215/215). The Windows job's log was not retrievable while the run was still in progress, so the Windows failure cause is unconfirmed (presumed, not shown, to be the same test)."
    artifacts:
      - path: "Sources/editor/app/single_instance.zig"
        issue: "WR-A02 test (line 1113) asserts a wall-clock bound; Instance.deinit's accept wait (line 391) counts 10 ms sleep iterations instead of reading the clock, so a loaded runner can overrun the bound. Either the test is a flake on slow runners or deinit can wait much longer than accept_wait_ms; both want a fix or an explicit decision."
    missing:
      - "Read the Windows job log of run 37151247396 (or rerun) and confirm which test failed on windows-platform"
      - "Make the WR-A02 test robust (drop or widen the wall-clock bound, or make deinit's wait clock-based) and get a CI run at the new tip with all six jobs green, including the WR-D05 steps now added to the Windows and macos-14 jobs"
human_verification:
  - test: "D-40.10: Johannes's hand try of M3 on the release build (macOS and Windows), following the checklist in 05-11-SUMMARY.md 'Hand try'"
    expected: "New map, heights, fields, fill, update map, multi-select, properties, links, damage, players, unit creation, check map, layers, minimap click and Create Minimap Images, Create Random Map, the four composers, options, view menu, drag and drop, single instance all behave as the MFC editor did, or better"
    why_human: "Release-build UX approval; a decision about quality, not automatable"
  - test: "Drag a map file from Finder/Explorer onto the editor window; start a second editor from a desktop session with a map path (macOS and Windows)"
    expected: "The map opens in the first editor; the second process exits"
    why_human: "The scripted drop uses the command path; the OS drag gesture and the Windows double launch need a desktop session (recorded as waived for automation, but not hand-tried)"
  - test: "Eyeball the map-editor-m3-auto minimap shots (m3-minimap-before/-after/-game/-heights) and the Windows GPU look"
    expected: "Terrain colours, unit markers, camera frame, patch grid and the height ramp look right"
    why_human: "The shot comparison proves change (14.05% and 11.10% of pixels differ), not visual quality (05-VALIDATION manual-only row)"
---

# Phase 5: Map editor M3 (random map templates, minimap, tools parity) Verification Report

**Phase Goal:** Map Editor M3: random map templates and generation from the editor, minimap tools, and every remaining MFC map editor feature so the portable editor reaches full parity; then delete the MFC map editor from the tree.
**Verified:** 2026-10-04
**Status:** gaps_found
**Re-verification:** No, initial verification

The ROADMAP defines no success criteria for this phase. The requirements of record are CONTEXT D-01..D-40, and the contract is the D-40 exit criteria (05-VALIDATION.md). The ten D-40 criteria are the truths below; D-01..D-39 are covered through the parity checklist and the evidence under each truth.

## Goal Achievement

### Observable Truths

| #   | Truth (D-40) | Status | Evidence |
| --- | ------------ | ------ | -------- |
| 1 | Every row of 05-PARITY.md closed with evidence; NF rows cite dead code | VERIFIED | Parsed the file: 152 rows, 0 empty evidence cells; 16 NF/NF-like rows each carry MFC file:line; M2 rows cite phase 4 (CI 36692345194), M1 rows cite 03-VERIFICATION. Not just the checklist: spot-checked the substance of rows (below) |
| 2 | `zig build test` passes on all CI targets | FAILED | Local: 791/791, rc 0 (my own run). CI at tip 0a5076cf5: macos-platform and windows-platform red in 'Map editor unit tier' (WR-A02 test timing on macOS). See gaps |
| 3 | Engine tier passes on macOS arm64 and Windows-MSVC, with new M3 round trips | VERIFIED (macOS at tip); Windows last shown green at e2e5d7e74 | `05-fix-gate-engine.log`: Build Summary 167/167 steps, 231/231 tests, `map-editor-engine: PASS (260 objects)`, M3 round trips for altitudes, players/unit creation, multi-select group move, minimap, random map, rmg composers/field sets/templates, layers. Windows engine job at the new tip has not run yet (job still in progress); CI 37121998156 was green on all six jobs |
| 4 | Composer round trip: 43 templates, 102 graphs, 404 containers, 27 field sets load/save/reload equal | VERIFIED | `composer-roundtrip: 43 templates, 102 graphs, 404 containers, 27 field sets ok` + PASS in the post-fix engine gate log; WR-D04 pins those counts as lower bounds |
| 5 | Authored template/graph/container/field set + fixed seed generates byte-identical .bzm twice; game loads it under BK_AUTO_UI, clean exit | VERIFIED | `rmg-determinism: authored set byte identical ok (seed 424242, graph 0, angle 0)` and two more variants, PASS; `map-editor: game reads it M3 PASS (... the authored template's map loads: 16 roads, exit 0)` in `05-fix-gate-map-editor-game-reads-it-m3.log` |
| 6 | CreateMiniMapImage from the editor writes the images beside a saved user map; minimap click moves camera, checked by a shot comparison | VERIFIED | m3-auto log frames 460-528: `file_save_bzm`, `minimap_click:8x8`, `expect=minimap_moved`, `differ ... 14.0501% of pixels differ`, `minimap_create`, `expect=minimap_files` (commands.zig:3257 checks all eight files exist, not a stub), heights ramp differ 11.10%. Bridge `BkEditorCreateMiniMapImage` calls `CMapInfo::CreateMiniMapImage` (bridge.cpp:2307) |
| 7 | `map-editor-m3-auto` passes locally on macOS arm64 and on win-home | PASSED (override) | macOS arm64: `BK_EDITOR_AUTO: done (666 actions)`, 14/14, post-review-fix (`05-fix-gate-map-editor-m3-auto.log`). win-home: user-waived 2026-10-03 (no GUI over ssh) |
| 8 | `test-map-files-all` 1,755 maps, 0 FAIL | VERIFIED | `map-file: sweeping 1755 maps` / `1755 of 1755 maps round-tripped` / PASS in the post-fix engine gate log, plus the M3 map-file tier (`altitude region`, `fill`, `field region`, `object fields`, `players`) |
| 9 | MFC editor, MapEditor.exe and every build/packaging/CI/VS Code reference gone | VERIFIED | `Sources/src/MapEditor` and `Sources/src/bin/MapEditor.exe` absent. `git grep 'src/MapEditor\|MapEditor.vcxproj\|Editors/MapEditor'` outside `.planning` and `docs/superpowers/plans` finds one hit: prose in the design spec `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md:5` ("It replaces the MFC Map Editor in `Sources/src/MapEditor`"), a historical statement, not a build reference. Remaining `MapEditor.exe` hits are the portable editor's own exe name. `RandomMapGen` and `Data/Editor` kept |
| 10 | Johannes's hand try on the release build approves M3 | HUMAN | Pending by design |

**Score:** 8/10 truths verified: truths 1, 3, 4, 5, 6, 8, 9 VERIFIED (7) plus truth 7 PASSED (override) = 8. Truth 2 FAILED; truth 10 awaits the human. The other two overrides (Depth Complexity, minimap marker filter) are parity waivers inside truth 1.

### Requirements of record D-01..D-39: spot checks beyond the logs

| Decision | Check | Result |
| -------- | ----- | ------ |
| D-05 szMODName | `MapInfo_StaticMethods_RMGeneration.cpp:975-993` stores `GetGlobalVar("MOD.Name")`/`MOD.Version`, clears the name with no MOD; `editor-bridge: M3 create random map ok` tests it | VERIFIED |
| D-01..D-03 Create Random Map | `BkEditorCreateRandomMap` (bridge.cpp:4937) -> `CMapInfo::CreateRandomMap` (session_rmg.cpp:392) with an `IProgressHook`; File menu item panels.zig:2673; dialog at panels_m3.zig:1118 | VERIFIED |
| D-06..D-12 composers | Tools menu entries for Containers, Graphs, Fields, Templates and Filters Composers (panels.zig:2752-2759); 40+ `BkEditorRmg*` entries in bridge.h; core tests in composers.zig/rmg.zig | VERIFIED |
| D-14..D-17 minimap | see truth 6 | VERIFIED |
| D-18..D-24 | `BkEditorHeightsStroke`, `GenerateHeights`, `SetZeroHeights`, `UpdateMap`, `FillEntireMap`, `NewMap`, `FieldApply*` exist; Fill/Update popups at panels.zig:2301/2321; engine log lines for each | VERIFIED |
| D-25..D-31 | `BkEditorSetLink/Unlink/CanLink`, `DamageObject`, `AddPlayer/DeletePlayer`, `UnitCreation*`, `ObjectFilter*`; CR-C02 link-cycle guard (`NMapRecords::WouldLinkCycle`, session.cpp:1890) | VERIFIED |
| D-32..D-35 | `BkEditorLayers`, `SetLayerShow`; Check Map (panels_m3.zig:945), Options, View, single_instance.zig (16 tests) | VERIFIED |
| D-38 deletion list | see truth 9 | VERIFIED |

### Review fixes (05-REVIEW.md: 6 critical, 29 warnings)

05-REVIEW-FIX.md lists 35 of 35 fixed, one commit each (git log e813d756f..HEAD shows the CR-/WR- commits). Verified the claims that matter: CR-A01 (6f993989c), CR-B01 (6d69f1030), CR-C02 (b0c3c04c5) are in the log and the M3 gates were re-run on the merged tree after them. WR-D05 is in 0a5076cf5. One of the fix commits (cc5f86e91, WR-A02) added the test that is red on CI (see gaps).

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Unit tier | `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` | `32/32 steps succeeded; 791/791 tests passed`, EXIT 0 (the `failed command` and errno 61 stack traces in the log are std noise from a handled connect, same as 05-fix-gate-test.log) | PASS |
| Panels tier, repeated | `zig build test-map-editor-panels ...` x3 | 215/215 each time, including the WR-A02 test | PASS locally |
| Hermeticity | 05-fix-hermeticity.log | 3/3 | PASS |
| Random-missions cover | 05-fix-gate-random-missions.log (155 cases, no summary line: cut short) + 05-fix-gate-random-missions-resume.log (`53 cases, 0 failed`) | 155 + 53 = 208, no failure text in either log | PASS |
| Editor scenarios | smoke (52 steps), auto (13), auto-m2 (298), m3-auto (666), all PASS; auto-m2 passed on rerun after one recorded ImGui-mouse flake under load (`05-fix-gate-map-editor-auto-m2-flake.log` line 140) | PASS |

### Anti-Patterns Found

None blocking. No `TBD`/`FIXME`/`XXX` in any of the 98 surviving source files this phase changed. No `TODO`/`HACK`/placeholder/stub markers in them either (the one `imgui_stub` in build.zig is a test module name).

Informational: the 19 info findings of 05-REVIEW.md stay open by design; deferred-items.md lists real parity gaps left for Johannes (no tile thumbnails in the Fields Composer tabs; the Settings Maps folder does not move a generated map; Browse takes one file where the MFC took several; the Properties window shares an ImGui ID with the docked panel; Depth Complexity greyed; party change does not rename flags). Each is weighed there and none is a PARITY row.

### Probe Execution

No probe scripts (`scripts/*/tests/probe-*.sh`) declared by this phase. SKIPPED.

### Requirements Coverage

No REQUIREMENTS.md IDs are mapped to this phase; D-01..D-40 are the requirements of record (see above). No orphaned requirements.

### Gaps Summary

One gap, and it is narrow. The functional work of the phase is in the code and the local gates are green on the merged tree (unit 791/791 reproduced here, engine tier, bridge, editor scenarios, composer round trip, determinism, game-reads-it, the 1,755-map sweep, the 208-case random-missions sweep). What is not shown is D-40.2 at the tip: the CI run for 0a5076cf5 (still in progress when checked) is red on macos-platform and windows-platform in the 'Map editor unit tier' step, with a timing-bound assertion in the new WR-A02 test failing on macOS. It is most probably a slow-runner flake or a too-tight bound, but it is observed red CI at the tip, and the Windows cause is not confirmed. Close it with a robust test and a green CI run at the new tip; no feature work is needed.

The hand try (D-40.10) is pending and is reported as human_needed, not as a gap. Recorded user-accepted gaps (Depth Complexity greyed, minimap markers not palette-filtered, Windows GUI legs via CI or by hand, script path relative, wheel by delta) were not re-opened.

---

_Verified: 2026-10-04T05:00:00Z_
_Verifier: Claude (gsd-verifier)_

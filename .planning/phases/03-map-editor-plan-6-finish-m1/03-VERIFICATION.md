---
phase: 03-map-editor-plan-6-finish-m1
verified: 2026-09-29T16:16:33Z
status: passed
score: 20/20 roadmap truths verified (19 verified + 1 override); plan-level truths all verified (1 further override); 0 behavior-unverified
covered_files:
  - .github/workflows/cross-platform.yml
  - .gitignore
  - .planning/WINDOWS.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-01-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-01-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-02-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-02-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-03-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-03-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-04-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-04-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-05-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-05-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-06-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-06-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-07-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-07-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-08-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-08-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-09-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-09-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-10-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-10-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-11-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-11-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-12-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-12-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-13-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-13-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-14-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-14-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-15-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-15-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-16-PLAN.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-16-SUMMARY.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-UAT.md
  - Sources/editor/app/auto.zig
  - Sources/editor/app/c_bridge.zig
  - Sources/editor/app/c_bridge_test.zig
  - Sources/editor/app/crt.zig
  - Sources/editor/app/host.zig
  - Sources/editor/app/main.zig
  - Sources/editor/app/panels.zig
  - Sources/editor/app/panels_logic.zig
  - Sources/editor/app/pictures.zig
  - Sources/editor/app/smoke.zig
  - Sources/editor/app/testing/imgui_stub.zig
  - Sources/editor/app/testlaunch.zig
  - Sources/editor/app/view.zig
  - Sources/editor/app/view_math.zig
  - Sources/editor/core/autosave.zig
  - Sources/editor/core/bridge.zig
  - Sources/editor/core/editor.zig
  - Sources/editor/core/fake_bridge.zig
  - Sources/editor/core/files.zig
  - Sources/editor/core/history.zig
  - Sources/editor/core/root.zig
  - Sources/editor/core/settings.zig
  - Sources/editor/core/shipped.zig
  - Sources/editor/imgui/imgui_backend.cpp
  - Sources/editor/imgui/imgui_backend.h
  - Sources/src/AILogic/AILogicInternal.cpp
  - Sources/src/Common/InterfaceScreenBase.cpp
  - Sources/src/EditorBridge/bridge.cpp
  - Sources/src/EditorBridge/bridge.h
  - Sources/src/EditorBridge/catalogue.cpp
  - Sources/src/EditorBridge/session.cpp
  - Sources/src/EditorBridge/session.h
  - Sources/src/Formats/fmtMap.h
  - Sources/src/GFXGPU/GraphicsEngineGpu.cpp
  - Sources/src/Game/GameMain.cpp
  - Sources/src/Game/GameMain.h
  - Sources/src/Game/main.cpp
  - Sources/src/GameTT/iMissionInternal.cpp
  - Sources/src/Platform/Paths.cpp
  - Sources/src/Platform/Paths.h
  - Sources/src/SFX/AudioBackendOpen.cpp
  - Sources/src/SFX/Sound.def
  - Sources/src/Scene/SoundScene.cpp
  - Sources/src/Scene/VisObjBuilder.cpp
  - Sources/src/StreamIO/SeasonData.h
  - build.zig
  - docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md
  - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
  - tools/zig/delete_matching_files.zig
  - tools/zig/editor_bridge_test.cpp
  - tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml
  - tools/zig/game_command_line_test.cpp
  - tools/zig/package.zig
  - tools/zig/platform_paths_test.cpp
  - tools/zig/season_textures.zig
  - tools/zig/sfx_module_test.cpp
  - tools/zig/stage.zig
  - tools/zig/stage_test.zig
covered_digest: "v1:sha256:162a7ef3e5652ff45dad5d04ad2561ff09e0255b0425a58aebf97297fc1711f0"
behavior_unverified: 0
overrides_applied: 2
overrides:
  - must_have: "Camera rotate (roadmap goal) / D-12 free 360 degree yaw; 03-06 truth 3 'If it ships: Alt+Q / Alt+E and the trackpad two-finger rotate turn the camera'"
    reason: "Deferred out of M1 by Johannes at the 03-06 checkpoint on measured evidence (TestYawMeasurement: lower-half black 0.5% at +0, 94.0% at +90, 99.2% at +180 - the terrain quad is laid out in fixed screen space). Recorded in 03-CONTEXT.md <deferred> and in the spec's M1 scope. The measurement and BkEditorSetYaw exist; no input is wired to them (verified: view.zig binds only plain Q/E to object rotation)."
    accepted_by: "Johannes (recorded decision, 03-CONTEXT.md, 03-06-SUMMARY.md)"
    accepted_at: "2026-09-28T00:00:00Z"
  - must_have: "03-08 truth 2: File > Mod ... reloads the object palette and reopens the current map under the new mod"
    reason: "D-26 revised by Johannes in the hand try: switching closes the map (prompt first) because reopening under another object database mixed databases (1359 unknown objects). Recorded in 03-CONTEXT.md D-26 and the spec. The revised behaviour is implemented (switchModClosingMap, performModSwitch) and covered by test-map-editor-panels and the host check's 'mod switch PASS' line."
    accepted_by: "Johannes (recorded decision, 03-CONTEXT.md D-26)"
    accepted_at: "2026-09-29T00:00:00Z"
re_verification:
  previous_status: human_needed
  previous_score: "19/20 roadmap truths (18 verified + 1 override; truth 17 open pending decision; 2 plan-level truths behavior-unverified)"
  gaps_closed:
    - "Truth 17 (plan 5's deferred minors): WINDOWS.md 1-3 fixed in 03-16 (d421402a8, 294bcdf74, 82a08a462); ledger open_count 0"
    - "D-15 per-map camera memory: now tested in view.zig ('view (D-15): ...') and passed UAT 1"
    - "Crash-recovery offer at the next start: passed UAT 2 (kill -9, Johannes 2026-09-29)"
    - "Restart reported the terminated game as a failure: gameExited in .restarting starts the next game and reports nothing (f36c0dc97); the test feeds TerminateProcess code 1, SIGTERM and SIGKILL"
    - "game-reads-it's undeclared empty-ground precondition: a baseline game run counts player 0 first (c55b61f22)"
    - "Windows package never built: CI's windows-platform job builds debug and release package-game-editors and checks both zips (ff22c445c, 5509a62c5); package.zig Windows compile error (3ad03a687) and the stage-game ordering race (a1d7b7a15) fixed on the way"
    - "MapEditor/Game depended on the working directory: installation, Data and shaders now come from SDL_GetBasePath (296118b26, 8ee15cb2e); host check runs from zig-out in CI"
  gaps_remaining: []
  regressions: []
---

# Phase 3: Map editor plan 6: finish M1 - Verification Report

**Phase Goal:** Meet the M1 exit criteria of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (test launch where the game plays the saved map, mods, camera rotate and zoom, safe save with the unsaved-changes prompt, settings and recent files, `BK_EDITOR_AUTO` with shot comparison, the full sweep, packaging `MapEditor` with the game) plus plan 5's "Carried to plan 6" list.
**Verified:** 2026-09-29T16:16:33Z
**Status:** passed
**Re-verification:** Yes. The previous report (b9752f0b8, status human_needed) was followed by gap-closure plan 03-16 and 03-UAT.md.

Requirement IDs (D-*, SPEC-*, M1-EXIT-*, CARRY-*) are not in `.planning/REQUIREMENTS.md`, which is the old port-level document. They were traced against `03-CONTEXT.md` (D-01..D-29 with the D-12, D-26 and D-29 amendments), the spec's "M1 scope" and "Exit criteria for M1", and the "Carried to plan 6" list in `docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md`.

## What changed since the last report

`git diff --name-only b9752f0b8..HEAD` (HEAD 34be3e063) touches 26 files. Every code, CI and build file among them was read for this report. The two changes that most affect the goal:

- **03-16 gap closure.** Status slot with sources, the Restart exit, view.zig tests, literal direction tests, the game-reads-it baseline, the CI package job.
- **Two follow-ups.**
  - The release Windows package race: `addStageGameRun` orders every stage-game run after `game-all` and the shaders.
  - The working-directory dependency. `host.zig` now defaults `data_root` to null, and `bridge.cpp` then uses `SDL_GetBasePath`. `Paths.cpp` `executableRoot()` uses `SDL_GetBasePath` on every OS. `GraphicsEngineGpu.cpp` takes its shaders from `SDL_GetBasePath() + "Shaders/GfxGpu"`; `abi.zig`/`renderer.zig` dupe that string, so the local `std::string` is safe.

Commits after the CI head 8ee15cb2e (c786f0d36, 34be3e063) touch only `03-16-SUMMARY.md`, `03-UAT.md` and this report.

## Goal Achievement

### Observable Truths (roadmap contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Test in game: the game plays the saved or edited map (D-01..D-09) | ✓ VERIFIED | Unchanged path: `startTestGame` -> `launchTestGame` (`testMapPath` -> `saveCopy`, never `editor.save`) -> `gamePath` beside MapEditor -> `testlaunch.start`, which now spawns Game in its own directory (testlaunch.zig:335). Game side (`-editor-test`, profile skip, cloud gate, windowed, help skip) unchanged. game-reads-it: `03-16-tiers.log` and again after the cwd fix `cwd-fix/gri.log` (296118b26): "game reads it PASS (14 units of player 0 near the placed unit, 0 before placing, plus the unit and the squads' 10 soldiers; ... game exit 0)". Real Windows: Johannes edited, test-launched and restarted on win-home (03-UAT.md). |
| 2 | Load a mod's data like the game (`-mod=Name`, `-mod=None`, File > Mod) | ✓ VERIFIED | `BkEditorSetMod`/`LoadDB`, MOD stamping unchanged. CI 36588755990 macOS host step: "mod switch PASS (EditorTestMod -> None -> EditorTestMod; asked first, map closed ...)". Reopen-under-new-mod replaced by close-the-map (override 2). |
| 3 | Camera zoom like the game, bounded, at the pointer, Home reset | ✓ VERIFIED | Engine `ZoomAtScreenPoint` unchanged. The view's wiring is now tested through real SDL events: "Shift + wheel zooms at the pointer instead of panning, Home resets the zoom", "a pinch zooms in whole steps at the pointer ..." (view.zig:1074, 1099), run here (46/46 in map-editor-view-test). |
| 4 | Camera rotate | PASSED (override) | Deferred by Johannes on measured evidence (override 1). `BkEditorSetYaw` + `TestYawMeasurement` exist; no rotate input is wired. |
| 5 | Safe save: temp file, bridge read-back, one `.bak` per session, swap | ✓ VERIFIED | `Editor.save` and `SaveSessionMap` read-back are unchanged since the last report (editor.zig and session.cpp are not in the diff). test-editor-core 86/86 run here. |
| 6 | Unsaved-changes prompt on Open, Quit, window close | ✓ VERIFIED | Unchanged `UnsavedPrompt` wiring (main.zig quit events -> `quit_requested`); panels tests 102/102 run here. |
| 7 | Editor settings and recent files, independent of game profiles | ✓ VERIFIED | `mapeditor.cfg` under the user root; settings.zig round-trip and `pushRecent`/`removeRecent` tests (core 86/86). New: Open Recent stores absolute paths, and a relative Maps folder resolves under the user root (`panels_logic.dialogFolderFor`, tested). |
| 8 | `BK_EDITOR_AUTO` automation with shot comparison | ✓ VERIFIED | `auto.zig` + `AutoRunner` unchanged; test-map-editor-auto 8/8 here; `03-16-tiers.log`: "BK_EDITOR_AUTO: done (13 actions)". Last local run predates the cwd fix (info below). |
| 9 | Full open/save sweep of every shipped map | ✓ VERIFIED | Re-run by this verifier at HEAD: `zig build test-map-files-all -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` -> "sweeping 1755 maps", "1755 of 1755 maps round-tripped", "map-file: PASS", rc=0 (`zig-out/local-test/verify-03-rv/sweep.log`). |
| 10 | Packaging `MapEditor` with the game | ✓ VERIFIED | `--map-editor` wiring unchanged; all three stage-game runs go through `addStageGameRun` (build.zig:2170/2366/2393), which depends on `game_all_step` and the shaders step. stage_test "every stage-game run in build.zig is ordered after game-all and the shaders" and 29 others pass here (30/30). CI 36588755990 windows-platform "Check the Windows packages": debug and release `Blitzkrieg-game.zip` 60220 and `-with-editors.zip` 60224 entries. Each has Game.exe and MapEditor.exe at the root, SeasonData/SeasonTextures.pak and no top-level mods/. The release Game.exe is 3453440 bytes, the same as the release build's own. win-home release build at a1d7b7a15: both zips, 63907/63911 entries, 0 mods. Johannes's real-Windows hand try passed (03-UAT.md). |
| 11 | Carry: object icons (D-29) | ✓ VERIFIED | Unchanged (`BkEditorObjectPicture`, `pictures.zig`, per-frame pump). |
| 12 | Carry: brush outline via world-to-screen | ✓ VERIFIED | Unchanged `RealBridge.worldToScreen` path; `TestWorldToScreenRoundTrip` in the engine tier (CI macOS "Map editor engine tier" and "Engine tier" pass). |
| 13 | Carry: panels follow a window resize | ✓ VERIFIED | panels.zig:577-587 re-places with `ImGuiCond_Always` for the frame the viewport size changes. The hover/release parts of 03-11 are now tested in view.zig ("a pan or stroke whose release went elsewhere ends on the next frame", "a hover over a panel is dropped ..."). |
| 14 | Carry: the map's sound list | ✓ VERIFIED | Unchanged; game-reads-it PASS names "the map's sound Amb_Water_circle started". |
| 15 | Carry: unknown-objects warning | ✓ VERIFIED | CI 36588755990 host steps (Windows and macOS): "unknown-object warning PASS". |
| 16 | Carry: Windows console subsystem | ✓ VERIFIED | `configureMapEditorExecutable(exe, target, .windows)`; CI "Map editor is a GUI program" (PE subsystem 2) success at 8ee15cb2e. |
| 17 | Carry: plan 5's deferred minors | ✓ VERIFIED | `.planning/WINDOWS.md`: open_count 0, fixed_count 3. Each fix was checked in code. (1) `view_math.StatusSlot` with `StatusSource {general, test_launch, frame, dialog}` and `clearFrom`; `TestLaunchPrompt.noteLaunch` maps status_failure / report_failure / started; `startTestGame` reports through it (panels.zig:1182-1186). (2) `View = ViewWith(SdlInput)` (view.zig:74); 19 `test "view..."` blocks drive real `SDL_Event`s; production uses `view_mod.View` in main.zig, panels.zig and smoke.zig. (3) The view_math.zig:319 test asserts the literals 1971.716/2028.284 etc. The other minors were already fixed in the last report. |
| 18 | M1 exit: `install-map-editor` builds on all six CI targets; core and map-file tiers pass | ✓ VERIFIED | `gh run view 36588755990`: headSha 8ee15cb2e941..., event workflow_dispatch on feat/map-editor-plan-6, conclusion success; macos-platform, macos-intel-platform, linux-platform, windows-mingw-platform, linux-arm-platform and windows-platform all success. Windows and macOS each ran the "Map editor unit tier", host, smoke and engine tier steps. Windows also ran both package steps and the package check. |
| 19 | M1 exit: engine, game and editor-app tiers pass locally on macOS arm64 | ✓ VERIFIED | Recorded logs, not re-run here, because those tiers write into `~/.local/share/Nival/Blitzkrieg`. `zig-out/local-test/03-16-tiers.log` (03-16 code, rc=0): editor-bridge PASS, map-editor-engine PASS (260 objects), 3x host check PASS (metal), mod switch PASS, smoke PASS (52 steps), game reads it PASS with baseline, BK_EDITOR_AUTO done (13 actions). After the cwd fix (296118b26): `cwd-fix/gri.log` (game reads it PASS, editor-bridge PASS) and `cwd-fix/engine.log` (map-editor-engine PASS). At 8ee15cb2e, CI's macOS job ran bridge, host check, smoke and engine tier on metal, all PASS. |
| 20 | M1 exit: Johannes's hand try on macOS arm64 | ✓ VERIFIED (human, done) | Approved 2026-09-29 (03-15-SUMMARY). Also 03-UAT.md 2/2 passed, and the real-Windows hand try on win-home (release MapEditor.exe: edit, test launch, Restart without the exit popup, start from outside the install directory). |

**Score:** 20/20 roadmap truths (19 ✓ VERIFIED + 1 PASSED (override)). 0 behavior-unverified.

### Plan must-haves (summary per plan)

| Plan | Must-have truths | Result |
|------|------------------|--------|
| 03-01 `-editor-test` | 5 | Verified (unchanged since last report) |
| 03-02 Test in game | 5 | Verified. The restart path is now tested with the real terminate shapes (the coincidental reliance is resolved) and was tried by hand on Windows. |
| 03-03 Safe save | 4 | Verified |
| 03-04 Prompt, read-only, maps folder | 4 | Verified |
| 03-05 Zoom, D-15, outline | 6 | 6 verified. D-15 now has a behavioural test (view.zig:1189: A panned and zoomed, then B, then A restored in view and engine, also across `closeMap`) and passed UAT 1. |
| 03-06 Rotation measurement | 3 | Measurement + decision verified; "if it ships" N/A (override 1) |
| 03-07 Settings, recent, autosave, recovery | 6 | 6 verified. The recovery offer at the next start passed UAT 2 (kill -9, Open/Discard/Later). |
| 03-08 Mods | 4 | Verified; truth 2 superseded by revised D-26 (override 2) |
| 03-09 Pictures | 4 | Verified |
| 03-10 Sound list | 4 | Verified |
| 03-11 Unknown warning, resize, gestures, minors | 5 | Verified; gesture/hover edge cases now tested in view.zig |
| 03-12 BK_EDITOR_AUTO | 4 | Verified; cursor isolation partial (warning, unchanged) |
| 03-13 Host check / bridge minors | 4 | Verified |
| 03-14 Packaging, console | 4 | Verified, now with a real CI package build and check. The SUMMARY's overstated "verified via CI" is corrected in place (03-16). |
| 03-15 Sweep, CI, tiers, hand try, spec | 5 | Verified |
| 03-16 Gap closure | 6 | 6 verified. Status line (tests "Test in game status: *", "StatusSlot: *"), Restart (the test at panels_logic.zig:2116 feeds code 1, SIGTERM and SIGKILL, and a kept game's code-1 exit is still reported), view.zig wiring tests (19, in `test-map-editor-view` via `view_test_step`, and in CI's unit tier on Windows MSVC and macOS), literal direction test, CI package build and check, game-reads-it baseline (main.zig:636-650 saves the unedited copy and counts before `addObject`; `playerUnitsNear` tested in testlaunch.zig:442). |

### Required Artifacts

`gsd-tools verify.artifacts` over all 16 plans: 35/35 artifacts exist and contain their required patterns (03-16 adds `pub fn ViewWith` in view.zig and `package-game-editors` in cross-platform.yml). New files read for substance: `Sources/editor/app/testing/imgui_stub.zig` (the test-only editor_imgui stand-in, wired only into `view_test_module`) and `tools/zig/package.zig` (Windows `unixMode` via `toAttributes().READONLY`).

### Key Link Verification

The previous 26 hand-checked links were re-checked where their files changed; all still WIRED. New links:

| From | To | Pattern | Status |
|------|----|---------|--------|
| `startTestGame` | `TestLaunchPrompt.noteLaunch` -> `view.status` | panels.zig:1185 | WIRED |
| `pollTestGame` | `gameExited` -> `.start` -> `startTestGame` | panels.zig:1134 | WIRED |
| Restart button | `answer(.restart)` + `running.terminate` (Windows `TerminateProcess(pid, windows_terminate_exit_code)`) | panels.zig:1233-1235, testlaunch.zig:275 | WIRED |
| `test-map-editor-view` | `map-editor-view-test` (view.zig with the sdl3, core and imgui_stub modules) | build.zig:2797, 5933-5952 | WIRED (editor platforms) |
| CI windows/macos jobs | map-editor unit steps | cross-platform.yml:315, 628 | WIRED (both success in 36588755990) |
| package-game / package-game-editors / install-game | `addStageGameRun` (after `game-all` and the shaders) | build.zig:2170/2366/2393, 2876-2896 | WIRED |
| CI windows job | package-game-editors debug + release, zip check | cross-platform.yml:408-470 | WIRED (success) |
| `Host.start` null `data_root` | `BkEditorStart` -> `SDL_GetBasePath` | host.zig:22/45, bridge.cpp:431-437 | WIRED |
| GFXGPU `Init` | `SDL_GetBasePath() + "Shaders/GfxGpu"` -> `setShaderDirectory` (duped) | GraphicsEngineGpu.cpp:284-286, abi.zig:162, renderer.zig:543 | WIRED |
| map-editor-host-check | launched from `zig-out` with a relative map | build.zig `run.setCwd(b.path("zig-out"))` (relative and absolute runs) | WIRED (CI Windows direct3d12 and macOS metal PASS) |
| game-reads-it | `gameReadsItBaseline` before `editor.addObject` | main.zig:636-653 | WIRED |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Real data | Status |
|----------|------|--------|-----------|--------|
| Test copy for the game | map bytes | `RealBridge.saveCopy` of the live session | Yes; the game counts the placed unit over a measured baseline | ✓ FLOWING |
| Status line | message + source | `StatusSlot.set/clearFrom` from edits, launches, frames, dialogs | Yes; rendered by the status bar | ✓ FLOWING |
| Settings window | `scroll_speed` etc. | `mapeditor.cfg` | Yes | ✓ FLOWING |
| Palette pictures | RGBA | `BkEditorObjectPicture` | Yes | ✓ FLOWING |
| Sounds panel | records | `BkEditorSounds` | Yes | ✓ FLOWING |
| Windows packages | zip entries | stage of zig-out after `game-all` | Yes; the release zip carries the release Game.exe | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| View, panels, testlaunch, auto, core, stage unit tests | `zig build test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto test-editor-core test-stage -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run --summary all` | 23/23 steps, 309/309 tests, rc=0. map-editor-view-test 46/46 (19 view.zig tests), view_math 27, panels 102, testlaunch 10, auto 8, core 86, stage 30 (`zig-out/local-test/verify-03-rv/unit.log`) | ✓ PASS |
| Build graph hermeticity (build.zig edited) | `zig test tools/zig/build_hermeticity_test.zig` | 3/3 passed | ✓ PASS |
| Full shipped-map sweep | `zig build test-map-files-all ... -Dtest-mode=run` | 1755 of 1755 round-tripped, PASS, rc=0 | ✓ PASS |
| CI at the code head | `gh run view 36588755990` | success, six jobs success, head 8ee15cb2e (last code commit) | ✓ PASS |
| CI Windows package contents | windows-platform job log, "Check the Windows packages" | 4 zips checked, Game.exe/MapEditor.exe/SeasonData present, release Game.exe = release build | ✓ PASS |
| Engine / game-reads-it / auto tiers | not re-run (they write into the real user cache) | recorded logs `03-16-tiers.log`, `cwd-fix/gri.log`, `cwd-fix/engine.log` read instead | ? SKIP (recorded evidence) |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist or are declared by the phase. Step 7c: SKIPPED.

### Requirements Coverage

| Requirement | Plans | Status | Evidence |
|-------------|-------|--------|----------|
| D-01, D-03, D-06, D-08 | 03-02, 03-14, 03-16 | ✓ SATISFIED | saveCopy, poll loop, Restart without a spurious report (tested with real exit shapes; tried by hand on Windows), `gamePath` beside MapEditor and packaged beside Game.exe |
| D-02, D-04, D-05, D-07 | 03-01, 03-02 | ✓ SATISFIED | game-side test mode; game-reads-it |
| D-09 | 03-02, 03-08 | ✓ SATISFIED | `buildArgv` `-mod=` |
| D-10, D-11, D-13, D-14, D-16 | 03-05 | ✓ SATISFIED | zoom path; view.zig wiring tests |
| D-12 | 03-06 | ✓ SATISFIED as amended (deferred) | override 1 |
| D-15 | 03-05, 03-16 | ✓ SATISFIED | view.zig D-15 test; UAT 1 |
| D-17, D-18, D-23 | 03-04, 03-08 | ✓ SATISFIED | `defaultMapsFolder`, `shipped.zig`, UnsavedPrompt |
| D-19, D-20 | 03-03, 03-07 | ✓ SATISFIED | safe save, autosave |
| D-21, D-22, D-24, D-25 | 03-07 | ✓ SATISFIED | settings/autosave code and defaults |
| D-26 | 03-08 | ✓ SATISFIED as revised | override 2 |
| D-27 | 03-07 | ✓ SATISFIED | `recent_capacity = 10`, order and removal tested in settings.zig; absolute paths since the cwd fix |
| D-28 | 03-08 | ✓ SATISFIED | mods/<F>/maps default, MOD name stamping |
| D-29 | 03-09 | ✓ SATISFIED | shipped icon.tga pictures |
| SPEC-TEST-LAUNCH, SPEC-GAME-READS-IT | 03-01, 03-02, 03-16 | ✓ SATISFIED | game-reads-it PASS with baseline |
| SPEC-SAVE-ERRORS | 03-03, 03-04 | ✓ SATISFIED | failure paths delete temp |
| SPEC-CRASH-RESTORE | 03-07 | ✓ SATISFIED | UAT 2 (kill -9, Open/Discard/Later) |
| SPEC-PRESERVATION | 03-10 | ✓ SATISFIED | sweep 1755/1755 re-run here |
| SPEC-AUTO | 03-12 | ✓ SATISFIED | BK_EDITOR_AUTO |
| M1-EXIT-BUILD, M1-EXIT-TIERS, M1-EXIT-SWEEP, M1-EXIT-HANDTRY | 03-12, 03-14, 03-15 | ✓ SATISFIED | truths 18-20, 9 |
| CARRY-ICONS, CARRY-OUTLINE, CARRY-RESIZE, CARRY-SOUNDS, CARRY-UNKNOWN-WARNING, CARRY-CONSOLE | 03-05, 03-09..03-11, 03-14 | ✓ SATISFIED | truths 11-16 |
| CARRY-PACKAGING | 03-14, 03-16 | ✓ SATISFIED | truth 10: CI builds and checks the debug and release Windows packages |
| CARRY-MINORS | 03-11, 03-12, 03-13, 03-16 | ✓ SATISFIED | WINDOWS.md open_count 0 |

No orphaned IDs: REQUIREMENTS.md maps nothing to this phase.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (all files changed since b9752f0b8, whole-file scan) | - | TBD/FIXME/XXX | none | No debt markers; no TODO/HACK/placeholder in added lines |
| Sources/editor/imgui/imgui_backend.cpp | 46-52 | `bk_imgui_backend_use_global_mouse` cannot disable the backend's private global-mouse fallback | ⚠️ Warning | Unchanged from the last report: synthetic-input isolation is narrowed, not complete (03-12's own admission). CI smoke is green. |
| Sources/editor/app/panels_logic.zig | 1014-1037 | The old game exits after the restart prompt opens but before the user answers: `gameExited` returns to idle, and a later "Restart with this version" is ignored (`answer` only acts in `asking_restart`), so nothing starts | ℹ️ Info | Narrow race. The user presses F5 again. Not a must-have. |
| tools/zig/package.zig / Data size | - | Zip entry count near the 16-bit limit: win-home's release zips hold 63,907/63,911 entries (CI's 60,220/60,224) against 65,535 | ℹ️ Info | Guarded: package.zig refuses more and CI checks the count. A few thousand more Data files would stop packaging until Zip64 is added. |
| Local tier evidence | - | The last local `map-editor-auto` run (17:09, `03-16-tiers.log`) predates the cwd fix (296118b26, 8ee15cb2e) | ℹ️ Info | The changed path (Game spawned in its own directory, shaders from the base path) was exercised afterwards by game-reads-it (`cwd-fix/gri.log`), CI's macOS/Windows host, smoke and engine tiers, and Johannes's Windows test launch. The auto tier's own game runs in the stage root either way. |
| .planning/ROADMAP.md | Phase 3 | "13/15 plans executed"; 03-14 and 03-15 unchecked; 03-16 not listed | ℹ️ Info | Bookkeeping lags; the orchestrator should update it on phase completion |
| Previous human items 4-6 (resize/hover visuals, outline on hills, Open Recent greyed after a restart) | - | Not separately re-run by a human | ℹ️ Info | They were visual confirmations of truths already verified in code. Hover/release are now unit-tested in view.zig, recent order/removal in settings.zig, and the projection in `TestWorldToScreenRoundTrip`. The orchestrator stated nothing needs human testing unless new; not escalated. |

### Human Verification Required

None. The two behavior-unverified items from the last report passed 03-UAT.md. The Windows items were covered by CI's package build and check and by Johannes's real-Windows hand try.

### Gaps Summary

No gaps. Every roadmap truth holds in the code at HEAD. Two rest on Johannes's recorded decisions: camera rotation deferred (override 1) and the D-26 mod switch revised (override 2).

The previous report held the phase at human_needed for three reasons, and all three are resolved:
- **The Broken Windows ledger entries 1-3.** Fixed and tested; open_count 0.
- **D-15 and the crash-recovery offer.** D-15 now has a behavioural test, and both passed UAT.
- **Windows packaging and Test in game had never run.** CI now builds and inspects the debug and release Windows packages. Johannes tried the release MapEditor.exe on a real Windows desktop.

The concrete Restart defect flagged last time is fixed. The fix is tested with the real exit shapes: TerminateProcess code 1, SIGTERM, SIGKILL.

The gap closure also found and fixed three defects:
- a Windows compile error in the package writer;
- a stage-game ordering race that could fail a release package, or zip stale binaries into it;
- a working-directory dependency. It affected the editor on all OSes, and it affected the game's shaders, and on macOS its module roots.

---

_Verified: 2026-09-29T16:16:33Z_
_Verifier: Claude (gsd-verifier)_

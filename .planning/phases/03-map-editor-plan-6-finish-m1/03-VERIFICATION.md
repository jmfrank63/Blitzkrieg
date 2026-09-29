---
phase: 03-map-editor-plan-6-finish-m1
verified: 2026-09-29T09:22:26Z
status: passed
score: 19/20 roadmap truths verified (18 verified + 1 override; 1 open pending decision; 2 plan-level truths present, behavior-unverified)
covered_files:
  - .github/workflows/cross-platform.yml
  - .gitignore
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
  - .planning/phases/03-map-editor-plan-6-finish-m1/03-CONTEXT.md
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
  - tools/zig/platform_paths_test.cpp
  - tools/zig/season_textures.zig
  - tools/zig/sfx_module_test.cpp
  - tools/zig/stage.zig
  - tools/zig/stage_test.zig
covered_digest: "v1:sha256:70e8abd1dbd81180057f6a2c0da6f7b072c45c8d726d059f026a4e33c74dab8e"
behavior_unverified: 2
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
behavior_unverified_items:
  - truth: "D-15: reopening a map in the same session restores its camera and zoom"
    test: "Open map A, scroll and zoom in 2-3 steps, open map B, then reopen map A (File > Open Recent)"
    expected: "A comes back at the scroll position and zoom it was left at; a map opened for the first time shows its middle unzoomed"
    why_human: "View.showMap/saveCurrentView (view.zig) is present and wired, but view.zig has no tests and no smoke or tier opens two maps and checks the restored camera; 03-05-SUMMARY marks this coverage 'status: unknown'. The hand-try checklist (03-15) did not include the two-map flow."
  - truth: "Spec Errors -> Crashes: at the next start, recovery copies are offered (Open / Discard / Later)"
    test: "Set autosave to 1 minute, edit a shipped map (not Saved As), wait for the recovery copy in ~/.local/share/Nival/Blitzkrieg/mapeditor/recovery, kill the editor (kill -9), start it again"
    expected: "A modal names the map and offers Open, Discard, Later; Open restores the edits, Discard deletes the copy, Later asks again next start"
    why_human: "scanRecoveryOffers (panels.zig) and the modal are present and wired, and the smoke proves the autosave writes the recovery copy, but no test starts the editor over an existing recovery folder; automated modes deliberately skip the scan. Not in the approved hand-try checklist."
coincidental_reliance_items:
  - truth: "D-06: Restart with this version waits for the old game's exit, then starts the new one"
    reason: fixture-only
    harden: "The unit test feeds gameExited a clean exit (code 0, no signal) for the terminated game. In production the terminated game exits via TerminateProcess(pid, 1) on Windows (code 1) or SIGTERM/SIGKILL on POSIX, which describe() classifies as a failure, so a Restart will also pop 'The game exited with code ...' (signal exits even print code 0). Mark a terminate-requested exit as expected in TestLaunchPrompt and test with the real exit shape."
  - truth: "map-editor-game-reads-it: the placed unit is counted for player 0"
    reason: undeclared-precondition
    harden: "The check is units(player 0 within radius 5) >= 1 + squad soldiers, with no baseline count taken before placing. It holds only because the chosen coldwinter ground has no player-0 units of its own. Count once on the unedited map (or place far from all map units and assert the delta)."
human_verification:
  - test: "Decide the three open Broken Windows ledger entries (.planning/WINDOWS.md 1-3, plan-5 carried minors): (1) the status line is never cleared after a later success (panels.zig); (2) no test of view.zig's event-to-tool wiring; (3) the scroll-direction test restates its own constants, no engine-tier ScreenToWorld direction check"
    expected: "Each entry is either fixed (a small follow-up plan) or waived with a reason via gsd-tools windows waive; ledger open_count reaches 0"
    why_human: "The phase goal includes plan 5's deferred minors; these three are observably still open (entry 1: startTestGame's success path never clears a previous 'test in game: ...' status; view.zig has 0 tests; view_math.zig:401-414 derives expectations from scroll_speed/sqrt2 and editor_bridge_test.cpp has no direction check). Whether they must be fixed for M1 or waived is Johannes's decision. /gsd-ship is blocked while open_count > 0."
  - test: "D-15: open map A, scroll and zoom in 2-3 steps, open map B, then reopen map A"
    expected: "A comes back where it was left; a first-time map opens on its middle unzoomed"
    why_human: "Behavior-unverified: View.showMap/saveCurrentView are wired but no test opens two maps and checks the restored camera"
  - test: "Crash recovery: with autosave at 1 minute, edit a shipped map, wait for the recovery copy, kill -9 the editor, start it again"
    expected: "Open / Discard / Later names the map; Open restores the edits"
    why_human: "Behavior-unverified: scanRecoveryOffers is wired but no test starts the editor over an existing recovery folder"
  - test: "Resize the MapEditor window wider and narrower; drag the map with the middle button and release over a panel; hover a panel with the brush tool"
    expected: "The right-hand panels stay on the right edge and the columns follow the height; the map stops following after the release; the status bar's tile and the brush outline disappear over the panel"
    why_human: "03-11's deferred <human-check> (CARRY-RESIZE); 03-11-SUMMARY records these as human_judgment. The approved 03-15 hand-try checklist does not contain them."
  - test: "Brush tool at radius 0 and 4 over hills and flat ground, zoomed out and in"
    expected: "The drawn outline sits on the cells a click paints"
    why_human: "03-05's deferred <human-check> (CARRY-OUTLINE). TestWorldToScreenRoundTrip proves the projection; the on-screen fit on sloped ground is visual. Not an explicit hand-try step (step 1 paints tiles but does not ask about the outline on hills)."
  - test: "File > Open Recent after opening three maps, quitting and restarting; rename one of them on disk"
    expected: "Newest first; the renamed one shows greyed with Remove"
    why_human: "03-07's deferred <human-check> (D-27). Hand-try step 6 opened Open Recent but did not cover restart order or the greyed/Remove state."
  - test: "On Windows x64: zig build package-game (and package-game-editors), then unzip -l the packages; start the packaged MapEditor.exe and press F5 twice, choosing Restart"
    expected: "MapEditor.exe sits beside Game.exe at the package root with no top-level mods/; MapEditor opens no console; Test in game starts Game.exe windowed; Restart replaces the running game (note whether a spurious 'exited with code 1' report appears)"
    why_human: "No CI job runs any package step (grep of cross-platform.yml: none). The Windows packaging is verified only through the shared build.zig wiring, stage.zig's own test-stage run on Windows and CI's PE-header GUI-subsystem check; 03-14-SUMMARY's 'verified via CI's windows-platform job' for package-game-editors overstates this. 03-15-SUMMARY itself lists the Windows Test-in-game path as never exercised on a real Windows desktop."
---

# Phase 3: Map editor plan 6: finish M1 - Verification Report

**Phase Goal:** Meet the M1 exit criteria of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (test launch where the game plays the saved map, mods, camera rotate and zoom, safe save with the unsaved-changes prompt, settings and recent files, `BK_EDITOR_AUTO` with shot comparison, the full sweep, packaging `MapEditor` with the game) plus plan 5's "Carried to plan 6" list.
**Verified:** 2026-09-29T09:22:26Z
**Status:** human_needed
**Re-verification:** No, initial verification

Requirement IDs (D-*, SPEC-*, M1-EXIT-*, CARRY-*) are not in `.planning/REQUIREMENTS.md`, which is the old port-level document. They were traced against `03-CONTEXT.md` (D-01..D-29 with the D-12, D-26 and D-29 amendments), the spec's "M1 scope" and "Exit criteria for M1", and the "Carried to plan 6" list in `docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md`.

## Goal Achievement

### Observable Truths (roadmap contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Test in game: the game plays the saved or edited map (D-01..D-09) | ✓ VERIFIED | `testlaunch.buildArgv` emits `-editor-test -profile=MapEditorTest -mod=<F>/None -windowed [-monitorN] mapeditor_test.bzm`. `panels.startTestGame` goes `testMapPath` -> `saveCopy` (never `editor.save`, D-01) -> `gamePath` (beside MapEditor, D-08) -> `testlaunch.start`. Game side: `main.cpp:187` parses it; `GameMain.cpp:509-537` skips `profiles/active.cfg` read and write; `CloudProviderSelected` returns false under `Editor.TestLaunch` (all 6 gate sites funnel through it); `:2291` forces windowed after all args; `InterfaceScreenBase.cpp:788` skips first-visit help. Tier log `map-editor-game-reads-it.log` / `03-15-c2-tiers2.log`: "game reads it PASS (14 units of player 0 ... game exit 0)", and the run checks the document stayed dirty at its original path. |
| 2 | Load a mod's data like the game (`-mod=Name`, `-mod=None`, File > Mod) | ✓ VERIFIED | `BkEditorSetMod` runs `LoadDB` (bridge.cpp); saves stamp `szMODName/szMODVersion` from the active mod only when one is active (bridge.cpp:1896-1903). Host check: "mod switch PASS (EditorTestMod -> None -> EditorTestMod; asked first, map closed ...)". Reopen-under-new-mod was replaced by close-the-map (override 2). |
| 3 | Camera zoom like the game, bounded, at the pointer, Home reset | ✓ VERIFIED | `ZoomAtScreenPoint` (bridge.cpp:108) clamps to `[0, GetMaxZoomSteps]`, keeps the world point under the pointer via `GetPos3` before/after + `SetAnchor`, then `ResetPosition`. view.zig: Shift+wheel -> `ZoomWheel` -> `zoomAt`; `SDL_EVENT_PINCH_*` -> `PinchZoom`; `SDLK_HOME` and View > Reset view -> `resetView`. Smoke steps "Shift + wheel zooms in at the pointer" pass (03-05-SUMMARY). |
| 4 | Camera rotate | PASSED (override) | Deferred by Johannes on measured evidence (override 1). `BkEditorSetYaw` + `TestYawMeasurement` exist; no rotate input is wired. |
| 5 | Safe save: temp file, bridge read-back, one `.bak` per session, swap | ✓ VERIFIED | `Editor.save` (editor.zig:164-244): refuse shipped -> delete stale temp -> `bridge.saveMap(temp)` -> first-write `.bak` copy tracked in `backed_up` -> `files.rename(temp, path)`, deleting the temp on every failure. `SaveSessionMap` re-reads and `AreEquivalent`s (session.cpp:366). Core tests pass (see spot-checks). |
| 6 | Unsaved-changes prompt on Open, Quit, window close (Save / Don't save / Cancel) | ✓ VERIFIED | main.zig:407 routes `SDL_EVENT_QUIT`/`WINDOW_CLOSE_REQUESTED` to `quit_requested`; `UnsavedPrompt` state machine; modal `drawUnsavedPrompt` with the three buttons; Save on a shipped/new/recovery map goes via `needsSaveAs` to Save As. Also hand-try step 5. |
| 7 | Editor settings and recent files, independent of game profiles | ✓ VERIFIED | `<UserRoot>mapeditor/mapeditor.cfg` (main.zig:340); read/written only when `!automated` and interactive; `settings.zig` defaults autosave on, 2 min, `recent_capacity = 10`; Settings window slider drives `view.wheel_sensitivity`. |
| 8 | `BK_EDITOR_AUTO` automation with shot comparison | ✓ VERIFIED | `auto.zig` parser + `compareTga` (size check, channel tolerance 24), `AutoRunner.runCompare` seeds a missing reference; `map-editor-auto` step runs paint/place/saveas/shot/compare/test/waitgame/exit. Tier log: "BK_EDITOR_AUTO: done (13 actions)". Local-only by design; cursor isolation is partial (warning below). |
| 9 | Full open/save sweep of every shipped map | ✓ VERIFIED | Re-run by this verifier: `zig build test-map-files-all -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` -> "sweeping 1755 maps", "1755 of 1755 maps round-tripped", "map-file: PASS", rc=0. |
| 10 | Packaging `MapEditor` with the game | ✓ VERIFIED (warning) | build.zig:2378/2409 pass `--map-editor` + `addFileArg(editor_exe.getEmittedBin())` to both package stages on the editor platforms; stage.zig copies by base name and `verifyStagedPayload` requires it; stage_test "--map-editor stages the file beside the game" and "a mods directory in the repository is never staged" run in CI on Linux, Windows and macOS; fixture mod installs only into the stage root, never into `package/`. macOS `package-game` was run for real (03-14). A Windows package was never built anywhere (human item). |
| 11 | Carry: object icons (D-29) | ✓ VERIFIED | `BkEditorObjectPicture` reads `<szPath>\icon.tga`; `pictures.zig` cache; panels.zig requests on group open and pumps with a per-frame budget (1775-1780); hand-try step 8. |
| 12 | Carry: brush outline via world-to-screen | ✓ VERIFIED | `RealBridge.worldToScreen` -> `BkEditorWorldToScreen`, used by view.zig:500 for the outline and :419 for sound markers; `TestWorldToScreenRoundTrip` in the engine tier. On-screen fit on slopes routed to human. |
| 13 | Carry: panels follow a window resize | ✓ VERIFIED (visual pending) | panels.zig:579-587 re-places with `ImGuiCond_Always` for the frame after the viewport size changes, keeping dragged widths. Only exercised by compiling into the smoke/host-check frame path; on-screen check routed to human. |
| 14 | Carry: the map's sound list | ✓ VERIFIED | `BkEditorSounds/AddSound/SetSound/DeleteSound` over `snapshot.sounds.sounds`; history `sound_add/sound_edit/sound_delete`; Sounds panel "Add at view centre"; game now plays MapSounds (iMissionInternal.cpp +17). game-reads-it checks the sound registered, started, and a loop starts once. |
| 15 | Carry: unknown-objects warning | ✓ VERIFIED | `summarizeUnknown` + `drawUnknownObjectsPrompt` ("N objects of M types are kept unchanged ... not shown", per-type counts). Host check prints "unknown-object warning PASS". |
| 16 | Carry: Windows console subsystem | ✓ VERIFIED | `configureMapEditorExecutable(exe, target, .windows)` for MapEditor, `.console` for the engine test (build.zig:5881/6011); `crt.attachParentConsole` for headless modes; CI step "Map editor is a GUI program" reads the PE header (subsystem 2) and the windows-platform job is green. |
| 17 | Carry: plan 5's deferred minors | ? OPEN (decision pending) | Most are fixed with evidence (e.g. catalogue sort is `std.sort.block`, `Tga.pixel` bounds-checked in main.zig, `CaptureNextFrame(false)` on both failure paths, `message_len` removed, host check honours `-Dtest-mode`). Three remain open in `.planning/WINDOWS.md` (entries 1-3), confirmed still true in code. Escalated for decision (human item 1). |
| 18 | M1 exit: `install-map-editor` builds on all six CI targets; core and map-file tiers pass | ✓ VERIFIED | `gh run view 36543562810`: headSha `af00ffa810ec...` (= `af00ffa81`), conclusion success, all six jobs success. HEAD `b3077230d` adds only planning docs on top. |
| 19 | M1 exit: engine, game and editor-app tiers pass locally on macOS arm64 | ✓ VERIFIED | `zig-out/local-test/03-15-c2-tiers2.log` (at `71bd19230`): `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)`, 3x host check + panel smoke PASS, `mod switch PASS`, `smoke PASS (52 steps)`, `game reads it PASS`, `BK_EDITOR_AUTO: done (13 actions)`. The commits after it touch only a configure-time guard in build.zig, a buffer rename in the bridge test and the spec. Not re-run here: those tiers write the test copy into the real `~/.local/share/Nival/Blitzkrieg/cache/generated/MapEditorTest`. |
| 20 | M1 exit: Johannes's hand try on macOS arm64 | ✓ VERIFIED (human, done) | Approved 2026-09-29 on the release build after eleven gap fixes (03-15-SUMMARY "Hand-try checklist", "Result: approved"). Not re-listed here. |

**Score:** 19/20 roadmap truths (18 ✓ VERIFIED + 1 PASSED (override); truth 17 open pending decision). 2 plan-level truths (D-15, recovery offer) are present but behavior-unverified and are not counted as verified.

### Plan must-haves (summary per plan)

| Plan | Must-have truths | Result |
|------|------------------|--------|
| 03-01 `-editor-test` | 5 | All verified in code (profile block, cloud gate, windowed forcing, help skip, `units=` verb used by game-reads-it) |
| 03-02 Test in game | 5 | Verified; restart path has a fixture-only test (coincidental reliance); Windows path unexercised |
| 03-03 Safe save | 4 | Verified (editor.zig, session.cpp read-back, core tests) |
| 03-04 Prompt, read-only, maps folder | 4 | Verified; `shipped.zig` generalised the rule after the hand try |
| 03-05 Zoom, D-15, outline | 6 | 5 verified; D-15 per-map memory ⚠ PRESENT_BEHAVIOR_UNVERIFIED |
| 03-06 Rotation measurement | 3 | Measurement + decision verified; "if it ships" N/A (override 1) |
| 03-07 Settings, recent, autosave, recovery | 6 | 5 verified; recovery offer at next start ⚠ PRESENT_BEHAVIOR_UNVERIFIED |
| 03-08 Mods | 4 | Verified; truth 2 superseded by revised D-26 (override 2) |
| 03-09 Pictures | 4 | Verified; the neutral-frame fallback per D-29 amendment |
| 03-10 Sound list | 4 | Verified; the list is `sounds.sounds` (spec amended in 03-10), not `soundsList` |
| 03-11 Unknown warning, resize, gestures, minors | 5 | Code verified; visual items routed to human; minors partly open (truth 17) |
| 03-12 BK_EDITOR_AUTO | 4 | Verified; cursor isolation partial (warning) |
| 03-13 Host check / bridge minors | 4 | Verified (`test_mode == .run` gating, probe colour, capture disarm) |
| 03-14 Packaging, console | 4 | Verified on macOS and by wiring/tests for Windows; real Windows package unbuilt (human item) |
| 03-15 Sweep, CI, tiers, hand try, spec | 5 | Verified (sweep re-run here, CI checked via gh, spec carries the decisions) |

### Required Artifacts

`gsd-tools verify.artifacts` over all 15 plans: 33/33 artifacts exist and contain their required patterns (`-editor-test`, `Editor.TestLaunch`, `pub fn buildArgv`, `BkEditorTestMapPath`, `map-editor-game-reads-it`, `pub const Files`, `AreEquivalent`, `UnsavedPrompt`, `BkEditorZoomAt`, `ZoomWheel`, `TestZoomStepsBoundedAndAnchored`, `BkEditorSetYaw`, `TestYawMeasurement`, `pub fn parse`, `pub fn due`, `BkEditorSetMod`, `MODName`, `BkEditorObjectPicture`, `pub const Pictures`, `BkEditorSoundRecord`, `sound_edit`, `summarizeUnknown`, `staleGesture`, `TestEntryPointsBeforeAMap`, `--map-editor`, `AttachConsole`, `-editor-test` in the spec). Each was also read for substance (see truths).

### Key Link Verification

The tool could not parse the descriptive `from:` fields (all reported "Source file not found"), so every link was checked by hand with its pattern:

| From | To | Pattern | Status |
|------|----|---------|--------|
| GameMain.cpp ProcessCommandLine | `Editor.TestLaunch` readers | `Editor\.TestLaunch` (GameMain 4, InterfaceScreenBase 1) | WIRED |
| GameMain profile block | profiles/active.cfg | `bEditorTest` (7) | WIRED |
| panels.zig F5 / menu | `testlaunch.start` | 2 hits, via `startTestGame` | WIRED |
| `BkEditorTestMapPath` | `NProfile::GeneratedDirectory` + `ModKey` | bridge.cpp:1957 | WIRED |
| `Editor.save` | `files.rename` | editor.zig:238 | WIRED |
| main.zig | `StdFiles` | 5 hits | WIRED |
| main.zig quit events | `quit_requested = true` | main.zig:407 | WIRED |
| panels dialogs | `defaultMapsFolder` | 2 hits | WIRED |
| bridge StartRenderer/Resize | `GFX.World.BaseSize` | 3 hits | WIRED |
| `BkEditorZoomAt` | ApplyZoomStep recipe | `ResetPosition` in `ZoomAtScreenPoint` | WIRED |
| view.zig Shift+wheel | `zoomAt` | view.zig:312/341 | WIRED |
| `BkEditorSetYaw` | `SetSessionCamera` | `fYawOffsetDegrees` in bridge+session | WIRED (unbound to input, by decision) |
| main.zig | mapeditor.cfg | main.zig:340 | WIRED |
| autosave tick | save / saveCopy | `panels.tickAutosave` only when interactive | WIRED |
| `BkEditorSetMod` | `LoadDB` | 3 hits | WIRED |
| `BkEditorSaveMap` | `szMODName/Version` | bridge.cpp:1896-1902 | WIRED |
| palette group open | `pictures.request/pump` | 4 hits | WIRED |
| `BkEditorObjectPicture` | `icon.tga` | 4 hits | WIRED |
| editor.zig sounds | bridge vtable | 17 hits | WIRED |
| `BkEditorSetSound` | `snapshot.sounds.sounds` | bridge.cpp:1830-1870 | WIRED |
| `State.mapOpened` | unknown-objects modal | `summarizeUnknown` | WIRED |
| main.zig `BK_EDITOR_AUTO` | `smoke.AutoRunner` | main.zig:263-307 | WIRED |
| build.zig host check | `test_mode` | `test_mode == .run` | WIRED |
| package stages | stage.zig `--map-editor` | build.zig:2378/2409 | WIRED |
| `configureMapEditorExecutable` | `exe.subsystem` | build.zig:5881 (.windows), 6011 (.console) | WIRED |
| `test-map-files-all` | MapFile Read/Write/AreEquivalent | build.zig step, `--all` | WIRED (run here) |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Real data | Status |
|----------|------|--------|-----------|--------|
| Test copy for the game | map bytes | `RealBridge.saveCopy` of the live session snapshot | Yes; game counts the placed unit | ✓ FLOWING |
| Settings window | `scroll_speed` etc. | `mapeditor.cfg` via `readSettingsFile` | Yes; drives `view.wheel_sensitivity` | ✓ FLOWING |
| Palette pictures | RGBA | `BkEditorObjectPicture` from data storage | Yes | ✓ FLOWING |
| Sounds panel | records | `BkEditorSounds` over `snapshot.sounds.sounds` | Yes | ✓ FLOWING |
| Unknown-objects modal | types/counts | `summarizeUnknown(document.objects)` | Yes | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Core, view, panels, testlaunch, auto unit tests | `zig build test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false` | 15/15 steps, 190/190 tests passed, rc=0 (`zig-out/local-test/verify-03/unit.log`) | ✓ PASS |
| Full shipped-map sweep | `zig build test-map-files-all ... -Dtest-mode=run` | 1755 of 1755 round-tripped, PASS, rc=0 (`zig-out/local-test/verify-03/sweep.log`) | ✓ PASS |
| CI at the tested head | `gh run view 36543562810` | success, six jobs success, head af00ffa81 | ✓ PASS |
| Engine / game-reads-it / auto tiers | not re-run (they write into the real user cache) | recorded log `03-15-c2-tiers2.log` read instead | ? SKIP (recorded evidence) |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist or are declared by the phase. Step 7c: SKIPPED.

### Requirements Coverage

Every ID in the 15 PLAN `requirements:` fields is accounted for:

| Requirement | Plans | Status | Evidence |
|-------------|-------|--------|----------|
| D-01, D-03, D-06, D-08 | 03-02, 03-14 | ✓ SATISFIED | saveCopy, poll-driven loop, TestLaunchPrompt, `gamePath` beside MapEditor; packaged beside Game |
| D-02, D-04, D-05, D-07 | 03-01, 03-02 | ✓ SATISFIED | game-side test mode; game-reads-it |
| D-09 | 03-02, 03-08 | ✓ SATISFIED | `buildArgv` `-mod=` from `state.modFolder()` |
| D-10, D-11, D-13, D-14, D-16 | 03-05 | ✓ SATISFIED | zoom path above |
| D-12 | 03-06 | ✓ SATISFIED as amended (deferred) | override 1 |
| D-15 | 03-05 | ? NEEDS HUMAN | behavior-unverified |
| D-17, D-18, D-23 | 03-04, 03-08 | ✓ SATISFIED | `defaultMapsFolder`, `shipped.zig`, UnsavedPrompt |
| D-19, D-20 | 03-03, 03-07 | ✓ SATISFIED | safe save, autosave into the map file |
| D-21, D-22, D-24, D-25 | 03-07 | ✓ SATISFIED | settings/autosave code and defaults |
| D-26 | 03-08 | ✓ SATISFIED as revised | override 2 |
| D-27 | 03-07 | ✓ SATISFIED (visual pending) | `recent_capacity = 10`; greyed/Remove visual routed to human |
| D-28 | 03-08 | ✓ SATISFIED | mods/<F>/maps default, MOD name stamping |
| D-29 | 03-09 | ✓ SATISFIED | shipped icon.tga pictures, neutral frame |
| SPEC-TEST-LAUNCH, SPEC-GAME-READS-IT | 03-01, 03-02 | ✓ SATISFIED | game-reads-it PASS |
| SPEC-SAVE-ERRORS | 03-03, 03-04 | ✓ SATISFIED | failure paths delete temp, status names the step |
| SPEC-CRASH-RESTORE | 03-07 | ? NEEDS HUMAN | recovery offer behavior-unverified |
| SPEC-PRESERVATION | 03-10 | ✓ SATISFIED | sweep 1755/1755; sounds edited only through their own list |
| SPEC-AUTO | 03-12 | ✓ SATISFIED | BK_EDITOR_AUTO |
| M1-EXIT-BUILD, M1-EXIT-TIERS, M1-EXIT-SWEEP, M1-EXIT-HANDTRY | 03-12, 03-14, 03-15 | ✓ SATISFIED | truths 18-20, 9 |
| CARRY-ICONS, CARRY-OUTLINE, CARRY-RESIZE, CARRY-SOUNDS, CARRY-UNKNOWN-WARNING, CARRY-PACKAGING, CARRY-CONSOLE | 03-05, 03-09..03-11, 03-14 | ✓ SATISFIED | truths 10-16 |
| CARRY-MINORS | 03-11, 03-12, 03-13 | ? OPEN | 3 ledger entries pending decision |

No orphaned IDs: REQUIREMENTS.md maps nothing to this phase.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (all phase-changed files) | - | TBD/FIXME/XXX | none | Scan of the 56 changed non-planning files found no debt markers; added lines carry no TODO/HACK/placeholder |
| Sources/editor/app/panels_logic.zig | 983-991 | `gameExited` reports any non-clean exit, including the one a Restart caused | ⚠️ Warning | On Windows (`TerminateProcess(pid, 1)`) a Restart will also show "The game exited with code 1 ..."; the test covering it uses a clean-exit fixture |
| Sources/editor/app/panels.zig | 1177-1215 | `startTestGame` success path does not clear an earlier "test in game: ..." status | ⚠️ Warning | Concrete instance of ledger entry 1 |
| Sources/editor/imgui/imgui_backend.cpp | 46-52 | `bk_imgui_backend_use_global_mouse` cannot disable the backend's private global-mouse fallback | ⚠️ Warning | Synthetic-input isolation is narrowed, not complete (03-12's own admission); CI smoke is green |
| .planning/ROADMAP.md | 219, 276, 280 | "13/15 plans executed"; 03-14 and 03-15 unchecked | ℹ️ Info | Bookkeeping lags the code; the orchestrator should update it on phase completion |
| .planning/phases/03-.../03-14-SUMMARY.md | 46, 140 | "verified via CI's windows-platform job" for package-game-editors | ℹ️ Info | No CI job runs a package step; the claim overstates the evidence |
| Sources/src/RandomMapGen/MapInfo_StaticMethods*.cpp | 1944-2000, 148, 204 | `rand() * n / (RAND_MAX + 1)` int overflow | ℹ️ Info | Known, deferred to backlog 999.1 with its determinism rule; not in this phase's scope |

### Human Verification Required

The approved M1 hand try (03-15) is not repeated here. These items are outside its checklist or are decisions:

1. **Decide the three open ledger entries** (`.planning/WINDOWS.md` 1-3). Fix them in a small follow-up plan or waive each with a reason. They block `/gsd-ship` while `open_count > 0`.
2. **Per-map camera memory (D-15).** Open A, zoom, open B, reopen A: A comes back as left.
3. **Crash recovery offer at the next start.** Kill the editor with a recovery copy on disk and restart: Open / Discard / Later names the map.
4. **Resize, middle-button release over a panel, hover over a panel** (03-11). The panels follow the edge, the pan stops, the hover tile and outline go away.
5. **Brush outline on hills** at radius 0 and 4, zoomed in and out (03-05).
6. **Open Recent after a restart and a renamed file** (03-07). Newest first, the missing file greyed with Remove.
7. **Windows**: build `package-game` / `package-game-editors` and check `MapEditor.exe` beside `Game.exe` with no top-level `mods/`; press F5 twice and choose Restart on a real Windows desktop.

### Gaps Summary

No must-have is FAILED. The code for the whole M1 goal is present, substantive and wired. The shipped-map sweep (re-run here, 1755/1755), the unit tiers (190/190, run here), CI run 36543562810 (checked via `gh`, six jobs green) and the recorded local engine/game/editor-app tier log all support it. Camera rotation and the "reopen under the new mod" behaviour differ from the roadmap and plan wording, and both rest on Johannes's recorded decisions (overrides 1 and 2).

The phase is not `passed` for three reasons:
- Three of plan 5's carried minors, which the phase goal names, are still open and waiting for Johannes's fix-or-waive decision.
- Two state transitions (per-map camera memory and the crash-recovery offer) have no test and were not in the hand-try checklist.
- The Windows package and the Windows Test-in-game path have never actually run.

One concrete defect found while reading the code is worth fixing along with ledger entry 1. On Windows, choosing Restart will almost certainly also pop the "The game exited with code 1" report, because `TestLaunchPrompt.gameExited` does not tell a terminate it requested apart from a crash. The unit test hides this by feeding a clean exit.

---

_Verified: 2026-09-29T09:22:26Z_
_Verifier: Claude (gsd-verifier)_

## Resolution (2026-09-29)

- Human verification: both behavior-unverified items passed in 03-UAT.md (D-15 camera restore; crash-recovery offer after kill -9).
- Plan-5 leftovers (WINDOWS.md 1-3), the Restart exit popup and the game-reads-it baseline: fixed in gap-closure plan 03-16.
- Windows: release package builds (a1d7b7a15, CI builds and checks it); MapEditor finds its installation from any working directory (296118b26, 8ee15cb2e); Johannes's real-Windows hand try passed.
- CI run 36588755990 green on all six jobs.
- Status set to passed.

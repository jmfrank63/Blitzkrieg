---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 13
subsystem: map-editor
tags: [map-editor, m2, exit-criteria, game-reads-it, bk-editor-auto, preservation-sweep, ci, win-home, release, parity, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 12
    provides: every M2 feature (04-01..04-12), the M2 game-reads-it harness and BK_MAP_TRACE, the map-editor-auto-m2 schedule
provides:
  - map-editor-game-reads-it-m2 with everything D-25.5 lists on one map; its PASS line names every check and the path of the game's shot
  - map-editor-auto-m2 extended to the full exit run: a compare after each of its 16 shots, the width-mode-All slider leg, script_choose on the saved user map, save, a Save As copy-along leg on a user map, Test in game with BK_MAP_TRACE, waitgame and the test_game_script predicate
  - commands script_choose, script_beside, test_game_script and vso_width_mode; BK_EDITOR_AUTO_GAME_TRACE
  - test-map-files-m2-sweep and test-editor-bridge-m2-sweep (local steps)
  - 04-PARITY.md and 05-PARITY.md M2 rows closed, spec "Exit criteria for M2", 04-VALIDATION.md signed off
affects: [05-map-editor-m3-random-map-templates-minimap-tools-parity]

commits: 2
plan_head_before: e9ee6a95b59bbc2c889f514626fb568c91820bbf
actuals:
  tokens: 31000
  tasks: 4
  commits: 2

tech-stack:
  added: []
  patterns:
    - "A sweep plans every edit on one fresh read, capturing the before and after records by value, and applies them to other fresh reads: the baseline write and the undone write each come from their own read (the SVertexAltitude padding rule)"
    - "The engine sweep counts a refusal (a crowded map, a garrisoned trench) and does not fail on it: a refusal must change nothing, which the byte comparison at the end proves anyway"
    - "A do= argument is at most 64 characters, so a fixture path is made relative to the stage root in build.zig (one ../ per component of stage_root)"
    - "A scripted Save As makes its folder; stale files a scenario would trip over (a script beside the saved map) are deleted by delete-matching-files before the run"
    - "PowerShell piped over ssh with -Command - runs a line as it arrives, so a piped script keeps every statement on one line (a multi-line block silently ends the script)"

key-files:
  created:
    - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-13-SUMMARY.md
  modified:
    - Sources/editor/app/game_reads_m2.zig
    - Sources/editor/app/commands.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/smoke.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/tool_registry.zig
    - Sources/editor/core/script_file.zig
    - Sources/editor/core/tools_vso.zig
    - tools/zig/map_file_test.cpp
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
    - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-PARITY.md
    - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-VALIDATION.md
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md

key-decisions:
  - "Save As offers to bring the script along for any map whose script is beside it, not only a shipped one, and not when the new map is in the same folder (the script is already beside it). A user map saved elsewhere loses its script just like a shipped one; this is what the orchestrator's copy-along leg on a user map needed"
  - "In the width mode All the Roads & Rivers width and opacity sliders re-width the selected line, one undo step per slider drag (a gesture begun when the slider is taken hold of): the MFC CVSOState::Update CW_ALL behaviour, cheap on the existing setVsoWidth/setVsoOpacity(.all)"
  - "The road and a W_WoodenBig_Heavy bridge rotated across it are placed 600 world units right and 100 up of the anchor (the open snow in the lower right of the game's 1440x900 shot); the river and the built-during-play bridge stay at the far point"
  - "script_choose takes a path relative to the working directory (the stage root) because a do= argument is at most 64 characters (T-04-03-01 bound kept); build.zig derives ../../../../../ from stage_root"
  - "The test game of map-editor-auto-m2 runs with BK_MAP_TRACE (BK_EDITOR_AUTO_GAME_TRACE=1) so test_game_script can read its own report that the copied script ran; M1's map-editor-auto is unchanged"
  - "--m2-sweep runs the sweep alone in both test executables; the regular runs are unchanged and the sweeps stay out of the default test step and CI"
  - "The four newest tools get readable palette labels (Script Areas, Start Target, Reserve Positions, AI General); the tool= words are the ToolId names and did not change"
  - "win-home builds with build.zig's default MSVC paths (VS 18 Insiders, which vswhere does not list without -prerelease), -Dtarget=x86_64-windows-msvc as in CI"

requirements-completed: [D-24, D-25, D-01, D-04, D-12, D-20, D-22, D-23]

coverage:
  - id: D1
    description: "D-25.5: one editor-made map with a new road, a river, two bridges (one rotated, one built during play), an entrenchment, fences, group 900 with the unit carrying 4245, a start command, a reserve position, a side-1 parcel, m2_area, player 0's anchor and m2_script beside it is loaded by the game under BK_AUTO_UI, shot, and exits cleanly; the trace shows the script ran and found the area and the group, the camera starts at the anchor, the shot shows the road and the bridge"
    requirement: "D-25"
    verification:
      - kind: e2e
        ref: "zig build map-editor-game-reads-it-m2 (debug and --release=fast): game reads it M2 PASS (camera at player 0's anchor 2172,2172, source=player; baseline 3843,579; roads 3 -> 4, rivers 0 -> 1, bridges 0 -> 2 (one rotated, one built during play), fences +10, entrenchments 0 -> 1, group 900 held 1, startcmd launched 0 -> 1, reserve applied 0 -> 1, general side 1 parcels 0 -> 1 (parcel type=1 r=256 dir=0), script m2_script ran (loaded=1 init=1), area m2_area found at 3072,3072 (lua 3072,3072), script group 4245 landed one unit at count 2, both games exited 0; shot .../map-editor-game-reads-it-m2.log.edited.rgba)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-13-rel-game-edited.png (release) and 04-13-t1-game-edited.png (debug), inspected: the rotated W_WoodenBig_Heavy_02 bridge crosses the new rail road in the lower right; the fence run, the L-shaped trench, the three Flak38 and the truck round the anchor; the release shot also shows 'Reinforcements Have Arrived'"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-25.6: map-editor-auto-m2 draws one of each, undoes and redoes, saves, compares its shots against local references, runs Test in game with the script copied along, and quits"
    requirement: "D-25"
    verification:
      - kind: e2e
        ref: "zig build map-editor-auto-m2, run twice (seed, then compare): BK_EDITOR_AUTO: done (298 actions); 16 compares 0.0000-0.0215 % (limit 1 %); script_choose, expect=script_beside:m2_script, save, Save As into map-editor-auto-m2-along, script_copy_along_yes, test, waitgame=240, expect=test_game_script:m2_script (the test game's BK_MAP_TRACE: script name=\"m2_script\" loaded=1 init=1)"
        status: pass
    human_judgment: false
  - id: D3
    description: "D-25.4: test-map-files-all 1755 of 1755; the M2 sweeps edit every Data/Maps map and undo it byte-exact"
    requirement: "D-25"
    verification:
      - kind: integration
        ref: "zig build test-map-files-all: map-file: 1755 of 1755 maps round-tripped, map-file: PASS (19.6 s)"
        status: pass
      - kind: integration
        ref: "zig build test-map-files-m2-sweep: map-file: M2 sweep 59 maps, 460 edits, all restored byte-exact (also on win-home, Windows-MSVC)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge-m2-sweep: editor-bridge: M2 sweep 57 maps, 238 edits, all restored byte-exact (both sweeps with their builds 4 min 44 s)"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-25.1-3 and D-25.7: the core, map-file and engine tiers pass locally and all six CI jobs are green; win-home builds and passes the non-GUI tiers"
    requirement: "D-25"
    verification:
      - kind: integration
        ref: "local suite (14 steps, 5 min 56 s, rc 0): map-file: PASS, editor-bridge: PASS, map-editor-engine: PASS (260 objects), smoke PASS (52 steps), panel smoke PASS, game reads it PASS, game reads it M2 PASS, BK_EDITOR_AUTO: done (13 and 298 actions)"
        status: pass
      - kind: integration
        ref: "CI run 36692345194 (01d03487b), see the CI section"
        status: pass
      - kind: integration
        ref: "zig-out/local-test/04-13-win-home.log: test-editor-core 226/226, app tiers 163/165 (2 skipped), map-file: 66 of 66 and map-file: PASS, map-file: M2 sweep 59 maps, 460 edits, all restored byte-exact, install-map-editor built MapEditor.exe and Game.exe; 04-13-win-home-package.log: install-map-editor package-game-editors --release=fast rc 0 in 499 s, two zips of 3.12 and 3.14 GB"
        status: pass
    human_judgment: false
  - id: D5
    description: "D-25.9 as amended: the agent-run scripted walk-through on the --release=fast build, every M2 shot and the game's shot inspected"
    requirement: "D-25"
    verification:
      - kind: other
        ref: "zig build map-editor-auto-m2 map-editor-game-reads-it-m2 --release=fast (rc 0, 2 min 1 s): the walk-through table below"
        status: pass
    human_judgment: true
    rationale: "Trackpad feel, the OS opening a .lua and the GPU look on Windows are not observable by an agent; they are listed under 'Not observable by an agent'"
  - id: D6
    description: "D-25.8: every M2 row of 05-PARITY closed with evidence, VO3 reads 'built during play', 04-PARITY's Plan column names the renumbered plans; spec Exit criteria for M2; 04-VALIDATION signed off"
    requirement: "D-25"
    verification:
      - kind: other
        ref: "grep: 'Exit criteria for M2' in the spec; 'built during play' in 05-PARITY.md; nyquist_compliant: true in 04-VALIDATION.md; no TBD or planning-time evidence left in the 04-PARITY M2 rows"
        status: pass
    human_judgment: false

duration: 1h25m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 13: Integration and exit Summary

**One editor-made M2 map carries everything D-25.5 lists. The real game plays it and reports every item back, and its shot shows the new road under the rotated bridge. The full editor-app scenario passes on debug and release: 16 shots compared, a script chosen and carried along a Save As on a user map, and a Test in game that runs it. Both new preservation sweeps restore every edit of every Data/Maps map byte for byte. The branch builds and passes its non-GUI tiers on win-home, the release package builds there, and every M2 parity row is closed with evidence.**

## Performance

- **Duration:** about 1 h 25 min (08:18Z to 09:43Z), much of it waiting on CI (42 min) and win-home
- **Tasks:** 4 (one tracer, three auto)
- **Files:** 16 changed

## Accomplishments

- **The exit run (tracer).**
  - `game_reads_m2.zig` puts the new road, and a W_WoodenBig_Heavy bridge rotated across it, in the start view. The PASS line now lists every check and the path of the game's shot.
  - `map-editor-auto-m2` compares every shot. It chooses the script beside the saved user map, saves, and brings the script along a Save As into a second folder. Test in game then runs the script, and the test game's own trace says so.
- **The four orchestrator follow-ups for the editor**, all closed:
  - trench visibility: explained with code evidence and shown in a capture;
  - copy-along on a user map;
  - the sliders in the width mode All;
  - the trace assertions.
- **Preservation sweeps.**
  - `--m2-sweep` in the map-file test: 59 maps, 460 record edits.
  - `--m2-sweep` in the bridge test: 57 maps, 238 engine edits, 22 refusals counted.
  - Both restore the unedited bytes.
- **CI, win-home, release walk-through, parity, spec and validation**, below.

## Task Commits

1. **Task 1: the M2 exit run (tracer)**, `d18feda52` (feat)
2. **Task 2: the preservation sweeps**, `01d03487b` (test)
3. **Tasks 3 and 4: CI, win-home, release walk-through; parity, spec, validation**. The docs commit that carries this file. Task 3 changed no source: CI, win-home and the release run all passed on `01d03487b`. `commits: 2` counts the two task commits before this file was written (the ledger's measure).

Tracer gate: Task 1's `<verify>` is automated-only. Both runs passed before Task 2 began: the game-reads-it PASS line, then the second auto run with every compare under 1 % and `done (298 actions)`. "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `map-editor-game-reads-it-m2` (debug) | PASS line above, rc 0 (2 min 56 s) |
| `map-editor-auto-m2` twice | seed, then all 16 compares 0.0000-0.0175 %, `done (298 actions)` |
| `test-map-files-all` | `1755 of 1755 maps round-tripped`, `map-file: PASS` |
| `test-map-files-m2-sweep test-editor-bridge-m2-sweep` | `map-file: M2 sweep 59 maps, 460 edits, all restored byte-exact`; `editor-bridge: M2 sweep 57 maps, 238 edits, all restored byte-exact`; 0 FAIL |
| Full local suite (the plan's 14 steps) | rc 0 in 5 min 56 s: `map-file: 66 of 66`, `map-file: PASS`, `editor-bridge: PASS` with all 22 `M2 ... ok` lines, `map-editor-engine: PASS (260 objects)`, `smoke PASS (52 steps)`, `panel smoke PASS`, `game reads it PASS`, `game reads it M2 PASS`, M1 `compare edited` 0.0324 %, `done (13 actions)` and `done (298 actions)` |
| Release (`--release=fast`) | `map-editor-auto-m2 map-editor-game-reads-it-m2` rc 0 in 2 min 1 s on the stage `zig-out/game/macos/arm64/release` (Game 2,932,392 bytes, MapEditor 5,559,528 bytes); compares 0.0000-0.0215 % |

The sweeps' edits by kind:
- **map-file:** script file 59, camera anchor 59, road resampled 58, object script ID 57, river resampled 41, group member added 39, AI mobile script ID added 38, start command number 38, script area renamed 32, AI parcel moved 21, reserve position moved 18. CVSOBuilder refused 9 lines' descriptors, and those lines were moved as stored instead.
- **engine:** bridge drawn 57, fence run drawn 57, entrenchment drawn 57, cascade delete 37, shipped bridge deleted 23, shipped entrenchment deleted 7. Refused, and recorded without failing: 22 shipped entrenchments (units in them) and 1 cascade candidate.

## CI

**Run 36692345194** ("Cross-platform validation", `feat/map-editor-m2` at `01d03487b`) succeeded on all six jobs. It ran from 08:51:32Z to 09:33:23Z.

| Job | Conclusion | Took | Tier lines in its log |
|---|---|---|---|
| linux-platform | success | 1 min 24 s | `map-file: PASS` |
| linux-arm-platform | success | 1 min 36 s | `map-file: PASS` |
| macos-intel-platform | success | 2 min 46 s | `map-file: PASS` |
| windows-mingw-platform | success | 2 min 1 s | none (the map-file tier is not built on MinGW, as before) |
| macos-platform | success | 23 min 29 s | `map-file: PASS`, `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)` |
| windows-platform (MSVC) | success | 41 min 45 s | `map-file: PASS`, `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)` |

The engine tier ran only on macOS arm64 and Windows-MSVC. The other four jobs do not schedule it, so none of them reports a pass it did not run. The core tier (`test-editor-core`) ran on all six. The four short jobs restored their build caches.

## win-home (Windows-MSVC, the repo clone only, no GUI launch)

The clone `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg` was fetched and checked out at `feat/map-editor-m2`, fast-forwarded to `01d03487b`. `tasklist` showed no MapEditor or Game running. The target was `-Dtarget=x86_64-windows-msvc` with build.zig's default MSVC paths, which are this machine's VS 18 Insiders. Logs: `zig-out/local-test/04-13-win-home.log` here, and the same file under the clone's `zig-out\local-test`.

| Step | Result |
|---|---|
| `test-editor-core` | rc 0, 32 s, `226/226 tests passed` |
| `test-map-editor-panels test-map-editor-auto test-map-editor-testlaunch` | rc 0, `163/165 tests passed (2 skipped)`. One skip is testlaunch.zig's POSIX signal test, which skips on Windows. |
| `test-map-files` | rc 0, 71 s, `map-file: 66 of 66 maps round-tripped`, `map-file: PASS` |
| `test-map-files-m2-sweep` | rc 0, 23 s, `map-file: M2 sweep 59 maps, 460 edits, all restored byte-exact` (the same edits by kind as on the Mac) |
| `install-map-editor` (build only) | rc 0, 194 s: debug MapEditor.exe (31,252,480 bytes) and Game.exe (22,221,824 bytes) |
| `install-map-editor package-game-editors --release=fast` (orchestrator follow-up 5) | rc 0, 499 s, `114/114 steps succeeded; 14/14 tests passed`. Two zips: Blitzkrieg-game.zip (3,115,780,329 bytes, 63,911 entries) and Blitzkrieg-game-with-editors.zip (3,136,050,359 bytes, 63,915 entries). Both hold Game.exe and MapEditor.exe (release: 3,452,928 and 6,751,232 bytes) and no test fixture (`m2_script` 0). |

The first attempt piped a script with multi-line statements. PowerShell's `-Command -` stopped after the MSVC lookup, and vswhere found nothing because the install is a prerelease. The script was rewritten as one-line statements on build.zig's default paths, and the second run is the one above.

## Release walk-through (D-25.9 as amended: replaces the hand try)

- `df -h .` gave 12 GiB free, above the 10 GiB rule, so the debug stage was kept.
- Command: `zig build map-editor-auto-m2 map-editor-game-reads-it-m2 --release=fast -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run`, rc 0. Log: `zig-out/local-test/04-13-t3-release.log`.
- Release stage: `zig-out/game/macos/arm64/release`.
- Each shot was converted to PNG (`zig-out/local-test/04-13-rel-*.png`, the TGAs with sips, the game's raw RGBA with ffmpeg) and looked at.

| Feature | Shot | What is visible | Result |
|---|---|---|---|
| Camera anchors | m2_anchor | The Camera anchors panel lists Neutral unset, players 0-3 at the map's values and player 4 at 2172, 2172. The view stands at player 0's anchor over the tank rows. | pass |
| Roads & Rivers | m2_roads | A three-point river (a dark water ribbon) selected with square control points and circular width handles, lighter after the opacity right-drag. The new road, outlined in orange, is at the top. The panel shows River, `defaultriver`, width 3, opacity 100, Single point, and "river 0: ID 19367, 3 points". | pass |
| Bridge | m2_bridges | The W_WoodenBig_Heavy_02 bridge (rotated, 4 spans) marked blue as built during play, with the green outline and orange span frames of the selection. The panel shows "bridge 0: W_WoodenBig_Heavy_02, 4 spans" and Built during play checked. | pass |
| Fences | m2_fences | A run of W_FactoryFence along the world x axis between the bridge and the tank rows, and the Fences type list with pictures. | pass |
| Entrenchment | m2_trench | The L-shaped trench drawn for player 0 in green outline, selected. The panel shows "entrenchment 0, player 0: 7 pieces in 2 sections". | pass |
| Groups | m2_groups_marked, m2_groups | Marked: the tank with script ID 4244 outlined in magenta. The status line reads "1 object carries one of group 0's script IDs; the first is selected", and Properties shows Script ID 4244. After the undos the Groups window says "the map has no reinforcement groups". | pass |
| Script areas and script file | m2_areas | The m2_area rectangle (yellow, selected, handles shown) and the m2_ring circle (cyan) with their names. The Script window shows "Script: m2_script", the warning "m2_script.lua is not beside the map" (correct for the shipped coldwinter), and the shipped .lua names beside it. | pass |
| Start commands | m2_startcmds, m2_startcmds_panel | The red line from the tank to its MOVE_TO target point, and the Start Commands window. | pass |
| Reserve positions | m2_reserve | A towed 10.5-cm_Flak38 and a Sdkfz_8 near the river, with the orange gun, truck and place line. The panel lists "0: 10.5-cm_Flak38 + Sdkfz_8, at 5033, ...". | pass |
| AI general | m2_ai_general | The reinforce parcel as a yellow ring with its centre handle, direction line and arrow handle, and a selected reinforce point with its arrow. The panel shows side 1, mobile ID 4245 with Remove, "0: reinforce, 4.0 tiles, 0 deg", and the wrapped selection lines. | pass |
| Final | m2_final | The saved state after the four AI undos: every marker kind of the earlier shots, readable labels on the four newest palette buttons (Script Areas, Start Target, Reserve Positions, AI General), and the AI General panel empty for side 1. | pass |
| The game | game (04-13-rel-game-edited.png) | The rotated wooden bridge crosses the new rail road in the lower right. The L-shaped trench, the fence run, three Flak38 and the truck are round the lit area at the anchor. The messages "Game saved: Mission Start Auto.sav" and "Reinforcements Have Arrived" are shown; the second is group 900's unit landed by `LandReinforcement(900)`. | pass |

### Not observable by an agent

| Item | Why | Automated evidence that covers it as far as it goes |
|---|---|---|
| Trackpad feel of the double-click finish and Ctrl-click as right-click (D-07, D-13) | Real input cannot be produced from the agent shell. | `dblclick=`, `rclick=` and `rpress=` in map-editor-auto-m2 drive the same SDL events, and view.zig's view-events tests cover Ctrl-as-right. |
| "Open script" opening an editor through the OS (D-20) | OS file association. | `openUrl` builds `file://` plus the resolved folder plus a validated name (script_file.zig tests). The scripted runs check the URL, not the desktop. |
| The GPU look of roads and animated rivers on Windows (D-09) | No desktop session over ssh. | Windows-MSVC CI runs the engine tier, including TestM2VsoRendersOnGpu's numbers. win-home builds the release package. |

## The orchestrator's follow-ups

1. **04-08's trench not in the game's shot.**
   - **Proven cause: fog of war.** `CEntrenchmentPart::Init` starts every piece with `bVisible = false`. `CEntrenchmentPart::Segment` (`Sources/src/AILogic/Entrenchment.cpp:51-70`) calls `pFullEntrenchment->SetVisible()` only once `theWarFog.IsTileVisible( tile, theDipl.GetMyParty() )` holds for one of its covered tiles. Only `SetVisible` sends `ACTION_NOTIFY_NEW_ST_OBJ` and `ACTION_NOTIFY_NEW_ENTRENCHMENT` to the client (lines 114-121). Fences and bridges are sent regardless, which is why they showed.
   - In 04-08 the map held no unit of player 0 near the anchor, so the whole view was fog-grey and the trench never reached the client (`zig-out/local-test/04-08-game-edited.png`).
   - Since 04-09 the scenario puts player 0's units beside the anchor, so their sight lights a circle round it. The trench is drawn in every shot since: `04-13-t1-game-edited.png` and `04-13-rel-game-edited.png`, left of the flag.
   - No code change.
2. **The Save As copy-along on a user map.**
   - The question was asked only for a shipped map. Now any Save As asks when the map names a script that is beside it and the new folder differs (`panels.zig` `offerScriptCopyAlong`; `script_file.sameFile` made public).
   - map-editor-auto-m2 runs the leg on its own user map `zig-out/local-test/map-editor-auto-m2/m2.bzm`, whose script was put there by `script_choose`. It saves as into `zig-out/local-test/map-editor-auto-m2-along/`, answers `script_copy_along_yes` and checks `script_beside:m2_script`.
   - The following Test in game ran the copied script.
3. **The Roads & Rivers sliders on a selected line.**
   - In the width mode All, the width and opacity sliders re-width or re-fade every key point of the selected line. A slider drag is one undo step: its gesture begins when the slider is taken hold of.
   - `vso_width` and `vso_opacity` do the same, one step each, and `vso_width_mode` sets the mode.
   - Tested in tools_vso.zig and in the scenario's roads segment.
   - The other modes still need a key point, which only a drag names; the MFC Update did the same only in CW_ALL.
4. **The trace lines in the final game-reads-it-m2 run.** Each is asserted by `game_reads_m2.zig`: a missing line or a wrong count is a FAIL. The release run printed:
   - `BK_MAP_TRACE: group id=900 held=1`
   - `entrenchments n=1` (baseline 0)
   - `reserve applied=1` (baseline 0)
   - `general side=1 parcels=1 mobile=0`
   - `parcel side=1 idx=0 type=1 cx=1536 cy=1536 r=256 dir=0`
   - plus the camera, terrain, bridges, area, startcmd and script lines.
5. **win-home.** The unit tiers, the headless tiers, a build of MapEditor and the game, and the release package build all ran there, with no GUI launch (above).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The four newest tools showed their identifiers in the palette**
- **Found during:** Task 1 (the first `m2_final` shot: "script_areas", "start_target", "reserve_positions", "ai_general" beside "Roads & Rivers")
- **Fix:** the labels are "Script Areas", "Start Target", "Reserve Positions" and "AI General". `tool=` uses the ToolId names, which did not change.
- **Side effect:** the M1 frame compares at 0.03 %. The first M2 references were seeded before the fix, so they were set aside as `zig-out/local-test/04-13-m2-reference-before-labels` and seeded again.
- **Files modified:** `tool_registry.zig`
- **Commit:** `d18feda52`

**2. [Rule 2 - Missing functionality] Save As of a user map never offered to bring its script along (orchestrator follow-up 2)**
- **Fix:** see follow-up 2 above.
- **Files modified:** `panels.zig`, `panels_m2.zig` (comment), `script_file.zig`
- **Commit:** `d18feda52`

**3. [Rule 2 - Missing functionality] The panel's width and opacity sliders did not re-width the selected line in mode All (orchestrator follow-up 3)**
- **Fix:** see follow-up 3 above.
- **Files modified:** `tools_vso.zig`, `commands.zig`, `panels_m2.zig`, `panels.zig`
- **Commit:** `d18feda52`

**4. [Rule 3 - Blocking] One format call takes at most 32 arguments**
- **Found during:** Task 1, the first build of the PASS line with 35 arguments.
- **Fix:** the counts are formatted into a buffer first, then printed in the PASS line.
- **Commit:** `d18feda52`

**5. [Rule 3 - Blocking] The scenario needed things the plan did not list**
- The copy-along folder had to exist and stale scripts had to go, or a second run would meet Choose other's "Replace it?".
- **Fix:**
  - a scripted `saveas=` makes its folder;
  - build.zig deletes `m2_script*.lua` in both scenario folders before the run;
  - `BK_EDITOR_AUTO_GAME_TRACE` passes `BK_MAP_TRACE` to the test game so a predicate can prove the script ran.
- **Files modified:** `smoke.zig`, `main.zig`, `build.zig`
- **Commit:** `d18feda52`

### Plan wording resolved

- **`do=script_choose:<repo>/tools/zig/fixtures/m2_script.lua`:** an absolute path is over the 64-character argument bound. The argument is relative to the working directory, and build.zig writes `../../../../../tools/zig/fixtures/m2_script.lua`.
- **The plan's final segment** (`saveas`, `script_choose`, `expect`, `save`, `shot`, `compare`, `test`, `waitgame`, `exit`) runs as written. Around it:
  - `script_file:none` comes first, because the 04-10 segment had already named m2_script and a put of the same name records nothing;
  - the copy-along leg comes before `test`, so Test in game runs the script that was carried along;
  - `expect=test_game_script` comes after `waitgame`.
- **"compares its shots":** every one of the 16 shots has a `compare=` right after it, not only `m2_final`.
- **Engine sweep refusals:** a trench with units in it cannot be deleted (04-08). 22 such shipped trenches were refused and counted, and 7 others were deleted and undone.
- **"skipped (never passed) elsewhere":** the other four CI jobs do not schedule the engine tier at all. The parity file and the spec say that, rather than "skipped".
- **Commit trailer:** `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`, the model that ran this plan.

---

**Total deviations:** 5 auto-fixed (one Rule 1, two Rule 2, two Rule 3), plus wording resolutions.
**Impact on plan:** no scope change. Deviations 2 and 3 are the orchestrator's follow-ups.

## Issues Encountered

- The first win-home attempt: a multi-line PowerShell script piped to `-Command -` stops at its first multi-line statement, and vswhere does not list a prerelease Visual Studio. Fixed as above.
- CI job logs could be fetched only after the whole run completed, with `gh run view --job <id> --log`; the raw API returned nothing earlier.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- M2's exit criteria are met; the phase can go to verification.
- The M2 local references are under `zig-out/local-test/map-editor-auto-m2/reference/`. They are never committed, and the first run on another machine seeds them.
- For M3 (phase 5): the O17/O19 Script ID part and the O16/O18 M2 parts are closed. The rest of each row stays open in `05-PARITY.md`.

## Threat surface

No new surface beyond the plan's model.
- T-04-13-01: only `feat/map-editor-m2` was pushed, never forced.
- T-04-13-02: the win-home scripts `Set-Location` to the clone and write only its `zig-out\local-test`. `D:\GOG\Blitzkrieg` is not named, and nothing GUI was launched (`install-map-editor` built, it did not run).
- T-04-13-03: `df -h` gave 12 GiB free before the release build, and no package was built on the Mac.
- T-04-13-04: every criterion names its command, printed line or run id.
- The new `script_choose` copies a file only through Choose other's existing `copyInto` (a bare name, the map's own folder, refused for a shipped map). `test_game_script` only reads the test game's log.

## Self-Check: PASSED

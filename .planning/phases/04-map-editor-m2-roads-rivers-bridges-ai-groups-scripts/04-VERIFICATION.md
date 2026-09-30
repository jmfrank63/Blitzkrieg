---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
verified: 2026-09-30T15:30:00Z
status: passed
score: 12/12 must-haves verified
covered_files:
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-01-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-01-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-02-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-02-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-03-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-03-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-04-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-04-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-05-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-05-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-06-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-06-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-07-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-07-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-08-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-08-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-09-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-09-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-10-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-10-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-11-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-11-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-12-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-12-SUMMARY.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-13-PLAN.md
  - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-13-SUMMARY.md
  - Sources/editor/app/auto.zig
  - Sources/editor/app/c_bridge.zig
  - Sources/editor/app/c_bridge_test.zig
  - Sources/editor/app/commands.zig
  - Sources/editor/app/game_reads_common.zig
  - Sources/editor/app/game_reads_m2.zig
  - Sources/editor/app/main.zig
  - Sources/editor/app/marker_logic.zig
  - Sources/editor/app/markers.zig
  - Sources/editor/app/panels.zig
  - Sources/editor/app/panels_logic.zig
  - Sources/editor/app/panels_m2.zig
  - Sources/editor/app/smoke.zig
  - Sources/editor/app/testlaunch.zig
  - Sources/editor/app/tool_registry.zig
  - Sources/editor/app/view.zig
  - Sources/editor/app/view_math.zig
  - Sources/editor/core/bridge.zig
  - Sources/editor/core/editor.zig
  - Sources/editor/core/fake_bridge.zig
  - Sources/editor/core/files.zig
  - Sources/editor/core/history.zig
  - Sources/editor/core/records.zig
  - Sources/editor/core/root.zig
  - Sources/editor/core/script_file.zig
  - Sources/editor/core/tools.zig
  - Sources/editor/core/tools_ai.zig
  - Sources/editor/core/tools_groups.zig
  - Sources/editor/core/tools_vso.zig
  - Sources/src/AILogic/AILogicInternal.cpp
  - Sources/src/AILogic/GeneralInternal.cpp
  - Sources/src/AILogic/Scripts/Scripts.cpp
  - Sources/src/EditorBridge/bridge.cpp
  - Sources/src/EditorBridge/bridge.h
  - Sources/src/EditorBridge/catalogue.cpp
  - Sources/src/EditorBridge/session.cpp
  - Sources/src/EditorBridge/session.h
  - Sources/src/EditorBridge/session_groups.cpp
  - Sources/src/EditorBridge/session_records.cpp
  - Sources/src/EditorBridge/session_vso.cpp
  - Sources/src/EditorBridge/world.h
  - Sources/src/GameTT/iMissionInternal.cpp
  - Sources/src/MapFile/MapGeometry.cpp
  - Sources/src/MapFile/MapGeometry.h
  - Sources/src/MapFile/MapOverlay.cpp
  - Sources/src/MapFile/MapOverlay.h
  - Sources/src/MapFile/MapRecords.cpp
  - Sources/src/MapFile/MapRecords.h
  - Sources/src/Scene/TerrainEditor.cpp
  - build.zig
  - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
  - docs/superpowers/specs/2026-09-30-portable-elk-design.md
  - docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md
  - tools/zig/editor_bridge_test.cpp
  - tools/zig/fixtures/m2_script.lua
  - tools/zig/map_file_test.cpp
covered_digest: "v1:sha256:46572bce6110ce63fcb33d81e6240f772f3a564a8fe5a51d15d1685ee2c1adc6"
behavior_unverified: 0
overrides_applied: 0
re_verification: false
---

# Phase 4: Map editor M2 Verification Report

**Phase Goal:** Map Editor M2: edit what M1 only preserves (roads and rivers, bridges including rotation, entrenchments, fences, AI and unit groups, script areas and script file, camera anchors) with M1's undo, save-preservation, test-in-game and CI standards. Free camera rotation (D-12) decided closed with evidence.
**Verified:** 2026-09-30
**Status:** passed
**Re-verification:** No, initial verification
**Worktree / HEAD:** `.worktrees/map-editor-6`, `feat/map-editor-m2` at `d1887204b`. The last source commit is `d189039ee`. Everything after it is docs.

## Method

I did not take the SUMMARY claims as evidence. The roadmap has no success-criteria list, so the truths come from CONTEXT D-25 (exit criteria 1 to 9) plus the goal's scope items. I re-ran the test tiers myself on macOS arm64, read the load-bearing code, looked at a captured game frame, and checked the CI runs with `gh`.

Logs are in `zig-out/local-test/`:
- `04-verify-tiers.log`
- `04-verify-app.log`
- `04-verify-sweeps.log`

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | D-25.1 Core tier: every M2 command has do/undo/redo against the fake bridge; the cascade restores every reference; refused edits leave document and history unchanged | VERIFIED | `test-editor-core` 245/245 pass, rerun by me (`04-verify-tiers.log`). Also `test-map-editor-view` 30/30 and `test-map-editor-panels`. The whole build reports 535/535 tests passed. The tests named in 04-PARITY exist, e.g. 'the full cascade ...' and 'a span and a trench piece are refused with the document and history untouched'. |
| 2 | D-25.2 Map-file tier: every M2 collection's overlay edits equal the expected-value builder; untouched collections byte-identical; cascade matches builder | VERIFIED | `test-map-files` rerun: `M2 record ops ok (92 cases)`, `camera anchor records`, `script areas`, `find references`, `cascade kinds`, `vso builder`, `bridge plan`, `fence plan`, `trench overlay`, `trench properties ok (500 polylines)`, `parcel formulas`, then `map-file: PASS`. |
| 3 | D-25.3 Engine tier: through the real `ITerrainEditor` / `IAIEditor`, each area is edited, saved, read back and compared, and engine state matches | VERIFIED | `test-editor-bridge` rerun gave 27 `M2 ... ok` lines covering roads, rivers with passability, road edits, bridges draw/rotate/toggle/delete, fences, entrenchments draw/delete, script IDs, groups, hide checked, start commands, reserve positions, AI general, script areas, script file, camera anchors, palette filter and cascade delete. It ends `editor-bridge: PASS`. `test-map-editor-engine` ran 2 of 2 and printed `PASS (260 objects)` with 9 round trips. The engine test was not skipped, so the GPU was present. |
| 4 | D-25.4 Preservation: 1,755 of 1,755 shipped maps round-trip; M2 sweeps edit then undo every map byte-exact | VERIFIED | Rerun by me: `map-file: 1755 of 1755 maps round-tripped`. `M2 sweep 59 maps, 460 edits, all restored byte-exact` (11 kinds, all with edits). `editor-bridge: M2 sweep 57 maps, 238 edits, all restored byte-exact` (bridges/fences/trenches drawn on 57, 23 shipped bridges and 7 shipped trenches deleted, 37 cascades). The sweeps enforce per-kind minimums (WR-C05), so they cannot pass with no edits. |
| 5 | D-25.5 Game reads it: an editor-edited map loads in the game under `BK_AUTO_UI`; trace shows the script ran and found the area and group; camera at the anchor; shot shows road and bridge | VERIFIED | Rerun by me (debug): `game reads it M2 PASS (camera at player 0's anchor 2172,2172, source=player; baseline 3843,579; roads 3 -> 4, rivers 0 -> 1, bridges 0 -> 2 (one rotated, one built during play), ... entrenchments 0 -> 1, group 900 held 1, startcmd launched 0 -> 1, reserve applied 0 -> 1, general side 1 parcels 0 -> 1, script m2_script ran (loaded=1 init=1), area m2_area found at 3072,3072 (lua 3072,3072) ..., both games exited 0)`. I read the release shot `04-13-rel-game-edited.png`. It shows a wooden bridge over a rail road, an L-shaped trench, a fence run, three flak guns and a truck, and the "Reinforcements Have Arrived" message. The script's `init=` value comes from `lua_call`'s own result (WR-A06 fix), so it is not a constant. |
| 6 | D-25.6 Editor app: a `map-editor-auto` M2 scenario draws one of each, undoes/redoes, saves, compares shots, runs Test in game with the script copied along, quits | VERIFIED | Rerun by me: `BK_EDITOR_AUTO: done (298 actions)`. All 16 shot compares are between 0.0000 and 0.79 % (limit 1 %). `expect=undo_depth`, `bridge_built`, `vso_points` and the other predicates ran. `script_choose`, `expect=script_beside`, `script_copy_along_yes`, `test`, `waitgame=240` and `expect=test_game_script:m2_script` ran (frames 343 to 357). M1's scenario still passes (`done (13 actions)`, edited compare 0.0335 %). |
| 7 | D-25.7 CI: all six jobs green; engine tier on macOS arm64 and Windows-MSVC; other jobs skip it, never pass it | VERIFIED | `gh run view 36713320474`: all six jobs `success`, `headSha` d189039ee, which is the last source commit. So CI ran the final code. The earlier run 36692345194 is also green on six jobs. Per 04-13's logs the engine tier printed PASS on macos-platform and windows-platform only. |
| 8 | D-25.8 Parity: every M2 row of 05-PARITY closed with evidence (M2, M6, U1 to U3, O16 refs, O18 trench drawing, VO1 to VO7, MT2, G1, AI1, S4 bridge spans) | VERIFIED | 04-PARITY.md has a populated "Closes with" cell for every row. It cites named tests and log lines. The ones I spot-checked all appear in my reruns. VO3's wording is corrected to "built during play" in 04-PARITY and in the spec. |
| 9 | D-04 / O16: deleting an object updates what refers to it, in one undo step. `FindReferences` is fixed: groups matched by script ID; entrenchment sections, reserve positions and mobile script IDs added | VERIFIED | `MapOverlay.cpp:284-350` reads: bridges, entrenchment `sections`, start commands (`linkID` and `unitLinkIDs`), reserve positions (artillery and truck), then groups by `GroupsHolding(nScriptID)` and AI sides by `SidesHolding`. Link 0 returns early. `TestM2FindReferences` and `TestM2CascadeKinds` pass in my rerun. |
| 10 | D-05: bridge spans (6), entrenchment pieces (4) and fences (9) are no longer placeable from the palette or `BkEditorAddObject`; loaded ones still load | VERIFIED | `panels_logic.zig` `isPlaceable` returns false for 4, 5, 6, 9 and 100. `editor-bridge: M2 palette filter ok`: the bridge refuses types 4, 6 and 9, and loaded ones still load and move. |
| 11 | D-03 / D-09 / D-12 specifics: the bridge assigns the saved road/river `nID` itself; river edits keep AI passability in step and roads do not; built-during-play only for `WoodenBig_Heavy_*` | VERIFIED | `session_vso.cpp:202` uses `NMapRecords::NextVsoID(snapshot)`. Lines 270 and 282 call `IAIEditor::DeleteRiver` and `AddRiver` for rivers only. `session_groups.cpp:40` sets `BUILD_DURING_PLAY_FAMILY = "WoodenBig_Heavy_"`, with refusal text at line 871. The fake bridge mirrors it. The passability and toggle tests pass. |
| 12 | D-23: free camera rotation closed as not planned, reason in the spec; `BkEditorSetYaw` and `TestYawMeasurement` stay as evidence, no input bound | VERIFIED | The spec (line 124 onward) records the 03-06 numbers (0.5 to 99.2 % black) and the pre-rendered-art reason, and says that M2 markers and M3 layer toggles cover the purpose. `BkEditorSetYaw` is only in `bridge.cpp/.h`, `session` and `editor_bridge_test.cpp`. A grep of `Sources/editor` (core and app) finds no caller, so no input is bound to it. |

**Score:** 12/12 truths verified, 0 present but behavior-unverified.

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `Sources/src/MapFile/MapRecords.{h,cpp}` | VERIFIED | 138 and 348 lines. Record-level overlay for every M2 collection. Exercised by `M2 record ops ok (92 cases)` and the sweeps. |
| `Sources/src/MapFile/MapGeometry.{h,cpp}` | VERIFIED | 282 and 745 lines. `PlanBridge`, fences, trench plan and parcel formulas, with their own tests. |
| `Sources/src/EditorBridge/session_{vso,groups,records}.cpp` | VERIFIED | 805, 1334 and 1546 lines. Real engine calls (`AddRoad`, `AddRiver`, `DeleteRiver`). Wired into `bridge.cpp` and `c_bridge.zig`. |
| `Sources/editor/core/tools_{vso,groups,ai}.zig`, `script_file.zig`, `records.zig` | VERIFIED | 1106, 1183, 1827, 622 and 730 lines. They have core tests and are driven by the auto scenario. |
| `Sources/editor/app/tool_registry.zig`, `markers.zig`, `panels_m2.zig`, `commands.zig` | VERIFIED | The registry holds every M2 tool (roads_rivers through ai_general). Menus wire Script, Player camera, Start commands, Artillery mode, Groups and View, Markers. The auto scenario drives them. |
| `tools/zig/fixtures/m2_script.lua` | VERIFIED | Real Lua. The game ran it and it found `m2_area` and group 4245. |

### Key Link Verification

| From | To | Status | Details |
|------|----|--------|---------|
| Tool, `Editor`, bridge vtable, C ABI (`Guarded`), session | engine and snapshot | WIRED | The same round trip passes on the fake bridge (core tier) and the real engine (`map-editor-engine` round trips). |
| Session edits, snapshot plus overlay save, read-back | saved `.bzm` | WIRED | Sweeps: edit and undo saves the unedited bytes on 59 and 57 maps. |
| Test in game, script copy, test game's `BK_MAP_TRACE` | script ran | WIRED | The scenario's `expect=test_game_script:m2_script` reads the game's own trace. The test folder is wiped of a stale script (WR-C06). |
| Editor-saved map, game loader (`LoadBridges`, `LoadEntrenchments`, `InitReservePositions`, `Scripts`) | game state | WIRED | The game-reads-it PASS line reports each count from the game's trace. The game asserts on every link and exited 0. |

### Data-Flow Trace (Level 4)

Saved records are built from command-stored before/after records, never from engine state. The session reads the map back and compares (`SaveSessionMap`). The game-reads-it tier then loads the file in the real game and reads the counts back. Flow is real and end to end.

### Behavioral Spot-Checks

| Behavior | Result | Status |
|----------|--------|--------|
| Core, view, panels, auto, testlaunch tiers | 535/535 tests passed | PASS |
| Map-file, engine-bridge and engine-Zig tiers | `map-file: PASS`, `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)` | PASS |
| M2 app scenario and game-reads-it M2 | `done (298 actions)`, `game reads it M2 PASS` | PASS |
| Preservation sweeps and 1,755 map round trip | byte-exact on 59 and 57 maps; 1755 of 1755 | PASS |

### Probe Execution

No `probe-*.sh` is declared by the phase or present. The equivalent runnable checks are the tiers above. SKIPPED.

### Requirements Coverage

The phase has no requirement IDs in REQUIREMENTS.md. The set is CONTEXT D-01 to D-25 and the M2 rows of 04-PARITY. All rows are covered by truths 1 to 12. No orphaned requirements.

### Anti-Patterns Found

I diffed the 52 files changed against main for added `TBD`, `FIXME`, `XXX`, `TODO`, `HACK` and `PLACEHOLDER` markers and found none. There are no stub returns on the paths that carry data.

### Review Follow-up

The 30 critical and warning findings in 04-REVIEW.md are all marked fixed in 04-REVIEW-FIX.md. Their regression tests ran in my tier reruns, e.g. `M2 short roads kept as read`, `shared bridge links kept as read`, `AI side shrink refused`, `group hold warning` and `off-map camera anchor undo`. The two commits after the last app-tier log (`d189039ee` and its neighbours) are covered by CI run 36713320474 and by my own rerun.

### Notes (not blockers, per the stated autonomy policy)

These are residual items that an agent cannot observe. They are not `human_needed`, because no plan required a human and the run was ordered fully autonomous.
1. Trackpad feel of double-click finish and Ctrl-click as right-click.
2. "Open script folder" opening the folder on Windows (`SDL_OpenURL` of a folder URL). WR-B04 changed "Open script" into "Open script folder", and it was only checked by the URL unit test.
3. The GPU look of roads and animated rivers on Windows. CI checks numbers only.
4. Error branches with no scenario or test step. The review-fix report lists them itself:
   - WR-A01, `SGroupEdit::Revert` rollback;
   - WR-A06, a script whose `Init` errors should report `init=0`;
   - WR-C01, `c_bridge.zig` allocation cleanup on failure;
   - WR-C04, `script_choose` when the copy is refused.
   All four failure paths are unreachable from shipped data and are covered by review, not by tests.
5. Hygiene only. `04-VALIDATION.md` frontmatter still says `status: draft` and `nyquist_compliant: false`, while its body says signed off. 04-13 left it "for validate-phase".
6. The branch diff against main also includes two unrelated specs (`portable-elk-design.md` and `portable-resource-editor-design.md`). They are in `covered_files` but are not phase 4 content.
7. The shot compares use local reference images that the scenario seeds on its first run. A compare passing shows stability against the seeded run, not against an independent golden image. The game-side evidence (the trace and the read shot) is independent.

## Gaps Summary

No gaps. Every D-25 exit criterion was re-run or re-read in the codebase and holds. M2 is implemented, wired from tool through bridge and engine to the saved file, and read back by the real game. Preservation holds on all shipped maps. CI is green on six jobs at the final source commit. The D-12 camera rotation question is closed with a written reason and no input bound.

---

_Verified: 2026-09-30_
_Verifier: Claude (gsd-verifier)_

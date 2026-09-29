---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 04
subsystem: map-editor
tags: [map-editor, m2, game-reads-it, bk-map-trace, test-launch, render-proof, engine, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: Editor.setCameraAnchor / clearCameraAnchor, the record_edit command
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 03
    provides: the M2 app foundations (commands, tool registry, automation verbs)
provides:
  - BK_MAP_TRACE, an env-gated seam in the game that prints one key=value line per map item it consumed (camera, terrain, script, areas, groups, bridges, entrenchments, start commands, reserve positions, generals, parcels) and mirrors Lua Trace
  - testlaunch.parseMapTrace and MapTraceSummary with fixed-size, optional fields
  - game_reads_m2.zig and MapEditor --game-reads-it-m2, the scenario every later M2 plan extends
  - game_reads_common.zig, the host/editor start, launch-and-wait and cleanup shared with --game-reads-it
  - build step map-editor-game-reads-it-m2
  - TestM2VsoRendersOnGpu, the measured proof that GFXGPU draws a shipped river and a shipped road
affects: [04-05, 04-06, 04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 3
plan_head_before: 00e44cd700eac3188c684b9d2e55658ae9a7c03f
actuals:
  tokens: 20700
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A later plan adds its edit to game_reads_m2.run after the camera anchor: edit through the core Editor, common.saveTestCopy, play(...) for the second run, compare the two MapTraceSummary values"
    - "A new kind of trace line is one fprintf under IsMapTraceOn() in the game and one branch in parseMapTrace with an inline-log test; unknown kinds and keys are ignored"
    - "Names in trace lines are double-quoted; numbers after a name are read from after the closing quote"

key-files:
  created:
    - Sources/editor/app/game_reads_common.zig
    - Sources/editor/app/game_reads_m2.zig
  modified:
    - Sources/src/GameTT/iMissionInternal.cpp
    - Sources/src/AILogic/Scripts/Scripts.cpp
    - Sources/src/AILogic/AILogicInternal.cpp
    - Sources/src/AILogic/GeneralInternal.cpp
    - Sources/src/Scene/TerrainEditor.cpp
    - Sources/editor/app/testlaunch.zig
    - Sources/editor/app/main.zig
    - build.zig
    - tools/zig/editor_bridge_test.cpp

key-decisions:
  - "Trace lines quote names (script name=\"x\", area name=\"y\") so an area name with spaces survives; the plan's line format otherwise stands"
  - "--game-reads-it-m2's log path is a combined report (the baseline's trace lines, then the edited run's); each run's own full log is <log>.baseline.log and <log>.edited.log beside it. A single game run prints one camera line, so the plan's 'at least 2 camera lines' can only hold in a combined file"
  - "The shared parts of --game-reads-it moved to game_reads_common.zig (Rig, TestPaths, saveTestCopy, runGame, deleteAutoshots, applyModArg, mapArgument) instead of staying in main.zig, because game_reads_m2.zig cannot import the executable's root file cleanly; main.zig keeps aliases"
  - "script loaded= and init= are the same value: CScripts::Load calls Init exactly when the file ran"
  - "The anchor point is the level one (least height change to its four tile neighbours) of five spread over the map that is at least 20 tiles (20 x 45.25 world units) from the baseline camera"
  - "The zig build step map-editor-game-reads-it-m2 depends on the M1 step (this Zig has no ordering-only edge), so requesting it runs both"

patterns-established:
  - "MapTraceSummary lists are Kept(T, capacity): the first N items kept, .seen counts all, .seen == 0 means not reported"

requirements-completed: [D-22, D-09, D-25]

coverage:
  - id: D1
    description: "A map whose player-0 camera anchor was set by the editor, test-launched, starts the game's camera at that anchor (BK_MAP_TRACE camera equals it within 1 world unit, source=player)"
    requirement: "D-22"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'game reads it M2 PASS (camera at player 0's anchor 2172,2172; baseline 3843,579)'; anchor z 5.7, game camera z 6 (A2)"
        status: pass
    human_judgment: false
  - id: D2
    description: "With BK_MAP_TRACE set the game prints one key=value line per consumed item and mirrors Lua Trace; without it, nothing new"
    requirement: "D-25"
    verification:
      - kind: integration
        ref: "map-editor-game-reads-it-m2.log: camera, terrain, entrenchments, bridges, reserve, general, startcmd, script lines in both runs; map-editor-game-reads-it.log holds 0 BK_MAP_TRACE lines (M1 still PASS)"
        status: pass
      - kind: integration
        ref: "MapEditor --game-reads-it-m2 Data\\Maps\\dessau.bzm (a map with a script, areas, bridges, start commands): area x5, bridges n=4, startcmd launched=22, general mobile=3, script name=\"dessau\" all reported; log zig-out/local-test/04-04-dessau-m2.log"
        status: pass
      - kind: unit
        ref: "zig build test-map-editor-testlaunch: 20 tests, every kind, truncated and malformed lines, list caps"
        status: pass
    human_judgment: false
  - id: D3
    description: "A local map-editor-game-reads-it-m2 step builds a map through the core Editor on the real bridge, test-launches the real Game twice and prints 'map-editor: game reads it M2 PASS'"
    requirement: "D-25"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it map-editor-game-reads-it-m2: two PASS lines (M1 and M2), rc=0"
        status: pass
    human_judgment: false
  - id: D4
    description: "The engine tier measures that GFXGPU draws a shipped river and a shipped road; the river's animated layer is compared across two captures a second apart"
    requirement: "D-09"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: 'M2 river drawn (5941 px of 40610) animated=0 (animation unobserved (A7))', 'M2 road drawn (10362 px of 53074)', 'editor-bridge: PASS'"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-04-river-a/-b and 04-04-road-a/-b PNGs, inspected: the river (green water under two rail bridges) is dry riverbed in B; the road's two wheel tracks in snow are gone in B"
        status: pass
    human_judgment: false

duration: 20min
completed: 2026-09-29
status: complete
---

# Phase 4 Plan 04: The game reports what it read (BK_MAP_TRACE) and the render proof Summary

**An env-gated `BK_MAP_TRACE` seam in the game, a parser for it, a `map-editor-game-reads-it-m2` scenario that proves the editor's player-0 camera anchor decides where the test game starts, and pixel measurements that GFXGPU draws a shipped river and road.**

## Performance

- **Duration:** about 20 min of work, most of it waiting for game builds (started about 2026-09-29T23:16Z)
- **Completed:** 2026-09-29T23:40Z
- **Tasks:** 3 (one tracer, two auto)
- **Files:** 11 changed (2 created), 1054 insertions, 155 deletions

## Accomplishments

- **The seam.** `iMissionInternal.cpp` prints `camera x= y= z= source=player|neutral|units` and `terrain roads= rivers=`; `Scripts.cpp` prints `script name="<basename>" loaded= init=`, `area name="" cx= cy=` per area and mirrors `CScripts::Trace` as `lua <text>`; `AILogicInternal.cpp` prints `group id= held=` per reinforcement group (every group the map names, zero included), `bridges n=`, `entrenchments n=`, `startcmd launched=`, `reserve applied=`; `GeneralInternal.cpp` prints `general side= parcels= mobile=` and `parcel side= idx= type= cx= cy= r= dir=`. Each file has its own cached `IsMapTraceOn()`; the M1 scenario's log holds no trace line.
- **The parser.** `testlaunch.parseMapTrace` reads every kind into fixed-size, self-owning summaries (`Kept` lists with the first 32 areas, 32 groups, 8 generals, 32 parcels, 16 Lua lines); an item the game did not print reads as null or `seen == 0`, never as zero; truncated and malformed lines add nothing.
- **The scenario (D-22).** `--game-reads-it-m2`: open coldwinter on the real bridge, save the unedited test copy, play it, set player 0's anchor with `Editor.setCameraAnchor` at the level point at least 20 tiles from the baseline camera, save, play again, and require `source=player` within 1 world unit. Measured: baseline camera 3843,579; anchor 2172,2172; the game started at 2172,2172 (z 6 for an editor z of 5.7: the game prints z to the unit, so the anchor's z is used as saved, A2).
- **The render proof (D-09).** The engine's own first river (arnheim) and first road (coldwinter) are removed with `ITerrainEditor::RemoveRiver` / `RemoveRoad` and the pixels in the stripe's screen box are compared: river 5941-5972 of 40610 pixels changed (14.6-14.7 %), road 10362 of 53074 (19.5 %), against a 2 % bar. The four captures were converted to PNG and looked at: the river in A is green water under two rail bridges and in B a dry bed; the road in A is two wheel tracks across snow and in B plain snow.
- **A7, the animated layer:** two captures of the river at the same camera after 60 frames with a 16 ms delay were identical inside the box (`animated=0`). The bridge's frame loop does not advance the water animation (or not by 1 s of wall time); the tools must not count on seeing it animate in the editor's view. Printed, not failed, as planned.

## Task Commits

1. **Task 1: the game reports its start camera; the M2 scenario proves the anchor (tracer)** - `3f7274941` (feat)
2. **Task 2: the rest of the seam, the full parser, baseline counts** - `aa7bec7a5` (feat)
3. **Task 3: render proof, RemoveRoad's return value** - `07c5d0f95` (test)

**Plan metadata:** the docs commit that carries this file.

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after the build.zig edit | 3/3 pass |
| `test-map-editor-testlaunch` | 20 tests pass (10 new parseMapTrace tests) |
| `map-editor-game-reads-it` (M1) | PASS, unchanged messages; the log holds no BK_MAP_TRACE line |
| `map-editor-game-reads-it-m2` | PASS; 2 camera lines and 10 trace lines of the plan's listed kinds in the combined report |
| `test-editor-bridge` | `editor-bridge: PASS` including the river and road proofs |

Tracer gate: the tracer's `<verify>` is automated, so it was re-run end to end under the end-of-phase mode (unit tier then the scenario) and passed before the seam was widened.

Baseline counts that later plans compare against (coldwinter): roads=3 rivers=0 bridges=0 entrenchments=0 startcmd=0 reserve=0 areas=0 groups=0 generals=1 (side 1, parcels=0 mobile=0) lua=0, script `""` (the map has none). On dessau (run once by hand): roads=26 rivers=8 bridges=4 startcmd=22 areas=5 generals=1 (mobile=3) script `dessau` with loaded=0, because the test copy sits in the generated-data directory where the map's `.lua` is not (the script plan has to put it there).

## Decisions Made

See `key-decisions`. Also: the M1 messages and behaviour of `--game-reads-it` are unchanged by the extraction; its launch-and-wait now goes through `common.runGame` with the `what` strings "game" and "baseline game" so the FAIL lines read as before.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `CTerrain::RemoveRoad` always returned false**
- **Found during:** Task 3 (the road proof: "the engine removes its first road (id 11506)" failed although the road was gone from the next frame)
- **Issue:** `Scene/TerrainEditor.cpp` removed the road from `terrainInfo.roads3` and `roads`, then `return false;`. `RemoveRiver` returns true. The only other caller (the MFC editor's VectorStripeObjectsState) ignores the result, but the M2 road tool needs to tell a removed road from an unknown ID.
- **Fix:** it now answers true when the road was there and is gone.
- **Files modified:** `Sources/src/Scene/TerrainEditor.cpp`
- **Committed in:** `07c5d0f95`

**2. [Plan detail] The combined report log**
- **Issue:** Task 1's verify counts `BK_MAP_TRACE: camera` lines in `map-editor-game-reads-it-m2.log` and requires 2, but one game run prints one camera line and `testlaunch.start` truncates its log.
- **Fix:** the scenario writes that path as a combined report (baseline lines, then edited lines); each run's own log is `<log>.baseline.log` and `<log>.edited.log`.
- **Committed in:** `3f7274941`

**3. [Plan detail] A new shared file**
- **Issue:** the plan says to extract the shared parts of `gameReadsIt` "into functions both call" in main.zig, but `game_reads_m2.zig` would have to import the executable's root file.
- **Fix:** they live in the new `game_reads_common.zig` (not in the plan's file list); main.zig imports it.
- **Committed in:** `3f7274941`

### Process note

The commit trailer is `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, the model that ran this plan, as in 04-01 to 04-03, not the `(1M context)` wording in the executor rules.

---

**Total deviations:** 1 Rule 1 bug fix, 2 plan-detail adjustments
**Impact on plan:** no scope change; the RemoveRoad fix is one return value in the engine and unblocks the road tool.

## Issues Encountered

- The Lua `Trace` mirror is covered by parser tests only: no shipped map calls `Trace` on its own in the first 420 frames, so no real game line exists yet to compare with. The plan that adds a script (04-12 on) should call `Trace` from its Lua and assert the `lua` line.
- `area`, `group` (held counts), `entrenchments`, `reserve` and `parcel` lines are exercised by the real game only where a shipped map has them: dessau showed areas, bridges, start commands and a general; coldwinter and dessau both report `entrenchments n=0`, `reserve applied=0`, `parcels=0`, and no map with reinforcement groups was run. Their plans add the edit and assert baseline + edit.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-05 onward: add the tool's edit inside `game_reads_m2.run` after the camera assertion (one more `saveTestCopy` + `play`), compare against `baseline.trace` and print the plan's PASS detail.
- `zig build map-editor-game-reads-it-m2` runs the M1 scenario first (about a minute longer); a single map can be tried by hand with `MapEditor --game-reads-it-m2 <map> [<log>]` from the staged installation.
- The render proof says a river and a road can be seen in the editor's own view; the river will not be seen animated there (A7).

## Threat surface

No new surface beyond the plan's model: the trace is env-gated, the script is printed as its basename only, no line carries an absolute path (T-04-04-01); the parser uses fixed-size summaries, ignores unknown keys, treats missing items as unreported and has truncated-line tests for every kind (T-04-04-02).

## Self-Check: PASSED

- Created files found: `game_reads_common.zig`, `game_reads_m2.zig`.
- Commits found: `3f7274941`, `aa7bec7a5`, `07c5d0f95`.
- Acceptance greps re-run: `BK_MAP_TRACE: camera` once in iMissionInternal.cpp inside `IsMapTraceOn()`; `parseMapTrace` 38 times in testlaunch.zig; `game-reads-it-m2` in main.zig and build.zig; BK_MAP_TRACE counts AILogicInternal.cpp 8, Scripts.cpp 5, GeneralInternal.cpp 4.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-29*

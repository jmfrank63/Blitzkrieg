---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 11
subsystem: map-editor
tags: [map-editor, m2, start-commands, reserve-positions, actions-ini, record-kinds, mfc-conversion, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords Insert/Replace/Erase of start commands and reserve positions, the byte-identity rule
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 02
    provides: the delete cascade over start commands and reserve positions (NMapOverlay::DeleteObject, tombstones, the fake's model)
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: the M2 game-reads-it harness and BK_MAP_TRACE (startcmd launched, reserve applied)
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 10
    provides: the tool shape in tools_ai.zig, the index-keyed list kinds on the generic record path, the two-pass read rule
provides:
  - BkEditorActionCommands (Data/Editor/actions.ini, STOP at the default entry) and BkEditorStartCommand/Count/Add/Set/Delete
  - BkEditorReserveRole and BkEditorReservePosition/Count/Add/Set/Delete with ValidateReservePosition
  - records.Kind.start_command and .reserve_position, StartCommand (owned units) and ReservePosition, truncateToAi
  - Editor.actionCommands/startCommands/addStartCommand/editStartCommand/deleteStartCommand/addUnitToStartCommand/removeUnitFromStartCommand/reserveRole/reservePositions/addReservePosition/editReservePosition/deleteReservePosition; both generations bump on an object's delete, restore and replays
  - tools_ai.StartTarget (one click, left after it) and tools_ai.ReservePositions (the MFC click order), both hidden in the palette
  - Unit menu (Add start command, Start commands..., Artillery positions mode), the Start Commands window, the Reserve positions panel, red lines and gun-truck-place lines
  - the game launching the new start command (startcmd launched 0 -> 1) and applying the new reserve position (reserve applied 0 -> 1)
affects: [04-12, 04-13]

commits: 4
plan_head_before: c11776b2042b72efaf4f51205ad7beb706a1abb1
actuals:
  tokens: 62400
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "A record that names other records (units, a target, a gun and a truck) is judged on what a put CHANGES: a field the current record already holds is not re-judged, and a record the map held when it was opened is always accepted back, so an undo of a delete or an edit on a file's own odd data never fails as a drift"
    - "A stat-dependent rule lives in the bridge (the object database is there) and reads the stats with dynamic_cast: the typed NGDB::GetRPGStats lookup static_casts when its asserts are compiled out, which would read a soldier's stats as a vehicle's"
    - "A tool that a panel or a menu command puts in hand carries `hidden` in the registry: no palette button, no Tools menu entry, no key; tool=<label> and the panel still reach it, and the M1 reference frame is unchanged"
    - "A panel that uses a key the map's tool also uses claims it for the frame (View.delete_claimed, cleared by panels.draw each frame), instead of capturing the whole keyboard, so undo and the camera keys stay the view's"
    - "A bridge warning that is not a refusal rides the OK answer's message (BkEditorLastMessage after a successful add) and the core puts it in the status bar with noteCascade; a test reads the message BEFORE the next bridge call, which clears it"

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/session_records.cpp
    - Sources/editor/core/records.zig
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/tools_ai.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/tool_registry.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/markers.zig
    - Sources/editor/app/commands.zig
    - Sources/editor/app/game_reads_m2.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig

key-decisions:
  - "The action list is parsed in the bridge from the ini text, not through OpenIniDataTable: the game's Zig StreamIO port answers nothing for it (legacy_bridge.cpp). The parser follows the MFC table's rules (trimmed lines, ';' comments, the first row only, a repeated name keeps its first place and takes its last value), so the list is the one CAISCHelper shows: 40 entries, STOP at entry 9"
  - "A start command's units must be units or squads the database knows (a building or a tree is refused, naming it), and a soldier is the squad the click already answers; the MFC editor only guarded this at add time, the game hands every unit to a group command"
  - "A set never changes from_explosion: the bridge takes it from the command already there and Editor.editStartCommand reads it first, so an edit that changes nothing else records nothing and the history holds what the bridge holds; an add stores the flag it is given so an undo of a delete brings a file's own 1 back"
  - "The reserve role of an object type is 1 self-propelled (a self-propelled or armoured unit, a super train, an artillery piece without crew places), 2 towed (artillery with crew places), 3 truck (carrier or tractor), read by dynamic_cast from the stats; a self-propelled gun takes no truck (the MFC editor only attached one to a towed gun) and a truck must pull more than the gun weighs"
  - "A start command or reserve position naming a missing object is exempt when the file held it: openedStartCommands and openedReservePositions make the undo of a delete of such a record an accepted add, while a new record like it is refused and a set is judged only on what it changes"
  - "The Start Target tool acts on the RELEASE (the MFC editor acts in OnLButtonUp), a ground click sets the truncated point and clears the link (the plan's rule), an object click keeps the point; a click the bridge refuses leaves the tool in hand with the editor's status, Escape leaves it. Reserve Positions picks on the release too"
  - "The fake keeps start commands as a fixed-capacity value type (16 units, every field of records.StartCommand, converted at the record calls) instead of owned slices: the cascade tests snapshot the list by copy and compare it after undo and redo, which owned slices would have broken without gaining a rule"
  - "Start Target and Reserve Positions are registry tools with `hidden`: the palette would otherwise show a button that only works with the Start Commands window, and the extra row moved 1.85% of the M1 reference frame (limit 1%); hidden, it is 0.5-0.65%"

patterns-established:
  - "The Start Commands window is a floating window (Unit menu, like Groups and Script), the Reserve positions panel takes the left column while its tool is in hand (like Script areas): a tool with a list of its own gets a panel, a tool entered for one click gets a window"
  - "Engine-tier tests pick their objects from the catalogue by BkEditorReserveRole and from the stats by the towing numbers, print what they found and note, never assume, the branches the data does not offer"

requirements-completed: [D-17, D-18, D-04, D-02, D-06]

coverage:
  - id: D1
    description: "D-17: Unit -> Add start command adds a STOP command (entry 9 of actions.ini) for the selected unit with no target, one undo step; the type list comes from Data/Editor/actions.ini; a soldier stands for his squad; fromExplosion is kept from the file and not editable"
    requirement: "D-17"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'a start command: add is one step of STOP with no target, undo removes it, redo puts it back', 'the action list comes from the bridge, STOP at the default, and a missing list refuses an add', 'start command: a type, a number and a target edit in one gesture are one undo step and the flag stays', 'a start command deleted and put back by undo keeps its flag, its target and every unit'; records.zig 'a start command value owns, clones, compares and frees its units'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2StartCommands on coldwinter: the actions list (40 entries, STOP = 9 at the default, MOVE_TO first) in two passes, add for one unit, edit type, target object, point and number, save equals the NMapRecords-built map, a scratch map with fromExplosion 1 keeps it through a set and gets it back through an add, undo all saves the unedited bytes (editor-bridge: M2 start commands ok)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 start command round trip ok (raw BkEditorStartCommand read, undo, redo, the unit's delete taking the command with it and the undo bringing it back)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=select, click a tank, do=startcmd_add, expect=startcmds_delta:1, startcmd_units:0:1, do=startcmd_type:MOVE_TO, startcmd_is:0:MOVE_TO, do=startcmd_number:2.5, do=startcmd_target_here, startcmd_target:0:pos (BK_EDITOR_AUTO: done, 237 actions)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-17: the target is set by a click on the map (the point, truncated as the MFC Vis -> AI) or on a unit (its link ID), fNumber is a field, the Start Commands window lists, edits and deletes commands (Delete key), more units are added one at a time with Add selected unit, and red lines run from the units to the target"
    requirement: "D-17"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig 'Start Target: a click on the ground sets the point with the MFC cut and clears the target, one step, and the tool is done', '... a click on an object makes it the target and keeps the point', '... no command chosen, or one gone ...', '... a point the bridge refuses leaves the tool active ...'; editor.zig 'start command units: Add selected unit skips a duplicate, Remove of the last unit deletes the command in one step'; records.zig 'truncateToAi cuts int(v + 0.3), towards zero, as Vis2AI does'"
        status: pass
      - kind: unit
        ref: "zig build test-map-editor-view-events: view.zig 'Set target puts the Start Target tool in hand for one click, sets the point on the release and returns to the tool it came from', 'Delete is not the tool's while a panel has claimed it'; test-map-editor-panels: tool_registry.zig 'the Start Target tool has no key, takes the left button only, and shows the start commands' lines'"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-11-m2_startcmds.png (from m2_startcmds.tga), inspected: a thick red line runs from the selected Panther (link 18) to a red square at the view centre, the Unit menu is in the bar; 04-11-m2_startcmds_panel.png: the Start commands window lists '0: MOVE_TO, 1 unit, point 5433, 821' and shows the MOVE_TO type combo, number 2.500, target, 'from explosion: no (kept from the file)', the unit row with Remove, Add selected unit and Delete"
        status: pass
    human_judgment: true
    rationale: "Whether the target click feels right at other zooms, whether the Delete key should also work with the window unfocused, and how the window reads beside the other floating windows are judgments; the scenario proves the commands and the shots show the drawing"
  - id: D3
    description: "D-18 with C4: Unit -> Artillery positions mode: click an artillery unit, optionally a truck, then the ground; Enter commits; the bridge refuses a unit that is not artillery, a towed gun without a truck, a truck that cannot tow the gun, a squad in either role, link ID 0 as artillery and a position with both link IDs 0; positions are listed, drawn (gun -> truck -> spot) and deletable; one undo step each"
    requirement: "D-18"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2ReservePositions on coldwinter: roles read over the catalogue by BkEditorReserveRole (48 towed guns, 90 self-propelled, 21 trucks, 70 squads; a soldier and a squad are role 0), a towed gun with a truck that tows it and a self-propelled gun alone add and save as the NMapRecords-built map; a towed gun without a truck, a self-propelled gun with one, a truck that cannot tow the gun, a squad in either role, a non-unit, a truck as the gun, a gun as the truck, link ID 0 as the gun, both 0, a missing object, a negative truck and a place off the map are REFUSED unchanged (bytes equal); the gun's delete erases its position ('also reserve position 0 erased') and the restore brings it back; everything deleted saves the placed map's bytes; a file's own odd position is deleted and put back, a new one like it is refused (editor-bridge: M2 reserve positions ok)"
        status: pass
      - kind: unit
        ref: "zig build test-editor-core: tools_ai.zig 'Reserve Positions: a towed gun, its truck, the ground and Enter make one entry, one undo step', '... a towed gun without a truck is Refused with the history unchanged and the choice kept', '... a self-propelled gun needs no truck ...', '... a click out of order or on the wrong thing is a note ...', '... Delete removes the selected position as one step ...'; editor.zig 'a reserve position: a towed gun with its truck, one undo step each, appended, exact undo and redo', 'reserve position refusals change nothing: ...', 'deleting a gun erases its reserve position and the undo brings it back, both generations moving'"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: tool=place, do=placer_role:towed, click, do=placer_name:Sdkfz_8, click, do=reserve_mode, click the gun, expect=reserve_pending:gun, click the truck, expect=reserve_pending:truck, click the ground, expect=reserve_pending:place, key=ENTER, expect=reserve_delta:1, undo, expect=reserve_delta:0, redo, shot=m2_reserve (BK_EDITOR_AUTO: done, 237 actions)"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-11-m2_reserve.png (from m2_reserve.tga), inspected: an orange line runs from a square at the 10.5-cm_Flak38 through a circle at the Sdkfz_8 to a diamond at the place; the Reserve positions panel lists '0: 10.5-cm_Flak38 + Sdkfz_8, at 5033, ...' with In hand, Add, Clear and Delete"
        status: pass
    human_judgment: true
    rationale: "The click order against the MFC editor's muscle memory, and whether the dashed choice in hand is easy to read on other terrain, are judgments; the tests and scenario prove the order and the refusals"
  - id: D4
    description: "D-04 integration: deleting a unit refreshes both lists (the cascade bumps their generations) and undo restores them"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'deleting and restoring a unit move the start-command generation, both ways', 'deleting a gun erases its reserve position and the undo brings it back, both generations moving'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2ReservePositions (the gun's delete and restore) and the 04-02 TestM2CascadeDelete, both passing with the new kinds in place"
        status: pass
    human_judgment: false
  - id: D5
    description: "D-02, D-06: both kinds ride the generic record command, add-then-delete or edit-then-inverse saves the unedited file byte for byte, and the markers draw them under the start_commands and reserve_positions kinds"
    requirement: "D-02"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2StartCommands and TestM2ReservePositions compare two session saves after an edit and its inverse (SameBytes), never a save against a fresh read (04-01)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-host-check: panel smoke PASS with the Unit menu, the window and the panel in the frame; map-editor-auto: compare=edited 0.5-0.65% of pixels differ (limit 1%)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The game reads it: the M2 map's new start command is launched and its new reserve position is applied (trace startcmd launched and reserve applied each one above the unedited map)"
    requirement: "D-17"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'BK_MAP_TRACE startcmd launched=1 (baseline 0)' for a STOP command of a second unit, 'reserve position 0: 10.5-cm_Flak38 (link 281) towed by Voroshilovets (link 284) to 3072,2704' and 'BK_MAP_TRACE reserve applied=1 (baseline 0)'; game reads it M2 PASS (... startcmd launched 0 -> 1, reserve applied 0 -> 1, script m2_script ran, area m2_area found)"
        status: pass
    human_judgment: false

duration: 1h20m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 11: Start commands and reserve positions Summary

**A start command is added for a selected unit with its type from actions.ini, aimed by a click on the map or a unit and drawn as a red line; an artillery reserve position is made in the MFC click order with its role and towing checks in the bridge; both ride the generic record command with exact undo, and the real game launches the command and applies the position.**

## Performance

- **Duration:** about 1 h 20 min (13:05 to 14:24 local, +07), of which most was waiting on the four-minute engine-tier runs and the game-reads run
- **Tasks:** 4 (one tracer, three auto)
- **Files:** 21 changed, 4719 insertions, 20 deletions

## Accomplishments

- **Start commands end to end (tracer):** `BkEditorActionCommands` reads Data/Editor/actions.ini (STOP at the default entry 9), `BkEditorStartCommand*` hold the records through NMapRecords on both copies with their rules (a unit or squad the database knows, no duplicate, a target that is 0 or an object, a listed type, a point on the map), `Kind.start_command` rides the generic record path, and `Unit -> Add start command` makes a STOP command for the selected unit as one undo step. The game launched it (`startcmd launched` 0 -> 1).
- **Targets, units, window, red lines:** the Start Target tool takes one click (an object becomes the target, the ground sets the truncated point) and hands the previous tool back; the Start Commands window lists, sets the type from the list and the number, adds the selected unit, removes units (the last takes the command with it), sets a target and deletes (the Delete key is the window's while it is focused); red lines run from each unit to the target.
- **Reserve positions with the MFC checks:** `BkEditorReserveRole` classifies a type from its stats; `ValidateReservePosition` refuses a squad or non-unit in either role, link ID 0 as the gun, both 0, a towed gun without a truck, a self-propelled gun with one and a truck that cannot tow (towing force not above the gun's weight), a place off the map; new positions append; a file's own records are exempt. Engine-tier test on the real catalogue.
- **Artillery positions mode:** the Reserve Positions tool follows the MFC click order (gun, a towed gun's truck, ground; Enter commits, Escape clears, Delete removes), a panel lists the positions and shows the choice in hand, gun -> truck -> place lines are drawn (the choice in hand dashed). The game applied the position (`reserve applied` 0 -> 1).
- **Cascade:** deleting, restoring and replaying an object bumps both generations, so the window and the panel read again; undo brings the records back exactly.

## Task Commits

1. **Task 1: A start command end to end (tracer)** - `14fffc117` (feat)
2. **Task 2: Start command targets, the window and red lines** - `32e705e6e` (feat)
3. **Task 3: Reserve positions as records with the role and towing checks** - `93a359d20` (feat)
4. **Task 4: The Reserve Positions tool, panel and lines; the game applies the position** - `c49e61b2c` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; the engine tier (`editor-bridge: M2 start commands ok`, `editor-bridge: PASS`, `map-editor-engine: M2 start command round trip ok`, `map-editor-engine: PASS (260 objects)`) and the game proof (`startcmd launched=1`, baseline 0) all passed before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core`, `test-map-editor-panels`, `test-map-editor-view-events`, `test-map-editor-view`, `test-map-editor-auto`, `test-map-editor-testlaunch` | 428 tests pass in the last combined run |
| `test-editor-bridge` | `M2 start commands ok`, `M2 reserve positions ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 start command round trip ok`, `PASS (260 objects)` |
| `map-editor-host-check` | `panel smoke PASS` |
| `map-editor-auto` (M1) | `compare=edited` 0.53-0.65% of pixels differ (limit 1%) |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (237 actions)` |
| `map-editor-game-reads-it-m2` | `game reads it M2 PASS (... startcmd launched 0 -> 1, reserve applied 0 -> 1, ...)` |

Logs are under `zig-out/local-test/04-11-*.log`; the inspected shots (as PNG) are `04-11-m2_startcmds.png`, `04-11-m2_startcmds_panel.png` and `04-11-m2_reserve.png`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The game's ini table is not there to ask**
- **Found during:** Task 1 (the engine tier's first run: "lists no action types")
- **Issue:** the plan reads actions.ini "through the data storage" as the MFC editor does (`OpenIniDataTable`), but the game's Zig StreamIO port answers `OpenIniDataTable` with nothing (`StreamIOZig/legacy_bridge.cpp`), so the table was null.
- **Fix:** `LoadActionCommands` reads the stream's text and `ParseActionsIni` applies the rules `CIniFile::LoadTables` has (trimmed lines, `;` comments, the first row only, a repeated name keeping its first place and its last value, `atoi` values), so the list is the one the MFC dialog showed.
- **Files modified:** `session_records.cpp`
- **Commit:** `14fffc117`

**2. [Rule 2 - Missing critical functionality] A unit of a start command must be a unit**
- **Found during:** Task 1 (threat T-04-11-02)
- **Issue:** the plan's rule "every unit an existing object" would let a building or a tree into a command; the game hands every unit to a group command, and the MFC editor only guarded this when adding.
- **Fix:** a NEW unit must be an object the database knows whose game type is a unit or a squad (`WhyNotAStartUnit`); units a command already holds are not judged again.
- **Files modified:** `session_records.cpp`, `fake_bridge.zig` (`markNonUnitFixture`)
- **Commit:** `14fffc117`

**3. [Rule 2 - Missing critical functionality] A file's own odd records must come back from an undo**
- **Found during:** Task 1 and Task 3 (the 04-09/04-10 pattern)
- **Issue:** the rules above would refuse the undo of a delete of a record a shipped or hand-made map holds that names a missing object or an unlisted type.
- **Fix:** `openedStartCommands` and `openedReservePositions` (taken at open): such a record is always accepted back by an add, a set is judged only on what it changes, and a new record like it is refused. Tested with a scratch map holding an odd command and an odd position.
- **Files modified:** `session.h`, `session.cpp`, `session_records.cpp`, `fake_bridge.zig`
- **Commit:** `14fffc117`, `93a359d20`

**4. [Rule 1 - Bug] The typed stats lookup would have read a soldier as a vehicle**
- **Found during:** Task 3
- **Issue:** `NGDB::GetRPGStats<SMechUnitRPGStats>` static_casts when its asserts are compiled out (as they are in the engine build), so asking the role of an infantry type would have read the wrong struct.
- **Fix:** the role and the towing numbers are read with `dynamic_cast` on `IObjectsDB::GetRPGStats`, as the MFC editor does.
- **Files modified:** `session_records.cpp`
- **Commit:** `93a359d20`

**5. [Rule 1 - Bug] The Delete key would have deleted the selected object too**
- **Found during:** Task 2
- **Issue:** the plan has the Start Commands window delete the selected command on Delete "while the panel is focused"; but a focused window without an active item does not capture the keyboard, so the view also handed Delete to the Select tool, which deletes the object selected on the map (the very unit the command is for).
- **Fix:** `View.delete_claimed`, set by the window each frame while it is focused and cleared by `panels.draw`, keeps Delete and Backspace from the tool; every other key stays the view's (capturing the whole keyboard would have swallowed undo and the camera keys). The first attempt, `igSetNextFrameWantCaptureKeyboard`, did exactly that and failed the scenario's undo.
- **Files modified:** `view.zig`, `panels.zig`, `panels_m2.zig`
- **Commit:** `32e705e6e`

**6. [Rule 3 - Blocking] The new tools' palette buttons moved the M1 reference frame**
- **Found during:** Task 2 (`map-editor-auto`: `compare=edited` 1.85% differ, limit 1%)
- **Issue:** a registry tool is a palette button and a Tools menu entry; Start Target only works with a command chosen in the window, and the extra row moved the tool palette.
- **Fix:** an Entry flag `hidden` (no palette button, no menu entry, no key; `tool=<label>` and the panel still reach it) for `start_target` and `reserve_positions`; the M1 frame then differs by 0.5-0.65%.
- **Files modified:** `tool_registry.zig`, `panels.zig`
- **Commit:** `32e705e6e`, `c49e61b2c`

**7. [Rule 1 - Bug] The engine test read the message after the next call had cleared it**
- **Found during:** Task 3 (the first engine run: "deleting the gun erases its position and says so")
- **Issue:** the check called `ReservePositionCountOf` (a bridge call, which clears the message) before reading `BkEditorLastMessage`.
- **Fix:** the message is captured first; it then reads "also reserve position 0 erased".
- **Files modified:** `tools/zig/editor_bridge_test.cpp`
- **Commit:** `93a359d20`

**8. [Rule 3 - Blocking] The scenario and the game run needed a gun a truck can tow**
- **Found during:** Task 4
- **Issue:** the first towed gun of the catalogue (10.5-cm_Flak38, 14600) is heavier than the first trucks (AEC_Matador_Cargo_GB pulls 9000, M3A1_Scout_Car_USA 6000), so the bridge rightly refused the pair, both in the scenario and in game-reads-it.
- **Fix:** the scenario uses a new `placer_name:Sdkfz_8` (24000) beside `placer_role:towed`, and picks the placed units with clicks a little above the points that placed them (a placed unit is drawn above its ground point); game-reads-it tries trucks from the end of the catalogue backwards (up to eight), taking the first the bridge accepts (Voroshilovets, link 284).
- **Files modified:** `commands.zig`, `build.zig`, `game_reads_m2.zig`
- **Commit:** `c49e61b2c`

### Plan wording resolved

- **`the fake's start-command model becomes records.StartCommand-based`:** the fake keeps every field of the record in a fixed-capacity value (16 units) and converts at the record calls, so the 04-02 cascade tests, which snapshot the list by copy and compare it after undo and redo, stay as they were; the rules are the real ones.
- **The Start Commands panel is a floating window** (Unit -> Start commands...), like Groups and Script; the Reserve positions panel takes the left column while its tool is in hand, like Script areas.
- **Start Target and Reserve Positions click on the release**, as the MFC editor does (OnLButtonUp), not on the press.
- **"red lines always on while the StartTarget tool or the panel's selection is active"** is the visibility rule `visible(set, start_commands, tool kinds) or (window open and a command selected)`; with View -> Markers at its default every kind is on anyway.
- **"place a towed gun and a truck through the palette commands the M1 scenario uses":** the M1 scenario has none; two small commands (`placer_role`, `placer_name`) set the Place tool's object.
- **`Editor.addUnitToStartCommand/removeUnitFromStartCommand`** are in Task 1's commit (with the other Editor helpers) rather than Task 2's; their tests run in the same core tier.
- **Commit trailer:** `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, as in 04-01, 04-09 and 04-10: the model that ran this plan, not the line the sequential-execution message names.

### Not a plan change

- The M2 scenario segment hides the Groups window (left open by 04-09) before the start-command shots so the red line's end is visible; the scripted scenario numbers after 233 moved (`saveas` at 305 now).
- `BkEditorStartCommand`'s sizing pass is REFUSED when the command has units (as a buffer too short is) and `c_bridge.readStartCommand` takes the count from it, as the 04-10 note asked.

---

**Total deviations:** 8 auto-fixed (two Rule 1, two Rule 2, one Rule 1 test fix, three Rule 3) plus wording resolutions
**Impact on plan:** no scope change; deviations 2-5 are correctness requirements the plan's rules implied (a start command that cannot crash the game loader, an undo that cannot drift, a role check that cannot misread stats, a Delete key that cannot delete the unit too).

## Issues Encountered

- Each C++ change costs a four-minute engine-tier run (the bridge test plays the whole 04-01..04-11 suite); the ini, message-clearing and towing problems were each found by one.
- `map-editor-auto-m2` flaked once on an early road predicate (`vso_points:road:3`) while a different run was starting; the rerun passed without a change, as the orchestrator's note said.
- The Zig `sed -i` on macOS needs a suffix argument; edits were made with Python instead.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-12 (AI general) can add its record the same way: a singleton or index-keyed kind rides `recordKeys/readRecord/putRecord/insertRecord/removeRecord`, a tool with a list of its own gets a left-column panel and a `hidden` registry entry, and `Unit` is now a top-level menu it can extend.
- `placer_role`, `placer_name`, `reserve_pending`, `startcmd_*` and the gun/truck shots are what 04-13's final walk-through can reuse.
- The game's `BK_MAP_TRACE reserve applied` and `startcmd launched` lines now have a nonzero baseline-plus-one proof in `game_reads_m2.zig`.

## Threat surface

No new surface beyond the plan's model: T-04-11-01 - `ValidateReservePosition` refuses a squad, a non-unit and link ID 0 in either role, tested per rule on the real catalogue; T-04-11-02 - units must be units or squads above 0 that exist, duplicates and held-back units handled (refused and warned), the cascade removes deleted units; T-04-11-03 - capacities, finite numbers, an explosion flag of 0 or 1 and index ranges are BAD_ARGUMENT, refusals change nothing (bytes compared); T-04-11-04 - a missing or empty actions.ini is REFUSED with a message, the window says so and Add start command is disabled (the fake's `no_action_list`). The ini text is parsed by a bounded reader (trimmed lines, no path built from it).

## Self-Check: PASSED

- Files found: `bridge.h`, `bridge.cpp`, `session.h`, `session_records.cpp`, `records.zig`, `bridge.zig`, `editor.zig`, `fake_bridge.zig`, `tools_ai.zig`, `c_bridge.zig`, `c_bridge_test.zig`, `view.zig`, `tool_registry.zig`, `panels.zig`, `panels_m2.zig`, `markers.zig`, `commands.zig`, `game_reads_m2.zig`, `editor_bridge_test.cpp`, `build.zig`.
- Commits found: `14fffc117`, `32e705e6e`, `93a359d20`, `c49e61b2c`.
- Acceptance greps: `BkEditorActionCommands|BkEditorAddStartCommand|BkEditorSetStartCommand|BkEditorDeleteStartCommand` in bridge.h at least 4; `start_command` in records.zig at least 1; `StartTarget` in tools_ai.zig at least 2; `drawStartCommands` in panels_m2.zig and panels.zig 2; `ValidateReservePosition` in session_records.cpp at least 2; `BkEditorReserveRole|BkEditorAddReservePosition` in bridge.h at least 2; `ReservePositions` in tools_ai.zig at least 2; `drawReservePositions` in panels_m2.zig and panels.zig 2; `std::min|std::max` added 0.
- The executor looked at `m2_startcmds` and `m2_reserve` (as PNG) and recorded what was seen (D2, D3).

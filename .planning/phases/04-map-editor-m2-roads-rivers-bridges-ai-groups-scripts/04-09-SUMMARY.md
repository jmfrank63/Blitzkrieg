---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 09
subsystem: map-editor
tags: [map-editor, m2, script-ids, reinforcement-groups, hide-checked, record-add-delete, bk-editor-auto, editor-bridge, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords (Put/EraseReinforcementGroup, FirstFreeGroupID, SetObjectScriptID), the generic record_edit command and its vtable pair, the byte-identity rule (read fresh, compare session saves)
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 04
    provides: the M2 scenario and game-reads-it harness, the BK_MAP_TRACE group line and its parser
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 08
    provides: the edit chain in game_reads_m2.zig, the M2 scenario up to the trench, a green mid-phase CI
provides:
  - BkEditorObjectRecord.script_id and BkEditorSetObjectScriptID (-1..32000, both map copies, engine untouched)
  - Editor.setScriptID (merge by gesture, drop on return), history script_id, the Properties panel Script ID field, commands and predicate script_id
  - records.Kind.group with owned script-ID lists, history record_add and record_delete, vtable recordKeys, insertRecord, removeRecord, firstFreeGroupID, Editor.addRecord/deleteRecord/newGroup/addScriptIDToGroup/removeScriptIDFromGroup/deleteGroup/groupIDs/groupScriptIDs
  - C ABI BkEditorGroupIDs, BkEditorGroup, BkEditorSetGroup, BkEditorDeleteGroup, BkEditorFirstFreeGroupID, BkEditorSetHiddenScriptIDs
  - the Reinforcement groups window (Map menu) with hide boxes, New, Delete, script IDs, Select objects, group markers, "N objects hidden" in the status bar
  - commands group_new, group_add_id, group_remove_id, group_delete, group_hide, group_select, groups_window; predicates group_has, groups_delta, hidden_count
  - the game holding back a grouped unit, proven in game-reads-it (group id=900 held=1)
affects: [04-10, 04-11, 04-12, 04-13]

commits: 3
plan_head_before: a6416b46dda0eb57c95a7e1f64f5a7303314d761
actuals:
  tokens: 34400
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "The generic record path grows list kinds by four vtable entries (recordKeys, insertRecord, removeRecord and, per kind, a helper such as firstFreeGroupID); undo of an add removes, undo of a delete re-inserts the value read before the removal, an edit is still record_edit"
    - "A put's validation exempts what the file already held (the session keeps openedGroups): only what an edit ADDS is held to 0..32000, once, so an undo can always put back a file's own odd data"
    - "Hide checked is the MFC editor's own RemoveFromScene/AddToScene, run around the world update: hidden objects come back for UpdateNow and leave again, so the world never moves an object the scene does not hold"
    - "The panel defers every command to after its loops over the group rows: a command re-reads the rows and frees the slices a loop still holds"

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/session_records.cpp
    - Sources/src/EditorBridge/session_groups.cpp
    - Sources/src/EditorBridge/world.h
    - Sources/editor/core/records.zig
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/history.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/markers.zig
    - Sources/editor/app/commands.zig
    - Sources/editor/app/game_reads_m2.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig

key-decisions:
  - "Hide checked takes the object out of the scene (CWorldBase::RemoveFromScene/AddToScene through two public wrappers on CEditorWorld), not opacity 0: the MFC editor did exactly this, and measured on a tank the opacity route left its mesh shadow and health bar standing where it had been (the mesh shadow pass and the icons ignore a visual's opacity). Picking still skips hidden link IDs, so it does not depend on whether the scene finds what it no longer holds (assumption A6 never arises)"
  - "UpdateSessionWorld shows the hidden objects before the world update and hides them again after (ShowHiddenForUpdate, ApplyHiddenMarks): the update moves and re-textures objects the scene must hold. It is the only place the world updates"
  - "A group put validates only what it ADDS (0..32000, once); an ID the group holds now, or held when the file was opened, is exempt as many times as it held it. The plan's flat rule would make the undo of removing one of a file's own duplicate or out-of-range IDs fail as a drift; the session keeps a copy of the opened groups (openedGroups) for it"
  - "The Group Manager is a floating window opened from Map -> Reinforcement groups..., not a fifth panel in the right column: the four panels there already fill it, and the MFC dialog was modeless too"
  - "Hide checked hides objects of the objects list with a link ID other than 0 (an object with link ID 0 names no one engine object, C11); the panel keeps the checked groups, and the bridge is handed the sorted, unique union of their script IDs whenever it changes, so an edit of a group, its undo and a group's deletion all follow the hiding"
  - "A group's ID is the record key, and Value.group.id must equal it (bad_argument otherwise); BkEditorSetGroup creates or replaces, so the insert-refusing-a-taken-ID rule lives in c_bridge's insertRecord (with an adapter-owned message), and the fake mirrors it"
  - "Select objects marks with a ring at the object's z = 0 ground point (like every other marker, Pitfall 17), selects the first object in the Select tool and says how many in the status bar; it does not move the camera"
  - "The scenario's group is group 0: coldwinter holds no reinforcement group, so New from 0 takes 0 (the plan's <new id>); game-reads-it uses group 900 and fails if the first free ID at or above 900 is not 900 (04-10's Lua fixture names it)"

patterns-established:
  - "Engine-tier hide measurement on pixels: a control pair of unchanged captures gives the noise, the changed count must exceed 1 % of the box and four times the noise, and the unhidden capture must be within the noise of the first (a shadow put back at the wrong opacity, or left behind, fails here)"
  - "Two saves of the same session compare bytes to prove file order (the same edits in another order), never a session save against a map written from another read (padding, 04-01)"

requirements-completed: [D-15, D-16, D-02, D-01]

coverage:
  - id: D1
    description: "D-15: the Properties panel has a Script ID field (-1 none, 0..32000) for the selected object; setting it is one undo step (merged within one gesture) and changes both map copies only (C7); a reopened saved map reports it through IAIEditor::GetObjectScriptID"
    requirement: "D-15"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'script ID: set, undo, redo, in the document and the bridge', 'an equal value records nothing', 'a value out of range ... is Refused and changes nothing', 'the edits of one gesture are one undo step, and one that returns to its start leaves none'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2ScriptIDs - set 4242 on object 192, BkEditorObjects reports it, the save equals the NMapRecords map, the reopened file gives GetObjectScriptID 4242 (editor-bridge: M2 script ids ok); set and set back saves the unedited bytes; 32001, -2, link 0 and an unknown link REFUSED unchanged"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 script id round trip ok (document and raw records agree, engine matches the map, undo and redo)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: a tank clicked, do=script_id:4244, expect=script_id:4244, undo_depth 12 (BK_EDITOR_AUTO: done, 148 actions)"
        status: pass
    human_judgment: false
  - id: D2
    description: "D-16: the Groups panel lists reinforcement groups by ID; New takes an ID field bumped to the first unused ID at or above it (C9); Delete removes a group; each group's script IDs can be added (duplicates skipped) and removed; each change is one undo step"
    requirement: "D-16"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'group: New takes the first unused ID at or above the field, one undo step', 'script IDs are added (a duplicate is skipped with a note) and removed, each one undo step', 'a refused add leaves the history, the generation and the group alone', 'delete takes the group and its IDs out, undo puts both back exactly, redo removes it again', 'New, add and delete all undone leave the map as it opened'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Groups on battleofbulge (12 groups): reads equal the file in ID and file order, sizing and short-buffer reads write nothing past the capacity, New from 0 takes the overlay's free ID, two IDs in, one out, another group gains one, a third is deleted, the save equals the NMapRecords map; -1, 32001, a duplicate, an unknown group, a negative ID or count REFUSED or BAD_ARGUMENT unchanged; a file's own odd group (-1, a duplicate, 40000) reads, loses an ID and is put back exactly (editor-bridge: M2 groups ok)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 groups round trip ok (real bridge: newGroup, add, remove, duplicate skipped, insert over a taken ID refused with 'already', delete and undo, all undone gives the group count at open)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: group_new:0 (groups_delta 1), group_add_id:0:4244 (group_has), group_remove_id, group_delete, undo x2 and x3 back to groups_delta 0 and undo_depth 11"
        status: pass
    human_judgment: false
  - id: D3
    description: "D-16: Hide checked hides, in the view and from picking, every object of the map's objects list whose script ID belongs to a checked group; Select objects selects the first such object and marks all of them"
    requirement: "D-16"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2HideGroups - a tree (object 192), a tank (object 48) on coldwinter and a squad (object 40) on arnheim: hiding changes 2493, 2213 and 244 of the 3600 pixels of the box (0 between two unchanged captures), a click no longer answers the object, unhiding answers again and the box differs 0 pixels from the first capture, the map saves the same bytes hidden and shown, a script ID that joins or leaves the hidden set hides or shows at once (editor-bridge: M2 hide checked ok)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto-m2: group_select:0 (status 'N object carries one; the first is selected'), group_hide:0:1 then expect=hidden_count:1, group_hide:0:0 then hidden_count:0"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-09-m2_groups_marked.png, 04-09-m2_groups_hidden.png, 04-09-m2_groups.png and 04-09-hide-*.png (from the .tga shots), inspected: the marked shot shows a magenta ring at the tank's base, Script ID 4244 in Properties and the count in the status bar; the hidden shot shows the window with the checked group, its script ID and Remove, the tank gone with no shadow or health bar left, and 'N objects hidden' in the status bar; the last shot shows the window with no groups after the undos"
        status: pass
    human_judgment: true
    rationale: "Whether the window reads well and the ring sits where a map maker expects it is a visual judgment; the pixel counts prove the object is gone and comes back"
  - id: D4
    description: "D-02: the generic record command gains record_add and record_delete (keyed by group ID here, by index in later plans), with undo writing the before-record back"
    requirement: "D-02"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: records.zig 'a group value owns, clones, compares and frees its script IDs' (testing allocator), editor.zig group tests (record_add undo removes, record_delete undo re-inserts the value read before)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: the same path through RealBridge (readGroup two passes, insertRecord, removeRecord)"
        status: pass
    human_judgment: false
  - id: D5
    description: "D-01: untouched groups are written byte-identically (sorted by key); add then undo of any group edit gives the unedited file"
    requirement: "D-01"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2Groups - the group edits and their inverses save the unedited file byte for byte; the same edits in another order (delete, edit, new) save the same bytes as the first time, so the groups go out in ID order; the refusals change nothing (unedited bytes)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The game reads it: a unit carrying a script ID that a new group holds is held back by the game (trace group id=G held=1)"
    requirement: "D-16"
    verification:
      - kind: integration
        ref: "zig build map-editor-game-reads-it-m2: 'placed 10.5-cm_Flak38 (link 279, script ID 4245) beside the start camera and put script ID 4245 in group 900'; zig-out/local-test/map-editor-game-reads-it-m2.log 'BK_MAP_TRACE: group id=900 held=1'; game reads it M2 PASS (... entrenchments 0 -> 1, group 900 held 1)"
        status: pass
    human_judgment: false

duration: 1h01m
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 09: Script IDs and reinforcement groups Summary

**Objects carry a script ID (-1 or 0..32000) edited from the Properties panel, reinforcement groups are edited record by record through the generic record command (now with add and delete) from a Group Manager window with Hide checked and Select objects, and the real game holds back a unit that a new group names.**

## Performance

- **Duration:** about 1 h (2026-09-30T04:09:53Z to about 05:10Z), of which about 40 min was waiting on engine and app builds (each C++ change costs a three-minute rebuild of the bridge tier)
- **Tasks:** 3 (one tracer, two auto)
- **Files:** 21 changed, 2637 insertions, 21 deletions

## Accomplishments

- **Script ID** end to end: `script_id` appended to `BkEditorObjectRecord`, `BkEditorSetObjectScriptID` (unknown, shared and link-0 objects refused, range checked, both map copies, engine untouched, C7), `Editor.setScriptID` with gesture merge and drop-on-return, an "Script ID" field in Properties (committed with the pose, one undo step), the `script_id` command and predicate. The engine tier reads the value back the only way the AI allows: save, reopen, `IAIEditor::GetObjectScriptID` gives 4242.
- **Groups through the generic record path:** `Kind.group` with an owned script-ID list, `record_add` and `record_delete` in the history, four new vtable entries, `Editor.newGroup/addScriptIDToGroup/removeScriptIDFromGroup/deleteGroup`, six C entry points, the fake's groups and the adapter's two-pass reads. The put rule (0..32000, once each, what the file held exempt) is one function in `session_records.cpp`.
- **Hide checked and Select objects:** `BkEditorSetHiddenScriptIDs` and the session's hidden set; objects leave and re-enter the scene the way the MFC editor's own check boxes did; picking skips them. The Groups window, the group markers, the status-bar count and the seven commands and three predicates.
- **Evidence:** all four tiers, the M2 scenario segment (script ID, New, Add, Select objects, Hide, Remove, Delete, five undos) and a game that reports `group id=900 held=1`.

## Task Commits

1. **Task 1: Script ID end to end (tracer)** - `5e48758b4` (feat)
2. **Task 2: Reinforcement groups through the generic record command** - `ad09b20e5` (feat)
3. **Task 3: Groups window, Hide checked, Select objects; scenario and game segments** - `f4db3f073` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: Task 1's `<verify>` is automated-only; `test-editor-core test-map-editor-panels` rc 0 and `test-editor-bridge test-map-editor-engine map-editor-host-check` rc 0 with all five named lines (7 matches) before Task 2 began: "Tracer verified end-to-end - expanding".

## Verification (macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` after each build.zig edit | 3/3 pass |
| `test-editor-core` | 148 tests pass (10 new: 4 script ID, 5 group, 1 records) |
| `test-map-editor-panels`, `-view`, `-view-events`, `-testlaunch` | rc 0 |
| `test-editor-bridge` | `M2 script ids ok`, `M2 groups ok`, `M2 hide checked ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 script id round trip ok`, `M2 groups round trip ok`, `PASS (260 objects)` |
| `map-editor-host-check`, `map-editor-smoke` | `panel smoke PASS`, `smoke PASS (52 steps)` |
| `map-editor-auto` (M1) | 0.5519 % against the refreshed reference, done (13 actions) |
| `map-editor-auto-m2` | `BK_EDITOR_AUTO: done (148 actions)` |
| `map-editor-game-reads-it-m2` | `game reads it M2 PASS (... group 900 held 1)`, trace `BK_MAP_TRACE: group id=900 held=1` |

Logs are under `zig-out/local-test/04-09-*.log`; the hide captures and the scenario shots (as PNG) are `zig-out/local-test/04-09-*.png`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Hide by opacity 0 left a tank's shadow and health bar, and a shadow restored at 255 was black**
- **Found during:** Task 3 (the engine-tier pixel test, then the scenario shot)
- **Issue:** the plan (research Q3) hides by setting the visuals' opacity to 0. The first version did, and the pixel test caught two faults: unhiding put the shadow back at 255 where `CWorldBase::AddToScene` gives shadows 0x80 (a black shadow; 1742 of 3600 pixels off the first capture), and the scenario shot showed a hidden tank's mesh shadow and health bar still drawn, since the mesh shadow pass and the icons ignore a visual's opacity.
- **Fix:** hidden objects leave the scene, as the MFC editor's own check boxes did (`TemplateEditorFrame1.cpp`, `RemoveFromScene`/`AddToScene`): two public wrappers on `CEditorWorld`, the objects come back for the world's update and leave again (`ShowHiddenForUpdate`, `ApplyHiddenMarks`). The test now measures a tree, a tank and a squad: hide changes the box, unhide restores it to 0 differing pixels.
- **Files modified:** `world.h`, `session.cpp`, `session_records.cpp`, `bridge.h`, `tools/zig/editor_bridge_test.cpp`
- **Commit:** `f4db3f073`

**2. [Rule 2 - Missing critical functionality] The put rule broke undo on a file's own odd data**
- **Found during:** Task 2 (the odd-group case of `TestM2Groups`)
- **Issue:** the plan's rule (an ID outside 0..32000 or a duplicate is REFUSED) applied flat, and then exempted by "what the group holds now", still refuses the undo of removing one of two duplicate IDs a file holds (the group no longer holds the second copy). The undo would fail as a drift.
- **Fix:** the session keeps the groups as opened (`openedGroups`); a put may keep any ID, and as many copies, that the group holds now or held at open. What an edit adds is still held to 0..32000, once. Tested on a scratch map with a group `{7, -1, 7, 40000}`.
- **Files modified:** `session.h`, `session.cpp`, `session_records.cpp`, `tools/zig/editor_bridge_test.cpp`
- **Commit:** `ad09b20e5`

**3. [Rule 3 - Blocking] Byte comparison against a map written from another read**
- **Found during:** Task 2 (`TestM2Groups`)
- **Issue:** the first version compared the edited save with `NMapFile::Write` of an `NMapRecords`-built map read fresh; even the unedited save differed from a fresh read written (first difference at byte 444089, `SVertexAltitude` padding: the session snapshot is a copy, 04-01).
- **Fix:** the byte tests compare two session saves only (edits and their inverses; the same edits in another order, which is what proves the ID order); `AreEquivalent` against the built map stays for content.
- **Commit:** `ad09b20e5`

### Plan wording resolved

- **`<new id>` in the scenario** is 0: coldwinter holds no group (printed by the engine tier: the first free ID from 0 is 0, from 900 is 900).
- **Panel placement:** the plan says a Groups panel with `drawGroups` in `panels_m2.zig`; it is a floating window (Map menu), see key-decisions. `drawGroups` is called from `panels.draw`.
- **`firstFreeGroupID` in the vtable:** the plan lists the C ABI call and the vtable entries `recordKeys/insertRecord/removeRecord`; the core takes the first free ID from the bridge (C9 lives in `NMapRecords`), so the vtable has a fourth entry. A superset.
- **`document.zig`** is not touched: `Document.find` returns a pointer, so `script_id` is updated in place in `ObjectRecord`.
- **Squad measured on arnheim**, not coldwinter (no squad of coldwinter is picked by a click with the camera on it); the run says so for any map where it finds none.
- **Game-reads-it unit** is the first placeable unit of the catalogue, `10.5-cm_Flak38` (link 279).

### Not a plan change

- **M1 compare 0.5519 % (was 0.29-0.37 %):** the new Script ID row in the Properties panel (inspected on the crop). The local reference was refreshed; the old one is kept as `zig-out/local-test/04-09-reference-edited-before-script-id.tga`.
- **One flaky host-check run:** `smoke FAIL: a click on the tile combo opens the picker` (the window losing focus on the shared desktop) passed unchanged on rerun, as the run notes said it could.
- **Process note:** the commit trailer is `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, as in 04-01: the model that ran this plan, not the line the sequential-execution message names.

---

**Total deviations:** 3 auto-fixed (Rule 1, Rule 2, Rule 3) plus wording resolutions
**Impact on plan:** no scope change; deviation 1 changes how Hide checked is done (same behaviour, complete instead of partial).

## Issues Encountered

- The three-minute C++ rebuild of the bridge tier made every engine-tier fix expensive; the two engine-only fixes (unhide restore, the odd-group put) were each found by the test the plan asked for.
- `ffmpeg` reads the editor's 32-bit TGA shots as blank; the shots were converted as raw BGRA (`-f rawvideo -pixel_format bgra`), which the 04-08 conversions must have done too.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-10 (script file, script areas) can rely on group 900 as its Lua fixture's group: game-reads-it already creates it and fails if the ID is not free. The record kinds `script_file` and `script_area` add an arm to `records.zig`, the two adapters and their C calls; `insertRecord/removeRecord/recordKeys` are the path for area lists keyed by index.
- The Groups window and its `groups_window` command are the pattern for the later windows (areas, start commands): a floating window opened from a menu, every control a command, a deferred action after the loops.
- `hidden_count` and the status-bar count only cover the document's objects list; scenario objects are never hidden, as the game never holds them back.

## Threat surface

No new surface beyond the plan's model: T-04-09-01 - script IDs -1..32000 for objects, 0..32000 for group members, group IDs >= 0, every refusal changing nothing (tests); T-04-09-02 - -1 can never be added to a group (the put refuses it, and `addScriptIDToGroup` refuses it in the core first); T-04-09-03 - every two-pass read checks its capacity and writes nothing past it, `BkEditorSetGroup` and `BkEditorSetHiddenScriptIDs` bound the count at 65536 as a bad argument. `BkEditorSetHiddenScriptIDs` is one new entry point, view-only, never saved.

## Self-Check: PASSED

- Files found: `bridge.h`, `session_records.cpp`, `world.h`, `records.zig`, `panels_m2.zig`, `commands.zig`, `game_reads_m2.zig`, `editor_bridge_test.cpp`, `build.zig`.
- Commits found: `5e48758b4`, `ad09b20e5`, `f4db3f073`.
- Acceptance greps: `script_id` in bridge.h 4 and `BkEditorSetObjectScriptID` 1; `Script ID` in panels.zig 3; `record_add|record_delete` in history.zig 4; `recordKeys|insertRecord|removeRecord` in bridge.zig 6; `BkEditorSetGroup|BkEditorDeleteGroup|BkEditorFirstFreeGroupID` in bridge.h 3 lines; `hiddenScriptIDs` in session.h and session.cpp 4; `drawGroups` in panels_m2.zig and panels.zig 2; `std::min|std::max` added 0.
- The executor looked at `m2_groups_marked`, `m2_groups_hidden`, `m2_groups` (as PNG) and the hide captures and recorded what was seen (D3).

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 02
subsystem: map-editor
tags: [map-editor, m2, cascade-delete, find-references, mapfile, editor-bridge, undo, zig, cpp]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: NMapRecords (InsertStartCommand, InsertReservePosition, PutReinforcementGroup, SetObjectScriptID, InsertBridgeEntry, InsertEntrenchment), the byte-identity rule (fresh reads for every compared write)
provides:
  - NMapOverlay::DeleteObject as the MFC editor's cascade (start-command units and targets, reserve positions, script-ID notes) with SCascade on SDeletedObject
  - RestoreObject that reverses every change in reverse order, byte-exact
  - DescribeCascade, the status-bar line, carried by BkEditorLastMessage
  - a FindReferences that matches groups and mobile reinforcements by script ID and finds entrenchments, reserve positions and targets; link ID 0 finds nothing
  - Editor.delete status note; fake bridge with bridge spans, trench pieces, start commands and reserve positions
affects: [04-03, 04-04, 04-05, 04-06, 04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 3
plan_head_before: 8db94461ff6bd93e4817133d012ea1e812c7e00e
actuals:
  tokens: 21200
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A compound edit is one overlay call that returns its own undo record (SDeletedObject with SCascade); the session keeps one per copy in the tombstone and the ABI stays one Delete and one Restore"
    - "Cascade changes are recorded with the list position at the moment of the change and undone in reverse; a message for the player converts back to the positions from before the delete"
    - "Refusal is a separate predicate (WhyRefused) from the reference list (FindReferences): what refuses protects a loader, what is merely named is edited"

key-files:
  created: []
  modified:
    - Sources/src/MapFile/MapOverlay.h
    - Sources/src/MapFile/MapOverlay.cpp
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - tools/zig/map_file_test.cpp
    - tools/zig/editor_bridge_test.cpp

key-decisions:
  - "A start command that lists the deleted unit and is left empty is erased whatever its target was; a command that only names the object as target keeps its units and gets target 0; a command with units it keeps and the object as both unit and target gets both edits"
  - "Only the vehicle direction of the passenger rule refuses (another object's nLinkWith names the deleted one). The plan lists 'carries a passenger' and 'is named as a passenger's container' separately, but they are the same relation; deleting a passenger changes nothing the vehicle holds"
  - "Link ID 0 never cascades and never refuses; the overlay still deletes the first link-0 object if asked, but RestoreObject of a shared 0 refuses (another 0 is 'in use'), so restore proofs run on unique IDs and the session refuses shared IDs before the overlay is reached"
  - "Group and side names in notes are sorted ascending, so the message does not depend on the hash map's order"
  - "The fake words its summary one change at a time; the real one groups ('start commands 2 and 5'). Tests match parts of the message, never the whole real one"

patterns-established:
  - "Compound-edit tests: RunM2Case for the byte-exact inverse, then the rule itself checked on one more fresh read (RunCascadeCase / RunCascadeRefusal)"

requirements-completed: [D-04, D-01, D-02, D-25]

coverage:
  - id: D1
    description: "Deleting an object removes it from every start command's units, erases a command left with no unit, clears a command's target to 0, and erases every reserve position naming it as artillery or truck; groups and mobileScriptIDs are never edited and a note says when a script ID they name has no object left"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-map-files: tools/zig/map_file_test.cpp#TestM2CascadeKinds (map-file: M2 cascade kinds ok)"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2CascadeDelete (editor-bridge: M2 cascade delete (all kinds) ok)"
        status: pass
    human_judgment: false
  - id: D2
    description: "One delete is one undo step; undo restores the object and every reference exactly (the saved file equals an unedited save); redo repeats the same cascade"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'the full cascade: units, targets and reserve positions, one undo step, exact undo and redo'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: TestM2CascadeDelete byte comparison (a cascade delete and its restore save the unedited file byte for byte)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 delete round trip ok"
        status: pass
    human_judgment: false
  - id: D3
    description: "A bridge span, a trench piece and an object carrying a passenger are still refused with a status note, leaving document and history unchanged"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'a span and a trench piece are refused with the document and history untouched'"
        status: pass
      - kind: unit
        ref: "zig build test-map-files: TestM2CascadeKinds refusal cases (span, trench piece, passenger, span also named by a start command)"
        status: pass
    human_judgment: false
  - id: D4
    description: "FindReferences matches groups by script ID (never link ID), finds entrenchment pieces, reserve positions, start-command units and targets and mobileScriptIDs, and never treats link ID 0 as a reference"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-map-files: tools/zig/map_file_test.cpp#TestM2FindReferences (map-file: M2 find references ok)"
        status: pass
    human_judgment: false
  - id: D5
    description: "The status bar tells the player what a delete also changed"
    requirement: "D-04"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: the cascade tests assert the status text; the engine tier asserts 'start command', 'reserve position' and 'script ID 77' in BkEditorLastMessage"
        status: pass
    human_judgment: true
    rationale: "Whether the wording reads well in the status bar (256 bytes, one line) is a judgment; the tests prove the parts are present, not how it reads to a map maker"

duration: 19min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 02: Cascade delete Summary

**Deleting an object that start commands and reserve positions name now cascades as the MFC editor does (units removed, empty commands erased, targets cleared to 0, reserve positions erased, script-ID notes) in one byte-exact undo step, with a FindReferences that matches groups by script ID and finds entrenchments, reserve positions and mobile reinforcements.**

## Performance

- **Duration:** about 19 min (start not recorded at the moment; taken from the first baseline run at 05:42 local and the plan read just before)
- **Completed:** 2026-09-30T05:57+07:00
- **Tasks:** 3 (one tracer, two auto)
- **Files modified:** 9, 1252 insertions, 63 deletions

## Accomplishments

- `NMapOverlay::DeleteObject` (`MapOverlay.cpp`): refuses only for a bridge span ("still referred to by bridge N"), a trench piece ("still part of entrenchment N") and a vehicle a passenger points at; otherwise removes the object and applies the cascade in a fixed order, recording every change with its position in an `SCascade` on `SDeletedObject`. `RestoreObject` undoes it in reverse, then puts the object back.
- `DescribeCascade`: "also removed from start command 1; start command 0 erased; target of start command 2 cleared; reserve position 0 erased; script ID 77 is still used by reinforcement group 5" (positions are the ones before the delete).
- `FindReferences` rewritten: groups and the AI general's mobile reinforcements by the object's script ID, plus entrenchment pieces, reserve positions, start-command units and targets; link ID 0 returns nothing.
- Session and C ABI: `DeleteObjectFromSession` already applied the overlay to both copies and kept both records in the tombstone, so the cascade rides along; it now puts `DescribeCascade` into `szMessage`. `BkEditorDeleteObject`'s doc comment states the cascade and the refusals. No ABI signature changed.
- Core: `Editor.delete` (and the redo path) copies a non-empty bridge message into the status as a note; the next command's outcome replaces it.
- Fake bridge: `bridge_spans`, `trench_pieces`, `start_commands`, `reserve_positions` with fixtures; the same cascade in the same order; tombstones carry their changes.
- Tests: core cascade, refusal and note-replacement tests; `TestM2FindReferences`, `TestM2CascadeKinds` (12 cascade cases, 4 refusals, byte-exact restore each); `TestM2CascadeDelete` in both scopes on the real engine; a delete/undo/redo/undo round trip in the Zig engine tier.

## Task Commits

1. **Task 1: Delete a unit named by a start command (tracer)** - `dc249465a` (feat)
2. **Task 2: The whole cascade and FindReferences** - `5f23acda1` (feat)
3. **Task 3: Fake models reserve positions and trench pieces; core tests** - `1f95fd866` (feat)

**Plan metadata:** the docs commit that carries this file.

Tracer gate: the tracer's `<verify>` is automated-only, so it was re-run end to end under the end-of-phase mode; core, engine bridge and map-file all passed and the plan expanded.

## Verification (plan level, macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `test-editor-core` | 100 tests pass (3 new in Task 3, 1 in Task 1) |
| `test-map-files` | `M2 find references ok`, `M2 cascade kinds ok`, `M2 record ops ok (87 cases)`, `map-file: PASS`, sweep 66/66, 0 FAIL |
| `test-editor-bridge` | `M2 cascade delete (start commands) ok`, `M2 cascade delete (all kinds) ok`, `editor-bridge: PASS` |
| `test-map-editor-engine`, `map-editor-smoke` | `M2 delete round trip ok`, `PASS (260 objects)`, `smoke PASS (52 steps)` |
| `test-map-editor-panels`, `-view`, `-auto`, `-testlaunch`, `-view-events` | pass |

Logs are under `zig-out/local-test/04-02-*.log`. The engine-tier log ends with a `failed command:` line after `PASS` with exit code 0; the 04-01 logs show the same line, so it is the test runner's echo, not a failure.

## Files Created/Modified

See the frontmatter `key-files`. No new files.

## Decisions Made

See `key-decisions`. Two worth repeating for the plans that follow:

- A later plan that adds a record naming an object (a start command tool, reserve positions, an M3 link) must add its rule to the cascade in `DeleteObject` and to `FindReferences` together, and give `SCascade` a vector of changes for it; `RestoreObject` and `DescribeCascade` follow the same shape.
- The status bar is 256 bytes (`status_buffer`). A delete that touches many commands is cut at that length by the core; the bridge's message is not limited. Accepted; the M2 panels can show the whole message if it matters.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] TestObjectOverlay's old refusal assertion had to change in Task 1, not Task 2**
- **Found during:** Task 1 verify (`map-file: PASS` is in Task 1's own count)
- **Issue:** the plan schedules the update of `TestObjectOverlay`'s referenced-object block in Task 2, but the first engine build after Task 1 failed `map-file` on the old assertion ("a referenced object refuses to go"), and Task 1's verify counts `map-file: PASS`.
- **Fix:** rewrote the block in Task 1: an object a start command names cascades and restores to an equivalent map; a bridge span is still refused, names the bridge and changes nothing. Coldwinter names no object that way, so the records are laid over a copy through `NMapRecords`.
- **Files modified:** `tools/zig/map_file_test.cpp`
- **Committed in:** `dc249465a`

**2. [Rule 1 - Bug in the test design] Link ID 0 case could not restore**
- **Found during:** Task 2 (`TestM2CascadeKinds`)
- **Issue:** deleting an object with link ID 0 through `RunM2Case` failed on the inverse: `RestoreObject` refuses while another object carries the ID, and coldwinter holds several link-0 objects. Not a cascade bug (the session refuses shared IDs before the overlay, so it never restores one).
- **Fix:** the restorable case became "link ID 0 in a command survives another unit's delete"; the delete of a link-0 object itself is checked without a restore.
- **Committed in:** `5f23acda1`

**3. [Rule 1 - Bug] Fake summary said "removed from" for a target-only edit and dropped its tail**
- **Found during:** Task 3 (full-cascade core test)
- **Issue:** the fake reported every non-erased command as a unit removal, and its 160-byte message buffer cut the last reserve position off.
- **Fix:** `unit_removed` on the change record; the buffer is 384 bytes.
- **Files modified:** `Sources/editor/core/fake_bridge.zig`
- **Committed in:** `1f95fd866`

### Plan wording resolved

- The plan's refusal list names "an object that carries a passenger" and "an object that is itself named as a passenger's container" as (c) and (d); they are one relation (another object's `nLinkWith` names the deleted one), so there is one refusal (see key-decisions).
- The plan says the fake's Task 1 cascade is "the same start-command cascade" as the C++ Task 1 (units only); target clearing and reserve positions arrived in the fake in Task 3, as the plan's Task 3 says.

---

**Total deviations:** 3 auto-fixed (1 Rule 3, 2 Rule 1) plus one plan-wording resolution
**Impact on plan:** no scope change. Deviation 1 moves one test edit one task earlier.

## Issues Encountered

- A brace slipped out of `Numbered` while inserting the helpers in Task 1 (compile error, fixed at once, never committed).
- The start time was not recorded at the moment the plan began, so the duration is an estimate.
- `.planning/state.json` (modified) and `.gsd/` (untracked) were already dirty before this plan started and were left alone.

## Known Stubs

None. The fake bridge's one-change-at-a-time wording and its missing reinforcement groups are documented test-double simplifications in its header.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- 04-03 (app input, tool registry, markers, verbs) has a delete that no longer refuses for start commands, so the Delete key on a selected unit is safe to wire; the status note arrives through `Editor.status()`.
- Nothing blocks 04-03.

## Threat surface

No new surface beyond the plan's threat model: link ID 0 matches nothing (T-04-02-01, map-file case "link ID 0 ignored" and the cascade case that keeps a command's 0); span and trench-piece deletes stay refused with map-file refusal cases and the core refusal test (T-04-02-02); every restore reverses every recorded change, proved byte-exact on the map-file tier for each rule and on the engine tier for the combined cascade (T-04-02-03).

## Self-Check: PASSED

- Modified files found: `MapOverlay.h`, `MapOverlay.cpp`, `fake_bridge.zig` and the rest of the frontmatter list.
- Commits found: `dc249465a`, `5f23acda1`, `1f95fd866`.
- Acceptance criteria re-run: `SCascade|DescribeCascade` in `MapOverlay.h` (3), `start_commands` in `fake_bridge.zig` (11), `reserve_positions|trench_pieces` (15), `nScriptID` compared inside FindReferences (via `GroupsHolding`/`SidesHolding` on the object's script ID), and all three tasks' verify commands passing.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

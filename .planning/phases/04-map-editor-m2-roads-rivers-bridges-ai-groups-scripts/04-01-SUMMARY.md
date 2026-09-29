---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 01
subsystem: map-editor
tags: [map-editor, m2, mapfile, editor-bridge, record-edit, camera-anchors, palette-filter, zig, cpp]

requires:
  - phase: 03-map-editor-plan-6-finish-m1
    provides: the M1 chain (tool, Editor, bridge vtable, C ABI, session), the sound-list precedent, the four test tiers
provides:
  - NMapRecords, a record-level overlay for every M2 collection (the expected-value builder)
  - the generic record_edit command (core, fake bridge, c_bridge adapter) proven end to end by camera anchors
  - C ABI BkEditorCameraAnchors, BkEditorSetCameraAnchors, BkEditorGroundHeight
  - WhyNotPlacedByPalette and the palette filter for bridge spans, trench pieces and fences (D-05)
  - spec M2 scope and the planning amendments (D-24 split, corrections C1-C13)
affects: [04-02, 04-03, 04-04, 04-05, 04-06, 04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 3
plan_head_before: a1a9600dd6b65f030efaeb092def3762f7f01106
actuals:
  tokens: 41700
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Record kinds: one Kind/Value union in records.zig, one readRecord/putRecord vtable pair, one record_edit command; a later plan adds a kind and its C call, nothing else in the command or the undo"
    - "Overlay ops edit in place by list position (-1 appends), never renumber, and take an optional out-record so undo can put the record back at its index"
    - "Byte-identity tests read the map fresh for every write they compare, never a copy"

key-files:
  created:
    - Sources/src/MapFile/MapRecords.h
    - Sources/src/MapFile/MapRecords.cpp
    - Sources/src/EditorBridge/session_records.cpp
    - Sources/editor/core/records.zig
  modified:
    - Sources/src/MapFile/MapOverlay.h
    - Sources/src/MapFile/MapOverlay.cpp
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/EditorBridge/catalogue.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/history.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/core/root.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/c_bridge_test.zig
    - Sources/editor/app/panels_logic.zig
    - tools/zig/map_file_test.cpp
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
    - .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-DISCUSSION-LOG.md

key-decisions:
  - "The C ABI camera-anchor struct is BkEditorCameraAnchorRecord: the plan's BkEditorCameraAnchors is also the entry point's name, and a C typedef and a function share one namespace"
  - "Setting camera anchors validates only the slots the call changes: an anchor a file already holds off the map stays as it is and must not make every other edit of the vector refuse"
  - "Erase operations take an optional out-record so undo can re-insert the exact record at its index; SetPlayerCameraAnchor and ClearPlayerCameraAnchor return bool (false for a negative or absurd player) instead of void"
  - "NextVsoID is one above the highest road or river nID and at least 1; an empty map starts at 1, never 0"
  - "PutScriptFile is an exact put with no name check; IsBareScriptName is the check a NEW name passes, so undo can restore whatever a file held"

patterns-established:
  - "Palette filter apart from the map-object guard: WhyNotPlacedByPalette is asked by the catalogue and BkEditorAddObject only, never by WhyNotAMapObject, which also guards PlaceOneObject"
  - "Fresh reads for byte comparisons: SVertexAltitude is written as a raw struct, so a copied map's padding bytes differ from a read map's"

requirements-completed: [D-01, D-02, D-03, D-05, D-22, D-23, D-24, D-25]

coverage:
  - id: D1
    description: "NMapRecords covers every M2 collection with an in-place record-level operation; an edit and its inverse write the unedited file byte for byte, and every edit equals the map the same call builds"
    requirement: "D-01"
    verification:
      - kind: unit
        ref: "zig build test-map-files: tools/zig/map_file_test.cpp#TestM2RecordOps (87 cases) and #TestCameraAnchorRecords"
        status: pass
    human_judgment: false
  - id: D2
    description: "The generic record command (record_edit) goes Editor, bridge vtable, C ABI (Guarded), session, and undo puts the before-record back through the same call; camera anchors are the tracer"
    requirement: "D-02"
    verification:
      - kind: unit
        ref: "zig build test-editor-core: editor.zig 'camera anchor: ...' and 'record edits of one gesture ...' tests"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-engine: map-editor-engine: M2 camera anchors round trip ok"
        status: pass
    human_judgment: false
  - id: D3
    description: "Camera anchor data path: setting player N pads playersCameraAnchors with VNULL3, never shrinks it, never resizes on open, undo restores the exact old size, and the saved map equals the builder's"
    requirement: "D-22"
    verification:
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2CameraAnchors (editor-bridge: M2 camera anchors ok)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Bridge spans, trench pieces and fences are no longer placeable from the palette or BkEditorAddObject; loaded objects of those types still load, draw and move"
    requirement: "D-05"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-panels: panels_logic.zig 'the palette leaves out the types a map cannot hold ...'"
        status: pass
      - kind: integration
        ref: "zig build test-editor-bridge: tools/zig/editor_bridge_test.cpp#TestM2PaletteFilter (editor-bridge: M2 palette filter ok)"
        status: pass
    human_judgment: false
  - id: D5
    description: "The spec records free camera rotation as closed with the 03-06 numbers and the pre-rendered-art reason, lists the fifth not-copied MFC rewrite, and states the M2 record-level saving and cascade rules"
    requirement: "D-23"
    verification:
      - kind: other
        ref: "grep -c 'HandOutLinks|99.2|WhyNotPlacedByPalette|NMapRecords' docs/superpowers/specs/2026-09-19-portable-map-editor-design.md (6)"
        status: pass
    human_judgment: true
    rationale: "Wording and completeness of a design document is a reading judgment; the grep proves the sections exist, not that they read right"
  - id: D6
    description: "The decision log records the D-24 split into thirteen sequential plans and corrections C1-C13 as taken"
    requirement: "D-24"
    verification:
      - kind: other
        ref: "grep -c 'Planning amendments (2026-09-30)' 04-DISCUSSION-LOG.md (1)"
        status: pass
    human_judgment: true
    rationale: "A decision log is read by the executors of later plans; the grep proves the section exists, not that it is complete"

duration: 40min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 01: M2 foundations Summary

**A record-level overlay (NMapRecords) for every M2 collection with byte-exact inverse proofs, a generic record_edit command traced end to end on the real engine by camera anchors, the D-05 palette filter, and the M2 spec and decision-log amendments.**

## Performance

- **Duration:** about 40 min
- **Started:** 2026-09-30, about 04:55 local time (start was not recorded at the moment; taken from the first baseline run at 05:00)
- **Completed:** 2026-09-30T05:32+07:00
- **Tasks:** 3 (one tracer, two auto; the third with a TDD-style RED on the panels test first)
- **Files modified:** 24 (4 created), 2664 insertions, 39 deletions

## Accomplishments

- `NMapRecords` (`Sources/src/MapFile/MapRecords.h/.cpp`): camera anchors, script file and `IsBareScriptName`, script areas and `IsAreaNameFree`, reinforcement groups and `FirstFreeGroupID`, start commands, reserve positions, AI general sides, roads and rivers and `NextVsoID`, bridge and entrenchment entries, object script ID and HP. All in place, by position, never renumbering; `SAddObject` carries `nFrameIndex`, `fHP`, `nScriptID` with defaults that keep the M1 values.
- The generic record command: `records.zig` (`Kind`, `Value`, `CameraAnchors`), `Command.record_edit`, `Editor.editRecord` / `setCameraAnchor` / `clearCameraAnchor` / `record_generations`, the vtable pair `readRecord` / `putRecord` plus `groundHeight`, the fake bridge and the c_bridge adapter, all in one commit.
- The C ABI and session for camera anchors and ground height (`session_records.cpp`), on both copies together and off the engine, with the 32-slot rule, the on-the-map rule for changed slots, and `Guarded` entry points.
- The D-05 palette filter: `WhyNotPlacedByPalette` asked by the catalogue and `BkEditorAddObject` only, `isPlaceable` without 4, 6, 9. Arnheim still opens with every span placed and a loaded fence still moves.
- Spec: M2 scope section, the record-level saving rules, five not-copied MFC rewrites, cascade delete, the corrected data-only sentence, D-23's closure with the 03-06 numbers. Decision log: the D-24 split and C1-C13.

## Task Commits

1. **Task 1: Camera anchors end to end (tracer)** - `fdd2d7cf7` (feat)
2. **Task 2: Record-level overlay for every other M2 collection** - `2132b9f81` (feat)
3. **Task 3: Palette filter; spec and decision-log amendments** - `c113a8d7d` (feat)

**Plan metadata:** the docs commit that carries this file.

## Verification (plan level, macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` (after the build.zig edit) | 3/3 pass |
| `test-editor-core` | 96 tests pass (10 new) |
| `test-map-files` | `M2 camera anchor records ok`, `M2 record ops ok (87 cases)`, `map-file: PASS`, sweep 66/66 |
| `test-editor-bridge` | `M2 camera anchors ok`, `M2 palette filter ok`, `editor-bridge: PASS` |
| `test-map-editor-engine` | `M2 camera anchors round trip ok`, `PASS (260 objects)` |
| `test-map-editor-panels`, `-view`, `-auto`, `-testlaunch`, `-view-events`, `map-editor-smoke` | pass; smoke 52 steps |

Tracer gate: the tracer's `<verify>` is automated-only, so it was re-run end to end under the end-of-phase mode; it passed and the plan expanded (Task 2 and 3).

## Files Created/Modified

See the frontmatter `key-files`. Logs are under `zig-out/local-test/04-01-*.log`.

## Decisions Made

- The anchor struct is `BkEditorCameraAnchorRecord` (see Deviation 1). Later plans keep the `...Record` suffix for the structs of the collections they add, so no struct shares a function's name.
- Changed-slot validation, out-records on erases, bool returns and the `NextVsoID` floor of 1 are listed under `key-decisions`. The two size caps (`nMaxCameraAnchorPlayers` 1024, `nMaxAIGeneralSides` 1024) keep a bad caller from asking for a vector of billions.
- The core `CameraAnchors` value holds its 32 slots inline (400 bytes), so a history entry that carries one is larger than the sound entries. Accepted for now; when a list kind lands in a later plan the value can move behind a pointer without touching the command's shape.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] C ABI struct and entry point shared a name**
- **Found during:** Task 1 (writing bridge.h)
- **Issue:** the plan names both the struct and the entry point `BkEditorCameraAnchors`; in C a typedef and a function are one namespace, so it cannot compile.
- **Fix:** the struct is `BkEditorCameraAnchorRecord` (the sound list's naming); the entry points are as planned.
- **Files modified:** `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`, `session.h`, `session_records.cpp`, `Sources/editor/app/c_bridge.zig`, `c_bridge_test.zig`, `tools/zig/editor_bridge_test.cpp`
- **Committed in:** `fdd2d7cf7`

**2. [Rule 1 - Bug in the test design] Byte-identity compared a map with a copy of it**
- **Found during:** Task 1 (`TestCameraAnchorRecords`)
- **Issue:** the edit-plus-inverse file differed from the unedited one at every 8th byte from offset 59044. `SVertexAltitude` (float plus one byte) is written as a raw struct, so its three padding bytes go to the file, and a copied `CMapInfo` holds whatever the copy left there. Not an editing bug.
- **Fix:** every byte comparison in the map-file tier reads the map fresh from the file for each write (`RunM2Case`, `TestCameraAnchorRecords`); the expected maps for `AreEquivalent` may be copies. The decision log and patterns record the rule for later plans.
- **Committed in:** `fdd2d7cf7`, `2132b9f81`

**3. [Rule 3 - Blocking] Test harness details**
- **Found during:** Tasks 1 and 2
- **Issue:** (a) `editor_bridge_test.cpp` builds engine paths with backslashes; `std::ifstream` and `remove` on macOS take them literally, so the byte comparison could not open its files. (b) A local named `records` in `c_bridge.zig` shadowed the new import. (c) A test that cast the integer 7 to `EVsoKind` trapped under UBSan.
- **Fix:** an `OsPath` helper in the engine test; the import is `record_types`; the enum test became a null-map refusal.
- **Committed in:** `fdd2d7cf7`, `2132b9f81`

**4. [Rule 2 - Correctness] Changed-slot validation for camera anchors**
- **Found during:** Task 1 (session rules)
- **Issue:** the plan's "an anchor that is not VNULL3 must lie on the map" applied to the whole vector would make every edit of one slot refuse on a shipped map that holds an off-map anchor in another.
- **Fix:** only slots the put changes are checked; unchanged slots pass as they are.
- **Committed in:** `fdd2d7cf7`

### Signature refinements (superset of the plan)

- `SetPlayerCameraAnchor` and `ClearPlayerCameraAnchor` return `bool`; every `Erase...` takes an optional out-record. Callers that ignore them behave as the planned `void`/plain versions.

### Process note

The commit trailer is `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, the model that ran this plan, not the `Claude Opus 5.5 (1M context)` line the plan and the sequential-execution message name. The harness attribution reminder for this session gave the Sonnet line and an accurate trailer is the point of the rule. The trailer is present on every commit (memory: never leave it off).

---

**Total deviations:** 4 auto-fixed (1 Rule 1-class test design, 2 Rule 3, 1 Rule 2) plus one signature refinement and one process note
**Impact on plan:** no scope change; deviation 2 changes how every later plan writes a byte-identity test.

## Issues Encountered

- `BkEditorGroundHeight` cannot be checked against a known height in the engine tier (no shipped map has a documented height at a documented point); it is checked to answer a finite number in the map's middle and to refuse off the map. The fake answers a flat 0 (its header says so).
- The engine tier's older tests remove scratch files with backslash paths, which leaves them behind on macOS (`remove` cannot open them). Out of scope; the new tests use `OsPath` and clean up.

## Known Stubs

None. The fake bridge's flat ground is a documented test-double simplification, not a stub.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- 04-02 (cascade delete) can build on `NMapRecords` and the fixed `FindReferences` plan in PATTERNS.
- A later plan adds a record kind by: a `Kind` and `Value` arm in `records.zig`, a `readRecord`/`putRecord` arm in the fake and in `c_bridge.zig`, the C calls, and its session functions in `session_records.cpp`. `NMapRecords` already has every operation the session functions need.
- Nothing blocks 04-02.

## Threat surface

No new surface beyond the plan's threat model: the three entry points validate count, finiteness and on-the-map (T-04-01-01), a file with more than 32 anchors is read-refused and kept byte-exact (T-04-01-02), the map-file tier proves inverses byte-exact for every collection (T-04-01-03), and the palette filter is in the catalogue and `BkEditorAddObject` (T-04-01-04).

## Self-Check: PASSED

- Created files found: `MapRecords.h`, `MapRecords.cpp`, `session_records.cpp`, `records.zig`.
- Commits found: `fdd2d7cf7`, `2132b9f81`, `c113a8d7d`.
- All three tasks' acceptance criteria re-run and passing; plan-level verification list above.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*

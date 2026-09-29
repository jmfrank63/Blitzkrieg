---
phase: 03-map-editor-plan-6-finish-m1
plan: 10
subsystem: map-editor-sounds
tags: [zig, cpp, imgui, editor-bridge, map-file, sound-list, undo]

requires:
  - phase: 03-map-editor-plan-6-finish-m1 (03-05)
    provides: the object palette, properties panel commit-on-deactivate pattern, drawOverlay's brush outline, and the sound/tank-pit placement refusal this plan's Sounds panel replaces
  - phase: 03-map-editor-plan-6-finish-m1 (03-09)
    provides: the catalogue read/sort/cache pattern (loadCatalogue, State.order) this plan's sound_names follows
provides:
  - "BkEditorSoundRecord, BkEditorSounds/AddSound/SetSound/DeleteSound - the map's sound list, bound to CMapInfo::sounds.sounds (the field CMapInfo::operator& actually serialises), not CMapInfo::soundsList as the plan's own objective named"
  - "core.bridge.SoundRecord, the core vtable's sounds/addSound/setSound/deleteSound, history's sound_add/sound_edit/sound_delete commands, Editor.addSound/editSound/deleteSound with undo/redo and sounds_generation"
  - "The Sounds panel: a read/select/edit list under Players, 'Add at view centre' and 'Delete', committing through editor.editSound; markers on the map (View.drawOverlay)"
  - "A pre-existing binary-save tag collision in SMapSoundInfo (nMaxRadius and bMuteDuringCombat both under tag 6) found and fixed"
affects: [future map-editor plans touching CMapInfo::sounds, the object catalogue's loadCatalogue, or View.drawOverlay]

actuals:
  tokens: 20979
  tasks: 3
  commits: 4
  plan_head_before: 33c38ef22

tech-stack:
  added: []
  patterns:
    - "A C ABI list accessor with no per-index read (BkEditorSounds only reads the whole list) is read whole and indexed by the core, the same two-pass sizing document.reload already uses for bridge.objects - editor.zig's readSoundAt"
    - "Sound edits carry no document mirror: the core reads the bridge's own list before an edit that needs a 'before' value (editSound, deleteSound) rather than keeping a parallel Document.sounds in step with it"
    - "A generation counter (sounds_generation) bumped by every mutation including undo/redo replay, checked once per frame by the app layer (State.sounds_generation_seen) to decide whether to re-read a list the core does not mirror"

key-files:
  created: []
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/src/Formats/fmtMap.h
    - tools/zig/editor_bridge_test.cpp
    - Sources/editor/core/bridge.zig
    - Sources/editor/core/history.zig
    - Sources/editor/core/editor.zig
    - Sources/editor/core/fake_bridge.zig
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/smoke.zig

key-decisions:
  - "Ground-truth correction (ADR-worthy): the bridge binds to CMapInfo::sounds.sounds (SSoundInfo, std::vector<SMapSoundInfo>, saved under tag 17 / \"MapSounds\"), not CMapInfo::soundsList as the plan's own objective and read_first citation named. CMapInfo::operator& (both binary and XML tree, RandomMapGen/MapInfo_Methods.cpp) never serialises soundsList, and nothing in this codebase populates it from a loaded file - confirmed directly from MapFile/MapEquivalence.cpp's own CompareMap comment (\"soundsList is not serialised - CMapInfo::operator& writes sounds and derives this\"). soundsList is a sibling field of the simpler CMapSoundInfo (name+position only, Formats/fmtSound.h) that IScene::InitMapSounds and the MFC editor's own sound dialog read - but the MFC editor's own code (TemplateEditorFrame1.cpp) never populates it from sounds.sounds either, so its own sound tab was already non-functional in this codebase before this plan. Binding to soundsList would have made every add/edit/delete a no-op the instant the map was saved and reopened."
  - "Sound positions are world (scene) units, per the plan's explicit directive (raw vPos, no AI2Vis conversion) - validated on-map by reusing the existing WorldToTile oracle (the same one BkEditorWorldToTile already exposes) rather than hand-deriving a width_tiles*fWorldCellSize bounds formula."
  - "The core keeps no Document.sounds mirror (unlike objects): editSound/deleteSound read the bridge's current list once (readSoundAt) to get a 'before' value, since BkEditorSounds has no per-index accessor. Kept the Task 2 file list to core/bridge.zig, history.zig, editor.zig, fake_bridge.zig, c_bridge.zig only, as the plan specified (no document.zig)."
  - "sounds_generation (u32, bumped on every add/edit/delete and their undo/redo replay) plus a per-frame sounds_generation_seen check in the app is how the panel notices an edit that did not go through its own mapOpened() reload - the same shape objects get for free from the document mirror, sounds get from this counter instead."
  - "state.sounds now reads through editor.bridge (Task 2's core vtable) rather than the Task 1 direct RealBridge.sounds() method, which is removed as superseded - the plan itself flagged this as the intended supersession (\"a direct method for now\")."
  - "The Sounds panel's 'known sounds' combo (state.sound_names) is built once per catalogue load (game type 100, filtered and sorted case-insensitively), not filtered/sorted every frame the combo is open: the catalogue carries 4385 sound entries, and a per-frame insertion sort at that size would not hold a frame budget."

patterns-established:
  - "sounds_generation + sounds_generation_seen: a change-notification counter for app state the core does not mirror, checked once per frame rather than the app polling a full re-read unconditionally"

requirements-completed: [CARRY-SOUNDS, SPEC-PRESERVATION]

coverage:
  - id: D1
    description: "The Sounds panel lists the map's own sound list (name, position, repeat, mute in combat, radii), reading CMapInfo::sounds.sounds through BkEditorSounds"
    requirement: CARRY-SOUNDS
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp#TestSoundList (zig build test-editor-bridge)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-panels / map-editor-host-check / map-editor-smoke"
        status: pass
    human_judgment: false
  - id: D2
    description: "Sounds can be added (at the view's centre, a known sound from the catalogue), edited and deleted; each is one undo step; a save+reload preserves a kept edit's own fields and leaves the rest of the map unchanged"
    requirement: SPEC-PRESERVATION
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp#TestSoundList (add/set/delete, refusal-leaves-list-unchanged, and the save+reload preservation checks for both a kept edit and the add+delete round trip)"
        status: pass
      - kind: unit
        ref: "Sources/editor/core/editor.zig sound add/edit/delete/undo/redo tests (zig build test-editor-core)"
        status: pass
      - kind: e2e
        ref: "Sources/editor/app/smoke.zig \"Add at view centre adds a sound\" / \"Ctrl+Z removes it\" (zig build map-editor-smoke)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Each sound is marked on the map where it plays (View.drawOverlay), the selected one highlighted"
    verification:
      - kind: other
        ref: "code inspection - View.drawSoundMarkers, called unconditionally from drawOverlay regardless of the active tool"
        status: pass
    human_judgment: true
    rationale: "No automated pixel check confirms the diamond markers render correctly on screen; the plan's own Task 3 <verify><human-check> (\"open a map with sounds - each is marked on the map; add one at the view centre, change its sound and radius, undo and redo; save, reopen - the edits are there\") is the intended verification and routes to end-of-phase UAT per workflow.human_verify_mode."
  - id: D4
    description: "Placing a sound from the object palette stays refused (carried from plan 5 Task 7.2) - the sound list is where sounds are edited"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp#TestEveryGameTypeAnswers (pre-existing, unchanged by this plan) and panels_logic.isPlaceable excluding game type 100 (pre-existing)"
        status: pass
    human_judgment: false

duration: ~88 min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 10: Map sound list Summary

**The map's sound list (CMapInfo::sounds.sounds, not CMapInfo::soundsList as the plan's own objective named) is readable, editable with undo, marked on the map, and now actually survives a save and reload - closing a pre-existing binary-format tag collision found along the way.**

## Performance

- **Duration:** ~88 min
- **Started:** 2026-09-28T11:29:00Z
- **Completed:** 2026-09-28T12:57:27Z
- **Tasks:** 3 (plus one deviation commit for a discovered serialization bug)
- **Files modified:** 15

## Accomplishments

- `BkEditorSoundRecord` + `BkEditorSounds`/`AddSound`/`SetSound`/`DeleteSound`, bound to `CMapInfo::sounds.sounds` (`SMapSoundInfo`) after discovering the plan's named field, `CMapInfo::soundsList`, is never serialised by `CMapInfo::operator&` and would have made every edit a no-op after save+reload.
- The core's `bridge.SoundRecord`, vtable (`sounds`/`addSound`/`setSound`/`deleteSound`), `history`'s three sound commands, and `Editor.addSound`/`editSound`/`deleteSound` with full undo/redo and a `sounds_generation` counter for the app layer.
- The Sounds panel: lists the open map's sounds, a selected row's fields (a filtered-by-game-type combo of known sounds, x/y, repeat/random repeat in seconds, mute in combat, min/max radius) committing on deactivation as one `editor.editSound`, "Add at view centre" and "Delete".
- `View.drawOverlay` marks every sound with a small diamond and its name, the selected one highlighted, regardless of the active tool.
- Two new smoke steps prove the whole stack end to end through the real engine: "Add at view centre adds a sound" and "Ctrl+Z removes it".
- Found and fixed a pre-existing binary-format bug: `SMapSoundInfo::operator&(IStructureSaver&)` wrote `nMaxRadius` and `bMuteDuringCombat` under the same tag (6). `BkEditorSaveMap`'s own read-back verification caught it the first time this codebase ever wrote non-default values through both fields on one sound.

## Task Commits

Each task was committed atomically:

1. **Task 1: The sound ABI end to end and a Sounds panel listing the map's sounds** - `05a4f4c9f` (feat)
2. **Task 2: Sound edits are undoable commands in the core** - `67f9307d8` (feat)
3. **Task 3: Editing sounds in the panel, and their markers on the map** - `97e0f2db1` (feat)
4. **Deviation: the SMapSoundInfo binary-save tag collision** - `66ad89f43` (fix)

**Plan metadata:** committed alongside this SUMMARY.

## Files Created/Modified

- `Sources/src/EditorBridge/bridge.h` - `BkEditorSoundRecord`, the four sound entry points, documented rules and ground-truth correction.
- `Sources/src/EditorBridge/bridge.cpp` - `BkEditorSounds`/`AddSound`/`SetSound`/`DeleteSound`, caller-bug (`BK_EDITOR_BAD_ARGUMENT`) checks.
- `Sources/src/EditorBridge/session.h` / `session.cpp` - `ReadSessionSounds`, `AddSoundToSession`/`SetSoundInSession`/`DeleteSoundFromSession`, `ValidateSoundRecord` (name known as game type 100, on-map via `WorldToTile`, non-negative times/radii, min<=max).
- `Sources/src/Formats/fmtMap.h` - `SMapSoundInfo`'s binary save tag fix (`bMuteDuringCombat` now tag 7).
- `tools/zig/editor_bridge_test.cpp` - `TestSoundList`: walks `Data/Maps` for a shipped map with sounds (falls back to coldwinter), round-trips `BkEditorSounds` against the file, add/set/delete, every refusal rule, and two preservation checks (a kept edit's own fields, and the add+delete round trip back to the original).
- `Sources/editor/core/bridge.zig` - `SoundRecord`, vtable entries and wrappers.
- `Sources/editor/core/history.zig` - `sound_add`/`sound_edit`/`sound_delete` commands.
- `Sources/editor/core/editor.zig` - `readSoundAt`, `addSound`/`editSound`/`deleteSound`, `replay` cases, `sounds_generation`, four tests.
- `Sources/editor/core/fake_bridge.zig` - an in-memory sound list with the real rules the core can see; header simplifications list updated.
- `Sources/editor/app/c_bridge.zig` - the four vtable functions over the C calls, `toSoundRecord`/`toCSoundRecord`; the Task 1 direct `sounds()` method removed as superseded.
- `Sources/editor/app/panels.zig` - the Sounds panel (list, selection, fields, Add/Delete), `state.sound_names` built once per catalogue load, `loadSounds` reading through `editor.bridge`.
- `Sources/editor/app/panels_logic.zig` - `msToSeconds`/`secondsToMs`, `sortNamesIgnoreCase`, `soundRadiusError`, tests.
- `Sources/editor/app/view.zig` - `drawSoundMarkers`, called unconditionally from `drawOverlay`.
- `Sources/editor/app/smoke.zig` - `add_sound_at_view_centre` input, `sound_added`/`sound_removed` expectations, two new script steps.

## Decisions Made

- Bound the whole feature to `CMapInfo::sounds.sounds`, not `CMapInfo::soundsList` as the plan's own objective named - see Deviations below for the full evidence trail.
- Sound positions are world (scene) units (the plan's explicit directive), validated on-map through the existing `WorldToTile` oracle rather than a hand-derived bounds formula.
- No `Document.sounds` mirror in the core; `editSound`/`deleteSound` read the bridge's list once for a "before" value (`readSoundAt`), matching the plan's own Task 2 file scope (no `document.zig`).
- `sounds_generation` + `sounds_generation_seen`: a change-notification counter, since the core keeps no mirror of the sound list to compare against for "did anything change".
- `state.sound_names` built once per catalogue load (not per-frame): the catalogue carries 4385 sound entries, and a per-frame case-insensitive sort at that size would not hold a frame budget - the same reasoning `state.order`'s own one-time build already follows.
- `state.sounds` now reads through `editor.bridge` (Task 2's vtable), and the Task 1 direct `RealBridge.sounds()` method was removed once Task 2 superseded it, per the plan's own "a direct method for now" framing.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The plan's objective named the wrong field: `CMapInfo::soundsList`, not `CMapInfo::sounds.sounds`**
- **Found during:** Task 1, reading the plan's own cited read_first range against `CMapInfo::operator&`
- **Issue:** The plan's objective and read_first list cite `CMapInfo::soundsList` (`SMapSoundInfo`) as the field the bridge should read/write, and frames the MFC editor's `TemplateEditorFrame1.cpp:2008-2018` marker loop as the model for units. Ground truth: `soundsList`'s element type is actually `CMapSoundInfo` (`Formats/fmtSound.h` - name and position only, no repeat/radius/mute), and `CMapInfo::operator&` (both `IStructureSaver` and `IDataTree`, `RandomMapGen/MapInfo_Methods.cpp`) never serialises `soundsList` at all - only `sounds` (`SSoundInfo::sounds`, `std::vector<SMapSoundInfo>`, tag 17 / "MapSounds"). `MapFile/MapEquivalence.cpp`'s own `CompareMap` comment confirms this outright: "soundsList is not serialised - CMapInfo::operator& writes `sounds` and derives this." The MFC editor's own marker loop the plan cited never actually runs on real data either: `TemplateEditorFrame1.cpp` never populates `soundsList` from `sounds.sounds` at map open, so that loop always iterates an empty list - it is dead code in this codebase, not a working model. Building the feature against `soundsList` would have compiled, listed, and edited a list that is nothing when saved, silently discarding every user edit the moment the map was reopened.
- **Fix:** Bound `BkEditorSounds`/`AddSound`/`SetSound`/`DeleteSound` and every downstream layer (core, app, tests) to `CMapInfo::sounds.sounds` instead. Kept the plan's `BkEditorSoundRecord` field set as specified (it already matches `SMapSoundInfo`'s fields exactly, not `CMapSoundInfo`'s - further evidence the plan's *data model* intent was `sounds.sounds`, only the field name citation was wrong) and its stated "world units" position convention (see below).
- **Files modified:** `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`, `session.h`, `session.cpp` (all of Task 1's C++ changes)
- **Verification:** `TestSoundList`'s save-and-read-back preservation check (add+delete round trip, and a kept edit's own fields) - both would have trivially "passed" against `soundsList` (always empty) without ever proving anything; against `sounds.sounds` they prove the real, persisted list.
- **Committed in:** `05a4f4c9f` (Task 1 commit)

**2. [Rule 1 - Bug] A pre-existing binary-format tag collision in `SMapSoundInfo`**
- **Found during:** After Task 3, extending `TestSoundList` to also prove a *kept* (not deleted) edit's own field values survive a save and reload - the original add+delete round trip only proved the list's shape came back, not that a real edit's values did.
- **Issue:** `SMapSoundInfo::operator&(IStructureSaver&)` (`Formats/fmtMap.h`) wrote both `nMaxRadius` and `bMuteDuringCombat` under tag 6. `BkEditorSaveMap`'s own read-back verification (`SaveSessionMap`) refused the write with "the written map reads back different at sounds.sounds[0].bMuteDuringCombat" - the first time anything in this codebase actually exercised writing distinct, non-default values through both fields on the same sound record.
- **Fix:** Gave `bMuteDuringCombat` its own tag, 7. The XML tree form (`CTreeAccessor`, named keys) never had this bug. No shipped map has a non-empty `sounds.sounds` today (measured: every `.bzm`/`.xml` under `Data/Maps` this session could read), so the tag change displaces nothing on disk.
- **Files modified:** `Sources/src/Formats/fmtMap.h`, `tools/zig/editor_bridge_test.cpp` (the new save-and-reload check that found it)
- **Verification:** `TestSoundList`'s new "the kept sound's own fields... survived the save and reload" check; full suite (`test-editor-bridge`, `test-editor-core`, `test-map-editor-panels`, `test-map-editor-engine`, `map-editor-smoke`, `map-editor-host-check`) re-run clean after the fix.
- **Committed in:** `66ad89f43` (separate deviation commit, after Task 3)

**3. [Rule 1 - Bug] A dangling-pointer bug in the Sounds panel's catalogue filter**
- **Found during:** Task 3, the `map-editor-smoke` "Add at view centre adds a sound" step - the added sound's name kept reading as garbage (empty, then apparently-whitespace, then unrelated bytes) regardless of code changes, until a raw hex dump of the catalogue's own buffer proved the *source* data was fine.
- **Issue:** `loadCatalogue`'s new `sound_names` loop wrote `for (entries) |entry| { ... std.mem.sliceTo(&entry.name, 0) ... }` - `entries` is a heap slice, but `for (entries) |entry|` copies each `CatalogueEntry` into a loop-local value; a slice taken from `&entry.name` (that local's address) dangles the instant the loop advances or exits. Every `state.sound_names` entry ended up pointing at whatever the stack held afterward.
- **Fix:** Pointer capture, `for (entries) |*entry|` - the same shape `state.order`'s own index-based access already used, now applied here too.
- **Files modified:** `Sources/editor/app/panels.zig`
- **Verification:** `map-editor-smoke`'s two new steps pass; the fix was confirmed with a temporary raw-byte debug print (reverted before committing) showing the catalogue's own buffer held correct ASCII names (`20mm_aviacannon`, etc.) both before and after the loop.
- **Committed in:** `97e0f2db1` (Task 3 commit)

---

**Total deviations:** 3 auto-fixed (2 correctness bugs found via genuine save+reload/engine-tier verification, 1 dangling-pointer bug found via smoke-test debugging). **Impact:** All three are corrections the plan's own stated goal (sounds "saved maps carry the edits") depends on; none is scope creep. The field-binding correction in particular is load-bearing - without it, this entire plan would have shipped a feature that appears to work in the editor session but discards every edit on save.

## Issues Encountered

- Diagnosing the dangling-pointer bug took several iterations of debug prints (name looked "empty", then "whitespace", then finally readable ASCII after dumping raw hex) before the root cause (loop-local copy, not the catalogue data) was clear - consistent with this project's own "code-reading guesses were repeatedly wrong" lesson: only the raw-byte measurement resolved it.
- All temporary debug prints (`std.debug.print("DEBUG ...")`) were removed before every commit; `git diff`/`git status` were checked clean of them after each removal.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The map's sound list is fully wired: bridge, core (undoable), panel (read/select/edit/add/delete), and map markers, with a proven save+reload round trip for both a kept edit and an add+delete cycle.
- Task 3's `<verify><human-check>` (Johannes opening a map with sounds interactively, adding/editing/undoing/redoing, saving and reopening) is deferred to end-of-phase UAT per `workflow.human_verify_mode` - not a blocker for this plan.
- No blockers for the remaining plans in this phase (03-11 through 03-15).
- Worth a follow-up note for whoever next touches `SMapSoundInfo`: its binary tag numbering now has a gap at nothing (1-7 used) - straightforward to extend, but any future field must not reuse 1-7.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

## Self-Check: PASSED

All key files found on disk (bridge.h/.cpp, session.h/.cpp, fmtMap.h, editor_bridge_test.cpp; core bridge.zig/history.zig/editor.zig/fake_bridge.zig; app c_bridge.zig/panels.zig/panels_logic.zig/view.zig/smoke.zig; this SUMMARY). All four commits (`05a4f4c9f`, `67f9307d8`, `97e0f2db1`, `66ad89f43`) found in `git log`. `zig build test-editor-bridge test-editor-core test-map-editor-panels test-map-editor-engine map-editor-smoke map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` re-run clean (all PASS) after every change, including after the tag-collision fix.

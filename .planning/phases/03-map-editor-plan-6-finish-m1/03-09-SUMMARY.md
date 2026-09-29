---
phase: 03-map-editor-plan-6-finish-m1
plan: 09
subsystem: map-editor-palette
tags: [zig, imgui, sdl-gpu, editor-bridge, gamedb, palette, icons]

requires:
  - phase: 03-map-editor-plan-6-finish-m1 (03-08)
    provides: File > Mod switching and State.reloadCatalogue, which this plan's pictures cache now hooks
provides:
  - "BkEditorObjectPicture: the object database's own icon.tga, decoded by the engine, with a squad-icon fallback for a member soldier with none of its own"
  - "Sources/editor/app/pictures.zig: Pictures, a per-session SDL GPU texture cache over that bridge call"
  - "Sources/editor/app/panels_logic.zig: PictureQueue, the pure pending/missing ordering and dedup rules, tested with no GPU or bridge"
  - "The object palette shows each object's own (or borrowed) picture, a neutral named frame for the rest, decoded a budget at a time per frame"
affects: [future map-editor UI plans that touch drawObjectPalette or Pictures]

actuals:
  tokens: 12262
  tasks: 4
  commits: 2
  plan_head_before: 75b815f549402f9b59fbfd55e662a7884cdac1f0

tech-stack:
  added: []
  patterns:
    - "Pure ordering/dedup logic (PictureQueue) extracted from a GPU-bound cache (Pictures) so it can run under the no-GPU panels_logic test tier"
    - "A same-size neutral frame drawn unconditionally (ImDrawList_AddRect) before any per-state content, so a row is never blank while GPU upload latency or a still-queued decode catches up"
    - "A per-session lazy-built name->name lookup map (SEditorSession) for a cross-referencing engine-tier query (soldier -> owning squad), invalidated wherever the object database itself reloads (ReloadAfterModChange)"

key-files:
  created:
    - Sources/editor/app/pictures.zig
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - tools/zig/editor_bridge_test.cpp

key-decisions:
  - "D-29 amended (Johannes, Task 2 checkpoint): keep the shipped icon.tga pictures decoded by the engine, no new off-screen render path; objects without one show a neutral frame with their own name, same frame for every type"
  - "PictureQueue pulled out of pictures.Pictures into panels_logic.zig, so its ordering/dedup/missing-never-retried rules are tested without a GPU or the bridge"
  - "The palette's picture cell always draws its bordered frame first, image (once ready) overlaid on top via ImDrawList_AddImage - closes a real GPU-texture-visibility gap found manually building this task (see Issues Encountered), not only the pending/missing states the plan named"
  - "picture_pump_budget stays 8/frame: Task 1 measured 2.226 ms/decode, so 8 decodes cap a frame's own work at ~17.8 ms, under the 33 ms target even when a 300-object group opens at once"
  - "User-requested addition (Johannes, after Task 3): a single soldier with no icon.tga of its own borrows its squad's, built once per session from every squad's own <Members> list; the alphabetically first squad name wins when more than one lists the same soldier"

patterns-established:
  - "PictureQueue: pending/missing name-ordering logic separated from the GPU-bound cache that owns it, for testability"

requirements-completed: [D-29, CARRY-ICONS]

coverage:
  - id: D1
    description: "A palette group shows each object's own icon.tga picture, decoded by the engine on demand and cached per session (D-29's shipped route)"
    requirement: D-29
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp#TestObjectPictures (zig build test-editor-bridge)"
        status: pass
      - kind: integration
        ref: "zig build test-map-editor-panels / map-editor-host-check / map-editor-smoke"
        status: pass
    human_judgment: false
  - id: D2
    description: "Objects without a picture (own or borrowed) show a neutral frame with their name inside; a still-pending row shows a blank frame - both always visibly bordered, closing the GPU-texture-visibility gap found manually while building this task"
    requirement: D-29
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig PictureQueue tests (zig build test-map-editor-panels)"
        status: pass
    human_judgment: true
    rationale: "The frame's visual correctness (border always drawn, name wrapped, alignment, the GPU-timing fix) was confirmed by this executor via manual screenshot capture (zig-out/local-test/03-09-final-crop.png, 03-09-missing-label-crop.png, not committed) and reverted debug builds, not by Johannes's own interactive playtest. The plan's Task 3 <verify><human-check> (open several palette groups, scrolling stays smooth, mod switch works) is still outstanding and routes to end-of-phase UAT per workflow.human_verify_mode."
  - id: D3
    description: "State.reloadCatalogue (mod switch, D-26) clears the picture cache so the new mod's pictures load"
    requirement: D-29
    verification:
      - kind: other
        ref: "grep -n \"pictures.clear\" Sources/editor/app/panels.zig (acceptance criterion)"
        status: pass
    human_judgment: true
    rationale: "No automated test exercises this GPU-backed path end to end (it needs a real SDL_GPU device and a second mod's catalogue); verified by code inspection and the acceptance grep only."
  - id: D4
    description: "A single soldier with no icon.tga of its own borrows its squad's picture (Johannes-requested addition); coverage rose from 1073/1167 to 1126/1167 placeable objects"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp#TestObjectPictures's Allies_Bren check (zig build test-editor-bridge)"
        status: pass
    human_judgment: false

duration: ~59 min (includes the Task 2 checkpoint wait for Johannes's decision)
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 9: Object palette pictures (D-29) Summary

**The palette shows each object's own shipped icon.tga, decoded by the engine on demand and cached, with squad icons borrowed for soldiers that have none, and a named neutral frame for the 41 objects still without a picture.**

## Performance

- **Duration:** ~59 min total (Task 1 commit to Task 4 commit; includes Johannes's Task 2 checkpoint decision time, not pure execution)
- **Started:** 2026-09-28T10:25:35Z (Task 1 commit)
- **Completed:** 2026-09-28T11:24:32Z (Task 4 commit)
- **Tasks:** 4 (Task 1, Task 2 checkpoint, Task 3, and a Johannes-requested Task 4 addition)
- **Files modified:** 8 (1 created, 7 modified across the whole plan)

## Accomplishments

- `BkEditorObjectPicture` decodes the object database's own `<szPath>\icon.tga` on demand, scaled down keeping aspect, matching the MFC palette's own picture source (Task 1).
- `Pictures` (new `pictures.zig`) caches decoded pictures as SDL GPU textures per session, requesting and decoding a budget at a time so opening a large group never stalls a frame (Task 1, budget tuned in Task 3).
- Coverage measured and re-measured: 1073/1167 placeable objects have a shipped picture (92%, Task 1); with the squad-icon fallback (Task 4), 1126/1167 (96%).
- Johannes's Task 2 checkpoint decision recorded: "shipped" - keep the engine-decoded shipped pictures, no new off-screen render path; a neutral frame for the rest.
- Every palette picture cell always shows a bordered frame - a missing object's own name wrapped inside it, the same frame for every type (D-29); this also closes a real GPU-texture-visibility gap this task found (see Issues Encountered).
- `PictureQueue` (new, in `panels_logic.zig`): the pending/missing ordering, dedup and "never retried" rules, pulled out of the GPU-bound cache and tested on their own.
- `State.reloadCatalogue` (mod switch) now actually clears the picture cache - previously only its own doc comment claimed this.
- User-requested addition: a single soldier with no icon.tga of its own (e.g. `Allies_Bren`) borrows the icon.tga of a squad that lists it as a member (e.g. `gb_bren_43`), built once per session from every squad's own `<Members>` list.

## Task Commits

Each task was committed atomically:

1. **Task 1: A palette group shows each object's own picture, decoded by the engine on demand; coverage measured** - `75b815f54` (feat) - by the prior executor agent, before this continuation.
2. **Task 2: How objects without a shipped picture are shown (D-29)** - checkpoint:decision, answered by Johannes ("shipped"); no code, recorded in this SUMMARY and in `03-CONTEXT.md`.
3. **Task 3: Finish the chosen route - placeholder, cache life, frame budget** - `b664d649f` (feat)
4. **Task 4 (Johannes-requested addition): single soldiers borrow their squad's picture** - `26e58bd4b` (feat)

**Plan metadata:** committed alongside this SUMMARY.

## Files Created/Modified

- `Sources/editor/app/pictures.zig` - `Pictures`: per-session SDL GPU texture cache (request/pump/lookup/clear/deinit), now built on `panels_logic.PictureQueue` for its pending/missing side.
- `Sources/editor/app/panels_logic.zig` - new `PictureQueue` (pure ordering/dedup/budget/missing rules, with tests).
- `Sources/editor/app/panels.zig` - `drawPaletteRowPicture` always draws a bordered frame first, image or name-label overlaid on top; `reloadCatalogue` calls `pictures.clear()`; `picture_pump_budget` named and documented.
- `Sources/src/EditorBridge/bridge.h` / `bridge.cpp` - `BkEditorObjectPicture`'s squad-icon fallback (`BuildSquadIconOwnerMap`), documented in the header comment.
- `Sources/src/EditorBridge/session.h` - `SEditorSession::squadIconOwnerBySoldier` / `bSquadIconOwnerMapBuilt`, cleared in `ReloadAfterModChange`.
- `Sources/editor/app/c_bridge.zig` - `RealBridge.objectPicture`, `RealBridge.gpuDevice` (Task 1).
- `tools/zig/editor_bridge_test.cpp` - `TestObjectPictures` (coverage line, sample picture, sane-decode checks, Task 1) plus the Task 4 `Allies_Bren` squad-icon check.

## Decisions Made

- D-29 amended twice (both recorded in `03-CONTEXT.md`): first after Task 1's coverage measurement (Johannes's "shipped" checkpoint choice), then after Task 4's squad-icon addition (coverage 1073 -> 1126 of 1167).
- `PictureQueue` extracted to `panels_logic.zig` rather than kept inline in `pictures.zig`, so its rules are provably correct without a GPU or the bridge - see the "Pure ordering/dedup logic" pattern above.
- The palette's picture cell draws its frame unconditionally (every state, not only pending/missing) - a deliberate widening of the plan's own wording, justified by a real bug this task found manually (see Issues Encountered).
- `picture_pump_budget` stays 8/frame, unchanged from Task 1's original choice - Task 1's own measured 2.226 ms/decode already gives ample headroom under the 33 ms target, so no change was needed, only measurement and documentation.
- Squad-icon fallback chooses deterministically by squad name (`std::string::operator<`), not catalogue order or squad size, per Johannes's own instruction ("the first by name").

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `State.reloadCatalogue` did not actually clear the picture cache**
- **Found during:** Task 3, reading `panels.zig`'s own doc comment against its code
- **Issue:** The `pictures` field's doc comment already claimed "cleared on a mod switch (reloadCatalogue)", but `reloadCatalogue` never called `pictures.clear()` - a mod switch would have kept the old mod's textures and missing-marks around, showing wrong or blank pictures for names the new mod's catalogue reused.
- **Fix:** Added the `self.pictures.clear()` call at the top of `reloadCatalogue`.
- **Files modified:** `Sources/editor/app/panels.zig`
- **Verification:** `grep -n "pictures.clear" Sources/editor/app/panels.zig` (plan's own acceptance criterion); `zig build test-editor-bridge` (`TestModsListSetAndClear`) still passes.
- **Committed in:** `b664d649f` (Task 3 commit)

**2. [Rule 1 - Bug] A texture decoded and uploaded on the very frame a group first opens was not always visible in that same frame's own render, even though `lookup()` already reported it `ready`**
- **Found during:** Task 3, while manually verifying the orchestrator's report that `122-mm_M-30`, `12_7_mm_DShK` and `12_7_mm_M2HB_USA` showed "no picture and no frame" in `03-09-manual-panels.png`
- **Issue:** Investigated with a temporary debug build (forced-open palette, per-pump debug prints, extra "settle" frames - all reverted before committing): the two named-but-not-actually-missing objects (`122-mm_M-30`, `12_7_mm_DShK` both have `icon.tga`, confirmed by grep and by a temporary `DEBUG missing:` print that never named them) decoded successfully in the same frame their group first opened and was captured, yet rendered nothing - the previous implementation only drew a frame for `pending`/`missing` states and called plain `igImage` for `ready`, so a `ready`-but-GPU-not-yet-visible texture showed as an invisible blank cell, identical to what the orchestrator originally reported. Adding three extra render frames (also reverted) made the same rows render correctly, confirming the gap was purely a same-frame GPU-upload-to-render timing gap, not a missing-picture or logic bug.
- **Fix:** `drawPaletteRowPicture` now draws the same-size bordered frame unconditionally, in every state, via the window's own draw list (`ImDrawList_AddRect`) before checking `lookup()`; the `ready` image is overlaid on top with `ImDrawList_AddImage` (which does not move the ImGui cursor, unlike `igImage`), so the border is always visible even on the one frame a fresh upload has not yet reached the screen, and the real picture simply appears inside it a frame or two later.
- **Files modified:** `Sources/editor/app/panels.zig`
- **Verification:** Rebuilt `map-editor-host-check` repeatedly with the same forced-open diagnostic, confirmed every row (ready, pending, and missing) now shows a visible frame at all times; `zig-out/local-test/03-09-final-crop.png` (not committed) shows the fix. Automated: `zig build test-map-editor-panels map-editor-host-check map-editor-smoke` all pass.
- **Committed in:** `b664d649f` (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (2 Rule 1 bugs, both found by this executor's own manual verification, not named by the plan). **Impact:** Both fixes are corrections to Task 1's/the plan's own intended behavior (D-29's "opening a group shows the objects' own pictures" truth), not scope creep - the second one directly resolves the orchestrator's reported visual defect.

## User-Requested Addition (outside the original plan)

Between Task 3 and finishing this plan, the coordinator relayed a scope addition from Johannes: single soldiers without their own icon should show their squad's picture instead of the empty frame, since the original GOG install has exactly the same 1125 `icon.tga` files as this repo's `Data` - nothing more can be fetched, but 59 of the 94 missing pictures are single soldiers whose squad already has one.

Implemented as `BkEditorObjectPicture`'s squad-icon fallback (`BuildSquadIconOwnerMap` in `bridge.cpp`, cache in `session.h`, doc comment in `bridge.h`), committed separately as `26e58bd4b`. Re-measured coverage: 53 of the 59 single soldiers now get a squad picture (game type 1's own missing count: 59 -> 6); the remaining 6 (`Allies_Paratrooper`, `Australian_Shooter`, `Finnish_Gunner_Hw`, `German_Paratrooper`, `USSR_Mosin`, `USSR_Paratrooper`) are not listed as a member of any squad in the shipped data and still show the neutral named frame, same as the 35 non-soldier objects (31 coast pieces, `Bomb`, `Grenade`, the `Entrenchment`, `AISingleUnitFormation`). An engine-tier check (`Allies_Bren` must decode `BK_EDITOR_OK` with a sane, non-black picture) was added to `TestObjectPictures` per Johannes's request, and the result was also confirmed visually (temporary `Allies_` palette filter, screenshot, reverted).

## Issues Encountered

- A same-frame GPU-texture-visibility gap (documented above under Deviations #2) was found only through manual, iterative debug builds - grepping/reading the code alone gave the wrong theory twice (first suspected the text-label rendering was broken, then suspected the wrong queue items were being decoded) before instrumenting `pump()` with temporary debug prints and adding temporary extra render frames pinned down the real cause. Consistent with this project's own "code-reading guesses were repeatedly wrong" lesson for graphics work - only measuring the actual rendered output (captured TGA/PNG, cropped and inspected) resolved it.
- All temporary diagnostics (forced-open palette header, inflated decode budget, hardcoded palette filter, per-pump debug prints, extra settle frames, and a debug-only "still missing" name printout in the C++ test) were reverted before every commit; `git status`/`git diff` were checked clean of them after each revert.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- D-29 and the CARRY-ICONS carry-over from plan 5 are both complete for this plan's scope.
- Outstanding: Task 3's `<verify><human-check>` (Johannes opening several palette groups interactively, confirming smooth scrolling and a mod switch's pictures) is deferred to end-of-phase UAT per `workflow.human_verify_mode` - not a blocker for this plan, but should be included in that consolidated review.
- No blockers for the remaining plans in this phase (03-10 through 03-15).

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

## Self-Check: PASSED

All key files found on disk (`Sources/editor/app/pictures.zig`, `panels_logic.zig`, `panels.zig`; `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`, `session.h`; `tools/zig/editor_bridge_test.cpp`; this SUMMARY). All three commits (`75b815f54`, `b664d649f`, `26e58bd4b`) found in `git log`. `zig build test-editor-bridge test-map-editor-panels map-editor-host-check map-editor-smoke -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` re-run clean (all PASS) after the Task 4 addition.

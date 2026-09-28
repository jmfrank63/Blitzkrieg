---
gsd_state_version: "1.0"
status: phase-2-executed-human-verification-outstanding
stopped_at: Completed 03-11-PLAN.md
last_updated: "2026-09-28T13:59:46.072Z"
state_head: 7660c4353d4808312f452461a613a329275e7eda
progress:
  total_phases: 2
  completed_phases: 0
  total_plans: 18
  completed_plans: 14
  percent: 0
current_phase_name: "Map editor plan 6: finish M1"
---

# Project state

- status: phase-2-executed-human-verification-outstanding
- projectName: Blitzkrieg Reloaded
- branch: main (feature merged through workspace/variable-zoom)
- created: 2026-06-06
- repoRoot: Blitzkrieg
- workflow: gsd-execute-phase

## Current summary

Phase 2 (variable zoom and minimap scaling) executed: all 3 plans complete.
Two review rounds applied: (1) zoom bound matches floored render scale, (2)
minimap flex with idempotent absolute baselines + 4:3 negative-size guard,
(3) fixup geometry applied immediately. Zig cross-compile (full game incl.
Metal shaders) clean. In-game sign-off rows outstanding — see
`.planning/phases/02-variable-zoom-and-minimap-scaling/02-VERIFICATION.md`
(its 10/10 source claims superseded by the review findings; corrected
behavior needs the in-game rows).

## Phase 4 runtime stability closeout (prior milestone work — historical)

- phase: 4
- phaseName: Runtime stability and compatibility
- dialog coverage: PASS (38 inventory resources mapped; no unresolved coverage gaps)
- startup stability pass/fail: PENDING human verification
- automated pass/fail: PASS (inventory/accounting checks and Debug build)
- evidence:
	- `.planning/phase-4/artifacts/dialog-inventory.txt`
	- `.planning/phase-4/artifacts/dialog-producers.txt`
	- `.planning/phase-4/artifacts/dialog-coverage-map.csv`
	- `.planning/phase-4/artifacts/dialog-coverage-gaps.txt`
	- `.planning/phase-4/VERIFICATION.md`
	- `.planning/phase-4/VALIDATION.md`
	- `Sources/src/UI/UIScreen.cpp` (`ShouldScaleLegacyLayout`)

## Next actions

1. Run in-game sign-off rows from `.planning/phases/02-variable-zoom-and-minimap-scaling/02-VERIFICATION.md` on the Windows build: zoom bounds at 640/1024/1920/3440, cursor anchoring, Shift+wheel (both shifts), J/K hold-repeat, L reset, mission restart/load reset, mid-mission resolution change, minimap cluster geometry + 2:1 diamond, fractional-zoom visuals, 1024×768 z=1 regression.
2. Record PASS/FAIL per row in the verification report; finalize Phase 2 status.

## Session

**Last session:** 2026-09-28T13:59:46.021Z
**Stopped at:** Completed 03-11-PLAN.md
**Resume file:** None

## Accumulated Context

### Roadmap Evolution

- Phase 3 added (2026-09-28): Map editor plan 6: finish M1 — branch feat/map-editor-plan-6, worktree .worktrees/map-editor-6

## Performance Metrics

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 03-map-editor-plan-6-finish-m1 P01 | 35min | 2 tasks | 5 files |
| Phase 03-map-editor-plan-6-finish-m1 P02 | 55min | 2 tasks | 10 files |
| Phase 03-map-editor-plan-6-finish-m1 P03 | 23min | 2 tasks | 10 files |
| Phase 03-map-editor-plan-6-finish-m1 P04 | ~25min | 2 tasks | 4 files |
| Phase 03-map-editor-plan-6-finish-m1 P05 | ~58min | 3 tasks | 11 files |
| Phase 03-map-editor-plan-6-finish-m1 P06 | ~20min | 1 tasks | 6 files |
| Phase 03-map-editor-plan-6-finish-m1 P07 | ~35min | 3 tasks | 8 files |
| Phase 03-map-editor-plan-6-finish-m1 P08 | ~60min | 3 tasks | 11 files |
| Phase 03-map-editor-plan-6-finish-m1 P09 | ~59min | 4 tasks | 8 files |
| Phase 03-map-editor-plan-6-finish-m1 P10 | 88min | 3 tasks | 15 files |
| Phase 03-map-editor-plan-6-finish-m1 P11 | ~57min | 3 tasks | 7 files |

## Decisions

- [Phase ?]: CloudProviderSelected gates on Editor.TestLaunch (one guard closes all five cloud-sync call sites) rather than gating each call site
- [Phase ?]: SMiniMapUnitInfo coordinate scale (1 unit = 2 AI tiles = 64 world units) determined by reading CAILogic::GetMiniMapInfo source directly, not by building the Map Editor app to cross-check a live dump
- [Phase ?]: BkEditorTestMapPath's units= query for BK_AUTO_UI uses the placed object's map position, not its scene-world position (GetMiniMapInfo's AI coordinates are map units per bridge.h - confirmed empirically, corrects 03-01-SUMMARY's wording)
- [Phase ?]: main.zig builds its own allocator-backed Io.Threaded instance instead of std.Io.Threaded.global_single_threaded, whose allocator is .failing by design and broke std.process.spawn with OutOfMemory
- [Phase ?]: Safe save's read-back verification (D-19) lives entirely in SaveSessionMap (C++), not duplicated in Zig - Editor.save only does the temp-file/backup/swap dance around a BkEditorSaveMap call that is already safe to trust once it answers OK
- [Phase ?]: Both shipped map formats (.bzm and .xml) round-tripped through the new read-back check with no float-rounding mismatch on this host - the spec's idempotent .xml fallback exists in the test but was never exercised
- [Phase ?]: UnsavedPrompt (D-23) is a pure state machine that never touches the bridge or filesystem itself - the caller makes the actual save and reports the outcome back through saveFinished, so its own tests need no fake bridge
- [Phase ?]: isShippedMap (D-18) treats a relative document path as already under base_root rather than resolving it against a cwd, matching how the smoke's own map argument and every real Open of a shipped map actually arrive
- [Phase ?]: BkEditorWorldToScreen uses z=0, not the terrain's real height, because GetPos3's real-terrain ray-cast never resolves in this bridge's headless session (measured: always falls back to its own z=0-plane path) - using real height would not round-trip with ScreenToWorld and would draw the brush outline off the ground a click actually resolves against
- [Phase ?]: CTerrain::GetTileIndex rounds to the nearest tile rather than flooring a bucket, and measures Y from the terrain's far edge, not world_y 0 - confirmed by the engine tier rather than assumed, since view.zig's brush-outline corners depend on the exact relationship
- [Phase ?]: D-12 (camera rotation) deferred out of M1: yaw measurement shows terrain clips away and sprites stay fixed at any yaw but the game's own 45 degrees
- [Phase ?]: needsSaveAs gained a user_root parameter for the D-22 recovery-folder check (a reopened recovery copy is saved with Save As) rather than a parallel function
- [Phase ?]: run() takes two explicit parameters (settings_path, is_interactive) instead of one flag reused for both, so a future settings-path resolution failure cannot silently also disable autosave
- [Phase ?]: 03-08: State.mod_folder is an owned fixed buffer (modFolder()/setModFolder), not a borrowed slice - State is returned by value from init, the same reasoning tile_count follows for tile_buffer — Avoids a dangling-slice bug the moment a field set after init pointed into anything but State's own storage
- [Phase ?]: 03-08: BkEditorSetMod validates (bare-name check, mod.xml read) entirely before CloseSessionMap or any storage/global change, so a refusal never closes the map it was about to switch away from
- [Phase ?]: D-29 shipped: engine decodes each object's own icon.tga, cached; a neutral named frame for the rest (Johannes's Task 2 checkpoint choice)
- [Phase ?]: 03-09: single soldiers with no icon.tga borrow their squad's (Johannes-requested addition); coverage rose from 1073/1167 to 1126/1167 placeable objects
- [Phase ?]: 03-10: bridge binds to CMapInfo::sounds.sounds, not soundsList as the plan named - operator& never serialises soundsList (confirmed via MapEquivalence.cpp's own comment); soundsList's own MFC-editor marker loop never populates it from a loaded file either
- [Phase ?]: 03-10: SMapSoundInfo's binary save had nMaxRadius and bMuteDuringCombat sharing tag 6 (pre-existing bug); gave bMuteDuringCombat tag 7 - no shipped map has non-empty sounds today so nothing on disk is displaced
- [Phase ?]: 03-11: the unknown-object host-check seam anchors on dirname(output), not process cwd, since map-editor-host-check's run step sets a staged-game cwd, not the repo root
- [Phase ?]: 03-11: State.defaultPlacerObject must iterate self.catalogue with |*entry|, not |entry| - a by-value loop copy dangles the returned name slice the moment the function returns (found by map-editor-smoke, same bug class as loadCatalogue's sound_names)

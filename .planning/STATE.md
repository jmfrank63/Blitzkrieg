---
gsd_state_version: "1.0"
status: phase-2-executed-human-verification-outstanding
stopped_at: Completed 03-06-PLAN.md
last_updated: "2026-09-28T06:36:06.696Z"
state_head: 14f0e3dc4a3cf0f4ecfd6cc0c8f4873301234871
progress:
  total_phases: 2
  completed_phases: 0
  total_plans: 18
  completed_plans: 9
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

**Last session:** 2026-09-28T06:36:06.644Z
**Stopped at:** Completed 03-06-PLAN.md
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

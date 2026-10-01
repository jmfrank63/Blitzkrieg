---
gsd_state_version: "1.0"
current_plan: 5
status: phase-2-executed-human-verification-outstanding
stopped_at: Completed 05-03-PLAN.md
last_updated: "2026-10-01T20:29:30.215Z"
state_head: 9d7923d6567c21c785484bfaf7539ea54185c603
progress:
  total_phases: 7
  completed_phases: 0
  total_plans: 43
  completed_plans: 35
  percent: 0
current_phase: 05
current_phase_name: "Map editor M3: random map templates, minimap tools, full parity"
---

# Project state

- status: phase-2-executed-human-verification-outstanding
- projectName: Blitzkrieg Reloaded
- branch: main (feature merged through workspace/variable-zoom)
- created: 2026-06-06
- repoRoot: Blitzkrieg
- workflow: gsd-execute-phase

## Current Position

Current Plan: 4
Total Plans in Phase: 11
Progress: [░░░░░░░░░░] 0%

Phase 05 wave 3: 05-03 (filters and fields) summarized; 05-04 next in the
wave order.

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

**Last session:** 2026-10-01T20:28:44.306Z
**Stopped at:** Completed 05-03-PLAN.md
**Resume file:** None

## Accumulated Context

### Roadmap Evolution

- Phase 3 added (2026-09-28): Map editor plan 6: finish M1 — branch feat/map-editor-plan-6, worktree .worktrees/map-editor-6
- Phase 3 complete (2026-09-29), merged into main 32b9233ce
- Phase 4 added (2026-09-29): Map editor M2: roads, rivers, bridges, AI groups, scripts — branch feat/map-editor-m2, worktree .worktrees/map-editor-6

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
| Phase 03-map-editor-plan-6-finish-m1 P12 | 55min | 2 tasks | 8 files |
| Phase 03-map-editor-plan-6-finish-m1 P13 P13 | ~58min | 3 tasks | 6 files |
| Phase 03-map-editor-plan-6-finish-m1 P14 | ~90min | 2 tasks | 6 files |
| Phase 03 P15 | ~95min | 3 tasks | 1 files |
| Phase 03 P15 | ~4h20min incl. hand-try gap fixes | 3 tasks | 44 files |
| Phase 03 P16 | 107min | 6 tasks | 13 files |
| Phase 04 P01 | 40min | 3 tasks | 24 files |
| Phase 04 P02 | 19 min | 3 tasks | 9 files |
| Phase 04 P03 | 20min | 3 tasks | 13 files |
| Phase 04 P04 | 20min | 3 tasks | 11 files |
| Phase 04 P05 | 53min | 4 tasks | 24 files |
| Phase 04 P06 | 50min | 4 tasks | 25 files |
| Phase 04 P07 | 28min | 2 tasks | 23 files |
| Phase 04 P08 | 2h01m | 3 tasks | 23 files |
| Phase 04 P09 | 1h01m | 3 tasks | 21 files |
| Phase 04 P10 | 1h02m | 4 tasks | 30 files |
| Phase 04 P11 | 1h20m | 4 tasks | 21 files |
| Phase 04 P12 | 50m | 3 tasks | 23 files |
| Phase 04 P13 | 85min | 4 tasks | 16 files |
| Phase 05 P01 | ~3h 30m | 3 tasks | 24 files |
| Phase 05 P02 | ~6h (incl. interruption recovery) | 3 tasks | 25 files |
| Phase 05 P03 | 2 waves (continuation) | 3 tasks | 27 files |
| Phase 05 P03 | 2 waves (continuation) | 3 tasks | 27 files |

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
- [Phase ?]: 03-12: panels.zig's mapIsOpen/requestTestLaunch made pub, plus State.test_extra_env, so AutoRunner's test/waitgame handlers can drive Test in game - not in this plan's files_modified, but the least invasive way to satisfy Zig's exhaustive switch over auto.zig's Action set
- [Phase ?]: 03-12: BK_EDITOR_AUTO's autoshot cleanup is a new Zig tool (tools/zig/delete_matching_files.zig) run via addRunArtifact, not addSystemCommand(sh/cmd) - the latter failed build_hermeticity_test.zig's forbidden-executable audit
- [Phase 03]: 03-13: the orange probe (255,128,0) replaces magenta in the host check - magenta's own R/B swap gives back magenta, so it could never catch the readback-swap bug the check exists to detect
- [Phase 03]: 03-13: BkEditorCaptureFrame's read-back failure gets bridge-composed distinct messages (allocation vs readback), not GraphicsEngineGpu's own internal message - IGFX has no accessor for it, and adding one is an architectural change outside a carried-minors bridge fix
- [Phase 03]: 03-13: KeepWindowOnItsOwnDisplay's second-monitor fix was verified with a live measurement against a real second display (throwaway instrumented host.zig, reverted), not assumed from reading SelectedDisplay's source alone
- [Phase ?]: 03-14: --map-editor's package-mods verify command (grep -ci /mods/) is broader than the threat (matches legitimate Data/*/mods/ localization folders); verified the precise top-level-only condition instead (0 top-level mods/ entries)
- [Phase ?]: 03-14: crt.attachParentConsole does the full CONOUT$-open-and-SetStdHandle recipe, not just AttachConsole - Zig's std.Io.File.stdout/stderr read the process-parameters block live on every call, so AttachConsole alone would not make anything print
- [Phase ?]: 03-15: M1 automated exit criteria met (sweep 1755/1755, 11 local tier PASS lines, CI 36473046568 six green jobs); hand try pending for Johannes
- [Phase ?]: 03-15: spec cites commands and dates for exit criteria; CI run ids live in the SUMMARY
- [Phase ?]: 03-15: generated winter/Africa unit textures are a build output staged beside Data as SeasonData/SeasonTextures.pak (one stored .pak for the zip entry limit), mounted over the base Data below any mod; Data is never written
- [Phase ?]: 03-15: Cmd+W closes the map, not the editor (SDL's Window > Close loses its key equivalent on macOS); hand try approved 2026-09-29
- [Phase 04]: 04-01: camera-anchor C ABI struct is BkEditorCameraAnchorRecord because a C typedef and a function share one namespace — The plan named both the struct and the entry point BkEditorCameraAnchors
- [Phase 04]: 04-01: byte-identity tests read the map fresh for every write they compare, never a copy of a map — SVertexAltitude is written as a raw struct, so a copied map's three padding bytes per vertex differ from a read map's; later plans follow the rule
- [Phase 04]: 04-01: camera-anchor set validates only the slots it changes; NextVsoID floors at 1; PutScriptFile is exact and IsBareScriptName checks new names — An off-map anchor a file already holds must not block other edits; undo must be able to restore any name a file held
- [Phase 04]: 04-02: a delete edits the records naming the object (start-command units and targets, reserve positions) and refuses only for a bridge span, a trench piece and a vehicle holding a passenger; link ID 0 is never a reference — Matches the MFC editor's cascade with C3; refusals protect the game's loaders and M3's links
- [Phase 04]: 04-03: tools get right-button, Ctrl-as-right and double click only when their registry entry asks; tool= uses the ToolId name; handlers return Outcome
- [Phase 04]: 04-04: BK_MAP_TRACE names are double-quoted; --game-reads-it-m2 writes a combined baseline+edited report; the shared game-reads helpers live in game_reads_common.zig — Names may hold spaces; one game run prints one camera line, so a two-run comparison needs one file; game_reads_m2.zig cannot import the executable's root
- [Phase ?]: 04-05: roads and rivers map saved nID to engine nID by a vector parallel to the saved list, not a map keyed by nID — survives repeated nIDs; 0 of 59 shipped maps repeat one
- [Phase ?]: 04-05: an edit of an existing road hands CVSOBuilder::Update the record's own first width and opacity, not the panel's — an edit never depends on panel state
- [Phase ?]: 04-05: BK_EDITOR_AUTO drags keep press, drag and release in one frame — the view's stale-gesture guard ends a gesture the real mouse does not hold
- [Phase ?]: Bridge edits are SGroupEdit records: RemoveGroup erases the entry then the spans, AddGroup restores spans then the entry, all or nothing; a new group goes in through the same AddGroup its redo uses
- [Phase ?]: Bridge span geometry is NMapGeometry::PlanBridge (and RotatedBridgeDrag for rotate), shared by the bridge and the map-file/engine tiers; the world-unit MFC nudges are left out, one map-unit nudge kept
- [Phase ?]: In the Bridge tool a click selects (or deselects) and never draws; a drag draws
- [Phase ?]: BK_EDITOR_AUTO scripted presses are held across frames until their release (View.holdScripted)
- [Phase ?]: 04-07: the fence tile mapping is the engine's GetAITileIndex (rounds), not CMapInfo::GetAITileIndices (truncates); they agree only at tile corners
- [Phase ?]: 04-07: a fence run is one SGroupEdit with no bridges entry (SBridgeGroup.bEntry false); PlanFences also refuses a moved fence that leaves the map
- [Phase ?]: 04-08: the engine places trench pieces anywhere (IsObjectInsideOfMap passes entrenchments), so PlanEntrenchment refuses a piece off the map
- [Phase ?]: 04-08: a bridge or entrenchment a unit is garrisoned in (nLinkWith) is refused whole before anything is taken out; moving units is M3
- [Phase ?]: 04-08: the trench builder is pinned by properties over 500 fixed-seed polylines, identical on all CI platforms; the fireplace/line switcher is global over the trench, as in MFC
- [Phase ?]: 04-08: mid-phase CI gate green on all six jobs (run 36663674380); narrow checkouts now carry road and river descriptors
- [Phase ?]: 04-09: Hide checked takes the object out of the scene (the MFC editor's RemoveFromScene/AddToScene), not opacity 0: opacity left a tank's mesh shadow and health bar standing; the objects come back for the world's update and leave again
- [Phase ?]: 04-09: a group put validates only what it adds (0..32000, once); what the file held when the map opened is exempt, so an undo can put a file's own odd data back (the session keeps openedGroups)
- [Phase ?]: 04-09: the Group Manager is a floating window (Map -> Reinforcement groups...), every control a command; the generic record path now has record_add and record_delete with recordKeys/insertRecord/removeRecord
- [Phase ?]: 04-10: a script file put takes None, a bare name or exactly the value the file held at open (shipped maps hold a folder path); every copy, list and URL uses the last path component the game itself keeps (gameScriptName), so the fixed-folder-plus-validated-name mitigation holds
- [Phase ?]: 04-10: a palette-placed object is linked with nothing (nLinkWith 0): the game lands a reinforcement only when it is 0, so an editor-placed unit in a group never landed; the group tools' spans, fences and trench pieces keep -1
- [Phase ?]: 04-10: script areas are index-keyed records stored verbatim in AI units; the MFC Vis -> AI truncation lives once in NMapGeometry and the bridge answers drag, move and resize as pure calls; a name the file held twice can be put back as often as it held it
- [Phase ?]: 04-11: the action list is parsed in the bridge from Data/Editor/actions.ini with the MFC table's rules (the game's StreamIO port answers nothing for OpenIniDataTable); STOP is entry 9 of 40
- [Phase ?]: 04-11: start commands and reserve positions are judged on what a put changes, and a record the file held at open is always accepted back (openedStartCommands, openedReservePositions), so an undo of a delete of odd file data never drifts; a NEW start-command unit must be a unit or squad the database knows
- [Phase ?]: 04-11: reserve roles (self-propelled, towed, truck) are read from the stats with dynamic_cast, never the typed lookup (it static_casts with asserts compiled out); a self-propelled gun takes no truck and a truck must pull more than the gun weighs; a set never changes from_explosion
- [Phase ?]: 04-11: Start Target and Reserve Positions are hidden registry tools (no palette button, menu entry or key) that click on the release; a panel that uses Delete claims it per frame (View.delete_claimed) instead of capturing the keyboard, so the Select tool cannot delete the selected unit as well
- [Phase ?]: 04-12: a parcel's type is a non-exhaustive enum(i32); the AI side is put whole with the side count so undo restores the side count exactly; the AI General tool's keys act on the selection; every tool is in the palette (tools panel 228 px) and the local M1 reference was refreshed
- [Phase ?]: Save As offers to bring the script along for any map whose script is beside it and the new folder differs (04-13)
- [Phase ?]: In the width mode All the Roads & Rivers sliders re-width the selected line, one undo step per slider drag (04-13, MFC CW_ALL)
- [Phase ?]: M2 exit met: M2 sweeps byte-exact (59 maps/460 edits, 57 maps/238 edits), CI 36692345194 green, win-home non-GUI tiers and release package green (04-13)
